#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/CatalogRoots.h"
#include "BodyChangeNG/PathText.h"
#include "BodyChangeNG/RuntimeAssetCache.h"

#include "BodyChangeNG/Settings.h"

#include <RE/P/PackUnpack.h>
#include <SKSE/Logger.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <map>
#include <ranges>
#include <set>
#include <system_error>
#include <tuple>

namespace
{
    void QueueTintTask(std::uint32_t actor, std::function<void()> work)
    {
        bcn::frame_tasks::Queue(actor, std::move(work), 1, 205, true);
    }
    constexpr std::size_t kMaxAssets = 512;
    std::atomic_uint64_t g_tintGeneration{};
    struct LayerOverride final
    {
        std::string assetID;
        bcn::player_tint::Color color;
    };

    struct CurrentTintState final
    {
        std::optional<std::string> pack;
        std::map<bcn::player_tint::Layer, LayerOverride> layerOverrides;
        std::set<bcn::player_tint::Layer> restoredLayers;
    };

    std::mutex g_currentTintStateLock;
    CurrentTintState g_currentTintState;

    [[nodiscard]] CurrentTintState CurrentState()
    {
        std::scoped_lock lock(g_currentTintStateLock);
        return g_currentTintState;
    }

    void SetCurrentPack(std::optional<std::string> pack)
    {
        std::scoped_lock lock(g_currentTintStateLock);
        g_currentTintState.pack = std::move(pack);
        g_currentTintState.layerOverrides.clear();
        g_currentTintState.restoredLayers.clear();
    }

    void RecordLayerOverride(const bcn::player_tint::Asset& asset,
        const bcn::player_tint::Color& color)
    {
        std::scoped_lock lock(g_currentTintStateLock);
        // The detailed editor is opened from a selected pack, so this pack is
        // the reproducible base even if its preceding game task was superseded
        // by a very fast color-picker input.
        g_currentTintState.pack = asset.pack;
        g_currentTintState.layerOverrides[asset.layer] = { asset.id, color };
        g_currentTintState.restoredLayers.erase(asset.layer);
    }

    void RecordLayerRestore(const bcn::player_tint::Layer layer)
    {
        std::scoped_lock lock(g_currentTintStateLock);
        g_currentTintState.layerOverrides.erase(layer);
        g_currentTintState.restoredLayers.insert(layer);
    }

