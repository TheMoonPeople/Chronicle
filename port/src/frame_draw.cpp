#include <libgraph.h>
#include <libvu0.h>

#include "boxvu0.hpp"
#include "draw3d.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "stubs/frame.hpp"
#include "visualvu1.hpp"

namespace {

// frame.cpp's static ShadowMatrix: the planar projection along light 0 onto the plane through
// point with the given normal. The plane is carried as d with x . d == 1 on it, which fails for a
// plane through the origin, so such a point is pushed a tenth of a unit back along the normal.
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
    t = 1.0f / t;
    float dx = axis[0] * t;
    float dy = axis[1] * t;
    float dz = axis[2] * t;

    sceVu0Normalize(direction, direction);
    float nx = direction[0];
    float ny = direction[1];
    float nz = direction[2];
    float depth = dx * nx + dy * ny + dz * nz;
    float scale = -1.0f / depth;

    matrix[0][0] = scale * (dx * nx - depth);
    matrix[1][0] = scale * (dy * nx);
    matrix[2][0] = scale * (dz * nx);
    matrix[3][0] = scale * -nx;
    matrix[0][1] = scale * (dx * ny);
    matrix[1][1] = scale * (dy * ny - depth);
    matrix[2][1] = scale * (dz * ny);
    matrix[3][1] = scale * -ny;
    matrix[0][2] = scale * (dx * nz);
    matrix[1][2] = scale * (dy * nz);
    matrix[2][2] = scale * (dz * nz - depth);
    matrix[3][2] = scale * -nz;
    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
    matrix[3][3] = scale * -depth;
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

unsigned int g_cursor[64];

} // namespace

Draw3DIdentityScope::Draw3DIdentityScope(const void *object, float teleport_distance, bool blend_vertices)
    : key_(gfx::CurrentInterpKey()), no_interpolation_(gfx::CurrentNoInterpolation()) {
    gfx::SetInterpKey(reinterpret_cast<uintptr_t>(object), false, teleport_distance, blend_vertices);
}

Draw3DIdentityScope::~Draw3DIdentityScope() {
    gfx::SetInterpKey(key_, no_interpolation_);
}

// Retail's culling, attributes and hierarchy walk, with the GS registers SetGsReg3 sent becoming
// the current register shadows the visual's draw reads. The screen-bound test runs on an
// unsqueezed view, so its vertical limits are the frame's full half-height (retail's quarter on
// the squeezed field), and its limits are what the target shows: past a 4:3 window, the sides.
PC_OVERRIDE int CFrameVu1::DrawVu1(unsigned int *packet, RenderInfo *info) {
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

        float near_z = info->near[2];
        float far_z = info->frame_far_z;
        float        guard = attr.remake_pending ? 2.0f : 1.0f;
        Draw3DExtent extent = Draw3DVisibleExtent();
        float        half_width = guard * extent.half_width * inv_scale;
        float        half_height = guard * extent.half_height * inv_scale;
        float depth = 0.96f * (2048.0f * inv_scale);

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
            visual->RemakeData(0);
        }
        attr.remake_pending = 0;

        {
            Draw3DIdentityScope identity(this, kDraw3DTeleportDistance);
            visual->DrawVu1(packet, matrix, info, VU1_PROGRAM_UNKNOWN6, 0, 0, 0);
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

PC_OVERRIDE int CFrameVu1::DrawVu1(sceVif1Packet *packet, RenderInfo *info) {
    return DrawVu1(g_cursor, info);
}
