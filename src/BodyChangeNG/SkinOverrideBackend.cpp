#include "BodyChangeNG/SkinOverrideBackend.h"

#include "BodyChangeNG/RaceMenuBodyMorph.h"

#include <SKSE/Logger.h>

#include <atomic>

namespace
{
    std::atomic<bcn::skin_backend::IPluginInterface*> g_interface{};
    std::atomic<bcn::racemenu_override::Route> g_route{
        bcn::racemenu_override::Route::unsupported };
}

namespace bcn::skin_backend
{
    IPluginInterface* Interface() noexcept
    {
        if (auto* existing = g_interface.load(std::memory_order_acquire)) return existing;
        auto* candidate = static_cast<IPluginInterface*>(
            bcn::racemenu::QueryInterface("Override"));
        if (!candidate) return nullptr;

        const auto version = candidate->GetVersion();
        const auto runtime = REL::Module::get().version();
        const auto aeRuntime = runtime.compare(REL::Version{ 1, 6, 0, 0 }) !=
            std::strong_ordering::less;
        const auto route = racemenu_override::ResolveRoute(version, aeRuntime);
        if (route == racemenu_override::Route::unsupported) {
            SKSE::log::error(
                "Body Change NG rejected unsupported RaceMenu Override interface version {}",
                version);
            return nullptr;
        }

        g_route.store(route, std::memory_order_release);
        g_interface.store(candidate, std::memory_order_release);
        SKSE::log::info(
            "Body Change NG received RaceMenu Override interface version {} runtime={} route={} path={}",
            version, runtime.string(), racemenu_override::RouteLabel(route),
            racemenu_override::UsesNativeV2(route) ?
                "native-v2-exact-persistent" : "Papyrus-NiOverride-exact-persistent");
        return candidate;
    }

    racemenu_override::Route ActiveRoute() noexcept
    {
        if (!Interface()) return racemenu_override::Route::unsupported;
        return g_route.load(std::memory_order_acquire);
    }

    IOverrideInterfaceV2* NativeV2() noexcept
    {
        auto* base = Interface();
        return base && racemenu_override::UsesNativeV2(ActiveRoute()) ?
            static_cast<IOverrideInterfaceV2*>(base) : nullptr;
    }

    bool UsesPapyrus() noexcept
    {
        return Interface() && racemenu_override::UsesPapyrus(ActiveRoute());
    }
}
