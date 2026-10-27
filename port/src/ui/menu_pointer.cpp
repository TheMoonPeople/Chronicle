#include "ui/menu_pointer.hpp"

#include <algorithm>

#include "gamepad.hpp"
#include "gfx/gfx.hpp"
#include "platform/input.hpp"
#include "platform/window.hpp"
#include "ui/name_mouse_layout.hpp"

namespace dc::ui {

using namespace dc::game;

namespace {

// Where the game's hand rests against the control it is on: the fingertip, as the Options screen's
// pointer takes it.
constexpr float kFingerX = 26.0f;
constexpr float kFingerY = 14.0f;

// Held pad buttons that hand a screen back to the pad.
constexpr std::uint16_t kPadButtons = kInputUp | kInputDown | kInputLeft | kInputRight | kInputCross | kInputCircle |
                                      kInputSquare | kInputTriangle | kInputL1 | kInputR1 | kInputL2 | kInputR2 |
                                      kInputStart;
constexpr int           kStickThreshold = 120;

int                synthetic_down = 0;
const MenuPointer *shown = nullptr;

bool PadHeld() {
    const InputPadState &pad = InputGetPad(0);
    if ((pad.buttons & kPadButtons) != 0) {
        return true;
    }
    const int stick_x = AxisCalibration(pad.left_x);
    const int stick_y = AxisCalibration(pad.left_y);
    return stick_x > kStickThreshold || stick_x < -kStickThreshold || stick_y > kStickThreshold ||
           stick_y < -kStickThreshold;
}

// 640x480 units per count of mouse motion along each axis, through the mapping the 2D is drawn with.
void UnitsPerCount(float &across, float &down) {
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::kMainTarget);
    int                       width = 0;
    int                       height = 0;
    // With no window the counts are target pixels, as a script gives them.
    if (!WindowSize(width, height)) {
        width = static_cast<int>(mapping.pixel_width);
        height = static_cast<int>(mapping.pixel_height);
    }
    across = name_mouse::PointerUnitsPerCount(mapping.scale_x, static_cast<float>(mapping.pixel_width),
                                             static_cast<float>(width));
    down = name_mouse::PointerUnitsPerCount(mapping.scale_y, static_cast<float>(mapping.pixel_height),
                                           static_cast<float>(height));
    if (across <= 0.0f || down <= 0.0f) {
        across = down = 1.0f;
    }
}

} // namespace

void MenuPointer::Open() {
    open = true;
    pointing = false;
    InputSetMenuMouse(true);
    buttons = InputTakeMenuMouse().buttons;
}

bool MenuPointer::Close() {
    pointing = false;
    if (!open) {
        return true;
    }
    if (InputTakeMenuMouse().buttons != 0) {
        return false;
    }
    open = false;
    InputSetMenuMouse(false);
    return true;
}

MenuPointerTake MenuPointer::Take(float start_x, float start_y) {
    MenuPointerTake take;
    if (!open) {
        return take;
    }
    InputMenuMouse mouse = InputTakeMenuMouse();
#ifdef __ANDROID__
    if (InputTouchOverlayVisible()) {
        // Ignore synthetic touch motion while the virtual pad is visible.
        mouse.dx = mouse.dy = mouse.wheel = 0.0f;
    }
#endif
    take.clicked = mouse.buttons & ~buttons;
    take.released = buttons & ~mouse.buttons;
    take.held = mouse.buttons;
    take.wheel = mouse.wheel;
    buttons = mouse.buttons;
    const bool motion = mouse.dx != 0.0f || mouse.dy != 0.0f;
    take.moved = motion || take.clicked != 0 || mouse.wheel != 0.0f;

    if (PadHeld()) {
        pointing = false;
    }
    if (take.moved) {
        if (!pointing) {
            pointing = true;
            x = start_x + kFingerX;
            y = start_y + kFingerY;
        }
        float across = 1.0f;
        float down = 1.0f;
        UnitsPerCount(across, down);
        x = std::clamp(x + mouse.dx * across, 0.0f, 639.0f);
        y = std::clamp(y + mouse.dy * down, 0.0f, 479.0f);
    }
    take.pointing = pointing;
    return take;
}

void MenuPointerPress(int mask) { synthetic_down |= mask; }

void MenuPointerClearPresses() { synthetic_down = 0; }

int MenuPointerSyntheticDown() { return synthetic_down; }

void MenuPointerShow(const MenuPointer *pointer) { shown = pointer; }

bool MenuPointerHand(float &x, float &y) {
    if (shown == nullptr || !shown->IsOpen() || !shown->pointing) {
        return false;
    }
    x = shown->x - kFingerX;
    y = shown->y - kFingerY;
    return true;
}

} // namespace dc::ui
