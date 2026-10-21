#pragma once

#include "common.h"

#include <span>

#include "rect.hpp"

class CTexture;

// What the title replacement units share.

// The previous frame drawn over the whole logical frame at alpha (0x80 opaque) through the current
// ALPHA register: the motion trail of the title scenes.
void TitlePortFeedback(u_char alpha);

// One band of the title's fog: frame_image's texel rect drawn back over the screen rect at alpha.
struct TitleFogBand {
    CRect_i_ screen;
    CRect_i_ texel;
    u_char   alpha = {};
};

// The fog's bands, softened as the PS2 softened them (see the definition).
void TitlePortFog(std::span<const TitleFogBand> bands);

/**
 * Captures the full visible title background for the next fog pass.
 */
void TitlePortCaptureFog();

/**
 * Scales the title smoke vertically to keep its animation returns outside the visible frame.
 */
float TitlePortBackdropHeightScale();

// How much larger than retail's the title's backdrops are drawn (the title screen's sky model and
// its cloud, whose animation keeps the spheres it is not showing just outside the frame's sides;
// the attract movie's turning title card): they are built to fill the logical frame's width and
// no more, so a target that shows past the frame takes them scaled by as much as the view is wider.
float TitlePortBackdropScale();

// The attract movie's title card: retail's 768-texel sprite turned by angle about the frame's
// centre, at TitlePortBackdropScale.
void TitlePortCard(CTexture *texture, float angle);

// Constructs the title overlay's objects the port defines with host classes, as the overlay's
// static constructors did each time retail loaded TITLE.BIN.
void TitleOverlayConstruct();
