#pragma once

#include <libvu0.h>

namespace dc::game {

/**
 * An axis-aligned bound over any component type, held as the two extreme corners with the greater
 * one first. Its corners are spaced as quadwords, so a CBoxVu0 and a frame's own pair of extremes
 * can be handed to the same code.
 */
template <class T>
class CBox3 {
public:
    T max[4]; /**< Greater corner on each axis. */
    T min[4]; /**< Lesser corner on each axis. */
};

/**
 * Writes the two extremes of three or four points at once, the greater through the first
 * argument, so the outputs are in the order a CBoxVu0 holds them.
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c);
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c, float *d);

/**
 * Writes one extreme of four points, which folds a set of bounds back into a single one.
 */
void VectorMax(float *max, float *a, float *b, float *c, float *d);
void VectorMin(float *min, float *a, float *b, float *c, float *d);

/**
 * Transforms a run of vectors by one matrix; a count below one still transforms one vector.
 */
void ApplyMatrixN(sceVu0FVECTOR *out, sceVu0FMATRIX matrix, sceVu0FVECTOR *in, int count);

/**
 * Answers the length of a vector, or the distance between two points.
 */
float DistVector(float *vector);
float DistVector(float *a, float *b);

/**
 * Finds where a line meets a triangle, given the triangle's plane normal. Answers zero when the
 * line misses, and writes the point through the last argument otherwise.
 */
int IntersectionPoint_line_poly3(float *from, float *to, float *v0, float *v1, float *v2, float *normal, float *hit);

} // namespace dc::game
