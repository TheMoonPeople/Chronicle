#pragma once

// The pause menu was drawn this tick, so its screen takes the mouse next tick.
void MenuMouseNoteBattleMenu();

// Draws what the mouse adds to the pause menu's own picture: the question asked before an item is
// thrown away. Called at the end of BattleMenuDraw.
void MenuMouseDrawOverlay();

// The same animated icon coordinates used by the renderer and pointer hit testing.
void BattleMenuIconPosition(int index, float &x, float &y);
