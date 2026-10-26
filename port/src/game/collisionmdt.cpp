#include "stubs/collisionmdt.hpp"

#include "collisionmdt.hpp"

#include <libvu0.h>

#include "mathutil.hpp"
/* The bound of whatever the shape is made of, which the base can build because the vertices are
   the one thing every shape answers. A shape with no vertices leaves the bound all zero. */
void CCollision::CreateBBox() {
    int            i;
    int            vertex_count;
    sceVu0FVECTOR *vertex;

    vertex = GetVertexAddress(&vertex_count);

    if (vertex == 0) {
        max[0] = min[0] = 0.0f;
        max[1] = min[1] = 0.0f;
        max[2] = min[2] = 0.0f;
        return;
    }

    max[0] = vertex[0][0];
    max[1] = vertex[0][1];
    max[2] = vertex[0][2];
    min[0] = vertex[0][0];
    min[1] = vertex[0][1];
    min[2] = vertex[0][2];

    for (i = 0; i < vertex_count; i++) {
        if (max[0] < vertex[0][0]) {
            max[0] = vertex[0][0];
        }

        if (max[1] < vertex[0][1]) {
            max[1] = vertex[0][1];
        }

        if (max[2] < vertex[0][2]) {
            max[2] = vertex[0][2];
        }

        if (min[0] > vertex[0][0]) {
            min[0] = vertex[0][0];
        }

        if (min[1] > vertex[0][1]) {
            min[1] = vertex[0][1];
        }

        if (min[2] > vertex[0][2]) {
            min[2] = vertex[0][2];
        }

        vertex++;
    }
}

sceVu0FVECTOR *CCollision::GetVertexAddress(int *count) {
    return 0;
}

int CCollisionMDT::GetPolygon(int index, sceVu0FMATRIX v0, sceVu0FMATRIX v1, sceVu0FMATRIX v2) {
    MDT_COLLISION *collision;

    if (model == 0) {
        return 0;
    }

    collision = (MDT_COLLISION *) ((char *) model + model->mesh_ofs);

    if (index >= collision->set.num) {
        return 0;
    }

    return 1;
}

/* The height of the mesh under a point, as the highest triangle a vertical line meets. The line is
   spelled from y zero to y one and the intersection treats it as infinite, so nothing bounds the
   search in y. */
int CCollisionMDT::GetMaxY(float *position) {
    sceVu0FVECTOR  v0;
    sceVu0FVECTOR  v1;
    sceVu0FVECTOR  v2;
    sceVu0FVECTOR  from;
    sceVu0FVECTOR  to;
    sceVu0FVECTOR  hit;
    sceVu0FVECTOR  normal;
    sceVu0FVECTOR  tri_max;
    sceVu0FVECTOR  tri_min;
    int            i;
    int            found;
    sceVu0FVECTOR *vertex;
    MDT_CPOLY_SET *set;
    MDT_CPOLY     *poly;
    float          top;

    found = 0;

    if (model == 0) {
        return 0;
    }

    /* An integer floor, converted rather than written as a float constant: the game spells it that
       way and the conversion is in the instruction stream. */
    top = -100000000;

    from[0] = to[0] = position[0];
    from[2] = to[2] = position[2];
    from[1] = 0.0f;
    to[1] = 1.0f;

    vertex = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
    set = &((MDT_COLLISION *) ((char *) model + model->mesh_ofs))->set;
    poly = set->poly;

    for (i = 0; i < set->num; i++) {
        *(u_long128 *) v0 = *(u_long128 *) vertex[poly->vertex[0]];
        *(u_long128 *) v1 = *(u_long128 *) vertex[poly->vertex[1]];
        *(u_long128 *) v2 = *(u_long128 *) vertex[poly->vertex[2]];
        poly++;

        vu_maxmin3(tri_max, tri_min, v0, v1, v2);

        if (position[0] < tri_min[0] || position[0] > tri_max[0]) {
            continue;
        }

        if (position[2] < tri_min[2] || position[2] > tri_max[2]) {
            continue;
        }

        vu_normal(normal, v0, v1, v2);

        if (IntersectionPoint_line_poly3(from, to, v0, v1, v2, normal, hit) == 0) {
            continue;
        }

        found = 1;

        if (top < hit[1]) {
            top = hit[1];
        }
    }

    if (found != 0) {
        position[1] = top;
    }

    return found;
}

