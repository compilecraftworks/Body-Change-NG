#include <imgui.h>
#include "BodyChangeNG/MouseInputQueue.h"
#include <cstdlib>
#include <iostream>

// Replays the event ordering in NativeImGuiHost without a game or GPU.
// A mismatched-coordinate case is conditional evidence, not a claim that
// Windows and Skyrim cursors differ on every installation.
static int Replay(float fps, bool mismatchedCoordinates, int clicks, bool fixed = false)
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {800, 600};
    io.DeltaTime = 1.0F / fps;
    unsigned char* pixels{};
    int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    int accepted{};
    bcn::native_ui::MouseInputQueue mouse;
    const auto frame = [&] {
        if (fixed) {
            const auto first = ImGui::GetCurrentContext()->InputEventsQueue.Size;
            io.AddMousePosEvent(mismatchedCoordinates ? 500.0F : 120.0F, 120);
            bcn::native_ui::RemoveBackendMousePositions(first);
            mouse.Drain(io, {120, 120});
        }
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({800, 600});
        ImGui::Begin("Replay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        ImGui::SetCursorScreenPos({100, 100});
        if (ImGui::Button("Test button", {150, 40})) ++accepted;
        ImGui::End();
        ImGui::Render();
    };
    io.AddMousePosEvent(120, 120);
    frame(); frame();
    for (int i = 0; i < clicks; ++i) {
        // Game input edges arrive between rendered frames, with NO Win32
        // messages. This was the missing delivery path in 1.2.5's test.
        if (fixed) {
            mouse.GameButton(0, true, false);
            mouse.GameButton(0, false, true);
        } else {
            io.AddMousePosEvent(mismatchedCoordinates ? 500.0F : 120.0F, 120);
            io.AddMouseButtonEvent(0, true);
            io.AddMouseButtonEvent(0, false);
        }
        // PostDisplay samples the game MenuCursor before ImGui::NewFrame.
        if (!fixed) io.AddMousePosEvent(120, 120);
        for (int drain = 0; drain < 5; ++drain) frame();
    }
    ImGui::DestroyContext();
    return accepted;
}

