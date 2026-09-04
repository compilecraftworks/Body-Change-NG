#include "BodyChangeNG/ActorEvents.h"
#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/InputSink.h"
#include "BodyChangeNG/NativeImGuiHost.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/RaceMenuPresetMigration.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinOverrides.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SmoothCamIntegration.h"
#include "BodyChangeNG/TextInputFilter.h"
#include "BodyChangeNG/UI.h"

#include <SKSE/Logger.h>

#include <spdlog/sinks/basic_file_sink.h>

namespace
{
    void QueueInitialDistribution(const std::uint64_t session, const std::uint8_t remainingHops)
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        bcn::frame_tasks::Queue(0, [session] {
            if (bcn::ActorRegistry::Get().SessionGeneration() != session) return;
            [[maybe_unused]] const auto applied = bcn::Distribution::Get().ApplyLoadedNPCs();
        }, remainingHops, 101);
    }

    void InitializeLogging()
    {
        auto directory = SKSE::log::log_directory();
        if (!directory) SKSE::stl::report_and_fail("Unable to resolve the SKSE log directory");
        const auto path = *directory / "BodyChangeNG.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto logger = std::make_shared<spdlog::logger>("BodyChangeNG", std::move(sink));
        logger->set_level(spdlog::level::info);
        // Per-actor distribution emits useful INFO diagnostics, but forcing a
        // physical file flush for every line can turn a dense cell load into a
        // visible frame spike. Warnings and errors still flush immediately;
        // normal INFO output is flushed by the sink without blocking every NPC.
        logger->flush_on(spdlog::level::warn);
        spdlog::set_default_logger(std::move(logger));
    }

    void OnSkseMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) return;
        if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
            bcn::frame_tasks::Reset(false);
            bcn::ui::OnLoadStart();
            bcn::ActorWorkQueue::Get().ResetSession();
            bcn::ActorEvents::Get().ResetSessionState();
        }
        if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
            // Public SKSE messaging only: SmoothCam remains entirely
            // optional and there is no load-time DLL dependency.
            bcn::smoothcam::RegisterInterfaceListener();
            bcn::smoothcam::RequestInterface();
            bcn::racemenu::Initialize();
        }
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            bcn::racemenu::Initialize();
            bcn::racemenu_preset_migration::MigrateVisiblePresets();
            bcn::PresetCatalog::Get().Refresh();
            bcn::SkinProfiles::Get().Refresh();
            bcn::player_tint::Catalog::Get().Refresh();
            [[maybe_unused]] const auto loadedBodyChangeRules = bcn::Distribution::Get().Load();
            // OBody's JSON is deliberately not read at startup. Outfit-
            // correction rules are registered only by the explicit popup action.
            bcn::OutfitRefit::Get().ClearLegacyRules();
            bcn::ActorEvents::Get().Register();
        }
        if (message->type == SKSE::MessagingInterface::kInputLoaded) {
            bcn::InputSink::Get().Register();
        }
        if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
            message->type == SKSE::MessagingInterface::kNewGame) {
            bcn::frame_tasks::Reset(true);
            bcn::ActorWorkQueue::Get().ResetSession();
            bcn::ActorEvents::Get().ResetSessionState();
            bcn::racemenu::ResetSessionState();
            bcn::skin_override::ResetSessionState();
            bcn::body_family::ResetRuntimeCaches();
            // Existing-save actors may never emit TESInitScriptEvent again.
            // Defer only this one initial enumeration so RaceMenu, serialization
            // listeners and optional overlay plugins can finish their load
            // callbacks first. Later cell attachments still use ActorEvents and
            // the normal coalescing queue.
            QueueInitialDistribution(bcn::ActorRegistry::Get().SessionGeneration(),
                bcn::InitialDistributionDelayTicks());
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
    bcn::ActorRegistry::Get().RegisterSerialization();
    bcn::ui::Initialize();

    const auto textInputFilterInstalled = bcn::text_input::Install();
    bcn::frame_tasks::SetAvailable(textInputFilterInstalled);
    const auto rendererHookInstalled = bcn::native_ui::InstallRendererHook();
    if (auto* messaging = SKSE::GetMessagingInterface()) {
        messaging->RegisterListener(OnSkseMessage);
    }
    SKSE::log::info("Body Change NG {} loaded; native UI renderer hook {}; text-focus input filter {}",
        BODY_CHANGE_NG_VERSION,
        rendererHookInstalled ? "installed" : "unavailable for this runtime",
        textInputFilterInstalled ? "installed" : "unavailable for this runtime");
    return true;
}
