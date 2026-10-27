#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "gfx/gfx.hpp"
#include "platform/config.hpp"

class CSaveData;

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
 * Clears save for a new game as retail's main() does, keeping its configuration, and keeping the
 * last file's number whole where retail's round trip keeps one byte of it.
 */
void ResetSaveKeepingConfig(CSaveData &save);

/**
 * Test hook: takes "edit[:<map no>]" (the game's MapNo: 0-4 the towns, 11 and up the sub maps, 99
 * the interior), "dungeon[:<n>]" (dungeon n, 0-6, map 200 + n, at its floor select), "title",
 * "rush", "opening" or "menu"; false for anything else. RunGame then sets DebugMode, skips the
 * warm-up and starts in that mode, set up as the developer menu (and, for a dungeon, its loader)
 * would have.
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
 * Whether a town that starts without an event overwrites the autosave; off for headless and --jump
 * runs.
 */
void GameSetAutosave(bool on);

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

// Each tick (MGBeginFrame to MGEndFrame) is recorded as a display list. MGEndFrame hands it to
// GameRenderTick, which renders it once as the canonical image the next tick's effects sample;
// then, while MGEndFrame waits for the next tick, GamePresentBetweenTicks presents display frames
// interpolated between the last two lists at the elapsed fraction of the tick, and
// GamePresentTickEnd presents the canonical image if nothing was presented during the tick.

/**
 * How display frames are presented between and at the end of ticks.
 */
struct GamePresentSettings {
    bool            interpolation = true;              /**< Off presents each tick's canonical image once. */
    double          max_fps = 0.0;                     /**< Display frame rate cap; 0 leaves it to the present mode. */
    int             display_per_tick = 0;              /**< Headless display renders per tick, at alphas k / n. */
    bool            show_fps = false;                  /**< Initial FPS counter state; never drawn into canonical images. */
    ConfigFpsDetail fps_detail = ConfigFpsDetail::All; /**< What the FPS counter shows. */
};

/**
 * Applies new presentation settings and, in async render-thread mode, installs the render
 * thread's idle hook that presents between ticks.
 */
void GameSetPresentSettings(const GamePresentSettings &settings);

/**
 * Renders a finished tick's display list once as the canonical image and keeps it, with the
 * previous one, for the display frames presented until the next tick.
 */
void GameRenderTick(gfx::DisplayListRef list);

/**
 * Presents a display frame interpolated at fraction of the tick, pacing to max_fps and never
 * sleeping past next_tick. False once nothing more should be presented this tick.
 */
bool GamePresentBetweenTicks(double fraction, std::chrono::steady_clock::time_point next_tick);

/**
 * Presents the canonical image if no display frame was presented during the tick.
 */
void GamePresentTickEnd();

/**
 * Stops display presentation and releases its retained lists before readback or renderer shutdown.
 */
void GameStopPresentation();

/**
 * Counters of rendering and presentation since start-up.
 */
struct GamePresentStats {
    std::uint64_t ticks;             /**< Canonical renders. */
    std::uint64_t display_frames;    /**< Display renders, game and render thread together. */
    double        canonical_seconds; /**< Time spent in canonical renders. */
    double        display_seconds;   /**< Time spent in display renders. */
    std::uint64_t mesh_draws;        /**< Mesh draws over all ticks. */
    std::uint64_t keyed_mesh_draws;  /**< Mesh draws with an interpolation key. */
    std::uint64_t draws_2d;          /**< 2D draws over all ticks. */
    std::uint64_t stateful;          /**< State commands over all ticks. */
    std::uint64_t max_draws;         /**< Most mesh and 2D draws in one tick. */
    std::uint64_t last_draws;        /**< Mesh and 2D draws of the newest tick. */
};

/**
 * The presentation counters so far, including the render thread's display frames.
 */
GamePresentStats GamePresentStatistics();

/**
 * Whether the FPS counter is drawn now.
 */
bool GameShowingFps();

/**
 * What the FPS counter says: presented frames per second, the measured and configured tick rates and
 * the newest tick's draws.
 */
std::string GameFpsText();

/**
 * Test hook for --screenshot-fps: the newest tick's image with the FPS counter over it, as a window
 * shows it, read back. False where the counter is off or no display render can be made.
 */
bool GameScreenshotWithFps(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height);

/**
 * The newest canonical image (or the loading screen's frame, if it presented since), RGBA8 rows top
 * to bottom: what --screenshot writes. A display frame, and so the FPS counter, never is. Outside a
 * frame only.
 */
bool GameScreenshot(std::vector<std::uint8_t> &rgba, std::uint32_t &width, std::uint32_t &height);
