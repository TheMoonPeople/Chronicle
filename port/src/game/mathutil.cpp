#include "mathutil.hpp"

#include <libvu0.h>

#include <array>
#include <cmath>
#include <cstdlib>

namespace {

constexpr int SIN_TABLE_SIZE = 1024;

// Table entries per radian: SIN_TABLE_SIZE / (2 * PI).
constexpr float SIN_TABLE_STEPS_PER_RADIAN = 162.97466f;

std::array<float, SIN_TABLE_SIZE> SinTable;

} // namespace

float DistPlanePoint(float *normal, float *on_plane, float *point) {
    sceVu0FVECTOR offset;

    sceVu0SubVector(offset, point, on_plane);
    return sceVu0InnerProduct(normal, offset);
}

float ReflectionPlane(float *normal, float *on_plane, float *point, float *reflection) {
    sceVu0FVECTOR step;

    const float distance = 2.0f * DistPlanePoint(normal, on_plane, point);
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

    sceVu0SubVector(line, to, from);
    sceVu0SubVector(e0, v0, from);
    sceVu0SubVector(e1, v1, from);
    sceVu0SubVector(e2, v2, from);
    const float above = -sceVu0InnerProduct(normal, e0);
    const float along = sceVu0InnerProduct(normal, line);

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

    sceVu0SubVector(p0, point, v0);
    sceVu0SubVector(p1, point, v1);
    sceVu0SubVector(p2, point, v2);
    sceVu0SubVector(e0, v1, v0);
    sceVu0SubVector(e1, v2, v1);
    sceVu0SubVector(e2, v0, v2);
    sceVu0OuterProduct(c0, e0, p0);
    sceVu0OuterProduct(c1, e1, p1);
    sceVu0OuterProduct(c2, e2, p2);
    const float d0 = sceVu0InnerProduct(c0, normal);
    const float d1 = sceVu0InnerProduct(c1, normal);
    const float d2 = sceVu0InnerProduct(c2, normal);

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

    sceVu0UnitMatrix(yaw);
    sceVu0CopyMatrix(pitch, yaw);
    sceVu0Normalize(unit, direction);
    sceVu0CopyVector(flat, unit);
    flat[1] = 0.0f;
    const float ground = DistVector(flat);
    const float cosine = ground == 0.0f ? 0.0f : unit[0] / ground;
    const float sine = ground == 0.0f ? 1.0f : unit[2] / ground;

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

    sceVu0SubVector(gap, to, from);

    switch (mode) {
        case INTERPOLATE_STEP:
            for (int axis = 0; axis < 3; axis++) {
                const float delta = gap[axis];
                const float magnitude = delta < 0.0f ? -delta : delta;

                if (magnitude < step) {
                    out[axis] = to[axis];
                } else if (delta < 0.0f) {
                    out[axis] = from[axis] - step;
                } else {
                    out[axis] = from[axis] + step;
                }
            }

            break;
        case INTERPOLATE_FRACTION:
            for (int axis = 0; axis < 3; axis++) {
                out[axis] = from[axis] + gap[axis] / step;
            }

            break;
        default:
            break;
    }
}

// Angles are radians in (-PI, PI]. Both operands start in range, so one whole-turn correction is
// enough to bring a sum or difference back into it.
float AngleInterpolate(float from, float to, float step, int mode) {
    float delta = to - from;

    if (delta > PI) {
        delta -= PI * 2.0f;
    }

    if (delta <= -PI) {
        delta += PI * 2.0f;
    }

    float offset = 0.0f;

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
        default:
            break;
    }

    float result = from + offset;

    if (result > PI) {
        result -= PI * 2.0f;
    }

    if (result <= -PI) {
        result += PI * 2.0f;
    }

    return result;
}

int AngleCmp(float a, float b, float tolerance) {
    float delta = a - b;

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
    if (angle < PI && angle > -PI) {
        return angle;
    }

    const float turns = angle / (PI * 2.0f);
    angle -= PI * 2.0f * static_cast<int>(turns);

    if (angle > PI) {
        angle -= PI * 2.0f;
    }

    if (angle < -PI) {
        angle += PI * 2.0f;
    }

    return angle;
}

float rnd() {
    return static_cast<float>(std::rand()) / 2147483648.0f;
}

float nrnd() {
    return rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() + rnd() - 6.0f;
}

void CreateSinTable() {
    for (int i = 0; i < SIN_TABLE_SIZE; i++) {
        SinTable[i] = sinf(PI * (2.0f * static_cast<float>(i)) / static_cast<float>(SIN_TABLE_SIZE));
    }
}

float Sinf(float angle) {
    if (angle >= 0.0f) {
        return SinTable[static_cast<int>(angle * SIN_TABLE_STEPS_PER_RADIAN) % SIN_TABLE_SIZE];
    }

    return -SinTable[static_cast<int>(-angle * SIN_TABLE_STEPS_PER_RADIAN) % SIN_TABLE_SIZE];
}

float Cosf(float angle) {
    return Sinf(PI / 2.0f + angle);
}
