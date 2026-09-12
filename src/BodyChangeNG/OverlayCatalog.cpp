#include "BodyChangeNG/RaceMenuOverlay.h"

#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/CatalogRoots.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/OverlayPolicy.h"
#include "BodyChangeNG/PathText.h"
#include "BodyChangeNG/SlaveTatsCatalog.h"

#include <RE/A/Actor.h>
#include <RE/G/GFxFunctionHandler.h>
#include <RE/G/GFxMovieView.h>
#include <RE/G/GFxValue.h>
#include <SKSE/API.h>
#include <SKSE/Events.h>
#include <SKSE/Logger.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

namespace
{
    constexpr std::string_view kMenuName{ "HUD Menu" };
    constexpr std::string_view kRootName{ "_global.skse.plugins.BodyChangeNG." };
    constexpr float kPaintRequestMask = static_cast<float>(0x02 | 0x04 | 0x08 | 0x10);

    std::mutex g_catalogLock;
    std::array<std::unordered_map<std::string, bcn::overlay::Entry>,
        bcn::overlay::Index(bcn::overlay::Area::count)> g_catalog;
    std::atomic<std::uint64_t> g_revision{};
    std::atomic_bool g_requested{};
    std::atomic_bool g_catalogSettled{};
    std::atomic_bool g_bridgeRegistered{};
    RE::ActorHandle g_requestTarget;
    std::uint8_t g_emptyResponseRetries{};
    constexpr std::uint8_t kEmptyResponseRetryCount = 3U;
    std::mutex g_slaveTatsLock;
    std::vector<bcn::overlay::Entry> g_slaveTatsCache;
    bool g_slaveTatsLoaded{};

    void SendModEvent(const char* name, std::string_view text = {},
        float number = 0.0F, RE::TESForm* sender = nullptr);
    void RequestWhenHudReady(RE::ActorHandle handle, std::uint32_t retries);

    [[nodiscard]] std::string InstalledTextureProvider(const std::string_view texturePath)
    {
        const auto layers = bcn::overlay::TextureLayers(texturePath);
        if (layers.empty()) return {};
        const auto identity = bcn::overlay::TextureIdentity(layers.front().path);
        if (identity.empty()) return {};
        const auto logical = std::filesystem::current_path() / "Data" / "Textures" /
            bcn::path_text::FromUtf8(identity);
        const auto provider = bcn::catalog_roots::ResolveProviderPath(logical);
        return provider ? bcn::path_text::GenericUtf8(*provider) : std::string{};
    }

    void ResolveInstalledMetadata(bcn::overlay::Entry& entry,
        const std::string_view additionalEvidence = {})
    {
        auto provider = InstalledTextureProvider(entry.texturePath);
        if (!additionalEvidence.empty()) {
            if (!provider.empty()) provider.push_back(' ');
            provider.append(additionalEvidence);
        }
        entry.layout = bcn::overlay::ClassifyLayout(entry.name, entry.texturePath, provider);
        entry.sex = bcn::overlay::ClassifySex(entry.name, entry.texturePath, provider);
    }

    [[nodiscard]] std::vector<bcn::overlay::Entry> ScanSlaveTatsCatalog()
    {
        const auto logicalRoot = std::filesystem::current_path() / "Data" / "Textures" /
            "Actors" / "Character" / "slavetats";
        std::unordered_map<std::string, bcn::overlay::Entry> merged;
        for (const auto& root : bcn::catalog_roots::Discover(logicalRoot)) {
            const auto provider = bcn::path_text::GenericUtf8(root);
            for (auto& entry : bcn::overlay::ScanSlaveTatsDirectory(root)) {
                ResolveInstalledMetadata(entry, provider);
                merged.insert_or_assign(entry.id, std::move(entry));
            }
        }
        std::vector<bcn::overlay::Entry> result;
        result.reserve(merged.size());
        for (auto& [id, entry] : merged) result.push_back(std::move(entry));
        return result;
    }

    [[nodiscard]] std::vector<bcn::overlay::Entry> LoadSlaveTatsCatalog()
    {
        {
            std::scoped_lock lock(g_slaveTatsLock);
            if (g_slaveTatsLoaded) return g_slaveTatsCache;
        }
        auto scanned = ScanSlaveTatsCatalog();
        std::scoped_lock lock(g_slaveTatsLock);
        if (!g_slaveTatsLoaded) {
            g_slaveTatsCache = std::move(scanned);
            g_slaveTatsLoaded = true;
        }
        return g_slaveTatsCache;
    }

