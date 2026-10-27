#pragma once

namespace dc::game {
class CRect_i_;
} // namespace dc::game

namespace dc::text::credits {

/**
 * Draws one band of an end-credits page (endname1..4) as Chronicle Sharp lettering at the screen's own resolution,
 * clipped to the band. Returns false for any other sprite, or when the font is not ready.
 */
bool DrawBand(const char *texture, const game::CRect_i_ &screen, const game::CRect_i_ &texel, int alpha);

} // namespace dc::text::credits
