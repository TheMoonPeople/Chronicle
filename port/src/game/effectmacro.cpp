#include "effectmacro.hpp"

#include <libvu0.h>

#include <cstdlib>

#include "effect.hpp"
#include "effectgroup.hpp"
#include "frame.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "texture.hpp"

sceVu0FVECTOR wind_dir = {0.0f, 0.0f, 0.0f, 0.0f};
int           effect_count;
CTexture     *smoke_tex;
CTexture     *sibuki_tex;
CTexture     *hamon_tex;

void EffectMacroStep(float *wind) {
    sceVu0CopyVector(wind_dir, wind);
    effect_count++;
    smoke_tex = TexManager.GetTexture("smoke", -1);
    sibuki_tex = TexManager.GetTexture("shibuki", -1);
    hamon_tex = TexManager.GetTexture("hamon", -1);
}

void EffectSmoke(CEffectGroup *group, float *position, float size, int period) {
    CEffectParam effect;

    // One puff every `period` frames, so a caller can thin its own smoke.
    if (effect_count % period != 0) {
        return;
    }

    effect.texel.x = effect.texel.y = effect.texel.width = effect.texel.height = 0;
    effect.Initialize();
    sceVu0CopyVector(effect.position, position);

    // Half the puffs drift, on a sine of their own in x and z.
    if (rand() % 2 != 0) {
        effect.position_oscillation_flags = 1;
        effect.position_oscillation_scale[0] = 0.3f * (float) rand() / 2.1474836e9f;
        effect.position_oscillation_rate[0] = PI / (30.0f + (float) (rand() * 20) / 2.1474836e9f);
        effect.position_oscillation_scale[2] = 0.3f * (float) rand() / 2.1474836e9f;
        effect.position_oscillation_rate[2] = PI / (30.0f + (float) (rand() * 20) / 2.1474836e9f);
    }

    effect.opacity_mode = EFFECT_OPACITY_FADE_OUT;
    effect.render_flags = 1;
    effect.opacity = 0.1f;
    sceVu0ScaleVector(effect.velocity, effect.velocity, 0.4f);
    effect.velocity[1] += 0.2f + 0.2f * (float) rand() / 2.1474836e9f;
    effect.scale_velocity[0] = 0.02f;
    effect.scale_velocity[1] = 0.02f;
    effect.scale[0] = size;
    effect.scale[1] = size;
    effect.lifetime = 0x78;
    effect.texture = smoke_tex;

    // The four puff shapes are the quarters of the texture.
    switch (rand() % 4) {
        case 0:
            effect.texel = CRect_i_(0, 0, 0x40, 0x40);
            break;
        case 1:
            effect.texel = CRect_i_(0x40, 0, 0x40, 0x40);
            break;
        case 2:
            effect.texel = CRect_i_(0, 0x40, 0x40, 0x40);
            break;
        case 3:
            effect.texel = CRect_i_(0x40, 0x40, 0x40, 0x40);
            break;
    }

    effect.width = 10.0f;
    effect.height = 10.0f;
    group->EnterEffect(&effect);
}

void EffectWaterSpray(CEffectGroup *group, float *position, float *extent, int period, int phase) {
    CEffectParam effect;

    if ((effect_count + phase) % period != 0) {
        return;
    }

    effect.texel.x = effect.texel.y = effect.texel.width = effect.texel.height = 0;
    effect.Initialize();
    sceVu0CopyVector(effect.position, position);
    effect.opacity_mode = EFFECT_OPACITY_FADE_OUT;
    effect.render_flags = 1;
    effect.opacity = 0.5f;

    float spread_x = 0.2f * extent[0];
    float spread_z = 0.2f * extent[2];
    float rise = 0.1f * extent[1] * (1.0f + (float) rand() / 2.1474836e9f);
    float width_grown = rise + 0.5f * (extent[0] + extent[2]);
    float height_grown = extent[1] + rise;

    effect.velocity[0] = spread_x * (float) rand() / 2.1474836e9f - 0.5f * spread_x;
    effect.velocity[2] = (spread_x * (float) rand() / 2.1474836e9f - 0.5f * spread_z) - 0.5f;
    effect.velocity[1] = 1.0f + 0.5f * (float) rand() / 2.1474836e9f;
    effect.acceleration[1] = -0.03f;
    effect.scale_velocity[0] = width_grown / 30.0f;
    effect.scale_velocity[1] = height_grown / 30.0f;
    effect.scale[0] = 0.2f * width_grown;
    effect.scale[1] = 0.2f * height_grown;
    effect.lifetime = 0x28;
    effect.texture = sibuki_tex;
    effect.texel = CRect_i_(0, 0, 0x40, 0x40);
    effect.width = 10.0f;
    effect.height = 10.0f;
    group->EnterEffect(&effect);
}

void EffectHamon(CEffectGroup *group, float *position, float size) {
    CEffectParam effect;

    effect.texel.x = effect.texel.y = effect.texel.width = effect.texel.height = 0;
    effect.Initialize();
    sceVu0CopyVector(effect.position, position);
    effect.opacity_mode = EFFECT_OPACITY_FADE_OUT;
    effect.render_flags = 1;
    effect.opacity = 1.0f;
    effect.draw_mode = 1;
    effect.lifetime = 0x28;
    // The ring starts at nothing and spreads for its whole life.
    effect.scale_velocity[0] = 0.025f;
    effect.scale_velocity[1] = 0.025f;
    effect.scale_velocity[2] = 0.025f;
    effect.scale[0] = 0.0f;
    effect.scale[1] = 0.0f;
    effect.scale[2] = 0.0f;
    effect.texture = hamon_tex;
    effect.texel = CRect_i_(0, 0, 0x20, 0x20);
    effect.width = size;
    effect.height = size;
    group->EnterEffect(&effect);
}

void DepthOfField(float *focus, int level, int alpha, int blur);

