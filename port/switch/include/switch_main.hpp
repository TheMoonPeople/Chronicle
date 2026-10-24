#pragma once

#include "gfx/gfx.hpp"

/**
 * Entry point callback receiving the composed command-line arguments.
 * Argument strings remain valid for the duration of the call.
 */
using SwitchRunFunction = int (*)(int, const char **);

/**
 * Runs the game with Homebrew Menu arguments followed by whitespace-separated arguments from args.txt.
 * The argument strings stay alive until run returns.
 */
int SwitchMain(int argc, const char **argv, SwitchRunFunction run);

/**
 * Chooses a render target scale from the current window's pixel height.
 * The game keeps its logical PAL resolution while using the display's available pixels.
 */
void SwitchConfigureRenderer(gfx::RendererConfig &config);

/**
 * Asks for the handheld configuration with the GPU at 460.8 MHz, or puts back the one the title
 * started with. Docked is left alone; the system drops the setting when the title exits.
 */
void SwitchApplyGpuBoost(bool on);
