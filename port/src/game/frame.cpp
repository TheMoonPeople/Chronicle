#include "frame.hpp"

#include <libdma.h>
#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>

#include "boxvu0.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "render/draw3d.hpp"
#include "renderinfo.hpp"
#include "visualvu1.hpp"
#include "vu0_ops.hpp"

void *operator new(size_t size, u_long128 *block) {
    return dc::game::PlaceDataAllocObject(size, block);
}

void *operator new[](size_t size, u_long128 *block) {
    return dc::game::PlaceDataAllocObject(size, block);
}

namespace dc::game {

using namespace dc::render;

void DevInit() {
    sceDmaEnv env;

    sceDmaReset(1);
    sceDmaGetEnv(&env);
    env.notify = 0x100;
    sceDmaPutEnv(&env);
    sceGsResetPath();
}

namespace {

void MulFrameMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX m2) {
    float product[4][4];
    for (int i = 0; i < 4; i++) {
        vu0::Apply(product[i], m1, m2[i]);
    }
    for (int i = 0; i < 4; i++) {
        vu0::Copy(m0[i], product[i]);
    }
}

void ScaleMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR scale) {
    const float factor[3] = {scale[0], scale[1], scale[2]};
    float       scaled[4][4];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            scaled[i][j] = m1[i][j] * factor[i];
        }
    }
    vu0::Copy(scaled[3], m1[3]);
    for (int i = 0; i < 4; i++) {
        vu0::Copy(m0[i], scaled[i]);
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
        float inverse_depth = vu0::Div(1.0f, std::fabs(corner[i][2]));
        corner[i][0] = vu0::Mul(corner[i][0], inverse_depth);
        corner[i][1] = vu0::Mul(corner[i][1], inverse_depth);
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

// The quaternion is stored scalar first: w, x, y, z.
void QuatToMat(const float *quaternion, sceVu0FMATRIX matrix) {
    const float x2 = quaternion[1] + quaternion[1];
    const float y2 = quaternion[2] + quaternion[2];
    const float z2 = quaternion[3] + quaternion[3];

    const float xx = quaternion[1] * x2;
    const float xy = quaternion[1] * y2;
    const float xz = quaternion[1] * z2;
    const float yy = quaternion[2] * y2;
    const float yz = quaternion[2] * z2;
    const float zz = quaternion[3] * z2;
    const float wx = quaternion[0] * x2;
    const float wy = quaternion[0] * y2;
    const float wz = quaternion[0] * z2;

    matrix[0][0] = 1.0f - (yy + zz);
    matrix[1][0] = xy - wz;
    matrix[2][0] = xz + wy;
    matrix[3][0] = 0.0f;

    matrix[0][1] = xy + wz;
    matrix[1][1] = 1.0f - (xx + zz);
    matrix[2][1] = yz - wx;
    matrix[3][1] = 0.0f;

    matrix[0][2] = xz - wy;
    matrix[1][2] = yz + wx;
    matrix[2][2] = 1.0f - (xx + yy);
    matrix[3][2] = 0.0f;

    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
    matrix[3][3] = 1.0f;
}

std::size_t NameStemLength(const char *name) {
    std::size_t length = 0;

    while (name[length] != '\0' && (name[length] != '_' || name[length + 1] != '_')) {
        length++;
    }

    return length;
}

// Only the stem before a "__" identifies a frame; the flags after it are ignored.
int StrCmp(const char *left, const char *right) {
    const std::string_view left_stem(left, NameStemLength(left));
    const std::string_view right_stem(right, NameStemLength(right));

    return left_stem == right_stem ? 1 : 0;
}

} // namespace

void CFrameAttr::Initialize() {
    clip_depth = 0.0f;
    far_clip_enable = program_option = clip_enable = remake_pending = unk_09 = fog_enable = false;
    far_clip = 0.0f;
    use_color = false;
    color[0] = color[1] = color[2] = color[3] = 128.0f;
    alpha_ref = -1;
    draw_on = 1;
    depth_write = true;
    ignore_depth = false;
    eye_relative = false;
    billboard = 0;
    blend_mode = 0;
    cull_enable = true;
    ambient_boost = false;
    unk_40[0] = unk_40[2] = unk_40[3] = 0.0f;
    unk_40[1] = 1.0f;
}

CFrameAttr::CFrameAttr() {
    Initialize();
}

