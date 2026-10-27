#pragma once

#include <cstdint>

namespace dc::ui {

/**
 * One tick of a menu pointer: whether it is in use and what the mouse did.
 */
struct MenuPointerTake {
    bool          pointing = false; /**< The pointer is in use and the screen should follow it. */
    bool          moved = false;    /**< Moved, clicked or wheeled this tick. */
    std::uint32_t clicked = 0;      /**< Mouse buttons pressed this tick (bit n-1 for Mouse n). */
    std::uint32_t released = 0;     /**< Mouse buttons let go this tick. */
    std::uint32_t held = 0;         /**< Every mouse button held now. */
    float         wheel = 0.0f;     /**< Wheel turn in notches, positive away from the user. */
};

/**
 * The mouse as the pointer of one of the game's menus, in the game's 640x480 space.
 *
 * A screen that takes the mouse owns a MenuPointer: it moves with the mouse's relative motion
 * (through the mapping the 2D is drawn with, so aspect, ui_scale and the window's size all hold),
 * appears on the first motion, click or wheel notch and goes back to the pad when a pad button or
 * the left stick is used. Its screen's code turns what it is over into the game's own cursor
 * state, and a click into a pad press the game reads through MenuPointerSyntheticDown.
 */
class MenuPointer {
public:
    float x = 0.0f;         /**< Pointer position in 640x480 units. */
    float y = 0.0f;         /**< Pointer position in 640x480 units. */
    bool  pointing = false; /**< The pointer has taken over from the pad. */

    /**
     * The screen opened: the mouse becomes the pointer, and buttons already held act only once
     * let go.
     */
    void Open();

    /**
     * The screen closed. The mouse goes back to the pad's buttons once none of its buttons is
     * held, since a held button would be a press of the next screen. Call every tick until true.
     */
    bool Close();

    /**
     * Reports whether the screen owning this pointer is open.
     */
    bool IsOpen() const { return open; }

    /**
     * Another screen took the mouse over: forgets it without handing it back.
     */
    void Abandon() {
        open = false;
        pointing = false;
    }

    /**
     * Takes this tick's motion, clicks and wheel. start_x and start_y are where the pointer
     * appears when it takes over from the pad (the game's cursor).
     */
    MenuPointerTake Take(float start_x, float start_y);

private:
    bool          open = false;
    std::uint32_t buttons = 0;
};

/**
 * Adds pad buttons (InputButton bits, the game's PAD_*) the mouse presses this tick.
 */
void MenuPointerPress(int mask);

/**
 * Clears the pad buttons the mouse pressed.
 */
void MenuPointerClearPresses();

/**
 * The pad buttons the mouse presses this tick.
 */
int MenuPointerSyntheticDown();

/**
 * Sets the pointer a hand is drawn at, or nullptr for none.
 */
void MenuPointerShow(const MenuPointer *pointer);

/**
 * The fingertip's place of the shown pointer's hand, if that pointer is in use.
 */
bool MenuPointerHand(float &x, float &y);

/**
 * A tick's worth of the Register Name screen's and the menus' mouse, ahead of the pad's read.
 */
void MenuMouseUpdate();

} // namespace dc::ui
