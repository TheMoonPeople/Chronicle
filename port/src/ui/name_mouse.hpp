#pragma once

// The mouse on the Register Name screen. While the screen is open the mouse is a pointer: hovering a
// control puts the game's own cursor on it, and a click presses the pad button that would act there,
// so the game's code does the rest. MenuMouseUpdate (menu_mouse.cpp) calls these ahead of the game's
// read of the pad.

namespace dc::game {
class CTexture;
class CRect_i_;
} // namespace dc::game

namespace dc::ui {

/**
 * Reports whether the screen is open: its textures are read and the game has not closed it.
 */
bool NameMouseOpen();

/**
 * Reads the pointer for this tick, moves the cursor to what it is over and presses the pad
 * buttons a click stands for.
 */
void NameMouseUpdate();

/**
 * The screen is not open: the mouse goes back to the pad's buttons once none of it is held.
 */
void NameMouseRelease();

/**
 * The game draws its hand twice (a shadow, then the hand) where the cursor is. While the pointer
 * is in use this moves both to the pointer, as the Options screen's hand is. Returns false when
 * the draw is not the hand's, true when screen was changed.
 */
bool NameMouseHand(game::CTexture *texture, game::CRect_i_ &screen, const game::CRect_i_ &texel, bool shadow);

} // namespace dc::ui