/* The nearest point at which a line meets the mesh. Each triangle is rejected first by its own
   bound and then by the plane it lies in, and only what survives both is intersected. */
int CCollisionMDT::Intersection(float *from, float *to, float *hit) {
    sceVu0FVECTOR  v0;
    sceVu0FVECTOR  v1;
    sceVu0FVECTOR  v2;
    sceVu0FVECTOR  point;
    sceVu0FVECTOR  edge0;
    sceVu0FVECTOR  edge1;
    sceVu0FVECTOR  normal;
    sceVu0FVECTOR  tri_max;
    sceVu0FVECTOR  tri_min;
    int            i;
    int            found;
    sceVu0FVECTOR *vertex;
    MDT_CPOLY_SET *set;
    MDT_CPOLY     *poly;
    float          plane_dist;
    float          from_dist;
    float          to_dist;
    float          best_distance;
    float          distance;
    float          dx;
    float          dy;
    float          dz;

    found = 0;

    vertex = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
    set = &((MDT_COLLISION *) ((char *) model + model->mesh_ofs))->set;
    poly = set->poly;

    for (i = 0; i < set->num; i++) {
        *(u_long128 *) v0 = *(u_long128 *) vertex[poly->vertex[0]];
        *(u_long128 *) v1 = *(u_long128 *) vertex[poly->vertex[1]];
        *(u_long128 *) v2 = *(u_long128 *) vertex[poly->vertex[2]];
        poly++;

        vu_maxmin3(tri_max, tri_min, v0, v1, v2);

        if (from[0] < tri_min[0] && to[0] < tri_min[0]) {
            continue;
        }

        if (from[0] > tri_max[0] && to[0] > tri_max[0]) {
            continue;
        }

        /* The far endpoint is held against the x extent in y and not the y one. That is what the
           game does and what its bytes depend on, so it is what this does; the effect is a reject
           that keeps more triangles than it needs to, and nothing below trusts it on its own. */
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

        plane_dist = sceVu0InnerProduct(normal, v0);
        from_dist = sceVu0InnerProduct(normal, from);
        to_dist = sceVu0InnerProduct(normal, to);

        if (plane_dist - from_dist > 0.0f && plane_dist - to_dist > 0.0f) {
            continue;
        }

        if (plane_dist - from_dist < 0.0f && plane_dist - to_dist < 0.0f) {
            continue;
        }

        if (IntersectionPoint_line_poly3(from, to, v0, v1, v2, normal, point) == 0) {
            continue;
        }

        dx = point[0] - from[0];
        dy = point[1] - from[1];
        dz = point[2] - from[2];
        distance = dx * dx + dy * dy + dz * dz;

        if (found == 0) {
            best_distance = distance;
            sceVu0CopyVector(hit, point);
            found = 1;
        } else {
            if (distance < best_distance) {
                best_distance = distance;
                sceVu0CopyVector(hit, point);
            }

            found = 1;
        }
    }

    return found;
}

