#pragma once

#include "BodyChangeNG/PopupPlacement.h"
#include <imgui.h>

namespace bcn::popup_placement
{
    struct ModalFrame
    {
        bool began{};
        Position settledPosition{};
    };

    class Modals
    {
    public:
        [[nodiscard]] ModalFrame Begin(const char* title, bool* open, const ImGuiWindowFlags flags,
            const Kind kind, const Position saved)
        {
            const auto* viewport = ImGui::GetMainViewport();
            auto& appearingFrame = appearingFrames_[static_cast<std::size_t>(kind)];
            const auto frame = ImGui::GetFrameCount();
            if (saved.set) {
                // Reopened auto-sized popups get a hidden measuring frame.
                // Restore once more afterwards, before ImGui's auto-place pass.
                ImGui::SetNextWindowPos(ImVec2(saved.x, saved.y),
                    appearingFrame > 0 && frame == appearingFrame + 1 ? ImGuiCond_Always : ImGuiCond_Appearing);
            } else if (viewport) {
                ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
            }
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
            ModalFrame result{ ImGui::BeginPopupModal(title, open, flags), {} };
            ImGui::PopStyleColor();
            if (result.began && ImGui::IsWindowAppearing()) appearingFrame = frame;
            // Never persist a temporary measuring position, drag-in-progress,
            // or minimized viewport. Let the caller store a settled change.
            if (result.began && !ImGui::IsWindowAppearing() && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                viewport && viewport->WorkSize.x > 0.0F && viewport->WorkSize.y > 0.0F) {
                const auto current = ImGui::GetWindowPos();
                const auto size = ImGui::GetWindowSize();
                const ImVec2 position{
                    ClampAxis(current.x, size.x, viewport->WorkPos.x, viewport->WorkSize.x),
                    ClampAxis(current.y, size.y, viewport->WorkPos.y, viewport->WorkSize.y)
                };
                if (position.x != current.x || position.y != current.y) ImGui::SetWindowPos(position);
                result.settledPosition = { true, position.x, position.y };
            }
            return result;
        }

    private:
        std::array<int, keys.size()> appearingFrames_{};
    };
}