    class DiscardPapyrusResult final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        void operator()(RE::BSScript::Variable) override {}
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };

    [[nodiscard]] std::string Lower(const std::string_view value)
    {
        std::string result{ value };
        std::ranges::transform(result, result.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return result;
    }

    [[nodiscard]] std::optional<std::filesystem::path> RelativeWithin(
        const std::filesystem::path& path, const std::filesystem::path& root)
    {
        auto relative = path.lexically_relative(root);
        if (relative.empty() || relative.is_absolute()) return std::nullopt;
        for (const auto& component : relative) {
            if (component == "..") return std::nullopt;
        }
        return relative;
    }

    [[nodiscard]] std::optional<bcn::player_tint::Layer> InferLayer(const std::string_view filename)
    {
        const auto lower = Lower(filename);
        using Layer = bcn::player_tint::Layer;
        if (lower.find("frekl") != std::string::npos || lower.find("freckl") != std::string::npos) return Layer::frekles;
        if (lower.find("lips") != std::string::npos) return Layer::lips;
        if (lower.find("lowercheek") != std::string::npos || lower.find("cheeks2") != std::string::npos) return Layer::lowerCheeks;
        if (lower.find("cheeks") != std::string::npos) return Layer::cheeks;
        if (lower.find("eyeliner") != std::string::npos) return Layer::eyeliner;
        if (lower.find("uppereye") != std::string::npos) return Layer::upperEyeSocket;
        if (lower.find("lowereye") != std::string::npos) return Layer::lowerEyeSocket;
        if (lower.find("skintone") != std::string::npos) return Layer::skinTone;
        if (lower.find("warpaint") != std::string::npos) return Layer::warPaint;
        if (lower.find("frown") != std::string::npos) return Layer::frownLines;
        if (lower.find("forehead") != std::string::npos) return Layer::forehead;
        if (lower.find("neck") != std::string::npos) return Layer::neck;
        if (lower.find("chin") != std::string::npos) return Layer::chin;
        if (lower.find("nose") != std::string::npos) return Layer::nose;
        if (lower.find("dirt") != std::string::npos) return Layer::dirt;
        return std::nullopt;
    }

    [[nodiscard]] bcn::player_tint::Sex InferSex(const std::string_view filename)
    {
        const auto lower = Lower(filename);
        if (lower.starts_with("female")) return bcn::player_tint::Sex::female;
        if (lower.starts_with("male")) return bcn::player_tint::Sex::male;
        return bcn::player_tint::Sex::unisex;
    }

    [[nodiscard]] bcn::body_family::Mask InferTintFamilies(
        const std::string_view pack, const std::string_view path,
        const bcn::player_tint::Sex sex)
    {
        using namespace bcn::body_family;
        const auto text = Lower(std::string(pack) + ' ' + std::string(path));
        const auto standardFemale = Bit(Family::femaleVanilla) |
            Bit(Family::cbbe) | Bit(Family::unp);
        // UBE documents COtR makeup/warpaints as compatible; keep those packs
        // visible to both UBE and conventional female heads.
        if (text.find("cotr") != std::string::npos || text.find("cot r") != std::string::npos) {
            return kFemaleFamilies;
        }
        const auto explicitFemale = DetectText(text, bcn::body_family::Sex::female) &
            NonVanillaFamilies(bcn::body_family::Sex::female);
        if (explicitFemale != 0U) return explicitFemale;
        if (sex == bcn::player_tint::Sex::male) return kMaleFamilies;
        if (sex == bcn::player_tint::Sex::unisex) return standardFemale | kMaleFamilies;
        return standardFemale;
    }

    [[nodiscard]] RE::TintMask::Type ToGameLayer(const bcn::player_tint::Layer layer)
    {
        using Layer = bcn::player_tint::Layer;
        switch (layer) {
        case Layer::frekles: return RE::TintMask::Type::kFrekles;
        case Layer::lips: return RE::TintMask::Type::kLips;
        case Layer::cheeks: return RE::TintMask::Type::kCheeks;
        case Layer::eyeliner: return RE::TintMask::Type::kEyeliner;
        case Layer::upperEyeSocket: return RE::TintMask::Type::kUpperEyeSocket;
        case Layer::lowerEyeSocket: return RE::TintMask::Type::kLowerEyeSocket;
        case Layer::skinTone: return RE::TintMask::Type::kSkinTone;
        case Layer::warPaint: return RE::TintMask::Type::kWarPaint;
        case Layer::frownLines: return RE::TintMask::Type::kFrownLines;
        case Layer::lowerCheeks: return RE::TintMask::Type::kLowerCheeks;
        case Layer::nose: return RE::TintMask::Type::kNose;
        case Layer::chin: return RE::TintMask::Type::kChin;
        case Layer::neck: return RE::TintMask::Type::kNeck;
        case Layer::forehead: return RE::TintMask::Type::kForehead;
        case Layer::dirt: return RE::TintMask::Type::kDirt;
        }
        return RE::TintMask::Type::kFrekles;
    }

    struct PlayerTintArrays final
    {
        RE::BSTArray<RE::TintMask*>* base{};
        RE::BSTArray<RE::TintMask*>* overlay{};
    };

    [[nodiscard]] PlayerTintArrays PlayerTintLists(RE::PlayerCharacter* player)
    {
        // CommonLibSSE-NG 6.7.0 declares PlayerCharacter::GetTintList(), but
        // its implementation is not exported by the plugin consumer target.
        // Keep the exact upstream versioned member selection in this one
        // adapter instead of touching an unconditional PlayerCharacter offset.
        // Upstream explicitly reports no player tint list for VR, so VR fails
        // closed rather than probing an unverified layout.
        if (!player || REL::Module::IsVR()) return {};
        const auto version = REL::Module::get().version();
        const auto newestLayout = version.compare(SKSE::RUNTIME_SSE_1_7_99) != std::strong_ordering::less;
        const auto aeLayout = version.compare(SKSE::RUNTIME_SSE_1_6_629) != std::strong_ordering::less;
        const auto baseOffset = newestLayout ? 0xB20 : aeLayout ? 0xB18 : 0xB10;
        const auto overlayOffset = newestLayout ? 0xB38 : aeLayout ? 0xB30 : 0xB28;
        auto* base = &REL::RelocateMember<RE::BSTArray<RE::TintMask*>>(player, baseOffset);
        auto* overlayTints = REL::RelocateMember<RE::BSTArray<RE::TintMask*>*>(player, overlayOffset);
        return { base, overlayTints };
    }

    void LogPlayerTintState(RE::PlayerCharacter* player, const std::string_view reason)
    {
        const auto lists = PlayerTintLists(player);
        const auto logList = [&](const char* listName, const RE::BSTArray<RE::TintMask*>* list) {
            if (!list) {
                SKSE::log::info("TintAudit {} list={} unavailable", reason, listName);
                return;
            }
            SKSE::log::info("TintAudit {} list={} count={}", reason, listName, list->size());
            for (std::uint32_t index{}; index < list->size(); ++index) {
                const auto* mask = (*list)[index];
                if (!mask) {
                    SKSE::log::info("TintAudit {} list={} index={} null", reason, listName, index);
                    continue;
                }
                const auto* path = mask->texture ? mask->texture->textureName.c_str() : nullptr;
                SKSE::log::info(
                    "TintAudit {} list={} index={} type={} path='{}' rgb=({},{},{}) alpha={:.4f}",
                    reason, listName, index, mask->type.underlying(), path ? path : "",
                    static_cast<std::uint32_t>(mask->color.red), static_cast<std::uint32_t>(mask->color.green),
                    static_cast<std::uint32_t>(mask->color.blue), mask->alpha);
            }
        };
        logList("base", lists.base);
        logList("overlay", lists.overlay);
    }

    [[nodiscard]] RE::TintMask* FindPlayerMask(RE::PlayerCharacter* player, const bcn::player_tint::Layer layer)
    {
        const auto expected = ToGameLayer(layer);
        const auto lists = PlayerTintLists(player);
        if (!lists.base) return nullptr;
        // RaceMenu overlay masks do not reliably carry their tint type. Match
        // the typed base mask first and then use the same array index, exactly
        // like SKSE's SetTintMaskTexturePath implementation.
        for (std::uint32_t index{}; index < lists.base->size(); ++index) {
            auto* baseMask = (*lists.base)[index];
            if (!baseMask || !baseMask->texture || baseMask->type != expected) continue;
            if (lists.overlay && index < lists.overlay->size()) {
                auto* overlayMask = (*lists.overlay)[index];
                if (overlayMask && overlayMask->texture) return overlayMask;
            }
            return baseMask;
        }
        return nullptr;
    }

    template <class Callback>
    std::size_t ForEachPlayerMask(RE::PlayerCharacter* player,
        const bcn::player_tint::Layer layer, Callback&& callback)
    {
        const auto expected = ToGameLayer(layer);
        const auto lists = PlayerTintLists(player);
        if (!lists.base) return 0U;
        std::size_t applied{};
        for (std::uint32_t index{}; index < lists.base->size(); ++index) {
            auto* baseMask = (*lists.base)[index];
            if (!baseMask || !baseMask->texture || baseMask->type != expected) continue;
            callback(*baseMask);
            ++applied;
            if (lists.overlay && index < lists.overlay->size()) {
                auto* overlayMask = (*lists.overlay)[index];
                if (overlayMask && overlayMask->texture && overlayMask != baseMask) {
                    callback(*overlayMask);
                    ++applied;
                }
            }
        }
        return applied;
    }

    [[nodiscard]] std::uint8_t Channel(const float value)
    {
        return static_cast<std::uint8_t>(std::clamp(value, 0.0F, 1.0F) * 255.0F + 0.5F);
    }

    [[nodiscard]] std::string TintMaskTexturePath(const std::filesystem::path& source)
    {
        auto path = bcn::runtime_assets::TexturePath(source, "tint");
        // BSTextureSet/NIF texture strings include the leading "textures\\",
        // but SKSE's Game.SetTintMaskTexturePath and the live TintMask arrays
        // store paths relative to the textures directory.  The player's
        // original masks are e.g. "Actors\\Character\\...", not
        // "textures\\Actors\\Character\\...".  Keeping the prefix here
        // materializes the DDS successfully but makes the tint compositor look
        // for Data\\textures\\textures\\..., so the visual never changes.
        constexpr std::string_view prefix = "textures\\";
        if (Lower(path).starts_with(prefix)) path.erase(0, prefix.size());
        return path;
    }

    void CaptureOriginalIfNeeded(RE::TintMask& mask, const bcn::player_tint::Layer layer)
    {
        auto settings = bcn::Settings::Get().Snapshot();
        const auto type = static_cast<std::uint8_t>(layer);
        if (std::ranges::find(settings.playerTintBackups, type, &bcn::PlayerTintBackup::type) != settings.playerTintBackups.end()) return;
        const auto* path = mask.texture ? mask.texture->textureName.c_str() : nullptr;
        if (!path || path[0] == '\0') return;
        settings.playerTintBackups.push_back({
            .type = type,
            .texturePath = path,
            .color = { mask.color.red, mask.color.green, mask.color.blue },
            .alpha = std::clamp(mask.alpha, 0.0F, 1.0F)
        });
        bcn::Settings::Get().Update(settings);
        [[maybe_unused]] const auto saved = bcn::Settings::Get().Save();
    }

    [[nodiscard]] bool IsCurrentTintChange(const std::uint64_t generation) noexcept
    {
        return g_tintGeneration.load(std::memory_order_acquire) == generation;
    }

    [[nodiscard]] bool RefreshPlayerTints()
    {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) return false;
        // This is SKSE's own tint refresh path: UpdateSkinColor followed by
        // rebuilding the player's tint composite. RegenerateHead/QueueNiNodeUpdate
        // can instead reload the old FaceGen state and hide a successful change.
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new DiscardPapyrusResult());
        const auto dispatched = vm->DispatchStaticCall(
            "Game", "UpdateTintMaskColors", RE::MakeFunctionArguments(), callback);
        SKSE::log::info("TintAudit Game.UpdateTintMaskColors dispatched={}", dispatched);
        return dispatched;
    }

    void ApplyNow(RE::ActorHandle playerHandle, const bcn::player_tint::Asset asset,
        const bcn::player_tint::Color color, const std::uint64_t generation)
    {
        const auto actor = playerHandle.get();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || !player || actor->GetFormID() != player->GetFormID() || !actor->Is3DLoaded()) return;
        if (!IsCurrentTintChange(generation)) return;
        const auto playerSex = actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale ?
            bcn::player_tint::Sex::female : bcn::player_tint::Sex::male;
        if (asset.sex != bcn::player_tint::Sex::unisex && asset.sex != playerSex) return;
        if (!bcn::player_tint::TintMatchesActor(
                asset.bodyFamilies, bcn::body_family::ResolveActor(actor.get()))) return;
        const auto path = TintMaskTexturePath(asset.source);
        if (path.empty()) {
            SKSE::log::error("Body Change NG could not cache tint asset '{}'", asset.name);
            return;
        }
        SKSE::log::info(
            "TintAudit expected asset='{}' pack='{}' layer={} source='{}' cache='{}' rgba=({:.4f},{:.4f},{:.4f},{:.4f})",
            asset.name, asset.pack, static_cast<std::uint32_t>(asset.layer),
            bcn::path_text::Utf8(asset.source), path, color.red, color.green, color.blue, color.alpha);
        if (auto* original = FindPlayerMask(player, asset.layer)) CaptureOriginalIfNeeded(*original, asset.layer);
        const auto applied = ForEachPlayerMask(player, asset.layer, [&](RE::TintMask& mask) {
            mask.texture->textureName = path.c_str();
            mask.color.red = Channel(color.red);
            mask.color.green = Channel(color.green);
            mask.color.blue = Channel(color.blue);
            mask.alpha = std::clamp(color.alpha, 0.0F, 1.0F);
        });
        if (applied == 0U) {
            SKSE::log::warn("Body Change NG could not apply tint '{}': the player has no matching active layer", asset.name);
            return;
        }
        if (IsCurrentTintChange(generation)) {
            [[maybe_unused]] const auto refreshed = RefreshPlayerTints();
            RecordLayerOverride(asset, color);
        }
        SKSE::log::info("Body Change NG applied player tint '{}' to {} base/overlay mask(s)", asset.name, applied);
    }

    void ApplyPackNow(RE::ActorHandle playerHandle, const std::vector<bcn::player_tint::Asset> assets,
        const std::uint64_t generation)
    {
        const auto actor = playerHandle.get();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || !player || actor->GetFormID() != player->GetFormID() || !actor->Is3DLoaded()) return;
        if (!IsCurrentTintChange(generation)) return;
        std::size_t applied{};
        for (const auto& asset : assets) {
            const auto path = TintMaskTexturePath(asset.source);
            if (path.empty()) continue;
            SKSE::log::info(
                "TintAudit expected pack='{}' asset='{}' layer={} source='{}' cache='{}'",
                asset.pack, asset.name, static_cast<std::uint32_t>(asset.layer),
                bcn::path_text::Utf8(asset.source), path);
            if (auto* original = FindPlayerMask(player, asset.layer)) CaptureOriginalIfNeeded(*original, asset.layer);
            applied += ForEachPlayerMask(player, asset.layer, [&path](RE::TintMask& mask) {
                mask.texture->textureName = path.c_str();
            });
        }
        if (applied != 0U && IsCurrentTintChange(generation)) {
            [[maybe_unused]] const auto refreshed = RefreshPlayerTints();
            SetCurrentPack(assets.front().pack);
        }
        SKSE::log::info("Body Change NG applied tint pack with {} mapped layers", applied);
    }

    void RestoreNow(RE::ActorHandle playerHandle, const bcn::player_tint::Layer layer,
        const bcn::PlayerTintBackup backup, const std::uint64_t generation)
    {
        const auto actor = playerHandle.get();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || !player || actor->GetFormID() != player->GetFormID() || !actor->Is3DLoaded()) return;
        if (!IsCurrentTintChange(generation)) return;
        const auto restored = ForEachPlayerMask(player, layer, [&backup](RE::TintMask& mask) {
            mask.texture->textureName = backup.texturePath.c_str();
            mask.color.red = backup.color[0];
            mask.color.green = backup.color[1];
            mask.color.blue = backup.color[2];
            mask.alpha = std::clamp(backup.alpha, 0.0F, 1.0F);
        });
        if (restored == 0U) return;
        if (IsCurrentTintChange(generation)) {
            [[maybe_unused]] const auto refreshed = RefreshPlayerTints();
            RecordLayerRestore(layer);
        }
        SKSE::log::info("Body Change NG restored original player tint layer {}", static_cast<std::uint32_t>(layer));
    }

    void RestoreAllNow(RE::ActorHandle playerHandle, const std::vector<bcn::PlayerTintBackup> backups,
        const std::uint64_t generation)
    {
        const auto actor = playerHandle.get();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || !player || actor->GetFormID() != player->GetFormID() || !actor->Is3DLoaded()) return;
        if (!IsCurrentTintChange(generation)) return;
        std::size_t restored{};
        for (const auto& backup : backups) {
            const auto layer = static_cast<bcn::player_tint::Layer>(backup.type);
            restored += ForEachPlayerMask(player, layer, [&backup](RE::TintMask& mask) {
                mask.texture->textureName = backup.texturePath.c_str();
                mask.color.red = backup.color[0];
                mask.color.green = backup.color[1];
                mask.color.blue = backup.color[2];
                mask.alpha = std::clamp(backup.alpha, 0.0F, 1.0F);
            });
        }
        if (restored != 0U && IsCurrentTintChange(generation)) {
            [[maybe_unused]] const auto refreshed = RefreshPlayerTints();
            SetCurrentPack(std::nullopt);
        }
        SKSE::log::info("Body Change NG restored {} original player tint layers", restored);
    }

    [[nodiscard]] std::string_view Filename(const std::string_view path)
    {
        const auto separator = path.find_last_of("\\/");
        return separator == std::string_view::npos ? path : path.substr(separator + 1U);
    }

    [[nodiscard]] std::string RaceFilenameToken(RE::Actor* actor)
    {
        const auto* race = actor ? actor->GetRace() : nullptr;
        const auto* editorID = race ? race->GetFormEditorID() : nullptr;
        const auto id = editorID ? Lower(editorID) : std::string{};
        for (const auto& [needle, token] : std::array<std::pair<std::string_view, std::string_view>, 12>{
                 std::pair{ "breton", "breton" }, std::pair{ "nord", "nord" },
                 std::pair{ "darkelf", "darkelf" }, std::pair{ "dunmer", "darkelf" },
                 std::pair{ "highelf", "highelf" }, std::pair{ "altmer", "highelf" },
                 std::pair{ "woodelf", "woodelf" }, std::pair{ "bosmer", "woodelf" },
                 std::pair{ "imperial", "imperial" }, std::pair{ "redguard", "redguard" },
                 std::pair{ "orc", "orc" }, std::pair{ "khajiit", "khajiit" } }) {
            if (id.find(needle) != std::string::npos) return std::string{ token };
        }
        if (id.find("argonian") != std::string::npos) return "argonian";
        return {};
    }

    [[nodiscard]] std::string AssetRaceToken(const std::string_view name)
    {
        const auto lower = Lower(name);
        for (const auto token : std::array<std::string_view, 10>{
                 "argonian", "khajiit", "redguard", "imperial", "darkelf",
                 "highelf", "woodelf", "breton", "nord", "orc" }) {
            if (lower.find(token) != std::string::npos) return std::string{ token };
        }
        return {};
    }

    [[nodiscard]] std::optional<bcn::player_tint::Asset> BestAssetForPlayer(
        RE::PlayerCharacter* player, const std::vector<bcn::player_tint::Asset>& catalog,
        const std::string_view pack, const bcn::player_tint::Layer layer)
    {
        if (!player || !FindPlayerMask(player, layer)) return std::nullopt;
        const auto* base = player->GetActorBase();
        const auto playerSex = base && base->GetSex() == RE::SEX::kFemale ?
            bcn::player_tint::Sex::female : bcn::player_tint::Sex::male;
        const auto raceToken = RaceFilenameToken(player);
        const auto playerFamily = bcn::body_family::ResolveActor(player);
        const auto* current = FindPlayerMask(player, layer);
        const auto currentName = current && current->texture ?
            Lower(Filename(current->texture->textureName.c_str())) : std::string{};
        const bcn::player_tint::Asset* best{};
        auto bestScore = -1;
        for (const auto& asset : catalog) {
            if (asset.pack != pack || asset.layer != layer ||
                (asset.sex != bcn::player_tint::Sex::unisex && asset.sex != playerSex)) {
                continue;
            }
            if (!bcn::player_tint::TintMatchesActor(asset.bodyFamilies, playerFamily)) continue;
            const auto filename = Lower(Filename(asset.path));
            const auto exactCurrent = !currentName.empty() && filename == currentName;
            const auto assetRace = AssetRaceToken(asset.name);
            // A DDS explicitly authored for another race is never a fallback.
            // Custom races can still use the exact texture already assigned by
            // RaceMenu, or a genuinely race-neutral asset from the pack.
            if (!exactCurrent && !assetRace.empty() && assetRace != raceToken) continue;
            auto score = 1;
            if (exactCurrent) score += 1000;
            if (!raceToken.empty() && assetRace == raceToken) score += 500;
            if (assetRace.empty()) score += 100;
            if (Lower(asset.name).find("human") != std::string::npos) score += 50;
            if (!best || score > bestScore) {
                best = &asset;
                bestScore = score;
            }
        }
        return best ? std::optional<bcn::player_tint::Asset>{ *best } : std::nullopt;
    }

    [[nodiscard]] std::vector<bcn::player_tint::Asset> BestPackAssetsForPlayer(
        RE::PlayerCharacter* player, const std::vector<bcn::player_tint::Asset>& catalog,
        const std::string_view pack)
    {
        std::vector<bcn::player_tint::Asset> selected;
        for (std::uint8_t raw{}; raw <= static_cast<std::uint8_t>(bcn::player_tint::Layer::dirt); ++raw) {
            if (auto asset = BestAssetForPlayer(player, catalog, pack,
                    static_cast<bcn::player_tint::Layer>(raw))) {
                selected.push_back(std::move(*asset));
            }
        }
        return selected;
    }
}

