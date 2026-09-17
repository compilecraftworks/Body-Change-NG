#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <array>
#include <mutex>
#include <vector>

namespace bcn::native_ui
{
    // Skyrim supplies edges, never Windows positions. Only the render thread writes
    // ImGui input, using the same game cursor position for hover and clicks.
    class MouseInputQueue final
    {
        struct Event { int button; bool down; float wheel; };
        std::mutex lock_;
        std::vector<Event> events_;
        std::array<bool, ImGuiMouseButton_COUNT> buttons_{};
        bool release_{};
    public:
        void Button(int button, bool down)
        {
            if (button < 0 || button >= ImGuiMouseButton_COUNT) return;
            std::scoped_lock lock(lock_);
            if (buttons_[button] == down) return;
            PushLocked({button, down, 0});
            buttons_[button] = down;
        }
        void Wheel(float amount) { Push({-1, false, amount}); }
        void GameButton(unsigned button, bool down, bool up)
        {
            // Skyrim DI mouse IDs: 0..7 buttons, 8/9 vertical wheel. Ignore
            // held/repeat states and wheel releases; preserve every real edge.
            if (button < ImGuiMouseButton_COUNT) {
                if (up) Button(static_cast<int>(button), false);
                else if (down) Button(static_cast<int>(button), true);
            } else if (down && (button == 8 || button == 9)) {
                Wheel(button == 8 ? 1.0F : -1.0F);
            }
        }
        void Reset()
        {
            std::scoped_lock lock(lock_);
            events_.clear();
            buttons_.fill(false);
            release_ = true;
        }
        void Push(Event event)
        {
            std::scoped_lock lock(lock_);
            PushLocked(event);
        }
    private:
        void PushLocked(Event event)
        {
            if (events_.size() >= 4096) { events_.clear(); buttons_.fill(false); release_ = true; }
            events_.push_back(event);
        }
    public:
        void Drain(ImGuiIO& io, ImVec2 position)
        {
            std::vector<Event> pending;
            bool release;
            {
                std::scoped_lock lock(lock_);
                pending.swap(events_);
                release = release_;
                release_ = false;
            }
            if (release) {
                auto& queue = ImGui::GetCurrentContext()->InputEventsQueue;
                for (int i = queue.Size - 1; i >= 0; --i)
                    if (queue[i].Type == ImGuiInputEventType_MousePos ||
                        queue[i].Type == ImGuiInputEventType_MouseButton ||
                        queue[i].Type == ImGuiInputEventType_MouseWheel) queue.erase(queue.Data + i);
                io.ClearInputMouse();
                for (int b = 0; b < ImGuiMouseButton_COUNT; ++b) {
                    io.MouseClickedTime[b] = -1.0e30;
                    io.MouseClickedLastCount[b] = io.MouseClickedCount[b] = 0;
                    io.MouseDoubleClicked[b] = false;
                }
            }
            io.AddMouseSourceEvent(ImGuiMouseSource_Mouse);
            io.AddMousePosEvent(position.x, position.y);
            for (const auto& event : pending) {
                if (event.button >= 0 && event.button < ImGuiMouseButton_COUNT)
                    io.AddMouseButtonEvent(event.button, event.down);
                else if (event.button == -1) io.AddMouseWheelEvent(0, event.wheel);
            }
        }
    };

    // Win32 NewFrame may enqueue its OS-cursor fallback. Remove ONLY the new
    // backend positions; retain older game-position events waiting in ImGui's
    // trickle queue, and leave keyboard/gamepad events untouched.
    inline void RemoveBackendMousePositions(int firstNewEvent)
    {
        auto& events = ImGui::GetCurrentContext()->InputEventsQueue;
        for (int i = events.Size - 1; i >= firstNewEvent; --i)
            if (events[i].Type == ImGuiInputEventType_MousePos) events.erase(events.Data + i);
    }
}
