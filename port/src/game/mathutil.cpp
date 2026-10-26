#include "mathutil.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdlib>

static float sin_table_num = 1024.0f;
static float sin_table_unit_1 = 162.97466f;
static float SinTable[1024];
float DistPlanePoint(float *normal, float *on_plane, float *point) {
    sceVu0FVECTOR offset;

    sceVu0SubVector(offset, point, on_plane);
    return sceVu0InnerProduct(normal, offset);
}

float ReflectionPlane(float *normal, float *on_plane, float *point, float *reflection) {
    sceVu0FVECTOR step;
    float         distance;

    distance = 2.0f * DistPlanePoint(normal, on_plane, point);
    sceVu0ScaleVector(step, normal, -distance);
    sceVu0SubVector(reflection, on_plane, point);
    sceVu0SubVector(reflection, reflection, step);
    return distance;
}

int IntersectionPoint_line_poly3(float *from, float *to, float *v0, float *v1, float *v2, float *normal, float *hit) {
    sceVu0FVECTOR line;
    sceVu0FVECTOR e0;
    sceVu0FVECTOR e1;
    sceVu0FVECTOR e2;
    float         above;
    float         along;

    sceVu0SubVector(line, to, from);
    sceVu0SubVector(e0, v0, from);
    sceVu0SubVector(e1, v1, from);
    sceVu0SubVector(e2, v2, from);
    above = -sceVu0InnerProduct(normal, e0);
    along = sceVu0InnerProduct(normal, line);

    if (along == 0.0f) {
        return 0;
    }

    sceVu0ScaleVector(hit, line, -above / along);
    sceVu0AddVector(hit, hit, from);
    return Check_Point_Poly3_XYZ(hit, v0, v1, v2, normal);
}

int Check_Point_Poly3_XYZ(float *point, float *v0, float *v1, float *v2, float *normal) {
    sceVu0FVECTOR p0;
    sceVu0FVECTOR p1;
    sceVu0FVECTOR p2;
    sceVu0FVECTOR e0;
    sceVu0FVECTOR e1;
    sceVu0FVECTOR e2;
    sceVu0FVECTOR c0;
    sceVu0FVECTOR c1;
    sceVu0FVECTOR c2;
    float         d0;
    float         d1;
    float         d2;

    sceVu0SubVector(p0, point, v0);
    sceVu0SubVector(p1, point, v1);
    sceVu0SubVector(p2, point, v2);
    sceVu0SubVector(e0, v1, v0);
    sceVu0SubVector(e1, v2, v1);
    sceVu0SubVector(e2, v0, v2);
    sceVu0OuterProduct(c0, e0, p0);
    sceVu0OuterProduct(c1, e1, p1);
    sceVu0OuterProduct(c2, e2, p2);
    d0 = sceVu0InnerProduct(c0, normal);
    d1 = sceVu0InnerProduct(c1, normal);
    d2 = sceVu0InnerProduct(c2, normal);

    if (d0 >= 0.0f && d1 >= 0.0f && d2 >= 0.0f) {
        return 1;
    }

    if (d0 <= 0.0f && d1 <= 0.0f && d2 <= 0.0f) {
        return 1;
    }

    return 0;
}
void LookAtMatrixZ(sceVu0FMATRIX matrix, float *direction) {
    sceVu0FMATRIX pitch;
    sceVu0FMATRIX yaw;
    sceVu0FVECTOR unit;
    sceVu0FVECTOR flat;
    float         ground;
    float         cosine;
    float         sine;

    sceVu0UnitMatrix(yaw);
    sceVu0CopyMatrix(pitch, yaw);
    sceVu0Normalize(unit, direction);
    sceVu0CopyVector(flat, unit);
    flat[1] = 0.0f;
    ground = DistVector(flat);
    /* The length is measured a second time and the answer thrown away; it is a call the function
       makes, like the truncation AngleLimit repeats below. */
    DistVector(flat);

    if (ground == 0.0f) {
        cosine = 0.0f;
        sine = 1.0f;
    } else {
        cosine = unit[0] / ground;
        sine = unit[2] / ground;
    }

    pitch[1][1] = ground;
    pitch[1][2] = -unit[1];
    pitch[2][1] = unit[1];
    pitch[2][2] = ground;
    yaw[0][0] = sine;
    yaw[0][2] = -cosine;
    yaw[2][0] = cosine;
    yaw[2][2] = sine;
    MulMatrix(matrix, yaw, pitch);
}
void VectorInterpolate(float *out, float *from, float *to, float step, int mode) {
    sceVu0FVECTOR gap;
    int           i;
    float         magnitude;
    float         delta;

    sceVu0SubVector(gap, to, from);

    switch (mode) {
        case INTERPOLATE_STEP:
            for (i = 0; i < 3; i++) {
                /* The signed difference and its magnitude are carried as two variables, and each
                   arm writes back the one the other arm would have changed - so the second assignment
                   stores nothing on a value that already agrees. */
                delta = gap[i];
                magnitude = delta;

                if (delta < 0.0f) {
                    magnitude = -delta;
                } else {
                    delta = magnitude;
                }

                if (magnitude < step) {
                    out[i] = to[i];
                } else if (delta < 0.0f) {
                    out[i] = from[i] - step;
                } else {
                    out[i] = from[i] + step;
                }
            }

            break;
        case INTERPOLATE_FRACTION:
            for (i = 0; i < 3; i++) {
                out[i] = from[i] + gap[i] / step;
            }

            break;
    }
}

