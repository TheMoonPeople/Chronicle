#include "stubs/frame.hpp"

#include <libdma.h>
#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string_view>

#include "collision.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "visualvu1.hpp"

namespace {

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

void DevInit() {
    sceDmaEnv env;

    sceDmaReset(1);
    sceDmaGetEnv(&env);
    env.notify = 0x100;
    sceDmaPutEnv(&env);
    sceGsResetPath();
}

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
