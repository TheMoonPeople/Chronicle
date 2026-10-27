#include "fireomni.hpp"

#include <libgraph.h>

#include <array>
#include <cstdlib>

#include "camera.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "render/draw2d.hpp"
#include "snd.hpp"
#include "texture.hpp"

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
