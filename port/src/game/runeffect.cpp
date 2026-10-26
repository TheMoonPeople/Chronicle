#include "runeffect.hpp"

#include <libgraph.h>
#include <libpkt.h>

#include <cmath>
#include <cstdlib>

#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

float waveAnimeCnt[32];
int   sw;

/**
 * Draws two textures blended over one another.
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
void initBlendCnt(int count, float scale) {
    for (int i = 0; i < count; i++) {
        float degrees = (360.0f / (float) count) * (float) i;
        float wave = sinf(PI * degrees / 180.0f);
        waveAnimeCnt[i] = wave * scale;
    }
}

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
 * Sets whether the running effect takes light.
 *
 * @mangled Lighting__10CRunEffectFi
 * @address 0x163470
 * @size 0xC
 */
void CRunEffect::Lighting(int enabled) {
    lighting = enabled;
}

/**
 * Draws the dust the player's run leaves behind.
 *
 * @mangled Draw__10CRunEffectFv
 * @address 0x163480
 * @size 0x46C
 */

/**
 * Starts one puff of run dust at a position.
 *
 * @mangled Set__10CRunEffectFPf
 * @address 0x1638F0
 * @size 0x8C
 */
void CRunEffect::Set(float *origin) {
    int slot = -1;

    for (int i = 0; i < 8; i++) {
        if (life[i] == 0) {
            slot = i;
        }
    }

    if (slot != -1) {
        position[slot][0] = origin[0];
        position[slot][1] = origin[1];
        position[slot][2] = origin[2];
        position[slot][3] = 1.0f;
        velocity_y[slot] = 0.3f;
        life[slot] = 16;
    }
}

/**
 * Advances the run dust by a frame.
 *
 * @mangled Step__10CRunEffectFv
 * @address 0x163980
 * @size 0x70
 */
void CRunEffect::Step() {
    for (int i = 0; i < 8; i++) {
        if (life[i] != 0) {
            life[i]--;
            velocity_y[i] -= 0.03f;
            position[i][1] += velocity_y[i];
        }
    }
}

/**
 * Constructs the run effect with no dust standing.
 *
 * @mangled __ct__10CRunEffectFv
 * @address 0x1639F0
 * @size 0x3C
 */
CRunEffect::CRunEffect() {
    for (int i = 0; i < 8; i++) {
        velocity_y[i] = 0.0f;
        life[i] = 0;
    }

    lighting = false;
}
