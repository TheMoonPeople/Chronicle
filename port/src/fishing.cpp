#include "fishing.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include <vector>

#include "draw2d_port.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "texture.hpp"

namespace {

// What one XYZF2/XYZF3 write leaves in the GS vertex queue: retail packs the screen ints into a
// 64-bit word unmasked, and the GS takes 16 bits of x and y and 24 of z from it.
gfx::Vertex2D LineVertex(const int *screen) {
    const u_long word = static_cast<u_long>(static_cast<long>(screen[0])) |
                        (static_cast<u_long>(static_cast<long>(screen[1])) << 16) |
                        (static_cast<u_long>(static_cast<long>(screen[2])) << 32);
    return draw2d::Vertex(MGPortLogicalX(static_cast<int>(word & 0xFFFF)),
                          MGPortLogicalY(static_cast<int>((word >> 16) & 0xFFFF)),
                          MGPortDepth(static_cast<unsigned>((word >> 32) & 0xFFFFFF)), 0.0f, 0.0f, 0x80, 0x80,
                          0x80, 0x80);
}

} // namespace

// The line is a GS line strip in which an XYZF3 write moves the pen without drawing: a segment
// exists only into a vertex sent with XYZF2. It becomes a line list of those segments.
PC_OVERRIDE void FishLineDraw(int above_water) {
    int           screen[4] = {};
    sceVu0FVECTOR center;
    sceVu0FVECTOR direction;
    sceVu0FMATRIX matrix;
    sceVu0FVECTOR uki_top;
    sceVu0FVECTOR hook_top;
    sceVu0FVECTOR esa_position;
    int           draw;
    int           visible;
    int           prev_visible;
    int           i;

    TexManager.ReloadTexture(GetVif1Packet(), fishing_texb);
    prev_visible = 0;

    const draw2d::Services &services = draw2d::Get();
    sceGsTest               test = mgPixelTest;
    test.bits.aref = 0;
    services.set_test(&test);

    std::vector<gfx::Vertex2D> segments;
    gfx::Vertex2D              pen = {};

    for (i = 0; i < 24; i++) {
        visible = MGRotTransPers(screen, point[i], 0);
        u_char above = point[i][1] > WaterLevel;
        bool   kick = true;

        if (visible == 0 || prev_visible == 0) {
            kick = false;
        }

        if (above_water) {
            if (!above) {
                kick = false;
            }
        } else if (above || draw_under_water == 0) {
            kick = false;
        }

        gfx::Vertex2D vertex = LineVertex(screen);
        if (kick) {
            segments.push_back(pen);
            segments.push_back(vertex);
        }
        pen = vertex;
        prev_visible = visible;
    }

    if (!segments.empty()) {
        gfx::DrawState state = services.draw_state();
        state.blend = true;
        state.fog = false;
        state.cull = gfx::CullMode::None;
        draw2d::DrawUntextured(gfx::Primitive::Lines, segments, state);
    }

    services.set_test(nullptr);

    draw = true;
    VectorMax(uki_top, ukip[0], ukip[1], ukip[2], ukip[3]);

    if (above_water) {
        if (uki_top[1] < WaterLevel) {
            draw = false;
        }
    } else if (uki_top[1] > WaterLevel || draw_under_water == 0) {
        draw = false;
    }

    if (UkiFrame != NULL && draw) {
        sceVu0AddVector(center, ukip[1], ukip[2]);
        sceVu0AddVector(center, center, ukip[3]);
        sceVu0ScaleVector(center, center, 0.333333f);
        sceVu0SubVector(direction, ukip[0], center);
        sceVu0Normalize(matrix[1], direction);
        matrix[1][3] = 0.0f;
        sceVu0SubVector(direction, center, ukip[1]);
        sceVu0Normalize(matrix[2], direction);
        sceVu0OuterProduct(matrix[0], matrix[1], matrix[2]);
        matrix[0][3] = 0.0f;
        matrix[3][0] = matrix[3][1] = matrix[3][2] = 0.0f;
        matrix[3][3] = 1.0f;
        UkiFrameTop.Initialize();
        UkiFrameTop.SetTransMatrix(matrix);
        UkiFrameTop.SetPosition(ukip[0]);
        UkiFrame->SetReference(&UkiFrameTop);
        MGDraw(UkiFrame);
    }

    draw = true;
    VectorMax(hook_top, hookp[0], hookp[1], hookp[2]);

    if (above_water) {
        if (hook_top[1] < WaterLevel) {
            draw = false;
        }
    } else if (hook_top[1] > WaterLevel || draw_under_water == 0) {
        draw = false;
    }

    if (HookFrame != NULL && draw) {
        sceVu0AddVector(center, hookp[1], hookp[2]);
        sceVu0ScaleVector(center, center, 0.5f);
        sceVu0SubVector(direction, hookp[0], center);
        sceVu0Normalize(matrix[1], direction);
        matrix[1][3] = 0.0f;
        sceVu0SubVector(center, hookp[1], hookp[0]);
        sceVu0SubVector(direction, hookp[2], hookp[0]);
        sceVu0OuterProduct(matrix[2], center, direction);
        sceVu0Normalize(matrix[2], matrix[2]);
        matrix[2][3] = 0.0f;
        sceVu0OuterProduct(matrix[0], matrix[1], matrix[2]);
        matrix[0][3] = 0.0f;
        matrix[3][0] = matrix[3][1] = matrix[3][2] = 0.0f;
        matrix[3][3] = 1.0f;
        HookFrame->SetTransMatrix(matrix);
        HookFrame->SetPosition(hookp[0]);
        MGDraw(HookFrame);

        if (EsaFrame != NULL) {
            sceVu0AddVector(esa_position, hookp[0], hookp[1]);
            sceVu0AddVector(esa_position, esa_position, hookp[2]);
            sceVu0ScaleVector(esa_position, esa_position, 0.3333f);
            EsaFrame->SetTransMatrix(matrix);
            EsaFrame->SetPosition(esa_position);
            TexManager.ReloadTexture(GetVif1Packet(), esa_texb);
            MGDraw(EsaFrame);
        }
    }
}
