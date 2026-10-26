#pragma once

#include "common.h"

#include "rect.hpp"

class CTexture;
struct sceVif1Packet;

/**
 * Blends two texture regions into a frame-buffer texture.
 *
 * @mangled blendTextuer__FP13sceVif1PacketiiiP8CTextureRC8CRect_i_RC8CRect_i_P8CTextureRC8CRect_i_RC8CRect_i_
 * @address 0x162580
 * @size 0x73C
 */
void blendTextuer(sceVif1Packet *packet, int destination, int width, int format, CTexture *first_texture, const CRect_i_ &first_destination, const CRect_i_ &first_source, CTexture *second_texture, const CRect_i_ &second_destination, const CRect_i_ &second_source);

/**
 * Fills the blend table with one period of a sine.
 *
 * @mangled initBlendCnt__Fif
 * @address 0x162CC0
 * @size 0xB4
 */
void initBlendCnt(int count, float scale);

/**
 * Copies a frame-buffer region into a work texture in two-line strips that
 * sway sideways by a depth-scaled wave, then blends a texture over the copy.
 *
 * @mangled blendTextuerTest__FP13sceVif1PacketiiiRC8CRect_i_P8CTextureRC8CRect_i_RC8CRect_i_ff
 * @address 0x162D80
 * @size 0x6E4
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
     *
     * @mangled Lighting__10CRunEffectFi
     * @address 0x163470
     * @size 0xC
     */
    void Lighting(int enabled);

    /**
     * Draws the dust the player's run leaves behind.
     *
     * @mangled Draw__10CRunEffectFv
     * @address 0x163480
     * @size 0x46C
     */
    void Draw();

    /**
     * Starts one puff of run dust at a position.
     *
     * @mangled Set__10CRunEffectFPf
     * @address 0x1638F0
     * @size 0x8C
     */
    void Set(float *origin);

    /**
     * Advances the run dust by a frame.
     *
     * @mangled Step__10CRunEffectFv
     * @address 0x163980
     * @size 0x70
     */
    void Step();

    /**
     * Constructs the run effect with no dust standing.
     *
     * @mangled __ct__10CRunEffectFv
     * @address 0x1639F0
     * @size 0x3C
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
