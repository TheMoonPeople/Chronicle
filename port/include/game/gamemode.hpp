#pragma once

namespace dc::game {

/**
 * Requests a transition to a map and optional starting event.
 */
void MapJump(int map_no, int event_no);

/**
 * Initializes the developer mode-selection menu.
 */
void MenuInit();

/**
 * Updates the developer mode-selection menu.
 */
int MenuLoop();

/**
 * Initializes the startup memory-card check.
 */
void MemCheckInit();

/**
 * Updates the startup memory-card check.
 */
int MemCheckLoop();

/**
 * Initializes the standalone save screen.
 */
void InitSave();

/**
 * Updates and draws the standalone save screen.
 */
int LoopSave();

/**
 * Initializes the trial-ending mode.
 */
void TrialEndInit();

/**
 * Completes the trial-ending mode immediately.
 */
int TrialEndLoop();

/**
 * Starts trial-version completion tracking.
 */
void TrialStart();

/**
 * Reports whether trial-version completion has been reached.
 */
int CheckTrialEnd();

} // namespace dc::game
