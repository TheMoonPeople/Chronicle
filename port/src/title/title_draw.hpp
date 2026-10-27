#pragma once

#include "common.h"

#include <span>

#include "rect.hpp"

namespace dc::game {
class CTexture;
} // namespace dc::game

namespace dc::title {

/**
 * Draws the previous frame over the whole logical frame at alpha (0x80 opaque) through the
 * current ALPHA register: the motion trail of the title scenes.
 */
void TitlePortFeedback(u_char alpha);

/**
 * One band of the title's fog: frame_image's texel rect drawn back over the screen rect at alpha.
 */
struct TitleFogBand {
    game::CRect_i_ screen;     /**< Destination rect on the logical frame. */
    game::CRect_i_ texel;      /**< Source rect in frame_image. */
    u_char         alpha = {}; /**< Blend alpha, 0x80 opaque. */
};

/**
 * Draws the fog's bands, softened to match how the GS sampled the field copy.
 */
void TitlePortFog(std::span<const TitleFogBand> bands);

/**
 * Captures the full visible title background for the next fog pass.
 */
void TitlePortCaptureFog();

/**
 * Scales the title smoke vertically to keep its animation returns outside the visible frame.
 */
float TitlePortBackdropHeightScale();

/**
 * How much larger than retail's the title's backdrops (sky model, cloud and attract title card)
 * are drawn: they fill only the logical frame's width, so a wider view scales them up to match.
 */
float TitlePortBackdropScale();

/**
 * Draws the attract movie's title card: the 768-texel sprite turned by angle about the frame's
 * centre, at TitlePortBackdropScale.
 */
void TitlePortCard(game::CTexture *texture, float angle);

} // namespace dc::title