sceVu0FVECTOR *CCollisionMDT::GetVertexAddress(int *count) {
    if (model == 0) {
        return 0;
    }

    *count = model->vertex_num;

    if (*count <= 0) {
        return 0;
    }

    return (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
}

/* Every triangle of the mesh whose own bound reaches a cube around the point. */
int CCollisionMDT::PickUpNearPoly(CCPoly *poly, float *position, float radius) {
    sceVu0FVECTOR  tri_max;
    sceVu0FVECTOR  tri_min;
    int            i;
    int            num;
    sceVu0FVECTOR *vertex;
    MDT_CPOLY_SET *set;
    MDT_CPOLY     *cpoly;
    CCPoly        *out;
    float          max_x;
    float          max_y;
    float          max_z;
    float          min_x;
    float          min_y;
    float          min_z;

    out = poly;
    num = 0;

    max_x = position[0] + radius;
    max_y = position[1] + radius;
    max_z = position[2] + radius;
    min_x = position[0] - radius;
    min_y = position[1] - radius;
    min_z = position[2] - radius;

    vertex = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
    set = &((MDT_COLLISION *) ((char *) model + model->mesh_ofs))->set;
    cpoly = set->poly;

    for (i = 0; i < set->num; i++) {
        *(u_long128 *) out->vertex[0] = *(u_long128 *) vertex[cpoly->vertex[0]];
        *(u_long128 *) out->vertex[1] = *(u_long128 *) vertex[cpoly->vertex[1]];
        *(u_long128 *) out->vertex[2] = *(u_long128 *) vertex[cpoly->vertex[2]];
        cpoly++;

        vu_maxmin3(tri_max, tri_min, out->vertex[0], out->vertex[1], out->vertex[2]);

        if (min_x > tri_max[0] || min_y > tri_max[1] || min_z > tri_max[2]) {
            continue;
        }

        if (max_x < tri_min[0] || max_y < tri_min[1] || max_z < tri_min[2]) {
            continue;
        }

        vu_normal(out->normal, out->vertex[0], out->vertex[1], out->vertex[2]);
        out++;
        num++;
    }

    return num;
}

/* Every built triangle whose bound overlaps the one the caller gives, tested two bounds at a time
   on the unit and read back through its sticky flags rather than as a value. */
int CCollisionMDT::PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box) {
    CBoxVu0    bound;
    int        i;
    int        num;
    CCPolyBox *box_poly;

    if (box.min[0] > max[0]) {
        return 0;
    }

    if (box.min[1] > max[1]) {
        return 0;
    }

    if (box.min[2] > max[2]) {
        return 0;
    }

    if (box.max[0] < min[0]) {
        return 0;
    }

    if (box.max[1] < min[1]) {
        return 0;
    }

    if (box.max[2] < min[2]) {
        return 0;
    }

    num = 0;

    bound.max[0] = box.max[0];
    bound.max[1] = box.max[1];
    bound.max[2] = box.max[2];
    bound.min[0] = box.min[0];
    bound.min[1] = box.min[1];
    bound.min[2] = box.min[2];

    vu_hold_box(bound.max, bound.min);

    box_poly = mesh;

    for (i = 0; i < this->mesh_count; i++, box_poly++) {
        if (vu_box_missed(box_poly->box.max, box_poly->box.min) == 0) {
            *(u_long128 *) poly->vertex[0] = *(u_long128 *) box_poly->poly.vertex[0];
            *(u_long128 *) poly->vertex[1] = *(u_long128 *) box_poly->poly.vertex[1];
            *(u_long128 *) poly->vertex[2] = *(u_long128 *) box_poly->poly.vertex[2];
            poly->info = box_poly->poly.info;
            *(u_long128 *) poly->normal = *(u_long128 *) box_poly->poly.normal;
            poly++;
            num++;
        }
    }

    return num;
}

/* The whole mesh, with no test at all. */
int CCollisionMDT::PickUpNearPoly(CCPoly *poly) {
    int            i;
    int            num;
    sceVu0FVECTOR *vertex;
    MDT_CPOLY_SET *set;
    MDT_CPOLY     *cpoly;

    num = 0;

    vertex = (sceVu0FVECTOR *) ((char *) model + model->vertex_ofs);
    set = &((MDT_COLLISION *) ((char *) model + model->mesh_ofs))->set;
    cpoly = set->poly;

    for (i = 0; i < set->num; i++) {
        *(u_long128 *) poly->vertex[0] = *(u_long128 *) vertex[cpoly->vertex[0]];
        *(u_long128 *) poly->vertex[1] = *(u_long128 *) vertex[cpoly->vertex[1]];
        *(u_long128 *) poly->vertex[2] = *(u_long128 *) vertex[cpoly->vertex[2]];
        cpoly++;

        vu_normal(poly->normal, poly->vertex[0], poly->vertex[1], poly->vertex[2]);
        poly++;
        num++;
    }

    return num;
}