void CFrame::Initialize() {
    world_valid = false;
    no_rotation = true;

    parent = child = brother = elder = nullptr;

    scale[0] = scale[1] = scale[2] = scale[3] = 1.0f;
    position[0] = position[1] = position[2] = position[3] = 0.0f;
    rotation[0] = rotation[1] = rotation[2] = rotation[3] = 0.0f;

    max[0] = max[1] = max[2] = 0.0f;
    min[0] = min[1] = min[2] = 0.0f;

    sceVu0UnitMatrix(world);
    sceVu0UnitMatrix(inverse);
    sceVu0UnitMatrix(local);

    for (auto &point : corner) {
        point[0] = 0.0f;
        point[1] = 0.0f;
        point[2] = 0.0f;
        point[3] = 1.0f;
    }

    attr.Initialize();

    flags = 1;
    collision = nullptr;
    name[0] = '\0';
    rot_type = 0;
    srt = false;
    reference = false;
}

void CFrame::SetPosition(float x, float y, float z) {
    srt = true;

    if (position[0] == x && position[1] == y && position[2] == z) {
        return;
    }

    position[0] = x;
    position[1] = y;
    position[2] = z;
    world_valid = false;
}

void CFrame::SetPosition(float *position) {
    srt = true;

    if (this->position[0] == position[0] && this->position[1] == position[1] && this->position[2] == position[2]) {
        return;
    }

    this->position[0] = position[0];
    this->position[1] = position[1];
    this->position[2] = position[2];
    world_valid = false;
}

void CFrame::SetScale(float x, float y, float z) {
    if (scale[0] == x && scale[1] == y && scale[2] == z) {
        return;
    }

    scale[0] = x;
    scale[1] = y;
    scale[2] = z;
    world_valid = false;
}

void CFrame::SetScale(float *scale) {
    if (this->scale[0] == scale[0] && this->scale[1] == scale[1] && this->scale[2] == scale[2]) {
        return;
    }

    this->scale[0] = scale[0];
    this->scale[1] = scale[1];
    this->scale[2] = scale[2];
    world_valid = false;
}

int CFrame::GetFrameNum() {
    int num = 1;

    for (CFrame *frame = child; frame != nullptr; frame = frame->brother) {
        num += frame->GetFrameNum();
    }

    return num;
}

void CFrame::SetParent(CFrame *parent) {
    if (this->parent != nullptr) {
        return;
    }

    this->parent = parent;

    if (parent == nullptr) {
        return;
    }

    parent->SetChild(this);
}

void CFrame::SetBrother(CFrame *brother) {
    if (brother == nullptr) {
        return;
    }

    if (this->brother != nullptr) {
        this->brother->SetBrother(brother);
        return;
    }

    this->brother = brother;
    this->brother->elder = this;
}

void CFrame::SetChild(CFrame *child) {
    if (child == nullptr) {
        this->child = nullptr;
        return;
    }

    if (this->child != nullptr) {
        this->child->SetBrother(child);
        return;
    }

    this->child = child;
    this->child->parent = this;
}

void CFrame::SetReference(CFrame *reference) {
    if (reference == nullptr) {
        return;
    }

    parent = reference;
    this->reference = true;
}

void CFrame::DeleteReference() {
    parent = nullptr;
    reference = false;
}

// Rebuilding the world matrix invalidates the children's caches, which were composed with it.
void CFrame::GetLWMatrix(sceVu0FMATRIX matrix) {
    sceVu0FMATRIX parent_world;
    sceVu0FMATRIX local_matrix;
    sceVu0FVECTOR translation;

    // Nothing on a referenced frame's side clears this cache when it moves.
    if (reference) {
        world_valid = false;
    }

    if (world_valid) {
        if (parent == nullptr) {
            sceVu0CopyMatrix(matrix, world);
            return;
        }

        for (CFrame *ancestor = parent; ancestor->world_valid;) {
            ancestor = ancestor->parent;

            if (ancestor == nullptr) {
                sceVu0CopyMatrix(matrix, world);
                return;
            }
        }
    }

    for (CFrame *sibling = child; sibling != nullptr; sibling = sibling->brother) {
        sibling->world_valid = false;
    }

    if (srt) {
        ScaleMatrix(local_matrix, this->local, scale);

        // Rotation type 2 turns about the frame's own origin, so the translation is removed around it.
        if (rot_type & 2) {
            sceVu0CopyVector(translation, local_matrix[3]);
            local_matrix[3][0] = local_matrix[3][1] = local_matrix[3][2] = 0.0f;
        }

        if (rot_type & 1) {
            if (rotation[0] != 0.0f) {
                sceVu0RotMatrixX(local_matrix, local_matrix, rotation[0]);
            }

            if (rotation[1] != 0.0f) {
                sceVu0RotMatrixY(local_matrix, local_matrix, rotation[1]);
            }

            if (rotation[2] != 0.0f) {
                sceVu0RotMatrixZ(local_matrix, local_matrix, rotation[2]);
            }
        }

        if (rot_type & 2) {
            sceVu0AddVector(local_matrix[3], translation, position);
        } else {
            sceVu0AddVector(local_matrix[3], local_matrix[3], position);
        }
    } else {
        sceVu0CopyMatrix(local_matrix, this->local);
    }

    if (parent == nullptr) {
        sceVu0CopyMatrix(world, local_matrix);
        sceVu0CopyMatrix(matrix, world);
        world_valid = true;
        return;
    }

    // A billboarded parent writes its own world matrix while drawing.
    if (parent->attr.billboard) {
        sceVu0CopyMatrix(parent_world, parent->world);
    } else {
        parent->GetLWMatrix(parent_world);
    }

    MulFrameMatrix(world, parent_world, local_matrix);
    sceVu0CopyMatrix(matrix, world);
    world_valid = true;
}

