#include "frame.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include <cmath>
#include <cstddef>
#include <cstring>

#include "boxvu0.hpp"
#include "dataalloc.hpp"
#include "draw3d.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "stubs/frame.hpp"
#include "visualvu1.hpp"
#include "vu0_ops.hpp"

void *PlaceDataAllocObject(std::size_t size, u_long128 *block);

namespace {

// frame.cpp's static ShadowMatrix: the planar projection along light 0 onto the plane through
// point with the given normal. A plane through the origin is pushed a tenth of a unit back along
// the normal, matching the game's chosen plane while avoiding a division by its offset.
void ShadowMatrix(sceVu0FMATRIX matrix, sceVu0FMATRIX light, float *point, float *normal) {
    sceVu0FVECTOR direction = {light[0][0], light[1][0], light[2][0], 0.0f};
    sceVu0FVECTOR plane;
    sceVu0FVECTOR axis;
    sceVu0CopyVector(plane, point);
    sceVu0CopyVector(axis, normal);

    float t = sceVu0InnerProduct(axis, plane);
    if (t == 0.0f) {
        plane[0] -= 0.1f * axis[0];
        plane[1] -= 0.1f * axis[1];
        plane[2] -= 0.1f * axis[2];
        t = sceVu0InnerProduct(axis, plane);
    }
    sceVu0Normalize(direction, direction);
    float nx = direction[0];
    float ny = direction[1];
    float nz = direction[2];
    float depth = axis[0] * nx + axis[1] * ny + axis[2] * nz;
    float axis_length = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    if (!std::isfinite(depth) || !std::isfinite(t) || !std::isfinite(axis_length) ||
        std::fabs(depth) <= 1e-3f * axis_length) {
        sceVu0UnitMatrix(matrix);
        return;
    }
    float inverse = 1.0f / depth;

    matrix[0][0] = 1.0f - nx * axis[0] * inverse;
    matrix[1][0] = -nx * axis[1] * inverse;
    matrix[2][0] = -nx * axis[2] * inverse;
    matrix[3][0] = nx * t * inverse;
    matrix[0][1] = -ny * axis[0] * inverse;
    matrix[1][1] = 1.0f - ny * axis[1] * inverse;
    matrix[2][1] = -ny * axis[2] * inverse;
    matrix[3][1] = ny * t * inverse;
    matrix[0][2] = -nz * axis[0] * inverse;
    matrix[1][2] = -nz * axis[1] * inverse;
    matrix[2][2] = 1.0f - nz * axis[2] * inverse;
    matrix[3][2] = nz * t * inverse;
    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
    matrix[3][3] = 1.0f;
}

// frame.cpp's static ShadowClipBox: the bound of the frame's corners and of the same corners
// flattened onto the shadow plane, in eye space.
void ShadowClipBox(sceVu0FVECTOR *screen, sceVu0FVECTOR *corner, sceVu0FMATRIX matrix, RenderInfo *info) {
    sceVu0FMATRIX shadow;
    sceVu0FVECTOR flat[8];
    sceVu0FVECTOR world[8];
    sceVu0FVECTOR extent[2];
    sceVu0FVECTOR max[4];
    sceVu0FVECTOR min[4];

    ApplyMatrixN(world, matrix, corner, 8);
    MulFrameMatrix(shadow, info->shadow, matrix);
    ApplyMatrixN(flat, shadow, corner, 8);
    VectorMaxMin(max[0], min[0], world[0], world[1], world[2], world[3]);
    VectorMaxMin(max[1], min[1], world[4], world[5], world[6], world[7]);
    VectorMaxMin(max[2], min[2], flat[0], flat[1], flat[2], flat[3]);
    VectorMaxMin(max[3], min[3], flat[4], flat[5], flat[6], flat[7]);
    VectorMax(extent[0], max[0], max[1], max[2], max[3]);
    VectorMin(extent[1], min[0], min[1], min[2], min[3]);
    for (int i = 0; i < 8; i++) {
        world[i][3] = 1.0f;
        world[i][0] = extent[(i & 1) != 0][0];
        world[i][1] = extent[(i & 2) != 0][1];
        world[i][2] = extent[(i & 4) != 0][2];
    }
    ApplyMatrixN(screen, info->view_scaled, world, 8);
}

} // namespace

