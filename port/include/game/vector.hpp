#pragma once

#include <libvu0.h>

namespace dc::game {

/**
 * The game's own vector and matrix helpers, taking the same four-float vectors and four-by-four
 * matrices as the SDK's. frame.cpp defines its own inline MulMatrix, so it gets the helpers it
 * needs from boxvu0.h instead of this header.
 */

/**
 * Componentwise extremes over two or three points. VectorMaxMin writes the maximum through the
 * first argument and the minimum through the second, in the order a CBoxVu0 holds them.
 */
void VectorMax(float *max, float *a, float *b);
void VectorMax(float *max, float *a, float *b, float *c);
void VectorMin(float *min, float *a, float *b);
void VectorMaxMin(float *max, float *min, float *a, float *b);

/**
 * Writes the plane normal of a triangle through the first argument. It is the unnormalised outer
 * product of two of the edges.
 */
void PlaneNormal(float *normal, float *v0, float *v1, float *v2);

/**
 * Returns how far a point lies along a plane's normal from a point on that plane, which is the
 * signed distance when the normal is a unit vector.
 */
float DistPlanePoint(float *normal, float *on_plane, float *point);

/**
 * Writes the offset that mirrors a point in a plane through the last argument and returns twice
 * the point's distance from the plane.
 */
float ReflectionPlane(float *normal, float *on_plane, float *point, float *reflection);

/**
 * Returns whether a point in the triangle's plane lies inside it, that is whether the three edge
 * cross products have one sign against the normal.
 */
int Check_Point_Poly3_XYZ(float *point, float *v0, float *v1, float *v2, float *normal);

/**
 * Writes the product of two matrices through the first argument.
 */
void MulMatrix(sceVu0FMATRIX product, sceVu0FMATRIX left_matrix, sceVu0FMATRIX right_matrix);

/**
 * Sets a matrix to a rotation of angle_y radians about the Y axis.
 */
void RotMatrixY(sceVu0FMATRIX matrix, float angle_y);

/**
 * Builds a rotation matrix whose Z axis points along a direction.
 */
void LookAtMatrixZ(sceVu0FMATRIX matrix, float *direction);

/**
 * Takes one step of a straight-line move towards a target, reading the fourth argument as a
 * per-call step in mode 0 and as a divisor in mode 1, as AngleInterpolate does.
 */
void VectorInterpolate(float *out, float *from, float *to, float step, int mode);

} // namespace dc::game
