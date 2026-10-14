#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "gfx/gfx.hpp"
#include "platform/config.hpp"

class CSaveData;

// The game's main(): start-up, then one mode after another. The window, renderer, input and clock
// must already be initialised; the tick callback is the game's to install, the idle hook and the
// pump hooks the caller's. Returns kExitOk once a stop was requested or the frame budget is spent,
// at the end of a frame of the main loop, outside any frame, so the last frame can be read back.
int RunGame(int argc, char **argv);

// Frames of RunGame's main loop, one per MGEndFrame it calls itself; the presents a mode makes
// on its own (EditLoop's fades) and the loading screen's are not counted. Negative is no budget.
void GameSetFrameBudget(std::int64_t frames);

std::int64_t GameFrameCount();

// Clears save for a new game as retail's main() does, keeping its configuration, and keeping the
// last file's number whole where retail's round trip keeps one byte of it.
void ResetSaveKeepingConfig(CSaveData &save);

// Test hooks. GameSetJump takes "edit[:<map no>]" (the game's MapNo: 0-4 the towns, 11 and up
// the sub maps, 99 the interior), "dungeon[:<n>]" (dungeon n, 0-6, map 200 + n, at its floor
// select), "title", "rush", "opening" or "menu"; false for anything else. RunGame then sets
// DebugMode, skips the warm-up and starts in that mode, set up as the developer menu (and, for a
// dungeon, its loader) would have. GameSetFastLoad cuts the loading screen's holds and fades to a
// few ticks.
bool GameSetJump(const char *spec);
void GameSetFastLoad(bool fast);

// Test hook: the item and weapon numbers (ps2/include/itemdata.hpp's ITEM_*) the party is given,
// those it lacks, whenever a town or dungeon starts. Takes them comma-separated; false for anything
// that is not a list of numbers from 1 to ITEM_WEAPON_END.
bool GameSetGive(const char *list);

// Whether a town that starts without an event overwrites the autosave; off for headless and --jump runs.
void GameSetAutosave(bool on);
bool GameFastLoad();

// DebugMode starts as the config file's game.debug_mode. GameCheckDebugToggle runs after every frame
// of the main loop and flips DebugMode when pad 1 holds L1+R1+L2+R2 and R3 is pressed: retail PAL's
// pad 2 combination, read past the game's pad lock.
void GameCheckDebugToggle();

// The way back to the developer menu from any mode while DebugMode is set. GameDeveloperMenuRequested
// runs after every frame of the main loop: true on the frame pad 1's Start and Select first are both
// held, read past the game's pad lock, outside the developer menu itself. GameEnterDeveloperMenu
// then ends the running mode where it stands (sound stopped, pad unlocked, any pending map jump or
// start event dropped) and sets mode to GAME_MODE_MENU.
bool GameDeveloperMenuRequested();
void GameEnterDeveloperMenu();

// Honoured at the next frame boundary of the main loop.
void GameRequestStop();

bool GameStopRequested();

// What RunGame does between two modes and after a mode's loop returns, factored out so the
// transitions can be tested against the game's globals.

// NextMapNo, as MapJump left it, into mode, MapNo, LocalMapNo and main_select_menu_no.
void GameFollowMapJump();

// A non-zero result from loop_mode's loop (or the rush movie's skip) into the next mode. Retail
// handles each result in the case that ran the loop, so a loop that sets mode itself (the
// developer menu's) is judged by the mode it ran in. Title's result 1 also starts a new game and
// its result 2 reinitialises sound.
void GameApplyLoopResult(int loop_mode, int result);

// ---- Presentation ----------------------------------------------------------------------------
//
// Each tick (MGBeginFrame to MGEndFrame) is recorded as a display list. MGEndFrame hands it to
// GameRenderTick, which renders it once as the canonical image the next tick's effects sample;
// then, while MGEndFrame waits for the next tick, GamePresentBetweenTicks presents display frames
// interpolated between the last two lists at the elapsed fraction of the tick, and
// GamePresentTickEnd presents the canonical image if nothing was presented during the tick.

struct GamePresentSettings {
    // Off presents each tick's canonical image once, as the PS2 showed a field per VSync.
    bool interpolation = true;
    // Display frames per second at most; 0 leaves the pace to the present mode.
    double max_fps = 0.0;
    // Unbounded clock only (headless tests): display renders made per tick, at alphas k / n, before
    // the canonical image is presented.
    int display_per_tick = 0;
    // The FPS counter over every presented frame, at first; input.bindings.fps_toggle flips it. It is drawn
    // after the list into the presented image only, never into a canonical image, so it is not in
    // kPreviousFrame, frame copies or GameScreenshot.
    bool            show_fps = false;
    ConfigFpsDetail fps_detail = ConfigFpsDetail::All;
};

void GameSetPresentSettings(const GamePresentSettings &settings);

void GameRenderTick(gfx::DisplayListRef list);
bool GamePresentBetweenTicks(double fraction, std::chrono::steady_clock::time_point next_tick);
void GamePresentTickEnd();

struct GamePresentStats {
    std::uint64_t ticks;
    std::uint64_t display_frames;
    double        canonical_seconds;
    double        display_seconds;
    std::uint64_t mesh_draws;
    std::uint64_t keyed_mesh_draws;
    std::uint64_t draws_2d;
    std::uint64_t stateful;
    std::uint64_t max_draws;
    // Mesh and 2D draws of the newest tick.
    std::uint64_t last_draws;
};

GamePresentStats GamePresentStatistics();

// Whether the FPS counter is drawn now.
bool GameShowingFps();

// What the FPS counter says: presented frames per second, the measured and configured tick rates and
// the newest tick's draws.
std::string GameFpsText();

// The newest canonical image (or the loading screen's frame, if it presented since), RGBA8 rows top
// to bottom: what --screenshot writes. A display frame, and so the FPS counter, never is. Outside a
// frame only.
// The newest tick's image with the FPS counter over it, as a window shows it: a display render of
// the counter on the canonical image, read back. False where the counter is off or no display
// render can be made. Test hook for --screenshot-fps.
bool GameScreenshotWithFps(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height);

bool GameScreenshot(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height);
