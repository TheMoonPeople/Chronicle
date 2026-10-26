#pragma once

#include <libvu0.h>

/**
 * The game's own vector and matrix helpers, which sit beside the SDK's rather than on top of it:
   they take the same four-float vectors and four-by-four matrices and are reached by callers all
   over the image. What frame.cpp needs of them is declared in boxvu0.h instead: that unit spells
   its own inline MulMatrix and so cannot see this header at all, which is a constraint the
   original's source had too. A unit that needs both reaches for both.
 */

/**
 * Componentwise extremes over two or three points. Each writes the greater extreme through the
   first argument, so the pair's two outputs come out in the order a CBoxVu0 holds them.
 */
void VectorMax(float *max, float *a, float *b);
void VectorMax(float *max, float *a, float *b, float *c);
void VectorMin(float *min, float *a, float *b);
void VectorMaxMin(float *max, float *min, float *a, float *b);

/**
 * The plane normal of a triangle, written through the first argument. Not normalised — it is the
   outer product of two of the edges and nothing scales it.
 */
void PlaneNormal(float *normal, float *v0, float *v1, float *v2);

/**
 * How far a point lies along a plane's normal from a point on that plane, which is the signed
   distance where the normal is a unit vector.
 */
float DistPlanePoint(float *normal, float *on_plane, float *point);

/**
 * The offset that mirrors a point in that plane, written through the last argument, and twice the
   distance it stood at returned.
 */
float ReflectionPlane(float *normal, float *on_plane, float *point, float *reflection);

/**
 * Whether a point in the triangle's plane is inside it, as the three edge cross products having
   one sign against the normal.
 */
int Check_Point_Poly3_XYZ(float *point, float *v0, float *v1, float *v2, float *normal);

void MulMatrix(sceVu0FMATRIX product, sceVu0FMATRIX left_matrix, sceVu0FMATRIX right_matrix);
void RotMatrixY(sceVu0FMATRIX matrix, float angle_y);
void LookAtMatrixZ(sceVu0FMATRIX matrix, float *direction);

/**
 * One step of a straight-line move towards a target, with the fourth argument read as a per-call
   step in mode 0 and as a divisor in mode 1, exactly as AngleInterpolate reads its own.
 */
void VectorInterpolate(float *out, float *from, float *to, float step, int mode);
