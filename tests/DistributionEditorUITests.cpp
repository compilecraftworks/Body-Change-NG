#include "BodyChangeNG/DistributionEditorState.h"
#include "BodyChangeNG/DistributionRuleNames.h"
#include "BodyChangeNG/DistributionTargetSearch.h"
#include "BodyChangeNG/DistributionAuthoring.h"
#include "BodyChangeNG/FittedTextUI.h"
#include "BodyChangeNG/PopupPlacementUI.h"
#include "BodyChangeNG/SessionSnapshot.h"
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

// Real production ImGui functions and editor state; only engine/catalog/file
// boundaries are stand-ins. Mouse clicks below go through ImGui's input queue.
using DistributionPool = bcn::distribution_editor::Pool;
struct DistributionItemRow { std::string id, name, detail; bcn::overlay::Area area{bcn::overlay::Area::body}; };
struct DistributionTargetOption { std::string display, editorID, plugin; std::uint32_t localFormID{}, runtimeFormID{}; std::string name; };
struct DistributionTargetSnapshot { std::vector<DistributionTargetOption> factions, races, keywords, classes; std::vector<std::string> plugins; };
bcn::distribution_editor::TargetSearch g_distributionTargetSearch;
std::string g_distributionStatus;
double g_distributionStatusUntil{};
bool g_showDistribution{}, g_distributionSelectionMode{}, g_distributionFemale{true}, g_distributionEditorLoaded{};
DistributionPool g_distributionPool{};
std::size_t g_selectedDistributionRule{};
std::uint64_t g_distributionCatalogRevision{}, g_distributionItemOverlayRevision{};
std::uint32_t g_nextDraftRuleID{1};
std::vector<bcn::DistributionRule> g_distributionRules, saved, active;
bcn::distribution_editor::Tabs g_distributionRuleTabs;
std::optional<bcn::distribution_editor::Items> g_distributionItemDraft;
std::vector<DistributionItemRow> g_distributionItemRows;
std::string g_distributionItemSearch;
bcn::async_work::SessionSnapshot<DistributionTargetSnapshot> g_distributionTargets;
std::shared_ptr<const DistributionTargetSnapshot> g_distributionTargetOptions;
std::vector<std::string> catalogSelection;
bool cancel{}, failSave{};
unsigned diskWrites{}, activations{}, immediate{}, rolls{};
float scale{1};
bcn::UiLanguage language{bcn::UiLanguage::english};
std::string notification;

