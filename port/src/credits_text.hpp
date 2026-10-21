#pragma once

class CRect_i_;

namespace credits {

// The end credits' lettering drawn from Chronicle Sharp: the game scrolls the credit pages (endname1..4) up the screen a
// band at a time, and each band is drawn as the lines of the page that fall in it, from the font at the screen's own
// resolution, clipped to the band. False for any other sprite, or without the font.
bool DrawBand(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha);

} // namespace credits
