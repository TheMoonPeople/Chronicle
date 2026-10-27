#pragma once

#include <string>
#include <utility>
#include <vector>

namespace dc::ui::journal_chest {

/**
 * Resets the selection and snaps the cursor when the Chest Tracker opens from the Journal menu.
 */
void Open();

/**
 * Handles gamepad input while the Chest Tracker is active. Returns true when the player backs out
 * to the Journal menu.
 */
bool HandleKey();

/**
 * Selects a settlement or toggles the filter at the given pointer position in display coordinates.
 */
void HandlePointer(float x, float y);

/**
 * Draws the Chest Tracker page with the UI framework.
 */
void Draw();

/**
 * Appends the Chest Tracker's localization keys and their default English strings to out.
 */
void AppendStrings(std::vector<std::pair<std::string, std::string>> &out);

} // namespace dc::ui::journal_chest

// DC_TRANSITIONAL_BEGIN
#ifndef DC_NO_TRANSITIONAL_USING
using namespace dc::ui;
namespace JournalChest = dc::ui::journal_chest;
#endif
// DC_TRANSITIONAL_END
