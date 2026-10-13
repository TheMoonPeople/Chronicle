#pragma once

#include <string>
#include <utility>
#include <vector>

struct PortDecodedTexture;

namespace areacard {

/**
 * Area-name translation keys and English defaults.
 */
std::vector<std::pair<std::string, std::string>> Strings();

void Clear();

/**
 * Replaces a card with its text-free plate when the font and layout are available.
 */
bool Prepare(const std::string &name, PortDecodedTexture &texture);

bool Prepared();

/**
 * Draws the prepared name inside the loading sprite, with fade from zero to one.
 */
void Draw(float x0, float y0, float x1, float y1, float fade);

} // namespace areacard
