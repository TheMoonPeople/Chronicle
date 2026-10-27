#pragma once

#include <bit>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace dc::game {

/**
 * The VU0 macro-mode operations the game's inline assembly is built from. A matrix is four column
 * vectors: m[i] is column i, as in sce/libvu0.cpp.
 */
namespace vu0 {

/**
 * Copies a four-lane vector.
 * `out` may alias `in`.
 */
inline void Copy(float out[4], const float in[4]) {
    std::memcpy(out, in, sizeof(float) * 4);
}

/**
 * Multiplies `v` by the column-major matrix `m` as the accumulator chain
 * vmulax/vmadday/vmaddaz/vmaddw does, in its order of additions, on all four lanes.
 * `out` may alias `v`.
 */
inline void Apply(float out[4], const float m[4][4], const float v[4]) {
    float result[4];
    for (int i = 0; i < 4; i++) {
        result[i] = m[0][i] * v[0];
        result[i] = result[i] + m[1][i] * v[1];
        result[i] = result[i] + m[2][i] * v[2];
        result[i] = result[i] + m[3][i] * v[3];
    }
    Copy(out, result);
}

/**
 * Returns the key vmax and vmini order floats by: their bits as
 * sign and magnitude, so -0 sorts below +0.
 */
inline std::uint32_t OrderKey(float value) {
    std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x80000000u) != 0 ? ~bits : bits | 0x80000000u;
}

/**
 * Returns the larger of two floats
 * as vmax orders them.
 */
inline float Max(float a, float b) {
    return OrderKey(a) >= OrderKey(b) ? a : b;
}

/**
 * Returns the smaller of two floats
 * as vmini orders them.
 */
inline float Min(float a, float b) {
    return OrderKey(a) <= OrderKey(b) ? a : b;
}

/**
 * Lane-wise vmax of two
 * four-lane vectors.
 */
inline void Max(float out[4], const float a[4], const float b[4]) {
    for (int i = 0; i < 4; i++) {
        out[i] = Max(a[i], b[i]);
    }
}

/**
 * Lane-wise vmini of two
 * four-lane vectors.
 */
inline void Min(float out[4], const float a[4], const float b[4]) {
    for (int i = 0; i < 4; i++) {
        out[i] = Min(a[i], b[i]);
    }
}

/**
 * Clamps an infinity to the largest finite value of its sign:
 * the unit has no infinities, so an overflow saturates.
 */
inline float Saturate(float value) {
    if (std::isinf(value)) {
        return std::copysign(FLT_MAX, value);
    }
    return value;
}

/**
 * vdiv: a zero (or denormal, which the unit flushes) divisor gives the largest value, signed by
 * both operands, where the host would give an infinity or a NaN.
 */
inline float Div(float numerator, float denominator) {
    if (std::fabs(denominator) < FLT_MIN) {
        return std::signbit(numerator) != std::signbit(denominator) ? -FLT_MAX : FLT_MAX;
    }
    return Saturate(numerator / denominator);
}

/**
 * vsqrt: the square root of
 * the magnitude of `value`.
 */
inline float Sqrt(float value) {
    return std::sqrt(std::fabs(value));
}

/**
 * vrsqrt:
 * numerator / sqrt(|value|).
 */
inline float Rsqrt(float numerator, float value) {
    return Div(numerator, Sqrt(value));
}

/**
 * Multiplies two floats, saturating
 * an overflow as the unit does.
 */
inline float Mul(float a, float b) {
    return Saturate(a * b);
}

/**
 * The squared length every length routine builds with vmr32
 * and two vadd.x: (x*x + y*y) + z*z.
 */
inline float LengthSquared(const float v[4]) {
    float x = v[0] * v[0];
    float y = v[1] * v[1];
    float z = v[2] * v[2];
    return z + (x + y);
}

/**
 * vftoi0/vftoi4/vftoi12/vftoi15: scales by `scale`,
 * truncates toward zero and saturates; NaN gives 0.
 */
inline std::int32_t Ftoi(float value, float scale) {
    float scaled = std::trunc(value * scale);
    if (std::isnan(scaled)) {
        return 0;
    }
    if (scaled <= -2147483648.0f) {
        return std::numeric_limits<std::int32_t>::min();
    }
    if (scaled >= 2147483648.0f) {
        return std::numeric_limits<std::int32_t>::max();
    }
    return static_cast<std::int32_t>(scaled);
}

/**
 * Sticky status bit for a zero result.
 */
inline constexpr int kStickyZero = 0x40;

/**
 * Sticky status bit for a negative result.
 */
inline constexpr int kStickySign = 0x80;

/**
 * The sticky status bits one result lane leaves: zero after the unit's flush of denormals, sign
 * from the sign bit.
 */
inline int StatusFlags(float result) {
    int flags = 0;
    if (std::fabs(result) < FLT_MIN) {
        flags |= kStickyZero;
    }
    if (std::signbit(result)) {
        flags |= kStickySign;
    }
    return flags;
}

/**
 * vopmula then vopmsub: the outer product a x b on xyz.
 * `out` may alias `a` or `b`.
 */
inline void Cross(float out[3], const float a[4], const float b[4]) {
    float result[3] = {
        a[1] * b[2] - b[1] * a[2],
        a[2] * b[0] - b[2] * a[0],
        a[0] * b[1] - b[0] * a[1],
    };
    std::memcpy(out, result, sizeof(result));
}

} // namespace vu0

} // namespace dc::game