    void FinishRequest()
    {
        if (!g_requested.exchange(false)) return;
        if (auto* events = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent restore{};
            restore.eventName = "RSMDT_SendRestore";
            events->SendEvent(&restore);
        }
        std::size_t count{};
        RE::ActorHandle retryTarget;
        {
            std::scoped_lock lock(g_catalogLock);
            for (const auto& area : g_catalog) count += area.size();
            if (count == 0U && g_emptyResponseRetries != 0U) {
                --g_emptyResponseRetries;
                retryTarget = g_requestTarget;
            }
        }
        if (retryTarget) {
            const auto retryQueued = bcn::frame_tasks::Queue(0U,
                [retryTarget] { RequestWhenHudReady(retryTarget, 100U); }, 60U,
                bcn::appearance::WorkChannel::overlayCatalogRequest);
            if (retryQueued) {
                SKSE::log::debug("Body Change NG RaceMenu overlay catalog returned no entries; retrying without opening RaceMenu");
                return;
            }
        }
        g_catalogSettled.store(true, std::memory_order_release);
        const auto queued = bcn::Distribution::Get().ApplyLoadedNPCs();
        SKSE::log::info("Body Change NG received {} RaceMenu overlay entries; queued {} NPC(s) after catalog settlement",
            count, queued);
    }

    void ScheduleFinish(const std::uint32_t delay)
    {
        [[maybe_unused]] const auto queued = bcn::frame_tasks::Queue(0U, FinishRequest, delay,
            bcn::appearance::WorkChannel::overlayCatalog);
    }

    [[nodiscard]] bool HudReady()
    {
        auto* ui = RE::UI::GetSingleton();
        return ui && ui->IsMenuOpen(kMenuName);
    }

    bool SendCatalogRequest(RE::Actor* target)
    {
        if (!target || !SKSE::GetModCallbackEventSource() || !HudReady()) return false;
        if (g_requested.exchange(true, std::memory_order_acq_rel)) return true;
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([] {
                auto* ui = RE::UI::GetSingleton();
                const auto menu = ui ? ui->GetMenu(kMenuName) : nullptr;
                if (menu) bcn::overlay::UpdateProviderCounts(menu->uiMovie.get());
            });
        }
        {
            std::scoped_lock lock(g_catalogLock);
            for (auto& area : g_catalog) area.clear();
        }
        const auto slaveTats = LoadSlaveTatsCatalog();
        {
            std::scoped_lock lock(g_catalogLock);
            for (const auto& entry : slaveTats) {
                g_catalog[bcn::overlay::Index(entry.area)].insert_or_assign(entry.id, entry);
            }
        }
        g_revision.fetch_add(1U, std::memory_order_release);
        SendModEvent("RSMDT_SendTargetActor", {}, 0.0F, target);
        SendModEvent("RSMDT_SendMenuName", kMenuName);
        SendModEvent("RSMDT_SendRootName", kRootName);
        SendModEvent("RSMDT_SendDataRequest", {}, kPaintRequestMask);
        ScheduleFinish(180U);
        return true;
    }

    void RequestWhenHudReady(RE::ActorHandle handle, const std::uint32_t retries)
    {
        const auto current = handle.get();
        if (!current) return;
        if (SendCatalogRequest(current.get()) || retries == 0U) return;
        [[maybe_unused]] const auto queued = bcn::frame_tasks::Queue(0U,
            [handle, retries] { RequestWhenHudReady(handle, retries - 1U); }, 6U,
            bcn::appearance::WorkChannel::overlayCatalogRequest);
    }

    void Receive(const bcn::overlay::Area area, RE::GFxFunctionHandler::Params& params)
    {
        bool changed{};
        std::scoped_lock lock(g_catalogLock);
        auto& entries = g_catalog[bcn::overlay::Index(area)];
        for (std::uint32_t index{}; index < params.argCount; ++index) {
            const auto& value = params.args[index];
            if (!value.IsString()) continue;
            const std::string_view packed{ value.GetString() ? value.GetString() : "" };
            const auto separator = packed.find(";;");
            if (separator == std::string_view::npos) continue;
            auto name = std::string{ packed.substr(0U, separator) };
            auto texturePath = std::string{ packed.substr(separator + 2U) };
            if (name.empty() || texturePath.empty() ||
                bcn::overlay::IsRaceMenuDefaultTexture(texturePath)) continue;
            auto entry = bcn::overlay::Entry{
                // The visible name may be localized by its paint provider.
                // Area + normalized registered path remains stable across UI
                // language changes and therefore owns the persistent ID.
                .id = bcn::overlay::StableId(area, texturePath),
                .name = std::move(name),
                .texturePath = std::move(texturePath),
                .area = area,
                .source = bcn::overlay::Source::raceMenu
            };
            ResolveInstalledMetadata(entry);
            changed |= entries.insert_or_assign(entry.id, std::move(entry)).second;
        }
        if (changed) g_revision.fetch_add(1U, std::memory_order_release);
        // Each callback moves the quiet-period boundary. Work-channel
        // coalescing guarantees that only the last settlement task survives.
        ScheduleFinish(45U);
    }

    template <bcn::overlay::Area AreaValue>
    class PaintReceiver final : public RE::GFxFunctionHandler
    {
    public:
        void Call(Params& params) override { Receive(AreaValue, params); }
    };

    bool RegisterScaleform(RE::GFxMovieView* view, RE::GFxValue* root)
    {
        if (!view || !root) return false;
        const auto add = [&](const char* name, RE::GFxFunctionHandler* handler) {
            RE::GFxValue function;
            view->CreateFunction(&function, handler);
            return root->SetMember(name, function);
        };
        const auto ok =
            add("RSM_AddFacePaints", new PaintReceiver<bcn::overlay::Area::face>()) &&
            add("RSM_AddBodyPaints", new PaintReceiver<bcn::overlay::Area::body>()) &&
            add("RSM_AddHandPaints", new PaintReceiver<bcn::overlay::Area::hands>()) &&
            add("RSM_AddFeetPaints", new PaintReceiver<bcn::overlay::Area::feet>());
        SKSE::log::info("Body Change NG RaceMenu overlay catalog Scaleform bridge {}",
            ok ? "registered" : "failed");
        return ok;
    }

    void SendModEvent(const char* name, const std::string_view text,
        const float number, RE::TESForm* sender)
    {
        if (auto* events = SKSE::GetModCallbackEventSource()) {
            SKSE::ModCallbackEvent event{};
            event.eventName = name;
            event.strArg = text;
            event.numArg = number;
            event.sender = sender;
            events->SendEvent(&event);
        }
    }
}

