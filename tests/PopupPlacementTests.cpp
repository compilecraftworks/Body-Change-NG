#include "BodyChangeNG/PopupPlacementUI.h"
#include "BodyChangeNG/FittedTextUI.h"

#include <cstdlib>
#include <iostream>
#include <source_location>
#include <string>

namespace
{
    void Require(const bool ok, const std::source_location location = std::source_location::current())
    {
        if (ok) return;
        std::cerr << "PopupPlacementTests failed at line " << location.line() << '\n';
        std::abort();
    }
}

int main()
{
    using namespace bcn::popup_placement;
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1920.0F, 1080.0F);
    unsigned char* pixels{};
    int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    Modals modals;
    Positions saved{};
    ImVec2 lastPosition{}, lastSize{};
    bool wrappedOutfit{};
    const auto tick = [&](const Kind kind, const bool show, const bool close, const bool drag = false) {
        const auto index = static_cast<std::size_t>(kind);
        io.MouseDown[0] = drag;
        ImGui::NewFrame();
        ImGui::Begin("Host");
        if (show) {
            const auto title = std::string(keys[index]) +
                (wrappedOutfit ? "###WrappedOutfitTest" : "###PlacementTest");
            if (!ImGui::IsPopupOpen(title.c_str())) ImGui::OpenPopup(title.c_str());
            bool open = true;
            if (wrappedOutfit) {
                const auto popupWidth = 700.0F * 1.875F;
                ImGui::SetNextWindowSizeConstraints(ImVec2(popupWidth, 0.0F),
                    ImVec2(popupWidth, io.DisplaySize.y * 0.9F));
            }
            const auto result = modals.Begin(title.c_str(), &open,
                ImGuiWindowFlags_AlwaysAutoResize, kind, saved[index]);
            if (result.began) {
                if (result.settledPosition.set) {
                    Require(!drag);
                    saved[index] = result.settledPosition;
                }
                if (wrappedOutfit) {
                    bool enabled = true;
                    ImGui::TextDisabled("Only applies to supported BodySlide sliders");
                    ImGui::Separator();
                    ImGui::Checkbox("Correct breasts while clothed", &enabled);
                    ImGui::Indent();
                    ImGui::Checkbox("Correct nipples while clothed", &enabled);
                    ImGui::Unindent();
                    ImGui::Button("Register OBody NG outfit-correction rules");
                    ImGui::SameLine();
                    ImGui::TextDisabled("OBody NG outfit-correction rules");
                    ImGui::TextWrapped("File path: Data\\SKSE\\Plugins\\OBody_presetDistributionConfig.json");
                    ImGui::Separator();
                    ImGui::Checkbox("Randomize NPC nipple shape", &enabled);
                    ImGui::Checkbox("Randomize NPC genital shape", &enabled);
                } else {
                    ImGui::Dummy(ImVec2(500.0F, 260.0F));
                }
                lastPosition = ImGui::GetWindowPos();
                lastSize = ImGui::GetWindowSize();
                if (close) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
        ImGui::End();
        ImGui::Render();
    };

    for (std::size_t index{}; index < keys.size(); ++index) {
        const auto kind = static_cast<Kind>(index);
        for (int frame{}; frame < 4; ++frame) tick(kind, true, false);
        Require(std::abs(lastPosition.x + lastSize.x * 0.5F - 960.0F) <= 1.0F);
        Require(std::abs(lastPosition.y + lastSize.y * 0.5F - 540.0F) <= 1.0F);
        tick(kind, true, true);
        tick(kind, false, false);
        saved[index] = { true, 110.0F + static_cast<float>(index) * 35.0F, 170.0F };
        const auto desired = saved[index];
        for (int reopen{}; reopen < 4; ++reopen) {
            for (int frame{}; frame < 4; ++frame) tick(kind, true, false);
            Require(lastPosition.x == desired.x && lastPosition.y == desired.y);
            tick(kind, true, false, true);
            tick(kind, true, true);
            tick(kind, false, false);
        }
    }

    // Saved 4K coordinates must remain accessible after switching to 720p.
    saved[0] = { true, 3200.0F, 1800.0F };
    io.DisplaySize = ImVec2(1280.0F, 720.0F);
    for (int frame{}; frame < 4; ++frame) tick(Kind::outfit, true, false);
    Require(lastPosition.x >= 0.0F && lastPosition.x + lastSize.x <= 1280.0F);
    Require(lastPosition.y >= 0.0F && lastPosition.y + lastSize.y <= 720.0F);
    tick(Kind::outfit, true, true);
    tick(Kind::outfit, false, false);

    // The outfit popup contains wrapped text in an auto-sized window, unlike
    // the fixed-content rectangle in the generic placement checks above.
    wrappedOutfit = true;
    io.DisplaySize = ImVec2(3840.0F, 2160.0F);
    io.FontGlobalScale = 1.875F;
    saved[0] = {};
    for (int frame{}; frame < 5; ++frame) {
        tick(Kind::outfit, true, false);
        if (frame >= 1) {
            Require(std::abs(lastPosition.y + lastSize.y * 0.5F - 1080.0F) <= 1.0F);
        }
    }
    Require(std::abs(lastPosition.y + lastSize.y * 0.5F - 1080.0F) <= 1.0F);
    tick(Kind::outfit, true, true);
    tick(Kind::outfit, false, false);
    wrappedOutfit = false;
    io.DisplaySize = ImVec2(1280.0F, 720.0F);
    io.FontGlobalScale = 1.0F;

    // Use the actual draw helper: every width stays one row high and the
    // complete string's glyph vertices fit without covering the next button.
    constexpr auto help = "Reads installed mods. (Double-click to apply/remove multiple)";
    int expectedVertices{};
    for (const auto availableWidth : { 900.0F, 450.0F, 230.0F, 100.0F, 23.0F }) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
        ImGui::SetNextWindowSize(ImVec2(1100.0F, 400.0F));
        ImGui::Begin("Fit test");
        ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - availableWidth);
        const auto origin = ImGui::GetCursorScreenPos();
        const auto rowHeight = ImGui::GetFrameHeight();
        auto* draw = ImGui::GetWindowDrawList();
        const auto first = draw->VtxBuffer.Size;
        bcn::ui_text::FittedDisabledLine(help);
        Require(ImGui::GetItemRectSize().y == rowHeight);
        if (expectedVertices == 0) expectedVertices = draw->VtxBuffer.Size - first;
        Require(draw->VtxBuffer.Size - first == expectedVertices && expectedVertices > 0);
        for (auto vertex = first; vertex < draw->VtxBuffer.Size; ++vertex) {
            const auto point = draw->VtxBuffer[vertex].pos;
            Require(point.x >= origin.x - 1.0F && point.x <= origin.x + availableWidth + 1.0F);
            Require(point.y >= origin.y - 1.0F && point.y <= origin.y + rowHeight + 1.0F);
        }
        ImGui::End();
        ImGui::Render();
    }
    // Title decoration must survive the content clip installed by Begin().
    // At narrow widths every character must still emit geometry, with no
    // overlap over the title/X and no extra body row or font-size cache entry.
    constexpr auto titleHint = "Rotate: right-mouse drag / LT+RS left/right";
    for (const auto fontScale : {1.0F, 1.25F, 1.875F}) {
        io.FontGlobalScale = fontScale;
        int expectedTitleVertices{};
        for (const auto windowWidth : {1000.0F, 700.0F, 480.0F, 360.0F}) {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(30.0F, 40.0F));
            ImGui::SetNextWindowSize(ImVec2(windowWidth, 350.0F));
            bool open = true;
            ImGui::Begin("Body Change NG", &open, ImGuiWindowFlags_NoCollapse);
            const auto cursor = ImGui::GetCursorScreenPos();
            auto* draw = ImGui::GetWindowDrawList();
            const auto oldClipMin = draw->GetClipRectMin(), oldClipMax = draw->GetClipRectMax();
            const auto firstVertex = draw->VtxBuffer.Size;
            bcn::ui_text::TitleBarRightHint("Body Change NG", titleHint);
            const auto count = draw->VtxBuffer.Size - firstVertex;
            if (!expectedTitleVertices) expectedTitleVertices = count;
            Require(count > 0 && count == expectedTitleVertices);
            Require(ImGui::GetCursorScreenPos().x == cursor.x && ImGui::GetCursorScreenPos().y == cursor.y);
            Require(draw->GetClipRectMin().x == oldClipMin.x && draw->GetClipRectMin().y == oldClipMin.y);
            Require(draw->GetClipRectMax().x == oldClipMax.x && draw->GetClipRectMax().y == oldClipMax.y);
            const auto& style = ImGui::GetStyle();
            const auto left = 30.0F + style.WindowBorderSize + style.FramePadding.x +
                ImGui::CalcTextSize("Body Change NG").x + style.ItemInnerSpacing.x * 2.0F;
            const auto right = 30.0F + windowWidth - style.WindowBorderSize -
                style.FramePadding.x - ImGui::GetFontSize() - style.ItemInnerSpacing.x;
            const auto bottom = 40.0F + ImGui::GetFrameHeight();
            float rightmost{};
            for (auto vertex = firstVertex; vertex < draw->VtxBuffer.Size; ++vertex) {
                const auto point = draw->VtxBuffer[vertex].pos;
                Require(point.x >= left - 1.0F && point.x <= right + 1.0F);
                Require(point.y >= 40.0F && point.y <= bottom);
                rightmost = (std::max)(rightmost, point.x);
            }
            Require(right - rightmost <= ImGui::GetFontSize());
            // Verify the actual command clipping, not only vertex positions:
            // the old implementation had valid coordinates but no visible text.
            std::size_t titleIndices{};
            for (const auto& command : draw->CmdBuffer) {
                if (command.ClipRect.x < left - 1.0F || command.ClipRect.y != 40.0F ||
                    command.ClipRect.w != bottom) continue;
                Require(command.ClipRect.z <= right + 1.0F);
                titleIndices += command.ElemCount;
            }
            Require(titleIndices == static_cast<std::size_t>(count / 4 * 6));
            ImGui::End();
            ImGui::Render();
        }
    }
    ImGui::DestroyContext();
    std::cout << "PopupPlacementTests passed: 8 popup positions, centering, repeated reopen, drag guard, smaller viewport, fitted one-line help, title hint clip/alignment/scaling\n";
}