/* Every angle in the game is radians held in (-PI, PI], and each of these puts its result back in
   that range by adding or subtracting one whole turn — once is enough because both operands are
   already in range, so a difference cannot leave it by more than a turn. */

float AngleInterpolate(float from, float to, float step, int mode) {
    float delta;
    float offset;
    float result;

    delta = to - from;

    if (delta > PI) {
        delta -= PI * 2.0f;
    }

    if (delta <= -PI) {
        delta += PI * 2.0f;
    }

    offset = 0.0f;

    if (mode == INTERPOLATE_STEP && (delta < 0.0f ? -delta : delta) < step) {
        return to;
    }

    switch (mode) {
        case INTERPOLATE_STEP:
            if (delta < 0.0f) {
                if (step < delta) {
                    return to;
                }

                offset -= step;
            }

            if (delta >= 0.0f) {
                if (step > delta) {
                    return to;
                }

                offset += step;
            }

            break;
        case INTERPOLATE_FRACTION:
            offset = delta / step;
            break;
    }

    result = from + offset;

    if (result > PI) {
        result -= PI * 2.0f;
    }

    if (result <= -PI) {
        result += PI * 2.0f;
    }

    return result;
}

int AngleCmp(float a, float b, float tolerance) {
    float delta;

    delta = a - b;

    if (delta == 0.0f) {
        return 0;
    }

    if (delta > PI) {
        delta -= PI * 2.0f;
    }

    if (delta < -PI) {
        delta += PI * 2.0f;
    }

    if (delta > tolerance) {
        return 1;
    }

    if (delta < -tolerance) {
        return -1;
    }

    return 0;
}

float AngleLimit(float angle) {
    float turns;
    int   whole;

    if (angle < PI && angle > -PI) {
        return angle;
    }

    turns = angle / (PI * 2.0f);
    /* The first truncation is a call whose result nothing reads, and it is kept because it is one
       the function makes: each conversion in the source is its own call. */
    whole = (int) turns;
    angle -= PI * 2.0f * (int) turns;

    if (angle > PI) {
        angle -= PI * 2.0f;
    }

    if (angle < -PI) {
        angle += PI * 2.0f;
    }

    return angle;
}

float rnd() {
    return (float) rand() / 2147483648.0f;
}

float nrnd() {
    return rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() - 6.0f;
}

void CreateSinTable() {
    int i;

    sin_table_num = 1024.0f;
    sin_table_unit_1 = 162.97466f;

    for (i = 0; i < 1024; i++) {
        SinTable[i] = sinf(PI * (2.0f * (float) i) / sin_table_num);
    }
}

float Sinf(float angle) {
    if (angle >= 0.0f) {
        return SinTable[(int) (angle * sin_table_unit_1) % 1024];
    }

    return -SinTable[(int) (-angle * sin_table_unit_1) % 1024];
}

float Cosf(float angle) {
    return Sinf(PI / 2.0f + angle);
}