namespace bcn::player_tint
{
    Catalog& Catalog::Get()
    {
        static Catalog catalog;
        return catalog;
    }

    std::filesystem::path Catalog::RootPath()
    {
        return std::filesystem::current_path() / "Data" / "TintMask";
    }

    std::vector<Asset> Catalog::ScanDirectory(const std::filesystem::path& root)
    {
        struct PackAudit final
        {
            std::size_t ddsTotal{};
            std::size_t tintDirectory{};
            std::size_t recognized{};
            std::size_t unrecognized{};
            std::size_t outsideTintDirectory{};
        };
        const auto dataRoot = root.parent_path();
        std::vector<Asset> loaded;
        std::map<std::string, PackAudit> audit;
        std::error_code error;
        if (std::filesystem::exists(root, error)) {
            for (std::filesystem::recursive_directory_iterator it(root,
                     std::filesystem::directory_options::skip_permission_denied, error), end;
                 it != end; it.increment(error)) {
                if (error) {
                    error.clear();
                    continue;
                }
                std::error_code statusError;
                if (!it->is_regular_file(statusError) || statusError || Lower(bcn::path_text::Utf8(it->path().extension())) != ".dds") continue;
                const auto rootRelative = RelativeWithin(it->path(), root);
                if (!rootRelative) continue;
                const auto first = rootRelative->begin();
                if (first == rootRelative->end()) continue;
                const auto pack = bcn::path_text::Utf8(*first);
                if (pack.empty()) continue;
                auto& packAudit = audit[pack];
                ++packAudit.ddsTotal;
                const auto parent = Lower(bcn::path_text::Utf8(it->path().parent_path().filename()));
                if (parent != "tintmasks") {
                    ++packAudit.outsideTintDirectory;
                    SKSE::log::info("TintCatalogAudit pack='{}' file='{}' mapping=outside-tintmasks",
                        pack, bcn::path_text::Utf8(it->path()));
                    continue;
                }
                ++packAudit.tintDirectory;
                const auto filename = bcn::path_text::Utf8(it->path().filename());
                const auto layer = InferLayer(filename);
                if (!layer) {
                    ++packAudit.unrecognized;
                    SKSE::log::info("TintCatalogAudit pack='{}' file='{}' mapping=unrecognized-layer",
                        pack, bcn::path_text::Utf8(it->path()));
                    continue;
                }
                if (loaded.size() >= kMaxAssets) continue;
                const auto dataRelative = RelativeWithin(it->path(), dataRoot);
                if (!dataRelative || !rootRelative) continue;
                auto path = bcn::path_text::GenericUtf8(*dataRelative);
                auto id = bcn::path_text::GenericUtf8(*rootRelative);
                if (path.size() > 1024U) continue;
                std::ranges::replace(path, '/', '\\');
                std::ranges::replace(id, '/', '\\');
                ++packAudit.recognized;
                SKSE::log::info(
                    "TintCatalogAudit pack='{}' file='{}' mapping=player-tint-layer layer={} sex={} families={}",
                    pack, bcn::path_text::Utf8(it->path()), static_cast<std::uint32_t>(*layer),
                    InferSex(filename) == Sex::female ? "female" : InferSex(filename) == Sex::male ? "male" : "unisex",
                    InferTintFamilies(pack, path, InferSex(filename)));
                loaded.push_back({
                    .id = std::move(id),
                    .pack = pack,
                    .name = bcn::path_text::Utf8(it->path().stem()),
                    .path = path,
                    .layer = *layer,
                    .sex = InferSex(filename),
                    .bodyFamilies = InferTintFamilies(pack, path, InferSex(filename)),
                    .source = it->path()
                });
            }
        }
        if (error) SKSE::log::warn("Body Change NG could not scan player tint masks at {}: {}", bcn::path_text::Utf8(root), error.message());
        for (const auto& [pack, counts] : audit) {
            SKSE::log::info(
                "TintCatalogAudit pack='{}' dds-total={} tintmasks-dds={} recognized={} unrecognized={} outside-tintmasks={}",
                pack, counts.ddsTotal, counts.tintDirectory, counts.recognized,
                counts.unrecognized, counts.outsideTintDirectory);
        }
        std::ranges::sort(loaded, {}, [](const Asset& asset) {
            return std::tuple{ asset.pack, static_cast<std::uint8_t>(asset.layer), asset.name, asset.id };
        });
        return loaded;
    }

