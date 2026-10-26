#pragma once

#include "common.h"

#include "rect.hpp"

/**
 * How an effect's opacity changes.
 */
// clang-format off
enum EffectOpacityMode {
    EFFECT_OPACITY_CONSTANT = 0, /**< Constant. */
    EFFECT_OPACITY_FADE_IN  = 1, /**< Increasing. */
    EFFECT_OPACITY_FADE_OUT = 2, /**< Decreasing. */
};

// clang-format on

class CTexture;

/**
 * Selects a texture and source rectangle until an exclusive animation frame.
 */
struct CEffectTextureFrame {
    int                  end_frame; /**< Exclusive final frame using this entry. */
    CTexture            *texture;   /**< Texture selected when this entry is active. */
    u32                  unk_08;
    u32                  unk_0c;
    CRect_i_             texel; /**< Rectangle sampled while this entry is active. */
    CEffectTextureFrame *next;  /**< Next animation interval, or null at the end. */
};

STATIC_ASSERT(sizeof(CEffectTextureFrame) == 0x30);

/**
 * Describes the initial motion, appearance, and animation of one effect.
 */
class CEffectParam {
public:
    int                  lifetime;                   /**< Number of frames before the effect is retired. */
    int                  position_oscillation_flags; /**< Bit zero enables sinusoidal position offsets. */
    float                width;                      /**< Initial unscaled sprite width. */
    float                height;                     /**< Initial unscaled sprite height. */
    int                  draw_mode;                  /**< Selects a world-oriented quad instead of a billboard. */
    int                  scale_oscillation_flags;    /**< Bit zero enables sinusoidal scale offsets. */
    u32                  unk_18;
    u32                  unk_1c;
    float                position[4];                   /**< Initial homogeneous world-space position. */
    float                velocity[4];                   /**< Position added on every step. */
    float                acceleration[4];               /**< Velocity added on every step. */
    float                position_oscillation_scale[4]; /**< Per-axis sinusoidal position amplitudes. */
    float                position_oscillation_rate[4];  /**< Per-axis sinusoidal phase rates. */
    float                scale[4];                      /**< Initial width and height scale. */
    float                scale_velocity[4];             /**< Scale added on every step. */
    float                scale_oscillation_scale[4];    /**< Per-axis sinusoidal scale amplitudes. */
    float                scale_oscillation_rate[4];     /**< Per-axis sinusoidal phase rates. */
    int                  opacity_mode;                  /**< Selects constant, increasing, or decreasing opacity. */
    int                  render_flags;                  /**< Selects temporary alpha and depth-buffer state. */
    float                opacity;                       /**< Initial opacity in the inclusive range zero to one. */
    CTexture            *texture;                       /**< Base texture used to draw the effect. */
    CRect_i_             texel;                         /**< Base rectangle sampled from the texture. */
    CEffectTextureFrame *texture_frames;                /**< Optional linked texture-animation frame list. */
    int                  texture_frame_period;          /**< Modulus applied before selecting a texture frame. */
    u32                  unk_d8;
    u32                  unk_dc;

