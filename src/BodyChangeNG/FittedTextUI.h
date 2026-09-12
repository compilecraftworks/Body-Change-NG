#pragma once

#include <imgui.h>
#include <algorithm>

namespace bcn::ui_text
{
    // Main window only: NoCollapse, left-aligned title, standard close button.
    // This is decoration, not a layout item or an input target. Begin() has
    // already installed the CONTENT clip; title text needs its own clip.
    inline void TitleBarRightHint(const char* title, const char* text)
    {
        const auto position = ImGui::GetWindowPos();
        const auto size = ImGui::GetWindowSize();
        const auto& style = ImGui::GetStyle();
        const auto height = ImGui::GetFrameHeight();
        const auto left = position.x + style.WindowBorderSize + style.FramePadding.x +
            ImGui::CalcTextSize(title).x + style.ItemInnerSpacing.x * 2.0F;
        const auto right = position.x + size.x - style.WindowBorderSize -
            style.FramePadding.x - ImGui::GetFontSize() - style.ItemInnerSpacing.x;
        const auto measured = ImGui::CalcTextSize(text);
        const auto width = right - left;
        if (width <= 0.0F || measured.x <= 0.0F) return;
        const auto scale = (std::min)(1.0F, width / measured.x);
        const ImVec2 origin{ right - measured.x * scale,
            position.y + (height - measured.y * scale) * 0.5F };
        auto* draw = ImGui::GetWindowDrawList();
        // Render all glyphs with the existing baked font, then scale vertices.
        // A new font size per resized width would grow the font atlas/cache.
        draw->PushClipRect(origin, ImVec2(origin.x + measured.x + 1.0F,
            origin.y + measured.y + 1.0F), false);
        draw->AddDrawCmd();
        const auto firstCommand = draw->CmdBuffer.Size - 1;
        const auto firstVertex = draw->VtxBuffer.Size;
        draw->AddText(origin, ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
        for (auto index = firstVertex; index < draw->VtxBuffer.Size; ++index) {
            auto& point = draw->VtxBuffer[index].pos;
            point.x = origin.x + (point.x - origin.x) * scale;
            point.y = origin.y + (point.y - origin.y) * scale;
        }
        const auto* viewport = ImGui::GetMainViewport();
        for (auto index = firstCommand; index < draw->CmdBuffer.Size; ++index) {
            draw->CmdBuffer[index].ClipRect = ImVec4(
                (std::max)(left, viewport->Pos.x), (std::max)(position.y, viewport->Pos.y),
                (std::min)(right, viewport->Pos.x + viewport->Size.x),
                (std::min)(position.y + height, viewport->Pos.y + viewport->Size.y));
        }
        draw->PopClipRect();
    }

    inline void FittedDisabledLine(const char* text)
    {
        const auto width = (std::max)(0.0F, ImGui::GetContentRegionAvail().x);
        const auto height = ImGui::GetFrameHeight();
        const auto origin = ImGui::GetCursorScreenPos();
        const auto measured = ImGui::CalcTextSize(text);
        // Reserve exactly one button-height row, not the unscaled text width.
        ImGui::Dummy(ImVec2(width, height));
        if (width <= 0.0F || measured.x <= 0.0F) return;
        const auto scale = (std::min)(1.0F, width / measured.x);
        const ImVec2 position{ origin.x, origin.y + (height - measured.y * scale) * 0.5F };
        auto* draw = ImGui::GetWindowDrawList();
        if (scale == 1.0F) {
            draw->AddText(position, ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
        } else {
            // Reuse the current baked glyphs. Asking ImGui for a different font
            // size at each resized width would populate its font atlas/cache.
            const auto clipMin = draw->GetClipRectMin();
            const auto clipMax = draw->GetClipRectMax();
            draw->PushClipRect(position, ImVec2(position.x + measured.x + 1.0F,
                position.y + measured.y + 1.0F), false);
            draw->AddDrawCmd(); // Isolate commands whose vertices we transform.
            const auto firstCommand = draw->CmdBuffer.Size - 1;
            const auto firstVertex = draw->VtxBuffer.Size;
            draw->AddText(position, ImGui::GetColorU32(ImGuiCol_TextDisabled), text);
            for (auto index = firstVertex; index < draw->VtxBuffer.Size; ++index) {
                auto& point = draw->VtxBuffer[index].pos;
                point.x = position.x + (point.x - position.x) * scale;
                point.y = position.y + (point.y - position.y) * scale;
            }
            // Restore the real table/window clip on every emitted text command.
            for (auto index = firstCommand; index < draw->CmdBuffer.Size; ++index) {
                draw->CmdBuffer[index].ClipRect = ImVec4(clipMin.x, clipMin.y,
                    (std::min)(clipMax.x, origin.x + width), clipMax.y);
            }
            draw->PopClipRect();
        }
        if (scale < 1.0F && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            ImGui::SetTooltip("%s", text);
        }
    }
}
