#include "BodyChangeNG/ActorEvents.h"
#include "BodyChangeNG/ActorRegistry.h"
#include "BodyChangeNG/ActorWorkQueue.h"
#include "BodyChangeNG/CatalogRefreshQueue.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/FutanariSupport.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/InputSink.h"
#include "BodyChangeNG/NativeImGuiHost.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/RenderedOutfit.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/RaceMenuFormDeleteGuard.h"
#include "BodyChangeNG/RaceMenuPresetMigration.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinApplication.h"
#include "BodyChangeNG/NativeAddonSkinBackend.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/SmoothCamIntegration.h"
#include "BodyChangeNG/TextInputFilter.h"
#include "BodyChangeNG/UI.h"

#include <SKSE/Logger.h>

#include <spdlog/sinks/basic_file_sink.h>

namespace
{
    [[nodiscard]] bool IsCanonicalPluginModule() noexcept
    {
        HMODULE module{};
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&IsCanonicalPluginModule), &module)) {
            return false;
        }

        std::array<wchar_t, 32768> modulePath{};
        const auto length = GetModuleFileNameW(
            module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
        if (length == 0U || length >= modulePath.size()) return false;

        const auto filename = std::filesystem::path{
            std::wstring_view{ modulePath.data(), length }
        }.filename().wstring();
        return _wcsicmp(filename.c_str(), L"BodyChangeNG.dll") == 0;
    }

    void QueueInitialDistribution(const std::uint64_t session, const std::uint8_t remainingHops)
    {
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        bcn::frame_tasks::Queue(0, [session] {
            if (bcn::ActorRegistry::Get().SessionGeneration() != session) return;
            [[maybe_unused]] const auto applied = bcn::Distribution::Get().ApplyLoadedNPCs();
            bcn::ActorEvents::Get().QueuePlayerLoadRestoration();
        }, remainingHops, bcn::appearance::WorkChannel::initialDistribution);
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
            bcn::rendered_outfit::Reset();
            bcn::ui::OnLoadStart();
            bcn::ActorWorkQueue::Get().ResetSession();
            bcn::ActorEvents::Get().ResetSessionState();
            // Restore only still-owned runtime form pointers while the old
            // game's ActorBase forms are unquestionably valid. Post-load runs
            // the same idempotent reset for direct New Game transitions.
            bcn::skin_application::ResetSessionState();
        }
        if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
            bcn::rendered_outfit::Initialize();
            // Public SKSE messaging only: SmoothCam remains entirely
            // optional and there is no load-time DLL dependency.
            bcn::smoothcam::RegisterInterfaceListener();
            bcn::smoothcam::RequestInterface();
            bcn::racemenu::Initialize();
            [[maybe_unused]] const auto overlayReady = bcn::overlay::IsReady();
        }
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            bcn::rendered_outfit::Initialize();
            bcn::native_addon::Install();
            // Form-backed optional feature detection is authoritative only
            // after every ESP/ESM has finished loading. Do not let an earlier
            // UI query cache an empty addon registry for the whole session.
            bcn::futanari_support::RefreshInstalledAddons();
            bcn::racemenu_form_delete::InstallInGame();
            bcn::racemenu::Initialize();
            bcn::racemenu_preset_migration::MigrateVisiblePresets();
            bcn::PresetCatalog::Get().Refresh();
            bcn::SkinProfiles::Get().Refresh();
            bcn::FutanariSkinProfiles::Get().Refresh();
            bcn::player_tint::Catalog::Get().Refresh();
            // Construct after every catalog so its worker joins before they are destroyed.
            (void)bcn::catalog_refresh::Get();
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
            bcn::rendered_outfit::Reset();
            bcn::ActorWorkQueue::Get().ResetSession();
            bcn::ActorEvents::Get().ResetSessionState();
            bcn::racemenu::ResetSessionState();
            bcn::overlay::ResetSessionState();
            bcn::skin_application::ResetSessionState();
            bcn::body_family::ResetRuntimeCaches();
            // Existing-save actors may never emit TESInitScriptEvent again.
            // Defer only this one initial enumeration so RaceMenu, serialization
            // listeners and optional overlay plugins can finish their load
            // callbacks first. Later cell attachments still use ActorEvents and
            // the normal coalescing queue.
            QueueInitialDistribution(bcn::ActorRegistry::Get().SessionGeneration(),
                bcn::InitialDistributionDelayTicks());
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                [[maybe_unused]] const auto requested = bcn::overlay::RequestCatalog(player);
            }
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    // SKSE loads every *.dll placed directly under SKSE/Plugins.  A backup
    // such as BodyChangeNG.before-update.dll is therefore another live copy,
    // not an inert backup.  Multiple copies register the same hooks and can
    // mutate one ActorBase Skin Armor concurrently.  Fail before SKSE::Init
    // and before opening the shared log unless this is the canonical module.
    if (!IsCanonicalPluginModule()) return false;

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
    [[maybe_unused]] const auto overlayBridge = bcn::overlay::InitializeCatalogBridge();

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
