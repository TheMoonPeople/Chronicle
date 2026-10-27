#pragma once

#include <cstdint>

#include "gfx/gfx.hpp"

namespace dc::platform {

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
 * Returns the fixed output dimensions for the current console operation mode.
 */
void SwitchDisplaySize(int &width, int &height);

/**
 * Logs the operation mode, performance mode and selected APM performance configuration.
 */
void SwitchLogClocks();

/**
 * Reports service-read CPU, GPU and EMC rates under DC_PERF_CLOCKS, and process/allocator memory under
 * independent DC_PERF_MEMORY. Query failures are nonfatal; memory reporting does not access clock services.
 * DC_PERF_CALIBRATE independently reports a fixed integer-throughput probe, without measuring CPU frequency.
 */
void SwitchLogClockRates();

/**
 * Reads the current thread's scheduled ticks when DC_PERF_THREAD_TICKS is enabled.
 * Returns false without querying when disabled; query failures are logged once and disable further reads.
 */
bool SwitchReadThreadTicks(std::uint64_t &ticks);

/**
 * Converts a scheduled-tick delta to milliseconds using libnx's system-counter conversion.
 */
double SwitchThreadTicksMilliseconds(std::uint64_t delta);

/**
 * Chooses a render target scale from the current console operation mode.
 * The game keeps its logical PAL resolution while using the display's available pixels.
 */
void SwitchConfigureRenderer(gfx::RendererConfig &config);

/**
 * Places the game and active render threads on separate available CPU cores.
 * Leaves their scheduler settings alone when the process has fewer than two cores.
 */
void SwitchConfigureThreads();

/**
 * Shows missing-data instructions on the console until + is pressed or the applet exits.
 */
void SwitchShowMissingData();

} // namespace dc::platform
