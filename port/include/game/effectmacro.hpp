#pragma once

#include "common.h"

#include <libvu0.h>

class CEffectGroup;
class CTexture;

/**
 * Advances the shared effect state for one frame under the given wind.
 *
 * @mangled EffectMacroStep__FPf
 * @address 0x164B30
 * @size 0x98
 */
void EffectMacroStep(float *wind);

/**
 * Adds a puff of smoke to an effect group.
 *
 * @mangled EffectSmoke__FP12CEffectGroupPffi
 * @address 0x164BD0
 * @size 0x350
 */
void EffectSmoke(CEffectGroup *group, float *position, float size, int period);

/**
 * Adds a spray of water to an effect group.
 *
 * @mangled EffectWaterSpray__FP12CEffectGroupPfPfii
 * @address 0x164F20
 * @size 0x29C
 */
void EffectWaterSpray(CEffectGroup *group, float *position, float *extent, int period, int phase);

/**
 * Adds a spreading ripple to an effect group.
 *
 * @mangled EffectHamon__FP12CEffectGroupPff
 * @address 0x1651C0
 * @size 0xF4
 */
void EffectHamon(CEffectGroup *group, float *position, float size);

/**
 * Blurs screen regions outside the two depth intervals surrounding the focus.
 *
 * @mangled DepthOfField__FPfiii
 * @address 0x1652C0
 * @size 0x9D0
 */
void DepthOfField(float *focus, int level, int alpha, int blur);

/**
 * Direction the wind blows the effects in.
 */
extern sceVu0FVECTOR wind_dir;
/**
 * Frames the effects have been running, which thins how often they spawn.
 */
extern int effect_count;
/**
 * Texture the smoke effect draws with, looked up once a frame.
 */
extern CTexture *smoke_tex;
/**
 * Texture the water-spray effect draws with, looked up once a frame.
 */
extern CTexture *sibuki_tex;
/**
 * Texture the ripple effect draws with, looked up once a frame.
 */
extern CTexture *hamon_tex;