namespace RE { class Actor {}; class TESForm { public: std::uint32_t id{}; static TESForm* LookupByID(std::uint32_t id) {static TESForm form; form.id = id; return id ? &form : nullptr;} }; }
namespace bcn {
    struct InputSink {
        static InputSink& Get() {static InputSink value; return value;}
        bool IsCapturingHotkey() const {return false;}
    };
    namespace native_ui {
        bool ConsumeEscape() {return std::exchange(cancel, false);}
        bool CancelPressed() {return false;}
    }
    Distribution& Distribution::Get() { static Distribution self; return self; }
    bool Distribution::SaveRulesForNextGame(std::vector<DistributionRule> rules) const {
        ++diskWrites; if (failSave) return false; saved = std::move(rules); return true;
    }
    void Distribution::SetRules(std::vector<DistributionRule> rules) { ++activations; active = std::move(rules); }
    std::vector<DistributionRule> Distribution::SavedRulesSnapshot() const { return saved; }
    std::size_t Distribution::ApplyLoadedNPCs() { return ++immediate; }
    bool SetDistributionRuleNPC(DistributionRule&, RE::TESForm*) {return false;}
    bool SetDistributionRuleTargetForm(DistributionRule& rule, RE::TESForm* form) {
        rule.targetFormID = form->id; return true;
    }
    namespace ui {void Notify(std::string message) {notification = std::move(message);}}
    namespace overlay { std::uint64_t CatalogRevision() {return 0;} }
}
const char* Text(const char* ko, const char* en, const char* zh) {
    return language == bcn::UiLanguage::korean ? ko : language == bcn::UiLanguage::chineseSimplified ? zh : en;
}
bcn::UiLanguage CurrentLanguage() {return language;}
float Scaled(float value) {return value * scale;}
ImVec2 DefaultWindowSize(float x, float y) {return {Scaled(x), Scaled(y)};}
std::string Lower(std::string text) {for (auto& c : text) c = static_cast<char>(bcn::asset_identity::FoldCase(c)); return text;}
RE::Actor* SelectedActor() {return nullptr;}
void RollbackSingleCatalogPreview(RE::Actor*, DistributionPool) {++rolls;}
void ResetCatalogNavigation() {}
void ClearDistributionCatalogSelection() {catalogSelection.clear();}
bool IsDistributionSelectionFor(DistributionPool pool) {return g_distributionSelectionMode && pool == g_distributionPool;}
std::size_t DistributionSelectionCount() {return catalogSelection.size();}
void SelectDistributionSex(bool female) {g_distributionFemale = female;}
void BeginDistributionCatalogSelection(DistributionPool pool) {g_distributionPool = pool; g_distributionSelectionMode = true;}
void CancelDistributionCatalogSelection() {g_distributionSelectionMode = false;}
void EnsureDistributionEditor() {if (!g_distributionEditorLoaded) {g_distributionRules = saved; g_distributionEditorLoaded = true;}}
void SynchronizeDistributionRuleNames() {}
void FillRuleTargetFromSelectedActor(bcn::DistributionRule&) {}
void PrepareResizableDropdown(std::size_t) {}
void PrepareDownwardResizableDropdown(std::size_t) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::CalcItemWidth(), 150), ImVec2(ImGui::CalcItemWidth(), 400));
}
bool TabButton(const char* label, bool) {return ImGui::Button(label);}
bool BeginUndimmedPopupModal(const char* title, bool* open, ImGuiWindowFlags flags, bcn::popup_placement::Kind kind) {
    static bcn::popup_placement::Modals modals;
    return modals.Begin(title, open, flags, kind, {}).began;
}
void SetRuleDistributionSelection(bcn::DistributionRule& rule) {
    bcn::distribution_editor::Items items{rule, g_distributionPool};
    items.Ids() = catalogSelection;
    if (!items.Commit(rule)) throw std::runtime_error("seed failed");
}
void BuildDistributionItemRows() {
    g_distributionItemRows = {{"a", "Asset A", "", bcn::overlay::Area::body},
        {"b", "Asset B ## literal", "", bcn::overlay::Area::body}};
}
void BeginDistributionItemEdit(const bcn::DistributionRule& rule) {
    g_distributionItemDraft = bcn::distribution_editor::Items{rule, g_distributionPool};
    g_distributionItemSearch.clear(); BuildDistributionItemRows();
}
#include "DistributionEditorUI.inl"
#include "DistributionTypingUI.inl"

