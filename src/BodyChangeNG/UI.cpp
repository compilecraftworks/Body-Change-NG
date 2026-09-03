#include "BodyChangeNG/UI.h"

#include "BodyChangeNG/ActorCatalog.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/InputSink.h"
#include "BodyChangeNG/MenuCharacterPresentation.h"
#include "BodyChangeNG/NativeImGuiHost.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinOverrides.h"
#include "BodyChangeNG/SkinProfiles.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cfloat>
#include <format>
#include <optional>
#include <system_error>
#include <unordered_set>

namespace
{
    enum class ActiveTab
    {
        body,
        skin,
        tint
    };

    enum class DistributionPool
    {
        body,
        skin
    };

    struct CatalogItem
    {
        std::string id;
        std::string name;
        std::string family;
        std::string source;
        bool favorite{};
        bool current{};
        bool compatible{ true };
        bool body{};
    };

    struct PendingChoice
    {
        RE::FormID actorFormID{};
        std::string id;
        std::string originalId;
        bool useDefault{};
    };

    struct CatalogNavigationState
    {
        std::size_t index{};
        bool initialized{};
        bool scrollRequested{};
    };

    struct CatalogNavigationCommand
    {
        std::size_t focused{};
        bool hasFocus{};
        bool preview{};
        bool confirm{};
    };

    ActiveTab g_activeTab{ ActiveTab::body };
    DistributionPool g_distributionPool{ DistributionPool::body };
    std::array<bool, 3> g_favoritesOnlyByTab{};
    bool g_showDistribution{};
    bool g_showOutfit{};
    bool g_orefitRulesRegistered{};
    bool g_showSettings{};
    bool g_showTintDetails{};
    bool g_initializeActorSelection{ true };
    bool g_distributionEditorLoaded{};
    std::vector<bcn::DistributionRule> g_distributionRules;
    std::size_t g_selectedDistributionRule{};
    std::uint32_t g_nextDraftRuleID{ 1U };
    std::string g_distributionBodySearch;
    std::string g_distributionSkinSearch;
    std::vector<std::string> g_distributionFactionOptions;
    std::vector<std::string> g_distributionPluginOptions;
    std::vector<std::string> g_distributionRaceOptions;
    std::uint32_t g_selectedActorFormID{};
    std::string g_actorSearch;
    std::string g_search;
    bcn::player_tint::Layer g_selectedTintLayer{ bcn::player_tint::Layer::lips };
    std::string g_selectedTintPack;
    std::string g_currentTintPack;
    std::string g_selectedTintAssetID;
    std::array<float, 4> g_tintColor{ 1.0F, 1.0F, 1.0F, 1.0F };
    std::chrono::steady_clock::time_point g_lastTintDetailApply{};
    std::optional<PendingChoice> g_pendingBody;
    std::optional<PendingChoice> g_pendingSkin;
    std::optional<PendingChoice> g_pendingTint;
    std::array<CatalogNavigationState, 3> g_catalogNavigation{};
    std::string g_notification;
    std::chrono::steady_clock::time_point g_notificationUntil{};
    std::mutex g_notificationLock;
    // Preserve the requested picker proportions. Resolution and text scaling
    // multiply both axes equally, including the 4K auto scale.
    constexpr auto kDefaultWindowWidth = 700.0F;
    constexpr auto kDefaultWindowHeight = 875.0F;
    constexpr auto kWindowPositionTolerance = 0.5F;

    [[nodiscard]] bcn::UiLanguage WindowsLanguage()
    {
        const auto language = PRIMARYLANGID(GetUserDefaultUILanguage());
        if (language == LANG_KOREAN) return bcn::UiLanguage::korean;
        if (language == LANG_CHINESE) return bcn::UiLanguage::chineseSimplified;
        return bcn::UiLanguage::english;
    }

    [[nodiscard]] bcn::UiLanguage CurrentLanguage()
    {
        const auto configured = bcn::Settings::Get().Snapshot().language;
        return configured == bcn::UiLanguage::automatic ? WindowsLanguage() : configured;
    }

    [[nodiscard]] const char* Text(const char* korean, const char* english, const char* chinese)
    {
        switch (CurrentLanguage()) {
        case bcn::UiLanguage::korean: return korean;
        case bcn::UiLanguage::chineseSimplified: return chinese;
        default: return english;
        }
    }

    [[nodiscard]] float LayoutScale()
    {
        const auto configured = bcn::Settings::Get().Snapshot().textScale;
        return bcn::native_ui::GetResolutionScale() * std::clamp(configured, 0.75F, 1.50F);
    }

    [[nodiscard]] float Scaled(const float value)
    {
        return value * LayoutScale();
    }

