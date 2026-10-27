#pragma once

namespace dc::battlemenu {

/**
 * Consumes and returns whether an in-game save load is ready to reload the town.
 */
bool TakeBattleMenuLoadRequest();

/**
 * Consumes and returns whether Quit was confirmed, so the main loop goes back to the title screen.
 */
bool TakeBattleMenuTitleRequest();

/**
 * Gives the animated ring icon position shared by the renderer and pointer hit testing.
 */
void BattleMenuIconPosition(int index, float &x, float &y);

} // namespace dc::battlemenu