Draw3DIdentityScope::Draw3DIdentityScope(const void *object, float teleport_distance, bool blend_vertices)
    : key(gfx::CurrentInterpKey()), no_interpolation(gfx::CurrentNoInterpolation()) {
    gfx::SetInterpKey(reinterpret_cast<uintptr_t>(object), false, teleport_distance, blend_vertices);
}

Draw3DIdentityScope::~Draw3DIdentityScope() {
    gfx::SetInterpKey(key, no_interpolation);
}

// Retail's culling, attributes and hierarchy walk, with the GS registers SetGsReg3 sent becoming
// the current register shadows the visual's draw reads. The screen-bound test runs on an
// unsqueezed view, so its vertical limits are the frame's full half-height (retail's quarter on
// the squeezed field), and its limits are what the target shows: past a 4:3 window, the sides.
PORT_OVERRIDE int CFrameVu1::DrawVu1(unsigned int *packet, RenderInfo *info) {
    sceVu0FMATRIX matrix;
    sceVu0FMATRIX screen_matrix;
    sceVu0FMATRIX turn;
    sceVu0FMATRIX light;
    sceVu0FVECTOR ambient;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR world_position;
    sceVu0FVECTOR origin = {0.0f, 0.0f, 0.0f, 1.0f};
    sceVu0FVECTOR screen_corner[8];
    sceVu0FMATRIX world_matrix;
    sceVu0FVECTOR screen_max;
    sceVu0FVECTOR screen_min;
    sceVu0FVECTOR color;
    int           near_clip = 0;
    int           far_clip = 0;
    int           visible = true;
    sceGsTest     test = mgPixelTest;
    sceGsZbuf     zbuf = mgZBuffer;
    sceGsAlpha    alpha;

    if (attr.billboard) {
        world_valid = false;
        GetWorldPosition(world_position, origin);
        GetLWMatrix(world_matrix);
        float axis_x = DistVector(world_matrix[0]);
        float axis_y = DistVector(world_matrix[1]);
        float axis_z = DistVector(world_matrix[2]);

        if ((attr.billboard & 2) && !(attr.billboard & 1)) {
            sceVu0CopyMatrix(matrix, mgUnitMatrix);
            sceVu0SubVector(matrix[2], info->view_position, world_position);
            matrix[2][1] = 0.0f;
            matrix[2][3] = 0.0f;
            sceVu0Normalize(matrix[2], matrix[2]);
            matrix[0][0] = matrix[2][2];
            matrix[0][2] = -matrix[2][0];
        }

        if ((attr.billboard & 2) && (attr.billboard & 1)) {
            sceVu0UnitMatrix(turn);
            sceVu0SubVector(direction, info->view_position, world_position);
            sceVu0Normalize(direction, direction);
            float height = direction[1];
            direction[1] = 0.0f;
            direction[3] = 0.0f;
            float flat = DistVector(direction);
            turn[1][1] = flat;
            turn[1][2] = -height;
            turn[2][1] = height;
            turn[2][2] = flat;

            sceVu0CopyMatrix(matrix, mgUnitMatrix);
            sceVu0SubVector(matrix[2], info->view_position, world_position);
            matrix[2][1] = 0.0f;
            matrix[2][3] = 0.0f;
            sceVu0Normalize(matrix[2], matrix[2]);
            matrix[0][0] = matrix[2][2];
            matrix[0][2] = -matrix[2][0];
            MulFrameMatrix(matrix, matrix, turn);
        }

        sceVu0ScaleVector(matrix[0], matrix[0], axis_x);
        sceVu0ScaleVector(matrix[1], matrix[1], axis_y);
        sceVu0ScaleVector(matrix[2], matrix[2], axis_z);
        matrix[3][0] = world_position[0];
        matrix[3][1] = world_position[1];
        matrix[3][2] = world_position[2];
        CopyMatrix(this->world, matrix);

        for (CFrame *sibling = this->child; sibling; sibling = sibling->brother) {
            sibling->world_valid = false;
        }
        world_valid = true;
    } else {
        GetLWMatrix(matrix);
    }

    if (!(attr.draw_on & 1) && (attr.draw_on & 2)) {
        return 0;
    }

    if (info->shadow_pass) {
        ShadowMatrix(info->shadow, info->light_direction, info->shadow_point, info->shadow_normal);
    }

    if (visual && attr.draw_on && attr.cull_enable) {
        MulFrameMatrix(screen_matrix, info->view_scaled, matrix);
        float inv_scale = 1.0f / info->scale[0];

        if (info->shadow_pass) {
            ShadowClipBox(screen_corner, this->corner, matrix, info);
        } else {
            ApplyMatrixN(screen_corner, screen_matrix, this->corner, 8);
        }
        ScreenBound(screen_corner, screen_max, screen_min);

        float        near_z = info->near[2];
        float        far_z = info->frame_far_z;
        float        guard = attr.remake_pending ? 2.0f : 1.0f;
        Draw3DExtent extent = Draw3DVisibleExtent();
        float        half_width = guard * extent.half_width * inv_scale;
        float        half_height = guard * extent.half_height * inv_scale;
        float        depth = 0.96f * (2048.0f * inv_scale);

        visible = false;
        if (screen_min[0] <= half_width && screen_max[0] >= -half_width && screen_min[1] <= half_height &&
            screen_max[1] >= -half_height && screen_max[2] >= near_z) {
            if (screen_min[2] < far_z) {
                near_clip = 8;
            }
            if (screen_min[2] > near_z && screen_max[0] < depth && screen_min[0] > -depth &&
                screen_max[1] < 2.0f * depth && screen_min[1] > -2.0f * depth) {
                far_clip = 8;
            }
            visible = true;
        }
    }

    info->frame_far_z = attr.clip_depth;
    info->scissor = (near_clip && far_clip < 8) && (attr.clip_enable || info->scissoring);
    info->fog_enabled = attr.fog_enable;
    info->eye_in_model = attr.eye_relative;
    info->clip_flags = 0;
    if (far_clip < 8 || attr.remake_pending) {
        info->clip_flags |= 1;
    }
    if (attr.remake_pending) {
        visible = true;
    }
    if (info->scissor) {
        info->clip_flags |= 2;
    }
    if (attr.program_option) {
        info->clip_flags |= 4;
    }

    if (visual && visible && attr.draw_on) {
        if (!info->shadow_pass) {
            if (attr.alpha_ref >= 0) {
                test.bits.aref = attr.alpha_ref;
            }
            if (attr.ignore_depth) {
                test.bits.zte = 1;
                test.bits.ztst = 1;
            }
            zbuf.bits.zmsk = !attr.depth_write;
            alpha = mgAlpha;
            if (attr.blend_mode > 0) {
                alpha.bits.a = 0;
                alpha.bits.b = 2;
                alpha.bits.c = 0;
                alpha.bits.d = 1;
            }
            if (attr.blend_mode < 0) {
                alpha.bits.a = 2;
                alpha.bits.b = 0;
                alpha.bits.c = 0;
                alpha.bits.d = 1;
            }
            MGPortRegisters &current = MGPortCurrent();
            current.test = test;
            current.zbuf = zbuf;
            current.alpha = alpha;
        }

        if (attr.use_color || attr.ambient_boost) {
            sceVu0CopyMatrix(light, info->light_direction);
            ZeroMatrix(info->light_direction);
            sceVu0CopyVector(ambient, info->ambient);
            if (attr.ambient_boost) {
                color[0] = info->light_color[0][0];
                color[1] = info->light_color[0][1];
                color[2] = info->light_color[0][2];
                info->ambient[0] += 0.3f * color[0];
                info->ambient[1] += 0.3f * color[1];
                info->ambient[2] += 0.3f * color[2];
            } else {
                info->ambient[0] = attr.color[0];
                info->ambient[1] = attr.color[1];
                info->ambient[2] = attr.color[2];
            }
        }

        if (attr.remake_pending) {
            visual->RemakeData(nullptr);
        }
        attr.remake_pending = 0;

        {
            Draw3DIdentityScope identity(this, kDraw3DTeleportDistance);
            visual->DrawVu1(packet, matrix, info, VU1_PROGRAM_UNKNOWN6, nullptr, 0, 0);
        }

        if (attr.use_color || attr.ambient_boost) {
            sceVu0CopyMatrix(info->light_direction, light);
            sceVu0CopyVector(info->ambient, ambient);
        }

        if (!info->shadow_pass) {
            MGPortRestoreRegisters();
        }
    }

    if (!(attr.draw_on & 2)) {
        for (CFrame *frame = this->child; frame; frame = frame->brother) {
            if (frame->attr.draw_on & 4) {
                continue;
            }
            frame->DrawVu1(packet, info);
        }
    }

    return 0;
}

