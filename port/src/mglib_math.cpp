#include <bit>
#include <cstdint>
#include <initializer_list>

#include "mglib.hpp"
#include "renderinfo.hpp"
#include "vu0_ops.hpp"

namespace {

void StoreCorner(int *out, const float corner[4], std::int32_t w) {
    out[0] = vu0::Ftoi(corner[0], 16.0f);
    out[1] = vu0::Ftoi(corner[1], 16.0f);
    out[2] = vu0::Ftoi(corner[2], 1.0f);
    out[3] = w;
}

} // namespace

// Visibility is retail's sticky zero and sign flags, so a corner exactly on 0 or 4096 hides the
// sprite. Without fog, retail stores the clip w lane untouched by vftoi, so the corners' w is its
// float bits. The fog clamp takes fog_near before fog_far, the reverse of MGRotTransPers2D.
int MGRotTransPers3DSprite(int *top_left, int *bottom_right, float *position, float width, float height, int fog) {
    float half[2] = {0.5f * width * mgRenderInfo.scale[0], 0.5f * height * mgRenderInfo.scale[1]};
    float point[4];

    vu0::Apply(point, mgRenderInfo.view_screen, position);
    float q = vu0::Div(1.0f, point[3]);
    for (int i = 0; i < 3; i++) {
        point[i] = vu0::Mul(point[i], q);
    }
    half[0] = vu0::Mul(half[0], q);
    half[1] = vu0::Mul(half[1], q);

    float near_corner[4];
    float far_corner[4];
    vu0::Copy(near_corner, point);
    vu0::Copy(far_corner, point);
    near_corner[0] = near_corner[0] - half[0];
    near_corner[1] = near_corner[1] - half[1];
    far_corner[0] = far_corner[0] + half[0];
    far_corner[1] = far_corner[1] + half[1];

    int status = 0;
    for (const float *corner : {near_corner, far_corner}) {
        status |= vu0::StatusFlags(corner[0]) | vu0::StatusFlags(corner[1]) | vu0::StatusFlags(corner[3]);
        status |= vu0::StatusFlags(4096.0f - corner[0]) | vu0::StatusFlags(4096.0f - corner[1]);
    }

    auto w = std::bit_cast<std::int32_t>(point[3]);
    if (fog) {
        float density = mgRenderInfo.fog_a + vu0::Mul(mgRenderInfo.fog_b, q);
        density = vu0::Min(density, mgRenderInfo.fog_near);
        density = vu0::Max(density, mgRenderInfo.fog_far);
        w = vu0::Ftoi(density, 1.0f);
    }

    StoreCorner(top_left, near_corner, w);
    StoreCorner(bottom_right, far_corner, w);

    return (status & (vu0::kStickyZero | vu0::kStickySign)) == 0;
}

// light_direction holds one light per column (sceVu0NormalLightMatrix's layout). The clamps run on
// all four lanes, so the colour's w is clamped to 255 like the rest.
void MGCalcColor(float *color, float *normal) {
    color[0] = 255.0f;
    float ceiling = color[0];

    float n[4];
    vu0::Copy(n, normal);
    const float (*direction)[4] = mgRenderInfo.light_direction;
    const float (*intensity)[4] = mgRenderInfo.light_color;
    const float *ambient = mgRenderInfo.ambient;

    float facing[4];
    for (int i = 0; i < 4; i++) {
        facing[i] = direction[0][i] * n[0];
        facing[i] = facing[i] + direction[1][i] * n[1];
        facing[i] = facing[i] + direction[2][i] * n[2];
        facing[i] = vu0::Max(facing[i], 0.0f);
    }

    float sum[4];
    for (int i = 0; i < 4; i++) {
        sum[i] = ambient[i];
        sum[i] = sum[i] + intensity[0][i] * facing[0];
        sum[i] = sum[i] + intensity[1][i] * facing[1];
        sum[i] = sum[i] + intensity[2][i] * facing[2];
        sum[i] = sum[i] + intensity[3][i] * facing[3];
        sum[i] = vu0::Min(sum[i], ceiling);
    }

    vu0::Copy(color, sum);
}