    void Catalog::Refresh()
    {
        bcn::runtime_assets::ClearGameRelativeSources("TintMask\\");
        std::vector<Asset> loaded;
        for (const auto& root : bcn::catalog_roots::Discover(RootPath())) {
            for (auto& asset : ScanDirectory(root)) {
                if (const auto found = std::ranges::find(loaded, asset.id, &Asset::id);
                    found != loaded.end()) *found = std::move(asset);
                else loaded.push_back(std::move(asset));
            }
        }
        std::ranges::sort(loaded, {}, [](const Asset& asset) {
            return std::tuple{ asset.pack, static_cast<std::uint8_t>(asset.layer), asset.name, asset.id };
        });
        std::scoped_lock lock(lock_);
        assets_ = std::move(loaded);
        SKSE::log::info("Body Change NG loaded {} player-only tint mask assets", assets_.size());
    }

    std::vector<Asset> Catalog::Snapshot() const
    {
        std::scoped_lock lock(lock_);
        return assets_;
    }

    std::optional<Asset> Catalog::Find(const std::string_view id) const
    {
        std::scoped_lock lock(lock_);
        const auto found = std::ranges::find(assets_, id, &Asset::id);
        return found != assets_.end() ? std::optional<Asset>{ *found } : std::nullopt;
    }

