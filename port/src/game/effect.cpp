#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "effect.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include "mathutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"

void C3DSprite::Draw() {
    if (texture == NULL) {
        return;
    }

    position[3] = 1.0f;
    int top_left[4];
    int bottom_right[4];

    if (MGRotTransPers3DSprite(top_left, bottom_right, position, half_width, half_height, 0) == 0) {
        return;
    }

    sceGsAlpha alpha = mgAlpha;

    if (alpha_blend > 0) {
        alpha.bits.a = 0;
        alpha.bits.b = 2;
        alpha.bits.c = 0;
        alpha.bits.d = 1;
        MGSetGsALPHA(&alpha);
    }

    if (disable_z_write != 0) {
        sceGsZbuf zbuffer = mgZBuffer;
        zbuffer.bits.zmsk = 1;
        MGSetGsZBUF(&zbuffer);
    }

    set3DSprite(GetVif1Packet(), texture, texel, top_left, bottom_right, &colour);

    if (alpha_blend != 0) {
        MGSetGsALPHA(&mgAlpha);
    }

    if (disable_z_write != 0) {
        MGSetGsZBUF(&mgZBuffer);
    }
}

void C3DSprite::Initialize() {
    texture = NULL;
    texel = CRect_i_(0, 0, 0, 0);
    colour.a = 128;
    colour.g = 128;
    colour.b = 128;
    colour.r = 128;
    alpha_blend = false;
    disable_z_write = false;
}

void CEffect::SetEffect(CEffectParam *parameters) {
    active = true;
    frame = 0;
    draw_mode = (s16) parameters->draw_mode;
    lifetime = (s16) parameters->lifetime;
    position_oscillation_flags = parameters->position_oscillation_flags;
    scale_oscillation_flags = parameters->scale_oscillation_flags;
    width = parameters->width;
    height = parameters->height;
    sceVu0CopyVector(position, parameters->position);
    position[3] = 1.0f;
    sceVu0CopyVector(velocity, parameters->velocity);
    sceVu0CopyVector(acceleration, parameters->acceleration);
    sceVu0CopyVector(position_oscillation_scale, parameters->position_oscillation_scale);
    sceVu0CopyVector(position_oscillation_rate, parameters->position_oscillation_rate);
    sceVu0CopyVector(scale, parameters->scale);
    sceVu0CopyVector(scale_velocity, parameters->scale_velocity);
    sceVu0CopyVector(scale_oscillation_scale, parameters->scale_oscillation_scale);
    sceVu0CopyVector(scale_oscillation_rate, parameters->scale_oscillation_rate);
    opacity_mode = parameters->opacity_mode;
    render_flags = parameters->render_flags;
    texture = parameters->texture;
    texel = parameters->texel;
    texture_frames = parameters->texture_frames;
    texture_frame_period = parameters->texture_frame_period;

    switch (opacity_mode) {
        case EFFECT_OPACITY_CONSTANT:
            opacity = parameters->opacity;
            opacity_step = 0.0f;
            break;
        case EFFECT_OPACITY_FADE_IN:
            opacity = parameters->opacity;
            opacity_step = (1.0f - parameters->opacity) / (float) lifetime;
            break;
        case EFFECT_OPACITY_FADE_OUT:
            opacity = parameters->opacity;
            opacity_step = -parameters->opacity / (float) lifetime;
            break;
    }
}

void CEffect::Step(int unused) {
    if (active == 0) {
        return;
    }

    frame++;

    if (frame > lifetime) {
        frame = 0;
        active = false;
    }

    sceVu0AddVector(position, position, velocity);
    sceVu0AddVector(velocity, velocity, acceleration);

    if ((position_oscillation_flags & 1) != 0) {
        float phase = (float) frame;

        if (position_oscillation_scale[0] > 0.0f) {
            position[0] += position_oscillation_scale[0] * Sinf(phase * position_oscillation_rate[0]);
        }

        if (position_oscillation_scale[1] > 0.0f) {
            position[1] += position_oscillation_scale[1] * Sinf(phase * position_oscillation_rate[1]);
        }

        if (position_oscillation_scale[2] > 0.0f) {
            position[2] += position_oscillation_scale[2] * Sinf(phase * position_oscillation_rate[2]);
        }
    }

    scale[0] += scale_velocity[0];
    scale[1] += scale_velocity[1];

    if ((scale_oscillation_flags & 1) != 0) {
        float phase = (float) frame;

        if (scale_oscillation_scale[0] > 0.0f) {
            scale[0] += scale_oscillation_scale[0] * Sinf(phase * scale_oscillation_rate[0]);
        }

        if (scale_oscillation_scale[1] > 0.0f) {
            scale[1] += scale_oscillation_scale[1] * Sinf(phase * scale_oscillation_rate[1]);
        }
    }

    opacity += opacity_step;

    if (opacity < 0.0f) {
        opacity = 0.0f;
    }

    if (opacity > 1.0f) {
        opacity = 1.0f;
    }
}

