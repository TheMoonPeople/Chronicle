#include "collisionmdt.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cstring>
#include <span>

#include "mathutil.hpp"
#include "vu0_ops.hpp"

namespace dc::game {

namespace {

// vu_hold_box parks the query box in VU0 registers for the vu_box_missed calls that follow.
float g_held_max[4];
float g_held_min[4];

} // namespace

void vu_maxmin3(float *max, float *min, float *a, float *b, float *c) {
    float hi[4];
    float lo[4];
    vu0::Max(hi, a, b);
    vu0::Min(lo, a, b);
    vu0::Max(hi, hi, c);
    vu0::Min(lo, lo, c);
    vu0::Copy(max, hi);
    vu0::Copy(min, lo);
}

// Retail stores w from a register vopmsub never writes; nothing reads it, so it is zeroed.
void vu_normal(float *normal, float *a, float *b, float *c) {
    float edge_ab[4] = {b[0] - a[0], b[1] - a[1], b[2] - a[2], 0.0f};
    float edge_ac[4] = {c[0] - a[0], c[1] - a[1], c[2] - a[2], 0.0f};
    float cross[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vu0::Cross(cross, edge_ab, edge_ac);
    vu0::Copy(normal, cross);
}

void vu_hold_box(float *max, float *min) {
    vu0::Copy(g_held_max, max);
    vu0::Copy(g_held_min, min);
}

// Retail answers with the sticky zero and sign flags, so boxes that only touch count as a miss.
int vu_box_missed(float *max, float *min) {
    int status = 0;
    for (int i = 0; i < 3; i++) {
        status |= vu0::StatusFlags(g_held_max[i] - min[i]);
    }
    for (int i = 0; i < 3; i++) {
        status |= vu0::StatusFlags(max[i] - g_held_min[i]);
    }
    return status & (vu0::kStickyZero | vu0::kStickySign);
}

namespace {

constexpr float kNoFloor = -100000000.0f;

sceVu0FVECTOR *VertexTable(MDT_HEADER *model) {
    return reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<char *>(model) + model->vertex_ofs);
}

MDT_COLLISION *CollisionSection(MDT_HEADER *model) {
    return reinterpret_cast<MDT_COLLISION *>(reinterpret_cast<char *>(model) + model->mesh_ofs);
}

std::span<const MDT_CPOLY> CollisionTriangles(MDT_HEADER *model) {
    const MDT_CPOLY_SET &set = CollisionSection(model)->set;
    return {set.poly, set.num};
}

void CopyVector(sceVu0FVECTOR destination, const sceVu0FVECTOR source) {
    std::memcpy(destination, source, sizeof(sceVu0FVECTOR));
}

} // namespace

// The bound of the shape's vertices; a shape with no vertices leaves the bound all zero.
void CCollision::CreateBBox() {
    int            vertex_count = 0;
    sceVu0FVECTOR *vertex = GetVertexAddress(&vertex_count);

    if (vertex == nullptr) {
        for (int axis = 0; axis < 3; axis++) {
            max[axis] = min[axis] = 0.0f;
        }
        return;
    }

    for (int axis = 0; axis < 3; axis++) {
        max[axis] = vertex[0][axis];
        min[axis] = vertex[0][axis];
    }

    for (int i = 0; i < vertex_count; i++) {
        for (int axis = 0; axis < 3; axis++) {
            max[axis] = std::max(max[axis], vertex[i][axis]);
            min[axis] = std::min(min[axis], vertex[i][axis]);
        }
    }
}

sceVu0FVECTOR *CCollision::GetVertexAddress(int * /*count*/) {
    return nullptr;
}

int CCollisionMDT::GetPolygon(int index, sceVu0FMATRIX /*v0*/, sceVu0FMATRIX /*v1*/, sceVu0FMATRIX /*v2*/) {
    if (model == nullptr) {
        return 0;
    }

    // Unsigned comparison: a negative index is out of range.
    if (static_cast<unsigned int>(index) >= CollisionSection(model)->set.num) {
        return 0;
    }

    return 1;
}

// The height of the highest triangle under a point. The vertical line is treated as infinite, so
// nothing bounds the search in y.
int CCollisionMDT::GetMaxY(float *position) {
    if (model == nullptr) {
        return 0;
    }

    sceVu0FVECTOR from;
    sceVu0FVECTOR to;
    from[0] = to[0] = position[0];
    from[2] = to[2] = position[2];
    from[1] = 0.0f;
    to[1] = 1.0f;

    bool                 found = false;
    float                top = kNoFloor;
    const sceVu0FVECTOR *vertex = VertexTable(model);

    for (const MDT_CPOLY &cpoly : CollisionTriangles(model)) {
        sceVu0FVECTOR v0;
        sceVu0FVECTOR v1;
        sceVu0FVECTOR v2;
        sceVu0FVECTOR tri_max;
        sceVu0FVECTOR tri_min;
        CopyVector(v0, vertex[cpoly.vertex[0]]);
        CopyVector(v1, vertex[cpoly.vertex[1]]);
        CopyVector(v2, vertex[cpoly.vertex[2]]);

        vu_maxmin3(tri_max, tri_min, v0, v1, v2);

        if (position[0] < tri_min[0] || position[0] > tri_max[0]) {
            continue;
        }

        if (position[2] < tri_min[2] || position[2] > tri_max[2]) {
            continue;
        }

        sceVu0FVECTOR normal;
        sceVu0FVECTOR hit;
        vu_normal(normal, v0, v1, v2);

        if (IntersectionPoint_line_poly3(from, to, v0, v1, v2, normal, hit) == 0) {
            continue;
        }

        found = true;
        top = std::max(top, hit[1]);
    }

    if (found) {
        position[1] = top;
    }

    return found ? 1 : 0;
}

// The nearest point at which a line meets the mesh. Each triangle is rejected first by its own
// bound and then by its plane, and only what survives both is intersected.
int CCollisionMDT::Intersection(float *from, float *to, float *hit) {
    bool                 found = false;
    float                best_distance = 0.0f;
    const sceVu0FVECTOR *vertex = VertexTable(model);

    for (const MDT_CPOLY &cpoly : CollisionTriangles(model)) {
        sceVu0FVECTOR v0;
        sceVu0FVECTOR v1;
        sceVu0FVECTOR v2;
        sceVu0FVECTOR tri_max;
        sceVu0FVECTOR tri_min;
        CopyVector(v0, vertex[cpoly.vertex[0]]);
        CopyVector(v1, vertex[cpoly.vertex[1]]);
        CopyVector(v2, vertex[cpoly.vertex[2]]);

        vu_maxmin3(tri_max, tri_min, v0, v1, v2);

        if (from[0] < tri_min[0] && to[0] < tri_min[0]) {
            continue;
        }

        if (from[0] > tri_max[0] && to[0] > tri_max[0]) {
            continue;
        }

        // The far endpoint is held against the x extent rather than the y one. This only loosens
        // the reject; the plane test below still discards what it lets through.
        if (from[1] < tri_min[1] && to[1] < tri_min[0]) {
            continue;
        }

        if (from[1] > tri_max[1] && to[1] > tri_max[0]) {
            continue;
        }

        if (from[2] < tri_min[2] && to[2] < tri_min[2]) {
            continue;
        }

        if (from[2] > tri_max[2] && to[2] > tri_max[2]) {
            continue;
        }

        sceVu0FVECTOR edge0;
        sceVu0FVECTOR edge1;
        sceVu0FVECTOR normal;
        edge0[0] = v1[0] - v0[0];
        edge0[1] = v1[1] - v0[1];
        edge0[2] = v1[2] - v0[2];
        edge0[3] = 0.0f;
        edge1[0] = v2[0] - v0[0];
        edge1[1] = v2[1] - v0[1];
        edge1[2] = v2[2] - v0[2];
        edge1[3] = 0.0f;

        sceVu0OuterProduct(normal, edge0, edge1);
        normal[3] = 0.0f;

        const float plane_dist = sceVu0InnerProduct(normal, v0);
        const float from_dist = sceVu0InnerProduct(normal, from);
        const float to_dist = sceVu0InnerProduct(normal, to);

        if (plane_dist - from_dist > 0.0f && plane_dist - to_dist > 0.0f) {
            continue;
        }

        if (plane_dist - from_dist < 0.0f && plane_dist - to_dist < 0.0f) {
            continue;
        }

        sceVu0FVECTOR point;

        if (IntersectionPoint_line_poly3(from, to, v0, v1, v2, normal, point) == 0) {
            continue;
        }

        const float dx = point[0] - from[0];
        const float dy = point[1] - from[1];
        const float dz = point[2] - from[2];
        const float distance = dx * dx + dy * dy + dz * dz;

        if (!found || distance < best_distance) {
            best_distance = distance;
            sceVu0CopyVector(hit, point);
        }

        found = true;
    }

    return found ? 1 : 0;
}

sceVu0FVECTOR *CCollisionMDT::GetVertexAddress(int *count) {
    if (model == nullptr) {
        return nullptr;
    }

    *count = model->vertex_num;

    if (*count <= 0) {
        return nullptr;
    }

    return VertexTable(model);
}

// Every triangle of the mesh whose own bound reaches a cube around the point.
int CCollisionMDT::PickUpNearPoly(CCPoly *poly, float *position, float radius) {
    const float max_x = position[0] + radius;
    const float max_y = position[1] + radius;
    const float max_z = position[2] + radius;
    const float min_x = position[0] - radius;
    const float min_y = position[1] - radius;
    const float min_z = position[2] - radius;

    int                  num = 0;
    const sceVu0FVECTOR *vertex = VertexTable(model);

    // A rejected triangle is still written into the next output slot, which the next one reuses.
    for (const MDT_CPOLY &cpoly : CollisionTriangles(model)) {
        CCPoly &out = poly[num];
        CopyVector(out.vertex[0], vertex[cpoly.vertex[0]]);
        CopyVector(out.vertex[1], vertex[cpoly.vertex[1]]);
        CopyVector(out.vertex[2], vertex[cpoly.vertex[2]]);

        sceVu0FVECTOR tri_max;
        sceVu0FVECTOR tri_min;
        vu_maxmin3(tri_max, tri_min, out.vertex[0], out.vertex[1], out.vertex[2]);

        if (min_x > tri_max[0] || min_y > tri_max[1] || min_z > tri_max[2]) {
            continue;
        }

        if (max_x < tri_min[0] || max_y < tri_min[1] || max_z < tri_min[2]) {
            continue;
        }

        vu_normal(out.normal, out.vertex[0], out.vertex[1], out.vertex[2]);
        num++;
    }

    return num;
}

// Every built triangle whose bound overlaps the given one, tested on the unit two bounds at a time
// and read back through its sticky flags rather than as a value.
int CCollisionMDT::PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box) {
    for (int axis = 0; axis < 3; axis++) {
        if (box.min[axis] > max[axis]) {
            return 0;
        }
    }

    for (int axis = 0; axis < 3; axis++) {
        if (box.max[axis] < min[axis]) {
            return 0;
        }
    }

    CBoxVu0 bound{};
    for (int axis = 0; axis < 3; axis++) {
        bound.max[axis] = box.max[axis];
        bound.min[axis] = box.min[axis];
    }

    vu_hold_box(bound.max, bound.min);

    int num = 0;

    for (int i = 0; i < mesh_count; i++) {
        CCPolyBox &box_poly = mesh[i];

        if (vu_box_missed(box_poly.box.max, box_poly.box.min) != 0) {
            continue;
        }

        CCPoly &out = poly[num];
        CopyVector(out.vertex[0], box_poly.poly.vertex[0]);
        CopyVector(out.vertex[1], box_poly.poly.vertex[1]);
        CopyVector(out.vertex[2], box_poly.poly.vertex[2]);
        out.info = box_poly.poly.info;
        CopyVector(out.normal, box_poly.poly.normal);
        num++;
    }

    return num;
}

// The whole mesh, with no test at all.
int CCollisionMDT::PickUpNearPoly(CCPoly *poly) {
    int                  num = 0;
    const sceVu0FVECTOR *vertex = VertexTable(model);

    for (const MDT_CPOLY &cpoly : CollisionTriangles(model)) {
        CCPoly &out = poly[num];
        CopyVector(out.vertex[0], vertex[cpoly.vertex[0]]);
        CopyVector(out.vertex[1], vertex[cpoly.vertex[1]]);
        CopyVector(out.vertex[2], vertex[cpoly.vertex[2]]);

        vu_normal(out.normal, out.vertex[0], out.vertex[1], out.vertex[2]);
        num++;
    }

    return num;
}

} // namespace dc::game
