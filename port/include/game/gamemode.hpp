#pragma once

/**
 * Requests a transition to a map and optional starting event.
 *
 * @mangled MapJump__Fii
 * @address 0x141D30
 * @size 0x10
 */
void MapJump(int map_no, int event_no);

/**
 * Initializes the developer mode-selection menu.
 *
 * @mangled MenuInit__Fv
 * @address 0x141D40
 * @size 0x118
 */
void MenuInit();

/**
 * Updates the developer mode-selection menu.
 *
 * @mangled MenuLoop__Fv
 * @address 0x141E60
 * @size 0x940
 */
int MenuLoop();

/**
 * Initializes the startup memory-card check.
 *
 * @mangled MemCheckInit__Fv
 * @address 0x1427A0
 * @size 0xE0
 */
void MemCheckInit();

/**
 * Updates the startup memory-card check.
 *
 * @mangled MemCheckLoop__Fv
 * @address 0x142880
 * @size 0x280
 */
int MemCheckLoop();

/**
 * Initializes the standalone save screen.
 *
 * @mangled InitSave__Fv
 * @address 0x142B00
 * @size 0x110
 */
void InitSave();

/**
 * Updates and draws the standalone save screen.
 *
 * @mangled LoopSave__Fv
 * @address 0x142C10
 * @size 0x3C
 */
int LoopSave();

/**
 * Initializes the trial-ending mode.
 *
 * @mangled TrialEndInit__Fv
 * @address 0x142C50
 * @size 0x8
 */
void TrialEndInit();

/**
 * Completes the trial-ending mode immediately.
 *
 * @mangled TrialEndLoop__Fv
 * @address 0x142C60
 * @size 0xC
 */
int TrialEndLoop();

/**
 * Starts trial-version completion tracking.
 *
 * @mangled TrialStart__Fv
 * @address 0x142C70
 * @size 0x8
 */
void TrialStart();

/**
 * Reports whether trial-version completion has been reached.
 *
 * @mangled CheckTrialEnd__Fv
 * @address 0x142C80
 * @size 0xC
 */
int CheckTrialEnd();
