#include "fireomni.hpp"

#include <libgraph.h>

#include <array>
#include <cstdlib>

#include "camera.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "runeffect.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

using namespace dc::render;

CFireOmni::CFireOmni()
    : unk_00(0), raster_phase(0.0f), flame_phase(0.0f), cell_phase(15.0f), flicker_width(0.0f), flicker_height(0.0f), flicker_seed(0),
      // Starting the count somewhere in the cycle keeps two fires out of step.
      flicker_count(static_cast<int>(4.0f * static_cast<float>(std::rand()) / 2.1474836e9f)), texture_set(0), core(nullptr), glow(nullptr) {}

void CFireOmni::FireStep() {
    cell_phase -= 0.4f;

    if (cell_phase <= 0.0f) {
        cell_phase = 14.8f;
    }

    flame_phase += 1.0f;

    if (!(flame_phase < 128.0f)) {
        flame_phase = 0.0f;
    }

    flicker_seed = static_cast<s16>(static_cast<int>(60000.0f * static_cast<float>(std::rand()) / 2.1474836e9f));
}

void CFireOmni::FireCreate() {
    CTexture *texture = TexManager.GetTexture("d01e02", -1);

    if (texture == nullptr) {
        return;
    }

    if (texture->bpp == 1) {
        int       cell = static_cast<int>(cell_phase);
        int       column = cell % 4;
        int       row = cell / 4;
        sceGsTex0 blend = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("blender", -1)->tex0);
        blendTextuer(Vif1Packet, blend.TBP0, blend.TBW, blend.PSM, TexManager.GetTexture("d01e03", -1), CRect_i_(0, 0, 128, 128), CRect_i_(0, static_cast<int>(flame_phase), 128, 64), TexManager.GetTexture("d01e02", -1), CRect_i_(0, 0, 128, 128), CRect_i_(column * 64, row * 64, 64, 64));
    } else {
        sceGsTex0 blend = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("blender", -1)->tex0);
        blendTextuer(Vif1Packet, blend.TBP0, blend.TBW, blend.PSM, TexManager.GetTexture("d01e03", -1), CRect_i_(0, 0, 128, 128), CRect_i_(0, static_cast<int>(flame_phase), 128, 64), TexManager.GetTexture("d01e02", -1), CRect_i_(0, 0, 128, 128), CRect_i_(0, static_cast<int>(cell_phase) * 64, 64, 64));
    }
}

void CFireOmni::SetTexture(CTexture *core_texture, CTexture *glow_texture) {
    core = core_texture;
    glow = glow_texture;
    texture_set = 1;
}

namespace {

// Per-layer GS state: no alpha test, depth tested but not written, additive Cs * As + Cd. The sprites
// set TEX1 themselves.
void SetLayerRegisters() {
    const draw2d::Services &services = draw2d::Get();
    sceGsTest               test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.zte = 1;
    services.set_test(&test);
    sceGsZbuf zbuffer = mgZBuffer;
    zbuffer.bits.zmsk = 1;
    services.set_zbuf(&zbuffer);
    sceGsAlpha alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    services.set_alpha(&alpha);
}

} // namespace

void CFireOmni::DrawFire(int /*unused0*/, int /*unused1*/, CCamera *camera, float * /*colour*/, float scale, int layers,
                         float camera_offset) {
    sceVu0FVECTOR      camera_direction;
    sceVu0FVECTOR      camera_ref;
    std::array<int, 4> near_top_left{};
    std::array<int, 4> near_bottom_right{};
    sceVu0FVECTOR      world;
    std::array<int, 4> top_left{};
    std::array<int, 4> bottom_right{};
    std::array<int, 4> top_right{};
    std::array<int, 4> bottom_left{};

    if (texture_set == 0) {
        if (core == nullptr) {
            core = TexManager.GetTexture(const_cast<char *>("lightling"), -1);
        }

        if (glow == nullptr) {
            glow = TexManager.GetTexture(const_cast<char *>("blender"), -1);
        }
    }

    camera->GetPos(camera_direction);
    camera->GetRef(camera_ref);
    sceVu0SubVector(camera_direction, camera_direction, camera_ref);
    sceVu0Normalize(camera_direction, camera_direction);
    sceVu0ScaleVector(camera_direction, camera_direction, camera_offset);
    spRGBA white = {.r = 0x80, .g = 0x80, .b = 0x80, .a = 0x80};

    if (layers & 1) {
        SetLayerRegisters();

        world[0] = pos[0];
        world[1] = pos[1] + 4.6f;
        world[2] = pos[2];
        world[3] = 1.0f;

        if (MGRotTransPers3DSprite(top_left.data(), bottom_right.data(), world, 18.0f * scale, 9.0f * scale, 1) == 1) {
            set3DSpriteFog(Vif1Packet, core, CRect_i_(0, 0, 64, 64), top_left.data(), bottom_right.data(), &white);
            set3DSpriteFog(Vif1Packet, glow, CRect_i_(2, 2, 124, 124), top_left.data(), bottom_right.data(), &white);
        }
    }

    if (layers & 2) {
        SetLayerRegisters();

        flicker_count++;
        srand(flicker_seed);

        if (flicker_count >= 5) {
            flicker_height = flicker_width = 1.0f + 0.1f * rand() / 2147483648.0f;
            flicker_count = 0;
        }

        world[0] = pos[0];
        world[1] = pos[1] + 4.6f;
        world[2] = pos[2];
        world[3] = 1.0f;

        if (MGRotTransPers3DSprite(top_left.data(), bottom_right.data(), world, 45.0f * flicker_width * scale,
                                   45.0f * flicker_height * scale / 2.0f, 1) == 1) {
            top_right = {bottom_right[0], top_left[1], top_left[2], top_left[3]};
            bottom_left = {top_left[0], bottom_right[1], bottom_right[2], bottom_right[3]};
            world[0] += camera_direction[0];
            world[1] += camera_direction[1];
            world[2] += camera_direction[2];

            if (MGRotTransPers3DSprite(near_top_left.data(), near_bottom_right.data(), world, 45.0f * scale + flicker_width,
                                       (45.0f * scale + flicker_height) / 2.0f, 0) == 1) {
                top_right[2] = top_left[2] = bottom_left[2] = bottom_right[2] = near_top_left[2];
            }

            set3DSpriteFog(Vif1Packet, core, CRect_i_(0, 0, 64, 64), top_left.data(), top_right.data(), bottom_left.data(), bottom_right.data(),
                           0x80);
        }
    }

    const draw2d::Services &services = draw2d::Get();
    services.set_test(nullptr);
    services.set_zbuf(nullptr);
    services.set_alpha(nullptr);
}

void CFireOmni::RasterStep() {
    float phase = raster_phase + ((2.0f * static_cast<float>(std::rand())) / 2.1474836e9f);
    raster_phase = phase;

    if (!(phase < 8.0f)) {
        raster_phase = 0.0f;
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

} // namespace dc::game
