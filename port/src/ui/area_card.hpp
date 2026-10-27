#pragma once

#include <string>
#include <utility>
#include <vector>

namespace dc::render {
struct PortDecodedTexture;
} // namespace dc::render

namespace dc::ui::area_card {

/**
 * Area-name translation keys and their English defaults.
 */
std::vector<std::pair<std::string, std::string>> Strings();

/**
 * Forgets the prepared card, so nothing is drawn until the next Prepare.
 */
void Clear();

/**
 * Replaces a card with its text-free plate when the font and layout are available.
 */
bool Prepare(const std::string &name, render::PortDecodedTexture &texture);

/**
 * Reports whether a card is prepared for drawing.
 */
bool Prepared();

/**
 * Draws the prepared name inside the loading sprite, with fade from zero to one.
 */
void Draw(float x0, float y0, float x1, float y1, float fade);

} // namespace dc::ui::area_card
