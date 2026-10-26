#pragma once

#include <libvu0.h>

/**
 * An axis-aligned bound, held as the two extreme corners with the greater one first.
 */
/**
 * The same bound over a component type the Vector Unit is not involved in. Its two corners are
   spaced as if they were quadwords even so, which is what lets a CBoxVu0 and a frame's own pair of
   extremes be handed to the same code.
 */
template <class T>
class CBox3 {
public:
    T max[4]; /**< Greater corner on each axis. */
    T min[4]; /**< Lesser corner on each axis. */
};

/**
 * The two extremes of three or four points at once, which is what builds a bound out of corners.
   Both write the greater extreme through the first argument, so the outputs are in the order a
   CBoxVu0 holds them.
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c);
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c, float *d);

/**
 * One extreme of four points, which is what folds a set of bounds back into a single one.
 */
void VectorMax(float *max, float *a, float *b, float *c, float *d);
void VectorMin(float *min, float *a, float *b, float *c, float *d);

void ApplyMatrixN(sceVu0FVECTOR *out, sceVu0FMATRIX matrix, sceVu0FVECTOR *in, int count);

/**
 * The length of a vector, and the distance between two points. Declared here rather than in
   vector.h for the same reason as the extremes above: frame.cpp reaches for them and cannot see
   that header at all.
 */
float DistVector(float *vector);
float DistVector(float *a, float *b);

/**
 * Where a line meets a triangle, given the triangle's plane normal rather than deriving it.
   Answers zero when the line misses, and writes the point through the last argument otherwise.
   Defined beside the rest of the vector helpers, in vector.h's unit.
 */
int IntersectionPoint_line_poly3(float *from, float *to, float *v0, float *v1, float *v2, float *normal, float *hit);
