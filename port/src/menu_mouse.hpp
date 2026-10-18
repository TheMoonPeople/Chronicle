#pragma once

// The pause menu was drawn this tick, so its screen takes the mouse next tick.
void MenuMouseNoteBattleMenu();

/**
 * Draws the Journal reading pointer and item-discard confirmation over the pause menu.
 */
void MenuMouseDrawOverlay();

// The same animated icon coordinates used by the renderer and pointer hit testing.
void BattleMenuIconPosition(int index, float &x, float &y);