namespace bcn::overlay
{
    namespace
    {
        bool BeginCatalogRequest(RE::Actor* target, const bool force)
        {
            if (!target || !InitializeCatalogBridge() || !SKSE::GetModCallbackEventSource()) return false;
            if (!force && g_catalogSettled.load(std::memory_order_acquire)) return true;
            if (g_requested.load(std::memory_order_acquire)) return true;
            if (force) g_catalogSettled.store(false, std::memory_order_release);
            {
                std::scoped_lock lock(g_catalogLock);
                g_requestTarget = target->GetHandle();
                g_emptyResponseRetries = kEmptyResponseRetryCount;
            }
            if (!SendCatalogRequest(target)) {
                const auto handle = target->GetHandle();
                if (!bcn::frame_tasks::Queue(0U,
                        [handle] { RequestWhenHudReady(handle, 100U); }, 1U,
                        bcn::appearance::WorkChannel::overlayCatalogRequest)) return false;
            }
            return true;
        }
    }

    void ResolveInstalledEntryMetadata(Entry& entry)
    {
        ResolveInstalledMetadata(entry);
    }

    bool InitializeCatalogBridge()
    {
        if (g_bridgeRegistered.load(std::memory_order_acquire)) return true;
        const auto* scaleform = SKSE::GetScaleformInterface();
        if (!scaleform || !scaleform->Register(RegisterScaleform, "BodyChangeNG")) {
            SKSE::log::error("Body Change NG could not register the RaceMenu overlay catalog bridge");
            return false;
        }
        g_bridgeRegistered.store(true, std::memory_order_release);
        return true;
    }

    bool RequestCatalog(RE::Actor* target)
    {
        return BeginCatalogRequest(target, false);
    }

    bool RefreshCatalog(RE::Actor* target) { return BeginCatalogRequest(target, true); }

    void EndCatalogRequest() { FinishRequest(); }

    std::vector<Entry> Snapshot(const Area area)
    {
        if (area == Area::count) return {};
        std::scoped_lock lock(g_catalogLock);
        std::vector<Entry> result;
        result.reserve(g_catalog[Index(area)].size());
        for (const auto& [id, entry] : g_catalog[Index(area)]) result.push_back(entry);
        std::ranges::sort(result, {}, &Entry::name);
        return result;
    }

    std::vector<Entry> SnapshotForActor(const Area area, RE::Actor* actor)
    {
        if (!actor) return {};
        const auto* base = actor->GetActorBase();
        const auto female = base && base->GetSex() == RE::SEX::kFemale;
        const auto family = body_family::ResolveActor(actor);
        auto result = Snapshot(area);
        std::erase_if(result, [female, family](const Entry& entry) {
            return !EntryMatchesActor(entry.layout, entry.sex, family, female);
        });
        return result;
    }

    std::vector<Entry> SnapshotLegacy(const Area area, const bool female)
    {
        auto result = Snapshot(area);
        std::erase_if(result, [female](const Entry& entry) {
            return entry.layout != Layout::legacy || !SexMatches(entry.sex, female);
        });
        return result;
    }

    std::optional<Entry> Find(const std::string_view id)
    {
        std::scoped_lock lock(g_catalogLock);
        for (const auto& area : g_catalog) {
            const auto found = area.find(std::string{ id });
            if (found != area.end()) return found->second;
        }
        return std::nullopt;
    }

    std::uint64_t CatalogRevision() noexcept { return g_revision.load(std::memory_order_acquire); }
    bool CatalogRequested() noexcept { return g_requested.load(std::memory_order_acquire); }

    void ResetCatalogSessionState()
    {
        g_requested.store(false, std::memory_order_release);
        g_catalogSettled.store(false, std::memory_order_release);
        std::scoped_lock lock(g_catalogLock);
        for (auto& area : g_catalog) area.clear();
        g_requestTarget.reset();
        g_emptyResponseRetries = 0U;
        g_revision.fetch_add(1U, std::memory_order_release);
    }
}
