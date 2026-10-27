#pragma once

#include "common.h"

namespace dc::game {

/**
 * @file
 * Declares the game's vector, geometry, interpolation, and trigonometry helpers.
 */

/**
 * How VectorInterpolate and AngleInterpolate move toward their target.
 */
// clang-format off
enum InterpolateMode {
    INTERPOLATE_STEP     = 0, /**< A fixed step, snapping when closer. */
    INTERPOLATE_FRACTION = 1, /**< The gap divided by the step. */
};

// clang-format on

/**
 * Raises an exception a function did not declare, through the unexpected handler.
 */
extern "C" void __unexpected(void *exception_record);

/**
 * Starts the MetroWerks runtime.
 */
extern "C" void mwInit(int argc, const char **argv, const char **envp);

/**
 * Reads an overlay into memory and gives back whether it succeeded.
 */
extern "C" int mwLoadOverlay(char *path, void *address);

/**
 * Writes the componentwise maximum of two vectors.
 */
void VectorMax(float *max, float *a, float *b);

/**
 * Writes the componentwise maximum of three vectors.
 */
void VectorMax(float *max, float *a, float *b, float *c);

/**
 * Writes the componentwise maximum of four vectors.
 */
void VectorMax(float *max, float *a, float *b, float *c, float *d);

/**
 * Writes the componentwise minimum of two vectors.
 */
void VectorMin(float *min, float *a, float *b);

/**
 * Writes the componentwise minimum of four vectors.
 */
void VectorMin(float *min, float *a, float *b, float *c, float *d);

/**
 * Writes the componentwise maximum and minimum of two vectors.
 */
void VectorMaxMin(float *max, float *min, float *a, float *b);

/**
 * Writes the componentwise maximum and minimum of three vectors.
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c);

/**
 * Writes the componentwise maximum and minimum of four vectors.
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c, float *d);

/**
 * Writes the unnormalised normal of a triangle, the cross product of two of its edges.
 */
void PlaneNormal(float *normal, float *v0, float *v1, float *v2);

/**
 * Returns how far a point lies along a plane's normal from a point on that plane.
 */
float DistPlanePoint(float *normal, float *on_plane, float *point);

/**
 * Writes the offset that mirrors a point in a plane and returns twice its distance from the plane.
 */
float ReflectionPlane(float *normal, float *on_plane, float *point, float *reflection);

/**
 * Finds where a line meets a triangle's plane and returns whether that point is inside the triangle.
 */
int IntersectionPoint_line_poly3(float *from, float *to, float *v0, float *v1, float *v2, float *normal, float *hit);

/**
 * Returns whether a point in a triangle's plane lies inside the triangle.
 */
int Check_Point_Poly3_XYZ(float *point, float *v0, float *v1, float *v2, float *normal);

/**
 * Returns the length of a vector.
 */
float DistVector(float *vector);

/**
 * Returns the distance between two positions.
 */
float DistVector(float *a, float *b);

/**
 * Multiplies two matrices, applying the right one first.
 */
void MulMatrix(float (*product)[4], float (*left_matrix)[4], float (*right_matrix)[4]);

/**
 * Writes a rotation about the Y axis.
 */
void RotMatrixY(float (*matrix)[4], float angle_y);

/**
 * Writes a rotation that turns the Z axis to face a direction.
 */
void LookAtMatrixZ(float (*matrix)[4], float *direction);

/**
 * Transforms a run of vectors by one matrix; a count below one still transforms one vector.
 */
void ApplyMatrixN(float (*out)[4], float (*matrix)[4], float (*in)[4], int count);

/**
 * Moves a position one step toward a target, as the InterpolateMode in mode says.
 */
void VectorInterpolate(float *out, float *from, float *to, float step, int mode);

/**
 * Turns an angle one step toward a target the short way round, as the InterpolateMode in mode says.
 */
float AngleInterpolate(float from, float to, float step, int mode);

/**
 * Compares two angles the short way round: 1 or -1 when apart by more than the tolerance, else 0.
 */
int AngleCmp(float a, float b, float tolerance);

/**
 * Wraps one angle into the half turn either side of zero.
 */
float AngleLimit(float angle);

/**
 * Returns rand() divided by 2^31, a fraction in [0, 1).
 */
float rnd();

/**
 * Returns the sum of twelve rnd() draws less six, an approximately normal random number.
 */
float nrnd();

/**
 * Fills the lookup table Sinf and Cosf read.
 */
void CreateSinTable();

/**
 * Returns the sine of an angle in radians from the lookup table.
 */
float Sinf(float angle);

/**
 * Returns the cosine of an angle in radians from the lookup table.
 */
float Cosf(float angle);

} // namespace dc::game
