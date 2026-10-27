#pragma once

#include <cstdint>

#include "app/presentation.hpp"

namespace dc::game {
class CSaveData;
} // namespace dc::game

namespace dc::app {

/**
 * The game's main(): start-up, then one mode after another. The window, renderer, input and clock
 * must already be initialised; the tick callback is the game's to install, the idle hook and the
 * pump hooks the caller's. Returns kExitOk once a stop was requested or the frame budget is spent,
 * at the end of a frame of the main loop, outside any frame, so the last frame can be read back.
 */
int RunGame(int argc, char **argv);

/**
 * Sets how many frames of RunGame's main loop may run, one per MGEndFrame it calls itself; the
 * presents a mode makes on its own (EditLoop's fades) and the loading screen's are not counted.
 * Negative is no budget.
 */
void GameSetFrameBudget(std::int64_t frames);

/**
 * Frames of RunGame's main loop run so far, as counted against the frame budget.
 */
std::int64_t GameFrameCount();

/**
 * Starts a DC_PERF_TRANSITIONS=1 load trace for the executable's launch.
 */
void GameLoadTraceLaunch();

/**
 * Starts a DC_PERF_TRANSITIONS=1 load trace for a world-map jump to map_no, unless one to it is
 * already running.
 */
void GameLoadTraceRequest(int map_no);

/**
 * Logs a named milestone of the running load trace with its wall time since the trace began and
 * since the previous milestone.
 */
void GameLoadTraceCheckpoint(const char *stage);

/**
 * Called every town frame: logs when the player first has control of the traced map and ends the
 * trace once its fade has cleared.
 */
void GameLoadTraceControlsReady();

/**
 * Logs the world map's cursor and state, and every place's position when selection opens, under
 * DC_PERF_TRANSITIONS=1.
 */
void GameLoadTraceWorldMapSelection(int cursor, int state, int target);

/**
 * The game's libc generator state, for load traces.
 */
unsigned int GameRandomStateForTrace();

/**
 * Clears save for a new game as retail's main() does, keeping its configuration, and keeping the
 * last file's number whole where retail's round trip keeps one byte of it.
 */
void ResetSaveKeepingConfig(game::CSaveData &save);

/**
 * Test hook: takes "edit[:<map no>]" (the game's MapNo: 0-4 the towns, 11 and up the sub maps, 99
 * the interior), "dungeon[:<n>]" (dungeon n, 0-6, map 200 + n, at its floor select), "title",
 * "rush", "opening", "menu", or the populated town presets "e01" (Norune), "e03" (Queens) and "e04"
 * (Muska Lacka); false for anything else. RunGame then sets DebugMode, skips the warm-up and starts
 * in that mode, set up as the developer menu (and, for a dungeon, its loader) would have.
 */
bool GameSetJump(const char *spec);

/**
 * Test hook: cuts the loading screen's holds and fades to a few ticks.
 */
void GameSetFastLoad(bool fast);

/**
 * Test hook: the item and weapon numbers (ITEM_*) the party is given, those it lacks, whenever a
 * town or dungeon starts. Takes them comma-separated; false for anything that is not a list of
 * numbers from 1 to ITEM_WEAPON_END.
 */
bool GameSetGive(const char *list);

/**
 * Whether the autosave may be written, with game.autosave on; off for headless and --jump runs. A
 * town that starts without an event writes it, and so does each dungeon floor as it starts.
 */
void GameSetAutosave(bool on);

/**
 * Writes the autosave for the dungeon floor the party has just started, once per floor, recording
 * the dungeon's map number as the map to resume on.
 */
void GameAutosaveFloor();

/**
 * Called by the title with a save just loaded: a save written on a dungeon floor makes that
 * dungeon's next start keep the party's status and event flags and offer the saved floor.
 */
void GameResumeLoaded();

/**
 * Returns the floor the dungeon entrance's floor list should start on for a resumed save, or -1,
 * and forgets it.
 */
int GameTakeResumeFloor();

/**
 * Whether the loading screen's holds and fades are cut short (GameSetFastLoad).
 */
bool GameFastLoad();

/**
 * Runs after every frame of the main loop and flips DebugMode (which starts as the config file's
 * game.debug_mode) when pad 1 holds L1+R1+L2+R2 and R3 is pressed: retail PAL's pad 2 combination,
 * read past the game's pad lock.
 */
void GameCheckDebugToggle();

/**
 * Runs after every frame of the main loop while DebugMode is set: true on the frame pad 1's Start
 * and Select first are both held, read past the game's pad lock, outside the developer menu itself.
 */
bool GameDeveloperMenuRequested();

/**
 * Ends the running mode where it stands (sound stopped, pad unlocked, any pending map jump or start
 * event dropped) and sets mode to GAME_MODE_MENU.
 */
void GameEnterDeveloperMenu();

/**
 * Asks RunGame to return at the next frame boundary of the main loop.
 */
void GameRequestStop();

/**
 * Whether GameRequestStop has been called.
 */
bool GameStopRequested();

/**
 * Follows NextMapNo, as MapJump left it, into mode, MapNo, LocalMapNo and main_select_menu_no:
 * what RunGame does after a mode's loop returns.
 */
void GameFollowMapJump();

/**
 * Turns a non-zero result from loop_mode's loop (or the rush movie's skip) into the next mode.
 * Retail handles each result in the case that ran the loop, so a loop that sets mode itself (the
 * developer menu's) is judged by the mode it ran in. Title's result 1 also starts a new game and its
 * result 2 reinitialises sound.
 */
void GameApplyLoopResult(int loop_mode, int result);

} // namespace dc::app
