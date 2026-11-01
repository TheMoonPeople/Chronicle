#pragma once

#include <string>
#include <utility>
#include <vector>

namespace dc::ui::georama_requests {

/**
 * Opens the town's request list from the Georama menu's Analysis page, at its first building.
 * texture_block is a block of the menu's free while the list is open, for the banner's picture.
 */
void Open(int texture_block);

/**
 * Handles gamepad input while the list is open. Returns true when the player backs out to the
 * Analysis page.
 */
bool HandleKey();

/**
 * Draws the request list with the UI framework.
 */
void Draw();

/**
 * The page arrow under (x, y), in logical coordinates: -1 back, 1 on, 0 for neither.
 */
int PageArrowAt(float x, float y);

/**
 * Appends the request list's localization keys and their default English strings to out.
 */
void AppendStrings(std::vector<std::pair<std::string, std::string>> &out);

} // namespace dc::ui::georama_requests
