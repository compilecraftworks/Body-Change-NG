#include "BodyChangeNG/UI.h"
#include "BodyChangeNG/AppearanceColorDrafts.h"
#include "BodyChangeNG/CatalogRefreshQueue.h"
#include "BodyChangeNG/UiText.h"
#include "BodyChangeNG/PopupPlacementUI.h"
#include "BodyChangeNG/FittedTextUI.h"
#include "BodyChangeNG/FrameTasks.h"
#include "BodyChangeNG/FutanariSupport.h"

#include "BodyChangeNG/ActorCatalog.h"
#include "BodyChangeNG/ActorSettingsReset.h"
#include "BodyChangeNG/BodyFamily.h"
#include "BodyChangeNG/BodyMorphPolicies.h"
#include "BodyChangeNG/Distribution.h"
#include "BodyChangeNG/DistributionRuleNames.h"
#include "BodyChangeNG/InputSink.h"
#include "BodyChangeNG/MenuCharacterPresentation.h"
#include "BodyChangeNG/NativeImGuiHost.h"
#include "BodyChangeNG/OutfitRefit.h"
#include "BodyChangeNG/PlayerTint.h"
#include "BodyChangeNG/PresetCatalog.h"
#include "BodyChangeNG/RaceMenuBodyMorph.h"
#include "BodyChangeNG/RaceMenuOverlay.h"
#include "BodyChangeNG/OverlayColor.h"
#include "BodyChangeNG/Settings.h"
#include "BodyChangeNG/SkinApplication.h"
#include "BodyChangeNG/SkinProfiles.h"
#include "BodyChangeNG/UiCatalogPolicy.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <RE/T/TESClass.h>

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
    std::atomic_uint64_t g_uiSessionEpoch{};
    std::mutex g_uiLifecycleLock;
    using ActiveTab = bcn::ui_catalog::Tab;

    enum class DistributionPool
    {
        body,
        skin,
        futanari,
        overlay
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

    struct DistributionTargetOption
    {
        std::string display;
        std::string editorID;
        std::string plugin;
        std::uint32_t localFormID{};
        std::uint32_t runtimeFormID{};
    };

    ActiveTab g_activeTab{ ActiveTab::body };
    DistributionPool g_distributionPool{ DistributionPool::body };
    std::array<bool, static_cast<std::size_t>(ActiveTab::count)> g_favoritesOnlyByTab{};
    bool g_showDistribution{};
    bool g_distributionSelectionMode{};
    std::unordered_set<std::string> g_distributionSelectedIds;
    std::array<std::unordered_set<std::string>,
        bcn::overlay::Index(bcn::overlay::Area::count)> g_distributionSelectedOverlayIds;
    bool g_showOutfit{};
    bool g_orefitRulesRegistered{};
    bool g_showSettings{};
    bool g_showTintDetails{};
    bool g_initializeActorSelection{ true };
    bool g_distributionEditorLoaded{};
    std::vector<bcn::DistributionRule> g_distributionRules;
    std::size_t g_selectedDistributionRule{};
    std::uint32_t g_nextDraftRuleID{ 1U };
    std::optional<bcn::UiLanguage> g_distributionRuleNameLanguage;
    std::vector<DistributionTargetOption> g_distributionFactionOptions;
    std::vector<std::string> g_distributionPluginOptions;
    std::vector<DistributionTargetOption> g_distributionRaceOptions;
    std::vector<DistributionTargetOption> g_distributionKeywordOptions;
    std::vector<DistributionTargetOption> g_distributionClassOptions;
    std::uint32_t g_selectedActorFormID{};
    std::string g_actorSearch;
    std::string g_search;
    bcn::player_tint::Layer g_selectedTintLayer{ bcn::player_tint::Layer::lips };
    std::string g_selectedTintPack;
    std::string g_currentTintPack;
    std::string g_selectedTintAssetID;
    std::array<float, 4> g_tintColor{ 1.0F, 1.0F, 1.0F, 1.0F };
    std::array<float, 4> g_overlayColor{ 1.0F, 1.0F, 1.0F, 1.0F };
    bool g_showOverlayDetails{};
    std::uint32_t g_overlayColorActor{};
    bcn::overlay::Area g_overlayColorArea{ bcn::overlay::Area::body };
    std::string g_overlayColorEntryId;
    bool g_overlayColorDistributionDraft{};
    std::chrono::steady_clock::time_point g_lastOverlayColorApply{};
    std::chrono::steady_clock::time_point g_lastTintDetailApply{};
    bcn::ui::AppearanceColorDrafts<std::uint32_t> g_overlayColorDrafts;
    bcn::ui::AppearanceColorDrafts<bcn::player_tint::PersistedLayerState> g_tintColorDrafts;
    std::array<std::optional<bcn::player_tint::Color>,
        static_cast<std::size_t>(bcn::player_tint::Layer::dirt) + 1U> g_tintSessionColors;
    std::optional<PendingChoice> g_pendingBody;
    std::optional<PendingChoice> g_pendingSkin;
    std::optional<PendingChoice> g_pendingFutanari;
    std::optional<PendingChoice> g_pendingTint;
    std::optional<bcn::player_tint::PersistedState> g_pendingTintBaseline;
    std::array<std::optional<PendingChoice>, bcn::overlay::Index(bcn::overlay::Area::count)>
        g_pendingOverlays;
    std::optional<bcn::overlay::Area> g_overlayArea;
    std::array<std::string, bcn::overlay::Index(bcn::overlay::Area::count)>
        g_overlayFocusedIds;
    std::array<bool, bcn::overlay::Index(bcn::overlay::Area::count)> g_overlaySectionsOpen{
        true, false, false, false
    };
    std::array<CatalogNavigationState, static_cast<std::size_t>(ActiveTab::count)>
        g_catalogNavigation{};
    std::string g_notification;
    std::chrono::steady_clock::time_point g_notificationUntil{};
    std::mutex g_notificationLock;
    // Preserve the requested picker proportions. Resolution and text scaling
    // multiply both axes equally, including the 4K auto scale.
    constexpr auto kDefaultWindowWidth = 700.0F;
    constexpr auto kDefaultWindowHeight = 875.0F;
    constexpr auto kWindowPositionTolerance = 0.5F;
    constexpr ImU32 kCardNormal = IM_COL32(29, 29, 29, 255);
    constexpr ImU32 kCardHovered = IM_COL32(42, 63, 77, 255);
    constexpr ImU32 kCardSelected = IM_COL32(48, 103, 129, 255);
    constexpr ImU32 kCardText = IM_COL32(238, 238, 238, 255);
    constexpr ImU32 kCardSubtext = IM_COL32(170, 170, 170, 255);
    constexpr ImU32 kCardIncompatible = IM_COL32(192, 145, 120, 255);

    [[nodiscard]] const char* Text(const char* korean, const char* english, const char* chinese);
    template<class Catalog>
    void RefreshFileCatalog(Catalog& catalog)
    {
        [[maybe_unused]] const auto queued = bcn::catalog_refresh::Get().Submit(
            &catalog, [&catalog] { catalog.Refresh(); }, [](std::exception_ptr error) {
                try { std::rethrow_exception(error); }
                catch (const std::exception& exception) {
                    SKSE::log::error("BCNG catalog refresh failed: {}", exception.what());
                } catch (...) {
                    SKSE::log::error("BCNG catalog refresh failed with an unknown exception");
                }
                bcn::ui::Notify(Text("목록 새로고침에 실패했습니다.", "Catalog refresh failed.", "列表刷新失败。"));
            });
    }

    [[nodiscard]] bcn::UiLanguage WindowsLanguage()
    {
        const auto language = PRIMARYLANGID(GetUserDefaultUILanguage());
        if (language == LANG_KOREAN) return bcn::UiLanguage::korean;
        if (language == LANG_CHINESE) return bcn::UiLanguage::chineseSimplified;
        return bcn::UiLanguage::english;
    }

    [[nodiscard]] bcn::UiLanguage CurrentLanguage()
    {
        const auto configured = bcn::Settings::Get().Language();
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
        const auto configured = bcn::Settings::Get().TextScale();
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

    void PrepareDownwardResizableDropdown(const std::size_t itemCount)
    {
        const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
        const auto padding = ImGui::GetStyle().WindowPadding.y * 2.0F;
        const auto desiredRows = (std::max)(std::size_t{ 1 },
            (std::min)(itemCount, std::size_t{ 14 }));
        auto maximumHeight = padding + rowHeight * static_cast<float>(desiredRows);
        if (const auto* viewport = ImGui::GetMainViewport()) {
            const auto fieldBottom = ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight();
            const auto workBottom = viewport->WorkPos.y + viewport->WorkSize.y;
            const auto remaining = (std::max)(rowHeight + padding, workBottom - fieldBottom);
            maximumHeight = (std::min)({ maximumHeight, Scaled(640.0F), remaining });
        }
        const auto minimumHeight = (std::min)(maximumHeight, padding + rowHeight);
        const auto popupWidth = ImGui::CalcItemWidth();
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(popupWidth, minimumHeight), ImVec2(popupWidth, maximumHeight));
        ImGui::SetNextWindowSize(ImVec2(popupWidth, maximumHeight), ImGuiCond_Appearing);
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
            g_showOverlayDetails || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
    }

    [[nodiscard]] bool NavigationKeyPressed(const ImGuiKey first, const ImGuiKey second,
        const ImGuiKey gamepad, const bool repeat)
    {
        if (bcn::native_ui::MenuActionHeld() || bcn::native_ui::ActivatePressed() ||
            bcn::native_ui::CancelPressed()) return false;
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
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) ||
            bcn::native_ui::ActivatePressed();
        if (bcn::native_ui::CancelPressed()) command.confirm = false;
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

    [[nodiscard]] std::string EllipsizeText(
        const std::string_view value, const float maximumWidth)
    {
        return bcn::ui_text::Ellipsize(value, maximumWidth, [](std::string_view text) {
            return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        });
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
                kCardHovered, Scaled(4.0F));
        }
        ImGui::GetWindowDrawList()->AddText(font, fontSize,
            ImVec2(cursor.x + (width - textSize.x) * 0.5F, textY),
            favorite ? IM_COL32(255, 190, 72, 255) : IM_COL32(182, 182, 182, 255), glyph);
        return ImGui::IsItemClicked();
    }

    [[nodiscard]] bool CenteredCheckbox(const char* id, bool& value,
        const ImVec2 rowCursor, const float rowHeight)
    {
        const auto checkboxY = rowCursor.y +
            (std::max)(0.0F, (rowHeight - ImGui::GetFrameHeight()) * 0.5F);
        ImGui::SetCursorScreenPos(ImVec2(rowCursor.x, checkboxY));
        const auto changed = ImGui::Checkbox(id, &value);
        const auto cardX = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorScreenPos(ImVec2(cardX, rowCursor.y));
        return changed;
    }

    [[nodiscard]] bool RightAlignedButton(const char* label)
    {
        const auto width = ImGui::CalcTextSize(label).x +
            ImGui::GetStyle().FramePadding.x * 2.0F;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(),
            ImGui::GetWindowContentRegionMax().x - width));
        return ImGui::Button(label);
    }

    void DrawTitleBarRotationHint()
    {
        const auto* hint = Text(
            "캐릭터 회전: 마우스 우클릭 드래그 / LT+RS 좌우",
            "Rotate: right-mouse drag / LT+RS left/right",
            "角色旋转：鼠标右键拖动 / LT+RS 左右");
        bcn::ui_text::TitleBarRightHint("Body Change NG", hint);
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
            const auto mappedCancel = bcn::native_ui::CancelPressed();
            // Consume Escape while typing, but never turn that same key-up into
            // a delayed window close after the text field releases focus.
            available = !bcn::InputSink::Get().IsCapturingHotkey() &&
                (mappedCancel || (!ImGui::GetIO().WantTextInput && (directInput || scaleform)));
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
        // Keep every catalog rectangle identical. Tint and Overlay use the
        // reserved row for fixed value/color controls; the other tabs retain
        // the same lower edge so switching tabs never changes list geometry.
        const auto footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        return (std::max)(Scaled(120.0F), ImGui::GetContentRegionAvail().y - footer);
    }

    // Keep popup input modal, but do not wash the running game or the main
    // picker with ImGui's modal dim overlay. The popup itself remains opaque
    // and still blocks accidental clicks behind it.
    [[nodiscard]] bool BeginUndimmedPopupModal(const char* title, bool* open, const ImGuiWindowFlags flags,
        const bcn::popup_placement::Kind placement)
    {
        auto& settings = bcn::Settings::Get();
        static bcn::popup_placement::Modals modals;
        const auto result = modals.Begin(title, open, flags, placement, settings.PopupPosition(placement));
        const auto position = result.settledPosition;
        if (position.set && settings.RememberPopupPosition(placement, position.x, position.y) && !settings.Save()) {
            bcn::ui::Notify(Text("창 위치를 저장하지 못했습니다.", "Could not save the window position.", "无法保存窗口位置。"));
        }
        return result.began;
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

    [[nodiscard]] bool IsDistributionSelectionFor(DistributionPool pool) noexcept;

    [[nodiscard]] std::vector<CatalogItem> BodyItems()
    {
        const auto presets = bcn::PresetCatalog::Get().Snapshot();
        const auto actor = bcn::ActorCatalog::Get().Resolve(g_selectedActorFormID);
        const auto actorBase = actor ? actor->GetActorBase() : nullptr;
        const auto selectedMale = actorBase && actorBase->GetSex() == RE::SEX::kMale;
        const auto settings = bcn::Settings::Get().Snapshot();
        const auto actorFamily = bcn::body_family::ResolveActor(actor);
        const auto currentPreset = bcn::racemenu::CurrentPresetId(actor);
        std::vector<CatalogItem> items;
        items.reserve(presets.size());
        for (const auto& preset : presets) {
            if (preset.male != selectedMale) continue;
            const auto presetMask = bcn::body_family::PresetMask(preset.family, preset.male);
            if (IsDistributionSelectionFor(DistributionPool::body)) {
                if ((presetMask & bcn::body_family::Bit(bcn::body_family::Family::ube)) != 0U) continue;
            } else if (!bcn::body_family::Matches(presetMask, actorFamily)) {
                continue;
            }
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

    void RollbackPendingSelections(RE::Actor* actor);

    void SelectActor(const RE::FormID formID)
    {
        if (formID == 0) return;
        if (g_selectedActorFormID != formID) {
            RollbackPendingSelections(SelectedActor());
            bcn::menu_character::Presentation::Get().Restore();
            g_selectedActorFormID = formID;
            bcn::skin_application::InvalidateFutanariDetection(formID);
            ResetCatalogNavigation();
        }
        g_actorSearch.clear();
        const auto settings = bcn::Settings::Get().Snapshot();
        bcn::menu_character::Presentation::Get().Apply(settings.characterPosition, SelectedActor());
        if (g_activeTab == ActiveTab::overlay) {
            [[maybe_unused]] const auto requested = bcn::overlay::RequestCatalog(SelectedActor());
        }
    }

    void ResetDistributionEditor()
    {
        g_distributionEditorLoaded = false;
        g_distributionRules.clear();
        g_selectedDistributionRule = 0;
        g_distributionPool = DistributionPool::body;
        g_distributionFactionOptions.clear();
        g_distributionPluginOptions.clear();
        g_distributionRaceOptions.clear();
        g_distributionKeywordOptions.clear();
        g_distributionClassOptions.clear();
    }

    void ClearDistributionCatalogSelection()
    {
        g_distributionSelectedIds.clear();
        for (auto& ids : g_distributionSelectedOverlayIds) ids.clear();
    }

    [[nodiscard]] bool IsDistributionSelectionFor(const DistributionPool pool) noexcept
    {
        return g_distributionSelectionMode && g_distributionPool == pool;
    }

    [[nodiscard]] std::size_t DistributionSelectionCount() noexcept
    {
        if (g_distributionPool != DistributionPool::overlay) {
            return g_distributionSelectedIds.size();
        }
        std::size_t count{};
        for (const auto& ids : g_distributionSelectedOverlayIds) count += ids.size();
        return count;
    }

    void BeginDistributionCatalogSelection(const DistributionPool pool)
    {
        g_distributionPool = pool;
        g_distributionSelectionMode = true;
        ClearDistributionCatalogSelection();
        ResetCatalogNavigation();
    }

    void CancelDistributionCatalogSelection()
    {
        g_distributionSelectionMode = false;
        ClearDistributionCatalogSelection();
        ResetCatalogNavigation();
    }

    [[nodiscard]] bool DistributionItemSelected(const std::string_view id)
    {
        return g_distributionSelectedIds.contains(std::string{ id });
    }

    void SetDistributionItemSelected(const std::string& id, const bool selected)
    {
        if (selected) g_distributionSelectedIds.insert(id);
        else g_distributionSelectedIds.erase(id);
    }

    [[nodiscard]] bool DistributionOverlaySelected(
        const bcn::overlay::Area area, const std::string_view id)
    {
        return g_distributionSelectedOverlayIds[bcn::overlay::Index(area)].contains(
            std::string{ id });
    }

    void SetDistributionOverlaySelected(const bcn::overlay::Area area,
        const std::string& id, const bool selected)
    {
        auto& ids = g_distributionSelectedOverlayIds[bcn::overlay::Index(area)];
        if (selected) ids.insert(id);
        else ids.erase(id);
    }

    void AddUniqueTargetOption(std::vector<std::string>& options,
        std::unordered_set<std::string>& known, const std::string_view value)
    {
        if (value.empty()) return;
        auto key = Lower(value);
        if (known.insert(key).second) options.emplace_back(value);
    }

    [[nodiscard]] std::uint32_t LocalFormID(const RE::TESForm* form, const RE::TESFile* file)
    {
        return form && file ? form->GetFormID() & (file->IsLight() ? 0xFFFU : 0xFFFFFFU) : 0U;
    }

    void AddFormTargetOption(std::vector<DistributionTargetOption>& options,
        std::unordered_set<std::string>& known, RE::TESForm* form)
    {
        const auto* file = form ? form->GetFile(0) : nullptr;
        if (!form || !file || file->GetFilename().empty()) return;
        const auto localFormID = LocalFormID(form, file);
        if (localFormID == 0U) return;
        const auto plugin = std::string{ file->GetFilename() };
        const auto identity = Lower(plugin) + ":" + std::format("{:06X}", localFormID);
        if (!known.insert(identity).second) return;

        const auto* rawName = form->GetName();
        const auto* rawEditorID = form->GetFormEditorID();
        const auto name = rawName && rawName[0] != '\0' ? std::string{ rawName } : std::string{};
        const auto editorID = rawEditorID && rawEditorID[0] != '\0' ? std::string{ rawEditorID } : std::string{};
        std::string display;
        if (!name.empty()) display = name;
        if (!editorID.empty() && Lower(name) != Lower(editorID)) {
            if (!display.empty()) display += " · ";
            display += editorID;
        }
        if (display.empty()) display = Text("이름 없음", "Unnamed", "未命名");
        display += std::format(" · {}:{:06X}", plugin, localFormID);
        options.push_back({
            .display = std::move(display),
            .editorID = editorID,
            .plugin = plugin,
            .localFormID = localFormID,
            .runtimeFormID = form->GetFormID()
        });
    }

    void RefreshDistributionTargetOptions()
    {
        g_distributionFactionOptions.clear();
        g_distributionPluginOptions.clear();
        g_distributionRaceOptions.clear();
        g_distributionKeywordOptions.clear();
        g_distributionClassOptions.clear();
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) return;

        std::unordered_set<std::string> knownFactions;
        for (auto* faction : dataHandler->GetFormArray<RE::TESFaction>()) {
            AddFormTargetOption(g_distributionFactionOptions, knownFactions, faction);
        }
        std::unordered_set<std::string> knownRaces;
        for (auto* race : dataHandler->GetFormArray<RE::TESRace>()) {
            AddFormTargetOption(g_distributionRaceOptions, knownRaces, race);
        }
        std::unordered_set<std::string> knownKeywords;
        for (auto* keyword : dataHandler->GetFormArray<RE::BGSKeyword>()) {
            AddFormTargetOption(g_distributionKeywordOptions, knownKeywords, keyword);
        }
        std::unordered_set<std::string> knownClasses;
        for (auto* npcClass : dataHandler->GetFormArray<RE::TESClass>()) {
            AddFormTargetOption(g_distributionClassOptions, knownClasses, npcClass);
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

        const auto sortFormOptions = [](auto& options) {
            std::ranges::sort(options, [](const auto& left, const auto& right) {
                return Lower(left.display) < Lower(right.display);
            });
        };
        sortFormOptions(g_distributionFactionOptions);
        std::ranges::sort(g_distributionPluginOptions, [](const auto& left, const auto& right) {
            return Lower(left) < Lower(right);
        });
        sortFormOptions(g_distributionRaceOptions);
        sortFormOptions(g_distributionKeywordOptions);
        sortFormOptions(g_distributionClassOptions);
    }

    void EnsureDistributionEditor()
    {
        if (g_distributionEditorLoaded) return;
        g_distributionRules = bcn::Distribution::Get().SavedRulesSnapshot();
        g_selectedDistributionRule = 0;
        RefreshDistributionTargetOptions();
        g_distributionEditorLoaded = true;
    }

    void SynchronizeDistributionRuleNames()
    {
        const auto language = CurrentLanguage();
        if (g_distributionRuleNameLanguage == language) return;
        for (auto& rule : g_distributionRules) {
            if (const auto localized = bcn::distribution_names::Localized(rule.nameKey, language);
                !localized.empty()) {
                rule.name = localized;
            }
        }
        g_distributionRuleNameLanguage = language;
    }

    [[nodiscard]] bcn::DistributionRule NewDistributionRule()
    {
        const auto actor = SelectedActor();
        const auto base = actor ? actor->GetActorBase() : nullptr;
        const auto female = !base || base->GetSex() == RE::SEX::kFemale;
        return {
            .id = bcn::GenerateUniqueUserRuleId(g_distributionRules, g_nextDraftRuleID),
            .name = female ? Text("새 여성 NPC 규칙", "New female NPC rule", "新的女性 NPC 规则") :
                Text("새 남성 NPC 규칙", "New male NPC rule", "新的男性 NPC 规则"),
            .nameKey = std::string{ bcn::distribution_names::NewRuleKey(female) },
            .female = female
        };
    }

    [[nodiscard]] const char* DistributionPoolLabel(const DistributionPool pool)
    {
        switch (pool) {
        case DistributionPool::body:
            return Text("바디프리셋", "Body Presets", "身体预设");
        case DistributionPool::skin:
            return Text("바디스킨", "Body Skins", "身体皮肤");
        case DistributionPool::futanari:
            return Text("후타스킨", "Futanari Skin", "扶她皮肤");
        default:
            return Text("오버레이", "Overlays", "叠加层");
        }
    }

    [[nodiscard]] bool RuleUsesDistributionPool(const bcn::DistributionRule& rule,
        const DistributionPool pool)
    {
        switch (pool) {
        case DistributionPool::body:
            return !rule.presetIds.empty();
        case DistributionPool::skin:
            return !rule.skinProfileIds.empty();
        case DistributionPool::futanari:
            return !rule.futanariSkinIds.empty();
        default:
            return std::ranges::any_of(rule.overlayIds,
                [](const auto& ids) { return !ids.empty(); });
        }
    }

    [[nodiscard]] bool RuleHasAnyDistributionPool(const bcn::DistributionRule& rule)
    {
        return !rule.presetIds.empty() || !rule.skinProfileIds.empty() ||
            !rule.futanariSkinIds.empty() ||
            std::ranges::any_of(rule.overlayIds,
                [](const auto& ids) { return !ids.empty(); });
    }

    [[nodiscard]] std::size_t RuleDistributionPoolCount(const bcn::DistributionRule& rule,
        const DistributionPool pool)
    {
        switch (pool) {
        case DistributionPool::body: return rule.presetIds.size();
        case DistributionPool::skin: return rule.skinProfileIds.size();
        case DistributionPool::futanari: return rule.futanariSkinIds.size();
        default: {
            std::size_t count{};
            for (const auto& ids : rule.overlayIds) count += ids.size();
            return count;
        }
        }
    }

    void SetRuleDistributionSelection(bcn::DistributionRule& rule)
    {
        switch (g_distributionPool) {
        case DistributionPool::body:
            rule.presetIds.assign(g_distributionSelectedIds.begin(),
                g_distributionSelectedIds.end());
            break;
        case DistributionPool::skin:
            rule.skinProfileIds.assign(g_distributionSelectedIds.begin(),
                g_distributionSelectedIds.end());
            break;
        case DistributionPool::futanari:
            rule.female = true;
            rule.name = Text("새 여성 후타 NPC 규칙", "New female futanari NPC rule",
                "新的女性扶她 NPC 规则");
            rule.nameKey.clear();
            rule.futanariSkinIds.assign(g_distributionSelectedIds.begin(),
                g_distributionSelectedIds.end());
            break;
        case DistributionPool::overlay:
            for (const auto area : bcn::overlay::kAreas) {
                const auto index = bcn::overlay::Index(area);
                rule.overlayIds[index].assign(g_distributionSelectedOverlayIds[index].begin(),
                    g_distributionSelectedOverlayIds[index].end());
                auto* actor = SelectedActor();
                rule.overlayColors[index] = g_overlayColorDrafts.CopySelection(g_selectedActorFormID,
                    static_cast<std::uint8_t>(area), rule.overlayIds[index],
                    [actor, area](const std::string& id) {
                        return bcn::overlay::CurrentColor(actor, area, id).value_or(0xFFFFFFFFU);
                    });
            }
            break;
        }
    }

    void OpenDistributionEditorFromCatalog()
    {
        if (DistributionSelectionCount() == 0U) {
            bcn::ui::Notify(Text("배포할 항목을 하나 이상 선택하세요.",
                "Select at least one item to distribute.", "请至少选择一个要分发的项目。"));
            return;
        }
        EnsureDistributionEditor();
        auto rule = NewDistributionRule();
        SetRuleDistributionSelection(rule);
        g_distributionRules.push_back(std::move(rule));
        g_selectedDistributionRule = g_distributionRules.size() - 1U;
        g_showDistribution = true;
        // The popup owns this catalog-selection snapshot until it closes.
        // Hiding the checkboxes must not erase the IDs that + Add rule copies.
        g_distributionSelectionMode = false;
        ResetCatalogNavigation();
    }

    template <class Refresh, class SelectAll>
    void DrawCatalogCommandRow(const DistributionPool pool, Refresh&& refresh,
        SelectAll&& selectAll, const char* help)
    {
        const auto selecting = IsDistributionSelectionFor(pool);
        if (ImGui::BeginTable("##catalogCommandRow", 2,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
            ImGui::TableSetupColumn("##catalogCommands", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("##catalogDistribution", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (selecting) {
                if (ImGui::Button(Text("전체 선택", "Select all", "全选"))) selectAll();
                ImGui::SameLine();
                if (ImGui::Button(Text("선택 해제", "Clear selection", "清除选择"))) {
                    ClearDistributionCatalogSelection();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s %zu", Text("선택", "Selected", "已选"),
                    DistributionSelectionCount());
            } else {
                if (ImGui::Button(Text("새로고침", "Refresh", "刷新"))) refresh();
                ImGui::SameLine();
                bcn::ui_text::FittedDisabledLine(help);
            }
            ImGui::TableSetColumnIndex(1);
            if (selecting) {
                if (ImGui::Button(Text("배포 NPC 조건", "Distribution NPC conditions", "分发 NPC 条件"))) {
                    OpenDistributionEditorFromCatalog();
                }
                ImGui::SameLine();
                if (ImGui::Button(Text("배포 취소", "Cancel distribution", "取消分发"))) {
                    CancelDistributionCatalogSelection();
                }
            } else if (ImGui::Button(Text("NPC 배포", "NPC distribution", "NPC 分发"))) {
                BeginDistributionCatalogSelection(pool);
            }
            ImGui::EndTable();
        }
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
            if (auto* race = base->GetRace()) {
                [[maybe_unused]] const auto normalized = bcn::SetDistributionRuleTargetForm(rule, race);
            }
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
            return Text("팩션", "Faction", "阵营");
        case bcn::DistributionScope::pluginFile:
            return Text("플러그인 파일명", "Plugin file name", "插件文件名");
        case bcn::DistributionScope::raceEditorID:
            return Text("종족", "Race", "种族");
        case bcn::DistributionScope::keyword:
            return Text("키워드", "Keyword", "关键字");
        case bcn::DistributionScope::npcClass:
            return Text("클래스", "Class", "职业");
        case bcn::DistributionScope::combatStyle:
            return Text("전투 스타일", "Combat style", "战斗风格");
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
        case bcn::DistributionScope::keyword:
            return Text("키워드", "Keyword", "关键字");
        case bcn::DistributionScope::npcClass:
            return Text("클래스", "Class", "职业");
        case bcn::DistributionScope::combatStyle:
            return Text("전투 스타일", "Combat style", "战斗风格");
        case bcn::DistributionScope::npcName:
            return Text("이름", "Name", "名称");
        case bcn::DistributionScope::npcBaseForm:
            return "FormID";
        }
        return "";
    }

    [[nodiscard]] bool DistributionScopeCombo(bcn::DistributionScope& scope)
    {
        constexpr std::array order{
            bcn::DistributionScope::allNPCs,
            bcn::DistributionScope::pluginFile,
            bcn::DistributionScope::raceEditorID,
            bcn::DistributionScope::factionEditorID,
            bcn::DistributionScope::keyword,
            bcn::DistributionScope::npcClass,
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
        PrepareDownwardResizableDropdown(options.size() + savedValueRow);
        if (ImGui::BeginCombo(id, preview, ImGuiComboFlags_PopupOnlyDown)) {
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

    [[nodiscard]] bool DistributionFormTargetCombo(const char* id, bcn::DistributionRule& rule,
        const std::vector<DistributionTargetOption>& options)
    {
        const auto selected = std::ranges::find_if(options, [&rule](const auto& option) {
            if (!rule.targetPlugin.empty() && rule.targetLocalFormID != 0U) {
                return Lower(option.plugin) == Lower(rule.targetPlugin) &&
                    option.localFormID == rule.targetLocalFormID;
            }
            return !rule.target.empty() && !option.editorID.empty() &&
                Lower(option.editorID) == Lower(rule.target);
        });
        const auto savedValue = selected == options.end() &&
            (!rule.target.empty() || !rule.targetPlugin.empty());
        std::string savedLabel;
        if (savedValue) {
            savedLabel = !rule.target.empty() ? rule.target :
                std::format("{}:{:06X}", rule.targetPlugin, rule.targetLocalFormID);
            savedLabel += Text(" (저장값)", " (saved value)", "（保存值）");
        }
        const auto* preview = selected != options.end() ? selected->display.c_str() :
            savedValue ? savedLabel.c_str() : Text("선택", "Select", "选择");
        auto changed = false;
        PrepareDownwardResizableDropdown(options.size() + (savedValue ? 1U : 0U));
        if (ImGui::BeginCombo(id, preview, ImGuiComboFlags_PopupOnlyDown)) {
            if (savedValue) {
                ImGui::Selectable(savedLabel.c_str(), true);
                ImGui::Separator();
            }
            for (const auto& option : options) {
                const auto isSelected = selected != options.end() &&
                    option.runtimeFormID == selected->runtimeFormID;
                if (ImGui::Selectable(option.display.c_str(), isSelected)) {
                    if (auto* form = RE::TESForm::LookupByID(option.runtimeFormID)) {
                        changed = bcn::SetDistributionRuleTargetForm(rule, form);
                    }
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    [[nodiscard]] bool SaveActiveDistributionRules()
    {
        // Never activate an unsaved draft when the disk write fails.
        if (!bcn::Distribution::Get().SaveRulesForNextGame(g_distributionRules)) return false;
        bcn::Distribution::Get().SetRules(g_distributionRules);
        return true;
    }

    void DiscardDistributionDraft()
    {
        // Keep catalog metadata cached. Discarding edits must not trigger a
        // new faction/race/keyword scan on the next popup in this menu session.
        g_distributionRules = g_distributionEditorLoaded ?
            bcn::Distribution::Get().SavedRulesSnapshot() : std::vector<bcn::DistributionRule>{};
        g_selectedDistributionRule = 0U;
        ClearDistributionCatalogSelection();
    }

    [[nodiscard]] const char* OverlayAreaLabel(const bcn::overlay::Area area)
    {
        switch (area) {
        case bcn::overlay::Area::face: return Text("얼굴", "Face", "脸部");
        case bcn::overlay::Area::body: return Text("몸", "Body", "身体");
        case bcn::overlay::Area::hands: return Text("손", "Hands", "手部");
        case bcn::overlay::Area::feet: return Text("발", "Feet", "脚部");
        default: return Text("알 수 없음", "Unknown", "未知");
        }
    }

    void ToggleOverlayFavorite(const std::string& id)
    {
        auto settings = bcn::Settings::Get().Snapshot();
        const auto found = std::ranges::find(settings.favoriteOverlays, id);
        if (found == settings.favoriteOverlays.end()) settings.favoriteOverlays.push_back(id);
        else settings.favoriteOverlays.erase(found);
        bcn::Settings::Get().Update(settings);
        if (!bcn::Settings::Get().Save()) {
            bcn::ui::Notify(Text("즐겨찾기를 저장하지 못했습니다.", "Could not save favorites.", "无法保存收藏。"));
        }
    }

    [[nodiscard]] const char* OverlayApplyResultMessage(const bcn::overlay::ApplyResult result)
    {
        switch (result) {
        case bcn::overlay::ApplyResult::queued:
            return Text("오버레이를 즉시 반영했습니다.", "Applied the overlay immediately.", "已立即应用叠加层。");
        case bcn::overlay::ApplyResult::unavailable:
        case bcn::overlay::ApplyResult::unsupportedInterface:
            return Text("RaceMenu Overlay/Override 인터페이스를 찾지 못했습니다.",
                "RaceMenu's Overlay/Override interfaces are unavailable.",
                "RaceMenu 的叠加层/覆盖接口不可用。");
        case bcn::overlay::ApplyResult::actor3DUnavailable:
            return Text("액터의 3D가 로드되지 않아 즉시 적용할 수 없습니다.",
                "The actor's 3D is not loaded, so the overlay cannot be applied immediately.",
                "角色的 3D 尚未加载，无法立即应用叠加层。");
        case bcn::overlay::ApplyResult::missingEntry:
            return Text("RaceMenu 목록에서 사라졌거나 텍스처가 비어 있는 오버레이입니다.",
                "The overlay disappeared from RaceMenu's catalog or has no texture.",
                "该叠加层已从 RaceMenu 列表中消失或没有纹理。");
        case bcn::overlay::ApplyResult::noFreeSlot:
            return Text("이 부위에 비어 있는 RaceMenu 오버레이 슬롯이 없습니다.",
                "There is no free RaceMenu overlay slot for this area.",
                "此部位没有可用的 RaceMenu 叠加层槽位。");
        case bcn::overlay::ApplyResult::ownershipConflict:
            return Text("다른 모드가 해당 슬롯을 바꿔 BCNG가 덮어쓰지 않았습니다.",
                "Another mod changed that slot, so BCNG did not overwrite it.",
                "其他模组已更改该槽位，因此 BCNG 未覆盖它。");
        case bcn::overlay::ApplyResult::unsupportedFace:
            return Text("현재 얼굴 지오메트리에서 안전한 FaceGen 오버레이 대상을 찾지 못했습니다.",
                "No safe FaceGen overlay target was found on the current face geometry.",
                "在当前脸部几何体上找不到安全的 FaceGen 叠加层目标。");
        case bcn::overlay::ApplyResult::incompatibleActor:
            return Text("선택한 오버레이는 이 액터의 성별 또는 UBE/일반 바디 구조와 맞지 않습니다.",
                "The overlay does not match this actor's sex or UBE/conventional-body layout.",
                "该叠加层与此角色的性别或 UBE/常规身体结构不匹配。");
        case bcn::overlay::ApplyResult::noTaskInterface:
            return Text("SKSE 게임 작업 인터페이스를 사용할 수 없습니다.",
                "The SKSE game-task interface is unavailable.",
                "SKSE 游戏任务接口不可用。");
        default:
            return Text("적용할 액터가 없습니다.", "No actor is available.", "没有可应用的角色。");
        }
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
        case bcn::racemenu::ApplyResult::incompatibleSex:
            return Text("선택한 바디 프리셋은 이 액터의 성별과 맞지 않습니다.", "The selected body preset does not match this actor's sex.", "所选身体预设与该角色的性别不匹配。");
        case bcn::racemenu::ApplyResult::incompatibleBodyFamily:
            return Text("선택한 바디 프리셋은 이 액터의 바디 계열과 맞지 않습니다.", "The selected body preset does not match this actor's body family.", "所选身体预设与该角色的身体系列不匹配。");
        case bcn::racemenu::ApplyResult::noTaskInterface:
            return Text("SKSE 게임 작업 인터페이스를 사용할 수 없습니다.", "The SKSE game-task interface is unavailable.", "SKSE 游戏任务接口不可用。");
        default:
            return Text("적용할 액터가 없습니다.", "No actor is available to apply this preset.", "没有可应用预设的角色。");
        }
    }

    [[nodiscard]] const char* SkinApplyResultMessage(const bcn::skin_application::ApplyResult result)
    {
        switch (result) {
        case bcn::skin_application::ApplyResult::queued:
            return Text("스킨을 즉시 반영했습니다.", "Applied the skin immediately.", "已立即应用皮肤。");
        case bcn::skin_application::ApplyResult::missingProfile:
            return Text("새로고침 후 사라진 스킨팩입니다.", "The skin pack disappeared after refresh.", "刷新后该皮肤包已不存在。");
        case bcn::skin_application::ApplyResult::incompatibleSex:
            return Text("선택한 스킨팩은 이 액터의 성별과 맞지 않습니다.", "The selected skin pack does not match this actor's sex.", "所选皮肤包与该角色的性别不匹配。");
        case bcn::skin_application::ApplyResult::incompatibleRace:
            return Text("선택한 스킨팩은 이 액터의 종족과 맞지 않습니다.", "The selected skin pack does not match this actor's race.", "所选皮肤包与该角色的种族不匹配。");
        case bcn::skin_application::ApplyResult::incompatibleBodyFamily:
            return Text("선택한 스킨팩은 이 액터의 바디 계열과 맞지 않습니다.", "The selected skin pack does not match this actor's body family.", "所选皮肤包与该角色的身体系列不匹配。");
        case bcn::skin_application::ApplyResult::ambiguousProfileLayout:
            return Text("스킨팩의 텍스처 구조를 지원되는 레이아웃으로 확정할 수 없습니다.", "The skin pack's texture structure does not identify a supported layout.", "无法根据皮肤包的纹理结构确定受支持的布局。");
        case bcn::skin_application::ApplyResult::ambiguousActorLayout:
            return Text("액터의 바디 UV 레이아웃을 안전하게 판별하지 못해 적용을 중단했습니다.", "The actor's body UV layout could not be identified safely, so the skin was not applied.", "无法安全识别角色的身体 UV 布局，因此未应用皮肤。");
        case bcn::skin_application::ApplyResult::incompatibleFutanariType:
            return Text("선택한 후타나리 스킨은 현재 성기 유형과 맞지 않습니다.", "The selected futanari skin does not match the active genital type.", "所选扶她皮肤与当前生殖器类型不匹配。");
        case bcn::skin_application::ApplyResult::noTaskInterface:
            return Text("SKSE 게임 작업 인터페이스를 사용할 수 없습니다.", "The SKSE game-task interface is unavailable.", "SKSE 游戏任务接口不可用。");
        case bcn::skin_application::ApplyResult::unsupportedRuntime:
            return Text("검증되지 않은 Skyrim 버전이라 스킨 적용을 안전하게 중단했습니다.", "Skin application was stopped safely on an unaudited Skyrim runtime.", "由于 Skyrim 运行时版本未经验证，已安全停止皮肤应用。");
        case bcn::skin_application::ApplyResult::actorBaseUnavailable:
            return Text("이 액터의 기본 Skin Armor를 찾지 못했습니다.", "The actor's native Skin Armor is unavailable.", "找不到该角色的原生皮肤护甲。");
        case bcn::skin_application::ApplyResult::sharedActorBaseConflict:
            return Text("같은 ActorBase를 공유하는 다른 NPC가 이미 다른 스킨을 사용 중입니다.", "Another NPC sharing this ActorBase already owns a different skin selection.", "共享此 ActorBase 的另一个 NPC 已使用不同的皮肤选择。");
        case bcn::skin_application::ApplyResult::ownershipConflict:
            return Text("다른 모드가 Skin Armor를 교체해 덮어쓰지 않았습니다.", "Another mod replaced the Skin Armor, so Body Change NG did not overwrite it.", "其他模组已替换皮肤护甲，因此 Body Change NG 未覆盖它。");
        default:
            return Text("적용할 액터가 없습니다.", "No actor is available.", "没有可应用的角色。");
        }
    }

    [[nodiscard]] bool QueuePreset(const CatalogItem& item, const bcn::racemenu::ApplyMode mode)
    {
        auto* actor = SelectedActor();
        const auto result = bcn::racemenu::QueueApply(actor, item.id, mode);
        if (result == bcn::racemenu::ApplyResult::queued && mode == bcn::racemenu::ApplyMode::commit) {
            // Direct choices are save-specific for the player and NPCs alike.
            // Distribution ignores the player, but ActorRegistry is also the
            // single ASTR co-save owner used by player load restoration.
            bcn::Distribution::Get().SetManualAssignment(actor, item.id);
            bcn::OutfitRefit::Get().ProcessActor(actor);
        }
        if (result != bcn::racemenu::ApplyResult::queued) {
            bcn::ui::Notify(std::string(item.name) + " · " + ApplyResultMessage(result));
        }
        return result == bcn::racemenu::ApplyResult::queued;
    }

    void SaveManualSkinIfNeeded(RE::Actor* actor, const std::string& profileId)
    {
        if (!actor) return;
        bcn::Distribution::Get().SetManualSkinAssignment(actor, profileId);
    }

    void SaveManualDefaultBodyIfNeeded(RE::Actor* actor)
    {
        if (!actor) return;
        bcn::Distribution::Get().SetManualDefaultBody(actor);
    }

    void SaveManualDefaultSkinIfNeeded(RE::Actor* actor)
    {
        if (!actor) return;
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
        if (!persistSelection) {
            const auto result = bcn::racemenu::QueuePreviewDefault(actor);
            if (result != bcn::racemenu::ApplyResult::queued) {
                bcn::ui::Notify(ApplyResultMessage(result));
            }
            return result == bcn::racemenu::ApplyResult::queued;
        }
        bcn::racemenu::QueueClearBodyChangeMorphs(actor);
        if (persistSelection) SaveManualDefaultBodyIfNeeded(actor);
        return true;
    }

    [[nodiscard]] bool QueueDefaultSkin(const bool persistSelection)
    {
        auto* actor = SelectedActor();
        const auto result = bcn::skin_application::QueueClear(actor);
        if (result == bcn::skin_application::ApplyResult::queued && persistSelection) {
            SaveManualDefaultSkinIfNeeded(actor);
        } else {
            if (result != bcn::skin_application::ApplyResult::queued) bcn::ui::Notify(SkinApplyResultMessage(result));
        }
        return result == bcn::skin_application::ApplyResult::queued;
    }

    void RememberPending(std::optional<PendingChoice>& pending, RE::Actor* actor,
        std::string id, const bool useDefault, std::string originalId)
    {
        if (!actor) return;
        bcn::frame_tasks::SetPreviewActor(actor->GetFormID());
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

    void RollbackPendingSelections(RE::Actor* actor)
    {
        if (!bcn::frame_tasks::IsCurrent(g_uiSessionEpoch.load())) return;
        if (actor) {
            const auto actorFormID = actor->GetFormID();
            if (g_pendingSkin && g_pendingSkin->actorFormID == actorFormID) {
                const auto result = g_pendingSkin->originalId.empty() ?
                    bcn::skin_application::QueueClear(actor) :
                    bcn::skin_application::QueueApply(actor, g_pendingSkin->originalId);
                if (result != bcn::skin_application::ApplyResult::queued) {
                    bcn::ui::Notify(SkinApplyResultMessage(result));
                }
            }
            if (g_pendingFutanari && g_pendingFutanari->actorFormID == actorFormID) {
                const auto result = g_pendingFutanari->originalId.empty() ?
                    bcn::skin_application::QueueClearFutanari(actor,
                        bcn::skin_application::FutanariSelectionMode::restore) :
                    bcn::skin_application::QueueApplyFutanari(actor,
                        g_pendingFutanari->originalId,
                        bcn::skin_application::FutanariSelectionMode::restore);
                if (result != bcn::skin_application::ApplyResult::queued) {
                    bcn::ui::Notify(SkinApplyResultMessage(result));
                }
            }
            bcn::overlay::QueueCancelPreviews(actor);
        } else {
            bcn::overlay::QueueCancelPreviews();
        }
        // Body previews live in a dedicated RaceMenu morph key, so cancelling
        // that key restores the exact last committed body without another
        // persistent write.
        bcn::racemenu::QueueCancelPreview();

        if (g_pendingTintBaseline) {
            const auto baseline = *g_pendingTintBaseline;
            bcn::player_tint::RestorePersistedState(baseline, false);
            const auto result = !baseline.pack && baseline.layers.empty() ?
                bcn::player_tint::QueueRestoreAll() :
                bcn::player_tint::QueueReapplyCurrent();
            if (result != bcn::player_tint::ApplyResult::queued) {
                bcn::ui::Notify(TintResultText(result));
            }
            g_currentTintPack = baseline.pack.value_or(std::string{});
            g_selectedTintPack = g_currentTintPack;
            g_selectedTintAssetID.clear();
        }

        g_pendingBody.reset();
        g_pendingSkin.reset();
        g_pendingFutanari.reset();
        g_pendingTint.reset();
        g_pendingTintBaseline.reset();
        for (auto& pending : g_pendingOverlays) pending.reset();
        bcn::frame_tasks::SetPreviewActor(0U);
    }

    void UpdatePreviewOwnership()
    {
        RE::FormID owner{};
        const auto observe = [&owner](const auto& pending) {
            if (pending) owner = pending->actorFormID;
        };
        observe(g_pendingBody); observe(g_pendingSkin); observe(g_pendingFutanari);
        observe(g_pendingTint);
        for (const auto& pending : g_pendingOverlays) observe(pending);
        bcn::frame_tasks::SetPreviewActor(owner);
    }

    void DiscardPendingSelectionsAfterReset(const bool allActors)
    {
        const auto* actor = SelectedActor();
        const auto id = actor ? actor->GetFormID() : 0U;
        if (allActors) {
            g_overlayColorDrafts.Clear();
            g_tintColorDrafts.Clear();
        } else {
            g_overlayColorDrafts.EraseActor(id);
            g_tintColorDrafts.EraseActor(id);
        }
        const auto discard = [allActors, id](auto& pending) {
            if (pending && (allActors || pending->actorFormID == id)) pending.reset();
        };
        discard(g_pendingBody);
        discard(g_pendingSkin);
        discard(g_pendingFutanari);
        for (auto& pending : g_pendingOverlays) discard(pending);
        if (allActors || (actor && actor->IsPlayerRef())) {
            g_pendingTint.reset();
            g_pendingTintBaseline.reset();
            g_currentTintPack.clear();
            g_selectedTintPack.clear();
            g_selectedTintAssetID.clear();
        }
    }

    void HandleTabNavigation(const bool playerSelected, const bool futanariAvailable)
    {
        if (CatalogNavigationBlocked()) return;
        const auto left = NavigationKeyPressed(
            ImGuiKey_LeftArrow, ImGuiKey_A, ImGuiKey_GamepadDpadLeft, false);
        const auto right = NavigationKeyPressed(
            ImGuiKey_RightArrow, ImGuiKey_D, ImGuiKey_GamepadDpadRight, false);
        if (left == right) return;

        const auto previousTab = g_activeTab;
        const auto tabs = bcn::ui_catalog::ResolveAvailableTabs(playerSelected, futanariAvailable);
        const auto move = [&](const auto& availableTabs) {
            const auto begin = availableTabs.values.begin();
            const auto end = begin + static_cast<std::ptrdiff_t>(availableTabs.size);
            const auto found = std::find(begin, end, g_activeTab);
            auto index = found == end ? std::size_t{} :
                static_cast<std::size_t>(found - begin);
            if (left) index = index == 0U ? availableTabs.size - 1U : index - 1U;
            else index = (index + 1U) % availableTabs.size;
            g_activeTab = availableTabs.values[index];
        };
        move(tabs);
        if (g_activeTab != previousTab && g_activeTab == ActiveTab::overlay) {
            [[maybe_unused]] const auto requested = bcn::overlay::RequestCatalog(SelectedActor());
        }
    }

    void DrawCatalog(std::vector<CatalogItem>& items, const bool body)
    {
        auto* actor = SelectedActor();
        const auto distributionSelecting = body &&
            IsDistributionSelectionFor(DistributionPool::body);
        const auto backendCurrentBody = bcn::racemenu::CurrentPresetId(actor);
        const auto confirmedBodyId = g_pendingBody && actor && g_pendingBody->actorFormID == actor->GetFormID() ?
            g_pendingBody->originalId : backendCurrentBody.value_or(std::string{});

        std::vector<CatalogItem*> visibleItems;
        visibleItems.reserve(items.size());
        for (auto& item : items) {
            if ((FavoritesOnly() && !item.favorite) || !MatchSearch(item)) continue;
            visibleItems.push_back(&item);
        }
        const auto hasDefaultRow = body && !distributionSelecting;
        std::size_t preferredIndex{};
        if (!confirmedBodyId.empty()) {
            const auto current = std::ranges::find(visibleItems, confirmedBodyId,
                [](const CatalogItem* item) -> const std::string& { return item->id; });
            if (current != visibleItems.end()) preferredIndex =
                (hasDefaultRow ? 1U : 0U) + static_cast<std::size_t>(current - visibleItems.begin());
        }
        const auto navigation = HandleCatalogNavigation(
            visibleItems.size() + (hasDefaultRow ? 1U : 0U), preferredIndex);
        const auto previewRow = [&](const std::size_t row) {
            if (distributionSelecting) return;
            if (hasDefaultRow && row == 0U) {
                if (QueueDefaultBody(false)) RememberPending(g_pendingBody, actor, {}, true, confirmedBodyId);
                return;
            }
            auto& item = *visibleItems[row - (hasDefaultRow ? 1U : 0U)];
            if (item.compatible && body && QueuePreset(item, bcn::racemenu::ApplyMode::preview)) {
                RememberPending(g_pendingBody, actor, item.id, false, confirmedBodyId);
            } else if (!item.compatible) {
                bcn::ui::Notify(Text("현재 액터와 호환되지 않습니다.", "This item is incompatible with the selected actor.", "与所选角色不兼容。"));
            }
        };
        const auto confirmRow = [&](const std::size_t row) {
            if (distributionSelecting) {
                auto& item = *visibleItems[row];
                SetDistributionItemSelected(item.id, !DistributionItemSelected(item.id));
                return;
            }
            if (hasDefaultRow && row == 0U) {
                if (QueueDefaultBody(true)) g_pendingBody.reset();
                return;
            }
            auto& item = *visibleItems[row - (hasDefaultRow ? 1U : 0U)];
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
            if (hasDefaultRow) {
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
                    confirmedBodyId.empty() ? kCardSelected :
                    hovered || navigationFocused ? kCardHovered : kCardNormal, Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), kCardText,
                    Text("기본 바디", "Default body", "默认身体"));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)), kCardSubtext,
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
                        "선택한 액터의 성별·바디 계열에 맞는 BodySlide 프리셋이 없습니다.",
                        "No BodySlide presets were found for the selected actor's sex and body family.",
                        "未找到适用于所选角色性别和体型系列的 BodySlide 预设。"));
                    ImGui::TextWrapped("%s", Text(
                        "CalienteTools\\BodySlide\\SliderPresets\\*.xml에 프리셋 XML을 넣고 새로고침하세요.",
                        "Place preset XML files in CalienteTools\\BodySlide\\SliderPresets\\*.xml, then press Refresh.",
                        "请将预设 XML 文件放入 CalienteTools\\BodySlide\\SliderPresets\\*.xml，然后点击‘刷新’。"));
                    ImGui::Spacing();
                }
            }
            for (auto* itemPointer : visibleItems) {
                auto& item = *itemPointer;
                ImGui::PushID(item.id.c_str());
                const auto rowCursor = ImGui::GetCursorScreenPos();
                const auto cardHeight = Scaled(48.0F);
                if (distributionSelecting) {
                    auto selected = DistributionItemSelected(item.id);
                    if (CenteredCheckbox("##distributionSelected", selected,
                            rowCursor, cardHeight)) {
                        SetDistributionItemSelected(item.id, selected);
                    }
                }
                const auto cursor = ImGui::GetCursorScreenPos();
                const auto width = ImGui::GetContentRegionAvail().x;
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
                const ImU32 fill = confirmedCurrent ? kCardSelected :
                    hovered || (navigation.hasFocus && navigation.focused == row) ?
                    kCardHovered : kCardNormal;
                draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + cardHeight), fill, Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), kCardText, item.name.c_str());
                const auto sub = item.family + (confirmedCurrent ? " · " + std::string(Text("현재 적용", "Current", "当前应用")) :
                    item.compatible ? "" : " · " + std::string(Text("호환되지 않음", "Not compatible", "不兼容")));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)),
                    item.compatible ? kCardSubtext : kCardIncompatible, sub.c_str());
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + width - favoriteWidth, cursor.y));
                if (FavoriteButton(item.favorite, cardHeight)) ToggleFavorite(item);
                if (distributionSelecting && clicked) {
                    SetDistributionItemSelected(item.id, !DistributionItemSelected(item.id));
                    FocusCatalogRow(row);
                } else if (doubleClicked) {
                    FocusCatalogRow(row);
                    confirmRow(row);
                } else if (clicked) {
                    FocusCatalogRow(row);
                    previewRow(row);
                }
                ScrollFocusedCatalogRow(row);
                ImGui::SetCursorScreenPos(ImVec2(rowCursor.x, cursor.y + cardHeight + Scaled(5.0F)));
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
        const auto actorRace = bcn::ResolveActorSkinRace(actor);
        const auto distributionSelecting = IsDistributionSelectionFor(DistributionPool::skin);

        const auto skins = bcn::SkinProfiles::Get().Snapshot();
        const auto settings = bcn::Settings::Get().Snapshot();
        // Skin UV compatibility must use the same live evidence as the
        // executor. Distribution defaults are suitable for morph filtering,
        // but must never make an unknown actor look safe for a DDS write.
        const auto actorFamily = bcn::body_family::ResolveActor(actor);
        const auto actorSex = female ? bcn::SkinSex::female : bcn::SkinSex::male;
        const auto backendCurrentSkin = bcn::skin_application::CurrentProfileId(actor);
        const auto confirmedSkinId = g_pendingSkin && g_pendingSkin->actorFormID == actor->GetFormID() ?
            g_pendingSkin->originalId : backendCurrentSkin.value_or(std::string{});

        std::vector<const bcn::SkinProfile*> visibleSkins;
        visibleSkins.reserve(skins.size());
        for (const auto& skin : skins) {
            if (distributionSelecting) {
                if (skin.sex != actorSex || skin.race != bcn::SkinRace::humanoid ||
                    skin.layout != bcn::SkinLayout::legacy) continue;
            } else if (!bcn::SkinProfileCompatibility(
                           skin, actorSex, actorRace, actorFamily).Compatible()) {
                continue;
            }
            if (!g_search.empty() && Lower(skin.name).find(Lower(g_search)) == std::string::npos &&
                Lower(skin.id).find(Lower(g_search)) == std::string::npos) continue;
            const auto favorite = std::ranges::find(settings.favoriteSkinProfiles, skin.id) !=
                settings.favoriteSkinProfiles.end();
            if (FavoritesOnly() && !favorite) continue;
            visibleSkins.push_back(&skin);
        }
        DrawCatalogCommandRow(DistributionPool::skin,
            [] { [[maybe_unused]] const auto started = bcn::SkinProfiles::Get().RefreshAsync(); },
            [&visibleSkins] {
                g_distributionSelectedIds.clear();
                for (const auto* skin : visibleSkins) g_distributionSelectedIds.insert(skin->id);
            },
            bcn::SkinProfiles::Get().Refreshing() ?
                Text("BodySkin\\<스킨팩>에서 바디스킨을 새로고침하는 중입니다. 기존 목록은 계속 사용할 수 있습니다.",
                    "Refreshing body skins from BodySkin\\<skin pack> in the background. The current list remains available.",
                    "正在后台从 BodySkin\\<皮肤包> 刷新身体皮肤。当前列表仍可继续使用。") :
                Text("BodySkin\\<스킨팩>\\textures\\~에서 바디스킨을 읽습니다.(더블클릭 적용)",
                    "Reads body skins from BodySkin\\<skin pack>\\textures\\~. (Double-click to apply)",
                    "从 BodySkin\\<皮肤包>\\textures\\~ 读取身体皮肤。（双击应用）"));
        const auto hasDefaultRow = !distributionSelecting;
        std::size_t preferredIndex{};
        if (!confirmedSkinId.empty()) {
            const auto current = std::ranges::find(visibleSkins, confirmedSkinId,
                [](const bcn::SkinProfile* skin) -> const std::string& { return skin->id; });
            if (current != visibleSkins.end()) preferredIndex =
                (hasDefaultRow ? 1U : 0U) + static_cast<std::size_t>(current - visibleSkins.begin());
        }
        const auto navigation = HandleCatalogNavigation(
            visibleSkins.size() + (hasDefaultRow ? 1U : 0U), preferredIndex);
        const auto previewRow = [&](const std::size_t row) {
            if (distributionSelecting) return;
            if (hasDefaultRow && row == 0U) {
                if (QueueDefaultSkin(false)) RememberPending(g_pendingSkin, actor, {}, true, confirmedSkinId);
                return;
            }
            const auto& skin = *visibleSkins[row - (hasDefaultRow ? 1U : 0U)];
            const auto result = bcn::skin_application::QueueApply(actor, skin.id);
            if (result == bcn::skin_application::ApplyResult::queued) {
                RememberPending(g_pendingSkin, actor, skin.id, false, confirmedSkinId);
            } else {
                bcn::ui::Notify(skin.name + " · " + SkinApplyResultMessage(result));
            }
        };
        const auto confirmRow = [&](const std::size_t row) {
            if (distributionSelecting) {
                const auto& skin = *visibleSkins[row];
                SetDistributionItemSelected(skin.id, !DistributionItemSelected(skin.id));
                return;
            }
            if (hasDefaultRow && row == 0U) {
                if (QueueDefaultSkin(true)) g_pendingSkin.reset();
                return;
            }
            const auto& skin = *visibleSkins[row - (hasDefaultRow ? 1U : 0U)];
            const auto result = bcn::skin_application::QueueApply(actor, skin.id);
            if (result == bcn::skin_application::ApplyResult::queued) {
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
            if (hasDefaultRow) {
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
                confirmedSkinId.empty() ? kCardSelected :
                defaultHovered || (navigation.hasFocus && navigation.focused == row) ?
                kCardHovered : kCardNormal, Scaled(4.0F));
            defaultDraw->AddText(ImVec2(defaultCursor.x + Scaled(10.0F), defaultCursor.y + Scaled(7.0F)), kCardText,
                Text("기본 스킨", "Default skin", "默认皮肤"));
            defaultDraw->AddText(ImVec2(defaultCursor.x + Scaled(10.0F), defaultCursor.y + Scaled(27.0F)), kCardSubtext,
                Text("몸 · 손 · 발 · 얼굴을 기본 스킨으로 복원", "Restore the original body · hands · feet · face skin", "恢复身体、手、脚和脸部的默认皮肤"));
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
            }
            const auto hasMatchingSkin = std::ranges::any_of(skins, [actorSex, actorFamily, actorRace](const auto& skin) {
                return bcn::SkinProfileCompatibility(
                    skin, actorSex, actorRace, actorFamily).Compatible();
            });
            if (!hasMatchingSkin) {
                ImGui::TextUnformatted(Text(
                    "선택한 액터의 종족·성별·바디 계열에 맞는 스킨팩이 없습니다.",
                    "No skin packs were found for the selected actor's race, sex, and body family.",
                    "未找到适用于所选角色种族、性别和体型系列的皮肤包。"));
                ImGui::Spacing();
                ImGui::TextWrapped("%s", Text(
                    "일반 스킨은 BodySkin\\<스킨팩>\\Textures\\actors\\character\\..., UBE 스킨은 BodySkin\\<스킨팩>\\Textures\\!UBE\\Body 및 Head 구조로 넣고 새로고침하세요.",
                    "Use BodySkin\\<skin pack>\\Textures\\actors\\character\\... for standard skins, or BodySkin\\<skin pack>\\Textures\\!UBE\\Body and Head for UBE skins, then press Refresh.",
                    "普通皮肤请使用 BodySkin\\<皮肤包>\\Textures\\actors\\character\\...；UBE 皮肤请使用 BodySkin\\<皮肤包>\\Textures\\!UBE\\Body 和 Head 结构，然后点击‘刷新’。"));
            }
            for (const auto* skinPointer : visibleSkins) {
                const auto& skin = *skinPointer;
                const bool favorite = std::ranges::find(settings.favoriteSkinProfiles, skin.id) !=
                    settings.favoriteSkinProfiles.end();
                ImGui::PushID(skin.id.c_str());
                const auto rowCursor = ImGui::GetCursorScreenPos();
                const auto height = Scaled(48.0F);
                if (distributionSelecting) {
                    auto selected = DistributionItemSelected(skin.id);
                    if (CenteredCheckbox("##distributionSelected", selected,
                            rowCursor, height)) {
                        SetDistributionItemSelected(skin.id, selected);
                    }
                }
                const auto cursor = ImGui::GetCursorScreenPos();
                const auto width = ImGui::GetContentRegionAvail().x;
                const auto favoriteWidth = Scaled(46.0F);
                ImGui::InvisibleButton("item", ImVec2((std::max)(0.0F, width - favoriteWidth), height));
                const auto hovered = ImGui::IsItemHovered();
                const auto clicked = ImGui::IsItemClicked();
                const auto doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                const auto confirmedCurrent = skin.id == confirmedSkinId;
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                    confirmedCurrent ? kCardSelected :
                    hovered || (navigation.hasFocus && navigation.focused == row) ?
                    kCardHovered : kCardNormal, Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)),
                    kCardText, skin.name.c_str());
                std::unordered_set<std::string> texturePaths;
                const auto collectPaths = [&texturePaths](const auto& layers) {
                    for (const auto& layer : layers) texturePaths.insert(layer.path);
                };
                collectPaths(skin.body);
                collectPaths(skin.cbbeGenitalAnal);
                collectPaths(skin.unpGenitalAnal);
                collectPaths(skin.hands);
                collectPaths(skin.feet);
                collectPaths(skin.face);
                collectPaths(skin.vampireFace);
                collectPaths(skin.elderBody);
                collectPaths(skin.elderHands);
                collectPaths(skin.elderFace);
                for (const auto& raceFace : skin.raceFace) collectPaths(raceFace);
                collectPaths(skin.faceDetails);
                const auto textureCount = texturePaths.size();
                const auto sub = std::string{ female ? Text("여성", "Female", "女性") : Text("남성", "Male", "男性") } +
                    " · " + bcn::SkinFamilyLabel(skin.layout, skin.sex) +
                    " · " + Text("텍스처 ", "Textures ", "纹理 ") + std::to_string(textureCount) + Text("개", "", " 个") +
                    (confirmedCurrent ? " · " + std::string(Text("현재 적용", "Current", "当前应用")) : "");
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)),
                    kCardSubtext, sub.c_str());
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + width - favoriteWidth, cursor.y));
                if (FavoriteButton(favorite, height)) ToggleSkinFavorite(skin.id);
                if (distributionSelecting && clicked) {
                    SetDistributionItemSelected(skin.id, !DistributionItemSelected(skin.id));
                    FocusCatalogRow(row);
                } else if (doubleClicked) {
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

    void DrawOverlayCatalog()
    {
        auto* actor = SelectedActor();
        if (!actor) {
            ImGui::TextUnformatted(Text("액터를 선택하세요.", "Select an actor.", "请选择角色。"));
            return;
        }
        const auto distributionSelecting = IsDistributionSelectionFor(DistributionPool::overlay);

        if (!bcn::overlay::IsReady()) {
            ImGui::TextColored(ImVec4(1.0F, .62F, .35F, 1.0F), "%s", Text(
                "RaceMenu Overlay/Override 인터페이스를 기다리는 중입니다.",
                "Waiting for RaceMenu's Overlay/Override interfaces.",
                "正在等待 RaceMenu 的叠加层/覆盖接口。"));
            return;
        }

        constexpr auto areaCount = bcn::overlay::Index(bcn::overlay::Area::count);
        struct EntriesCache {
            std::uint64_t revision{}, epoch{};
            bcn::body_family::Mask family{};
            bool female{}, distribution{}, initialized{};
            std::array<std::vector<bcn::overlay::Entry>, areaCount> entries;
        };
        static EntriesCache cache;
        auto& entriesByArea = cache.entries;
        std::array<std::vector<const bcn::overlay::Entry*>, areaCount> visibleByArea;
        std::array<std::unordered_set<std::string>, areaCount> confirmedIds;
        const auto settings = bcn::Settings::Get().Snapshot();
        const std::unordered_set<std::string_view> favorites(settings.favoriteOverlays.begin(),
            settings.favoriteOverlays.end());
        const auto needle = Lower(g_search);
        const auto* base = actor->GetActorBase();
        const auto female = !base || base->GetSex() == RE::SEX::kFemale;
        const auto family = bcn::body_family::ResolveActor(actor);
        const auto revision = bcn::overlay::CatalogRevision();
        const auto epoch = bcn::frame_tasks::Epoch();
        const auto rebuildEntries = !cache.initialized || cache.revision != revision || cache.epoch != epoch ||
            cache.family != family || cache.female != female || cache.distribution != distributionSelecting;
        cache.revision = revision; cache.epoch = epoch; cache.family = family;
        cache.female = female; cache.distribution = distributionSelecting; cache.initialized = true;
        for (const auto area : bcn::overlay::kAreas) {
            const auto areaIndex = bcn::overlay::Index(area);
            if (rebuildEntries) entriesByArea[areaIndex] = distributionSelecting ?
                bcn::overlay::SnapshotLegacy(area, female) :
                bcn::overlay::SnapshotForActor(area, actor);
            auto& visible = visibleByArea[areaIndex];
            visible.reserve(entriesByArea[areaIndex].size());
            for (const auto& entry : entriesByArea[areaIndex]) {
                const auto favorite = favorites.contains(entry.id);
                if (FavoritesOnly() && !favorite) continue;
                if (!needle.empty() && Lower(entry.name).find(needle) == std::string::npos &&
                    Lower(entry.id).find(needle) == std::string::npos) continue;
                visible.push_back(std::addressof(entry));
            }
            for (auto& id : bcn::overlay::CurrentSelectionIds(actor, area)) {
                confirmedIds[areaIndex].insert(std::move(id));
            }
        }
        DrawCatalogCommandRow(DistributionPool::overlay,
            [actor] { [[maybe_unused]] const auto requested = bcn::overlay::RefreshCatalog(actor); },
            [&visibleByArea] {
                for (auto& ids : g_distributionSelectedOverlayIds) ids.clear();
                for (const auto area : bcn::overlay::kAreas) {
                    auto& selected = g_distributionSelectedOverlayIds[bcn::overlay::Index(area)];
                    for (const auto* entry : visibleByArea[bcn::overlay::Index(area)]) {
                        selected.insert(entry->id);
                    }
                }
            },
            Text("설치된 모드에서 오버레이를 읽습니다.(더블클릭 복수 적용/해제)",
                "Reads installed mods. (Double-click to apply/remove multiple)",
                "从已安装的模组读取叠加层。（双击应用/移除多个）"));

        struct OverlayRow
        {
            bcn::overlay::Area area{};
            const bcn::overlay::Entry* entry{};
        };
        std::vector<OverlayRow> rows;
        for (const auto area : bcn::overlay::kAreas) {
            const auto areaIndex = bcn::overlay::Index(area);
            if (!g_overlaySectionsOpen[areaIndex]) continue;
            if (!distributionSelecting) rows.push_back({ area, nullptr });
            for (const auto* entry : visibleByArea[areaIndex]) rows.push_back({ area, entry });
        }

        const auto applyRow = [&](const OverlayRow& row, const bool confirm) {
            if (distributionSelecting) {
                if (row.entry) {
                    g_overlayArea = row.area;
                    g_overlayFocusedIds[bcn::overlay::Index(row.area)] = row.entry->id;
                }
                if (confirm && row.entry) {
                    SetDistributionOverlaySelected(row.area, row.entry->id,
                        !DistributionOverlaySelected(row.area, row.entry->id));
                }
                return;
            }
            const auto areaIndex = bcn::overlay::Index(row.area);
            auto& pending = g_pendingOverlays[areaIndex];
            if (!confirm && row.entry && confirmedIds[areaIndex].contains(row.entry->id)) {
                g_overlayArea = row.area;
                return;
            }
            const auto mode = confirm ?
                bcn::overlay::ApplyMode::manualCommit : bcn::overlay::ApplyMode::preview;
            const auto result = row.entry ?
                bcn::overlay::QueueApply(actor, row.area, row.entry->id, mode,
                    g_overlayColorDrafts.Find(actor->GetFormID(),
                        static_cast<std::uint8_t>(row.area), row.entry->id)) :
                bcn::overlay::QueueClear(actor, row.area, mode);
            if (result == bcn::overlay::ApplyResult::queued) {
                g_overlayArea = row.area;
                if (confirm) pending.reset();
                else RememberPending(pending, actor, row.entry ? row.entry->id : std::string{},
                    row.entry == nullptr, confirmedIds[areaIndex].empty() ?
                        std::string{} : *confirmedIds[areaIndex].begin());
            } else {
                const auto prefix = row.entry ? row.entry->name + " · " : std::string{};
                bcn::ui::Notify(prefix + OverlayApplyResultMessage(result));
            }
        };

        std::size_t preferredIndex{};
        if (g_overlayArea) {
            const auto preferredArea = *g_overlayArea;
            const auto& preferredId = g_overlayFocusedIds[bcn::overlay::Index(preferredArea)];
            const auto found = std::ranges::find_if(rows, [&](const OverlayRow& row) {
                return row.area == preferredArea && (preferredId.empty() ?
                    row.entry == nullptr : row.entry && row.entry->id == preferredId);
            });
            if (found != rows.end()) {
                preferredIndex = static_cast<std::size_t>(found - rows.begin());
            }
        }
        const auto navigation = HandleCatalogNavigation(rows.size(), preferredIndex);
        if (navigation.hasFocus && navigation.focused < rows.size() &&
            (navigation.preview || navigation.confirm)) {
            applyRow(rows[navigation.focused], navigation.confirm);
        }

        if (ImGui::BeginChild("OverlayCatalog", ImVec2(0.0F, CatalogListHeight()), true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs)) {
            std::size_t row{};
            for (const auto area : bcn::overlay::kAreas) {
                const auto areaIndex = bcn::overlay::Index(area);
                const auto& visible = visibleByArea[areaIndex];
                const auto& confirmed = confirmedIds[areaIndex];
                const char* paintLabel = "";
                switch (area) {
                case bcn::overlay::Area::face: paintLabel = "Face Paint"; break;
                case bcn::overlay::Area::body: paintLabel = "Body Paint"; break;
                case bcn::overlay::Area::hands: paintLabel = "Hand Paint"; break;
                case bcn::overlay::Area::feet: paintLabel = "Feet Paint"; break;
                default: break;
                }

                const auto wasOpen = g_overlaySectionsOpen[areaIndex];
                const auto usage = bcn::overlay::CurrentSlotUsage(actor, area);
                ImGui::SetNextItemOpen(wasOpen, ImGuiCond_Always);
                const auto headerId = std::string{ "###overlayArea_" } +
                    std::string{ bcn::overlay::StableName(area) };
                const auto open = ImGui::TreeNodeEx(headerId.c_str(),
                    ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding,
                    "%s · %s  (%zu)  ·  %s %u/%u",
                    OverlayAreaLabel(area), paintLabel, visible.size(),
                    Text("적용", "Applied", "已应用"), usage.applied, usage.capacity);
                if (ImGui::IsItemClicked()) g_overlayArea = area;
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", Text(
                        "앞: BCNG 확정 적용 수 · 뒤: 전체 슬롯에서 다른 모드가 차지한 슬롯을 뺀 수\n미리보기는 두 숫자에 포함하지 않습니다.",
                        "Applied: BCNG confirmed selections / total slots minus slots occupied by other mods.\nPreview does not change either number.",
                        "已应用：BCNG 已确认数量 / 总槽位减去其他模组占用的槽位。\n预览不改变两个数值。"));
                }
                g_overlaySectionsOpen[areaIndex] = open;
                if (open != wasOpen) NavigationState() = {};
                if (!open) continue;

                ImGui::PushID(bcn::overlay::StableName(area).data());
                auto renderedRowBottom = ImGui::GetCursorScreenPos().y;
                const auto drawRow = [&](const OverlayRow& overlayRow, const std::size_t globalRow) {
                    const auto* entry = overlayRow.entry;
                    ImGui::PushID(entry ? entry->id.c_str() : "DefaultOverlay");
                    const auto rowCursor = ImGui::GetCursorScreenPos();
                    const auto height = Scaled(48.0F);
                    if (distributionSelecting && entry) {
                        auto selected = DistributionOverlaySelected(area, entry->id);
                        if (CenteredCheckbox("##distributionSelected", selected,
                                rowCursor, height)) {
                            SetDistributionOverlaySelected(area, entry->id, selected);
                            g_overlayArea = area;
                            g_overlayFocusedIds[areaIndex] = entry->id;
                            FocusCatalogRow(globalRow);
                        }
                    }
                    const auto cursor = ImGui::GetCursorScreenPos();
                    const auto width = ImGui::GetContentRegionAvail().x;
                    const auto favoriteWidth = entry ? Scaled(46.0F) : 0.0F;
                    ImGui::InvisibleButton("item", ImVec2(width - favoriteWidth, height));
                    const auto hovered = ImGui::IsItemHovered();
                    const auto clicked = ImGui::IsItemClicked();
                    const auto doubleClicked = hovered &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                    auto* draw = ImGui::GetWindowDrawList();
                    const auto current = entry ? confirmed.contains(entry->id) : confirmed.empty();
                    draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                        current ? kCardSelected :
                        hovered || (navigation.hasFocus && navigation.focused == globalRow) ?
                        kCardHovered : kCardNormal, Scaled(4.0F));
                    draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)),
                        kCardText, entry ? entry->name.c_str() :
                            Text("기본값 복원", "Restore default", "恢复默认值"));
                    const auto subtitle = entry ?
                        std::string{ OverlayAreaLabel(area) } + " · " + entry->texturePath :
                        std::string{ Text("이 부위의 Body Change NG 오버레이 제거",
                            "Remove the Body Change NG overlay from this area",
                            "移除此部位的 Body Change NG 叠加层") };
                    const auto subtitleWidth = (std::max)(0.0F,
                        width - favoriteWidth - Scaled(20.0F));
                    const auto visibleSubtitle = EllipsizeText(subtitle, subtitleWidth);
                    draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)),
                        kCardSubtext, visibleSubtitle.c_str());
                    if (hovered && visibleSubtitle != subtitle) ImGui::SetTooltip("%s", subtitle.c_str());
                    if (entry) {
                        const auto favorite = std::ranges::find(settings.favoriteOverlays, entry->id) !=
                            settings.favoriteOverlays.end();
                        ImGui::SetCursorScreenPos(ImVec2(cursor.x + width - favoriteWidth, cursor.y));
                        if (FavoriteButton(favorite, height)) ToggleOverlayFavorite(entry->id);
                    }
                    if (distributionSelecting && entry && clicked) {
                        SetDistributionOverlaySelected(area, entry->id,
                            !DistributionOverlaySelected(area, entry->id));
                        g_overlayArea = area;
                        g_overlayFocusedIds[areaIndex] = entry->id;
                        FocusCatalogRow(globalRow);
                    } else if (doubleClicked) {
                        g_overlayFocusedIds[areaIndex] = entry ? entry->id : std::string{};
                        FocusCatalogRow(globalRow);
                        applyRow(overlayRow, true);
                    } else if (clicked) {
                        g_overlayFocusedIds[areaIndex] = entry ? entry->id : std::string{};
                        FocusCatalogRow(globalRow);
                        applyRow(overlayRow, false);
                    }
                    ScrollFocusedCatalogRow(globalRow);
                    renderedRowBottom = (std::max)(renderedRowBottom,
                        cursor.y + height + Scaled(5.0F));
                    ImGui::SetCursorScreenPos(ImVec2(rowCursor.x, renderedRowBottom));
                    ImGui::Dummy(ImVec2(0.0F, 0.0F));
                    ImGui::PopID();
                };

                if (!distributionSelecting) {
                    drawRow({ area, nullptr }, row);
                    ++row;
                }
                const auto entryRowBegin = row;
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(visible.size()), Scaled(53.0F));
                if (navigation.hasFocus && navigation.focused >= entryRowBegin &&
                    navigation.focused < entryRowBegin + visible.size()) {
                    clipper.IncludeItemByIndex(static_cast<int>(navigation.focused - entryRowBegin));
                }
                while (clipper.Step()) {
                    for (auto index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                        drawRow({ area, visible[static_cast<std::size_t>(index)] },
                            entryRowBegin + static_cast<std::size_t>(index));
                    }
                }
                const auto sectionCursor = ImGui::GetCursorScreenPos();
                if (renderedRowBottom > sectionCursor.y) {
                    ImGui::SetCursorScreenPos(ImVec2(sectionCursor.x, renderedRowBottom));
                    ImGui::Dummy(ImVec2(0.0F, 0.0F));
                }
                row += visible.size();

                if (visible.empty()) {
                    ImGui::TextWrapped("%s", bcn::overlay::CatalogRequested() ? Text(
                        "RaceMenuBase·SlaveTats 목록을 불러오는 중입니다. RaceMenu 화면을 열 필요는 없습니다.",
                        "Loading RaceMenuBase and SlaveTats entries; opening RaceMenu is not required.",
                        "正在加载 RaceMenuBase 与 SlaveTats 条目；无需打开 RaceMenu。") : Text(
                        "이 액터의 성별과 바디 구조에 맞는 페인트가 없습니다.",
                        "No paints match this actor's sex and body layout.",
                        "没有符合此角色性别与身体结构的绘制。"));
                }

                ImGui::PopID();
                ImGui::TreePop();
            }
        }
        ImGui::EndChild();

        // Keep color controls outside the scrolling catalog. A large
        // RaceMenuBase/SlaveTats section must never push them thousands of
        // rows below the viewport. The last clicked, previewed, or confirmed
        // area owns this footer; Face is the deterministic initial area.
        const auto colorArea = g_overlayArea.value_or(bcn::overlay::Area::face);
        const auto& colorEntryId = g_overlayFocusedIds[bcn::overlay::Index(colorArea)];
        const auto color = bcn::overlay::CurrentColor(actor, colorArea, colorEntryId);
        const auto draft = g_overlayColorDrafts.Find(actor->GetFormID(),
            static_cast<std::uint8_t>(colorArea), colorEntryId);
        // Distribution colors belong to the catalog selection, not to paint
        // already installed on this actor. Even unpreviewable legacy entries
        // can be configured when the selected player uses another body layout.
        const auto distributionColorTarget = distributionSelecting &&
            std::ranges::any_of(entriesByArea[bcn::overlay::Index(colorArea)],
                [&](const auto& entry) { return entry.id == colorEntryId; });
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s: %s", Text("색상 부위", "Color area", "颜色部位"),
            OverlayAreaLabel(colorArea));
        ImGui::SameLine();
        ImGui::BeginDisabled(!color.has_value() && !distributionColorTarget);
        const auto values = bcn::overlay::UnpackColor(draft.value_or(color.value_or(0xFFFFFFFFU)));
        const auto openColor = [&] {
            g_overlayArea = colorArea;
            g_overlayColor = values;
            g_overlayColorActor = actor->GetFormID();
            g_overlayColorArea = colorArea;
            g_overlayColorEntryId = colorEntryId;
            g_overlayColorDistributionDraft = distributionSelecting;
            g_showOverlayDetails = true;
        };
        if (ImGui::ColorButton("##overlayColorPreview",
                ImVec4(values[0], values[1], values[2], values[3]),
                ImGuiColorEditFlags_AlphaPreviewHalf,
                ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) openColor();
        ImGui::SameLine();
        if (ImGui::Button(Text("색상·투명도 조절", "Adjust color and opacity", "调整颜色与不透明度"))) {
            openColor();
        }
        ImGui::SameLine();
        if (ImGui::Button(Text("색상 복원", "Reset color", "还原颜色"))) {
            g_overlayColorDrafts.Set(actor->GetFormID(), static_cast<std::uint8_t>(colorArea),
                colorEntryId, 0xFFFFFFFFU);
            if (!distributionSelecting) {
                const auto result = bcn::overlay::QueueColor(actor, colorArea,
                    colorEntryId, 0xFFFFFFFFU);
                if (result != bcn::overlay::ApplyResult::queued) {
                    bcn::ui::Notify(OverlayApplyResultMessage(result));
                }
            }
        }
        ImGui::EndDisabled();
    }

    void DrawFutanariCatalog()
    {
        auto* actor = SelectedActor();
        if (!bcn::futanari_support::Available()) {
            ImGui::TextUnformatted(Text(
                "지원되는 여성 후타 애드온이 설치되어 있지 않습니다.",
                "No supported female futanari addon is installed.",
                "未安装受支持的女性扶她附加组件。"));
            return;
        }
        const auto distributionSelecting = IsDistributionSelectionFor(DistributionPool::futanari);
        const auto actorType = actor ?
            bcn::futanari_support::RegisteredType(actor) : std::nullopt;

        const auto profiles = bcn::FutanariSkinProfiles::Get().Snapshot();
        const auto actorFamily = actor ?
            bcn::body_family::ResolveActor(actor) : bcn::body_family::Mask{};
        const auto backendCurrentID = actor && !distributionSelecting ?
            bcn::skin_application::CurrentFutanariProfileId(actor).value_or(std::string{}) :
            std::string{};
        const auto currentID = actor && g_pendingFutanari &&
            g_pendingFutanari->actorFormID == actor->GetFormID() ?
            g_pendingFutanari->originalId : backendCurrentID;
        std::vector<const bcn::FutanariSkinProfile*> visible;
        visible.reserve(profiles.size());
        for (const auto& profile : profiles) {
            if (distributionSelecting) {
                if (profile.type == bcn::FutanariSkinType::ubeTrx) continue;
            } else if (!actorType || profile.type != *actorType ||
                !bcn::FutanariSkinTypeMatchesActor(profile.type, actorFamily)) {
                continue;
            }
            if (!g_search.empty() && Lower(profile.name).find(Lower(g_search)) == std::string::npos &&
                Lower(profile.id).find(Lower(g_search)) == std::string::npos) continue;
            visible.push_back(&profile);
        }
        DrawCatalogCommandRow(DistributionPool::futanari,
            [] { RefreshFileCatalog(bcn::FutanariSkinProfiles::Get()); },
            [&visible] {
                g_distributionSelectedIds.clear();
                for (const auto* profile : visible) g_distributionSelectedIds.insert(profile->id);
            },
            Text("Futanari\\<스킨명>\\Textures\\~ 에서 후타스킨을 읽습니다.(더블클릭 적용)",
                "Reads futanari skins from Futanari\\<skin name>\\Textures\\~. (Double-click to apply)",
                "从 Futanari\\<皮肤名称>\\Textures\\~ 读取扶她皮肤。（双击应用）"));
        // Installing a supported female addon enables the feature and its NPC
        // rule editor globally. Manual preview/apply is a separate actor-level
        // concern and must not prevent the user from configuring distribution.
        if (!distributionSelecting && !actor) {
            ImGui::TextUnformatted(Text("액터를 선택하세요.", "Select an actor.", "请选择角色。"));
            return;
        }
        if (!distributionSelecting && !actorType) {
            ImGui::TextUnformatted(Text(
                "이 액터는 SOS 또는 TNG에 여성 후타 액터로 등록되지 않아 대상이 아닙니다.",
                "This actor is not registered as a female futanari actor by SOS or TNG.",
                "该角色未被 SOS 或 TNG 注册为女性扶她角色，因此不是目标。"));
            return;
        }
        const auto hasDefaultRow = !distributionSelecting;
        std::size_t preferredIndex{};
        if (!currentID.empty()) {
            const auto current = std::ranges::find(visible, currentID,
                [](const bcn::FutanariSkinProfile* profile) -> const std::string& {
                    return profile->id;
                });
            if (current != visible.end()) {
                preferredIndex = (hasDefaultRow ? 1U : 0U) +
                    static_cast<std::size_t>(current - visible.begin());
            }
        }
        const auto navigation = HandleCatalogNavigation(
            visible.size() + (hasDefaultRow ? 1U : 0U), preferredIndex);
        const auto applyRow = [&](const std::size_t row, const bool confirm) {
            if (distributionSelecting) {
                if (confirm) {
                    const auto& selectedID = visible[row]->id;
                    SetDistributionItemSelected(selectedID,
                        !DistributionItemSelected(selectedID));
                }
                return;
            }
            const auto selectedID = hasDefaultRow && row == 0U ?
                std::string{} : visible[row - (hasDefaultRow ? 1U : 0U)]->id;
            const auto mode = confirm ?
                bcn::skin_application::FutanariSelectionMode::manual :
                bcn::skin_application::FutanariSelectionMode::preview;
            const auto result = hasDefaultRow && row == 0U ?
                bcn::skin_application::QueueClearFutanari(actor, mode) :
                bcn::skin_application::QueueApplyFutanari(actor, selectedID, mode);
            if (result == bcn::skin_application::ApplyResult::queued) {
                if (confirm) g_pendingFutanari.reset();
                else RememberPending(g_pendingFutanari, actor, selectedID,
                    hasDefaultRow && row == 0U, currentID);
            } else {
                bcn::ui::Notify(SkinApplyResultMessage(result));
            }
        };
        if (navigation.preview) applyRow(navigation.focused, false);
        if (navigation.confirm) applyRow(navigation.focused, true);

        if (ImGui::BeginChild("FutanariCatalog", ImVec2(0.0F, CatalogListHeight()), true,
                ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoNavInputs)) {
            std::size_t row{};
                const auto drawRow = [&](const std::string& id, const std::string& name,
                const std::string& subtitle, const bool current) {
                ImGui::PushID(id.c_str());
                const auto rowCursor = ImGui::GetCursorScreenPos();
                const auto height = Scaled(48.0F);
                if (distributionSelecting) {
                    auto selected = DistributionItemSelected(id);
                    if (CenteredCheckbox("##distributionSelected", selected,
                            rowCursor, height)) {
                        SetDistributionItemSelected(id, selected);
                    }
                }
                const auto cursor = ImGui::GetCursorScreenPos();
                const auto width = ImGui::GetContentRegionAvail().x;
                ImGui::InvisibleButton("item", ImVec2(width, height));
                const auto hovered = ImGui::IsItemHovered();
                const auto clicked = ImGui::IsItemClicked();
                const auto doubleClicked = hovered &&
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                auto* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(cursor, ImVec2(cursor.x + width, cursor.y + height),
                    current ? kCardSelected :
                    hovered || (navigation.hasFocus && navigation.focused == row) ?
                    kCardHovered : kCardNormal, Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)),
                    kCardText, name.c_str());
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)),
                    kCardSubtext, subtitle.c_str());
                if (distributionSelecting && clicked) {
                    SetDistributionItemSelected(id, !DistributionItemSelected(id));
                    FocusCatalogRow(row);
                } else switch (bcn::ui_catalog::MouseIntent(clicked, doubleClicked)) {
                case bcn::ui_catalog::ChoiceIntent::confirm:
                    FocusCatalogRow(row);
                    applyRow(row, true);
                    break;
                case bcn::ui_catalog::ChoiceIntent::preview:
                    FocusCatalogRow(row);
                    applyRow(row, false);
                    break;
                default:
                    break;
                }
                ScrollFocusedCatalogRow(row);
                ImGui::SetCursorScreenPos(ImVec2(rowCursor.x, cursor.y + height + Scaled(5.0F)));
                ImGui::Dummy(ImVec2(0.0F, 0.0F));
                ImGui::PopID();
                ++row;
            };

            if (hasDefaultRow) {
                drawRow("DefaultFutanariSkin",
                    Text("기본 후타나리 스킨", "Default futanari skin", "默认扶她皮肤"),
                    Text("성기 애드온의 기본 텍스처로 복원",
                        "Restore the genital addon's default textures",
                        "恢复生殖器附加组件的默认纹理"), currentID.empty());
            }

            if (visible.empty()) {
                ImGui::TextWrapped("%s", Text(
                    "현재 유형에 맞는 스킨이 없습니다. UBE SOS/TNG는 Textures\\!UBE\\Body, CBBE 3BA+TRX는 Textures\\[TRX] Futa addon\\Regular\\Default, ERF는 Textures\\ERF_Futanari\\FairSkinCBBE 경로를 사용하세요.",
                    "No matching skin was found. Use Textures\\!UBE\\Body for UBE SOS/TNG, Textures\\[TRX] Futa addon\\Regular\\Default for CBBE 3BA+TRX, or Textures\\ERF_Futanari\\FairSkinCBBE for ERF.",
                    "未找到匹配皮肤。UBE SOS/TNG 使用 Textures\\!UBE\\Body，CBBE 3BA+TRX 使用 Textures\\[TRX] Futa addon\\Regular\\Default，ERF 使用 Textures\\ERF_Futanari\\FairSkinCBBE。"));
            }
            for (const auto* profile : visible) {
                auto subtitle = bcn::FutanariSkinTypeLabel(profile->type) + " · " +
                    Text("텍스처 ", "Textures ", "纹理 ") + std::to_string(profile->layers.size()) +
                    Text("개", "", " 个");
                if (profile->id == currentID) {
                    subtitle += " · ";
                    subtitle += Text("현재 적용", "Current", "当前应用");
                }
                drawRow(profile->id, profile->name, subtitle, profile->id == currentID);
            }
        }
        ImGui::EndChild();
    }

    [[nodiscard]] std::vector<bcn::player_tint::PersistedLayerState> TintDraftsForPack(
        const std::string_view pack)
    {
        std::vector<bcn::player_tint::PersistedLayerState> result;
        for (auto& entry : g_tintColorDrafts.Entries(g_selectedActorFormID)) {
            if (entry.id.starts_with(pack) && entry.id.size() > pack.size() &&
                (entry.id[pack.size()] == '\\' || entry.id[pack.size()] == '/')) {
                result.push_back(std::move(entry.color));
            }
        }
        return result;
    }

    void RememberTintColor(const bool restored = false)
    {
        g_tintColorDrafts.Set(g_selectedActorFormID,
            static_cast<std::uint8_t>(g_selectedTintLayer), g_selectedTintAssetID,
            bcn::player_tint::PersistedLayerState{
                .layer = g_selectedTintLayer, .restored = restored,
                .assetID = g_selectedTintAssetID,
                .color = { g_tintColor[0], g_tintColor[1], g_tintColor[2], g_tintColor[3] }
            });
    }

    void ReadTintColorDraft()
    {
        const auto draft = g_tintColorDrafts.Find(g_selectedActorFormID,
            static_cast<std::uint8_t>(g_selectedTintLayer), g_selectedTintAssetID);
        const auto color = draft ? std::optional{ draft->color } :
            bcn::player_tint::CurrentColor(g_selectedTintLayer);
        if (color) g_tintColor = { color->red, color->green, color->blue, color->alpha };
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
            RefreshFileCatalog(bcn::player_tint::Catalog::Get());
        }
        ImGui::SameLine();
        bcn::ui_text::FittedDisabledLine(Text(
            "BodySkin\\<스킨팩>\\textures\\~에서 틴트마스크를 읽습니다.(더블클릭 적용)",
            "Reads tint masks from BodySkin\\<skin pack>\\textures\\~. (Double-click to apply)",
            "从 BodySkin\\<皮肤包>\\textures\\~ 读取色调蒙版。（双击应用）"));

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
            if (!bcn::player_tint::TintAssetMatchesActor(asset.sex,
                    asset.bodyFamilies, actorFamily, female)) continue;
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
            const auto baseline = !confirm && !g_pendingTintBaseline ?
                std::optional{ bcn::player_tint::SnapshotPersistedState() } : std::nullopt;
            if (baseline) bcn::player_tint::BeginPreview();
            const auto result = bcn::player_tint::QueueRestoreAll(confirm);
            if (result == bcn::player_tint::ApplyResult::queued) {
                if (confirm) {
                    g_pendingTint.reset();
                    g_pendingTintBaseline.reset();
                } else {
                    if (baseline) g_pendingTintBaseline = *baseline;
                    RememberPending(g_pendingTint, selectedActor, {}, true, confirmedTintPack);
                }
                g_currentTintPack.clear();
                g_selectedTintPack.clear();
            } else {
                if (baseline) bcn::player_tint::RestorePersistedState(*baseline, false);
                bcn::ui::Notify(TintResultText(result));
            }
        };
        const auto selectPack = [&](const std::size_t row, const bool confirm) {
            const auto& pack = *visiblePacks[row - 1U];
            g_selectedTintPack = pack.name;
            const auto baseline = !confirm && !g_pendingTintBaseline ?
                std::optional{ bcn::player_tint::SnapshotPersistedState() } : std::nullopt;
            if (baseline) bcn::player_tint::BeginPreview();
            const auto committedState = bcn::player_tint::SnapshotPersistedState();
            for (const auto& asset : assets) {
                if (asset.pack != pack.name) continue;
                const auto layer = static_cast<std::uint8_t>(asset.layer);
                if (!g_tintColorDrafts.Find(g_selectedActorFormID, layer, asset.id)) {
                    if (const auto color = g_tintSessionColors[layer]) {
                        const auto restored = committedState.pack == pack.name &&
                            std::ranges::any_of(committedState.layers, [&](const auto& value) {
                                return value.layer == asset.layer && value.restored;
                            });
                        g_tintColorDrafts.Set(g_selectedActorFormID, layer, asset.id,
                            bcn::player_tint::PersistedLayerState{
                                .layer = asset.layer, .restored = restored, .assetID = asset.id, .color = *color });
                    }
                }
            }
            const auto result = bcn::player_tint::QueueApplyPack(pack.name, TintDraftsForPack(pack.name), confirm);
            if (result == bcn::player_tint::ApplyResult::queued) {
                if (confirm) {
                    g_pendingTint.reset();
                    g_pendingTintBaseline.reset();
                } else {
                    if (baseline) g_pendingTintBaseline = *baseline;
                    RememberPending(g_pendingTint, selectedActor, pack.name, false, confirmedTintPack);
                }
                g_currentTintPack = pack.name;
            } else {
                if (baseline) bcn::player_tint::RestorePersistedState(*baseline, false);
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
                confirmedTintPack.empty() ? kCardSelected :
                hovered || (navigation.hasFocus && navigation.focused == row) ?
                kCardHovered : kCardNormal, Scaled(4.0F));
            draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), kCardText,
                Text("기본 틴트", "Default tint", "默认色调"));
            draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)), kCardSubtext,
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
                ImGui::TextUnformatted(Text(
                    "플레이어에게 사용할 수 있는 틴트마스크팩이 없습니다.",
                    "No tint-mask packs are available for the player.",
                    "未找到可供玩家使用的色调蒙版包。"));
                ImGui::TextWrapped("%s", Text(
                    "BodySkin\\<스킨팩>\\textures\\actors\\character\\character assets\\tintmasks\\*.dds에 RaceMenu 얼굴 틴트마스크 DDS를 넣고 새로고침하세요.",
                    "Place RaceMenu facial tint-mask DDS files in BodySkin\\<skin pack>\\textures\\actors\\character\\character assets\\tintmasks\\*.dds, then press Refresh.",
                    "请将 RaceMenu 面部色调蒙版 DDS 文件放入 BodySkin\\<皮肤包>\\textures\\actors\\character\\character assets\\tintmasks\\*.dds，然后点击‘刷新’。"));
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
                    kCardSelected : hovered || (navigation.hasFocus && navigation.focused == row) ?
                    kCardHovered : kCardNormal, Scaled(4.0F));
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(7.0F)), kCardText, pack.name.c_str());
                const auto sub = std::string{ female ? Text("여성", "Female", "女性") : Text("남성", "Male", "男性") } +
                    " · " + bcn::player_tint::TintFamilyLabel(pack.bodyFamilies) +
                    " · DDS " + std::to_string(pack.count) + Text("개", "", " 个");
                draw->AddText(ImVec2(cursor.x + Scaled(10.0F), cursor.y + Scaled(27.0F)), kCardSubtext, sub.c_str());
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
            ReadTintColorDraft();
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
                        ReadTintColorDraft();
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
                if (const auto color = bcn::player_tint::OriginalColor(g_selectedTintLayer)) {
                    g_tintColor = { color->red, color->green, color->blue, color->alpha };
                    RememberTintColor(true);
                }
                const auto result = bcn::player_tint::QueueApplyPack(g_selectedTintPack,
                    TintDraftsForPack(g_selectedTintPack), !g_pendingTint.has_value());
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

    void DrawOverlayDetailPopup()
    {
        if (!g_showOverlayDetails) return;
        const auto title = std::string{ Text("오버레이 색상", "Overlay color", "覆盖层颜色") } + "###OverlayDetails";
        if (!ImGui::IsPopupOpen(title.c_str())) ImGui::OpenPopup(title.c_str());
        ImGui::SetNextWindowSize(ImVec2(Scaled(360.0F), 0.0F), ImGuiCond_Appearing);
        if (BeginUndimmedPopupModal(title.c_str(), &g_showOverlayDetails, ImGuiWindowFlags_AlwaysAutoResize,
                bcn::popup_placement::Kind::overlayColor)) {
            auto* actor = SelectedActor();
            if (EscapePressed() || !actor || actor->GetFormID() != g_overlayColorActor ||
                g_activeTab != ActiveTab::overlay) {
                g_showOverlayDetails = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::Text("%s", OverlayAreaLabel(g_overlayColorArea));
            ImGui::SetNextItemWidth(Scaled(300.0F));
            const auto changed = ImGui::ColorPicker4(Text("색상 및 강도", "Color and opacity", "颜色与不透明度"),
                g_overlayColor.data(), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
            const auto finished = ImGui::IsItemDeactivatedAfterEdit();
            const auto done = RightAlignedButton(Text("완료", "Done", "完成"));
            const auto now = std::chrono::steady_clock::now();
            if (changed || finished || done) {
                g_overlayColorDrafts.Set(g_overlayColorActor, static_cast<std::uint8_t>(g_overlayColorArea),
                    g_overlayColorEntryId, bcn::overlay::PackColor(g_overlayColor));
            }
            // Batch selection edits only its per-entry draft. Do not mutate a
            // confirmed player paint (or allocate a preview slot) just to pick
            // the color that a future NPC distribution rule will use.
            if (!g_overlayColorDistributionDraft &&
                ((changed && now - g_lastOverlayColorApply >= std::chrono::milliseconds(100)) || finished || done)) {
                const auto result = bcn::overlay::QueueColor(actor, g_overlayColorArea,
                    g_overlayColorEntryId, bcn::overlay::PackColor(g_overlayColor));
                if (result == bcn::overlay::ApplyResult::queued) g_lastOverlayColorApply = now;
                else bcn::ui::Notify(OverlayApplyResultMessage(result));
            }
            if (done) {
                g_showOverlayDetails = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawTintDetailPopup()
    {
        if (!g_showTintDetails) return;
        const auto title = std::string{ Text("틴트 상세 값", "Tint details", "色调详情") } + "###TintDetails";
        if (!ImGui::IsPopupOpen(title.c_str())) ImGui::OpenPopup(title.c_str());
        ImGui::SetNextWindowSize(ImVec2(Scaled(360.0F), 0.0F), ImGuiCond_Appearing);
        if (BeginUndimmedPopupModal(title.c_str(), &g_showTintDetails, ImGuiWindowFlags_AlwaysAutoResize,
                bcn::popup_placement::Kind::tintColor)) {
            if (EscapePressed()) {
                g_showTintDetails = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::SetNextItemWidth(Scaled(300.0F));
            const auto colorChanged = ImGui::ColorPicker4(Text("색상 및 강도", "Color and opacity", "颜色与不透明度"),
                g_tintColor.data(), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
            const auto colorFinished = ImGui::IsItemDeactivatedAfterEdit();
            const auto done = RightAlignedButton(Text("완료", "Done", "完成"));
            if (colorChanged || colorFinished) RememberTintColor();
            const auto now = std::chrono::steady_clock::now();
            const auto liveUpdateDue = colorChanged &&
                now - g_lastTintDetailApply >= std::chrono::milliseconds(100);
            if ((liveUpdateDue || colorFinished || done) && !g_selectedTintAssetID.empty()) {
                const auto result = bcn::player_tint::QueueApplyPack(g_selectedTintPack,
                    TintDraftsForPack(g_selectedTintPack), !g_pendingTint.has_value());
                if (result == bcn::player_tint::ApplyResult::queued) {
                    g_lastTintDetailApply = now;
                } else {
                    bcn::ui::Notify(TintResultText(result));
                }
            }
            if (done) {
                g_showTintDetails = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawDistributionPopup()
    {
        if (!g_showDistribution) return;
        EnsureDistributionEditor();
        SynchronizeDistributionRuleNames();

        const auto popupTitle = std::string{ DistributionPoolLabel(g_distributionPool) } + " · " +
            Text("NPC 배포 조건", "NPC distribution conditions", "NPC 分发条件") +
            "###DistributionPopup";
        ImGui::OpenPopup(popupTitle.c_str());
        auto popupSize = DefaultWindowSize(760.0F, 700.0F);
        if (const auto* viewport = ImGui::GetMainViewport()) {
            popupSize.x = (std::min)(popupSize.x, viewport->WorkSize.x * 0.94F);
            popupSize.y = (std::min)(popupSize.y, viewport->WorkSize.y * 0.92F);
        }
        ImGui::SetNextWindowSize(popupSize, ImGuiCond_Appearing);
        constexpr std::array distributionPlacements{
            bcn::popup_placement::Kind::distributionBody, bcn::popup_placement::Kind::distributionSkin,
            bcn::popup_placement::Kind::distributionFutanari, bcn::popup_placement::Kind::distributionOverlay
        };
        if (BeginUndimmedPopupModal(popupTitle.c_str(), &g_showDistribution,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse,
                distributionPlacements[static_cast<std::size_t>(g_distributionPool)])) {
            const auto closeEditor = [&] {
                DiscardDistributionDraft();
                g_showDistribution = false;
                ImGui::CloseCurrentPopup();
            };
            if (EscapePressed()) {
                closeEditor();
                ImGui::EndPopup();
                return;
            }

            ImGui::TextWrapped("%s", (std::string{ Text(
                "선택한 ", "Only the selected ", "仅将所选") } +
                DistributionPoolLabel(g_distributionPool) +
                Text("만 이 조건에 맞는 NPC에게 배포합니다. 비어 있는 다른 기능은 건드리지 않습니다.",
                    " items are distributed to matching NPCs. Other empty features are left unchanged.",
                    "项目分发给符合条件的 NPC；其他空白功能保持不变。")).c_str());
            ImGui::Separator();

            const auto relevantIndices = [&] {
                std::vector<std::size_t> result;
                result.reserve(g_distributionRules.size());
                for (std::size_t index{}; index < g_distributionRules.size(); ++index) {
                    if (RuleUsesDistributionPool(g_distributionRules[index], g_distributionPool) ||
                        (index == g_selectedDistributionRule &&
                            !RuleHasAnyDistributionPool(g_distributionRules[index]))) {
                        result.push_back(index);
                    }
                }
                return result;
            };

            if (g_selectedDistributionRule >= g_distributionRules.size() &&
                !g_distributionRules.empty()) {
                const auto visible = relevantIndices();
                g_selectedDistributionRule = visible.empty() ? 0U : visible.front();
            }

            if (g_selectedDistributionRule < g_distributionRules.size()) {
                auto& rule = g_distributionRules[g_selectedDistributionRule];
                ImGui::PushID(rule.id.c_str());
                ImGui::TextUnformatted(Text("규칙 이름", "Rule name", "规则名称"));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0F);
                if (ImGui::InputText("##ruleName", &rule.name)) rule.nameKey.clear();

                ImGui::TextUnformatted(Text("성별", "Sex", "性别"));
                ImGui::SameLine();
                if (g_distributionPool == DistributionPool::futanari) {
                    rule.female = true;
                    ImGui::TextDisabled("%s", Text("여성 후타 NPC (고정)",
                        "Female futanari NPCs (fixed)", "女性扶她 NPC（固定）"));
                } else {
                    int sex = rule.female ? 0 : 1;
                    ImGui::SetNextItemWidth(Scaled(170.0F));
                    PrepareResizableDropdown(2U);
                    if (ImGui::Combo("##ruleSex", &sex,
                            Text("여성\0남성\0", "Female\0Male\0", "女性\0男性\0"))) {
                        const auto oldNameKey = rule.nameKey;
                        if (bcn::SetDistributionRuleSex(rule, sex == 0)) {
                            if (const auto retargeted =
                                    bcn::distribution_names::RetargetGeneratedRuleKey(
                                        oldNameKey, rule.female);
                                !retargeted.empty()) {
                                rule.nameKey = retargeted;
                                rule.name = bcn::distribution_names::Localized(
                                    retargeted, CurrentLanguage());
                            } else {
                                rule.nameKey.clear();
                            }
                            bcn::ui::Notify(Text(
                                "성별이 바뀌어 이전 성별의 배포 항목 선택을 비웠습니다. 해당 성별 액터 목록에서 항목을 다시 선택하세요.",
                                "Changing sex cleared the previous sex's selected items. Select items again from an actor of that sex.",
                                "性别已更改，原性别的分发项目已清空。请从该性别角色的列表中重新选择项目。"));
                        }
                    }
                }

                ImGui::SameLine();
                ImGui::TextUnformatted(Text("대상", "Target", "目标"));
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0F);
                if (DistributionScopeCombo(rule.scope)) {
                    rule.target.clear();
                    rule.npcBaseFormID = 0U;
                    rule.npcPlugin.clear();
                    rule.npcLocalFormID = 0U;
                    rule.targetFormID = 0U;
                    rule.targetPlugin.clear();
                    rule.targetLocalFormID = 0U;
                    FillRuleTargetFromSelectedActor(rule);
                }

                if (rule.scope == bcn::DistributionScope::allNPCs) {
                    auto excludeCustomFollowers = !rule.includeCustomFollowers;
                    if (ImGui::Checkbox(Text("커스텀 팔로워 제외", "Exclude custom followers",
                            "排除自定义随从"), &excludeCustomFollowers)) {
                        rule.includeCustomFollowers = !excludeCustomFollowers;
                    }
                    ImGui::SameLine();
                    auto excludeElderNPCs = !rule.includeElderNPCs;
                    if (ImGui::Checkbox(Text("노인 NPC 제외", "Exclude elder NPCs",
                            "排除老年 NPC"), &excludeElderNPCs)) {
                        rule.includeElderNPCs = !excludeElderNPCs;
                    }
                } else if (rule.scope == bcn::DistributionScope::npcBaseForm) {
                    ImGui::TextUnformatted("FormID");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0F);
                    if (ImGui::InputScalar("##ruleFormID", ImGuiDataType_U32,
                            &rule.npcBaseFormID, nullptr, nullptr, "%08X",
                            ImGuiInputTextFlags_CharsHexadecimal |
                                ImGuiInputTextFlags_CharsUppercase)) {
                        if (auto* form = RE::TESForm::LookupByID(rule.npcBaseFormID);
                            !bcn::SetDistributionRuleNPC(rule, form)) {
                            rule.npcPlugin.clear();
                            rule.npcLocalFormID = 0U;
                        }
                    }
                } else if (rule.scope == bcn::DistributionScope::npcName) {
                    ImGui::TextUnformatted(TargetLabel(rule.scope));
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0F);
                    ImGui::InputText("##ruleTarget", &rule.target);
                } else {
                    ImGui::TextUnformatted(TargetLabel(rule.scope));
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.0F);
                    switch (rule.scope) {
                    case bcn::DistributionScope::factionEditorID: {
                        [[maybe_unused]] const auto factionChanged =
                            DistributionFormTargetCombo("##ruleFaction", rule,
                                g_distributionFactionOptions);
                        break;
                    }
                    case bcn::DistributionScope::pluginFile: {
                        [[maybe_unused]] const auto pluginChanged =
                            DistributionTargetCombo("##rulePlugin", rule.target,
                                g_distributionPluginOptions);
                        break;
                    }
                    case bcn::DistributionScope::raceEditorID: {
                        [[maybe_unused]] const auto raceChanged =
                            DistributionFormTargetCombo("##ruleRace", rule,
                                g_distributionRaceOptions);
                        break;
                    }
                    case bcn::DistributionScope::keyword: {
                        [[maybe_unused]] const auto keywordChanged =
                            DistributionFormTargetCombo("##ruleKeyword", rule,
                                g_distributionKeywordOptions);
                        break;
                    }
                    case bcn::DistributionScope::npcClass: {
                        [[maybe_unused]] const auto classChanged =
                            DistributionFormTargetCombo("##ruleClass", rule,
                                g_distributionClassOptions);
                        break;
                    }
                    default:
                        ImGui::TextDisabled("%s", DistributionScopeLabel(rule.scope));
                        break;
                    }
                }

                ImGui::TextDisabled("%s: %zu", DistributionPoolLabel(g_distributionPool),
                    RuleDistributionPoolCount(rule, g_distributionPool));
                ImGui::PopID();
            } else {
                ImGui::TextDisabled("%s", Text(
                    "규칙을 추가하세요.", "Add a rule.", "请添加规则。"));
            }

            ImGui::Separator();
            if (ImGui::Button(Text("+ 규칙 추가", "+ Add rule", "+ 添加规则"))) {
                auto rule = NewDistributionRule();
                SetRuleDistributionSelection(rule);
                g_distributionRules.push_back(std::move(rule));
                g_selectedDistributionRule = g_distributionRules.size() - 1U;
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("- 규칙 삭제", "- Delete rule", "- 删除规则")) &&
                g_selectedDistributionRule < g_distributionRules.size()) {
                g_distributionRules.erase(g_distributionRules.begin() +
                    static_cast<std::ptrdiff_t>(g_selectedDistributionRule));
                const auto visible = relevantIndices();
                g_selectedDistributionRule = visible.empty() ? 0U : visible.front();
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("위로", "Up", "上移"))) {
                const auto visible = relevantIndices();
                const auto current = std::ranges::find(visible,
                    g_selectedDistributionRule);
                if (current != visible.end() && current != visible.begin()) {
                    const auto other = *(current - 1);
                    std::swap(g_distributionRules[g_selectedDistributionRule],
                        g_distributionRules[other]);
                    g_selectedDistributionRule = other;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("아래로", "Down", "下移"))) {
                const auto visible = relevantIndices();
                const auto current = std::ranges::find(visible,
                    g_selectedDistributionRule);
                if (current != visible.end() && current + 1 != visible.end()) {
                    const auto other = *(current + 1);
                    std::swap(g_distributionRules[g_selectedDistributionRule],
                        g_distributionRules[other]);
                    g_selectedDistributionRule = other;
                }
            }

            ImGui::TextUnformatted(Text("지정한 조건", "Configured conditions", "已设置条件"));
            const auto footerHeight = ImGui::GetFrameHeightWithSpacing() + Scaled(12.0F);
            const auto listHeight = (std::max)(Scaled(150.0F),
                ImGui::GetContentRegionAvail().y - footerHeight);
            if (ImGui::BeginChild("DistributionRuleList", ImVec2(0.0F, listHeight), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                const auto visible = relevantIndices();
                for (const auto index : visible) {
                    const auto& rule = g_distributionRules[index];
                    const auto detail = std::to_string(index + 1U) + "  " + rule.name +
                        "\n    " + DistributionScopeLabel(rule.scope) + " · " +
                        std::to_string(RuleDistributionPoolCount(rule,
                            g_distributionPool)) +
                        Text("개 선택", " selected", " 个已选");
                    if (ImGui::Selectable(detail.c_str(),
                            index == g_selectedDistributionRule,
                            ImGuiSelectableFlags_AllowDoubleClick)) {
                        g_selectedDistributionRule = index;
                    }
                }
                if (visible.empty()) {
                    ImGui::TextDisabled("%s", Text("이 기능의 규칙이 없습니다.",
                        "No rules exist for this feature.", "此功能没有规则。"));
                }
            }
            ImGui::EndChild();

            if (ImGui::Button(Text("로드된 NPC 즉시 배포",
                    "Distribute to loaded NPCs now", "立即分发给已加载的 NPC"))) {
                if (SaveActiveDistributionRules()) {
                    const auto queued = bcn::Distribution::Get().ApplyLoadedNPCs();
                    bcn::ui::Notify(std::to_string(queued) + Text(
                        "명의 변경 대상 NPC를 확인하고 규칙을 저장했습니다.",
                        " changed loaded NPCs were checked and the rules were saved.",
                        " 名已加载 NPC 的变更已检查，规则也已保存。"));
                } else {
                    bcn::ui::Notify(Text(
                        "NPC 배포 규칙을 저장하지 못해 즉시 배포하지 않았습니다.",
                        "The rules could not be saved, so immediate distribution was not started.",
                        "无法保存 NPC 分发规则，因此未开始立即分发。"));
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("다음 게임 실행 시 배포",
                    "Distribute on next game launch", "下次启动游戏时分发"))) {
                if (bcn::Distribution::Get().SaveRulesForNextGame(g_distributionRules)) {
                    bcn::ui::Notify(Text(
                        "현재 편집 값을 다음 게임 실행용으로 저장했습니다.",
                        "Saved the edited values for the next game launch.",
                        "已保存当前编辑值，供下次启动游戏时使用。"));
                } else {
                    bcn::ui::Notify(Text(
                        "다음 게임 실행용 배포 규칙을 저장하지 못했습니다.",
                        "Could not save the distribution rules for the next game start.",
                        "无法保存下次启动游戏时使用的分发规则。"));
                }
            }
            const auto* closeLabel = Text("닫기", "Close", "关闭");
            const auto closeWidth = ImGui::CalcTextSize(closeLabel).x +
                ImGui::GetStyle().FramePadding.x * 2.0F;
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - closeWidth);
            if (ImGui::Button(closeLabel)) closeEditor();
            ImGui::EndPopup();
        }
        if (!g_showDistribution) {
            DiscardDistributionDraft();
        }
    }
    void DrawOutfitPopup()
    {
        if (!g_showOutfit) return;
        const auto popupTitle = std::string{ Text("의상·랜덤화", "Outfit · randomization", "服装·随机化") } + "###OutfitPopup";
        ImGui::OpenPopup(popupTitle.c_str());
        // Give wrapped text its final width in the first measuring frame.
        // Fully automatic width starts almost at zero, creating a very tall
        // temporary popup whose centered Y would then be saved near the top.
        const auto outfitBounds = DefaultWindowSize(700.0F, 1200.0F);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(outfitBounds.x, 0.0F), outfitBounds);
        if (BeginUndimmedPopupModal(popupTitle.c_str(), &g_showOutfit, ImGuiWindowFlags_AlwaysAutoResize,
                bcn::popup_placement::Kind::outfit)) {
            if (EscapePressed()) {
                g_showOutfit = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::TextDisabled("%s", Text("지원되는 BodySlide 슬라이더에만 적용", "Only applies to supported BodySlide sliders", "仅适用于受支持的 BodySlide 滑块"));
            auto* player = RE::PlayerCharacter::GetSingleton();
            const auto playerUbe = player &&
                (bcn::body_family::ResolveActor(player) &
                    bcn::body_family::Bit(bcn::body_family::Family::ube)) != 0U;
            const auto selectedFamily = bcn::body_morph_policy::ResolveFemaleFamily(
                bcn::body_family::ResolveActor(SelectedActor()));
            const auto selectedUbe = selectedFamily == bcn::body_morph_policy::FemaleFamily::ube;
            if (playerUbe) {
                ImGui::TextColored(ImVec4(.95F, .72F, .32F, 1.0F), "%s", Text(
                    "UBE 플레이어에는 가슴·유두 보정을 적용하지 않습니다. 활성화된 보정은 지원되는 NPC에만 적용됩니다.",
                    "Breast/nipple correction skips the UBE player. The enabled corrections apply only to supported NPCs.",
                    "胸部/乳头修正会跳过 UBE 玩家；已启用的修正仅应用于受支持的 NPC。"));
            } else if (selectedUbe) {
                ImGui::TextColored(ImVec4(.95F, .72F, .32F, 1.0F), "%s", Text(
                    "선택한 UBE 액터에는 가슴·유두 보정과 NPC 신체 무작위화를 적용하지 않습니다.",
                    "Breast/nipple correction and NPC anatomy randomization are disabled for the selected UBE actor.",
                    "不会对所选 UBE 角色应用胸部/乳头修正或 NPC 身体随机化。"));
            }
            ImGui::Separator();
            auto settings = bcn::Settings::Get().Snapshot();
            // These settings remain enabled for supported NPCs even when the
            // player uses UBE. Normalize an older disabled setting once, then
            // present both controls as checked/read-only for that environment.
            const auto normalizedNpcCorrection = playerUbe &&
                (!settings.orefitEnabled || !settings.orefitNippleMorphing);
            if (playerUbe) {
                settings.orefitEnabled = true;
                settings.orefitNippleMorphing = true;
                ImGui::BeginDisabled();
            }
            auto settingsChanged = normalizedNpcCorrection;
            const auto refitChanged = ImGui::Checkbox(Text("의상 착용 시 가슴 보정", "Correct breasts while clothed", "穿衣时修正胸部"), &settings.orefitEnabled);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", Text(
                "CBBE 3BA와 BHUNP/UNP를 지원합니다. UBE 플레이어에는 적용하지 않고 지원되는 NPC에만 적용합니다.",
                "Supports CBBE 3BA and BHUNP/UNP. The UBE player is skipped and supported NPCs continue to receive it.",
                "支持 CBBE 3BA 与 BHUNP/UNP；会跳过 UBE 玩家，并继续应用于受支持的 NPC。"));
            settingsChanged |= refitChanged;
            ImGui::Indent();
            if (!settings.orefitEnabled) ImGui::BeginDisabled();
            const auto nippleRefitChanged = ImGui::Checkbox(Text("의상 착용 시 유두 보정", "Correct nipples while clothed", "穿衣时修正乳头"), &settings.orefitNippleMorphing);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip | ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", Text(
                "CBBE 3BA와 BHUNP/UNP를 지원합니다. UBE 플레이어에는 적용하지 않고 지원되는 NPC에만 적용합니다.",
                "Supports CBBE 3BA and BHUNP/UNP. The UBE player is skipped and supported NPCs continue to receive it.",
                "支持 CBBE 3BA 与 BHUNP/UNP；会跳过 UBE 玩家，并继续应用于受支持的 NPC。"));
            settingsChanged |= nippleRefitChanged;
            if (!settings.orefitEnabled) ImGui::EndDisabled();
            ImGui::Unindent();
            if (playerUbe) ImGui::EndDisabled();
            if (ImGui::Button(Text("OBody NG 의상 보정 규칙 등록", "Register OBody NG outfit-correction rules", "注册 OBody NG 服装修正规则"))) {
                const auto report = bcn::OutfitRefit::Get().LoadOBodyRules();
                if (report.loaded) {
                    g_orefitRulesRegistered = true;
                    const auto processed = bcn::OutfitRefit::Get().ProcessLoadedActors();
                    const auto excluded = report.excludedNames + report.excludedPlugins + report.excludedFormIDs;
                    const auto forced = report.forcedNames + report.forcedFormIDs;
                    const auto mappings = report.femaleMappings + report.maleMappings;
                    bcn::ui::Notify(
                        std::to_string(excluded) + Text("개 제외, ", " exclusions, ", " 条排除、") +
                        std::to_string(forced) + Text("개 강제 보정, ", " force-refit entries, ", " 条强制修正、") +
                        std::to_string(mappings) + Text("개 프리셋 매핑을 등록하고 ", " preset mappings registered; ", " 条预设映射已注册；") +
                        std::to_string(processed) + Text(
                            "명의 로드된 액터를 다시 판정했습니다.",
                            " loaded actors were re-evaluated.",
                            " 名已加载角色已重新判定。"));
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
            TextDisabledWrapped(Text(
                "파일 경로: Data\\SKSE\\Plugins\\OBody_presetDistributionConfig.json",
                "File path: Data\\SKSE\\Plugins\\OBody_presetDistributionConfig.json",
                "文件路径：Data\\SKSE\\Plugins\\OBody_presetDistributionConfig.json"));
            ImGui::Separator();
            const auto nippleRandomizationChanged = ImGui::Checkbox(
                Text("NPC 유두 형태 무작위화", "Randomize NPC nipple shape", "随机 NPC 乳头形态"),
                &settings.nippleRandomization);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", Text(
                "CBBE 3BA와 BHUNP/UNP NPC를 지원합니다. 플레이어와 UBE NPC에는 적용되지 않습니다.",
                "Supports CBBE 3BA and BHUNP/UNP NPCs. The player and UBE NPCs are skipped.",
                "支持 CBBE 3BA 与 BHUNP/UNP NPC；会跳过玩家和 UBE NPC。"));
            const auto genitalRandomizationChanged = ImGui::Checkbox(
                Text("NPC 생식기 형태 무작위화", "Randomize NPC genital shape", "随机 NPC 生殖器形态"),
                &settings.genitalRandomization);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", Text(
                "CBBE 3BA와 BHUNP/UNP NPC를 지원합니다. 플레이어와 UBE NPC에는 적용되지 않습니다.",
                "Supports CBBE 3BA and BHUNP/UNP NPCs. The player and UBE NPCs are skipped.",
                "支持 CBBE 3BA 与 BHUNP/UNP NPC；会跳过玩家和 UBE NPC。"));
            settingsChanged |= nippleRandomizationChanged || genitalRandomizationChanged;
            if (settingsChanged) {
                bcn::Settings::Get().Update(settings);
                if (!bcn::Settings::Get().Save()) {
                    bcn::ui::Notify(Text("의상·랜덤화 설정을 저장하지 못했습니다.", "Could not save outfit and randomization settings.", "无法保存服装与随机化设置。"));
                }
                if (normalizedNpcCorrection || refitChanged || nippleRefitChanged) {
                    bcn::OutfitRefit::Get().ProcessActor(SelectedActor());
                }
                // The popup has no separate Apply button. Rebuild the owned
                // committed key immediately so disabling randomization also
                // removes values that were generated by the previous state.
                if (nippleRandomizationChanged || genitalRandomizationChanged) {
                    bcn::racemenu::QueueReapplyCurrent(SelectedActor());
                }
                if (normalizedNpcCorrection || refitChanged || nippleRefitChanged || nippleRandomizationChanged ||
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
                ImGuiWindowFlags_NoScrollWithMouse, bcn::popup_placement::Kind::settings)) {
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
            TextDisabledWrapped(Text("3인칭에서 선택한 액터를 창 옆에 임시 배치합니다. 캐릭터가 있는 화면 바깥쪽을 우클릭 드래그하거나 게임패드 LT를 누른 채 RS를 좌우로 움직이면 회전하며, 대상 변경·창 닫기 때 카메라와 방향을 복원합니다.", "In third person, temporarily frames the selected actor beside the window. Right-drag the outer character area, or hold gamepad LT and move RS left or right, to rotate; camera and facing restore when the target changes or the window closes.", "第三人称下会临时将所选角色置于窗口旁。右键拖动角色所在的外侧区域，或按住手柄 LT 并左右推动 RS，即可旋转；切换目标或关闭窗口时会恢复镜头和朝向。"));
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
            ImGui::Separator();
            ImGui::TextUnformatted(Text("NPC 배포 바디 타입", "NPC distribution body type", "NPC 分发身体类型"));
            int femaleNpcBodyType = static_cast<int>(settings.femaleNpcBodyType);
            ImGui::SetNextItemWidth(Scaled(300.0F));
            PrepareResizableDropdown(4U);
            if (ImGui::Combo(Text("여성##femaleNpcBodyType", "Female##femaleNpcBodyType", "女性##femaleNpcBodyType"),
                &femaleNpcBodyType,
                Text("CBBE 3BA\0BHUNP / UNP\0UBE\0바닐라\0",
                    "CBBE 3BA\0BHUNP / UNP\0UBE\0Vanilla\0",
                    "CBBE 3BA\0BHUNP / UNP\0UBE\0原版\0"))) {
                settings.femaleNpcBodyType = static_cast<bcn::FemaleNpcBodyType>(femaleNpcBodyType);
                settingsChanged = true;
            }
            int maleNpcBodyType = static_cast<int>(settings.maleNpcBodyType);
            ImGui::SetNextItemWidth(Scaled(300.0F));
            PrepareResizableDropdown(3U);
            if (ImGui::Combo(Text("남성##maleNpcBodyType", "Male##maleNpcBodyType", "男性##maleNpcBodyType"),
                &maleNpcBodyType,
                Text("HIMBO\0SAM\0바닐라\0", "HIMBO\0SAM\0Vanilla\0", "HIMBO\0SAM\0原版\0"))) {
                settings.maleNpcBodyType = static_cast<bcn::MaleNpcBodyType>(maleNpcBodyType);
                settingsChanged = true;
            }
            TextDisabledWrapped(Text(
                "NPC 자동 배포의 바디 프리셋·바디스킨 후보를 이 계열로 제한합니다. 바닐라는 해당 성별의 Body Change NG 바디 모프를 제거하며, 직접 선택과 메인 목록의 액터 자동 감지는 바뀌지 않습니다.",
                "Limits automatic NPC distribution body-preset and body-skin candidates to this family. Vanilla removes Body Change NG body morphs for that sex. Direct selection and actor detection in the main list are unchanged.",
                "将 NPC 自动分发的身体预设和身体皮肤候选项限制为该系列。选择原版会移除该性别的 Body Change NG 身体形态。直接选择和主列表中的角色自动检测不受影响。"));
            settingsChanged |= ImGui::Checkbox(Text("성능 모드", "Performance mode", "性能模式"), &settings.performanceMode);
            TextDisabledWrapped(Text(
                "자동 NPC 작업은 설정과 관계없이 중복을 합쳐 한 액터씩 안전하게 처리합니다. 켜면 액터 사이에 처리 간격을 한 번 더 두며, 새로 나타난 NPC는 세이브 로드 대량 작업보다 항상 우선합니다. 직접 선택은 즉시 처리되고 최종 결과는 같습니다.",
                "Automatic NPC work is always coalesced and safely processed one actor at a time. Enabling this adds one more scheduling interval between actors; newly visible NPCs always take priority over bulk save-load work. Direct selections stay immediate and final results are identical.",
                "无论此设置如何，自动 NPC 任务都会合并重复事件并逐个安全处理。启用后会在角色之间额外增加一次调度间隔；新出现的 NPC 始终优先于读档批量任务。直接选择仍会立即处理，最终结果相同。"));
            if (settingsChanged) {
                bcn::Settings::Get().Update(settings);
            }
            if (ImGui::Button(Text("선택 액터 설정 값 초기화", "Reset selected actor settings", "重置所选角色设置值"))) {
                const auto reset = bcn::actor_settings_reset::QueueActor(SelectedActor());
                if (reset.accepted) DiscardPendingSelectionsAfterReset(false);
                bcn::ui::Notify(reset.accepted ?
                    Text("선택 액터의 바디·바디스킨·성기·후타스킨·오버레이·틴트마스크 초기화를 시작했습니다.", "Started resetting the selected actor's body, body skin, genitals, futanari skin, overlays, and tint masks.", "已开始重置所选角色的身体、身体皮肤、生殖器、扶她皮肤、覆盖层和色调遮罩。") :
                    Text("선택 액터 설정 값 초기화를 시작하지 못했습니다.", "Could not start resetting the selected actor settings.", "无法开始重置所选角色设置值。"));
            }
            ImGui::SameLine();
            if (ImGui::Button(Text("전체 액터 설정 값 초기화", "Reset all actor settings", "重置全部角色设置值"))) {
                const auto reset = bcn::actor_settings_reset::QueueAll();
                if (reset.accepted) DiscardPendingSelectionsAfterReset(true);
                bcn::ui::Notify(reset.accepted ?
                    Text("저장에 남아 있는 전체 액터의 바디·바디스킨·성기·후타스킨·오버레이와 플레이어 틴트마스크 초기화를 시작했습니다.", "Started resetting body, body skin, genitals, futanari skin, and overlays for all saved actors, plus player tint masks.", "已开始重置全部已保存角色的身体、身体皮肤、生殖器、扶她皮肤和覆盖层，以及玩家色调遮罩。") :
                    Text("전체 액터 설정 값 초기화를 시작하지 못했습니다.", "Could not start resetting all actor settings.", "无法开始重置全部角色设置值。"));
            }
            const auto* closeLabel = Text("닫기", "Close", "关闭");
            const auto closeWidth = ImGui::CalcTextSize(closeLabel).x +
                ImGui::GetStyle().FramePadding.x * 2.0F;
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - closeWidth);
            if (ImGui::Button(closeLabel)) g_showSettings = false;
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
        // DataLoaded populates asset catalogs once; OnOpened refreshes actors.
        // Avoid hashing all DDS files twice during plugin startup.
        native_ui::Register(&Draw);
    }

    void OnOpened()
    {
        std::scoped_lock lifecycle(g_uiLifecycleLock);
        g_uiSessionEpoch = frame_tasks::Epoch();
        g_overlayColorDrafts.Clear();
        g_tintColorDrafts.Clear();
        for (std::size_t layer{}; layer < g_tintSessionColors.size(); ++layer) {
            g_tintSessionColors[layer] = player_tint::CurrentColor(static_cast<player_tint::Layer>(layer));
        }
        // The actor list is deliberately refreshed only once at menu open.
        // A previous menu session must not retain an NPC camera target. Start
        // from Player every time. Nearby actors are still rebuilt at open and
        // by the explicit refresh button, but never replace that initial row.
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            g_selectedActorFormID = player->GetFormID();
            bcn::skin_application::InvalidateFutanariDetection(g_selectedActorFormID);
        } else {
            g_selectedActorFormID = 0;
        }
        g_actorSearch.clear();
        g_currentTintPack = player_tint::CurrentPack().value_or(std::string{});
        g_selectedTintPack = g_currentTintPack;
        for (const auto& layer : player_tint::SnapshotPersistedState().layers) {
            if (!layer.assetID.empty()) {
                g_tintColorDrafts.Set(g_selectedActorFormID,
                    static_cast<std::uint8_t>(layer.layer), layer.assetID, layer);
            }
        }
        g_selectedTintAssetID.clear();
        g_showTintDetails = false;
        g_showOverlayDetails = false;
        g_overlayArea.reset();
        g_pendingBody.reset();
        g_pendingSkin.reset();
        g_pendingFutanari.reset();
        g_pendingTint.reset();
        g_pendingTintBaseline.reset();
        for (auto& pending : g_pendingOverlays) pending.reset();
        ResetCatalogNavigation();
        g_initializeActorSelection = true;
        g_activeTab = ActiveTab::body;
        if (auto* actor = SelectedActor()) {
            [[maybe_unused]] const auto requested = overlay::RequestCatalog(actor);
        }
    }

    void OnLoadStart()
    {
        std::scoped_lock lifecycle(g_uiLifecycleLock);
        g_uiSessionEpoch = 0;
        g_overlayColorDrafts.Clear();
        g_tintColorDrafts.Clear();
        g_tintSessionColors = {};
        // A load is not a user confirmation. Do not let a delayed kHide
        // commit the old save's UI preview into the newly loaded actor.
        g_pendingBody.reset();
        g_pendingSkin.reset();
        g_pendingFutanari.reset();
        g_pendingTint.reset();
        g_pendingTintBaseline.reset();
        for (auto& pending : g_pendingOverlays) pending.reset();
        racemenu::QueueCancelPreview();
        overlay::QueueCancelPreviews();
        g_overlayArea.reset();
        g_showDistribution = false;
        // Loading another save is not a confirmation of the previous save's
        // in-memory editor draft.  OnClosed runs as part of the native close,
        // so clear it first to prevent cross-save JSON leakage.
        ResetDistributionEditor();
        bcn::menu_character::Presentation::Get().Restore();
        [[maybe_unused]] const auto closed = native_ui::Close();
    }

    void OnClosed()
    {
        std::scoped_lock lifecycle(g_uiLifecycleLock);
        // A preview is never a selection. Only an explicit double click or
        // activation command commits; closing restores the entry state.
        RollbackPendingSelections(SelectedActor());
        g_overlayColorDrafts.Clear();
        g_tintColorDrafts.Clear();
        g_tintSessionColors = {};
        g_showTintDetails = false;
        g_showOverlayDetails = false;
        DiscardDistributionDraft();
        g_showDistribution = false;
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
        std::scoped_lock lifecycle(g_uiLifecycleLock);
        if (!frame_tasks::IsCurrent(g_uiSessionEpoch.load())) return;
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
        DrawTitleBarRotationHint();
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
        const auto* refreshActorsLabel = Text("액터 새로고침", "Refresh actors", "刷新角色");
        const auto* outfitLabel = Text("의상·랜덤화", "Outfit · randomization", "服装·随机化");
        const auto* settingsLabel = Text("모드 설정", "Mod settings", "模组设置");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
            ImVec2(Scaled(4.0F), ImGui::GetStyle().ItemSpacing.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
            ImVec2(Scaled(6.0F), ImGui::GetStyle().FramePadding.y));
        const auto buttonWidth = [](const char* label) {
            return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0F;
        };
        const auto reservedWidth = buttonWidth(refreshActorsLabel) + buttonWidth(outfitLabel) +
            buttonWidth(settingsLabel) + ImGui::GetStyle().ItemSpacing.x * 3.0F;
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
        if (ImGui::Button(outfitLabel)) g_showOutfit = true;
        ImGui::SameLine();
        if (ImGui::Button(settingsLabel)) g_showSettings = true;
        ImGui::PopStyleVar(2);

        const auto* selectedActor = SelectedActor();
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto playerSelected = selectedActor && player &&
            selectedActor->GetFormID() == player->GetFormID();
        // Feature availability belongs to the installed female addon forms,
        // not to the number of currently registered or visibly equipped
        // actors. Actor eligibility is explained inside the tab itself.
        const auto futanariAvailable = bcn::futanari_support::Available();
        if (!playerSelected && g_activeTab == ActiveTab::tint) {
            // Applying a tint is already immediate, so dropping this transient
            // UI confirmation does not alter the saved/current tint state.
            g_activeTab = ActiveTab::body;
            g_pendingTint.reset();
            g_selectedTintAssetID.clear();
            g_showTintDetails = false;
            bcn::menu_character::Presentation::Get().SetTintFocus(false);
            g_showOverlayDetails = false;
        }
        if (!futanariAvailable && g_activeTab == ActiveTab::futanari) {
            g_activeTab = ActiveTab::skin;
        }

        bcn::menu_character::Presentation::Get().Apply(runtimeSettings.characterPosition, SelectedActor());

        ImGui::Separator();
        const auto activeTabBeforeControls = g_activeTab;
        HandleTabNavigation(playerSelected, futanariAvailable);
        if (TabButton(Text("바디프리셋", "Body Presets", "身体预设"),
                g_activeTab == ActiveTab::body)) g_activeTab = ActiveTab::body;
        ImGui::SameLine();
        if (TabButton(Text("바디스킨", "Body Skins", "身体皮肤"), g_activeTab == ActiveTab::skin)) {
            g_activeTab = ActiveTab::skin;
        }
        if (playerSelected) {
            ImGui::SameLine();
            if (TabButton(Text("틴트마스크", "Tint Masks", "色调蒙版"), g_activeTab == ActiveTab::tint)) {
                g_activeTab = ActiveTab::tint;
            }
        }
        if (futanariAvailable) {
            ImGui::SameLine();
            if (TabButton(Text("후타스킨", "Futanari Skin", "扶她皮肤"),
                    g_activeTab == ActiveTab::futanari)) {
                g_activeTab = ActiveTab::futanari;
            }
        }
        ImGui::SameLine();
        if (TabButton(Text("오버레이", "Overlays", "叠加层"),
                g_activeTab == ActiveTab::overlay)) {
            g_activeTab = ActiveTab::overlay;
            [[maybe_unused]] const auto requested = bcn::overlay::RequestCatalog(SelectedActor());
        }
        if (g_activeTab != activeTabBeforeControls && g_distributionSelectionMode) {
            CancelDistributionCatalogSelection();
        }
        ImGui::SameLine();
        const auto* favoritesLabel = Text("즐겨찾기", "Favorites", "收藏");
        const auto showFavorites = g_activeTab != ActiveTab::futanari;
        const auto favoritesControlWidth = showFavorites ?
            ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
                ImGui::CalcTextSize(favoritesLabel).x : 0.0F;
        const auto searchWidth = (std::max)(1.0F,
            ImGui::GetContentRegionAvail().x - favoritesControlWidth -
                (showFavorites ? ImGui::GetStyle().ItemSpacing.x : 0.0F));
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##search", Text("이름 검색", "Search", "搜索名称"), &g_search);
        if (showFavorites) {
            ImGui::SameLine();
            ImGui::Checkbox(favoritesLabel, &FavoritesOnly());
        }
        if (const auto* actor = SelectedActor(); actor && actor != RE::PlayerCharacter::GetSingleton() &&
            bcn::Distribution::Get().HasManualAssignment(actor)) {
            ImGui::TextColored(ImVec4(.48F, .82F, .96F, 1.0F), "%s", Text(
                "직접 선택 우선", "Direct selection takes priority", "优先使用直接选择"));
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
            auto items = BodyItems();
            DrawCatalogCommandRow(DistributionPool::body,
                [] { RefreshFileCatalog(PresetCatalog::Get()); },
                [&items] {
                    g_distributionSelectedIds.clear();
                    for (const auto& item : items) g_distributionSelectedIds.insert(item.id);
                },
                Text("CalienteTools\\~에서 바디프리셋을 읽습니다.(더블클릭 적용)",
                    "Reads body presets from CalienteTools\\~. (Double-click to apply)",
                    "从 CalienteTools\\~ 读取身体预设。（双击应用）"));
            DrawCatalog(items, true);
        } else if (g_activeTab == ActiveTab::skin) {
            DrawSkinCatalog();
        } else if (g_activeTab == ActiveTab::overlay) {
            DrawOverlayCatalog();
        } else if (g_activeTab == ActiveTab::futanari) {
            DrawFutanariCatalog();
        } else {
            DrawPlayerTintCatalog();
        }

        DrawDistributionPopup();
        DrawOutfitPopup();
        DrawSettingsPopup();
        DrawTintDetailPopup();
        DrawOverlayDetailPopup();
        UpdatePreviewOwnership();
        // Tint and its detailed value popup deliberately share one face view;
        // leaving Tint restores the normal left/right presentation.
        if (g_activeTab == ActiveTab::tint) {
            bcn::menu_character::Presentation::Get().SetTintFocus(true);
        } else if (g_activeTab == ActiveTab::overlay) {
            bcn::menu_character::Presentation::Get().SetOverlayFocus(g_overlayArea);
        } else {
            bcn::menu_character::Presentation::Get().SetTintFocus(false);
        }
        bcn::menu_character::Presentation::Get().UpdateRotationInteraction();
        ImGui::End();
    }
}
