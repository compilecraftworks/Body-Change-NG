#include "BodyChangeNG/FormDeleteCallbackPattern.h"
#include "BodyChangeNG/PeImageFile.h"
#include "BodyChangeNG/PopupPlacement.h"
#include <limits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <source_location>
#include <string>

namespace {
    void Require(bool ok, const std::source_location location =
        std::source_location::current())
    {
        if (!ok) {
            std::cerr << "CodePatternTests failed at line " << location.line() << '\n';
            std::abort();
        }
    }
    template<class T> void Put(std::span<std::uint8_t> bytes, std::size_t offset,T value) {
        Require(offset<=bytes.size() && sizeof(T)<=bytes.size()-offset);
        std::memcpy(bytes.data()+offset,&value,sizeof(value));
    }
}
int main()
{
    using namespace bcn::popup_placement;
    Require(keys.size() == 8U);
    Require(Changed({}, 320.0F, 240.0F));
    Require(!Changed({ true, 320.0F, 240.0F }, 320.25F, 240.25F));
    Require(Changed({ true, 320.0F, 240.0F }, 350.0F, 240.0F));
    Require(!Valid(std::numeric_limits<float>::infinity(), 0.0F));
    Require(!Valid(0.0F, std::numeric_limits<float>::quiet_NaN()));
    Require(ClampAxis(1800.0F, 700.0F, 0.0F, 1920.0F) == 1220.0F);
    Require(ClampAxis(-500.0F, 700.0F, 0.0F, 1920.0F) == 0.0F);
    Require(ClampAxis(600.0F, 700.0F, 100.0F, 600.0F) == 100.0F);
    Require(ClampAxis(600.0F, 700.0F, 0.0F, 0.0F) == 600.0F);

    using namespace bcn::racemenu_form_delete;
    constexpr std::array<std::uint8_t,42> original{
        0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x8b,0xd1,
        0x48,0x8d,0x0d,0x7e,0xbc,0x18,0x00,0xe8,0x89,0x88,0x03,0x00,
        0x8b,0xd3,0x48,0x8d,0x0d,0x70,0xbc,0x18,0x00,0x48,0x83,0xc4,0x20,
        0x5b,0xe9,0xc6,0x89,0x03,0x00};
    Require(DecodeNarrowingCallback(original,0x50AA0,0x300000)==CallbackPlan{0x50AA0,0x1DC730,0x89340,0x89490});
    // Address moves do not require another DLL hash/version entry. All four
    // signed relative displacements must be decoded on the loaded image.
    for (auto entry:{0x1000U,0xF0000U,0x200000U}) {
        auto code=original;
        const auto write=[&](std::size_t offset,std::uint32_t target) {
            Put<std::int32_t>(code,offset,static_cast<std::int32_t>(target-entry-offset-4));
        };
        write(14,0xD0000); write(28,0xD0000); write(19,0x60000); write(38,0x50000);
        Require(DecodeNarrowingCallback(code,entry,0x300000)==CallbackPlan{entry,0xD0000,0x60000,0x50000});
        write(28,0xD0008); Require(!DecodeNarrowingCallback(code,entry,0x300000));
        write(28,0xD0000); write(38,0x60000); Require(!DecodeNarrowingCallback(code,entry,0x300000));
        write(38,entry); Require(!DecodeNarrowingCallback(code,entry,0x300000));
        write(38,0x400000); Require(!DecodeNarrowingCallback(code,entry,0x300000));
    }
    for (std::size_t i{};i<original.size();++i) {
        if ((i>=14 && i<18) || (i>=19 && i<23) || (i>=28 && i<32) || i>=38) continue;
        auto changed=original; changed[i]^=0x01;
        Require(!DecodeNarrowingCallback(changed,0x50AA0,0x300000));
    }
    // A full-width legacy callback is NOT this bug and must stay untouched.
    auto wide=std::vector<std::uint8_t>(original.begin(),original.end());
    wide.insert(wide.begin()+9,0x48);
    Require(!DecodeNarrowingCallback(wide,0x50AA0,0x300000));
    Require(!DecodeNarrowingCallback({},0x50AA0,0x300000));
    Require(!DecodeNarrowingCallback(original,0xFFFFFFF0,0xFFFFFFFF));

    using bcn::code_image::File;
    std::vector<std::uint8_t> pe(0xC00);
    Put<std::uint16_t>(pe,0,0x5A4D); Put<std::uint32_t>(pe,0x3C,0x80);
    Put<std::uint32_t>(pe,0x80,0x4550); Put<std::uint16_t>(pe,0x84,0x8664);
    Put<std::uint16_t>(pe,0x86,2); Put<std::uint16_t>(pe,0x94,0xF0);
    Put<std::uint16_t>(pe,0x98,0x20B); Put<std::uint32_t>(pe,0x98+56,0x4000);
    Put<std::uint32_t>(pe,0x98+136,0x1800); Put<std::uint32_t>(pe,0x98+140,12);
    Put<std::uint32_t>(pe,0x188+8,0xA00); Put<std::uint32_t>(pe,0x188+12,0x1000);
    Put<std::uint32_t>(pe,0x188+16,0xA00); Put<std::uint32_t>(pe,0x188+20,0x200);
    Put<std::uint32_t>(pe,0x188+36,0x60000020);
    // BSS has virtual extent but no bytes on disk. Global Override resides
    // there in real SKEE; executable bytes must NOT be read from that range.
    Put<std::uint32_t>(pe,0x1B0+8,0x1000); Put<std::uint32_t>(pe,0x1B0+12,0x2000);
    Put<std::uint32_t>(pe,0x1B0+36,0xC0000080);
    Put<std::uint32_t>(pe,0xA00,0x1100); Put<std::uint32_t>(pe,0xA04,0x112A);
    const auto parsed=File::Parse(pe);
    Require(parsed.has_value());
    Require(parsed->IsFunction(0x1100) && !parsed->IsFunction(0x1101));
    Require(parsed->HasFlags(0x2800,8,0x80000000) && parsed->Bytes(0x2800,8).empty());
    Require(parsed->Bytes(0x1100,42).size()==42);
    Require(!parsed->HasFlags(0xFFFFFFF0,64,0x80000000));
    for (std::size_t size{};size<pe.size();++size) Require(!File::Parse(std::span(pe).first(size)));
    auto bad=pe; Put<std::uint32_t>(bad,0x3C,0xFFFFFFF0); Require(!File::Parse(bad));
    bad=pe; Put<std::uint16_t>(bad,0x86,97); Require(!File::Parse(bad));
    bad=pe; Put<std::uint32_t>(bad,0x188+16,0xFFFFFFFF); Require(!File::Parse(bad));
    bad=pe; Put<std::uint32_t>(bad,0xA04,0x1100); Require(!File::Parse(bad));
    bad=pe; Put<std::uint32_t>(bad,0x188+36,0x40000040); Require(!File::Parse(bad));

    // The NiNodeUpdate event is the face rebuild barrier. Once it fires, face
    // work must go directly to SKSE's game queue; routing it back through the
    // input-tick actor queue reintroduces a visible extra quiet-lease tick.
    std::ifstream faceFile(std::filesystem::path("src") / "BodyChangeNG" /
        "FaceSkinOverrides.cpp", std::ios::binary);
    Require(faceFile.good());
    const std::string faceSource((std::istreambuf_iterator<char>(faceFile)), {});
    const auto eventBegin = faceSource.find("void OnNiNodeUpdate(RE::Actor* actor)");
    const auto eventEnd = faceSource.find("void Reset(bool preserveBaselines)", eventBegin);
    Require(eventBegin != std::string::npos && eventEnd != std::string::npos);
    const auto eventBody = std::string_view(faceSource).substr(eventBegin, eventEnd - eventBegin);
    Require(eventBody.contains("SKSE::GetTaskInterface()"));
    Require(eventBody.contains("tasks->AddTask"));
    Require(!eventBody.contains("frame_tasks::Queue"));

    std::ifstream uiFile(std::filesystem::path("src") / "BodyChangeNG" /
        "UI.cpp", std::ios::binary);
    Require(uiFile.good());
    const std::string uiSource((std::istreambuf_iterator<char>(uiFile)), {});
    const auto placementBegin = uiSource.find("bool BeginUndimmedPopupModal(");
    const auto placementEnd = uiSource.find("void TextDisabledWrapped", placementBegin);
    Require(placementBegin != std::string::npos && placementEnd != std::string::npos);
    const auto placementBody = std::string_view(uiSource).substr(placementBegin, placementEnd - placementBegin);
    Require(placementBody.contains("modals.Begin(") && placementBody.contains("RememberPopupPosition"));
    for (const auto* kind : keys) {
        Require(uiSource.contains(std::string("Kind::") + kind));
    }
    const auto outfitBegin = uiSource.find("void DrawOutfitPopup()");
    const auto outfitEnd = uiSource.find("void DrawSettingsPopup()", outfitBegin);
    const auto outfitBody = std::string_view(uiSource).substr(outfitBegin, outfitEnd - outfitBegin);
    Require(outfitBody.find("SetNextWindowSizeConstraints") < outfitBody.find("BeginUndimmedPopupModal"));
    const auto overlayBegin = uiSource.find("void DrawOverlayCatalog()");
    const auto overlayFooter = uiSource.find("// Keep color controls outside", overlayBegin);
    const auto overlayEnd = uiSource.find("void DrawFutanariCatalog()", overlayFooter);
    const auto footer = std::string_view(uiSource).substr(overlayFooter, overlayEnd - overlayFooter);
    Require(!footer.contains("if (distributionSelecting) return;") &&
        footer.contains("!color.has_value() && !distributionColorTarget") &&
        footer.contains("g_overlayColorDistributionDraft = distributionSelecting") &&
        footer.contains("if (!distributionSelecting)"));
    const auto colorPopupBegin = uiSource.find("void DrawOverlayDetailPopup()");
    const auto colorPopupEnd = uiSource.find("void DrawTintDetailPopup()", colorPopupBegin);
    const auto colorPopup = std::string_view(uiSource).substr(colorPopupBegin, colorPopupEnd - colorPopupBegin);
    Require(colorPopup.contains("g_overlayColorDrafts.Set(") &&
        colorPopup.contains("if (!g_overlayColorDistributionDraft &&"));
    Require(uiSource.contains("rule.overlayColors[index] = g_overlayColorDrafts.CopySelection"));
    const auto closeBegin = uiSource.find("void OnClosed()");
    const auto closeEnd = uiSource.find("void Notify(", closeBegin);
    Require(closeBegin != std::string::npos && closeEnd != std::string::npos);
    const auto closeBody = std::string_view(uiSource).substr(closeBegin, closeEnd - closeBegin);
    Require(closeBody.contains("RollbackPendingSelections(SelectedActor())"));
    Require(closeBody.contains("DiscardDistributionDraft()") &&
        !uiSource.contains("SaveDistributionDraft"));
    const auto popupStart = uiSource.find("void DrawDistributionPopup()");
    const auto closeEditorStart = uiSource.find("const auto closeEditor", popupStart);
    const auto closeEditorEnd = uiSource.find("if (EscapePressed())", closeEditorStart);
    const auto closeEditor = std::string_view(uiSource).substr(closeEditorStart,
        closeEditorEnd - closeEditorStart);
    Require(closeEditor.contains("DiscardDistributionDraft()") && !closeEditor.contains("Save"));
    const auto immediateStart = uiSource.find("bool SaveActiveDistributionRules()");
    const auto immediateEnd = uiSource.find("void DiscardDistributionDraft", immediateStart);
    const auto immediate = std::string_view(uiSource).substr(immediateStart, immediateEnd - immediateStart);
    Require(immediate.find("SaveRulesForNextGame") < immediate.find("SetRules(") &&
        immediate.contains("return false"));
    Require(uiSource.contains("SavedRulesSnapshot()") &&
        uiSource.contains("더블클릭 복수 적용/해제") && uiSource.contains("\"적용\", \"Applied\""));
    Require(!uiSource.contains("더블클릭으로") && uiSource.contains("FittedDisabledLine(help)"));
    Require(uiSource.contains("Reads installed mods. (Double-click to apply/remove multiple)"));
    Require(uiSource.contains("CalienteTools\\\\~에서 바디프리셋을 읽습니다.(더블클릭 적용)") &&
        uiSource.contains("Futanari\\\\<스킨명>\\\\Textures\\\\~ 에서 후타스킨을 읽습니다.(더블클릭 적용)") &&
        !uiSource.contains("usage.managed") && uiSource.contains("usage.applied, usage.capacity"));
    const auto discardBegin = uiSource.find("void DiscardDistributionDraft()");
    const auto discardEnd = uiSource.find("const char* OverlayAreaLabel", discardBegin);
    const auto discardBody = std::string_view(uiSource).substr(discardBegin, discardEnd-discardBegin);
    Require(discardBody.contains("SavedRulesSnapshot()") && !discardBody.contains("SaveRulesForNextGame") &&
        !discardBody.contains("SetRules(") && !discardBody.contains("RefreshDistributionTargetOptions"));
    Require(!uiSource.contains("CommitPendingSelections"));
    Require(uiSource.contains("ImGuiKey_W") && uiSource.contains("ImGuiKey_S") &&
        uiSource.contains("ImGuiKey_A") && uiSource.contains("ImGuiKey_D") &&
        uiSource.contains("native_ui::ActivatePressed()") && uiSource.contains("ImGuiKey_Enter") &&
        uiSource.contains("ImGuiKey_GamepadDpadUp") &&
        uiSource.contains("ImGuiKey_GamepadDpadDown") &&
        uiSource.contains("ImGuiKey_GamepadDpadLeft") &&
        uiSource.contains("ImGuiKey_GamepadDpadRight") &&
        uiSource.contains("native_ui::CancelPressed()"));
    Require(uiSource.contains("g_pendingFutanari") && uiSource.contains("g_pendingTintBaseline"));
    Require(uiSource.contains("SkinProfiles::Get().RefreshAsync()"));
    Require(uiSource.contains("overlay::RefreshCatalog(actor)"));
    Require(uiSource.contains("Reads body skins from BodySkin\\\\<skin pack>\\\\textures\\\\~. (Double-click to apply)"));
    Require(uiSource.contains("Reads tint masks from BodySkin\\\\<skin pack>\\\\textures\\\\~. (Double-click to apply)"));
    Require(uiSource.contains("Rotate: right-mouse drag / LT+RS left/right") &&
        uiSource.contains("bcn::ui_text::TitleBarRightHint(\"Body Change NG\", hint)") &&
        uiSource.contains("DrawTitleBarRotationHint();") &&
        !uiSource.contains("if (x <= titleEnd) return;") &&
        !uiSource.contains("ImGui::TextDisabled(\"%s\", Text(\n            \"캐릭터 회전"));
    Require(uiSource.contains("DrawCatalogCommandRow(DistributionPool::body") &&
        uiSource.contains("DrawCatalogCommandRow(DistributionPool::skin") &&
        uiSource.contains("DrawCatalogCommandRow(DistributionPool::overlay") &&
        uiSource.contains("DrawCatalogCommandRow(DistributionPool::futanari") &&
        uiSource.contains("Distribution NPC conditions") &&
        uiSource.contains("Cancel distribution") &&
        uiSource.contains("Exclude custom followers") &&
        uiSource.contains("Exclude elder NPCs") &&
        uiSource.contains("return Text(\"이름\", \"Name\", \"名称\")") &&
        !uiSource.contains("이름이 같은 NPC") &&
        !uiSource.contains("ImportOBodyDefaults") &&
        !uiSource.contains("Load saved values") &&
        !uiSource.contains("배포 제외"));

    const auto openDistribution = uiSource.find("void OpenDistributionEditorFromCatalog()");
    const auto commandRow = uiSource.find("void DrawCatalogCommandRow", openDistribution);
    Require(openDistribution != std::string::npos && commandRow != std::string::npos);
    const auto openDistributionBody = std::string_view(uiSource).substr(
        openDistribution, commandRow - openDistribution);
    Require(openDistributionBody.contains("SetRuleDistributionSelection(rule)") &&
        !openDistributionBody.contains("ClearDistributionCatalogSelection()"));
    const auto addRuleBegin = uiSource.find("+ Add rule");
    const auto deleteRuleBegin = uiSource.find("- Delete rule", addRuleBegin);
    Require(addRuleBegin != std::string::npos && deleteRuleBegin != std::string::npos);
    const auto addRuleBody = std::string_view(uiSource).substr(
        addRuleBegin, deleteRuleBegin - addRuleBegin);
    Require(addRuleBody.contains("SetRuleDistributionSelection(rule)") &&
        !addRuleBody.contains("source.presetIds") &&
        !addRuleBody.contains("source.skinProfileIds"));
    Require(uiSource.contains("SameLine(ImGui::GetWindowContentRegionMax().x - closeWidth)"));
    Require(uiSource.contains("File path: Data\\\\SKSE\\\\Plugins\\\\OBody_presetDistributionConfig.json"));
    Require(uiSource.contains("선택 액터 설정 값 초기화") &&
        uiSource.contains("전체 액터 설정 값 초기화") &&
        uiSource.contains("actor_settings_reset::QueueActor(SelectedActor())") &&
        uiSource.contains("actor_settings_reset::QueueAll()") &&
        !uiSource.contains("선택 액터 바디 모프 초기화") &&
        !uiSource.contains("전체 배포 바디 결과 초기화"));
    Require(uiSource.find("RightAlignedButton(Text(\"완료\", \"Done\", \"完成\"))") !=
        uiSource.rfind("RightAlignedButton(Text(\"완료\", \"Done\", \"完成\"))"));

    std::ifstream distributionHeaderFile(std::filesystem::path("src") / "BodyChangeNG" /
        "Distribution.h", std::ios::binary);
    Require(distributionHeaderFile.good());
    const std::string distributionHeader((std::istreambuf_iterator<char>(distributionHeaderFile)), {});
    Require(!distributionHeader.contains("bodyExcluded") &&
        !distributionHeader.contains("skinExcluded") &&
        !distributionHeader.contains("overlayExcluded") &&
        !distributionHeader.contains("bool excluded"));
    std::ifstream packagedRulesFile(std::filesystem::path("package") / "SKSE" /
        "Plugins" / "BodyChangeNGdistribution.json", std::ios::binary);
    Require(packagedRulesFile.good());
    const std::string packagedRules((std::istreambuf_iterator<char>(packagedRulesFile)), {});
    Require(packagedRules.contains("\"schemaVersion\": 7") &&
        packagedRules.contains("\"rules\": []") &&
        !packagedRules.contains("Excluded") &&
        !packagedRules.contains("excluded"));
    std::ifstream packageScriptFile(std::filesystem::path("scripts") /
        "Package-Release.ps1", std::ios::binary);
    Require(packageScriptFile.good());
    const std::string packageScript((std::istreambuf_iterator<char>(packageScriptFile)), {});
    Require(packageScript.contains("schemaVersion -ne 7") &&
        packageScript.contains("rules.Count -ne 0") &&
        !packageScript.contains("default-exclude-") &&
        !packageScript.contains("TintMask/README.txt"));

    const auto skinCatalog = uiSource.find("void DrawSkinCatalog()");
    const auto skinPreview = uiSource.find("const auto previewRow", skinCatalog);
    const auto skinConfirm = uiSource.find("const auto confirmRow", skinPreview);
    Require(skinCatalog != std::string::npos && skinPreview != std::string::npos &&
        skinConfirm != std::string::npos);
    const auto skinPreviewBody = std::string_view(uiSource).substr(
        skinPreview, skinConfirm - skinPreview);
    Require(skinPreviewBody.contains("QueueDefaultSkin(false)"));
    Require(!skinPreviewBody.contains("SaveManualSkinIfNeeded"));
    Require(uiSource.contains("QueuePreviewDefault(actor)"));
    const auto manualSkinHelper = uiSource.find("void SaveManualSkinIfNeeded");
    const auto manualDefaultBodyHelper = uiSource.find("void SaveManualDefaultBodyIfNeeded", manualSkinHelper);
    const auto manualDefaultSkinHelper = uiSource.find("void SaveManualDefaultSkinIfNeeded", manualDefaultBodyHelper);
    const auto pendingHelper = uiSource.find("void RememberPending", manualDefaultSkinHelper);
    Require(manualSkinHelper != std::string::npos && manualDefaultBodyHelper != std::string::npos &&
        manualDefaultSkinHelper != std::string::npos && pendingHelper != std::string::npos);
    const auto manualPersistenceHelpers = std::string_view(uiSource).substr(
        manualSkinHelper, pendingHelper - manualSkinHelper);
    Require(!manualPersistenceHelpers.contains("actor == player") &&
        manualPersistenceHelpers.contains("SetManualSkinAssignment") &&
        manualPersistenceHelpers.contains("SetManualDefaultBody") &&
        manualPersistenceHelpers.contains("SetManualDefaultSkin"));

    const auto mainTabs = uiSource.find("HandleTabNavigation(playerSelected, futanariAvailable)");
    Require(!uiSource.contains("선택 액터 적용 중") &&
        !uiSource.contains("Applying to selected actor") &&
        !uiSource.contains("선택 액터 적용 대기 중"));
    const auto bodyTab = uiSource.find("TabButton(Text(\"바디프리셋\"", mainTabs);
    const auto skinTab = uiSource.find("TabButton(Text(\"바디스킨\"", bodyTab);
    const auto tintTab = uiSource.find("TabButton(Text(\"틴트마스크\"", skinTab);
    const auto futanariTab = uiSource.find("TabButton(Text(\"후타스킨\"", tintTab);
    const auto overlayTab = uiSource.find("TabButton(Text(\"오버레이\"", futanariTab);
    Require(mainTabs != std::string::npos && bodyTab < skinTab && skinTab < tintTab &&
        tintTab < futanariTab && futanariTab < overlayTab);

    const auto overlayCatalogUi = uiSource.find("void DrawOverlayCatalog()");
    const auto futanariCatalogUi = uiSource.find("void DrawFutanariCatalog()", overlayCatalogUi);
    Require(overlayCatalogUi != std::string::npos && futanariCatalogUi != std::string::npos);
    const auto futanariCatalogEnd = uiSource.find("void DrawPlayerTintCatalog()", futanariCatalogUi);
    const auto futanariDistributionMode = uiSource.find(
        "const auto distributionSelecting = IsDistributionSelectionFor(DistributionPool::futanari)",
        futanariCatalogUi);
    const auto futanariCommandRow = uiSource.find(
        "DrawCatalogCommandRow(DistributionPool::futanari", futanariDistributionMode);
    const auto futanariActorRejection = uiSource.find(
        "if (!distributionSelecting && !actorType)", futanariCommandRow);
    Require(futanariCatalogEnd != std::string::npos &&
        futanariDistributionMode < futanariCommandRow &&
        futanariCommandRow < futanariActorRejection &&
        futanariActorRejection < futanariCatalogEnd);
    const auto overlayCatalogBody = std::string_view(uiSource).substr(
        overlayCatalogUi, futanariCatalogUi - overlayCatalogUi);
    Require(overlayCatalogBody.contains("ImGui::TreeNodeEx") &&
        overlayCatalogBody.contains("g_overlaySectionsOpen") &&
        overlayCatalogBody.contains("ImGuiListClipper") &&
        overlayCatalogBody.contains("Face Paint") &&
        overlayCatalogBody.contains("Body Paint") &&
        overlayCatalogBody.contains("Hand Paint") &&
        overlayCatalogBody.contains("Feet Paint") &&
        overlayCatalogBody.contains("EllipsizeText(subtitle, subtitleWidth)") &&
        overlayCatalogBody.contains("renderedRowBottom") &&
        overlayCatalogBody.contains("sectionCursor") &&
        !overlayCatalogBody.contains("← Areas") &&
        !overlayCatalogBody.contains("← 부위 선택"));
    const auto overlayCatalogEndChild = overlayCatalogBody.rfind("ImGui::EndChild()");
    const auto overlayFixedColor = overlayCatalogBody.rfind(
        "const auto color = bcn::overlay::CurrentColor(actor, colorArea, colorEntryId)");
    Require(overlayCatalogEndChild != std::string_view::npos &&
        overlayFixedColor != std::string_view::npos &&
        overlayFixedColor > overlayCatalogEndChild &&
        overlayCatalogBody.contains("Color area"));
    Require(!uiSource.contains("틴트마스크 방식만 호환되며") &&
        !uiSource.contains("Only the tint-mask method is compatible"));
    Require(uiSource.contains("CenteredCheckbox(\"##distributionSelected\"") &&
        uiSource.contains("ImGui::AlignTextToFramePadding();"));

    std::ifstream overlayFile(std::filesystem::path("src") / "BodyChangeNG" /
        "RaceMenuOverlay.cpp", std::ios::binary);
    Require(overlayFile.good());
    const std::string overlaySource((std::istreambuf_iterator<char>(overlayFile)), {});
    const auto colorBegin = overlaySource.find("ApplyResult QueueColor(");
    const auto colorEnd = overlaySource.find("ApplyResult QueueClear(", colorBegin);
    Require(colorBegin != std::string::npos && colorEnd != std::string::npos);
    const auto colorBody = std::string_view(overlaySource).substr(
        colorBegin, colorEnd - colorBegin);
    Require(colorBody.contains("coloringPreview") &&
        colorBody.contains("StorePreview") &&
        colorBody.contains("coloringPreview ? ApplyMode::preview") &&
        colorBody.contains("NodeOwnedBySelection"));

    std::ifstream presentationFile(std::filesystem::path("src") / "BodyChangeNG" /
        "MenuCharacterPresentation.cpp", std::ios::binary);
    Require(presentationFile.good());
    const std::string presentationSource((std::istreambuf_iterator<char>(presentationFile)), {});
    const auto rotationBegin = presentationSource.find("void Presentation::UpdateRotationInteraction()");
    Require(rotationBegin != std::string::npos);
    const auto rotationBody = std::string_view(presentationSource).substr(rotationBegin);
    Require(rotationBody.contains("ImGuiKey_GamepadL2") &&
        rotationBody.contains("ImGuiKey_GamepadRStickLeft") &&
        rotationBody.contains("ImGuiKey_GamepadRStickRight") &&
        rotationBody.contains("leftTrigger >= kGamepadTriggerThreshold") &&
        rotationBody.contains("std::clamp(io.DeltaTime, 0.0F, 0.05F)"));

    std::ifstream inputFilterFile(std::filesystem::path("src") / "BodyChangeNG" /
        "TextInputFilter.cpp", std::ios::binary);
    Require(inputFilterFile.good());
    const std::string inputFilterSource((std::istreambuf_iterator<char>(inputFilterFile)), {});
    Require(inputFilterSource.contains("kMenuNavigationButtons") &&
        inputFilterSource.contains("SubmitMenuNavigationKey(scanCode, !release)") &&
        inputFilterSource.contains("g_preMenuNavigationButtons") &&
        inputFilterSource.contains("g_swallowedMenuNavigationUntilReleaseButtons") &&
        inputFilterSource.contains("*link = event->next"));
    Require(inputFilterSource.contains("GetMappedKey(\"Activate\", device") &&
        inputFilterSource.contains("GetMappedKey(\"Cancel\", device") &&
        inputFilterSource.contains("bindingsFor(RE::INPUT_DEVICE::kKeyboard)") &&
        inputFilterSource.contains("bindingsFor(RE::INPUT_DEVICE::kGamepad)"));
    const auto navigationStart = uiSource.find("CatalogNavigationCommand HandleCatalogNavigation");
    const auto navigationEnd = uiSource.find("void FocusCatalogRow", navigationStart);
    const auto navigationSource = std::string_view(uiSource).substr(navigationStart,
        navigationEnd - navigationStart);
    Require(navigationSource.contains("native_ui::ActivatePressed()") &&
        !navigationSource.contains("ImGuiKey_E,") &&
        !navigationSource.contains("ImGuiKey_Space") &&
        !navigationSource.contains("ImGuiKey_GamepadFaceDown"));
    Require(uiSource.contains("native_ui::CancelPressed()") &&
        !uiSource.contains("ImGuiKey_GamepadFaceRight"));

    std::ifstream catalogFile(std::filesystem::path("src") / "BodyChangeNG" /
        "OverlayCatalog.cpp", std::ios::binary);
    Require(catalogFile.good());
    const std::string catalogSource((std::istreambuf_iterator<char>(catalogFile)), {});
    Require(catalogSource.contains("g_catalogSettled") &&
        catalogSource.contains("g_slaveTatsLoaded") &&
        catalogSource.contains("BeginCatalogRequest(target, false)") &&
        catalogSource.contains("BeginCatalogRequest(target, true)"));

    std::ifstream profilesFile(std::filesystem::path("src") / "BodyChangeNG" /
        "SkinProfiles.cpp", std::ios::binary);
    Require(profilesFile.good());
    const std::string profilesSource((std::istreambuf_iterator<char>(profilesFile)), {});
    Require(profilesSource.contains("bool SkinProfiles::RefreshAsync()") &&
        profilesSource.contains("catalog_refresh::Get().Submit(this") &&
        profilesSource.contains("refreshing_.exchange"));

    std::ifstream assetCacheFile(std::filesystem::path("src") / "BodyChangeNG" /
        "RuntimeAssetCache.cpp", std::ios::binary);
    Require(assetCacheFile.good());
    const std::string assetCacheSource((std::istreambuf_iterator<char>(assetCacheFile)), {});
    const auto registerBegin = assetCacheSource.find("void RegisterGameRelativeSource(");
    const auto registerEnd = assetCacheSource.find("std::uint64_t SourceContentHash(", registerBegin);
    Require(registerBegin != std::string::npos && registerEnd != std::string::npos);
    const auto registerBody = std::string_view(assetCacheSource).substr(
        registerBegin, registerEnd - registerBegin);
    Require(registerBody.contains("File hashing is intentionally outside") &&
        registerBody.find("HashFile(hash, stableSource)") >
            registerBody.find("File hashing is intentionally outside"));

    std::ifstream faceBatchFile(std::filesystem::path("src") / "BodyChangeNG" /
        "FaceSkinOverrides.cpp", std::ios::binary);
    Require(faceBatchFile.good());
    const std::string faceBatchSource((std::istreambuf_iterator<char>(faceBatchFile)), {});
    const auto currentBegin = faceBatchSource.find("bool Current()");
    const auto actorBegin = faceBatchSource.find("RE::NiPointer<RE::Actor> Actor()", currentBegin);
    Require(currentBegin != std::string::npos && actorBegin != std::string::npos);
    const auto currentBody = std::string_view(faceBatchSource).substr(
        currentBegin, actorBegin - currentBegin);
    Require(currentBody.contains("OwnsActiveBatch") &&
        currentBody.contains("activeGeneration") &&
        !currentBody.contains("it->second.generation == generation"));

    std::ifstream morphFile(std::filesystem::path("src") / "BodyChangeNG" /
        "RaceMenuBodyMorph.cpp", std::ios::binary);
    Require(morphFile.good());
    const std::string morphSource((std::istreambuf_iterator<char>(morphFile)), {});
    const auto defaultPreviewBegin = morphSource.find("ApplyResult QueuePreviewDefault(");
    const auto defaultPreviewEnd = morphSource.find("void QueueReapplyCurrent(", defaultPreviewBegin);
    Require(defaultPreviewBegin != std::string::npos && defaultPreviewEnd != std::string::npos);
    const auto defaultPreviewBody = std::string_view(morphSource).substr(
        defaultPreviewBegin, defaultPreviewEnd - defaultPreviewBegin);
    Require(defaultPreviewBody.contains("kPreviewKey, -value"));
    Require(!defaultPreviewBody.contains(
        "ClearBodyMorphKeys(resolved.get(), kCommittedKey)"));
    Require(!defaultPreviewBody.contains(
        "ClearBodyMorphKeys(resolved.get(), kLegacyOBodyKey)"));
    // These are production features, not opt-in diagnostic build variants.
    std::ifstream nativeFeatureFile(std::filesystem::path("src") / "BodyChangeNG" /
        "NativeAddonSkinBackend.cpp", std::ios::binary);
    Require(nativeFeatureFile.good());
    const std::string nativeFeatureSource((std::istreambuf_iterator<char>(nativeFeatureFile)), {});
    Require(!nativeFeatureSource.contains("BODY_CHANGE_NG_NATIVE_ADDON_TXST_TRIAL") &&
        nativeFeatureSource.contains("patterns::ae") &&
        nativeFeatureSource.contains("MatchRelocatedCode(") &&
        nativeFeatureSource.contains("ResolveRuntimeLayout(version)") &&
        nativeFeatureSource.contains("RtlLookupFunctionEntry") &&
        nativeFeatureSource.contains("visitor call sites already changed"));
    std::ifstream mainFeatureFile(std::filesystem::path("src") / "main.cpp", std::ios::binary);
    Require(mainFeatureFile.good());
    const std::string mainFeatureSource((std::istreambuf_iterator<char>(mainFeatureFile)), {});
    Require(mainFeatureSource.contains("racemenu_form_delete::InstallInGame()") &&
        !mainFeatureSource.contains("BODY_CHANGE_NG_FORM_DELETE_GUARD_TRIAL"));
    Require(mainFeatureSource.find("(void)bcn::catalog_refresh::Get()") >
        mainFeatureSource.find("bcn::player_tint::Catalog::Get().Refresh()"));
    Require(profilesSource.contains("~ClearRefreshFlag()") &&
        !profilesSource.contains("AuditProfileDds") &&
        !morphSource.contains("LogBodyTriState") &&
        !faceBatchSource.contains("FaceGPU"));
    std::cout<<"CodePatternTests passed: callback, PE bounds, face queue, and preview lifecycle\n";
}
