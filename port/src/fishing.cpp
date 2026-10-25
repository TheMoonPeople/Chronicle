#include "fishing.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include <algorithm>
#include <iterator>
#include <vector>

#include "collision.hpp"
#include "draw2d_port.hpp"
#include "fish_port.hpp"
#include "framevu1.hpp"
#include "itemdata.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "runscript.hpp"
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

// The pairs of hook and float points held apart by a fixed length (retail's statics).
const int hook_link[3][2] = {
    {0, 1},
    {0, 2},
    {1, 2}
};
const int uki_link[6][2] = {
    {0, 1},
    {0, 2},
    {0, 3},
    {1, 2},
    {2, 3},
    {3, 1}
};

} // namespace

// The line is a GS line strip in which an XYZF3 write moves the pen without drawing: a segment
// exists only into a vertex sent with XYZF2. It becomes a line list of those segments.
PORT_OVERRIDE void FishLineDraw(int above_water) {
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

    if (UkiFrame != nullptr && draw) {
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

    if (HookFrame != nullptr && draw) {
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

        if (EsaFrame != nullptr) {
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

namespace {

// The baits the picker offers (_GOTO_CHANGE_ESA) and esa_info knows.
constexpr int kBaits[] = {ITEM_EVY, ITEM_MIMI, ITEM_PRICKLY, ITEM_THROBBING_CHERRY, ITEM_GOOEY_PEACH, ITEM_BOMB_NUTS,
                          ITEM_POISONOUS_APPLE, ITEM_MELLOW_BANANA, ITEM_CARROT, ITEM_POTATO_CAKE, ITEM_MINON,
                          ITEM_BATTAN, ITEM_PETITE_FISH};

// How many baits the towns' scripts list after the variable the picker writes to.
constexpr int kScriptBaitList = 8;

bool IsBait(int item) {
    return std::find(std::begin(kBaits), std::end(kBaits), item) != std::end(kBaits);
}

} // namespace

bool ListBaitForScript(RS_STACKDATA *choice, int bait) {
    if (choice == nullptr || !IsBait(bait)) {
        return false;
    }
    for (int i = 1; i <= kScriptBaitList; i++) {
        if (choice[i].type != RS_INT || !IsBait(choice[i].i) || choice[i].i == bait) {
            return false;
        }
    }
    choice[1].i = bait;
    return true;
}

PORT_OVERRIDE void FishLineStep(float *rod_position, float *unused) {
    sceVu0FVECTOR hook_from;
    sceVu0FVECTOR hook_to;
    sceVu0FVECTOR uki_from;
    sceVu0FVECTOR uki_to;
    sceVu0FVECTOR hit;
    sceVu0FVECTOR normal;
    sceVu0FVECTOR diff;
    sceVu0FVECTOR move0;
    sceVu0FVECTOR move1;
    float         water;
    float         weight;
    int           i;
    int           iteration;

    if (cpoly != NULL) {
        sceVu0CopyVector(hook_from, point[23]);
        sceVu0CopyVector(hook_to, point[23]);
        hook_from[1] += 40.0f;
        hook_to[1] -= 40.0f;
        int hit_no = CheckHit(cpoly, cpoly_num, hook_from, hook_to, hit, 1, 0);

        if (hit_no >= 0) {
            sceVu0Normalize(normal, cpoly[hit_no].normal);
            float slope = normal[1] < 0.0f ? -normal[1] : normal[1];

            if (slope > 0.2f) {
                HookGroundLevel = 0.2f + hit[1];
            }
        }

        sceVu0CopyVector(uki_from, point[18]);
        sceVu0CopyVector(uki_to, point[18]);
        uki_from[1] += 40.0f;
        uki_to[1] -= 40.0f;
        hit_no = CheckHit(cpoly, cpoly_num, uki_from, uki_to, hit, 1, 0);

        if (hit_no >= 0) {
            sceVu0Normalize(normal, cpoly[hit_no].normal);
            float slope = normal[1] < 0.0f ? -normal[1] : normal[1];

            if (slope > 0.2f) {
                UkiGroundLevel = 1.0f + hit[1];
            }
        }
    }

    if (UkiGroundLevel < HookGroundLevel) {
        LineGroundLevel = UkiGroundLevel;
    } else {
        LineGroundLevel = HookGroundLevel;
    }

    water = WaterLevel;

    sceVu0CopyVector(rod_top, rod_position);
    rod_top[3] = 1.0f;
    fishhook[3] = 1.0f;

    for (i = 0; i < 24; i++) {
        velo[i][1] += -0.1f;
        velo[i][3] = 0.0f;
        sceVu0AddVector(point[i], point[i], velo[i]);
    }

    weight = pull_hook;
    point[23][1] += weight;

    for (i = 0; i < 3; i++) {
        hookv[i][1] += -0.1f + weight;
        hookv[i][3] = 0.0f;
        sceVu0AddVector(hookp[i], hookp[i], hookv[i]);
    }

    for (i = 0; i < 4; i++) {
        ukiv[i][1] += -0.1f + weight;
        ukiv[i][3] = 0.0f;
        sceVu0AddVector(ukip[i], ukip[i], ukiv[i]);
    }

    for (iteration = 0; iteration < 16; iteration++) {
        sceVu0CopyVector(point[0], rod_top);
        sceVu0CopyVector(old_p[0], rod_top);

        for (i = 1; i < 24; i++) {
            weight = 0.5f;

            if (i == 23) {
                weight = 0.9f;
            }

            float  rest = 1.0f - weight;
            float *near = point[i - 1];
            float *far = point[i];
            sceVu0SubVector(diff, near, far);
            float length = DistVector(diff);
            float stretch = length - distp;
            float scale = rest * stretch / length;
            weight = weight * stretch / length;
            sceVu0ScaleVector(move0, diff, scale);
            sceVu0ScaleVector(move1, diff, weight);
            sceVu0SubVector(near, near, move0);
            sceVu0AddVector(far, far, move1);
        }

        sceVu0CopyVector(hookp[0], point[23]);

        for (i = 0; i < 3; i++) {
            sceVu0SubVector(diff, hookp[hook_link[i][0]], hookp[hook_link[i][1]]);
            float length = DistVector(diff);
            sceVu0ScaleVector(move0, diff, 0.5f * (length - hook_dist[i]) / length);
            sceVu0SubVector(hookp[hook_link[i][0]], hookp[hook_link[i][0]], move0);
            sceVu0AddVector(hookp[hook_link[i][1]], hookp[hook_link[i][1]], move0);
        }

        sceVu0SubVector(diff, point[18], ukip[0]);
        float length = DistVector(diff);
        // A pinned float sits on its line point: the EE's 0/0 left both still, the host's is NaN.
        if (length == 0.0f) {
            length = 1.0f;
        }
        weight = 0.2f;
        float scale = length * (1.0f - weight) / length;
        weight = length * weight / length;
        sceVu0ScaleVector(move0, diff, scale);
        sceVu0ScaleVector(move1, diff, weight);
        sceVu0SubVector(point[18], point[18], move0);
        sceVu0AddVector(ukip[0], ukip[0], move1);

        for (i = 0; i < 6; i++) {
            sceVu0SubVector(diff, ukip[uki_link[i][0]], ukip[uki_link[i][1]]);
            float length = DistVector(diff);
            sceVu0ScaleVector(move0, diff, 0.5f * (length - uki_dist[i]) / length);
            sceVu0SubVector(ukip[uki_link[i][0]], ukip[uki_link[i][0]], move0);
            sceVu0AddVector(ukip[uki_link[i][1]], ukip[uki_link[i][1]], move0);
        }

        if (set_hook_pos) {
            sceVu0CopyVector(point[23], fishhook);
            sceVu0CopyVector(hookp[0], fishhook);
        }

        if (set_uki_pos) {
            sceVu0CopyVector(point[18], uki);
            sceVu0CopyVector(ukip[0], uki);
        }
    }

    for (i = 0; i < 24; i++) {
        sceVu0SubVector(velo[i], point[i], old_p[i]);

        if (i != 23) {
            sceVu0ScaleVector(velo[i], velo[i], 0.96f);
        } else {
            sceVu0ScaleVector(velo[i], velo[i], 0.999f);
        }

        if (point[i][1] < LineGroundLevel) {
            point[i][1] = LineGroundLevel;
            velo[i][0] *= 0.4f;
            velo[i][1] *= -0.4f;
            velo[i][2] *= 0.4f;
        }

        if (point[i][1] < water) {
            sceVu0ScaleVector(velo[i], velo[i], 0.4f);

            if (i < 19) {
                velo[i][1] += 0.1f;
            }
        }

        sceVu0CopyVector(old_p[i], point[i]);
    }

    for (i = 0; i < 3; i++) {
        sceVu0SubVector(hookv[i], hookp[i], hookop[i]);

        if (hookp[i][1] < HookGroundLevel) {
            hookp[i][1] = HookGroundLevel;
            hookv[i][0] *= 0.8f;
            hookv[i][1] *= -0.6f;
            hookv[i][2] *= 0.8f;
        }

        sceVu0CopyVector(hookop[i], hookp[i]);
        sceVu0ScaleVector(hookv[i], hookv[i], 0.99f);
    }

    for (i = 0; i < 4; i++) {
        sceVu0SubVector(ukiv[i], ukip[i], ukiop[i]);

        if (ukip[i][1] < UkiGroundLevel) {
            ukip[i][1] = UkiGroundLevel;
            ukiv[i][0] *= 0.6f;
            ukiv[i][1] *= -0.8f;
            ukiv[i][2] *= 0.6f;
        }

        if (i != 0 && ukip[i][1] < water && ukip[0][1] < water) {
            sceVu0ScaleVector(ukiv[i], ukiv[i], 0.6f);
            ukiv[i][1] += 0.3f;
        }

        if (ukip[0][1] < water && !(ukip[i][1] < water)) {
            float height = ukip[0][1] - ukip[i][1];
            float depth = ukip[0][1] - ukip[i][1];
            height = depth < 0.0f ? -depth : height;
            ukiv[i][0] *= 0.6f;
            ukiv[i][2] *= 0.6f;

            if ((depth < 0.0f ? -depth : depth) > 0.001f) {
                ukiv[i][1] += 0.3f * (water - ukip[0][1]) / height;
            }
        }

        if (ukip[i][1] < water && !(ukip[0][1] < water)) {
            float height = ukip[i][1] - ukip[0][1];
            height = height < 0.0f ? -height : height;
            float depth = ukip[0][1] - ukip[i][1];

            if ((depth < 0.0f ? -depth : depth) > 0.001f) {
                ukiv[i][1] += 0.3f * (water - ukip[i][1]) / height;
            }
        }

        sceVu0CopyVector(ukiop[i], ukip[i]);
        sceVu0ScaleVector(ukiv[i], ukiv[i], 0.999f);
    }

    LineGroundLevel = HookGroundLevel = UkiGroundLevel = GroundLevel;
    set_uki_pos = 0;
    set_hook_pos = 0;
    pull_hook = 0.0f;
}
