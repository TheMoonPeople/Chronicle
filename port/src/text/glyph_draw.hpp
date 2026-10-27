#pragma once

#include "platform/glyphs.hpp"
#include "rect.hpp"

namespace dc::text {

/**
 * Draws a pad button's symbol (platform/glyphs.hpp) centred in a slot of 2D screen space, keeping
 * the art's proportions; grow adds pixels on every side. False when the game's own art is used.
 */
bool GlyphDrawSlot(glyphs::Button button, const game::CRect_i_ &slot, int alpha, float grow = 0.0f);

/**
 * Counts the button symbols painted into the named texture
 * that GlyphReplaceSprite and GlyphOverlaySprite know about.
 */
int GlyphBakedCount(const char *texture);

/**
 * Draws a button symbol in place of a sprite whose texel rectangle is about one of the painted
 * boxes in its texture; true when drawn, so the caller skips the sprite.
 */
bool GlyphReplaceSprite(const char *texture, const game::CRect_i_ &screen, const game::CRect_i_ &texel, int alpha);

/**
 * Draws button symbols over painted boxes lying within a larger sprite;
 * call after drawing the sprite.
 */
void GlyphOverlaySprite(const char *texture, const game::CRect_i_ &screen, const game::CRect_i_ &texel, int alpha);

} // namespace dc::text