struct Widget { ImRect rect; std::string label, window; };
std::unordered_map<ImGuiID, Widget> widgets;
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx, ImGuiID id, const ImRect& rect, const ImGuiLastItemData*) {
    auto& w = widgets[id]; w.rect = rect; w.window = ctx->CurrentWindow->Name;
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*, ImGuiID id, const char* label, ImGuiItemStatusFlags) {widgets[id].label = label;}
void ImGuiTestEngineHook_Log(ImGuiContext*, const char*, ...) {}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*, ImGuiID) {return "";}
void Check(bool ok, const char* what) {if (!ok) throw std::runtime_error(what);}
Widget Find(const std::string& label, const std::string& window) {
    for (const auto& [id, widget] : widgets)
        if (widget.label == label && widget.window.contains(window)) return widget;
    // This vendored BeginCombo submits ItemAdd but no ItemInfo label hook.
    // Recover its real ImGui ID from the production rule ID stack.
    if (label.starts_with("##rule") && g_selectedDistributionRule < g_distributionRules.size()) {
        for (const auto& [id, widget] : widgets) if (widget.window.contains(window)) {
            if (const auto* owner = ImGui::FindWindowByName(widget.window.c_str())) {
                const auto seed = ImHashStr(g_distributionRules[g_selectedDistributionRule].id.c_str(), 0, owner->ID);
                if (id == ImHashStr(label.c_str(), 0, seed)) return widget;
            }
        }
    }
    std::cerr << "Selected rule=" << g_selectedDistributionRule << " rules=" << g_distributionRules.size()
        << " pool=" << static_cast<int>(g_distributionPool) << " targets=" << static_cast<bool>(g_distributionTargetOptions) << '\n';
    for (const auto& [id, widget] : widgets) if (!widget.label.empty()) std::cerr << widget.window << ": " << widget.label << '\n';
    throw std::runtime_error("Missing widget " + label + " in " + window);
}
void Frame() {
    widgets.clear();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize({Scaled(700),Scaled(875)});
    ImGui::Begin("Host", nullptr, ImGuiWindowFlags_NoSavedSettings);
    DrawCatalogCommandRow(DistributionPool::body, []{}, []{}, "Catalog help");
    DrawDistributionPopup();
    ImGui::End(); ImGui::Render();
}
void Click(const std::string& label, const std::string& window) {
    const auto point = Find(label, window).rect.GetCenter();
    ImGui::GetIO().AddMousePosEvent(point.x, point.y); Frame();
    ImGui::GetIO().AddMouseButtonEvent(0, true); Frame();
    ImGui::GetIO().AddMouseButtonEvent(0, false); Frame(); Frame();
}
void NewContext() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {1920,1080};
    // Match NativeImGuiHost's 20 px Latin/Korean/Chinese fonts where available.
    const auto addFont = [&](const char* path, const ImWchar* range, bool merge) {
        if (!std::filesystem::exists(path)) return;
        ImFontConfig config; config.MergeMode = merge; config.PixelSnapH = true;
        config.OversampleH = config.OversampleV = 1;
        io.Fonts->AddFontFromFileTTF(path, 20.0F, &config, range);
    };
    addFont("C:/Windows/Fonts/segoeui.ttf", io.Fonts->GetGlyphRangesDefault(), false);
    if (io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault();
    addFont("C:/Windows/Fonts/malgun.ttf", io.Fonts->GetGlyphRangesKorean(), true);
    addFont("C:/Windows/Fonts/msyh.ttc", io.Fonts->GetGlyphRangesChineseSimplifiedCommon(), true);
    unsigned char* pixels{}; int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height); io.Fonts->SetTexID(1);
    GImGui->TestEngineHookItems = true;
}
int main() try {
    NewContext(); Frame(); Frame();
    Click("Distribution conditions", "Host");
    Check(g_showDistribution && g_distributionRules.empty() && saved.empty(), "opening conditions created a rule");
    Click("+ Add rule", "DistributionPopup");
    Check(g_distributionRules.size() == 1 && g_distributionRules[0].presetIds.empty(), "new direct rule inherited candidates");
    Click("Edit items", "DistributionPopup");
    Check(g_distributionItemDraft.has_value(), "item popup did not open");
    Click("##distributionItemSearch", "DistributionItems");
    Check(CurrentEditableTextInputActive() && ImGui::GetIO().WantTextInput,
        "new search box did not enter the shared keyboard-suppression path");
    ImGui::GetIO().AddInputCharactersUTF8("Asset"); Frame(); Frame();
    Check(g_distributionItemSearch == "Asset" && g_distributionRules[0].presetIds.empty(),
        "typing changed items or failed to edit the search field");
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true); Frame();
    ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false); Frame(); Frame();
    Check(g_distributionItemDraft && g_showDistribution && !CurrentEditableTextInputActive(),
        "Escape from text edit closed dialogs or retained text ownership");
    Click("Select all", "DistributionItems");
    Check(g_distributionRules[0].presetIds.empty(), "item selection wrote through before Save");
    Click("Close", "DistributionItems");
    Check(!g_distributionItemDraft && g_showDistribution && g_distributionRules[0].presetIds.empty(), "child close committed or closed parent");
    Click("Edit items", "DistributionPopup"); Click("Select all", "DistributionItems");
    cancel = true; Frame(); Frame();
    Check(!g_distributionItemDraft && g_showDistribution && g_distributionRules[0].presetIds.empty(), "child Escape committed or closed parent");
    Click("Edit items", "DistributionPopup"); Click("Select all", "DistributionItems"); Click("Save", "DistributionItems");
    Check(g_distributionRules[0].presetIds.size() == 2 && diskWrites == 0, "child Save did not stage exactly two candidates");
    failSave = true; Click("Save", "DistributionPopup");
    Check(saved.empty() && activations == 0 && g_distributionRules[0].presetIds.size() == 2, "write failure lost edits or activated rules");
    Check(g_distributionStatus.contains("Could not save") && notification.empty(), "save failure escaped to main UI");
    failSave = false; Click("Save", "DistributionPopup");
    Check(saved.size() == 1 && active.size() == 1 && immediate == 0, "final Save forced immediate distribution or lost rules");
    Check(g_distributionStatus == "Saved distribution changes." && notification.empty(), "save success escaped to main UI");
    Click("##ruleName", "DistributionPopup");
    Check(CurrentEditableTextInputActive(), "rule name lost shared typing suppression");
    ImGui::GetIO().AddInputCharactersUTF8(" typed"); Frame(); Frame();
    Check(g_distributionRules[0].name.contains("typed") && saved[0].name != g_distributionRules[0].name,
        "typing the rule name bypassed the draft");
    Click("Body Skins", "DistributionPopup");
    Check(g_selectedDistributionRule == bcn::distribution_editor::noRule, "empty skin tab selected a body rule");
    Click("+ Add rule", "DistributionPopup");
    const auto skinRule = g_selectedDistributionRule;
    Click("Futanari Skin", "DistributionPopup"); Click("Body Skins", "DistributionPopup");
    Check(g_selectedDistributionRule == skinRule, "empty skin draft disappeared on tab switch");
    Click("Edit items", "DistributionPopup"); Click("Select all", "DistributionItems"); Click("Save", "DistributionItems");
    Check(g_distributionRules[skinRule].skinProfileIds.size() == 2 && g_distributionRules[0].presetIds.size() == 2,
        "skin items altered body rule");
    Click("Close", "DistributionPopup");
    Check(!g_showDistribution && !g_distributionItemDraft && g_distributionRules.size() == 1, "parent close saved unsaved skin rule");
    Check(!CurrentEditableTextInputActive(), "closed editor retained keyboard ownership");
    Check(g_distributionStatus.empty() && g_distributionStatusUntil == 0.0, "closed editor retained status");
    Click("NPC distribution", "Host"); catalogSelection = {"a"}; Frame();
    Click("Distribute", "Host");
    Check(g_distributionRules.size() == 2 && g_distributionRules.back().presetIds == catalogSelection,
        "catalog-first route lost selected items");
    Click("+ Add rule", "DistributionPopup");
    Check(g_distributionRules.back().presetIds == catalogSelection, "catalog-first Add lost original seed workflow");
    Click("Close", "DistributionPopup");
    // Both entry routes carry all four source tabs, never the last-used tab.
    for (auto pool : {DistributionPool::body, DistributionPool::skin,
             DistributionPool::futanari, DistributionPool::overlay}) {
        const auto before = g_distributionRules.size();
        OpenDistributionConditions(pool); Frame(); Frame();
        Check(g_distributionPool == pool && g_distributionRules.size() == before,
            "direct entry did not retain the source tab or created a rule");
        Click("Close", "DistributionPopup");
        g_distributionPool = pool; catalogSelection = {"a"};
        OpenDistributionEditorFromCatalog(); Frame(); Frame();
        Check(g_distributionPool == pool &&
            bcn::distribution_editor::Count(g_distributionRules.back(), pool) == 1,
            "catalog-first entry did not retain the source tab/candidates");
        Click("Edit items", "DistributionPopup");
        g_distributionItemRows.clear();
        for (unsigned i{}; i < 20000; ++i)
            g_distributionItemRows.push_back({std::to_string(i), "Large catalog " + std::to_string(i), {}, bcn::overlay::Area::body});
        Frame(); Frame();
        Check(ImGui::GetDrawData()->TotalVtxCount < 100000, "offscreen item rows were not clipped");
        Click("Close", "DistributionItems");
        Check(g_distributionItemRows.empty() && !g_distributionItemDraft,
            "item metadata/draft remained owned after close");
        Click("Close", "DistributionPopup");
    }
    // Actual target dropdowns for all five scope types, not UI stand-ins.
    OpenDistributionConditions(DistributionPool::body); Frame(); Frame();
    g_selectedDistributionRule = 0;
    auto targets = std::make_shared<DistributionTargetSnapshot>();
    for (unsigned i{}; i < 20000; ++i) {
        const auto label = "Target " + std::to_string(i) + " · FactionEDID · Skyrim.esm:123456";
        DistributionTargetOption option{label, "FactionEDID", "Skyrim.esm", 0x123456, i + 1};
        targets->races.push_back(option); targets->factions.push_back(option);
        targets->keywords.push_back(option); targets->classes.push_back(option);
        targets->plugins.push_back("Plugin" + std::to_string(i) + ".esp");
    }
    const DistributionTargetOption special{"Needle Name · SpecialEDID · Special.esl:000ABC", "SpecialEDID", "Special.esl", 0xABC, 0xFE123ABC};
    for (auto* list : {&targets->races, &targets->factions, &targets->keywords, &targets->classes}) list->push_back(special);
    targets->plugins.push_back("NeedlePlugin.esp");
    g_distributionTargetOptions = targets;
    const std::array scopes{bcn::DistributionScope::pluginFile, bcn::DistributionScope::raceEditorID,
        bcn::DistributionScope::factionEditorID, bcn::DistributionScope::keyword, bcn::DistributionScope::npcClass};
    const std::array combos{"##rulePlugin", "##ruleRace", "##ruleFaction", "##ruleKeyword", "##ruleClass"};
    const std::array queries{"nEeDlEpLuGiN", "nEeDlE nAmE", "sPeCiAlEdId", "sPeCiAl.EsL", "fe123aBc"};
    for (std::size_t i{}; i < scopes.size(); ++i) {
        auto& rule = g_distributionRules[0]; rule.scope = scopes[i]; rule.target.clear();
        rule.targetPlugin.clear(); rule.targetLocalFormID = rule.targetFormID = 0;
        Frame(); Frame(); Click(combos[i], "DistributionPopup");
        Check(g_distributionTargetSearch.query.empty() && g_distributionTargetSearch.visible.size() == 20001,
            "new dropdown inherited previous query or lost entries");
        Check(!CurrentEditableTextInputActive(), "opening target dropdown captured typing");
        Check(ImGui::GetDrawData()->TotalVtxCount < 100000, "target dropdown did not clip 20000 rows");
        Click("##distributionTargetSearch", "##Combo");
        Check(CurrentEditableTextInputActive() && ImGui::GetIO().WantTextInput, "target search bypassed typing suppression");
        ImGui::GetIO().AddInputCharactersUTF8(queries[i]); Frame(); Frame();
        Check(g_distributionTargetSearch.visible == std::vector<std::size_t>{20000}, "target name/ID/plugin filter mismatch");
        Check(rule.target.empty() && rule.targetFormID == 0 && !g_distributionTargetSearch.Filter(), "typing modified rule or rebuilt unchanged filter");
        Click(i == 0 ? "NeedlePlugin.esp" : special.display, "targetResults");
        Check(i == 0 ? rule.target == "NeedlePlugin.esp" : rule.targetFormID == special.runtimeFormID, "filtered row selected wrong original target");
        // Reopen, no-match search and Escape must not discard the parent draft.
        Click(combos[i], "DistributionPopup"); Click("##distributionTargetSearch", "##Combo");
        ImGui::GetIO().AddInputCharactersUTF8("zzzz-no-match"); Frame(); Frame();
        Check(g_distributionTargetSearch.visible.empty(), "no-match query returned rows");
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true); Frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false); Frame(); Frame();
        Check(g_showDistribution && !CurrentEditableTextInputActive(), "Escape in target search closed parent or retained typing");
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true); Frame();
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false); Frame(); Frame();
        Check(g_showDistribution, "closing dropdown closed parent");
        Check(GImGui->OpenPopupStack.Size == 1, "Escape did not close the target dropdown alone");
    }
    Click("Close", "DistributionPopup");
    Check(g_distributionTargetSearch.keys.empty() && g_distributionTargetSearch.visible.empty(), "target cache retained after editor closed");
    ImGui::DestroyContext();
    // The same production layout for all language/scale combinations. Rects
    // verify requested placement and footer visibility without running Skyrim.
    for (auto lang : {bcn::UiLanguage::english,bcn::UiLanguage::korean,bcn::UiLanguage::chineseSimplified})
        for (float size : {0.75F, 1.0F, 1.5F}) {
            language = lang; scale = size; NewContext();
            ImGui::GetStyle().ScaleAllSizes(size); ImGui::GetIO().FontGlobalScale = size;
            g_showDistribution = true; g_distributionPool = DistributionPool::body;
            g_selectedDistributionRule = 0; g_distributionRules = saved;
            Frame(); Frame(); Frame();
            NotifyDistributionEditor(Text("배포 조건 변경을 저장했습니다.", "Saved distribution changes.", "已保存分发条件更改。"));
            Frame(); Frame();
            const auto save = Find(Text("저장", "Save", "保存"), "DistributionPopup").rect;
            const auto down = Find(Text("아래로", "Down", "下移"), "DistributionPopup").rect;
            const auto edit = Find(Text("항목변경", "Edit items", "更改项目"), "DistributionPopup").rect;
            const auto close = Find(Text("닫기", "Close", "关闭"), "DistributionPopup").rect;
            Check(save.Min.y > down.Max.y && save.Max.x < close.Min.x &&
                std::abs(save.Min.y-close.Min.y) < 1, "Save not immediately left of footer Close");
            const auto next = Find(Text("다음 게임 실행 시 배포", "Distribute on next game launch", "下次启动游戏时分发"), "DistributionPopup").rect;
            Check(save.Min.y >= next.Max.y || save.Min.x > next.Max.x, "Save overlaps distribution action");
            Check(edit.Max.y < save.Min.y && close.Max.y < 1080, "item edit misplaced or Close below screen");
            ImGui::DestroyContext();
        }
    std::cout << "DistributionEditorUITests passed: actual ImGui mouse flow, nested cancel/save, failed writes, both entry routes and 9 language/scale layouts\n";
} catch (const std::exception& e) {std::cerr << e.what() << '\n'; return 1;}
