#pragma once

#include "common.h"

namespace dc::game {

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
     */
    void Initialize();

    /**
     * Constructs a candle effect in its initial state.
     */
    CCandleEffect();

    /**
     * Assigns the flame-atlas texture.
     */
    void SetTexture(CTexture *flame_texture);

    /**
     * Sets the flame sprite's world-space half-extents.
     */
    void SetScale(float half_width, float half_height);

    /**
     * Copies the flame sprite's homogeneous world-space centre.
     */
    void SetPosition(float *world);

    /**
     * Advances and wraps the fractional flame-atlas frame.
     */
    void Step();

    /**
     * Draws the current flame-atlas cell as an alpha-blended billboard.
     */
    void Draw();
};

STATIC_ASSERT(sizeof(CCandleEffect) == 0x30);

} // namespace dc::game
