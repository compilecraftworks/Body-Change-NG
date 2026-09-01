#pragma once

namespace bcn::native_ui
{
    using RenderCallback = void (*)();

    bool Register(RenderCallback a_callback);
    bool InstallRendererHook();
    bool Open();
    bool Toggle();
    bool Close();
    void SubmitMouseWheel(float a_delta) noexcept;
    // PollInputDevices may consume RMB before Skyrim creates a Scaleform
    // event. Queue the same button transition directly for ImGui so actor
    // rotation remains available while gameplay attack/block never sees it.
    void SubmitRightMouseButton(bool a_down) noexcept;
    // TextInputFilter removes DirectInput keyboard events before gameplay and
    // other shortcut sinks see them. Preserve the same press/release edge for
    // ImGui so Backspace, Delete, navigation, and clipboard shortcuts keep
    // working while an editable field owns keyboard input.
    void SubmitTextInputKey(std::uint32_t a_scanCode, bool a_down) noexcept;
    // DirectInput reaches the SKSE input sink even when another UI menu has
    // intercepted the Scaleform key event.  Latch Escape here and consume it
    // once from the ImGui frame so every Body Changer NG window can close
    // reliably with the same key.
    void SubmitEscape() noexcept;
    [[nodiscard]] bool ConsumeEscape() noexcept;
    // Automatic runtime baseline: 1.0 at 1080p, 1.25 at 1440p (2K), and 1.5
    // at 2160p (4K). The user's UI-size slider is applied on top.
    [[nodiscard]] float GetResolutionScale() noexcept;
    [[nodiscard]] bool IsOpen() noexcept;
    [[nodiscard]] bool IsReady() noexcept;
    // Mirrors ImGui text-entry ownership into Skyrim's public ControlMap text
    // input state so hotkey mods can suppress themselves without a raw input
    // dispatcher hook.
    [[nodiscard]] bool WantsTextInput() noexcept;
}
