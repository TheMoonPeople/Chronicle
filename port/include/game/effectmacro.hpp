#pragma once

#include "common.h"

#include <libvu0.h>

namespace dc::game {

class CEffectGroup;

/**
 * Advances the shared effect state for one frame under the given wind.
 */
void EffectMacroStep(float *wind);

/**
 * Adds a puff of smoke to an effect group.
 */
void EffectSmoke(CEffectGroup *group, float *position, float size, int period);

/**
 * Adds a spray of water to an effect group.
 */
void EffectWaterSpray(CEffectGroup *group, float *position, float *extent, int period, int phase);

/**
 * Adds a spreading ripple to an effect group.
 */
void EffectHamon(CEffectGroup *group, float *position, float size);

/**
 * Blurs screen regions outside the two depth intervals surrounding the focus.
 */
void DepthOfField(float *focus, int level, int alpha, int blur);

} // namespace dc::game
