#pragma once

#include <libvu0.h>

/**
 * Writes m1 * m2 into m0, the composition of a parent matrix m1 with a local matrix m2.
 */
void MulFrameMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX m2);

/**
 * Copies m1 into m0 with its first three rows scaled by the matching components of scale.
 */
void ScaleMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR scale);

/**
 * Copies the matrix m1 into m0.
 */
void CopyMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1);

/**
 * Clears every element of m0 to zero.
 */
void ZeroMatrix(sceVu0FMATRIX m0);

/**
 * Divides eight screen-space corners by their depth and writes the extent of the result.
 */
void ScreenBound(sceVu0FVECTOR *screen, sceVu0FVECTOR max, sceVu0FVECTOR min);

/**
 * Sets the matrix trance_normal transforms triangles by.
 */
void pre_trance_normal(sceVu0FMATRIX matrix);

/**
 * Transforms a triangle in place by the pre_trance_normal matrix and writes its unnormalised normal.
 */
void trance_normal(float *p0, float *p1, float *p2, float *plane);
