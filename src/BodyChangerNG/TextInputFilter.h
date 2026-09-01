#pragma once

namespace bcn::text_input
{
    // Installs a verified PollInputDevices call hook. Keyboard events are
    // filtered only while an ImGui text widget actually owns text focus.
    [[nodiscard]] bool Install();
    void Reset() noexcept;
}
