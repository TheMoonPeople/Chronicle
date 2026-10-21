#include "mathutil.hpp"

#include <libvu0.h>

#include "vu0_ops.hpp"

void VectorMax(float *max, float *a, float *b) {
    vu0::Max(max, a, b);
}

void VectorMax(float *max, float *a, float *b, float *c) {
    float r[4];
    vu0::Max(r, a, b);
    vu0::Max(max, r, c);
}

void VectorMax(float *max, float *a, float *b, float *c, float *d) {
    float r[4];
    vu0::Max(r, a, b);
    vu0::Max(r, r, c);
    vu0::Max(max, r, d);
}

void VectorMin(float *min, float *a, float *b) {
    vu0::Min(min, a, b);
}

void VectorMin(float *min, float *a, float *b, float *c, float *d) {
    float r[4];
    vu0::Min(r, a, b);
    vu0::Min(r, r, c);
    vu0::Min(min, r, d);
}

// Both results are computed before either is stored, so an output may alias an input.
void VectorMaxMin(float *max, float *min, float *a, float *b) {
    float hi[4];
    float lo[4];
    vu0::Max(hi, a, b);
    vu0::Min(lo, a, b);
    vu0::Copy(max, hi);
    vu0::Copy(min, lo);
}

void VectorMaxMin(float *max, float *min, float *a, float *b, float *c) {
    float hi[4];
    float lo[4];
    vu0::Max(hi, a, b);
    vu0::Min(lo, a, b);
    vu0::Max(hi, hi, c);
    vu0::Min(lo, lo, c);
    vu0::Copy(max, hi);
    vu0::Copy(min, lo);
}

void VectorMaxMin(float *max, float *min, float *a, float *b, float *c, float *d) {
    float hi[4];
    float lo[4];
    vu0::Max(hi, a, b);
    vu0::Min(lo, a, b);
    vu0::Max(hi, hi, c);
    vu0::Min(lo, lo, c);
    vu0::Max(hi, hi, d);
    vu0::Min(lo, lo, d);
    vu0::Copy(max, hi);
    vu0::Copy(min, lo);
}

// Retail stores w from a register vopmsub never writes; nothing reads it, so it is zeroed.
void PlaneNormal(float *normal, float *v0, float *v1, float *v2) {
    float e0[4] = {v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2], 0.0f};
    float e1[4] = {v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2], 0.0f};
    float r[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vu0::Cross(r, e0, e1);
    vu0::Copy(normal, r);
}

float DistVector(float *vector) {
    return vu0::Sqrt(vu0::LengthSquared(vector));
}

float DistVector(float *a, float *b) {
    float d[4] = {b[0] - a[0], b[1] - a[1], b[2] - a[2], 0.0f};
    return vu0::Sqrt(vu0::LengthSquared(d));
}

// product[i][j] = sum over k of right[i][k] * left[k][j]: right is applied first.
void MulMatrix(sceVu0FMATRIX product, sceVu0FMATRIX left_matrix, sceVu0FMATRIX right_matrix) {
    float r[4][4];
    for (int i = 0; i < 4; i++) {
        vu0::Apply(r[i], left_matrix, right_matrix[i]);
    }
    for (int i = 0; i < 4; i++) {
        vu0::Copy(product[i], r[i]);
    }
}

void RotMatrixY(sceVu0FMATRIX matrix, float angle_y) {
    float angle = angle_y;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            matrix[i][j] = i == j ? 1.0f : 0.0f;
        }
    }

    matrix[2][2] = Cosf(angle);
    matrix[0][0] = matrix[2][2];
    matrix[2][0] = Sinf(angle);
    matrix[0][2] = -matrix[2][0];
}

// The loop is a do-while on count - 1 >= 0, so a count below one still transforms one vector.
void ApplyMatrixN(sceVu0FVECTOR *out, sceVu0FMATRIX matrix, sceVu0FVECTOR *in, int count) {
    float m[4][4];
    std::memcpy(m, matrix, sizeof(m));

    int i = 0;
    do {
        vu0::Apply(out[i], m, in[i]);
        i++;
    } while (i < count);
}

// The host has already run every static constructor before main starts.
PORT_OVERRIDE extern "C" void mwInit(int argc, const char **argv, const char **envp) {}

PORT_OVERRIDE extern "C" int mwLoadOverlay(char *path, void *address) {
    return 1;
}

PORT_OVERRIDE extern "C" void __unexpected(void *exception_record) {
    PS2_UNIMPLEMENTED();
}

PORT_OVERRIDE extern "C" void *__vt__Q23std9exception[4] = {};
