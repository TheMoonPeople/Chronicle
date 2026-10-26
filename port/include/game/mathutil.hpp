#pragma once

#include "common.h"

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
 *
 * @mangled __unexpected
 * @address 0x122B40
 * @size 0x1C0
 */
extern "C" void __unexpected(void *exception_record);

/**
 * Starts the MetroWerks runtime.
 *
 * @mangled mwInit
 * @address 0x122DA0
 * @size 0x40
 */
extern "C" void mwInit(int argc, const char **argv, const char **envp);

/**
 * Reads an overlay into memory and gives back whether it succeeded.
 *
 * @mangled mwLoadOverlay
 * @address 0x122F40
 * @size 0x70
 */
extern "C" int mwLoadOverlay(char *path, void *address);

/**
 * @mangled VectorMax__FPfPfPf
 * @address 0x122FB0
 * @size 0x18
 * @unknownret
 */
void VectorMax(float *max, float *a, float *b);

/**
 * @mangled VectorMax__FPfPfPfPf
 * @address 0x122FD0
 * @size 0x20
 * @unknownret
 */
void VectorMax(float *max, float *a, float *b, float *c);

/**
 * @mangled VectorMax__FPfPfPfPfPf
 * @address 0x122FF0
 * @size 0x28
 * @unknownret
 */
void VectorMax(float *max, float *a, float *b, float *c, float *d);

/**
 * @mangled VectorMin__FPfPfPf
 * @address 0x123020
 * @size 0x18
 * @unknownret
 */
void VectorMin(float *min, float *a, float *b);

/**
 * @mangled VectorMin__FPfPfPfPfPf
 * @address 0x123040
 * @size 0x28
 * @unknownret
 */
void VectorMin(float *min, float *a, float *b, float *c, float *d);

/**
 * @mangled VectorMaxMin__FPfPfPfPf
 * @address 0x123070
 * @size 0x20
 * @unknownret
 */
void VectorMaxMin(float *max, float *min, float *a, float *b);

/**
 * @mangled VectorMaxMin__FPfPfPfPfPf
 * @address 0x123090
 * @size 0x2C
 * @unknownret
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c);

/**
 * @mangled VectorMaxMin__FPfPfPfPfPfPf
 * @address 0x1230C0
 * @size 0x38
 * @unknownret
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c, float *d);

/**
 * @mangled PlaneNormal__FPfPfPfPf
 * @address 0x123100
 * @size 0x28
 * @unknownret
 */
void PlaneNormal(float *normal, float *v0, float *v1, float *v2);

/**
 * @mangled DistPlanePoint__FPfPfPf
 * @address 0x123130
 * @size 0x4C
 */
float DistPlanePoint(float *normal, float *on_plane, float *point);

/**
 * @mangled ReflectionPlane__FPfPfPfPf
 * @address 0x123180
 * @size 0xA8
 */
float ReflectionPlane(float *normal, float *on_plane, float *point, float *reflection);

/**
 * @mangled IntersectionPoint_line_poly3__FPfPfPfPfPfPfPf
 * @address 0x123230
 * @size 0x144
 */
int IntersectionPoint_line_poly3(float *from, float *to, float *v0, float *v1, float *v2, float *normal, float *hit);

/**
 * @mangled Check_Point_Poly3_XYZ__FPfPfPfPfPf
 * @address 0x123380
 * @size 0x1DC
 */
int Check_Point_Poly3_XYZ(float *point, float *v0, float *v1, float *v2, float *normal);

/**
 * @mangled DistVector__FPf
 * @address 0x123560
 * @size 0x30
 */
float DistVector(float *vector);

/**
 * Returns the distance between two positions.
 *
 * @mangled DistVector__FPfPf
 * @address 0x123590
 * @size 0x38
 */
float DistVector(float *a, float *b);

/**
 * @mangled MulMatrix__FPA4_fPA4_fPA4_f
 * @address 0x1235D0
 * @size 0x78
 * @unknownret
 */
void MulMatrix(float (*product)[4], float (*left_matrix)[4], float (*right_matrix)[4]);

/**
 * @mangled RotMatrixY__FPA4_ff
 * @address 0x123650
 * @size 0x74
 * @unknownret
 */
void RotMatrixY(float (*matrix)[4], float angle_y);

/**
 * @mangled LookAtMatrixZ__FPA4_fPf
 * @address 0x1236D0
 * @size 0x10C
 * @unknownret
 */
void LookAtMatrixZ(float (*matrix)[4], float *direction);

/**
 * @mangled ApplyMatrixN__FPA4_fPA4_fPA4_fi
 * @address 0x1237E0
 * @size 0x60
 * @unknownret
 */
void ApplyMatrixN(float (*out)[4], float (*matrix)[4], float (*in)[4], int count);

/**
 * @mangled VectorInterpolate__FPfPfPffi
 * @address 0x123840
 * @size 0x18C
 * @unknownret
 */
void VectorInterpolate(float *out, float *from, float *to, float step, int mode);

/**
 * @mangled AngleInterpolate__Ffffi
 * @address 0x1239D0
 * @size 0x160
 */
float AngleInterpolate(float from, float to, float step, int mode);

/**
 * @mangled AngleCmp__Ffff
 * @address 0x123B30
 * @size 0xA8
 * @unknownret
 */
int AngleCmp(float a, float b, float tolerance);

/**
 * Wraps one angle into the half turn either side of zero.
 *
 * @mangled AngleLimit__Ff
 * @address 0x123BE0
 * @size 0xD0
 */
float AngleLimit(float angle);

/**
 * @mangled rnd__Fv
 * @address 0x123CB0
 * @size 0x3C
 */
float rnd();

/**
 * @mangled nrnd__Fv
 * @address 0x123CF0
 * @size 0xC0
 */
float nrnd();

/**
 * @mangled CreateSinTable__Fv
 * @address 0x123DB0
 * @size 0x90
 * @unknownret
 */
void CreateSinTable();

/**
 * @mangled Sinf__Ff
 * @address 0x123E40
 * @size 0xB8
 */
float Sinf(float angle);

/**
 * @mangled Cosf__Ff
 * @address 0x123F00
 * @size 0x28
 */
float Cosf(float angle);

/** + 4 more not-yet-named function(s) in this range (IDA/disassembler could not name them) */
