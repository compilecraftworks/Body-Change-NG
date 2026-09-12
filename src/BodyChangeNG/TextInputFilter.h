#pragma once

namespace bcn::text_input
{
    // Installs a verified PollInputDevices call hook for text editing, catalog
    // navigation, mapped keyboard/gamepad Activate/Cancel, and RMB rotation.
    [[nodiscard]] bool Install();
    void Reset() noexcept;
}
