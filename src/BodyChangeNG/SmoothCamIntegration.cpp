#include "BodyChangeNG/SmoothCamIntegration.h"

#include <SKSE/Logger.h>

#include <cstring>

namespace
{
    constexpr auto kSmoothCamPluginName = "SmoothCam";
    constexpr auto kSmoothCamModuleName = L"SmoothCam.dll";
    constexpr std::uint32_t kSmoothCamCommandHeader = 0x9007CA50;

    enum class InterfaceVersion : std::uint8_t
    {
        v1,
        v2
    };

    enum class ApiResult : std::uint8_t
    {
        ok,
        notOwner,
        mustKeep,
        alreadyGiven,
        alreadyTaken,
        badThread
    };

    // ABI-compatible subset of SmoothCam's documented V2 public interface.
    // V1 remains complete because V2 extends it virtually.
    class ISmoothCamV1
    {
    public:
        virtual unsigned long GetSmoothCamThreadId() const noexcept = 0;
        virtual ApiResult RequestCameraControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
        virtual ApiResult RequestCrosshairControl(SKSE::PluginHandle a_pluginHandle, bool a_restoreDefaults = true) noexcept = 0;
        virtual ApiResult RequestStealthMeterControl(SKSE::PluginHandle a_pluginHandle, bool a_restoreDefaults = true) noexcept = 0;
        virtual SKSE::PluginHandle GetCameraOwner() const noexcept = 0;
        virtual SKSE::PluginHandle GetCrosshairOwner() const noexcept = 0;
        virtual SKSE::PluginHandle GetStealthMeterOwner() const noexcept = 0;
        virtual ApiResult ReleaseCameraControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
        virtual ApiResult ReleaseCrosshairControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
        virtual ApiResult ReleaseStealthMeterControl(SKSE::PluginHandle a_pluginHandle) noexcept = 0;
    };

    class ISmoothCamV2 : public ISmoothCamV1
    {
    public:
        virtual RE::NiPoint3 GetLastCameraPosition() const noexcept = 0;
        virtual ApiResult RequestInterpolatorUpdates(SKSE::PluginHandle a_pluginHandle, bool a_allowUpdates) noexcept = 0;
        virtual ApiResult SendToGoalPosition(SKSE::PluginHandle a_pluginHandle, bool a_shouldMoveToGoal,
            bool a_moveNow = false, const RE::Actor* a_reference = nullptr) noexcept = 0;
        virtual void GetGoalPosition(RE::TESObjectREFR* a_reference, RE::NiPoint3& a_world, RE::NiPoint3& a_local) const noexcept = 0;
        virtual bool IsCameraEnabled() const noexcept = 0;
    };

    struct PluginCommand
    {
        enum class Type : std::uint8_t { requestInterface };
        std::uint32_t header{ kSmoothCamCommandHeader };
        Type type{ Type::requestInterface };
        void* commandStructure{};
    };

    struct InterfaceRequest
    {
        InterfaceVersion interfaceVersion{ InterfaceVersion::v2 };
    };

    struct PluginResponse
    {
        enum class Type : std::uint8_t { error, interfaceProvider };
        Type type{ Type::error };
        void* responseData{};
    };

    struct InterfaceContainer
    {
        void* interfaceInstance{};
        InterfaceVersion interfaceVersion{ InterfaceVersion::v1 };
    };

    std::mutex g_interfaceLock;
    ISmoothCamV2* g_interface{};
    bool g_listenerRegistered{};
    bool g_controlOwned{};
    bool g_denialLogged{};

    void OnSmoothCamMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message || !message->sender || std::strcmp(message->sender, kSmoothCamPluginName) != 0 ||
            message->type != 0 || message->dataLen != sizeof(PluginResponse) || !message->data) {
            return;
        }
        const auto* response = static_cast<const PluginResponse*>(message->data);
        if (response->type != PluginResponse::Type::interfaceProvider || !response->responseData) return;
        const auto* container = static_cast<const InterfaceContainer*>(response->responseData);
        if (!container->interfaceInstance || container->interfaceVersion < InterfaceVersion::v2) return;
        const std::scoped_lock lock(g_interfaceLock);
        g_interface = static_cast<ISmoothCamV2*>(container->interfaceInstance);
        SKSE::log::info("Body Change NG connected to SmoothCam V2 camera control");
    }
}

namespace bcn::smoothcam
{
    void RegisterInterfaceListener()
    {
        const std::scoped_lock lock(g_interfaceLock);
        if (g_listenerRegistered || !GetModuleHandleW(kSmoothCamModuleName)) return;
        if (auto* messaging = SKSE::GetMessagingInterface(); messaging &&
            messaging->RegisterListener(kSmoothCamPluginName, OnSmoothCamMessage)) {
            g_listenerRegistered = true;
        }
    }

    void RequestInterface()
    {
        if (!GetModuleHandleW(kSmoothCamModuleName)) return;
        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging) return;
        InterfaceRequest request;
        PluginCommand command;
        command.commandStructure = std::addressof(request);
        static_cast<void>(messaging->Dispatch(0, std::addressof(command), sizeof(command), kSmoothCamPluginName));
    }

    bool AcquireCameraControl()
    {
        const std::scoped_lock lock(g_interfaceLock);
        if (!g_interface || !g_interface->IsCameraEnabled()) return true;
        const auto result = g_interface->RequestCameraControl(SKSE::GetPluginHandle());
        if (result != ApiResult::ok && result != ApiResult::alreadyGiven) {
            if (!g_denialLogged) {
                SKSE::log::warn("SmoothCam refused Body Change NG menu camera control ({})", static_cast<std::uint8_t>(result));
                g_denialLogged = true;
            }
            return false;
        }
        g_controlOwned = true;
        g_denialLogged = false;
        // RequestCameraControl already suspends SmoothCam's interpolator.
        // Re-enabling it while this menu owns the camera makes the two camera
        // controllers compete and produces pause-dependent framing.
        return true;
    }

    void ReleaseCameraControl()
    {
        const std::scoped_lock lock(g_interfaceLock);
        if (g_controlOwned && g_interface) {
            static_cast<void>(g_interface->ReleaseCameraControl(SKSE::GetPluginHandle()));
        }
        g_controlOwned = false;
    }
}