void CEffect::Draw() {
    if (active == 0 || texture == NULL) {
        return;
    }

    position[3] = 1.0f;
    float sprite_width = scale[0] * width;
    float sprite_height = scale[1] * height;
    int   top_left[4];
    int   bottom_right[4];
    int   screen[4][4];
    float corner[4][4];

    if (draw_mode != 0) {
        for (int i = 0; i < 4; i++) {
            sceVu0CopyVector(corner[i], position);
        }

        corner[0][0] -= sprite_width / 2.0f;
        corner[0][2] -= sprite_height / 2.0f;
        corner[1][0] += sprite_width / 2.0f;
        corner[1][2] -= sprite_height / 2.0f;
        corner[2][0] -= sprite_width / 2.0f;
        corner[2][2] += sprite_height / 2.0f;
        corner[3][0] += sprite_width / 2.0f;
        corner[3][2] += sprite_height / 2.0f;

        for (int i = 0; i < 4; i++) {
            if (MGRotTransPers(screen[i], corner[i], 0) == 0) {
                return;
            }
        }
    } else if (MGRotTransPers3DSprite(top_left, bottom_right, position, sprite_width, sprite_height, 0) == 0) {
        return;
    }

    if (render_flags != 0) {
        sceGsZbuf  zbuffer = mgZBuffer;
        sceGsAlpha alpha = mgAlpha;
        zbuffer.bits.zmsk = 1;
        MGSetGsZBUF(&zbuffer);

        if ((render_flags & 1) != 0) {
            alpha.bits.a = 0;
            alpha.bits.b = 2;
            alpha.bits.c = 0;
            alpha.bits.d = 1;
        }

        if ((render_flags & 2) != 0) {
            alpha.bits.a = 2;
            alpha.bits.b = 0;
            alpha.bits.c = 0;
            alpha.bits.d = 1;
        }

        MGSetGsALPHA(&alpha);
    }

    // Texture changes lag their corresponding source rectangle by one draw.
    CTexture            *draw_texture = texture;
    CRect_i_             draw_texel = texel;
    CEffectTextureFrame *animation;

    if (texture_frames != NULL) {
        int animation_frame = frame;

        if (texture_frame_period > 0) {
            animation_frame %= texture_frame_period;
        }

        for (animation = texture_frames; animation != NULL; animation = animation->next) {
            if (animation_frame < animation->end_frame) {
                texture = animation->texture;
                draw_texel = animation->texel;
                break;
            }
        }
    }

    spRGBA colour = {0x68, 0x80, 0x80, (int) (opacity * 128.0f)};

    if (draw_mode != 0) {
        set3DSprite(GetVif1Packet(), draw_texture, draw_texel, screen[0], screen[1], screen[2], screen[3], &colour);
    } else {
        set3DSprite(GetVif1Packet(), draw_texture, draw_texel, top_left, bottom_right, &colour);
    }

    if (render_flags != 0) {
        MGSetGsZBUF(NULL);
        MGSetGsALPHA(NULL);
    }
}

void CEffectParam::Initialize() {
    lifetime = 0;
    draw_mode = 0;
    position_oscillation_flags = 0;
    scale_oscillation_flags = 0;
    opacity_mode = EFFECT_OPACITY_CONSTANT;
    opacity = 1.0f;
    render_flags = 0;
    width = height = 0.0f;
    position[0] = position[1] = position[2] = position[3] = 0.0f;
    sceVu0CopyVector(velocity, position);
    sceVu0CopyVector(acceleration, position);
    sceVu0CopyVector(position_oscillation_scale, position);
    sceVu0CopyVector(position_oscillation_rate, position);
    scale[0] = scale[1] = 1.0f;
    sceVu0CopyVector(scale_velocity, position);
    sceVu0CopyVector(scale_oscillation_scale, position);
    sceVu0CopyVector(scale_oscillation_rate, position);
    texture = NULL;
    texel = CRect_i_(0, 0, 0, 0);
    texture_frame_period = 0;
    texture_frames = NULL;
}

void CEffect::Initialize() {
    active = false;
    frame = 0;
    lifetime = 0;
    draw_mode = 0;
    position_oscillation_flags = 0;
    scale_oscillation_flags = 0;
    opacity_mode = EFFECT_OPACITY_CONSTANT;
    opacity = 1.0f;
    render_flags = 0;
    opacity_step = 0.0f;
    width = height = 0.0f;
    position[0] = position[1] = position[2] = position[3] = 0.0f;
    sceVu0CopyVector(velocity, position);
    sceVu0CopyVector(acceleration, position);
    sceVu0CopyVector(position_oscillation_scale, position);
    sceVu0CopyVector(position_oscillation_rate, position);
    scale[0] = scale[1] = 1.0f;
    sceVu0CopyVector(scale_velocity, position);
    sceVu0CopyVector(scale_oscillation_scale, position);
    sceVu0CopyVector(scale_oscillation_rate, position);
    texture = NULL;
    texel = CRect_i_(0, 0, 0, 0);
    texture_frames = NULL;
    texture_frame_period = 0;
}
