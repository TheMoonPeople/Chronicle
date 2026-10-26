#include "fireomni.hpp"

#include <cstdlib>

#include "camera.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "runeffect.hpp"
#include "snd.hpp"
#include "texture.hpp"

CFireOmni::CFireOmni() {
    raster_phase = 0.0f;
    flame_phase = 0.0f;
    cell_phase = 15.0f;
    flicker_width = 0.0f;
    flicker_height = 0.0f;
    // Starting the count somewhere in the cycle keeps two fires out of step.
    flicker_count = (int) (4.0f * (float) rand() / 2.1474836e9f);
    flicker_seed = 0;
    unk_00 = 0;
    glow = NULL;
    core = NULL;
    texture_set = 0;
}

void CFireOmni::FireStep() {
    cell_phase -= 0.4f;

    if (cell_phase <= 0.0f) {
        cell_phase = 14.8f;
    }

    flame_phase += 1.0f;

    if (!(flame_phase < 128.0f)) {
        flame_phase = 0.0f;
    }

    flicker_seed = (int) (60000.0f * (float) rand() / 2.1474836e9f);
}

void CFireOmni::FireCreate() {
    CTexture *texture = TexManager.GetTexture("d01e02", -1);

    if (texture == NULL) {
        return;
    }

    if (texture->bpp == 1) {
        int       cell = (int) cell_phase;
        int       column = cell % 4;
        int       row = cell / 4;
        sceGsTex0 blend = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("blender", -1)->tex0);
        blendTextuer(Vif1Packet, blend.TBP0, blend.TBW, blend.PSM, TexManager.GetTexture("d01e03", -1), CRect_i_(0, 0, 128, 128), CRect_i_(0, (int) flame_phase, 128, 64), TexManager.GetTexture("d01e02", -1), CRect_i_(0, 0, 128, 128), CRect_i_(column * 64, row * 64, 64, 64));
    } else {
        sceGsTex0 blend = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("blender", -1)->tex0);
        blendTextuer(Vif1Packet, blend.TBP0, blend.TBW, blend.PSM, TexManager.GetTexture("d01e03", -1), CRect_i_(0, 0, 128, 128), CRect_i_(0, (int) flame_phase, 128, 64), TexManager.GetTexture("d01e02", -1), CRect_i_(0, 0, 128, 128), CRect_i_(0, (int) cell_phase * 64, 64, 64));
    }
}

void CFireOmni::SetTexture(CTexture *core_texture, CTexture *glow_texture) {
    this->core = core_texture;
    this->glow = glow_texture;
    this->texture_set = 1;
}

void CFireOmni::RasterStep() {
    float phase = this->raster_phase + ((2.0f * (float) rand()) / 2.1474836e9f);
    this->raster_phase = phase;

    if (!(phase < 8.0f)) {
        this->raster_phase = 0.0f;
    }
}

void CFireOmni::DrawRaster() {
    sceVu0FVECTOR world;
    int           top_left[4];
    int           bottom_right[4];

    world[0] = pos[0];
    world[1] = pos[1] + 3.0f;
    world[2] = pos[2];
    world[3] = 1.0f;

    if (MGRotTransPers3DSprite(top_left, bottom_right, world, 15.0f, 16.0f, 0) != 1) {
        return;
    }

    int x = (top_left[0] - 0x6C00) >> 4;
    int y = (top_left[1] - GS_Y_OFFSET) >> 3;
    int width = (bottom_right[0] - top_left[0]) >> 4;
    int height = (bottom_right[1] - top_left[1]) >> 4;
    height >>= 1;

    if (x + width > 639 && x < 640) {
        width = 639 - x;
    }

    if (x < 0 && x + width > 0) {
        width = x + width;
        x = 0;
    }

    if (y < 1) {
        height += y >> 1;
        y = 0;
    }

    sceGsTex0 blend = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("blender", -1)->tex0);
    blendTextuerTest(Vif1Packet, blend.TBP0, blend.TBW, blend.PSM, CRect_i_(x, y >> 1, width, height), TexManager.GetTexture("alpha01", -1), CRect_i_(0, 0, width, height), CRect_i_(0, 0, 64, 64), top_left[2] >> 4, raster_phase);
    set2DSprite(Vif1Packet, TexManager.GetTexture("blender", -1), CRect_i_(x, y, width, height * 2), CRect_i_(1, 1, width - 2, height - 2));
}
