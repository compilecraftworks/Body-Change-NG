#include "BodyChangerNG/ActorEvents.h"
#include "BodyChangerNG/BodyFamily.h"
#include "BodyChangerNG/InputSink.h"
#include "BodyChangerNG/NativeImGuiHost.h"
#include "BodyChangerNG/OutfitRefit.h"
#include "BodyChangerNG/PresetCatalog.h"
#include "BodyChangerNG/RaceMenuBodyMorph.h"
#include "BodyChangerNG/Distribution.h"
#include "BodyChangerNG/Settings.h"
#include "BodyChangerNG/SkinProfiles.h"
#include "BodyChangerNG/SmoothCamIntegration.h"
#include "BodyChangerNG/TextInputFilter.h"
#include "BodyChangerNG/UI.h"

#include <SKSE/Logger.h>

#include <spdlog/sinks/basic_file_sink.h>

namespace
{
    void InitializeLogging()
    {
        auto directory = SKSE::log::log_directory();
        if (!directory) SKSE::stl::report_and_fail("Unable to resolve the SKSE log directory");
        const auto path = *directory / "BodyChangerNG.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto logger = std::make_shared<spdlog::logger>("BodyChangerNG", std::move(sink));
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
    }

    void OnSkseMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) return;
        if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
            // Public SKSE messaging only: SmoothCam remains entirely
            // optional and there is no load-time DLL dependency.
            bcn::smoothcam::RegisterInterfaceListener();
            bcn::smoothcam::RequestInterface();
            bcn::racemenu::Initialize();
        }
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            bcn::racemenu::Initialize();
            bcn::PresetCatalog::Get().Refresh();
            bcn::SkinProfiles::Get().Refresh();
            [[maybe_unused]] const auto loadedBodyChangerRules = bcn::Distribution::Get().Load();
            // OBody's JSON is deliberately not read at startup.  ORefit outfit
            // ORefit rules are registered only from the explicit outfit-popup action.
            bcn::OutfitRefit::Get().ClearLegacyRules();
            bcn::ActorEvents::Get().Register();
        }
        if (message->type == SKSE::MessagingInterface::kInputLoaded) {
            bcn::InputSink::Get().Register();
        }
        if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
            message->type == SKSE::MessagingInterface::kNewGame) {
            bcn::body_family::ResetRuntimeCaches();
            // Existing-save actors may never emit TESInitScriptEvent again.
            // Apply the saved rules to every currently loaded process actor as
            // soon as a game becomes active; later cell attachments are handled
            // by ActorEvents.
            [[maybe_unused]] const auto applied = bcn::Distribution::Get().ApplyLoadedNPCs();
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);
    // SKSE::Init installs CommonLib's default logger.  Create our file logger
    // afterwards so it is not immediately replaced, otherwise the file is
    // truncated on startup but every diagnostic line is silently lost.
    InitializeLogging();
    // Keep enough shared trampoline space for both the renderer hook and the
    // verified text-focus input call hook.
    SKSE::AllocTrampoline(1 << 12);
    bcn::Settings::Get().Load();
    bcn::ui::Initialize();

    const auto textInputFilterInstalled = bcn::text_input::Install();
    const auto rendererHookInstalled = bcn::native_ui::InstallRendererHook();
    if (auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener(OnSkseMessage);
    }
    SKSE::log::info("Body Changer NG {} loaded; native UI renderer hook {}; text-focus input filter {}",
        BODY_CHANGER_NG_VERSION,
        rendererHookInstalled ? "installed" : "unavailable for this runtime",
        textInputFilterInstalled ? "installed" : "unavailable for this runtime");
    return true;
}