    std::string_view LayerName(const Layer layer)
    {
        switch (layer) {
        case Layer::frekles: return "Freckles";
        case Layer::lips: return "Lips";
        case Layer::cheeks: return "Cheeks";
        case Layer::eyeliner: return "Eyeliner";
        case Layer::upperEyeSocket: return "Upper eye socket";
        case Layer::lowerEyeSocket: return "Lower eye socket";
        case Layer::skinTone: return "Skin tone";
        case Layer::warPaint: return "War paint";
        case Layer::frownLines: return "Frown lines";
        case Layer::lowerCheeks: return "Lower cheeks";
        case Layer::nose: return "Nose";
        case Layer::chin: return "Chin";
        case Layer::neck: return "Neck";
        case Layer::forehead: return "Forehead";
        case Layer::dirt: return "Dirt";
        }
        return "Tint";
    }

    std::string TintFamilyLabel(const body_family::Mask families)
    {
        const auto ube = (families & body_family::Bit(body_family::Family::ube)) != 0U;
        const auto conventional = (families & (body_family::Bit(body_family::Family::femaleVanilla) |
            body_family::Bit(body_family::Family::cbbe) |
            body_family::Bit(body_family::Family::unp))) != 0U;
        if (ube && conventional) return "UBE / CBBE 3BA / BHUNP / UNP";
        if (ube) return "UBE";
        if (conventional) return "CBBE 3BA / BHUNP / UNP";
        return "Male";
    }

