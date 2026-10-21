
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace JournalChest {

/// Resets selection and snaps the cursor when opening the Chest Tracker from the Journal menu.
void Open();

/// Handles gamepad input while the Chest Tracker is active. Returns true if the user exited back to the Journal menu.
bool HandleKey();

// Select a settlement or toggle the filter at the displayed pointer position.
void HandlePointer(float x, float y);

/// Renders the Chest Tracker dashboard using the UI framework.
void Draw();

/// Appends the Chest Tracker's localization keys and default English strings to `out`.
void AppendStrings(std::vector<std::pair<std::string, std::string>> &out);

} // namespace JournalChest
