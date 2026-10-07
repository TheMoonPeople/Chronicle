#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "input.hpp"

// Scripted input for headless runs. Each line is
//
//     <frame> [pad1|pad2] [button ...] [key:<name> ...] [mouseN ...] [mouse:dx,dy] [wheel:notches] [lx ly rx ry]
//
// and holds those buttons (and sticks, centred when omitted) on that pad (pad 1 when omitted) from
// that frame of the game's main loop until the pad's next line. Keys (SDL names), mouse buttons and
// mouse motion (counts per tick) go through the keyboard and mouse bindings and the mouse look as
// live input does, on pad 1 only. Wheel notches are per tick, positive zooms in; use a released line
// on the next frame for one wheel step. Frame 0 covers the start-up warm-up too. `#` starts a comment.
// Pad 1 is held released before its first line; pad 2 is left to its device unless a line names it.

struct InputScriptStep {
    std::int64_t       frame = 0;
    int                pad = 0;
    InputPadState      state;
    InputKeyboardMouse devices;
    bool               uses_devices = false;
};

struct InputScript {
    std::vector<InputScriptStep> steps;
};

// Parses a script. On failure returns false and sets error to "line N: why".
bool InputScriptParse(std::string_view text, InputScript &script, std::string &error);

bool InputScriptLoad(const std::filesystem::path &path, InputScript &script, std::string &error);

// The state the script holds pad (0 or 1) in at frame.
InputPadState InputScriptStateAt(const InputScript &script, int pad, std::int64_t frame);

// The keyboard and mouse pad 1's line holds at frame, which also reach the host actions
// (InputSetScriptedDevices): key:grave toggles debug mode as the live key does.
InputKeyboardMouse InputScriptDevicesAt(const InputScript &script, std::int64_t frame);

bool InputScriptDrivesPad(const InputScript &script, int pad);

// Installs script as the override of the pads it drives; InputScriptApply then moves it to a
// frame. An empty script uninstalls.
void InputScriptInstall(InputScript script);

void InputScriptApply(std::int64_t frame);

bool InputScriptActive();
