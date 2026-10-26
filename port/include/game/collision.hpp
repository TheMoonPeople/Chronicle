#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * What a collision polygon's area does, as CCPolyInfo::area_kind holds it.
 */
// clang-format off
enum AreaKind {
    AREA_KIND_NONE        = 0,  /**< Nothing. */
    AREA_KIND_FADE_OUT    = 2,  /**< Fades the character out. */
    AREA_KIND_AMBIENT_0   = 3,  /**< Selects ground ambient set 0. */
    AREA_KIND_AMBIENT_1   = 4,  /**< Selects ground ambient set 1. */
    AREA_KIND_HIGH_CAMERA = 10, /**< Holds the battle camera higher. */
};

// clang-format on

struct MDT_HEADER;

/**
 * Holds the maximum and minimum corners of an axis-aligned bound.
 */
class CBoxVu0 {
public:
    sceVu0FVECTOR max; /**< Greater extent on each axis. */
    sceVu0FVECTOR min; /**< Lesser extent on each axis. */
};

/**
 * Stores the opaque metadata associated with a collision triangle.
 */
class CCPolyInfo {
public:
    float unk_00;
    float unk_04;
    float unk_08;
    float unk_0c;
};

/**
 * Stores a collision triangle, its plane normal, and its associated metadata.
 */
class CCPoly {
public:
    sceVu0FVECTOR vertex[3]; /**< Corners of the triangle. */
    sceVu0FVECTOR normal;    /**< Normal of the triangle's plane. */

    union {
        CCPolyInfo info; /**< Surface metadata as the collision data stores it, copied whole. */

        struct {
            s16 ground_kind; /**< What the surface is made of. */
            s16 foot_sound;  /**< Sound the character's feet play on it. */
            s16 area_kind;   /**< Kind of area the surface marks. @see AreaKind. */
            s16 ignore_mask; /**< Collision query modes that pass through the surface. */
            u8  unk_48[8];
        } attr;
    };
} __attribute__((aligned(16)));

/**
 * Pairs a collision triangle with its axis-aligned bounding box.
 */
class CCPolyBox {
public:
    CCPoly  poly; /**< Collision triangle. */
    CBoxVu0 box;  /**< Axis-aligned bounds of the triangle. */
};

/**
 * Provides the common query interface and bounds for collision geometry.
 */
class CCollision {
public:
    sceVu0FVECTOR max; /**< Greater corner of the geometry's bounds. */
    sceVu0FVECTOR min; /**< Lesser corner of the geometry's bounds. */

    virtual int            GetPolygon(int index, sceVu0FMATRIX v0, sceVu0FMATRIX v1, sceVu0FMATRIX v2);
    virtual int            GetMaxY(float *position);
    virtual sceVu0FVECTOR *GetVertexAddress(int *count);
    virtual int            Intersection(float *from, float *to, float *hit);
    virtual int            PickUpNearPoly(CCPoly *poly, float *position, float radius);
    virtual int            PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box);
    virtual int            PickUpNearPoly(CCPoly *poly);
    virtual void           Initialize();

    void CreateBBox();
};

/**
 * Implements collision queries over geometry stored in an MDT resource.
 */
class CCollisionMDT : public CCollision {
public:
    MDT_HEADER *model;      /**< MDT resource the collision geometry is read from. */
    CCPolyBox  *mesh;       /**< Bounded polygons the collision tests against. */
    int         mesh_count; /**< Number of bounded polygons in mesh. */

    /* The same three fields Initialize clears, cleared again here because construction cannot
       reach a virtual of its own class. */
    CCollisionMDT() {
        model = 0;
        mesh = 0;
        mesh_count = 0;
    }

    virtual int            GetPolygon(int index, sceVu0FMATRIX v0, sceVu0FMATRIX v1, sceVu0FMATRIX v2);
    virtual int            GetMaxY(float *position);
    virtual sceVu0FVECTOR *GetVertexAddress(int *count);
    virtual int            Intersection(float *from, float *to, float *hit);
    virtual int            PickUpNearPoly(CCPoly *poly, float *position, float radius);
    virtual int            PickUpNearPoly(CCPoly *poly, const CBoxVu0 &box);
    virtual int            PickUpNearPoly(CCPoly *poly);
    virtual void           Initialize();
};

STATIC_ASSERT(sizeof(CCPoly) == 0x50);
STATIC_ASSERT(sizeof(CBoxVu0) == 0x20);

/**
 * Reports whether a ray meets any polygon of a set, and where.
 *
 * @mangled CheckHit__FP6CCPolyiPfPfPfii
 * @address 0x149D50
 * @size 0x324
 * @unknownret
 */
int CheckHit(CCPoly *poly, int count, float *from, float *to, float *hit_point, int nearest, int mode);

/**
 * Finds the collision polygon under a point, and the one above it.
 *
 * @mangled GetFootPoly__FPffP6CCPolyPfP6CCPolyii
 * @address 0x14ABB0
 * @size 0x1DC
 * @unknownret
 */
int GetFootPoly(float *position, float depth, CCPoly *found, float *ground, CCPoly *polys, int count, int mode);