    std::optional<Asset> BestAssetForPlayer(const std::string_view pack, const Layer layer)
    {
        return ::BestAssetForPlayer(RE::PlayerCharacter::GetSingleton(), Catalog::Get().Snapshot(), pack, layer);
    }

    std::optional<Color> CurrentColor(const Layer layer)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto* mask = FindPlayerMask(player, layer);
        if (!mask) return std::nullopt;
        return Color{
            .red = static_cast<float>(mask->color.red) / 255.0F,
            .green = static_cast<float>(mask->color.green) / 255.0F,
            .blue = static_cast<float>(mask->color.blue) / 255.0F,
            .alpha = std::clamp(mask->alpha, 0.0F, 1.0F)
        };
    }

    std::optional<Color> OriginalColor(const Layer layer)
    {
        const auto settings = Settings::Get().Snapshot();
        const auto found = std::ranges::find(settings.playerTintBackups,
            static_cast<std::uint8_t>(layer), &PlayerTintBackup::type);
        if (found == settings.playerTintBackups.end()) return std::nullopt;
        return Color{
            .red = static_cast<float>(found->color[0]) / 255.0F,
            .green = static_cast<float>(found->color[1]) / 255.0F,
            .blue = static_cast<float>(found->color[2]) / 255.0F,
            .alpha = std::clamp(found->alpha, 0.0F, 1.0F)
        };
    }

    ApplyResult QueueApply(std::string assetID, const Color color)
    {
        const auto asset = Catalog::Get().Find(assetID);
        if (!asset) return ApplyResult::invalidAsset;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return ApplyResult::unavailable;
        if (!TintMatchesActor(asset->bodyFamilies, body_family::ResolveActor(player))) {
            return ApplyResult::incompatibleBodyFamily;
        }
        if (!FindPlayerMask(player, asset->layer)) return ApplyResult::unsupportedLayer;
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        const auto handle = player->GetHandle();
        const auto generation = g_tintGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        auto baseAssets = CurrentPack() == asset->pack ? std::vector<Asset>{} :
            BestPackAssetsForPlayer(player, Catalog::Get().Snapshot(), asset->pack);
        QueueTintTask(player->GetFormID(), [handle, asset = *asset, color, baseAssets = std::move(baseAssets), generation] {
            if (!baseAssets.empty()) ApplyPackNow(handle, baseAssets, generation);
            ApplyNow(handle, asset, color, generation);
        });
        return ApplyResult::queued;
    }

    ApplyResult QueueApplyPack(std::string pack)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return ApplyResult::unavailable;
        const auto catalog = Catalog::Get().Snapshot();
        auto selected = BestPackAssetsForPlayer(player, catalog, pack);
        for (std::uint8_t raw{}; raw <= static_cast<std::uint8_t>(Layer::dirt); ++raw) {
            const auto layer = static_cast<Layer>(raw);
            const auto* current = FindPlayerMask(player, layer);
            const auto currentName = current && current->texture ? Lower(Filename(current->texture->textureName.c_str())) : std::string{};
            std::size_t candidates{};
            for (const auto& asset : catalog) {
                if (asset.pack == pack && asset.layer == layer) ++candidates;
            }
            const auto best = ::BestAssetForPlayer(player, catalog, pack, layer);
            if (best) {
                SKSE::log::info(
                    "TintPackAudit pack='{}' layer={} active=true candidates={} selected='{}' current='{}'",
                    pack, static_cast<std::uint32_t>(layer), candidates, best->path, currentName);
            } else {
                SKSE::log::info(
                    "TintPackAudit pack='{}' layer={} active={} candidates={} selected='<none>' current='{}'",
                    pack, static_cast<std::uint32_t>(layer), current != nullptr, candidates, currentName);
            }
        }
        if (selected.empty()) {
            const auto packExists = std::ranges::any_of(catalog,
                [pack](const Asset& asset) { return asset.pack == pack; });
            return packExists ? ApplyResult::incompatibleBodyFamily : ApplyResult::invalidAsset;
        }
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        const auto handle = player->GetHandle();
        const auto generation = g_tintGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        QueueTintTask(player->GetFormID(), [handle, assets = std::move(selected), generation] {
            ApplyPackNow(handle, assets, generation);
        });
        return ApplyResult::queued;
    }

    std::optional<std::string> CurrentPack()
    {
        return CurrentState().pack;
    }

    ApplyResult QueueReapplyCurrent()
    {
        const auto state = CurrentState();
        if (!state.pack && state.layerOverrides.empty() && state.restoredLayers.empty()) {
            return ApplyResult::unavailable;
        }
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return ApplyResult::unavailable;
        const auto catalog = Catalog::Get().Snapshot();
        auto baseAssets = state.pack ? BestPackAssetsForPlayer(player, catalog, *state.pack) :
            std::vector<Asset>{};
        std::vector<std::pair<Asset, Color>> overrides;
        for (const auto& [layer, selection] : state.layerOverrides) {
            const auto found = std::ranges::find(catalog, selection.assetID, &Asset::id);
            if (found != catalog.end() && found->layer == layer) {
                overrides.emplace_back(*found, selection.color);
            }
        }
        std::vector<std::pair<Layer, PlayerTintBackup>> restores;
        const auto backups = Settings::Get().Snapshot().playerTintBackups;
        for (const auto layer : state.restoredLayers) {
            const auto found = std::ranges::find(backups, static_cast<std::uint8_t>(layer), &PlayerTintBackup::type);
            if (found != backups.end()) restores.emplace_back(layer, *found);
        }
        if (baseAssets.empty() && overrides.empty() && restores.empty()) return ApplyResult::invalidAsset;
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        const auto handle = player->GetHandle();
        const auto generation = g_tintGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        SKSE::log::info(
            "Body Change NG is reapplying tint state after RaceMenu: pack='{}' detail-overrides={} restored-layers={}",
            state.pack.value_or(std::string{}), overrides.size(), restores.size());
        QueueTintTask(player->GetFormID(), [handle, baseAssets = std::move(baseAssets), overrides = std::move(overrides),
                           restores = std::move(restores), generation] {
            if (!baseAssets.empty()) ApplyPackNow(handle, baseAssets, generation);
            for (const auto& [asset, color] : overrides) ApplyNow(handle, asset, color, generation);
            for (const auto& [layer, backup] : restores) RestoreNow(handle, layer, backup, generation);
        });
        return ApplyResult::queued;
    }

    ApplyResult QueueRestore(const Layer layer, std::string basePack)
    {
        const auto settings = Settings::Get().Snapshot();
        const auto found = std::ranges::find(settings.playerTintBackups, static_cast<std::uint8_t>(layer), &PlayerTintBackup::type);
        if (found == settings.playerTintBackups.end()) return ApplyResult::noOriginalBackup;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return ApplyResult::unavailable;
        if (!FindPlayerMask(player, layer)) return ApplyResult::unsupportedLayer;
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        const auto handle = player->GetHandle();
        const auto generation = g_tintGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        auto baseAssets = basePack.empty() || CurrentPack() == basePack ? std::vector<Asset>{} :
            BestPackAssetsForPlayer(player, Catalog::Get().Snapshot(), basePack);
        QueueTintTask(player->GetFormID(), [handle, layer, backup = *found, baseAssets = std::move(baseAssets), generation] {
            if (!baseAssets.empty()) ApplyPackNow(handle, baseAssets, generation);
            RestoreNow(handle, layer, backup, generation);
        });
        return ApplyResult::queued;
    }

    ApplyResult QueueRestoreAll()
    {
        // Invalidate a previously queued pack even when it has not yet run and
        // therefore has not captured its original layers. This makes rapid
        // pack -> default input deterministic instead of letting the older
        // task land after the default request.
        const auto generation = g_tintGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return ApplyResult::unavailable;
        const auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !bcn::frame_tasks::Active()) return ApplyResult::noTaskInterface;
        const auto handle = player->GetHandle();
        QueueTintTask(player->GetFormID(), [handle, generation] {
            // Read backups on the game task, not on the UI submission frame.
            // If an older apply was already running, its original capture is
            // visible here and this newest default request can still undo it.
            const auto backups = bcn::Settings::Get().Snapshot().playerTintBackups;
            if (backups.empty()) {
                if (IsCurrentTintChange(generation)) SetCurrentPack(std::nullopt);
                SKSE::log::info("Body Change NG default tint request required no layer restoration");
                return;
            }
            RestoreAllNow(handle, backups, generation);
        });
        return ApplyResult::queued;
    }

    PersistedState SnapshotPersistedState()
    {
        const auto current = CurrentState();
        PersistedState state{ .pack = current.pack };
        state.layers.reserve(current.layerOverrides.size() + current.restoredLayers.size());
        for (const auto& [layer, selection] : current.layerOverrides) {
            state.layers.push_back(PersistedLayerState{
                .layer = layer,
                .assetID = selection.assetID,
                .color = selection.color
            });
        }
        for (const auto layer : current.restoredLayers) {
            state.layers.push_back(PersistedLayerState{ .layer = layer, .restored = true });
        }
        return state;
    }

    void RestorePersistedState(PersistedState state)
    {
        CurrentTintState restored;
        if (state.pack && state.pack->size() <= 1024U) restored.pack = std::move(state.pack);
        for (auto& layer : state.layers) {
            if (static_cast<std::uint8_t>(layer.layer) > static_cast<std::uint8_t>(Layer::dirt)) continue;
            if (layer.restored) {
                restored.restoredLayers.insert(layer.layer);
                continue;
            }
            if (layer.assetID.empty() || layer.assetID.size() > 1024U) continue;
            restored.layerOverrides[layer.layer] = LayerOverride{
                .assetID = std::move(layer.assetID),
                .color = layer.color
            };
        }
        std::scoped_lock lock(g_currentTintStateLock);
        g_currentTintState = std::move(restored);
    }

    void ResetPersistedState()
    {
        g_tintGeneration.fetch_add(1U, std::memory_order_acq_rel);
        std::scoped_lock lock(g_currentTintStateLock);
        g_currentTintState = {};
    }
}