float (*CFrame::GetInverseMatrix())[4] {
    sceVu0FMATRIX world_matrix;

    GetLWMatrix(world_matrix);
    sceVu0InversMatrix(inverse, world_matrix);

    return inverse;
}

void CFrame::SetTransMatrix(sceVu0FMATRIX matrix) {
    sceVu0CopyMatrix(local, matrix);
    world_valid = false;
}

void CFrame::SetTransMatrix(float *quaternion) {
    sceVu0FVECTOR translation;

    world_valid = false;
    std::ranges::copy(local[3], translation);
    QuatToMat(quaternion, local);
    std::ranges::copy(translation, local[3]);
}

int FrameNameComp(char *left, char *right) {
    return StrCmp(left, right);
}

CFrame *CFrame::SearchFrame(char *name) {
    if (StrCmp(this->name, name) != 0) {
        return this;
    }

    for (CFrame *frame = child; frame != nullptr; frame = frame->brother) {
        CFrame *found = frame->SearchFrame(name);

        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

// With a parent, the bound is the eight corners run through the local transform.
void CFrame::GetBoundBox(CBoxVu0 *box, int children) {
    sceVu0FVECTOR low;
    sceVu0FVECTOR high;
    sceVu0FVECTOR point;
    CBoxVu0       child_box;

    low[0] = min[0];
    low[1] = min[1];
    low[2] = min[2];
    low[3] = 1.0f;

    high[0] = max[0];
    high[1] = max[1];
    high[2] = max[2];
    high[3] = 1.0f;

    if (parent == nullptr) {
        sceVu0CopyVector(box->min, low);
        sceVu0CopyVector(box->max, high);
    } else {
        sceVu0ApplyMatrix(box->max, local, corner[0]);
        sceVu0CopyVector(box->min, box->max);

        for (int i = 1; i < 8; i++) {
            sceVu0ApplyMatrix(point, local, corner[i]);
            VectorMaxMin(box->max, box->min, point, box->max, box->min);
        }
    }

    if (children == 0) {
        return;
    }

    for (CFrame *frame = child; frame != nullptr; frame = frame->brother) {
        frame->GetBoundBox(&child_box, 1);
        VectorMaxMin(box->max, box->min, box->max, box->min, child_box.max, child_box.min);
    }
}

void CFrame::ScaleBoundBox(float *scale) {
    sceVu0FVECTOR center;
    sceVu0FVECTOR dmax;
    sceVu0FVECTOR dmin;
    sceVu0FVECTOR new_max;
    sceVu0FVECTOR new_min;

    new_max[0] = this->max[0];
    new_max[1] = this->max[1];
    new_max[2] = this->max[2];

    new_min[0] = this->min[0];
    new_min[1] = this->min[1];
    new_min[2] = this->min[2];

    sceVu0AddVector(center, new_max, new_min);
    sceVu0ScaleVector(center, center, 0.5f);
    sceVu0SubVector(dmax, new_max, center);
    sceVu0SubVector(dmin, new_min, center);

    dmax[0] *= scale[0];
    dmax[1] *= scale[1];
    dmax[2] *= scale[2];

    dmin[0] *= scale[0];
    dmin[1] *= scale[1];
    dmin[2] *= scale[2];

    sceVu0AddVector(new_max, dmax, center);
    sceVu0AddVector(new_min, dmin, center);

    this->max[0] = new_max[0];
    this->max[1] = new_max[1];
    this->max[2] = new_max[2];

    this->min[0] = new_min[0];
    this->min[1] = new_min[1];
    this->min[2] = new_min[2];

    const std::array<const float *, 2> bound = {this->min, this->max};

    for (int i = 0; i < 8; i++) {
        this->corner[i][3] = 1.0f;
        this->corner[i][0] = bound[(i & 1) != 0][0];
        this->corner[i][1] = bound[(i & 2) != 0][1];
        this->corner[i][2] = bound[(i & 4) != 0][2];
    }
}

void CFrame::SetAttr(CFrameAttr &attr, int children, int mask) {
    if (mask == 0) {
        this->attr = attr;
    } else {
        if (mask & 0x1) {
            this->attr.draw_on = attr.draw_on;
        }

        if (mask & 0x2) {
            this->attr.clip_depth = attr.clip_depth;
        }

        if (mask & 0x4) {
            this->attr.clip_enable = attr.clip_enable;
        }

        if (mask & 0x8) {
            this->attr.unk_09 = attr.unk_09;
        }

        if (mask & 0x10) {
            this->attr.remake_pending = attr.remake_pending;
        }

        if (mask & 0x20) {
            this->attr.program_option = attr.program_option;
        }

        if (mask & 0x40) {
            this->attr.fog_enable = attr.fog_enable;
        }

        if (mask & 0x80) {
            this->attr.far_clip_enable = attr.far_clip_enable;
        }

        if (mask & 0x100) {
            this->attr.far_clip = attr.far_clip;
        }

        if (mask & 0x200) {
            this->attr.use_color = attr.use_color;
        }

        if (mask & 0x400) {
            std::ranges::copy(attr.color, this->attr.color);
        }

        if (mask & 0x800) {
            this->attr.alpha_ref = attr.alpha_ref;
        }

        if (mask & 0x1000) {
            this->attr.depth_write = attr.depth_write;
        }

        if (mask & 0x2000) {
            this->attr.eye_relative = attr.eye_relative;
        }

        if (mask & 0x4000) {
            this->attr.billboard = attr.billboard;
        }

        if (mask & 0x8000) {
            this->attr.blend_mode = attr.blend_mode;
        }

        if (mask & 0x10000) {
            this->attr.ignore_depth = attr.ignore_depth;
        }
    }

    if (children == 0) {
        return;
    }

    for (CFrame *frame = child; frame != nullptr; frame = frame->brother) {
        frame->SetAttr(attr, 1, mask);
    }
}

void CFrame::GetWorldPosition(float *world_point, float *local_point) {
    sceVu0FMATRIX matrix;

    local_point[3] = 1.0f;
    GetLWMatrix(matrix);
    sceVu0ApplyMatrix(world_point, matrix, local_point);
}

void CFrame::SetRotation(float x, float y, float z) {
    srt = true;
    no_rotation = false;
    rotation[3] = 0.0f;

    if (rotation[0] == x && rotation[1] == y && rotation[2] == z) {
        return;
    }

    world_valid = false;
    rot_type |= 1;
    rotation[0] = x;
    rotation[1] = y;
    rotation[2] = z;
}

void CFrame::GetRotation(float *rotation) {
    if (no_rotation != 0) {
        rotation[0] = rotation[1] = rotation[2] = rotation[3] = 0.0f;
        return;
    }

    sceVu0CopyVector(rotation, this->rotation);
}

void CFrame::SetRotType(int type) {
    rot_type = type;

    if (type & 0x2) {
        rot_type |= 1;
    }
}

// The copy keeps the transform and attributes but none of the original's place in the hierarchy.
CFrame &CFrame::operator=(CFrame &other) {
    std::memcpy(static_cast<void *>(this), static_cast<const void *>(&other), sizeof(CFrame));

    parent = child = brother = nullptr;
    world_valid = false;
    no_rotation = false;

    return *this;
}

namespace {

// Planar projection along light 0 onto the plane through point with the given normal. A plane
// through the origin is pushed a tenth of a unit back along the normal so its offset is non-zero.
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

// Bound of the frame's corners and of the same corners flattened onto the shadow plane, in eye
// space.
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

// Culls the frame, applies its attributes to the current GS register shadows the visual's draw
// reads, then walks the hierarchy. The screen-bound test runs on an unsqueezed view, so its
// vertical limit is the frame's full half-height, and its limits are what the target shows (past
// a 4:3 window, the sides).
int CFrameVu1::DrawVu1(unsigned int *packet, RenderInfo *info) {
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
    bool          visible = true;
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

int CFrameVu1::DrawVu1(sceVif1Packet * /*packet*/, RenderInfo *info) {
    return DrawVu1(static_cast<unsigned int *>(nullptr), info);
}

void CFrame::SetCollision(CCollision *collision) {
    this->collision = collision;
}

int CFrame::DrawVu1(unsigned int * /*packet*/, RenderInfo * /*info*/) {
    return 0;
}

int CFrame::DrawVu1(sceVif1Packet * /*packet*/, RenderInfo * /*info*/) {
    return 0;
}

CFrame::CFrame() {
    CFrame::Initialize();
}

void CFrameVu1::SetVisual(CVisualVu1 *visual) {
    this->visual = visual;
}

CVisualVu1 *CFrameVu1::GetVisual() {
    return visual;
}

void CFrameVu1::Initialize() {
    CFrame::Initialize();
    visual = nullptr;
}

CFrameVu1::CFrameVu1() {
    CFrameVu1::Initialize();
}

CFrameVu1 &CFrameVu1::operator=(CFrameVu1 &other) {
    CFrame::operator=(other);
    visual = other.visual;

    return *this;
}

namespace {

// pre_trance_normal parks its matrix in VU0 registers for the trance_normal calls that follow.
float g_pick_matrix[4][4];

void pre_trance_normal(sceVu0FMATRIX matrix) {
    std::memcpy(g_pick_matrix, matrix, sizeof(g_pick_matrix));
}

// All three corners are loaded from p0 onwards (the caller's corners are contiguous) but stored
// through p0, p1 and p2. The plane's w is a lane the cross product never writes; it is zeroed.
void trance_normal(float *p0, float *p1, float *p2, float *plane) {
    const std::span<float, 12> corners(p0, 12);
    float                      a[4];
    float                      b[4];
    float                      c[4];
    vu0::Apply(a, g_pick_matrix, corners.subspan<0, 4>().data());
    vu0::Apply(b, g_pick_matrix, corners.subspan<4, 4>().data());
    vu0::Apply(c, g_pick_matrix, corners.subspan<8, 4>().data());

    float edge0[4] = {b[0] - a[0], b[1] - a[1], b[2] - a[2], b[3] - a[3]};
    float edge1[4] = {c[0] - a[0], c[1] - a[1], c[2] - a[2], c[3] - a[3]};
    float normal[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vu0::Cross(normal, edge0, edge1);

    vu0::Copy(p0, a);
    vu0::Copy(p1, b);
    vu0::Copy(p2, c);
    vu0::Copy(plane, normal);
}

} // namespace

// The world-space box is carried into each frame's local space, so collision is searched untransformed
// and only the polygons found are transformed back to world space.
int CFrame::PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box) {
    sceVu0FVECTOR box_corner[8];
    sceVu0FVECTOR local_corner[8];
    sceVu0FVECTOR max0;
    sceVu0FVECTOR min0;
    sceVu0FVECTOR max1;
    sceVu0FVECTOR min1;
    sceVu0FVECTOR extent[2];
    sceVu0FMATRIX world_matrix;
    CBoxVu0       bound;
    int           num = 0;

    if (flags == 4) {
        return 0;
    }

    if (collision != nullptr && (flags & 1) != 0) {
        GetLWMatrix(world_matrix);
        float (*inverse_matrix)[4] = GetInverseMatrix();

        std::ranges::copy(box.min, extent[0]);
        std::ranges::copy(box.max, extent[1]);

        for (int i = 0; i < 8; i++) {
            box_corner[i][3] = 1.0f;
            box_corner[i][0] = extent[(i & 1) != 0][0];
            box_corner[i][1] = extent[(i & 2) != 0][1];
            box_corner[i][2] = extent[(i & 4) != 0][2];
        }

        ApplyMatrixN(local_corner, inverse_matrix, box_corner, 8);
        VectorMaxMin(max0, min0, local_corner[0], local_corner[1], local_corner[2], local_corner[3]);
        VectorMaxMin(max1, min1, local_corner[4], local_corner[5], local_corner[6], local_corner[7]);
        VectorMaxMin(bound.max, bound.min, max0, max1, min0, min1);

        num = collision->PickUpNearPoly(poly, bound);

        pre_trance_normal(world_matrix);

        for (int i = 0; i < num; i++) {
            trance_normal(poly[i].vertex[0], poly[i].vertex[1], poly[i].vertex[2], poly[i].normal);
        }
    }

    if ((flags & 2) == 0 && (flags & 4) == 0) {
        for (CFrame *frame = child; frame != nullptr; frame = frame->brother) {
            num += frame->PickUpNearPoly(&poly[num], box);
        }
    }

    return num;
}

} // namespace dc::game
