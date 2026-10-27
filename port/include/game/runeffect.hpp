#pragma once

#include "common.h"

#include "rect.hpp"

struct sceVif1Packet;

namespace dc::game {

class CTexture;

/**
 * Blends two texture regions into a frame-buffer texture.
 */
void blendTextuer(sceVif1Packet *packet, int destination, int width, int format, CTexture *first_texture, const CRect_i_ &first_destination, const CRect_i_ &first_source, CTexture *second_texture, const CRect_i_ &second_destination, const CRect_i_ &second_source);

/**
 * Fills the blend table with one period of a sine.
 */
void initBlendCnt(int count, float scale);

/**
 * Copies a frame-buffer region into a work texture in two-line strips that
 * sway sideways by a depth-scaled wave, then blends a texture over the copy.
 */
void blendTextuerTest(sceVif1Packet *packet, int destination, int width, int format, const CRect_i_ &source, CTexture *texture, const CRect_i_ &texture_destination, const CRect_i_ &texture_source, float depth, float phase);

/**
 * Raises the puffs of dust the player's run leaves behind.
 */
class CRunEffect {
public:
    int   lighting; /**< Whether the dust takes light. */
    u8    unk_04[0xC];
    float position[8][4]; /**< World position of each puff of dust. */
    float velocity_y[8];  /**< Speed each puff is rising at, which gravity eats into. */
    int   life[8];        /**< Frames each puff has left before it goes. */

    /**
     * Sets whether the running effect takes light.
     */
    void Lighting(int enabled);

    /**
     * Draws the dust the player's run leaves behind.
     */
    void Draw();

    /**
     * Starts one puff of run dust at a position.
     */
    void Set(float *origin);

    /**
     * Advances the run dust by a frame.
     */
    void Step();

    /**
     * Constructs the run effect with no dust standing.
     */
    CRunEffect();
};

STATIC_ASSERT(sizeof(CRunEffect) == 0xD0);

/**
 * One period of a sine the blend effects scale their offsets by, filled by initBlendCnt.
 */
extern float waveAnimeCnt[32];

/**
 * Toggled each draw of the running effect to alternate between its two footprint textures.
 */
extern int sw;

} // namespace dc::game
