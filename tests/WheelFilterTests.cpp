#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <unordered_set>
#include "BodyChangeNG/MenuActionInput.h"

// Only engine/UI boundaries are mocked; the filter and all its tracking state
// below are extracted verbatim from the production source before compilation.
namespace RE {
enum class INPUT_DEVICE { kKeyboard, kMouse, kGamepad };
enum class INPUT_EVENT_TYPE { kButton, kMouseMove };
struct ButtonEvent;
struct InputEvent {
    INPUT_DEVICE device{};
    INPUT_EVENT_TYPE type{INPUT_EVENT_TYPE::kButton};
    InputEvent* next{};
    ButtonEvent* AsButtonEvent();
    INPUT_DEVICE GetDevice() const { return device; }
    INPUT_EVENT_TYPE GetEventType() const { return type; }
};
struct ButtonEvent : InputEvent {
    std::uint32_t id{};
    int edge{}; // 0 down, 1 held, 2 up
    std::uint32_t GetIDCode() const { return id; }
    bool IsPressed() const { return edge != 2; }
    bool IsDown() const { return edge == 0; }
    bool IsUp() const { return edge == 2; }
};
ButtonEvent* InputEvent::AsButtonEvent() {
    return type == INPUT_EVENT_TYPE::kButton ? static_cast<ButtonEvent*>(this) : nullptr;
}
struct BSWin32MouseDevice { enum class Key { kLeftButton=0, kRightButton=1, kWheelUp=8, kWheelDown=9 }; };
struct Console { static constexpr auto MENU_NAME = "Console"; };
struct UI {
    static inline bool console{};
    static UI* GetSingleton() { static UI ui; return &ui; }
    bool IsMenuOpen(const char*) { return console; }
};
struct ControlMap {
    enum class InputContextID { kGameplay, kMenuMode };
    static ControlMap* GetSingleton() { static ControlMap map; return &map; }
    std::uint32_t GetMappedKey(const char*, INPUT_DEVICE, InputContextID context) const {
        return context == InputContextID::kGameplay ? 0x12 : 0x01;
    }
};
template<class T> struct BSTEventSource {};
}
namespace REL { template<class T> struct Relocation {}; }
namespace bcn {
struct InputSink {
    static inline bool capturing{};
    static InputSink& Get() { static InputSink sink; return sink; }
    bool IsCapturingHotkey() const { return capturing; }
};
namespace native_ui {
bool open{}, typing{};
bool IsOpen() { return open; }
bool WantsTextInput() { return typing; }
void SetMenuActionHeld(bool) {}
void SubmitActivate() {}
void SubmitCancel() {}
void SubmitTextInputKey(std::uint32_t, bool) {}
void SubmitMenuNavigationKey(std::uint32_t, bool) {}
void SubmitGameMouseButton(std::uint32_t, bool, bool) {}
}
}

namespace {
#include "wheel_filter_production.inc"
}

int main() {
    unsigned checked{};
    // Positive control: this must execute the production suppression logic,
    // not merely pass every event through a no-op test double.
    {
        ResetLocked();
        bcn::native_ui::open = true;
        RE::ButtonEvent keyboard, wheel;
        keyboard.device = RE::INPUT_DEVICE::kKeyboard;
        keyboard.id = 0xC8;
        wheel.device = RE::INPUT_DEVICE::kMouse;
        wheel.id = 8;
        keyboard.next = &wheel;
        RE::InputEvent* first = &keyboard;
        FilterKeyboardEvents(&first);
        if (first != &wheel) return 1;
        bcn::native_ui::open = false;
        keyboard.edge = 2;
        first = &keyboard;
        FilterKeyboardEvents(&first);
        if (first != &wheel) return 1;
    }
    // All keyboard scan-code values, keyboard/controller devices, wheel edges,
    // typing/hotkey/console states, and open -> close -> reopen transitions.
    for (unsigned key=0; key<256; ++key) {
        for (auto keyboardDevice : {RE::INPUT_DEVICE::kKeyboard, RE::INPUT_DEVICE::kGamepad}) {
            for (unsigned wheelID : {8U, 9U}) {
                for (int wheelEdge=0; wheelEdge<3; ++wheelEdge) {
                    for (unsigned mode=0; mode<8; ++mode) {
                        ResetLocked();
                        RE::UI::console = (mode & 1) != 0;
                        bcn::InputSink::capturing = (mode & 2) != 0;
                        for (unsigned frame=0; frame<6; ++frame) {
                            bcn::native_ui::open = frame==0 || frame==1 || frame==4;
                            bcn::native_ui::typing = bcn::native_ui::open && (mode & 4);
                            RE::ButtonEvent keyboard, wheel, right;
                            keyboard.device=keyboardDevice;
                            keyboard.id=key; keyboard.edge=static_cast<int>(frame%3);
                            wheel.device=RE::INPUT_DEVICE::kMouse;
                            wheel.id=wheelID; wheel.edge=wheelEdge;
                            right.device=RE::INPUT_DEVICE::kMouse;
                            right.id=1; right.edge=keyboard.edge;
                            RE::InputEvent movement;
                            movement.device=RE::INPUT_DEVICE::kMouse;
                            movement.type=RE::INPUT_EVENT_TYPE::kMouseMove;
                            keyboard.next=&right; right.next=&wheel; wheel.next=&movement;
                            RE::InputEvent* first=&keyboard;
                            FilterKeyboardEvents(&first);
                            bool foundWheel{}, foundMovement{};
                            unsigned traversed{};
                            for (auto* item=first; item && traversed<5; item=item->next, ++traversed) {
                                foundWheel |= item==&wheel;
                                foundMovement |= item==&movement;
                            }
                            if (!foundWheel || !foundMovement || traversed>4) {
                                std::cerr << "FAIL key=" << key << " wheel=" << wheelID
                                          << " edge=" << wheelEdge << " mode=" << mode
                                          << " frame=" << frame << '\n';
                                return 1;
                            }
                            ++checked;
                        }
                    }
                }
            }
        }
    }
    std::cout << checked << " production-filter replay checks passed; wheel and movement preserved\n";
}
