#pragma once

#include "platform/glyphs.hpp"
#include "rect.hpp"

// Draws the button symbols of platform/glyphs.hpp into the game's 2D screen space.

// The symbol of a pad button in a slot: the art keeps its proportions, scaled so a round face
// button fills the slot's height, and sits in the slot's middle; a wide one (a key) may reach out
// of the slot up to twice its width. grow adds pixels on every side. False (nothing drawn) when
// the game's own art should be used (see glyphs::Find).
bool GlyphDrawSlot(glyphs::Button button, const CRect_i_ &slot, int alpha, float grow = 0.0f);

/**
 * Counts the painted controller symbols in a texture.
 */
int GlyphBakedCount(const char *texture);

// Button symbols painted into the game's textures (the skip prompt's two buttons, the shoulder
// button labels of the name entry and the character board), by texture name and the box of the
// art in the texture's texels. The sprite draws (RectSprite, snd.cpp) call these for every
// textured rectangle:
//  - Replace: the drawn texel rectangle is (about) one of the boxes; the symbol is drawn in its
//    place and true is returned, so the caller skips the rectangle.
//  - Overlay: the drawn rectangle holds a box among other art; call after drawing the rectangle,
//    and the symbol is drawn over the painted one.
bool GlyphReplaceSprite(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha);
void GlyphOverlaySprite(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha);
