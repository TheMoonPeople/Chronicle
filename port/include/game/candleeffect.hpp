#pragma once

#include "common.h"

class CTexture;

/**
 * Animates and draws the camera-facing sprite used for a candle flame.
 */
class CCandleEffect {
public:
    int       enabled;         /**< Whether the candle sprite can be drawn. */
    float     animation_frame; /**< Fractional frame within the eight-frame flame cycle. */
    CTexture *texture;         /**< Texture containing the 4-by-2 flame atlas. */
    u32       unk_0c;
    float     position[4]; /**< Homogeneous world-space centre of the flame. */
    float     half_width;  /**< Half of the sprite's world-space width. */
    float     half_height; /**< Half of the sprite's world-space height. */
    u32       unk_28;
    u32       unk_2c;

    /**
     * Restores the enabled candle to its initial animation state.
     *
     * @mangled Initialize__13CCandleEffectFv
     * @address 0x163A30
     * @size 0x24
     */
    void Initialize();

    /**
     * Constructs a candle effect in its initial state.
     *
     * @mangled __ct__13CCandleEffectFv
     * @address 0x163A60
     * @size 0x30
     */
    CCandleEffect();

    /**
     * Assigns the flame-atlas texture.
     *
     * @mangled SetTexture__13CCandleEffectFP8CTexture
     * @address 0x163A90
     * @size 0xC
     */
    void SetTexture(CTexture *flame_texture);

    /**
     * Sets the flame sprite's world-space half-extents.
     *
     * @mangled SetScale__13CCandleEffectFff
     * @address 0x163AA0
     * @size 0x10
     */
    void SetScale(float half_width, float half_height);

    /**
     * Copies the flame sprite's homogeneous world-space centre.
     *
     * @mangled SetPosition__13CCandleEffectFPf
     * @address 0x163AB0
     * @size 0x24
     */
    void SetPosition(float *world);

    /**
     * Advances and wraps the fractional flame-atlas frame.
     *
     * @mangled Step__13CCandleEffectFv
     * @address 0x163AE0
     * @size 0x38
     */
    void Step();

    /**
     * Draws the current flame-atlas cell as an alpha-blended billboard.
     *
     * @mangled Draw__13CCandleEffectFv
     * @address 0x163B20
     * @size 0x1F4
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CCandleEffect) == 0x30);
