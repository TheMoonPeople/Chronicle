#pragma once

#include <cstdint>

// The mouse as the pointer of one of the game's menus, in the game's 640x480 space. A screen that
// takes the mouse owns a MenuPointer: it moves with the mouse's relative motion (through the
// mapping the 2D is drawn with, so aspect, ui_scale and the window's size all hold), appears on
// the first motion, click or wheel notch and goes back to the pad when a pad button or the left
// stick is used. Its screen's code turns what it is over into the game's own cursor state, and a
// click into a pad press the game reads through MenuPointerSyntheticDown.

struct MenuPointerTake {
    // The pointer is in use: the screen should follow it.
    bool pointing = false;
    // Moved, clicked or wheeled this tick.
    bool moved = false;
    // Mouse buttons pressed this tick (bit n-1 for Mouse n) and the wheel's turn in notches, positive
    // away from the user.
    std::uint32_t clicked = 0;
    // Mouse buttons let go this tick, and every button held now.
    std::uint32_t released = 0;
    std::uint32_t held = 0;
    float         wheel = 0.0f;
};

class MenuPointer {
public:
    float x = 0.0f;
    float y = 0.0f;
    bool  pointing = false;

    // The screen opened: the mouse becomes the pointer, and buttons already held act only once let go.
    void Open();

    // The screen closed. The mouse goes back to the pad's buttons once no button of it is held, since
    // a held button would be a press of the next screen. Call every tick until it returns true.
    bool Close();

    bool IsOpen() const { return open; }

    // Another screen took the mouse over: forget it without handing it back.
    void Abandon() {
        open = false;
        pointing = false;
    }

    // Takes this tick's motion, clicks and wheel. start_x and start_y are where the pointer appears
    // when it takes over from the pad (the game's cursor).
    MenuPointerTake Take(float start_x, float start_y);

private:
    bool          open = false;
    std::uint32_t buttons = 0;
};

// Pad buttons (InputButton bits, the game's PAD_*) the mouse presses this tick.
void MenuPointerPress(int mask);
void MenuPointerClearPresses();
int  MenuPointerSyntheticDown();

// The pointer a hand is drawn at, if one is in use: the fingertip's place.
void MenuPointerShow(const MenuPointer *pointer);
bool MenuPointerHand(float &x, float &y);

// A tick's worth of the Register Name screen's and the menus' mouse, ahead of the pad's read.
void MenuMouseUpdate();