    /**
     * Initializes an effect description to its neutral defaults.
     *
     * @mangled Initialize__12CEffectParamFv
     * @address 0x164770
     * @size 0x100
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(CEffectParam) == 0xE0);

/**
 * Stores one live animated sprite effect in an effect-group pool.
 */
class CEffect {
public:
    s16                  active;                     /**< Whether this pool slot contains a live effect. */
    s16                  frame;                      /**< Current zero-based effect frame. */
    s16                  lifetime;                   /**< Last frame before the effect is retired. */
    s16                  draw_mode;                  /**< Selects a world-oriented quad instead of a billboard. */
    float                width;                      /**< Unscaled sprite width. */
    float                height;                     /**< Unscaled sprite height. */
    int                  position_oscillation_flags; /**< Bit zero enables sinusoidal position offsets. */
    u32                  unk_14;
    u32                  unk_18;
    u32                  unk_1c;
    float                position[4];                   /**< Current homogeneous world-space position. */
    float                velocity[4];                   /**< Position added on every step. */
    float                acceleration[4];               /**< Velocity added on every step. */
    float                position_oscillation_scale[4]; /**< Per-axis sinusoidal position amplitudes. */
    float                position_oscillation_rate[4];  /**< Per-axis sinusoidal phase rates. */
    int                  scale_oscillation_flags;       /**< Bit zero enables sinusoidal scale offsets. */
    u32                  unk_74;
    u32                  unk_78;
    u32                  unk_7c;
    float                scale[4];                   /**< Current width and height scale. */
    float                scale_velocity[4];          /**< Scale added on every step. */
    float                scale_oscillation_scale[4]; /**< Per-axis sinusoidal scale amplitudes. */
    float                scale_oscillation_rate[4];  /**< Per-axis sinusoidal phase rates. */
    int                  opacity_mode;               /**< Mode used to derive the opacity step. */
    float                opacity;                    /**< Current opacity clamped from zero to one. */
    float                opacity_step;               /**< Opacity added on every step. */
    int                  render_flags;               /**< Selects temporary alpha and depth-buffer state. */
    CTexture            *texture;                    /**< Texture currently assigned to the effect. */
    u32                  unk_d4;
    u32                  unk_d8;
    u32                  unk_dc;
    CRect_i_             texel;                /**< Rectangle currently sampled from the texture. */
    CEffectTextureFrame *texture_frames;       /**< Optional linked texture-animation frame list. */
    int                  texture_frame_period; /**< Modulus applied before selecting a texture frame. */
    u32                  unk_f8;
    u32                  unk_fc;

    /**
     * Copies an effect description into this pool slot and activates it.
     *
     * @mangled SetEffect__7CEffectFP12CEffectParam
     * @address 0x163EE0
     * @size 0x1D8
     */
    void SetEffect(CEffectParam *parameters);

    /**
     * Advances lifetime, motion, scale, and opacity by one frame.
     *
     * @mangled Step__7CEffectFi
     * @address 0x1640C0
     * @size 0x278
     */
    void Step(int unused);

    /**
     * Projects and draws the active effect with its requested render state.
     *
     * @mangled Draw__7CEffectFv
     * @address 0x164340
     * @size 0x424
     */
    void Draw();

    /**
     * Resets this effect pool slot to its inactive neutral state.
     *
     * @mangled Initialize__7CEffectFv
     * @address 0x164870
     * @size 0x10C
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(CEffect) == 0x100);

/**
 * Draws a textured rectangle positioned in three-dimensional world space.
 */
class C3DSprite {
public:
    float     position[4]; /**< Homogeneous world-space centre of the sprite. */
    float     half_width;  /**< Half of the sprite's world-space width. */
    float     half_height; /**< Half of the sprite's world-space height. */
    CTexture *texture;     /**< Texture drawn by the sprite. */
    u32       unk_1c;
    CRect_i_  texel;           /**< Rectangle sampled from the texture. */
    spRGBA    colour;          /**< Colour and opacity applied to the sprite. */
    int       alpha_blend;     /**< Whether the sprite temporarily enables alpha blending. */
    int       disable_z_write; /**< Whether the sprite temporarily disables depth writes. */
    u32       unk_3c;

    /**
     * Constructs a sprite with an empty source rectangle and no texture.
     *
     * @mangled __ct__9C3DSpriteFv
     * @address 0x183760
     * @size 0x40
     */
    C3DSprite();

    /**
     * Draws the sprite when its texture and projected rectangle are valid.
     *
     * @mangled Draw__9C3DSpriteFv
     * @address 0x163D20
     * @size 0x170
     */
    void Draw();

    /**
     * Clears sprite rendering state while preserving its placement and scale.
     *
     * @mangled Initialize__9C3DSpriteFv
     * @address 0x163E90
     * @size 0x4C
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(C3DSprite) == 0x40);