PORT_OVERRIDE int CFrameVu1::DrawVu1(sceVif1Packet *, RenderInfo *info) {
    return DrawVu1(static_cast<unsigned int *>(nullptr), info);
}

namespace {

// pre_trance_normal parks its matrix in VU0 registers for the trance_normal calls that follow.
float g_pick_matrix[4][4];

} // namespace

void MulFrameMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX m2) {
    float r[4][4];
    for (int i = 0; i < 4; i++) {
        vu0::Apply(r[i], m1, m2[i]);
    }
    for (int i = 0; i < 4; i++) {
        vu0::Copy(m0[i], r[i]);
    }
}

void ScaleMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR scale) {
    float s[3] = {scale[0], scale[1], scale[2]};
    float r[4][4];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            r[i][j] = m1[i][j] * s[i];
        }
    }
    vu0::Copy(r[3], m1[3]);
    for (int i = 0; i < 4; i++) {
        vu0::Copy(m0[i], r[i]);
    }
}

void CopyMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1) {
    std::memmove(m0, m1, sizeof(float) * 16);
}

void ZeroMatrix(sceVu0FMATRIX m0) {
    std::memset(m0, 0, sizeof(float) * 16);
}

void ScreenBound(sceVu0FVECTOR *screen, sceVu0FVECTOR max, sceVu0FVECTOR min) {
    float corner[8][4];
    for (int i = 0; i < 8; i++) {
        vu0::Copy(corner[i], screen[i]);
        float q = vu0::Div(1.0f, std::fabs(corner[i][2]));
        corner[i][0] = vu0::Mul(corner[i][0], q);
        corner[i][1] = vu0::Mul(corner[i][1], q);
    }

    float hi[4];
    float lo[4];
    vu0::Max(hi, corner[0], corner[1]);
    vu0::Min(lo, corner[0], corner[1]);
    for (int i = 2; i < 8; i++) {
        vu0::Max(hi, hi, corner[i]);
        vu0::Min(lo, lo, corner[i]);
    }
    vu0::Copy(max, hi);
    vu0::Copy(min, lo);
}