static bool Lifecycle()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr; io.DisplaySize = {800, 600};
    io.DeltaTime = 1.0F / 60.0F;
    unsigned char* pixels{}; int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    io.Fonts->SetTexID(ImTextureID(1));
    bcn::native_ui::MouseInputQueue mouse;
    float frameWheel{};
    const auto tick = [&](ImVec2 position) {
        mouse.Drain(io, position);
        ImGui::NewFrame();
        frameWheel = io.MouseWheel; // EndFrame intentionally clears wheel deltas.
        ImGui::Begin("Lifecycle"); ImGui::TextUnformatted("input"); ImGui::End();
        ImGui::Render();
    };
    tick({120,120}); tick({120,120});
    for (unsigned b = 0; b < 5; ++b) mouse.GameButton(b, true, false);
    tick({120,120});
    bool ok = true;
    for (int b = 0; b < 5; ++b) ok = ok && io.MouseDown[b];
    if (!ok) std::cerr << "button-down failed\n";
    tick({150,120});
    ok = ok && ImGui::GetMouseDragDelta(0).x == 30 && ImGui::GetMouseDragDelta(1).x == 30;
    if (!ok) std::cerr << "drag failed: " << ImGui::GetMouseDragDelta(0).x << ',' << ImGui::GetMouseDragDelta(1).x << '\n';
    for (unsigned b = 0; b < 5; ++b) mouse.GameButton(b, false, true);
    tick({150,120});
    for (int b = 0; b < 5; ++b) ok = ok && !io.MouseDown[b];
    mouse.Wheel(1); tick({150,120});
    ok = ok && frameWheel == 1;
    if (!ok) std::cerr << "release/wheel failed: " << io.MouseDown[0] << ',' << io.MouseDown[1] << ',' << io.MouseWheel << '\n';
    mouse.Reset(); tick({120,120}); tick({120,120});
    mouse.Button(0, true); tick({120,120});
    mouse.Button(0, false); tick({120,120});
    mouse.Button(0, true); tick({120,120});
    ok = ok && ImGui::IsMouseDoubleClicked(0);
    if (!ok) std::cerr << "double-click failed\n";
    // Focus loss/reopen must discard even edges already awaiting trickling.
    io.AddMouseButtonEvent(0, false); io.AddMouseButtonEvent(0, true);
    mouse.Button(0, true); mouse.Reset();
    io.AddKeyEvent(ImGuiKey_A, true);
    tick({120,120}); tick({120,120});
    ok = ok && !io.MouseDown[0] && ImGui::IsKeyDown(ImGuiKey_A);
    if (!ok) std::cerr << "reset failed\n";
    // Removing a newly generated OS position must not remove an older game
    // position or unrelated keyboard input awaiting the next frame.
    io.AddMousePosEvent(130,120);
    const auto first = ImGui::GetCurrentContext()->InputEventsQueue.Size;
    io.AddMousePosEvent(500,120); io.AddKeyEvent(ImGuiKey_A, false);
    bcn::native_ui::RemoveBackendMousePositions(first);
    const auto& events = ImGui::GetCurrentContext()->InputEventsQueue;
    ok = ok && events.Size == first + 1 && events[first - 1].Type == ImGuiInputEventType_MousePos;
    if (!ok) std::cerr << "isolation failed: " << first << ',' << events.Size << '\n';
    // Held/repeat samples must not manufacture edges; ignore unsupported
    // buttons and wheel releases. A fresh press after reset must still work.
    mouse.Reset(); tick({120,120});
    mouse.GameButton(0, false, false);
    mouse.GameButton(7, true, false);
    mouse.GameButton(8, false, true);
    mouse.GameButton(9, false, false);
    tick({120,120});
    ok = ok && !io.MouseDown[0] && frameWheel == 0;
    mouse.GameButton(8, true, false); tick({120,120});
    ok = ok && frameWheel == 1;
    mouse.GameButton(9, true, false); tick({120,120});
    ok = ok && frameWheel == -1;
    mouse.GameButton(1, true, false);
    mouse.GameButton(1, true, false); // duplicate source sample is idempotent
    mouse.GameButton(1, false, false);
    mouse.GameButton(1, false, true); // quick RMB click between frames
    tick({120,120});
    ok = ok && io.MouseDown[1];
    tick({120,120});
    ok = ok && !io.MouseDown[1];
    mouse.GameButton(0, true, false); mouse.Reset();
    mouse.GameButton(0, false, false); tick({120,120});
    ok = ok && !io.MouseDown[0];
    mouse.GameButton(0, false, true);
    mouse.GameButton(0, true, false); tick({120,120});
    ok = ok && io.MouseDown[0];
    mouse.GameButton(0, false, true); tick({120,120});
    ok = ok && !io.MouseDown[0];
    // A stalled render thread must not let pending game input grow without
    // bound. Overflow releases old ownership; closing drops queued edges.
    for (int click = 0; click < 5000; ++click) {
        mouse.GameButton(0, true, false);
        mouse.GameButton(0, false, true);
    }
    mouse.Drain(io, {120,120});
    ok = ok && ImGui::GetCurrentContext()->InputEventsQueue.Size <= 4097;
    mouse.Reset(); tick({120,120}); tick({120,120});
    ok = ok && !io.MouseDown[0];
    if (!ok) std::cerr << "game-input edge/lifecycle regression\n";
    ImGui::DestroyContext();
    std::cout << "drag, double-click, wheel, five buttons, reset and queue isolation: " << (ok ? "PASS" : "FAIL") << '\n';
    return ok;
}

int main()
{
    for (float fps : {60.0F, 15.0F, 5.0F}) {
        const auto same = Replay(fps, false, 20);
        const auto mixed = Replay(fps, true, 20);
        const auto corrected = Replay(fps, true, 20, true);
        std::cout << fps << " FPS: same-position=" << same
                  << "/20 mixed-position=" << mixed << "/20 corrected=" << corrected << "/20\n";
        if (same != 20 || mixed != 0 || corrected != 20) return EXIT_FAILURE;
    }
    return Lifecycle() ? EXIT_SUCCESS : EXIT_FAILURE;
}