    void PrepareResizableDropdown(const std::size_t itemCount)
    {
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        const auto padding = ImGui::GetStyle().WindowPadding.y * 2.0F;
        const auto minimumRows = (std::max)(std::size_t{ 1 }, (std::min)(itemCount, std::size_t{ 4 }));
        const auto initialRows = (std::max)(minimumRows, (std::min)(itemCount, std::size_t{ 14 }));
        const auto minimumHeight = padding + rowHeight * static_cast<float>(minimumRows);
        auto maximumHeight = Scaled(640.0F);
        if (const auto* viewport = ImGui::GetMainViewport()) {
            maximumHeight = (std::min)(maximumHeight, viewport->WorkSize.y * 0.72F);
        }
        maximumHeight = (std::max)(minimumHeight, maximumHeight);
        const auto initialHeight = (std::min)(maximumHeight,
            padding + rowHeight * static_cast<float>(initialRows));
        const auto popupWidth = ImGui::CalcItemWidth();
        // Fix the width to the owning field and let only the lower edge move.
        // This avoids reintroducing horizontal scrolling while allowing a
        // short or very long actor/race/faction/plugin list to be resized.
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(popupWidth, minimumHeight), ImVec2(popupWidth, maximumHeight));
        ImGui::SetNextWindowSize(ImVec2(popupWidth, initialHeight), ImGuiCond_Appearing);
    }

    [[nodiscard]] bool& FavoritesOnly()
    {
        return g_favoritesOnlyByTab[static_cast<std::size_t>(g_activeTab)];
    }

    [[nodiscard]] CatalogNavigationState& NavigationState()
    {
        return g_catalogNavigation[static_cast<std::size_t>(g_activeTab)];
    }

    void ResetCatalogNavigation()
    {
        for (auto& state : g_catalogNavigation) state = {};
    }

    [[nodiscard]] bool CatalogNavigationBlocked()
    {
        return ImGui::GetIO().WantTextInput || bcn::InputSink::Get().IsCapturingHotkey() ||
            g_showDistribution || g_showOutfit || g_showSettings || g_showTintDetails ||
            ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    }

    [[nodiscard]] bool NavigationKeyPressed(const ImGuiKey first, const ImGuiKey second,
        const ImGuiKey gamepad, const bool repeat)
    {
        return ImGui::IsKeyPressed(first, repeat) || ImGui::IsKeyPressed(second, repeat) ||
            ImGui::IsKeyPressed(gamepad, repeat);
    }

    [[nodiscard]] CatalogNavigationCommand HandleCatalogNavigation(
        const std::size_t rowCount, const std::size_t preferredIndex)
    {
        auto& state = NavigationState();
        if (rowCount == 0U) {
            state = {};
            return {};
        }
        if (!state.initialized) {
            state.index = (std::min)(preferredIndex, rowCount - 1U);
            state.initialized = true;
        } else if (state.index >= rowCount) {
            state.index = rowCount - 1U;
        }

        CatalogNavigationCommand command{ .focused = state.index, .hasFocus = true };
        if (CatalogNavigationBlocked()) return command;

        const auto up = NavigationKeyPressed(ImGuiKey_UpArrow, ImGuiKey_W, ImGuiKey_GamepadDpadUp, true);
        const auto down = NavigationKeyPressed(ImGuiKey_DownArrow, ImGuiKey_S, ImGuiKey_GamepadDpadDown, true);
        if (up != down) {
            const auto previous = state.index;
            if (up && state.index != 0U) --state.index;
            if (down && state.index + 1U < rowCount) ++state.index;
            if (state.index != previous) {
                command.preview = true;
                state.scrollRequested = true;
            }
        }
        command.focused = state.index;
        command.confirm = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_Space, false) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false);
        return command;
    }

    void FocusCatalogRow(const std::size_t row)
    {
        auto& state = NavigationState();
        state.index = row;
        state.initialized = true;
    }

    void ScrollFocusedCatalogRow(const std::size_t row)
    {
        auto& state = NavigationState();
        if (state.scrollRequested && state.index == row) {
            ImGui::SetScrollHereY(0.5F);
            state.scrollRequested = false;
        }
    }

    [[nodiscard]] std::string Lower(std::string_view value)
    {
        std::string result{ value };
        std::ranges::transform(result, result.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return result;
    }

    [[nodiscard]] std::string ActorLabel(const bcn::ActorEntry& entry)
    {
        if (entry.player) return Text("플레이어", "Player", "玩家");
        return std::format("{}, {} ({:08X})", entry.name,
            entry.female ? Text("여성", "Female", "女性") : Text("남성", "Male", "男性"), entry.formID);
    }

    [[nodiscard]] std::string ActorLabel(RE::Actor* actor)
    {
        if (!actor) return Text("선택된 액터 없음", "No selected actor", "未选择角色");
        if (actor == RE::PlayerCharacter::GetSingleton()) return Text("플레이어", "Player", "玩家");
        const auto* base = actor->GetActorBase();
        const auto* displayName = actor->GetDisplayFullName();
        const auto* baseName = base ? base->GetName() : nullptr;
        const bcn::ActorEntry entry{
            .formID = actor->GetFormID(),
            .name = displayName && displayName[0] != '\0' ? displayName :
                baseName && baseName[0] != '\0' ? baseName : "NPC",
            .female = base && base->GetSex() == RE::SEX::kFemale
        };
        return ActorLabel(entry);
    }

    [[nodiscard]] bool ActorMatchesSearch(const bcn::ActorEntry& entry)
    {
        if (g_actorSearch.empty()) return true;
        const auto needle = Lower(g_actorSearch);
        auto id = std::format("{:08x}", entry.formID);
        auto shortID = id;
        while (shortID.size() > 1U && shortID.front() == '0') shortID.erase(shortID.begin());
        const auto searchableName = entry.player ?
            std::string{ Text("플레이어", "Player", "玩家") } : entry.name;
        return Lower(searchableName).find(needle) != std::string::npos || id.find(needle) != std::string::npos ||
            shortID.find(needle) != std::string::npos || (needle.starts_with("0x") && id.find(needle.substr(2)) != std::string::npos);
    }

    [[nodiscard]] std::optional<RE::FormID> ExactActorFormID(std::string_view text)
    {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
        if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
        if (text.empty() || text.size() > 8U) return std::nullopt;
        RE::FormID value{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
        if (error != std::errc{} || end != text.data() + text.size() || value == 0) return std::nullopt;
        return value;
    }

    [[nodiscard]] bool TabButton(const char* label, const bool active)
    {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20F, 0.48F, 0.62F, 1.0F));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25F, 0.56F, 0.70F, 1.0F));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.29F, 0.62F, 0.76F, 1.0F));
        }
        const auto clicked = ImGui::Button(label);
        if (active) ImGui::PopStyleColor(3);
        return clicked;
    }

    [[nodiscard]] bool FavoriteButton(const bool favorite, const float height)
    {
        const auto cursor = ImGui::GetCursorScreenPos();
        const auto width = Scaled(42.0F);
        ImGui::InvisibleButton("favorite", ImVec2(width, height));
        const auto hovered = ImGui::IsItemHovered();
        const auto glyph = favorite ? "★" : "☆";
        const auto fontSize = ImGui::GetFontSize() * 1.25F;
        auto* font = ImGui::GetFont();
        const auto textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0F, glyph);
        auto textY = cursor.y + (height - textSize.y) * 0.5F;
        if (auto* baked = font->GetFontBaked(fontSize)) {
            const auto codepoint = static_cast<ImWchar>(favorite ? 0x2605 : 0x2606);
            if (const auto* visibleGlyph = baked->FindGlyphNoFallback(codepoint)) {
                const auto scale = fontSize / baked->Size;
                const auto visibleHeight = (visibleGlyph->Y1 - visibleGlyph->Y0) * scale;
                textY = cursor.y + (height - visibleHeight) * 0.5F - visibleGlyph->Y0 * scale;
            }
        }
        if (hovered) {
            ImGui::GetWindowDrawList()->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                IM_COL32(55, 78, 92, 255), Scaled(4.0F));
        }
        ImGui::GetWindowDrawList()->AddText(font, fontSize,
            ImVec2(cursor.x + (width - textSize.x) * 0.5F, textY),
            favorite ? IM_COL32(255, 190, 72, 255) : IM_COL32(205, 218, 226, 255), glyph);
        return ImGui::IsItemClicked();
    }

    [[nodiscard]] bool EscapePressed()
    {
        static int cachedFrame = -1;
        static bool available{};
        static bool consumed{};
        const auto frame = ImGui::GetFrameCount();
        if (cachedFrame != frame) {
            cachedFrame = frame;
            // Always evaluate both routes.  Consuming only one side of a
            // duplicated DirectInput/Scaleform event could otherwise close a
            // popup and then the main window on consecutive frames.
            const auto directInput = bcn::native_ui::ConsumeEscape();
            const auto scaleform = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
            const auto gamepad = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false);
            // Consume Escape while typing, but never turn that same key-up into
            // a delayed window close after the text field releases focus.
            available = !ImGui::GetIO().WantTextInput &&
                !bcn::InputSink::Get().IsCapturingHotkey() && (directInput || scaleform || gamepad);
            consumed = false;
        }
        if (!available || consumed) return false;
        consumed = true;
        return true;
    }

    [[nodiscard]] ImVec2 DefaultWindowSize(const float width, const float height)
    {
        const auto scale = LayoutScale();
        ImVec2 size{ width * scale, height * scale };
        if (const auto* viewport = ImGui::GetMainViewport()) {
            size.x = std::min(size.x, viewport->WorkSize.x * 0.90F);
            size.y = std::min(size.y, viewport->WorkSize.y * 0.90F);
        }
        return size;
    }

    [[nodiscard]] float CatalogListHeight()
    {
        // Keep the three catalog rectangles identical. Tint uses the reserved
        // row for its value controls; Body and Skin intentionally retain the
        // same lower edge so switching tabs never changes the list geometry.
        const auto footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        return (std::max)(Scaled(120.0F), ImGui::GetContentRegionAvail().y - footer);
    }

    // Keep popup input modal, but do not wash the running game or the main
    // picker with ImGui's modal dim overlay. The popup itself remains opaque
    // and still blocks accidental clicks behind it.
    [[nodiscard]] bool BeginUndimmedPopupModal(const char* title, bool* open, const ImGuiWindowFlags flags)
    {
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
        const auto began = ImGui::BeginPopupModal(title, open, flags);
        ImGui::PopStyleColor();
        return began;
    }

    void TextDisabledWrapped(const char* text)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }

    [[nodiscard]] const char* TintResultText(const bcn::player_tint::ApplyResult result)
    {
        using Result = bcn::player_tint::ApplyResult;
        switch (result) {
        case Result::queued: return Text("플레이어 틴트를 즉시 반영했습니다.", "Applied the player tint immediately.", "已立即应用玩家色调。");
        case Result::invalidAsset: return Text("선택한 틴트 파일을 찾지 못했습니다.", "The selected tint file was not found.", "找不到所选色调文件。");
        case Result::incompatibleBodyFamily: return Text("선택한 틴트팩은 플레이어의 바디·헤드 계열과 맞지 않습니다.", "The selected tint pack does not match the player's body/head family.", "所选色调包与玩家的身体/头部系列不匹配。");
        case Result::unsupportedLayer: return Text("현재 플레이어에게 이 틴트 레이어가 없습니다.", "This tint layer is unavailable on the current player.", "当前玩家没有此色调图层。");
        case Result::noOriginalBackup: return Text("복원할 원본 틴트 백업이 없습니다.", "There is no original tint backup to restore.", "没有可还原的原始色调备份。");
        default: return Text("RaceMenu 또는 작업 인터페이스를 사용할 수 없습니다.", "RaceMenu or the task interface is unavailable.", "RaceMenu 或任务接口不可用。");
        }
    }

    [[nodiscard]] const char* TintLayerText(const bcn::player_tint::Layer layer)
    {
        using Layer = bcn::player_tint::Layer;
        switch (layer) {
        case Layer::frekles: return Text("주근깨", "Freckles", "雀斑");
        case Layer::lips: return Text("입술", "Lips", "嘴唇");
        case Layer::cheeks: return Text("볼", "Cheeks", "脸颊");
        case Layer::eyeliner: return Text("아이라이너", "Eyeliner", "眼线");
        case Layer::upperEyeSocket: return Text("윗눈가", "Upper eye socket", "上眼窝");
        case Layer::lowerEyeSocket: return Text("아랫눈가", "Lower eye socket", "下眼窝");
        case Layer::skinTone: return Text("피부 톤", "Skin tone", "肤色");
        case Layer::warPaint: return Text("전쟁 페인트", "War paint", "战纹");
        case Layer::frownLines: return Text("주름", "Frown lines", "皱纹");
        case Layer::lowerCheeks: return Text("아래 볼", "Lower cheeks", "下脸颊");
        case Layer::nose: return Text("코", "Nose", "鼻部");
        case Layer::chin: return Text("턱", "Chin", "下巴");
        case Layer::neck: return Text("목", "Neck", "颈部");
        case Layer::forehead: return Text("이마", "Forehead", "额头");
        case Layer::dirt: return Text("얼굴 먼지", "Dirt", "污渍");
        }
        return Text("틴트", "Tint", "色调");
    }

    [[nodiscard]] bool MatchSearch(const CatalogItem& item)
    {
        if (g_search.empty()) return true;
        const auto contains = [](std::string_view value, const std::string& needle) {
            return value.find(needle) != std::string::npos;
        };
        return contains(item.name, g_search) || contains(item.family, g_search) || contains(item.source, g_search);
    }

    void ToggleFavorite(CatalogItem& item)
    {
        if (!item.body) {
            item.favorite = !item.favorite;
            return;
        }
        auto settings = bcn::Settings::Get().Snapshot();
        const auto found = std::ranges::find(settings.favoriteBodyPresets, item.id);
        if (found == settings.favoriteBodyPresets.end()) {
            settings.favoriteBodyPresets.push_back(item.id);
            item.favorite = true;
        } else {
            settings.favoriteBodyPresets.erase(found);
            item.favorite = false;
        }
        bcn::Settings::Get().Update(settings);
        if (!bcn::Settings::Get().Save()) {
            bcn::ui::Notify(Text("즐겨찾기를 저장하지 못했습니다.", "Could not save favorites.", "无法保存收藏。"));
        }
    }

    void ToggleSkinFavorite(const std::string& id)
    {
        auto settings = bcn::Settings::Get().Snapshot();
        const auto found = std::ranges::find(settings.favoriteSkinProfiles, id);
        if (found == settings.favoriteSkinProfiles.end()) settings.favoriteSkinProfiles.push_back(id);
        else settings.favoriteSkinProfiles.erase(found);
        bcn::Settings::Get().Update(settings);
        if (!bcn::Settings::Get().Save()) {
            bcn::ui::Notify(Text("즐겨찾기를 저장하지 못했습니다.", "Could not save favorites.", "无法保存收藏。"));
        }
    }

    void ToggleTintFavorite(const std::string& pack)
    {
        auto settings = bcn::Settings::Get().Snapshot();
        const auto found = std::ranges::find(settings.favoriteTintPacks, pack);
        if (found == settings.favoriteTintPacks.end()) settings.favoriteTintPacks.push_back(pack);
        else settings.favoriteTintPacks.erase(found);
        bcn::Settings::Get().Update(settings);
        if (!bcn::Settings::Get().Save()) {
            bcn::ui::Notify(Text("즐겨찾기를 저장하지 못했습니다.", "Could not save favorites.", "无法保存收藏。"));
        }
    }

    [[nodiscard]] std::vector<CatalogItem> BodyItems()
    {
        const auto presets = bcn::PresetCatalog::Get().Snapshot();
        const auto actor = bcn::ActorCatalog::Get().Resolve(g_selectedActorFormID);
        const auto actorBase = actor ? actor->GetActorBase() : nullptr;
        const auto selectedMale = actorBase && actorBase->GetSex() == RE::SEX::kMale;
        const auto actorFamily = bcn::body_family::ResolveActor(actor);
        const auto settings = bcn::Settings::Get().Snapshot();
        const auto currentPreset = bcn::racemenu::CurrentPresetId(actor);
        std::vector<CatalogItem> items;
        items.reserve(presets.size());
        for (const auto& preset : presets) {
            if (preset.male != selectedMale) continue;
            if (!bcn::body_family::Matches(
                    bcn::body_family::PresetMask(preset.family, preset.male), actorFamily)) continue;
            const auto id = preset.PersistentId();
            items.push_back(CatalogItem{
                .id = id,
                .name = preset.name,
                .family = preset.family,
                .source = preset.source,
                .favorite = std::ranges::find(settings.favoriteBodyPresets, id) !=
                    settings.favoriteBodyPresets.end(),
                .current = currentPreset && *currentPreset == id,
                .body = true
            });
        }
        return items;
    }

    [[nodiscard]] RE::Actor* SelectedActor() noexcept
    {
        return bcn::ActorCatalog::Get().Resolve(g_selectedActorFormID);
    }

    void CommitPendingSelections();

    void SelectActor(const RE::FormID formID)
    {
        if (formID == 0) return;
        if (g_selectedActorFormID != formID) {
            CommitPendingSelections();
            // A transient runtime NPC can disappear before its pending choice
            // is committed. Never let that stale FormID selection leak into a
            // later actor that reuses the same dynamic ID.
            g_pendingBody.reset();
            g_pendingSkin.reset();
            g_pendingTint.reset();
            bcn::menu_character::Presentation::Get().Restore();
            g_selectedActorFormID = formID;
            ResetCatalogNavigation();
        }
        g_actorSearch.clear();
        const auto settings = bcn::Settings::Get().Snapshot();
        bcn::menu_character::Presentation::Get().Apply(settings.characterPosition, SelectedActor());
    }

    void ResetDistributionEditor()
    {
        g_distributionEditorLoaded = false;
        g_distributionRules.clear();
        g_selectedDistributionRule = 0;
        g_distributionPool = DistributionPool::body;
        g_distributionBodySearch.clear();
        g_distributionSkinSearch.clear();
        g_distributionFactionOptions.clear();
        g_distributionPluginOptions.clear();
        g_distributionRaceOptions.clear();
    }

    void AddUniqueTargetOption(std::vector<std::string>& options,
        std::unordered_set<std::string>& known, const std::string_view value)
    {
        if (value.empty()) return;
        auto key = Lower(value);
        if (known.insert(key).second) options.emplace_back(value);
    }

    void RefreshDistributionTargetOptions()
    {
        g_distributionFactionOptions.clear();
        g_distributionPluginOptions.clear();
        g_distributionRaceOptions.clear();
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return;

        std::unordered_set<std::string> knownFactions;
        for (const auto* faction : dataHandler->GetFormArray<RE::TESFaction>()) {
            AddUniqueTargetOption(g_distributionFactionOptions, knownFactions,
                faction ? std::string_view{ faction->GetFormEditorID() } : std::string_view{});
        }
        std::unordered_set<std::string> knownRaces;
        for (const auto* race : dataHandler->GetFormArray<RE::TESRace>()) {
            AddUniqueTargetOption(g_distributionRaceOptions, knownRaces,
                race ? std::string_view{ race->GetFormEditorID() } : std::string_view{});
        }
        std::unordered_set<std::string> knownPlugins;
        const auto appendPlugins = [&](const RE::TESFile* const* files, const std::size_t count) {
            if (!files) return;
            for (std::size_t index{}; index < count; ++index) {
                AddUniqueTargetOption(g_distributionPluginOptions, knownPlugins,
                    files[index] ? files[index]->GetFilename() : std::string_view{});
            }
        };
        appendPlugins(dataHandler->GetLoadedMods(), dataHandler->GetLoadedModCount());
        appendPlugins(dataHandler->GetLoadedLightMods(), dataHandler->GetLoadedLightModCount());

        const auto sortOptions = [](auto& options) {
            std::ranges::sort(options, [](const auto& left, const auto& right) {
                return Lower(left) < Lower(right);
            });
        };
        sortOptions(g_distributionFactionOptions);
        sortOptions(g_distributionPluginOptions);
        sortOptions(g_distributionRaceOptions);
    }

    void EnsureDistributionEditor()
    {
        if (g_distributionEditorLoaded) return;
        g_distributionRules = bcn::Distribution::Get().Snapshot();
        g_selectedDistributionRule = 0;
        RefreshDistributionTargetOptions();
        g_distributionEditorLoaded = true;
    }

    [[nodiscard]] bcn::DistributionRule NewDistributionRule()
    {
        const auto actor = SelectedActor();
        const auto base = actor ? actor->GetActorBase() : nullptr;
        const auto female = !base || base->GetSex() == RE::SEX::kFemale;
        return {
            .id = "user-rule-" + std::to_string(g_nextDraftRuleID++),
            .name = female ? Text("새 여성 NPC 규칙", "New female NPC rule", "新的女性 NPC 规则") :
                Text("새 남성 NPC 규칙", "New male NPC rule", "新的男性 NPC 规则"),
            .female = female
        };
    }

    void FillRuleTargetFromSelectedActor(bcn::DistributionRule& rule)
    {
        const auto actor = SelectedActor();
        const auto base = actor ? actor->GetActorBase() : nullptr;
        if (!base) return;
        switch (rule.scope) {
        case bcn::DistributionScope::npcBaseForm: {
            [[maybe_unused]] const auto normalized = bcn::SetDistributionRuleNPC(rule, base);
            break;
        }
        case bcn::DistributionScope::npcName:
            rule.target = base->GetName();
            break;
        case bcn::DistributionScope::pluginFile:
            if (const auto* file = base->GetFile(0)) rule.target = file->GetFilename();
            break;
        case bcn::DistributionScope::raceEditorID:
            if (const auto* race = base->GetRace()) rule.target = race->GetFormEditorID();
            break;
        default:
            break;
        }
    }

    [[nodiscard]] const char* TargetLabel(const bcn::DistributionScope scope)
    {
        switch (scope) {
        case bcn::DistributionScope::npcName:
            return Text("NPC 이름", "NPC name", "NPC 名称");
        case bcn::DistributionScope::factionEditorID:
            return Text("팩션 EditorID", "Faction EditorID", "阵营 EditorID");
        case bcn::DistributionScope::pluginFile:
            return Text("플러그인 파일명", "Plugin file name", "插件文件名");
        case bcn::DistributionScope::raceEditorID:
            return Text("종족 EditorID", "Race EditorID", "种族 EditorID");
        default:
            return "";
        }
    }

    [[nodiscard]] const char* DistributionScopeLabel(const bcn::DistributionScope scope)
    {
        switch (scope) {
        case bcn::DistributionScope::allNPCs:
            return Text("전체 NPC", "All NPCs", "全部 NPC");
        case bcn::DistributionScope::modInstalledFollower:
            return Text("커스텀 팔로워", "Custom followers", "自定义随从");
        case bcn::DistributionScope::elderNPC:
            return Text("노인 NPC", "Elder NPCs", "老年 NPC");
        case bcn::DistributionScope::pluginFile:
            return Text("플러그인", "Plugin", "插件");
        case bcn::DistributionScope::raceEditorID:
            return Text("종족", "Race", "种族");
        case bcn::DistributionScope::factionEditorID:
            return Text("팩션", "Faction", "阵营");
        case bcn::DistributionScope::npcName:
            return Text("이름이 같은 NPC", "NPCs with the same name", "同名 NPC");
        case bcn::DistributionScope::npcBaseForm:
            return "FormID";
        }
        return "";
    }

    [[nodiscard]] bool DistributionScopeCombo(bcn::DistributionScope& scope)
    {
        constexpr std::array order{
            bcn::DistributionScope::allNPCs,
            bcn::DistributionScope::modInstalledFollower,
            bcn::DistributionScope::elderNPC,
            bcn::DistributionScope::pluginFile,
            bcn::DistributionScope::raceEditorID,
            bcn::DistributionScope::factionEditorID,
            bcn::DistributionScope::npcName,
            bcn::DistributionScope::npcBaseForm
        };
        auto changed = false;
        PrepareResizableDropdown(order.size());
        if (ImGui::BeginCombo("##ruleScope", DistributionScopeLabel(scope))) {
            for (const auto option : order) {
                const auto selected = option == scope;
                if (ImGui::Selectable(DistributionScopeLabel(option), selected)) {
                    scope = option;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    [[nodiscard]] bool DistributionTargetCombo(const char* id, std::string& target,
        const std::vector<std::string>& options)
    {
        const auto* preview = target.empty() ? Text("선택", "Select", "选择") : target.c_str();
        auto changed = false;
        const auto savedValueRow = !target.empty() && !std::ranges::any_of(options, [&target](const auto& option) {
            return Lower(option) == Lower(target);
        }) ? 1U : 0U;
        PrepareResizableDropdown(options.size() + savedValueRow);
        if (ImGui::BeginCombo(id, preview)) {
            const auto installed = std::ranges::any_of(options, [&target](const auto& option) {
                return Lower(option) == Lower(target);
            });
            if (!target.empty() && !installed) {
                const auto savedLabel = target + Text(" (저장값)", " (saved value)", "（保存值）");
                if (ImGui::Selectable(savedLabel.c_str(), true)) changed = true;
                ImGui::Separator();
            }
            for (const auto& option : options) {
                const auto selected = Lower(option) == Lower(target);
                if (ImGui::Selectable(option.c_str(), selected)) {
                    target = option;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    [[nodiscard]] bool SaveActiveDistributionRules()
    {
        bcn::Distribution::Get().SetRules(g_distributionRules);
        return bcn::Distribution::Get().Save();
    }

    [[nodiscard]] bool ContainsPreset(const bcn::DistributionRule& rule, const std::string_view presetId)
    {
        return std::ranges::find(rule.presetIds, presetId) != rule.presetIds.end();
    }

    void ToggleRulePreset(bcn::DistributionRule& rule, const std::string& presetId)
    {
        const auto found = std::ranges::find(rule.presetIds, presetId);
        if (found == rule.presetIds.end()) rule.presetIds.push_back(presetId);
        else rule.presetIds.erase(found);
    }

    [[nodiscard]] bool ContainsSkinProfile(const bcn::DistributionRule& rule, const std::string_view profileId)
    {
        return std::ranges::find(rule.skinProfileIds, profileId) != rule.skinProfileIds.end();
    }

    void ToggleRuleSkinProfile(bcn::DistributionRule& rule, const std::string& profileId)
    {
        const auto found = std::ranges::find(rule.skinProfileIds, profileId);
        if (found == rule.skinProfileIds.end()) rule.skinProfileIds.push_back(profileId);
        else rule.skinProfileIds.erase(found);
    }

    [[nodiscard]] const char* ApplyResultMessage(const bcn::racemenu::ApplyResult result)
    {
        switch (result) {
        case bcn::racemenu::ApplyResult::queued:
            return Text("월드에 즉시 반영했습니다.", "Applied immediately in the world.", "已立即应用到游戏世界中。");
        case bcn::racemenu::ApplyResult::unavailable:
            return Text("RaceMenu BodyMorph 인터페이스를 찾지 못했습니다.", "RaceMenu's BodyMorph interface is unavailable.", "RaceMenu 的 BodyMorph 接口不可用。");
        case bcn::racemenu::ApplyResult::missingPreset:
            return Text("새로고침 후 사라진 프리셋입니다.", "The preset disappeared after refresh.", "刷新后该预设已不存在。");
        case bcn::racemenu::ApplyResult::actor3DUnavailable:
            return Text("액터의 3D가 로드되지 않아 즉시 적용할 수 없습니다.", "The actor's 3D is not loaded, so it cannot be applied immediately.", "角色的 3D 尚未加载，无法立即应用。");
        case bcn::racemenu::ApplyResult::emptyPreset:
            return Text("이 프리셋에는 적용할 슬라이더가 없습니다.", "This preset has no applicable sliders.", "该预设没有可应用的滑块。");
        case bcn::racemenu::ApplyResult::noTaskInterface:
            return Text("SKSE 게임 작업 인터페이스를 사용할 수 없습니다.", "The SKSE game-task interface is unavailable.", "SKSE 游戏任务接口不可用。");
        default:
            return Text("적용할 액터가 없습니다.", "No actor is available to apply this preset.", "没有可应用预设的角色。");
        }
    }

    [[nodiscard]] const char* SkinApplyResultMessage(const bcn::skin_override::ApplyResult result)
    {
        switch (result) {
        case bcn::skin_override::ApplyResult::queued:
            return Text("스킨을 즉시 반영했습니다.", "Applied the skin immediately.", "已立即应用皮肤。");
        case bcn::skin_override::ApplyResult::missingProfile:
            return Text("새로고침 후 사라진 스킨팩입니다.", "The skin pack disappeared after refresh.", "刷新后该皮肤包已不存在。");
        case bcn::skin_override::ApplyResult::actor3DUnavailable:
            return Text("액터의 3D가 로드되지 않아 스킨을 즉시 적용할 수 없습니다.", "The actor's 3D is not loaded, so the skin cannot be applied immediately.", "角色的 3D 尚未加载，无法立即应用皮肤。");
        case bcn::skin_override::ApplyResult::incompatibleSex:
            return Text("선택한 스킨팩은 이 액터의 성별과 맞지 않습니다.", "The selected skin pack does not match this actor's sex.", "所选皮肤包与该角色的性别不匹配。");
        case bcn::skin_override::ApplyResult::incompatibleBodyFamily:
            return Text("선택한 스킨팩은 이 액터의 바디 계열과 맞지 않습니다.", "The selected skin pack does not match this actor's body family.", "所选皮肤包与该角色的身体系列不匹配。");
        case bcn::skin_override::ApplyResult::faceGeometryUnavailable:
            return Text("현재 얼굴 지오메트리를 찾지 못해 목선을 방지하려고 스킨 전체 적용을 중단했습니다.", "The live face geometry was not found, so the whole skin application was stopped to prevent a neck seam.", "未找到当前脸部几何体；为避免颈部接缝，已停止应用整套皮肤。");
        case bcn::skin_override::ApplyResult::noTaskInterface:
            return Text("SKSE 게임 작업 인터페이스를 사용할 수 없습니다.", "The SKSE game-task interface is unavailable.", "SKSE 游戏任务接口不可用。");
        case bcn::skin_override::ApplyResult::unavailable:
            return Text("RaceMenu NiOverride 인터페이스를 사용할 수 없습니다.", "RaceMenu's NiOverride interface is unavailable.", "RaceMenu 的 NiOverride 接口不可用。");
        default:
            return Text("적용할 액터가 없습니다.", "No actor is available.", "没有可应用的角色。");
        }
    }

    [[nodiscard]] bool QueuePreset(const CatalogItem& item, const bcn::racemenu::ApplyMode mode)
    {
        auto* actor = SelectedActor();
        const auto result = bcn::racemenu::QueueApply(actor, item.id, mode);
        if (result == bcn::racemenu::ApplyResult::queued && mode == bcn::racemenu::ApplyMode::commit) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (actor && actor != player) {
                bcn::Distribution::Get().SetManualAssignment(actor, item.id);
            }
            bcn::OutfitRefit::Get().ProcessActor(actor);
        }
        if (result != bcn::racemenu::ApplyResult::queued) {
            bcn::ui::Notify(std::string(item.name) + " · " + ApplyResultMessage(result));
        }
        return result == bcn::racemenu::ApplyResult::queued;
    }

    void SaveManualSkinIfNeeded(RE::Actor* actor, const std::string& profileId)
    {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || actor == player) return;
        bcn::Distribution::Get().SetManualSkinAssignment(actor, profileId);
    }

    void SaveManualDefaultBodyIfNeeded(RE::Actor* actor)
    {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || actor == player) return;
        bcn::Distribution::Get().SetManualDefaultBody(actor);
    }

    void SaveManualDefaultSkinIfNeeded(RE::Actor* actor)
    {
        const auto* player = RE::PlayerCharacter::GetSingleton();
        if (!actor || actor == player) return;
        bcn::Distribution::Get().SetManualDefaultSkin(actor);
    }

    [[nodiscard]] bool QueueDefaultBody(const bool persistSelection)
    {
        auto* actor = SelectedActor();
        if (!actor) {
            bcn::ui::Notify(Text("기본 바디를 적용할 액터가 없습니다.", "No actor is available for the default body.", "没有可恢复默认身体的角色。"));
            return false;
        }
        if (!bcn::racemenu::IsReady()) {
            bcn::ui::Notify(Text("RaceMenu BodyMorph 인터페이스를 찾지 못했습니다.", "RaceMenu's BodyMorph interface is unavailable.", "RaceMenu 的 BodyMorph 接口不可用。"));
            return false;
        }
        if (!actor->Is3DLoaded()) {
            bcn::ui::Notify(Text("액터의 3D가 로드되지 않아 기본 바디를 즉시 복원할 수 없습니다.", "The actor's 3D is not loaded, so the default body cannot be restored immediately.", "角色的 3D 尚未加载，无法立即恢复默认身体。"));
            return false;
        }
        if (!SKSE::GetTaskInterface()) {
            bcn::ui::Notify(Text("SKSE 게임 작업 인터페이스를 사용할 수 없습니다.", "The SKSE game-task interface is unavailable.", "SKSE 游戏任务接口不可用。"));
            return false;
        }
        bcn::racemenu::QueueClearBodyChangeMorphs(actor);
        if (persistSelection) SaveManualDefaultBodyIfNeeded(actor);
        return true;
    }

    [[nodiscard]] bool QueueDefaultSkin(const bool persistSelection)
    {
        auto* actor = SelectedActor();
        const auto result = bcn::skin_override::QueueClear(actor);
        if (result == bcn::skin_override::ApplyResult::queued && persistSelection) {
            SaveManualDefaultSkinIfNeeded(actor);
        } else {
            if (result != bcn::skin_override::ApplyResult::queued) bcn::ui::Notify(SkinApplyResultMessage(result));
        }
        return result == bcn::skin_override::ApplyResult::queued;
    }

    void RememberPending(std::optional<PendingChoice>& pending, RE::Actor* actor,
        std::string id, const bool useDefault, std::string originalId)
    {
        if (!actor) return;
        if (!pending || pending->actorFormID != actor->GetFormID()) {
            pending = PendingChoice{
                .actorFormID = actor->GetFormID(),
                .id = std::move(id),
                .originalId = std::move(originalId),
                .useDefault = useDefault
            };
            return;
        }
        pending->id = std::move(id);
        pending->useDefault = useDefault;
    }

    void CommitPendingSelections()
    {
        auto* actor = SelectedActor();
        if (!actor) return;
        const auto actorFormID = actor->GetFormID();
        if (g_pendingBody && g_pendingBody->actorFormID == actorFormID) {
            if (g_pendingBody->useDefault) {
                [[maybe_unused]] const auto committed = QueueDefaultBody(true);
            } else {
                CatalogItem item{ .id = g_pendingBody->id, .name = g_pendingBody->id, .body = true };
                [[maybe_unused]] const auto committed = QueuePreset(item, bcn::racemenu::ApplyMode::commit);
            }
            g_pendingBody.reset();
        }
        if (g_pendingSkin && g_pendingSkin->actorFormID == actorFormID) {
            if (g_pendingSkin->useDefault) {
                [[maybe_unused]] const auto committed = QueueDefaultSkin(true);
            } else {
                const auto result = bcn::skin_override::QueueApply(actor, g_pendingSkin->id);
                if (result == bcn::skin_override::ApplyResult::queued) SaveManualSkinIfNeeded(actor, g_pendingSkin->id);
                else bcn::ui::Notify(SkinApplyResultMessage(result));
            }
            g_pendingSkin.reset();
        }
        if (g_pendingTint && g_pendingTint->actorFormID == actorFormID) {
            // Tint previews already replace the live player layers. Confirming
            // only changes the UI selection state; the current pack remains.
            g_pendingTint.reset();
        }
    }

    void HandleTabNavigation(const bool playerSelected)
    {
        if (CatalogNavigationBlocked()) return;
        const auto left = NavigationKeyPressed(
            ImGuiKey_LeftArrow, ImGuiKey_A, ImGuiKey_GamepadDpadLeft, false);
        const auto right = NavigationKeyPressed(
            ImGuiKey_RightArrow, ImGuiKey_D, ImGuiKey_GamepadDpadRight, false);
        if (left == right) return;

        const std::array playerTabs{ ActiveTab::body, ActiveTab::skin, ActiveTab::tint };
        const std::array npcTabs{ ActiveTab::body, ActiveTab::skin };
        const auto move = [&](const auto& tabs) {
            auto found = std::ranges::find(tabs, g_activeTab);
            auto index = found == tabs.end() ? std::size_t{} : static_cast<std::size_t>(found - tabs.begin());
            if (left) index = index == 0U ? tabs.size() - 1U : index - 1U;
            else index = (index + 1U) % tabs.size();
            g_activeTab = tabs[index];
        };
        if (playerSelected) move(playerTabs);
        else move(npcTabs);
    }

    void DrawCatalog(std::vector<CatalogItem>& items, const bool body)
    {
        auto* actor = SelectedActor();
        const auto backendCurrentBody = bcn::racemenu::CurrentPresetId(actor);
        const auto confirmedBodyId = g_pendingBody && actor && g_pendingBody->actorFormID == actor->GetFormID() ?
            g_pendingBody->originalId : backendCurrentBody.value_or(std::string{});

        std::vector<CatalogItem*> visibleItems;
        visibleItems.reserve(items.size());
        for (auto& item : items) {
            if ((FavoritesOnly() && !item.favorite) || !MatchSearch(item)) continue;
            visibleItems.push_back(&item);
        }
        std::size_t preferredIndex{};
        if (!confirmedBodyId.empty()) {
            const auto current = std::ranges::find(visibleItems, confirmedBodyId,
                [](const CatalogItem* item) -> const std::string& { return item->id; });
            if (current != visibleItems.end()) preferredIndex = 1U + static_cast<std::size_t>(current - visibleItems.begin());
        }
        const auto navigation = HandleCatalogNavigation(visibleItems.size() + (body ? 1U : 0U), preferredIndex);
        const auto previewRow = [&](const std::size_t row) {
            if (body && row == 0U) {
                if (QueueDefaultBody(false)) RememberPending(g_pendingBody, actor, {}, true, confirmedBodyId);
                return;
            }
            auto& item = *visibleItems[row - (body ? 1U : 0U)];
            if (item.compatible && body && QueuePreset(item, bcn::racemenu::ApplyMode::preview)) {
                RememberPending(g_pendingBody, actor, item.id, false, confirmedBodyId);
            } else if (!item.compatible) {
                bcn::ui::Notify(Text("현재 액터와 호환되지 않습니다.", "This item is incompatible with the selected actor.", "与所选角色不兼容。"));
            }
        };
        const auto confirmRow = [&](const std::size_t row) {
            if (body && row == 0U) {
                if (QueueDefaultBody(true)) g_pendingBody.reset();
                return;
            }
            auto& item = *visibleItems[row - (body ? 1U : 0U)];
            if (item.compatible) {
                if (body && QueuePreset(item, bcn::racemenu::ApplyMode::commit)) g_pendingBody.reset();
            } else {
                bcn::ui::Notify(Text("현재 액터와 호환되지 않습니다.", "This item is incompatible with the selected actor.", "与所选角色不兼容。"));
            }
        };
        if (navigation.preview) previewRow(navigation.focused);
        if (navigation.confirm) confirmRow(navigation.focused);

        if (ImGui::BeginChild("Catalog", ImVec2(0.0F, CatalogListHeight()), true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs)) {
            std::size_t row{};
            if (body) {
                ImGui::PushID("DefaultBody");
                const auto cursor = ImGui::GetCursorScreenPos();
                const auto width = ImGui::GetContentRegionAvail().x;
                const auto cardHeight = Scaled(48.0F);
                ImGui::InvisibleButton("item", ImVec2(width, cardHeight));
                const auto hovered = ImGui::IsItemHovered();
                const auto clicked = ImGui::IsItemClicked();
                const auto doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                const auto navigationFocused = navigation.hasFocus && navigation.focused == row;
                const auto draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + cardHeight),
                    confirmedBodyId.empty() ? IM_COL32(48, 103, 129, 255) :
                    hovered || navigationFocused ? IM_COL32(42, 63, 77, 255) : IM_COL32(35, 47, 57, 255), Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), IM_COL32(238, 244, 248, 255),
                    Text("기본 바디", "Default body", "默认身体"));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)), IM_COL32(160, 181, 193, 255),
                    Text("이 액터의 Body Change NG·기존 OBody 바디 모프 제거", "Remove Body Change NG and legacy OBody body morphs from this actor", "移除此角色的 Body Change NG 与旧版 OBody 身体形态"));
                if (doubleClicked) {
                    FocusCatalogRow(row);
                    confirmRow(row);
                } else if (clicked) {
                    FocusCatalogRow(row);
                    previewRow(row);
                }
                ScrollFocusedCatalogRow(row);
                ImGui::Dummy(ImVec2(0.0F, Scaled(5.0F)));
                ImGui::PopID();
                ++row;
                if (items.empty()) {
                    ImGui::TextWrapped("%s", Text(
                        "선택한 액터의 성별에 맞는 바디 프리셋을 찾지 못했습니다.",
                        "No body presets match the selected actor's sex.",
                        "未找到与所选角色性别匹配的身体预设。"));
                    ImGui::Spacing();
                }
            }
            for (auto* itemPointer : visibleItems) {
                auto& item = *itemPointer;
                ImGui::PushID(item.id.c_str());
                const auto cursor = ImGui::GetCursorScreenPos();
                const auto width = ImGui::GetContentRegionAvail().x;
                const auto cardHeight = Scaled(48.0F);
                const auto favoriteWidth = Scaled(46.0F);
                // The favorite star is a separate interactive control. Do not
                // let the card-wide apply button claim its mouse-down.
                const auto selectableWidth = (std::max)(0.0F, width - favoriteWidth);
                ImGui::InvisibleButton("item", ImVec2(selectableWidth, cardHeight));
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                const auto draw = ImGui::GetWindowDrawList();
                const auto confirmedCurrent = item.id == confirmedBodyId;
                const ImU32 fill = confirmedCurrent ? IM_COL32(48, 103, 129, 255) :
                    hovered || (navigation.hasFocus && navigation.focused == row) ?
                    IM_COL32(42, 63, 77, 255) : IM_COL32(35, 47, 57, 255);
                draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + cardHeight), fill, Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), IM_COL32(238, 244, 248, 255), item.name.c_str());
                const auto sub = item.family + (confirmedCurrent ? " · " + std::string(Text("현재 적용", "Current", "当前应用")) :
                    item.compatible ? "" : " · " + std::string(Text("호환되지 않음", "Not compatible", "不兼容")));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)),
                    item.compatible ? IM_COL32(160, 181, 193, 255) : IM_COL32(192, 145, 120, 255), sub.c_str());
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + width - favoriteWidth, cursor.y));
                if (FavoriteButton(item.favorite, cardHeight)) ToggleFavorite(item);
                if (doubleClicked) {
                    FocusCatalogRow(row);
                    confirmRow(row);
                } else if (clicked) {
                    FocusCatalogRow(row);
                    previewRow(row);
                }
                ScrollFocusedCatalogRow(row);
                ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + cardHeight + Scaled(5.0F)));
                ImGui::Dummy(ImVec2(0.0F, 0.0F));
                ImGui::PopID();
                ++row;
            }
        }
        ImGui::EndChild();
    }

    void DrawSkinCatalog()
    {
        auto* actor = SelectedActor();
        if (!actor) {
            ImGui::TextUnformatted(Text("액터를 선택하세요.", "Select an actor.", "请选择角色。"));
            return;
        }
        const auto* base = actor->GetActorBase();
        const bool female = !base || base->GetSex() == RE::SEX::kFemale;
        const auto actorFamily = bcn::body_family::ResolveActor(actor);

        const auto refreshLabel = std::string{ Text("새로고침", "Refresh", "刷新") } + "##skinCatalogRefresh";
        if (ImGui::Button(refreshLabel.c_str())) {
            bcn::SkinProfiles::Get().Refresh();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", Text("BodySkin\\의 Skin 폴더를 읽습니다.", "Reads Skin folders under BodySkin\\.", "读取 BodySkin\\ 下的 Skin 文件夹。"));

        const auto skins = bcn::SkinProfiles::Get().Snapshot();
        const auto settings = bcn::Settings::Get().Snapshot();
        const auto backendCurrentSkin = bcn::skin_override::CurrentProfileId(actor);
        const auto confirmedSkinId = g_pendingSkin && g_pendingSkin->actorFormID == actor->GetFormID() ?
            g_pendingSkin->originalId : backendCurrentSkin.value_or(std::string{});

        std::vector<const bcn::SkinProfile*> visibleSkins;
        visibleSkins.reserve(skins.size());
        for (const auto& skin : skins) {
            if ((female && skin.sex != bcn::SkinSex::female) || (!female && skin.sex != bcn::SkinSex::male)) continue;
            if (!bcn::SkinMatchesActor(skin.bodyFamilies, actorFamily)) continue;
            if (!g_search.empty() && Lower(skin.name).find(Lower(g_search)) == std::string::npos &&
                Lower(skin.id).find(Lower(g_search)) == std::string::npos) continue;
            const auto favorite = std::ranges::find(settings.favoriteSkinProfiles, skin.id) !=
                settings.favoriteSkinProfiles.end();
            if (FavoritesOnly() && !favorite) continue;
            visibleSkins.push_back(&skin);
        }
        std::size_t preferredIndex{};
        if (!confirmedSkinId.empty()) {
            const auto current = std::ranges::find(visibleSkins, confirmedSkinId,
                [](const bcn::SkinProfile* skin) -> const std::string& { return skin->id; });
            if (current != visibleSkins.end()) preferredIndex = 1U + static_cast<std::size_t>(current - visibleSkins.begin());
        }
        const auto navigation = HandleCatalogNavigation(visibleSkins.size() + 1U, preferredIndex);
        const auto previewRow = [&](const std::size_t row) {
            if (row == 0U) {
                if (QueueDefaultSkin(false)) RememberPending(g_pendingSkin, actor, {}, true, confirmedSkinId);
                return;
            }
            const auto& skin = *visibleSkins[row - 1U];
            const auto result = bcn::skin_override::QueueApply(actor, skin.id);
            if (result == bcn::skin_override::ApplyResult::queued) {
                RememberPending(g_pendingSkin, actor, skin.id, false, confirmedSkinId);
            } else {
                bcn::ui::Notify(skin.name + " · " + SkinApplyResultMessage(result));
            }
        };
        const auto confirmRow = [&](const std::size_t row) {
            if (row == 0U) {
                if (QueueDefaultSkin(true)) g_pendingSkin.reset();
                return;
            }
            const auto& skin = *visibleSkins[row - 1U];
            const auto result = bcn::skin_override::QueueApply(actor, skin.id);
            if (result == bcn::skin_override::ApplyResult::queued) {
                SaveManualSkinIfNeeded(actor, skin.id);
                g_pendingSkin.reset();
            } else {
                bcn::ui::Notify(skin.name + " · " + SkinApplyResultMessage(result));
            }
        };
        if (navigation.preview) previewRow(navigation.focused);
        if (navigation.confirm) confirmRow(navigation.focused);

        if (ImGui::BeginChild("SkinCatalog", ImVec2(0.0F, CatalogListHeight()), true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs)) {
            std::size_t row{};
            ImGui::PushID("DefaultSkin");
            const auto defaultCursor = ImGui::GetCursorScreenPos();
            const auto defaultWidth = ImGui::GetContentRegionAvail().x;
            const auto defaultHeight = Scaled(48.0F);
            ImGui::InvisibleButton("item", ImVec2(defaultWidth, defaultHeight));
            const auto defaultHovered = ImGui::IsItemHovered();
            const auto defaultClicked = ImGui::IsItemClicked();
            const auto defaultDoubleClicked = defaultHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            const auto defaultDraw = ImGui::GetWindowDrawList();
            defaultDraw->AddRectFilled(defaultCursor, ImVec2(defaultCursor.x + defaultWidth, defaultCursor.y + defaultHeight),
                confirmedSkinId.empty() ? IM_COL32(48, 103, 129, 255) :
                defaultHovered || (navigation.hasFocus && navigation.focused == row) ?
                IM_COL32(42, 63, 77, 255) : IM_COL32(35, 47, 57, 255), Scaled(4.0F));
            defaultDraw->AddText(ImVec2(defaultCursor.x + Scaled(10.0F), defaultCursor.y + Scaled(7.0F)), IM_COL32(238, 244, 248, 255),
                Text("기본 스킨", "Default skin", "默认皮肤"));
            defaultDraw->AddText(ImVec2(defaultCursor.x + Scaled(10.0F), defaultCursor.y + Scaled(27.0F)), IM_COL32(160, 181, 193, 255),
                Text("몸 · 손 · 발 · 얼굴 텍스처 오버라이드 제거", "Remove body · hands · feet · face texture overrides", "移除身体 · 手 · 脚 · 脸部纹理覆盖"));
            if (defaultDoubleClicked) {
                FocusCatalogRow(row);
                confirmRow(row);
            } else if (defaultClicked) {
                FocusCatalogRow(row);
                previewRow(row);
            }
            ScrollFocusedCatalogRow(row);
            ImGui::Dummy(ImVec2(0.0F, Scaled(5.0F)));
            ImGui::PopID();
            ++row;
            const auto hasMatchingSkin = std::ranges::any_of(skins, [female, actorFamily](const auto& skin) {
                return ((female && skin.sex == bcn::SkinSex::female) ||
                    (!female && skin.sex == bcn::SkinSex::male)) &&
                    bcn::SkinMatchesActor(skin.bodyFamilies, actorFamily);
            });
            if (!hasMatchingSkin) {
                ImGui::TextUnformatted(Text("이 액터의 성별·바디 계열에 맞는 스킨팩을 찾지 못했습니다.", "No skin packs were found for this actor's sex and body family.", "未找到适用于该角色性别和身体系列的皮肤包。"));
                ImGui::Spacing();
                ImGui::TextWrapped("%s", Text(
                    "일반 스킨은 BodySkin\\<스킨 이름>\\Textures\\actors\\character 구조를, UBE 스킨은 BodySkin\\<스킨 이름>\\Textures\\!UBE\\Body 및 Head 구조를 그대로 유지하세요.",
                    "Keep conventional skins under BodySkin\\<skin name>\\Textures\\actors\\character, and UBE skins under BodySkin\\<skin name>\\Textures\\!UBE\\Body and Head.",
                    "普通皮肤请保留 BodySkin\\<皮肤名称>\\Textures\\actors\\character 结构；UBE 皮肤请保留 BodySkin\\<皮肤名称>\\Textures\\!UBE\\Body 和 Head 结构。"));
            }
            for (const auto* skinPointer : visibleSkins) {
                const auto& skin = *skinPointer;
                const bool favorite = std::ranges::find(settings.favoriteSkinProfiles, skin.id) !=
                    settings.favoriteSkinProfiles.end();
                ImGui::PushID(skin.id.c_str());
                const auto cursor = ImGui::GetCursorScreenPos();
                const auto width = ImGui::GetContentRegionAvail().x;
                const auto height = Scaled(48.0F);
                const auto favoriteWidth = Scaled(46.0F);
                ImGui::InvisibleButton("item", ImVec2((std::max)(0.0F, width - favoriteWidth), height));
                const auto hovered = ImGui::IsItemHovered();
                const auto clicked = ImGui::IsItemClicked();
                const auto doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                const auto confirmedCurrent = skin.id == confirmedSkinId;
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                    confirmedCurrent ? IM_COL32(48, 103, 129, 255) :
                    hovered || (navigation.hasFocus && navigation.focused == row) ?
                    IM_COL32(42, 63, 77, 255) : IM_COL32(35, 47, 57, 255), Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)),
                    IM_COL32(238, 244, 248, 255), skin.name.c_str());
                std::unordered_set<std::string> texturePaths;
                const auto collectPaths = [&texturePaths](const auto& layers) {
                    for (const auto& layer : layers) texturePaths.insert(layer.path);
                };
                collectPaths(skin.body);
                collectPaths(skin.hands);
                collectPaths(skin.feet);
                collectPaths(skin.face);
                collectPaths(skin.vampireFace);
                collectPaths(skin.faceDetails);
                const auto textureCount = texturePaths.size();
                const auto sub = std::string{ female ? Text("여성", "Female", "女性") : Text("남성", "Male", "男性") } +
                    " · " + bcn::SkinFamilyLabel(skin.bodyFamilies, skin.sex) +
                    " · " + Text("텍스처 ", "Textures ", "纹理 ") + std::to_string(textureCount) + Text("개", "", " 个") +
                    (confirmedCurrent ? " · " + std::string(Text("현재 적용", "Current", "当前应用")) : "");
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)),
                    IM_COL32(160, 181, 193, 255), sub.c_str());
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + width - favoriteWidth, cursor.y));
                if (FavoriteButton(favorite, height)) ToggleSkinFavorite(skin.id);
                if (doubleClicked) {
                    FocusCatalogRow(row);
                    confirmRow(row);
                } else if (clicked) {
                    FocusCatalogRow(row);
                    previewRow(row);
                }
                ScrollFocusedCatalogRow(row);
                ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + height + Scaled(5.0F)));
                ImGui::Dummy(ImVec2(0.0F, 0.0F));
                ImGui::PopID();
                ++row;
            }
        }
        ImGui::EndChild();
    }

    void DrawPlayerTintCatalog()
    {
        auto* selectedActor = SelectedActor();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!selectedActor || !player || selectedActor->GetFormID() != player->GetFormID()) {
            ImGui::TextUnformatted(Text("틴트는 플레이어에게만 적용됩니다.", "Tint applies to the player only.", "色调仅应用于玩家。"));
            ImGui::TextDisabled("%s", Text("상단 액터 목록에서 플레이어를 선택하세요.", "Select Player in the actor list above.", "请在上方角色列表中选择玩家。"));
            return;
        }

        const auto refreshLabel = std::string{ Text("새로고침", "Refresh", "刷新") } + "##tintCatalogRefresh";
        if (ImGui::Button(refreshLabel.c_str())) {
            bcn::player_tint::Catalog::Get().Refresh();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", Text("TintMask\\의 Tint 폴더를 읽습니다.", "Reads Tint folders under TintMask\\.", "读取 TintMask\\ 下的 Tint 文件夹。"));

        const auto* base = selectedActor->GetActorBase();
        const bool female = base && base->GetSex() == RE::SEX::kFemale;
        const auto actorFamily = bcn::body_family::ResolveActor(selectedActor);
        const auto assets = bcn::player_tint::Catalog::Get().Snapshot();
        const auto settings = bcn::Settings::Get().Snapshot();
        struct TintPackRow final
        {
            std::string name;
            std::size_t count{};
            bcn::body_family::Mask bodyFamilies{};
        };
        std::vector<TintPackRow> packs;
        for (const auto& asset : assets) {
            if ((asset.sex == bcn::player_tint::Sex::female && !female) ||
                (asset.sex == bcn::player_tint::Sex::male && female)) {
                continue;
            }
            if (!bcn::player_tint::TintMatchesActor(asset.bodyFamilies, actorFamily)) continue;
            const auto found = std::ranges::find(packs, asset.pack, &TintPackRow::name);
            if (found == packs.end()) packs.push_back({ asset.pack, 1U, asset.bodyFamilies });
            else {
                ++found->count;
                found->bodyFamilies |= asset.bodyFamilies;
            }
        }
        if (std::ranges::find(packs, g_selectedTintPack, &TintPackRow::name) == packs.end()) {
            g_selectedTintPack = packs.empty() ? std::string{} : packs.front().name;
        }
        const auto confirmedTintPack = g_pendingTint && g_pendingTint->actorFormID == selectedActor->GetFormID() ?
            g_pendingTint->originalId : g_currentTintPack;

        std::vector<const TintPackRow*> visiblePacks;
        visiblePacks.reserve(packs.size());
        for (const auto& pack : packs) {
            if (!g_search.empty() && Lower(pack.name).find(Lower(g_search)) == std::string::npos) continue;
            const auto favorite = std::ranges::find(settings.favoriteTintPacks, pack.name) !=
                settings.favoriteTintPacks.end();
            if (FavoritesOnly() && !favorite) continue;
            visiblePacks.push_back(&pack);
        }
        std::size_t preferredIndex{};
        if (!confirmedTintPack.empty()) {
            const auto current = std::ranges::find(visiblePacks, confirmedTintPack,
                [](const TintPackRow* pack) -> const std::string& { return pack->name; });
            if (current != visiblePacks.end()) preferredIndex = 1U + static_cast<std::size_t>(current - visiblePacks.begin());
        }
        const auto navigation = HandleCatalogNavigation(visiblePacks.size() + 1U, preferredIndex);
        const auto selectDefault = [&](const bool confirm) {
            const auto result = bcn::player_tint::QueueRestoreAll();
            if (result == bcn::player_tint::ApplyResult::queued) {
                if (confirm) g_pendingTint.reset();
                else RememberPending(g_pendingTint, selectedActor, {}, true, confirmedTintPack);
                g_currentTintPack.clear();
                g_selectedTintPack.clear();
            } else {
                bcn::ui::Notify(TintResultText(result));
            }
        };
        const auto selectPack = [&](const std::size_t row, const bool confirm) {
            const auto& pack = *visiblePacks[row - 1U];
            g_selectedTintPack = pack.name;
            const auto result = bcn::player_tint::QueueApplyPack(pack.name);
            if (result == bcn::player_tint::ApplyResult::queued) {
                if (confirm) g_pendingTint.reset();
                else RememberPending(g_pendingTint, selectedActor, pack.name, false, confirmedTintPack);
                g_currentTintPack = pack.name;
            } else {
                bcn::ui::Notify(pack.name + " · " + TintResultText(result));
            }
        };
        if (navigation.preview) {
            if (navigation.focused == 0U) selectDefault(false);
            else selectPack(navigation.focused, false);
        }
        if (navigation.confirm) {
            if (navigation.focused == 0U) selectDefault(true);
            else selectPack(navigation.focused, true);
        }

        if (ImGui::BeginChild("TintPackCatalog", ImVec2(0.0F, CatalogListHeight()), true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs)) {
            std::size_t row{};
            ImGui::PushID("DefaultTint");
            auto cursor = ImGui::GetCursorScreenPos();
            auto width = ImGui::GetContentRegionAvail().x;
            const auto height = Scaled(48.0F);
            ImGui::InvisibleButton("item", ImVec2(width, height));
            auto hovered = ImGui::IsItemHovered();
            const auto clicked = ImGui::IsItemClicked();
            const auto doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            auto* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                confirmedTintPack.empty() ? IM_COL32(48, 103, 129, 255) :
                hovered || (navigation.hasFocus && navigation.focused == row) ?
                IM_COL32(42, 63, 77, 255) : IM_COL32(35, 47, 57, 255), Scaled(4.0F));
            draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), IM_COL32(238, 244, 248, 255),
                Text("기본 틴트", "Default tint", "默认色调"));
            draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)), IM_COL32(160, 181, 193, 255),
                Text("첫 변경 전에 저장한 RaceMenu 원본 레이어 복원", "Restore RaceMenu source layers saved before the first change", "还原首次更改前保存的 RaceMenu 原始图层"));
            if (doubleClicked) {
                FocusCatalogRow(row);
                selectDefault(true);
            } else if (clicked) {
                FocusCatalogRow(row);
                selectDefault(false);
            }
            ScrollFocusedCatalogRow(row);
            ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + height + Scaled(5.0F)));
            ImGui::Dummy(ImVec2(0.0F, 0.0F));
            ImGui::PopID();
            ++row;

            if (packs.empty()) {
                ImGui::TextUnformatted(Text("플레이어의 성별·바디 계열에 맞는 틴트팩을 찾지 못했습니다.", "No tint packs were found for the player's sex and body family.", "未找到适用于玩家性别和身体系列的色调包。"));
                ImGui::TextWrapped("%s", Text(
                    "MO2 모드 루트의 TintMask\\<틴트팩>\\textures\\actors\\character\\character assets\\tintmasks에 DDS 파일을 넣고 새로고침하세요.",
                    "Place DDS files in TintMask\\<tint pack>\\textures\\actors\\character\\character assets\\tintmasks at the MO2 mod root, then refresh.",
                    "请将 DDS 文件放入 MO2 模组根目录的 TintMask\\<色调包>\\textures\\actors\\character\\character assets\\tintmasks，然后刷新。"));
                ImGui::Spacing();
            }

            for (const auto* packPointer : visiblePacks) {
                const auto& pack = *packPointer;
                const auto favorite = std::ranges::find(settings.favoriteTintPacks, pack.name) !=
                    settings.favoriteTintPacks.end();
                ImGui::PushID(pack.name.c_str());
                cursor = ImGui::GetCursorScreenPos();
                width = ImGui::GetContentRegionAvail().x;
                const auto favoriteWidth = Scaled(46.0F);
                ImGui::InvisibleButton("item", ImVec2((std::max)(0.0F, width - favoriteWidth), height));
                hovered = ImGui::IsItemHovered();
                const auto packClicked = ImGui::IsItemClicked();
                const auto packDoubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                const auto selected = pack.name == confirmedTintPack;
                draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height), selected ?
                    IM_COL32(48, 103, 129, 255) : hovered || (navigation.hasFocus && navigation.focused == row) ?
                    IM_COL32(42, 63, 77, 255) : IM_COL32(35, 47, 57, 255), Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), IM_COL32(238, 244, 248, 255), pack.name.c_str());
                const auto sub = std::string{ female ? Text("여성", "Female", "女性") : Text("남성", "Male", "男性") } +
                    " · " + bcn::player_tint::TintFamilyLabel(pack.bodyFamilies) +
                    " · DDS " + std::to_string(pack.count) + Text("개", "", " 个");
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)), IM_COL32(160, 181, 193, 255), sub.c_str());
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + width - favoriteWidth, cursor.y));
                if (FavoriteButton(favorite, height)) ToggleTintFavorite(pack.name);
                if (packDoubleClicked) {
                    FocusCatalogRow(row);
                    selectPack(row, true);
                } else if (packClicked) {
                    FocusCatalogRow(row);
                    selectPack(row, false);
                }
                ScrollFocusedCatalogRow(row);
                ImGui::SetCursorScreenPos(ImVec2(cursor.x, cursor.y + height + Scaled(5.0F)));
                ImGui::Dummy(ImVec2(0.0F, 0.0F));
                ImGui::PopID();
                ++row;
            }
        }
        ImGui::EndChild();

        constexpr std::array allLayers{
            bcn::player_tint::Layer::frekles, bcn::player_tint::Layer::lips,
            bcn::player_tint::Layer::cheeks, bcn::player_tint::Layer::eyeliner,
            bcn::player_tint::Layer::upperEyeSocket, bcn::player_tint::Layer::lowerEyeSocket,
            bcn::player_tint::Layer::skinTone, bcn::player_tint::Layer::warPaint,
            bcn::player_tint::Layer::frownLines, bcn::player_tint::Layer::lowerCheeks,
            bcn::player_tint::Layer::nose, bcn::player_tint::Layer::chin,
            bcn::player_tint::Layer::neck, bcn::player_tint::Layer::forehead,
            bcn::player_tint::Layer::dirt
        };
        std::vector<std::pair<bcn::player_tint::Layer, bcn::player_tint::Asset>> availableLayers;
        if (!g_selectedTintPack.empty()) {
            for (const auto layer : allLayers) {
                if (auto asset = bcn::player_tint::BestAssetForPlayer(g_selectedTintPack, layer)) {
                    availableLayers.emplace_back(layer, std::move(*asset));
                }
            }
        }
        auto selectedLayer = std::ranges::find(availableLayers, g_selectedTintLayer,
            [](const auto& entry) { return entry.first; });
        if (selectedLayer == availableLayers.end() && !availableLayers.empty()) {
            g_selectedTintLayer = availableLayers.front().first;
            selectedLayer = availableLayers.begin();
        }
        const auto nextAssetID = selectedLayer == availableLayers.end() ?
            std::string{} : selectedLayer->second.id;
        if (g_selectedTintAssetID != nextAssetID) {
            g_selectedTintAssetID = nextAssetID;
            if (const auto color = bcn::player_tint::CurrentColor(g_selectedTintLayer)) {
                g_tintColor = { color->red, color->green, color->blue, color->alpha };
            }
        }

        if (!availableLayers.empty()) {
            ImGui::SetNextItemWidth(Scaled(190.0F));
            PrepareResizableDropdown(availableLayers.size());
            if (ImGui::BeginCombo("##tintPart", TintLayerText(g_selectedTintLayer))) {
                for (const auto& [layer, asset] : availableLayers) {
                    const auto isSelected = layer == g_selectedTintLayer;
                    ImGui::PushID(asset.id.c_str());
                    if (ImGui::Selectable(TintLayerText(layer), isSelected)) {
                        g_selectedTintLayer = layer;
                        g_selectedTintAssetID = asset.id;
                        if (const auto color = bcn::player_tint::CurrentColor(layer)) {
                            g_tintColor = { color->red, color->green, color->blue, color->alpha };
                        }
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::ColorButton("##tintColorPreview",
                ImVec4(g_tintColor[0], g_tintColor[1], g_tintColor[2], g_tintColor[3]),
                ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
            ImGui::SameLine();
            if (ImGui::Button(Text("틴트 값 상세 조절", "Adjust tint values", "调整色调值"))) {
                g_showTintDetails = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("틴트 값 복원", "Restore tint values", "还原色调值"))) {
                const auto result = bcn::player_tint::QueueRestore(g_selectedTintLayer, g_selectedTintPack);
                if (result == bcn::player_tint::ApplyResult::queued) {
                    if (const auto color = bcn::player_tint::OriginalColor(g_selectedTintLayer)) {
                        g_tintColor = { color->red, color->green, color->blue, color->alpha };
                    }
                } else {
                    bcn::ui::Notify(TintResultText(result));
                }
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(Scaled(190.0F));
            PrepareResizableDropdown(1U);
            if (ImGui::BeginCombo("##tintPart", Text("적용 가능한 부위 없음", "No available part", "无可用部位"))) {
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::Button(Text("틴트 값 상세 조절", "Adjust tint values", "调整色调值"));
            ImGui::SameLine();
            ImGui::Button(Text("틴트 값 복원", "Restore tint values", "还原色调值"));
            ImGui::EndDisabled();
        }
    }

    void DrawTintDetailPopup()
    {
        if (!g_showTintDetails) return;
        const auto title = std::string{ Text("틴트 상세 값", "Tint details", "色调详情") } + "###TintDetails";
        ImGui::OpenPopup(title.c_str());
        if (BeginUndimmedPopupModal(title.c_str(), &g_showTintDetails, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (EscapePressed()) {
                g_showTintDetails = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            const auto colorChanged = ImGui::ColorPicker4(Text("색상 및 강도", "Color and opacity", "颜色与不透明度"),
                g_tintColor.data(), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
            const auto colorFinished = ImGui::IsItemDeactivatedAfterEdit();
            const auto now = std::chrono::steady_clock::now();
            const auto liveUpdateDue = colorChanged &&
                now - g_lastTintDetailApply >= std::chrono::milliseconds(100);
            if ((liveUpdateDue || colorFinished) && !g_selectedTintAssetID.empty()) {
                const auto result = bcn::player_tint::QueueApply(g_selectedTintAssetID, {
                    .red = g_tintColor[0], .green = g_tintColor[1],
                    .blue = g_tintColor[2], .alpha = g_tintColor[3]
                });
                if (result == bcn::player_tint::ApplyResult::queued) {
                    g_lastTintDetailApply = now;
                } else {
                    bcn::ui::Notify(TintResultText(result));
                }
            }
            if (ImGui::Button(Text("완료", "Done", "完成"))) g_showTintDetails = false;
            ImGui::EndPopup();
        }
    }

    void DrawDistributionPopup()
    {
        if (!g_showDistribution) {
            ResetDistributionEditor();
            return;
        }
        EnsureDistributionEditor();
        const auto popupTitle = std::string{ Text("NPC 배포 규칙", "NPC distribution rules", "NPC 分发规则") } + "###DistributionPopup";
        ImGui::OpenPopup(popupTitle.c_str());
        const std::array footerLabels{
            Text("+ 규칙 추가", "+ Add rule", "+ 添加规则"),
            Text("- 규칙 삭제", "- Delete rule", "- 删除规则"),
            Text("위 우선순위", "Move priority up", "提高优先级"),
            Text("아래 우선순위", "Move priority down", "降低优先级"),
            Text("저장 값 불러오기", "Load saved values", "加载保存值"),
            Text("로드된 NPC 즉시 배포", "Distribute to loaded NPCs now", "立即分发给已加载的 NPC"),
            Text("다음 게임 실행 시 배포", "Distribute on next game launch", "下次启动游戏时分发")
        };
        const auto& style = ImGui::GetStyle();
        auto footerContentWidth = style.ItemSpacing.x * static_cast<float>(footerLabels.size() - 1U);
        for (const auto* label : footerLabels) {
            footerContentWidth += ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0F;
        }
        auto popupSize = DefaultWindowSize(920.0F, 520.0F);
        const auto footerWindowWidth = footerContentWidth + style.WindowPadding.x * 2.0F;
        popupSize.x = (std::max)(popupSize.x, footerWindowWidth);
        auto maximumSize = ImVec2(FLT_MAX, FLT_MAX);
        if (const auto* viewport = ImGui::GetMainViewport()) {
            maximumSize = ImVec2(viewport->WorkSize.x * 0.96F, viewport->WorkSize.y * 0.94F);
            popupSize.x = (std::min)(popupSize.x, maximumSize.x);
        }
        // The footer is deliberately one unbroken row.  Prevent manual popup
        // resizing from making it narrower than the seven localized buttons.
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(popupSize.x, Scaled(360.0F)), maximumSize);
        ImGui::SetNextWindowSize(popupSize, ImGuiCond_Appearing);
        if (BeginUndimmedPopupModal(popupTitle.c_str(), &g_showDistribution,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse)) {
            if (EscapePressed()) {
                g_showDistribution = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                ResetDistributionEditor();
                return;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", Text("위에서부터 평가합니다. 처음 일치한 배포 규칙의 바디·스킨만 사용하며, 비어 있는 경우 해당 항목을 바꾸지 않습니다.", "Rules are evaluated top-down. Only the body and skin settings from the first matching rule are used; an empty item is left unchanged.", "规则从上至下评估。只使用第一条匹配规则的身体与皮肤设置；空项目保持不变。"));
            ImGui::PopStyleColor();
            ImGui::Separator();
            const auto ruleListWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.36F,
                Scaled(280.0F), Scaled(380.0F));
            // Reserve one complete footer row. Without NoHostExtendY, a table
            // row containing zero-height children may grow to the host window's
            // bottom and permanently push the action buttons out of view even
            // when the popup itself is resized.
            const auto footerHeight = ImGui::GetFrameHeightWithSpacing() + Scaled(2.0F);
            const auto editorHeight = (std::max)(Scaled(220.0F),
                ImGui::GetContentRegionAvail().y - footerHeight);
            if (ImGui::BeginTable("DistributionEditor", 2,
                ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_NoHostExtendY,
                    ImVec2(0.0F, editorHeight))) {
                ImGui::TableSetupColumn(Text("규칙 우선순위", "Rule priority", "规则优先级"),
                    ImGuiTableColumnFlags_WidthFixed, ruleListWidth);
                ImGui::TableSetupColumn(Text("선택한 규칙", "Selected rule", "当前规则"), ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::BeginChild("RuleList", ImVec2(0.0F, 0.0F), false)) {
                    for (std::size_t index{}; index < g_distributionRules.size(); ++index) {
                        const auto& rule = g_distributionRules[index];
                        ImGui::PushID(rule.id.c_str());
                        std::string summary;
                        if (rule.bodyExcluded && rule.skinExcluded) {
                            summary = Text("배포 제외", "Excluded from distribution", "排除分发");
                        } else if (rule.bodyExcluded) {
                            summary = Text("바디 배포 제외 · ", "Body excluded · ", "身体排除 · ") +
                                std::to_string(rule.skinProfileIds.size()) + Text("개 스킨", " skins", " 个皮肤");
                        } else if (rule.skinExcluded) {
                            summary = std::to_string(rule.presetIds.size()) + Text("개 바디 · 스킨 배포 제외", " bodies · Skin excluded", " 个身体 · 皮肤排除");
                        } else {
                            summary = std::to_string(rule.presetIds.size()) + Text("개 바디 · ", " bodies · ", " 个身体 · ") +
                                std::to_string(rule.skinProfileIds.size()) + Text("개 스킨", " skins", " 个皮肤");
                        }
                        const auto detail = std::to_string(index + 1U) + "  " + rule.name + "\n    " + summary;
                        const auto rowStart = ImGui::GetCursorScreenPos();
                        const auto rowWidth = ImGui::GetContentRegionAvail().x;
                        const auto padding = Scaled(7.0F);
                        const auto textSize = ImGui::CalcTextSize(detail.c_str(), nullptr, false,
                            (std::max)(1.0F, rowWidth - padding * 2.0F));
                        const auto rowHeight = (std::max)(Scaled(52.0F), textSize.y + padding * 2.0F);
                        ImGui::InvisibleButton("rule", ImVec2(rowWidth, rowHeight));
                        const auto hovered = ImGui::IsItemHovered();
                        if (ImGui::IsItemClicked()) {
                            g_selectedDistributionRule = index;
                        }
                        auto* draw = ImGui::GetWindowDrawList();
                        if (index == g_selectedDistributionRule || hovered) {
                            draw->AddRectFilled(rowStart,
                                ImVec2(rowStart.x + rowWidth, rowStart.y + rowHeight),
                                index == g_selectedDistributionRule ? IM_COL32(48, 103, 129, 255) :
                                IM_COL32(42, 63, 77, 255), Scaled(3.0F));
                        }
                        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                            ImVec2(rowStart.x + padding, rowStart.y + padding),
                            ImGui::GetColorU32(ImGuiCol_Text), detail.c_str(), nullptr,
                            (std::max)(1.0F, rowWidth - padding * 2.0F));
                        ImGui::PopID();
                    }
                    if (g_distributionRules.empty()) {
                        ImGui::TextDisabled("%s", Text("규칙이 없습니다.", "No rules yet.", "尚无规则。"));
                    }
                }
                ImGui::EndChild();

                ImGui::TableSetColumnIndex(1);
                if (g_selectedDistributionRule < g_distributionRules.size()) {
                    auto& rule = g_distributionRules[g_selectedDistributionRule];
                    ImGui::PushID(rule.id.c_str());
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputText("##ruleName", &rule.name);

                    int sex = rule.female ? 0 : 1;
                    ImGui::TextUnformatted(Text("성별", "Sex", "性别"));
                    ImGui::SetNextItemWidth(Scaled(180.0F));
                    PrepareResizableDropdown(2U);
                    if (ImGui::Combo("##ruleSex", &sex, Text("여성\0남성\0", "Female\0Male\0", "女性\0男性\0"))) {
                        rule.female = sex == 0;
                        rule.bodyFamily.clear();
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted(Text("대상 범위", "Scope", "目标范围"));
                    ImGui::SetNextItemWidth(-1.0F);
                    if (DistributionScopeCombo(rule.scope)) {
                        rule.target.clear();
                        rule.npcBaseFormID = 0;
                        rule.npcPlugin.clear();
                        rule.npcLocalFormID = 0;
                        if (rule.scope == bcn::DistributionScope::modInstalledFollower ||
                            rule.scope == bcn::DistributionScope::elderNPC) {
                            rule.bodyExcluded = true;
                        } else {
                            FillRuleTargetFromSelectedActor(rule);
                        }
                    }
                    if (rule.scope == bcn::DistributionScope::npcBaseForm) {
                        ImGui::TextUnformatted(Text("NPC BaseID 또는 RefID", "NPC BaseID or RefID", "NPC BaseID 或 RefID"));
                        ImGui::SetNextItemWidth(-1.0F);
                        if (ImGui::InputScalar("##ruleFormID", ImGuiDataType_U32, &rule.npcBaseFormID,
                            nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal |
                            ImGuiInputTextFlags_CharsUppercase)) {
                            if (auto* form = RE::TESForm::LookupByID(rule.npcBaseFormID);
                                !bcn::SetDistributionRuleNPC(rule, form)) {
                                rule.npcPlugin.clear();
                                rule.npcLocalFormID = 0U;
                            }
                        }
                        if (!rule.npcPlugin.empty()) {
                            ImGui::TextDisabled("%s · %06X", rule.npcPlugin.c_str(), rule.npcLocalFormID);
                        } else if (rule.npcBaseFormID != 0U) {
                            ImGui::TextColored(ImVec4(1.0F, .62F, .35F, 1.0F), "%s",
                                Text("NPC Base 또는 Ref를 찾지 못했습니다.", "No NPC base or reference was found.", "找不到 NPC 基础或引用。"));
                        }
                    } else if (rule.scope == bcn::DistributionScope::npcName) {
                        ImGui::TextUnformatted(TargetLabel(rule.scope));
                        ImGui::SetNextItemWidth(-1.0F);
                        ImGui::InputText("##ruleTarget", &rule.target);
                    } else if (rule.scope == bcn::DistributionScope::factionEditorID) {
                        ImGui::TextUnformatted(TargetLabel(rule.scope));
                        ImGui::SetNextItemWidth(-1.0F);
                        [[maybe_unused]] const auto changed =
                            DistributionTargetCombo("##ruleFaction", rule.target, g_distributionFactionOptions);
                    } else if (rule.scope == bcn::DistributionScope::pluginFile) {
                        ImGui::TextUnformatted(TargetLabel(rule.scope));
                        ImGui::SetNextItemWidth(-1.0F);
                        [[maybe_unused]] const auto changed =
                            DistributionTargetCombo("##rulePlugin", rule.target, g_distributionPluginOptions);
                    } else if (rule.scope == bcn::DistributionScope::raceEditorID) {
                        ImGui::TextUnformatted(TargetLabel(rule.scope));
                        ImGui::SetNextItemWidth(-1.0F);
                        [[maybe_unused]] const auto changed =
                            DistributionTargetCombo("##ruleRace", rule.target, g_distributionRaceOptions);
                    }

                    if (TabButton(Text("바디", "Body", "身体"), g_distributionPool == DistributionPool::body)) {
                        g_distributionPool = DistributionPool::body;
                    }
                    ImGui::SameLine();
                    if (TabButton(Text("스킨", "Skin", "皮肤"), g_distributionPool == DistributionPool::skin)) {
                        g_distributionPool = DistributionPool::skin;
                    }
                    ImGui::SameLine();
                    auto& channelExcluded = g_distributionPool == DistributionPool::body ?
                        rule.bodyExcluded : rule.skinExcluded;
                    const auto channelMode = channelExcluded ? 1 : 0;
                    if (ImGui::RadioButton(Text("배포", "Distribute", "分发"), channelMode == 0)) {
                        channelExcluded = false;
                        rule.enabled = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton(Text("배포 제외", "Exclude from distribution", "排除分发"), channelMode == 1)) {
                        channelExcluded = true;
                        rule.enabled = true;
                    }
                    ImGui::Separator();
                    if (channelExcluded) {
                        ImGui::TextColored(ImVec4(1.0F, .74F, .35F, 1.0F), "%s",
                            g_distributionPool == DistributionPool::body ?
                                Text("이 규칙에 맞는 NPC의 바디는 배포하지 않습니다.", "Body distribution is disabled for NPCs matching this rule.", "不向匹配此规则的 NPC 分发身体。") :
                                Text("이 규칙에 맞는 NPC의 스킨은 배포하지 않습니다.", "Skin distribution is disabled for NPCs matching this rule.", "不向匹配此规则的 NPC 分发皮肤。"));
                    } else if (g_distributionPool == DistributionPool::skin) {
                    const auto skins = bcn::SkinProfiles::Get().Snapshot();
                    ImGui::TextDisabled("%s", Text("이 규칙 전용 스킨 풀 — 하나면 고정, 여러 개면 이 규칙의 NPC마다 안정적으로 랜덤 배포됩니다.", "This rule's skin pool — one skin pack is fixed; multiple skin packs are stably randomized per matching NPC.", "本规则专用皮肤池 — 选择一个则固定，多个则按匹配 NPC 稳定随机分发。"));
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputTextWithHint("##skinPoolSearch",
                        Text("스킨명 검색", "Search skin names", "搜索皮肤名称"), &g_distributionSkinSearch);
                    const auto poolHeight = (std::max)(1.0F, ImGui::GetContentRegionAvail().y);
                    if (ImGui::BeginChild("SkinProfilePool", ImVec2(0.0F, poolHeight), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs)) {
                        for (const auto& skin : skins) {
                            if ((skin.sex == bcn::SkinSex::female && !rule.female) || (skin.sex == bcn::SkinSex::male && rule.female)) continue;
                            if (!g_distributionSkinSearch.empty() &&
                                Lower(skin.name).find(Lower(g_distributionSkinSearch)) == std::string::npos) continue;
                            bool selected = ContainsSkinProfile(rule, skin.id);
                            ImGui::PushID(skin.id.c_str());
                            if (ImGui::Checkbox(skin.name.c_str(), &selected)) ToggleRuleSkinProfile(rule, skin.id);
                            ImGui::SameLine();
                            ImGui::TextDisabled("%s", skin.id.c_str());
                            ImGui::PopID();
                        }
                        if (skins.empty()) {
                            ImGui::TextDisabled("%s", Text("스킨팩이 없습니다. 스킨 탭에서 폴더 위치를 확인하세요.", "No skin packs. Check the folder location on the Skin tab.", "没有皮肤包。请在皮肤标签中查看文件夹位置。"));
                        }
                    }
                    ImGui::EndChild();
                    } else {
                    const auto presets = bcn::PresetCatalog::Get().Snapshot();
                    std::vector<std::string> families;
                    families.push_back(Text("전체 바디 계열", "All body families", "全部身体系列"));
                    for (const auto& preset : presets) {
                        if (preset.male != !rule.female || preset.family.empty() || std::ranges::find(families, preset.family) != families.end()) continue;
                        families.push_back(preset.family);
                    }
                    auto familyIndex = 0;
                    if (!rule.bodyFamily.empty()) {
                        const auto found = std::ranges::find(families, rule.bodyFamily);
                        if (found != families.end()) familyIndex = static_cast<int>(std::distance(families.begin(), found));
                    }
                    std::vector<const char*> familyLabels;
                    familyLabels.reserve(families.size());
                    for (const auto& family : families) familyLabels.push_back(family.c_str());
                    ImGui::TextUnformatted(Text("바디 계열", "Body family", "身体系列"));
                    ImGui::SetNextItemWidth(-1.0F);
                    PrepareResizableDropdown(families.size());
                    if (ImGui::Combo("##bodyFamily", &familyIndex, familyLabels.data(), static_cast<int>(familyLabels.size()))) {
                        rule.bodyFamily = familyIndex == 0 ? std::string{} : families[static_cast<std::size_t>(familyIndex)];
                    }
                    ImGui::TextDisabled("%s", Text("이 규칙 전용 바디 풀 — 하나면 고정, 여러 개면 이 규칙의 NPC마다 안정적으로 랜덤 배포됩니다.", "This rule's body pool — one preset is fixed; multiple presets are stably randomized per matching NPC.", "本规则专用身体池 — 选择一个则固定，多个则按匹配 NPC 稳定随机分发。"));
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputTextWithHint("##bodyPoolSearch",
                        Text("바디 프리셋명 검색", "Search body presets", "搜索身体预设"), &g_distributionBodySearch);
                    const auto poolHeight = (std::max)(1.0F, ImGui::GetContentRegionAvail().y);
                    if (ImGui::BeginChild("PresetPool", ImVec2(0.0F, poolHeight), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs)) {
                        for (const auto& preset : presets) {
                            if (preset.male != !rule.female || (!rule.bodyFamily.empty() && preset.family != rule.bodyFamily)) continue;
                            if (!g_distributionBodySearch.empty()) {
                                const auto needle = Lower(g_distributionBodySearch);
                                if (Lower(preset.name).find(needle) == std::string::npos &&
                                    Lower(preset.family).find(needle) == std::string::npos) continue;
                            }
                            const auto id = preset.PersistentId();
                            bool selected = ContainsPreset(rule, id);
                            ImGui::PushID(id.c_str());
                            if (ImGui::Checkbox(preset.name.c_str(), &selected)) ToggleRulePreset(rule, id);
                            ImGui::SameLine();
                            ImGui::TextDisabled("%s", preset.family.c_str());
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();
                    }
                    ImGui::PopID();
                } else {
                    ImGui::TextDisabled("%s", Text("왼쪽에서 규칙을 추가하거나 선택하세요.", "Add or select a rule on the left.", "请在左侧添加或选择规则。"));
                }
            }
            ImGui::EndTable();
            ImGui::Separator();
            if (ImGui::Button(Text("+ 규칙 추가", "+ Add rule", "+ 添加规则"))) {
                g_distributionRules.push_back(NewDistributionRule());
                g_selectedDistributionRule = g_distributionRules.size() - 1U;
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("- 규칙 삭제", "- Delete rule", "- 删除规则")) && g_selectedDistributionRule < g_distributionRules.size()) {
                g_distributionRules.erase(g_distributionRules.begin() + static_cast<std::ptrdiff_t>(g_selectedDistributionRule));
                if (g_selectedDistributionRule >= g_distributionRules.size() && !g_distributionRules.empty()) --g_selectedDistributionRule;
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("위 우선순위", "Move priority up", "提高优先级")) && g_selectedDistributionRule > 0 && g_selectedDistributionRule < g_distributionRules.size()) {
                std::swap(g_distributionRules[g_selectedDistributionRule], g_distributionRules[g_selectedDistributionRule - 1U]);
                --g_selectedDistributionRule;
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("아래 우선순위", "Move priority down", "降低优先级")) && g_selectedDistributionRule + 1U < g_distributionRules.size()) {
                std::swap(g_distributionRules[g_selectedDistributionRule], g_distributionRules[g_selectedDistributionRule + 1U]);
                ++g_selectedDistributionRule;
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("저장 값 불러오기", "Load saved values", "加载保存值"))) {
                const auto loaded = bcn::Distribution::Get().Load();
                const auto imported = bcn::Distribution::Get().ImportOBodyDefaults();
                g_distributionRules = bcn::Distribution::Get().Snapshot();
                g_selectedDistributionRule = 0;
                bcn::ui::Notify(loaded ?
                    (imported ?
                        Text("저장값과 OBody 호환 규칙을 함께 불러왔습니다.", "Loaded saved values and OBody-compatible rules.", "已加载保存值和 OBody 兼容规则。") :
                        Text("저장값을 불러왔습니다.", "Loaded saved values.", "已加载保存值。")) :
                    (imported ?
                        Text("기본 샘플 조건과 OBody 호환 규칙을 불러왔습니다.", "Loaded the default sample rules and OBody-compatible rules.", "已加载默认示例规则和 OBody 兼容规则。") :
                        Text("저장값이 없어 기본 샘플 조건을 불러왔습니다.", "No saved values were found; the default sample rules were loaded.", "未找到保存值，已加载默认示例规则。")));
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("로드된 NPC 즉시 배포", "Distribute to loaded NPCs now", "立即分发给已加载的 NPC"))) {
                if (SaveActiveDistributionRules()) {
                    const auto queued = bcn::Distribution::Get().ApplyLoadedNPCs();
                    bcn::ui::Notify(std::to_string(queued) + Text("명의 변경 대상 NPC를 확인하고 규칙을 저장했습니다.", " changed loaded NPCs were checked and the rules were saved.", " 名已加载 NPC 的变更已检查，规则也已保存。"));
                } else {
                    bcn::ui::Notify(Text("NPC 배포 규칙을 저장하지 못해 즉시 배포하지 않았습니다.", "The rules could not be saved, so immediate distribution was not started.", "无法保存 NPC 分发规则，因此未开始立即分发。"));
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("다음 게임 실행 시 배포", "Distribute on next game launch", "下次启动游戏时分发"))) {
                if (bcn::Distribution::Get().SaveRulesForNextGame(g_distributionRules)) {
                    bcn::ui::Notify(Text("현재 편집 값을 저장했습니다. 현재 게임의 배포 규칙은 바꾸지 않습니다.", "Saved the edited values without changing this session's active distribution rules.", "已保存当前编辑值，不更改本次游戏的有效分发规则。"));
                } else {
                    bcn::ui::Notify(Text("다음 게임 실행용 배포 규칙을 저장하지 못했습니다.", "Could not save the distribution rules for the next game start.", "无法保存下次启动游戏时使用的分发规则。"));
                }
            }
            ImGui::EndPopup();
        }
    }

    void DrawOutfitPopup()
    {
        if (!g_showOutfit) return;
        const auto popupTitle = std::string{ Text("의상·랜덤화", "Outfit · randomization", "服装·随机化") } + "###OutfitPopup";
        ImGui::OpenPopup(popupTitle.c_str());
        if (BeginUndimmedPopupModal(popupTitle.c_str(), &g_showOutfit, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (EscapePressed()) {
                g_showOutfit = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::TextDisabled("%s", Text("지원되는 BodySlide 슬라이더에만 적용", "Only applies to supported BodySlide sliders", "仅适用于受支持的 BodySlide 滑块"));
            ImGui::Separator();
            auto settings = bcn::Settings::Get().Snapshot();
            auto settingsChanged = false;
            const auto refitChanged = ImGui::Checkbox(Text("의상 착용 시 가슴 보정", "Correct breasts while clothed", "穿衣时修正胸部"), &settings.orefitEnabled);
            settingsChanged |= refitChanged;
            ImGui::Indent();
            if (!settings.orefitEnabled) ImGui::BeginDisabled();
            const auto nippleRefitChanged = ImGui::Checkbox(Text("의상 착용 시 유두 보정", "Correct nipples while clothed", "穿衣时修正乳头"), &settings.orefitNippleMorphing);
            settingsChanged |= nippleRefitChanged;
            if (!settings.orefitEnabled) ImGui::EndDisabled();
            ImGui::Unindent();
            if (ImGui::Button(Text("OBody NG 의상 보정 규칙 등록", "Register OBody NG outfit-correction rules", "注册 OBody NG 服装修正规则"))) {
                const auto registered = bcn::OutfitRefit::Get().LoadOBodyRules();
                if (registered) {
                    g_orefitRulesRegistered = true;
                    bcn::OutfitRefit::Get().ProcessActor(SelectedActor());
                } else {
                    bcn::ui::Notify(Text("OBody NG 의상 보정 규칙을 등록하지 못했습니다.", "Could not register OBody NG outfit-correction rules.", "无法注册 OBody NG 服装修正规则。"));
                }
            }
            ImGui::SameLine();
            if (g_orefitRulesRegistered) {
                ImGui::TextColored(ImVec4(.38F, .86F, .62F, 1.0F), "%s",
                    Text("OBody NG 의상 보정 규칙 등록됨", "OBody NG outfit-correction rules registered", "OBody NG 服装修正规则已注册"));
            } else {
                ImGui::TextDisabled("%s", Text("OBody NG 의상 보정 규칙", "OBody NG outfit-correction rules", "OBody NG 服装修正规则"));
            }
            ImGui::Separator();
            const auto nippleRandomizationChanged = ImGui::Checkbox(
                Text("유두 형태 무작위화", "Randomize nipple shape", "随机乳头形态"),
                &settings.nippleRandomization);
            const auto genitalRandomizationChanged = ImGui::Checkbox(
                Text("생식기 형태 무작위화", "Randomize genital shape", "随机生殖器形态"),
                &settings.genitalRandomization);
            settingsChanged |= nippleRandomizationChanged || genitalRandomizationChanged;
            if (settingsChanged) {
                bcn::Settings::Get().Update(settings);
                if (!bcn::Settings::Get().Save()) {
                    bcn::ui::Notify(Text("의상·랜덤화 설정을 저장하지 못했습니다.", "Could not save outfit and randomization settings.", "无法保存服装与随机化设置。"));
                }
                if (refitChanged || nippleRefitChanged) bcn::OutfitRefit::Get().ProcessActor(SelectedActor());
                // The popup has no separate Apply button. Rebuild the owned
                // committed key immediately so disabling randomization also
                // removes values that were generated by the previous state.
                if (nippleRandomizationChanged || genitalRandomizationChanged) {
                    bcn::racemenu::QueueReapplyCurrent(SelectedActor());
                }
                if (refitChanged || nippleRefitChanged || nippleRandomizationChanged ||
                    genitalRandomizationChanged) {
                    // Signatures keep unchanged channels cheap: this scan
                    // updates only the body/outfit result whose option bits
                    // changed, and performance mode spreads distant actors.
                    [[maybe_unused]] const auto queued = bcn::Distribution::Get().ApplyLoadedNPCs();
                }
            }
            ImGui::EndPopup();
        }
    }

    void DrawSettingsPopup()
    {
        if (!g_showSettings) return;
        const auto popupTitle = std::string{ Text("모드 설정", "Mod settings", "模组设置") } + "###SettingsPopup";
        ImGui::OpenPopup(popupTitle.c_str());
        const auto settingsWidth = DefaultWindowSize(700.0F, 0.0F).x;
        // The settings list is intentionally short enough to fit as one
        // panel. Let ImGui derive its height from the localized wrapped text
        // so the final reset/close row is visible without a scrollbar at the
        // current 1080p/2K/4K scale.
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(settingsWidth, 0.0F), ImVec2(settingsWidth, FLT_MAX));
        if (BeginUndimmedPopupModal(popupTitle.c_str(), &g_showSettings,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse)) {
            if (EscapePressed()) {
                g_showSettings = false;
                bcn::InputSink::Get().CancelHotkeyCapture();
                [[maybe_unused]] const auto saved = bcn::Settings::Get().Save();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            // Snapshot() intentionally returns a fresh copy.  Every widget
            // that changes this copy must therefore commit it in this same
            // frame; otherwise the following frame restores the old value.
            auto settings = bcn::Settings::Get().Snapshot();
            auto settingsChanged = false;
            ImGui::TextUnformatted(Text("창 열기 단축키", "Open window shortcut", "窗口打开快捷键"));
            const auto capturing = bcn::InputSink::Get().IsCapturingHotkey();
            const auto label = capturing ? Text("원하는 키 또는 조합을 누르세요...", "Press a key or modifier chord...", "请按下按键或组合键...") : settings.openHotkey.DisplayName();
            if (ImGui::Button(label.c_str(), ImVec2(Scaled(300.0F), 0.0F))) {
                bcn::InputSink::Get().BeginHotkeyCapture();
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("기본 F7", "Default F7", "默认 F7"))) {
                settings.openHotkey = {};
                settingsChanged = true;
            }
            TextDisabledWrapped(Text("Ctrl+F7, Shift+F7, Ctrl+Shift+F7처럼 복합 단축키를 사용할 수 있습니다. ESC는 입력을 취소합니다.", "Modifier chords such as Ctrl+F7 and Ctrl+Shift+F7 are supported. Escape cancels capture.", "支持 Ctrl+F7、Ctrl+Shift+F7 等组合键。Esc 取消输入。"));
            ImGui::Separator();
            int characterPosition = settings.characterPosition == bcn::CharacterPosition::disabled ? 0 :
                settings.characterPosition == bcn::CharacterPosition::left ? 1 : 2;
            ImGui::TextUnformatted(Text("UI 열 때 캐릭터 위치", "Character position while open", "打开 UI 时的角色位置"));
            ImGui::SetNextItemWidth(Scaled(300.0F));
            PrepareResizableDropdown(3U);
            if (ImGui::Combo("##characterPosition", &characterPosition,
                Text("사용 안 함\0왼쪽\0오른쪽\0", "Disabled\0Left\0Right\0", "禁用\0左侧\0右侧\0"))) {
                settings.characterPosition = characterPosition == 0 ? bcn::CharacterPosition::disabled :
                    characterPosition == 1 ? bcn::CharacterPosition::left : bcn::CharacterPosition::right;
                settingsChanged = true;
                bcn::menu_character::Presentation::Get().Apply(settings.characterPosition, SelectedActor());
            }
            TextDisabledWrapped(Text("3인칭에서 선택한 액터를 창 옆에 임시 배치합니다. 캐릭터가 있는 화면 바깥쪽을 우클릭 드래그하면 회전하며, 대상 변경·창 닫기 때 카메라와 방향을 복원합니다.", "In third person, temporarily frames the selected actor beside the window. Right-drag the outer character area to rotate; camera and facing restore when the target changes or the window closes.", "第三人称下会临时将所选角色置于窗口旁。右键拖动角色所在的外侧区域可旋转；切换目标或关闭窗口时会恢复镜头和朝向。"));
            settingsChanged |= ImGui::Checkbox(Text("게임 일시정지", "Pause game while open", "打开时暂停游戏"), &settings.pauseGameWhenOpen);
            TextDisabledWrapped(Text("창은 항상 플레이어를 선택한 상태로 열립니다. 일시정지 변경은 다음에 창을 열 때 적용됩니다.", "The window always opens with Player selected. Pause changes apply the next time it opens.", "窗口始终以玩家为当前选择打开。暂停设置会在下次打开窗口时生效。"));
            ImGui::Separator();
            ImGui::TextUnformatted(Text("화면 표시", "Display", "显示"));
            ImGui::SetNextItemWidth(Scaled(300.0F));
            const auto textScaleChanged = ImGui::SliderFloat(Text("텍스트 크기##textScale", "Text size##textScale", "文字大小##textScale"), &settings.textScale, 0.75F, 1.50F, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
            settingsChanged |= textScaleChanged;
            TextDisabledWrapped(Text("화면 높이를 자동 감지해 1080p는 100%, 2K는 125%, 4K는 150%를 기본 적용합니다. 글자 크기를 바꾸면 버튼·여백·팝업·목록과 창 기본 크기도 같은 비율로 조정됩니다.", "The runtime automatically uses a 100% baseline at 1080p, 125% at 2K, and 150% at 4K. Text size scales buttons, spacing, popups, lists, and the default window size proportionally.", "运行时会自动检测屏幕高度：1080p 为 100%，2K 为 125%，4K 为 150%。调整文字大小时，按钮、间距、弹窗、列表和默认窗口大小也会按相同比例缩放。"));
            ImGui::Separator();
            int language = static_cast<int>(settings.language);
            ImGui::TextUnformatted(Text("언어", "Language", "语言"));
            PrepareResizableDropdown(4U);
            if (ImGui::Combo("##language", &language, Text("자동 · Windows 언어\0한국어\0English\0简体中文\0", "Automatic · Windows language\0Korean\0English\0Simplified Chinese\0", "自动 · Windows 语言\0韩语\0English\0简体中文\0"))) {
                settings.language = static_cast<bcn::UiLanguage>(language);
                settingsChanged = true;
            }
            settingsChanged |= ImGui::Checkbox(Text("성능 모드", "Performance mode", "性能模式"), &settings.performanceMode);
            TextDisabledWrapped(Text(
                "자동 NPC 작업은 설정과 관계없이 중복을 합쳐 한 액터씩 안전하게 처리합니다. 켜면 액터 사이에 처리 간격을 한 번 더 두며, 새로 나타난 NPC는 세이브 로드 대량 작업보다 항상 우선합니다. 직접 선택은 즉시 처리되고 최종 결과는 같습니다.",
                "Automatic NPC work is always coalesced and safely processed one actor at a time. Enabling this adds one more scheduling interval between actors; newly visible NPCs always take priority over bulk save-load work. Direct selections stay immediate and final results are identical.",
                "无论此设置如何，自动 NPC 任务都会合并重复事件并逐个安全处理。启用后会在角色之间额外增加一次调度间隔；新出现的 NPC 始终优先于读档批量任务。直接选择仍会立即处理，最终结果相同。"));
            if (settingsChanged) {
                bcn::Settings::Get().Update(settings);
            }
            if (ImGui::Button(Text("선택 액터 바디 모프 초기화", "Reset selected actor body morphs", "重置所选角色身体形态"))) {
                auto* actor = SelectedActor();
                [[maybe_unused]] const auto removedManualLock = bcn::Distribution::Get().RemoveManualBodyAssignment(actor);
                bcn::racemenu::QueueClearBodyChangeMorphs(actor);
                bcn::ui::Notify(Text("선택 액터의 Body Change NG 모프와 직접 선택을 초기화했습니다.", "Cleared Body Change NG morphs and the direct selection on the selected actor.", "已清除所选角色的 Body Change NG 形态及直接选择。"));
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("전체 배포 바디 결과 초기화", "Reset all distributed body results", "重置全部已分发身体结果"))) {
                bcn::Distribution::Get().ClearManualBodyAssignments();
                const auto started = bcn::racemenu::QueueClearAllBodyChangeMorphs();
                bcn::ui::Notify(started ?
                    Text("저장에 남아 있는 모든 Body Change NG 바디 모프와 NPC 직접 선택 값의 초기화를 시작했습니다.", "Started clearing all saved Body Change NG body morphs and direct NPC selections.", "已开始清除存档中全部 Body Change NG 身体形态及 NPC 直接选择。") :
                    Text("전체 바디 결과 초기화를 시작하지 못했습니다.", "Could not start the full body reset.", "无法开始完整身体重置。"));
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("닫기", "Close", "关闭"))) g_showSettings = false;
            ImGui::EndPopup();
        }
        if (!g_showSettings) {
            bcn::InputSink::Get().CancelHotkeyCapture();
            if (!bcn::Settings::Get().Save()) {
                bcn::ui::Notify(Text("설정 변경을 저장하지 못했습니다.", "Could not save the setting changes.", "无法保存设置更改。"));
            }
        }
    }
}

namespace bcn::ui
{
    const char* Localize(const char* korean, const char* english, const char* chineseSimplified)
    {
        return Text(korean, english, chineseSimplified);
    }

    void Initialize()
    {
        PresetCatalog::Get().Refresh();
        SkinProfiles::Get().Refresh();
        player_tint::Catalog::Get().Refresh();
        ActorCatalog::Get().Refresh();
        native_ui::Register(&Draw);
    }

    void OnOpened()
    {
        // The actor list is deliberately refreshed only once at menu open.
        // A previous menu session must not retain an NPC camera target. Start
        // from Player every time. Nearby actors are still rebuilt at open and
        // by the explicit refresh button, but never replace that initial row.
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            g_selectedActorFormID = player->GetFormID();
        } else {
            g_selectedActorFormID = 0;
        }
        g_actorSearch.clear();
        g_currentTintPack = player_tint::CurrentPack().value_or(std::string{});
        g_selectedTintPack = g_currentTintPack;
        g_selectedTintAssetID.clear();
        g_showTintDetails = false;
        ResetCatalogNavigation();
        g_initializeActorSelection = true;
        g_activeTab = ActiveTab::body;
    }

    void OnClosed()
    {
        // Closing confirms the last live selections for the exact actor that
        // owns them. Commit before removing the BodyMorph preview layer.
        CommitPendingSelections();
        g_pendingBody.reset();
        g_pendingSkin.reset();
        g_pendingTint.reset();
        g_showTintDetails = false;
        racemenu::QueueCancelPreview();
        InputSink::Get().ResetTransientState();
        [[maybe_unused]] const auto settingsSaved = Settings::Get().Save();
        bcn::menu_character::Presentation::Get().Restore();
    }

    void Notify(std::string message)
    {
        std::scoped_lock lock(g_notificationLock);
        g_notification = std::move(message);
        g_notificationUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(3500);
    }

    void PrepareMainWindowPlacement()
    {
        const auto settings = bcn::Settings::Get().Snapshot();
        if (settings.mainWindowPositionSet) {
            ImGui::SetNextWindowPos(ImVec2(settings.mainWindowPositionX, settings.mainWindowPositionY), ImGuiCond_Appearing);
            return;
        }
        if (const auto* viewport = ImGui::GetMainViewport()) {
            // Leave the left half of the viewport available for the default
            // left-side character presentation.  Anchor the window's left
            // edge at the work-area center while keeping it vertically
            // centered; ImGui still clamps the window on smaller displays.
            ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.0F, 0.5F));
        }
    }

    void SaveMainWindowPosition()
    {
        const auto position = ImGui::GetWindowPos();
        auto settings = bcn::Settings::Get().Snapshot();
        const auto changed = !settings.mainWindowPositionSet ||
            std::abs(settings.mainWindowPositionX - position.x) > kWindowPositionTolerance ||
            std::abs(settings.mainWindowPositionY - position.y) > kWindowPositionTolerance;
        if (!changed || ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
        settings.mainWindowPositionSet = true;
        settings.mainWindowPositionX = position.x;
        settings.mainWindowPositionY = position.y;
        bcn::Settings::Get().Update(settings);
        if (!bcn::Settings::Get().Save()) {
            bcn::ui::Notify(Text("창 위치를 저장하지 못했습니다.", "Could not save the window position.", "无法保存窗口位置。"));
        }
    }

    void Draw()
    {
        const auto defaultWindowSize = DefaultWindowSize(kDefaultWindowWidth, kDefaultWindowHeight);
        ImGui::SetNextWindowSize(defaultWindowSize, ImGuiCond_FirstUseEver);
        // The top command row is intentionally one line. Prevent narrowing the
        // picker far enough to clip the Mod settings button or require
        // horizontal scrolling.
        ImGui::SetNextWindowSizeConstraints(
            // Keep the requested 0.8:1 baseline even when an older ImGui ini
            // entry remembers the former short window. Both values already
            // include 4K scaling and viewport clamping.
            defaultWindowSize,
            ImVec2(FLT_MAX, FLT_MAX));
        PrepareMainWindowPlacement();
        bool open = true;
        if (!ImGui::Begin("Body Change NG", &open,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs)) {
            ImGui::End();
            return;
        }
        SaveMainWindowPosition();
        if (!open) native_ui::Close();
        // Let the active popup consume Escape first.  Checking the popup state
        // before EscapePressed() is important because the event is a one-shot
        // latch shared by every Body Change NG window in this frame.
        if (!g_showDistribution && !g_showOutfit && !g_showSettings && !g_showTintDetails && EscapePressed()) {
            native_ui::Close();
            ImGui::End();
            return;
        }

        auto actors = ActorCatalog::Get().Snapshot();
        const auto runtimeSettings = bcn::Settings::Get().Snapshot();
        if (g_initializeActorSelection || actors.empty()) {
            ActorCatalog::Get().Refresh(false);
            actors = ActorCatalog::Get().Snapshot();
            if (!actors.empty() && std::ranges::find(actors, g_selectedActorFormID, &ActorEntry::formID) == actors.end() &&
                !ActorCatalog::Get().Resolve(g_selectedActorFormID)) {
                // Keep an explicitly entered persistent FormID even when the
                // actor is outside the currently loaded actor list. Only a
                // genuinely expired reference falls back to Player.
                g_selectedActorFormID = actors.front().formID;
            }
            g_initializeActorSelection = false;
        }
        if (g_selectedActorFormID == 0 && !actors.empty()) g_selectedActorFormID = actors.front().formID;
        if (!SelectedActor() && !actors.empty()) {
            // Runtime FF references can disappear after the catalog snapshot
            // was built.  Do not keep their stale label while the camera
            // presentation falls back to another actor.
            SelectActor(actors.front().formID);
        }
        const auto selected = std::ranges::find(actors, g_selectedActorFormID, &ActorEntry::formID);
        const auto selectedName = selected != actors.end() ? ActorLabel(*selected) : ActorLabel(SelectedActor());
        const auto* refreshActorsLabel = Text("주변 액터 새로고침", "Refresh nearby actors", "刷新附近角色");
        const auto* distributionLabel = Text("NPC 배포", "NPC distribution", "NPC 分发");
        const auto* outfitLabel = Text("의상·랜덤화", "Outfit · randomization", "服装·随机化");
        const auto* settingsLabel = Text("모드 설정", "Mod settings", "模组设置");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
            ImVec2(Scaled(4.0F), ImGui::GetStyle().ItemSpacing.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
            ImVec2(Scaled(6.0F), ImGui::GetStyle().FramePadding.y));
        const auto buttonWidth = [](const char* label) {
            return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0F;
        };
        const auto reservedWidth = buttonWidth(refreshActorsLabel) + buttonWidth(distributionLabel) +
            buttonWidth(outfitLabel) + buttonWidth(settingsLabel) + ImGui::GetStyle().ItemSpacing.x * 4.0F;
        const auto actorWidth = (std::max)(Scaled(150.0F), ImGui::GetContentRegionAvail().x - reservedWidth);
        ImGui::SetNextItemWidth(actorWidth);
        PrepareResizableDropdown(actors.size() + 2U);
        if (ImGui::BeginCombo("##actor", selectedName.c_str())) {
            // Opening the actor combo must not immediately enter typing mode.
            // Give the popup a tiny non-text default navigation item; the
            // search field receives text ownership only after an explicit
            // mouse/keyboard activation.
            ImGui::Selectable("##actorComboFocusGuard", false,
                ImGuiSelectableFlags_NoAutoClosePopups, ImVec2(0.0F, 1.0F));
            ImGui::SetItemDefaultFocus();
            ImGui::SetNextItemWidth(-FLT_MIN);
            const auto exactActorRequested = ImGui::InputTextWithHint("##actorSearch",
                Text("이름 또는 FormID 입력", "Type a name or FormID", "输入名称或 FormID"), &g_actorSearch,
                ImGuiInputTextFlags_EnterReturnsTrue);
            if (exactActorRequested) {
                if (const auto formID = ExactActorFormID(g_actorSearch)) {
                    if (auto* exactActor = ActorCatalog::Get().Resolve(*formID)) {
                        SelectActor(exactActor->GetFormID());
                        ImGui::CloseCurrentPopup();
                        bcn::ui::Notify(exactActor->Is3DLoaded() ?
                            Text("FormID 액터를 선택했습니다.", "Selected the FormID actor.", "已选择该 FormID 角色。") :
                            Text("액터를 선택했습니다. 3D가 로드되면 목록 선택을 즉시 적용할 수 있습니다.", "Selected the actor. List selections can be applied once its 3D is loaded.", "已选择角色。其 3D 加载后即可应用列表选择。"));
                    } else {
                        bcn::ui::Notify(Text("해당 FormID의 액터를 찾지 못했습니다.", "No actor was found for that FormID.", "未找到该 FormID 对应的角色。"));
                    }
                }
            }
            ImGui::Separator();
            for (const auto& entry : actors) {
                if (!ActorMatchesSearch(entry)) continue;
                const auto label = ActorLabel(entry);
                ImGui::PushID(static_cast<int>(entry.formID));
                if (ImGui::Selectable(label.c_str(), entry.formID == g_selectedActorFormID)) {
                    SelectActor(entry.formID);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button(refreshActorsLabel)) {
            ActorCatalog::Get().Refresh(false);
            actors = ActorCatalog::Get().Snapshot();
            if (std::ranges::find(actors, g_selectedActorFormID, &ActorEntry::formID) == actors.end() && !actors.empty() &&
                !ActorCatalog::Get().Resolve(g_selectedActorFormID)) {
                SelectActor(actors.front().formID);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(distributionLabel)) g_showDistribution = true;
        ImGui::SameLine();
        if (ImGui::Button(outfitLabel)) g_showOutfit = true;
        ImGui::SameLine();
        if (ImGui::Button(settingsLabel)) g_showSettings = true;
        ImGui::PopStyleVar(2);

        bcn::menu_character::Presentation::Get().Apply(runtimeSettings.characterPosition, SelectedActor());

        ImGui::Separator();
        const auto* selectedActor = SelectedActor();
        const auto* player = RE::PlayerCharacter::GetSingleton();
        const auto playerSelected = selectedActor && player &&
            selectedActor->GetFormID() == player->GetFormID();
        if (!playerSelected && g_activeTab == ActiveTab::tint) g_activeTab = ActiveTab::body;
        HandleTabNavigation(playerSelected);
        if (TabButton(Text("바디", "Body", "身体"), g_activeTab == ActiveTab::body)) g_activeTab = ActiveTab::body;
        ImGui::SameLine();
        if (TabButton(Text("스킨", "Skin", "皮肤"), g_activeTab == ActiveTab::skin)) {
            g_activeTab = ActiveTab::skin;
        }
        if (playerSelected) {
            ImGui::SameLine();
            if (TabButton(Text("틴트", "Tint", "色调"), g_activeTab == ActiveTab::tint)) {
                g_activeTab = ActiveTab::tint;
            }
        }
        ImGui::SameLine();
        const auto* favoritesLabel = Text("즐겨찾기", "Favorites", "收藏");
        const auto favoritesControlWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
            ImGui::CalcTextSize(favoritesLabel).x;
        const auto searchWidth = (std::max)(1.0F,
            ImGui::GetContentRegionAvail().x - favoritesControlWidth - ImGui::GetStyle().ItemSpacing.x);
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##search", Text("이름 검색", "Search", "搜索名称"), &g_search);
        ImGui::SameLine();
        ImGui::Checkbox(favoritesLabel, &FavoritesOnly());
        if (g_activeTab == ActiveTab::body) {
            ImGui::TextDisabled("%s", racemenu::IsReady() ?
                Text("선택 액터에게 RaceMenu BodyMorph로 즉시 적용", "Applies immediately to the selected actor through RaceMenu BodyMorph", "通过 RaceMenu BodyMorph 立即应用于所选角色") :
                Text("RaceMenu BodyMorph 인터페이스를 기다리는 중", "Waiting for RaceMenu's BodyMorph interface", "正在等待 RaceMenu 的 BodyMorph 接口"));
        } else if (g_activeTab == ActiveTab::skin) {
            ImGui::TextDisabled("%s", Text("선택 액터에게 RaceMenu NiOverride로 즉시 적용", "Applies immediately to the selected actor through RaceMenu NiOverride", "通过 RaceMenu NiOverride 立即应用于所选角色"));
        } else {
            ImGui::TextDisabled("%s", Text("플레이어의 현재 RaceMenu 틴트 레이어에만 적용", "Applies only to the player's current RaceMenu tint layers", "仅应用于玩家当前的 RaceMenu 色调图层"));
        }
        if (const auto* actor = SelectedActor(); actor && actor != RE::PlayerCharacter::GetSingleton() &&
            bcn::Distribution::Get().HasManualAssignment(actor)) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(.48F, .82F, .96F, 1.0F), "%s", Text(
                "직접 선택 유지 · 자동 배포 제외", "Direct selection kept · Excluded from auto distribution", "保留直接选择 · 不参与自动分发"));
        }

        std::string notification;
        {
            std::scoped_lock lock(g_notificationLock);
            if (std::chrono::steady_clock::now() < g_notificationUntil) notification = g_notification;
        }
        if (!notification.empty()) {
            ImGui::TextColored(ImVec4(.48F, .82F, .96F, 1.0F), "%s", notification.c_str());
        }

        if (g_activeTab == ActiveTab::body) {
            const auto bodyRefreshLabel = std::string{ Text("새로고침", "Refresh", "刷新") } + "##bodyCatalogRefresh";
            if (ImGui::Button(bodyRefreshLabel.c_str())) PresetCatalog::Get().Refresh();
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Text(
                "CalienteTools\\BodySlide\\SliderPresets의 XML 프리셋을 읽습니다.",
                "Reads XML presets from CalienteTools\\BodySlide\\SliderPresets.",
                "读取 CalienteTools\\BodySlide\\SliderPresets 中的 XML 预设。"));
            auto items = BodyItems();
            DrawCatalog(items, true);
        } else if (g_activeTab == ActiveTab::skin) {
            DrawSkinCatalog();
        } else {
            DrawPlayerTintCatalog();
        }

        DrawDistributionPopup();
        DrawOutfitPopup();
        DrawSettingsPopup();
        DrawTintDetailPopup();
        // Tint and its detailed value popup deliberately share one face view;
        // leaving Tint restores the normal left/right presentation.
        bcn::menu_character::Presentation::Get().SetTintFocus(g_activeTab == ActiveTab::tint);
        bcn::menu_character::Presentation::Get().UpdateRotationInteraction();
        ImGui::End();
    }
}
