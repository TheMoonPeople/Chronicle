#pragma once

/**
 * Notes that the pause menu was drawn this tick, so its screen takes the mouse next tick.
 */
void MenuMouseNoteBattleMenu();

/**
 * Draws the Journal reading pointer and item-discard confirmation over the pause menu.
 */
void MenuMouseDrawOverlay();

/**
 * Gives the animated ring icon position shared by the renderer and pointer hit testing.
 */
void BattleMenuIconPosition(int index, float &x, float &y);