void pre_trance_normal(sceVu0FMATRIX matrix) {
    std::memcpy(g_pick_matrix, matrix, sizeof(g_pick_matrix));
}

// Retail loads all three corners from p0 onwards (the caller's corners are contiguous) but stores
// them through p0, p1 and p2. The plane's w is a lane vopmsub never writes; it is zeroed.
void trance_normal(float *p0, float *p1, float *p2, float *plane) {
    float a[4];
    float b[4];
    float c[4];
    vu0::Apply(a, g_pick_matrix, p0);
    vu0::Apply(b, g_pick_matrix, p0 + 4);
    vu0::Apply(c, g_pick_matrix, p0 + 8);

    float e0[4] = {b[0] - a[0], b[1] - a[1], b[2] - a[2], b[3] - a[3]};
    float e1[4] = {c[0] - a[0], c[1] - a[1], c[2] - a[2], c[3] - a[3]};
    float n[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vu0::Cross(n, e0, e1);

    vu0::Copy(p0, a);
    vu0::Copy(p1, b);
    vu0::Copy(p2, c);
    vu0::Copy(plane, n);
}

PORT_OVERRIDE void *operator new(size_t size, u_long128 *block) {
    return PlaceDataAllocObject(size, block);
}

PORT_OVERRIDE void *operator new[](size_t size, u_long128 *block) {
    return PlaceDataAllocObject(size, block);
}
