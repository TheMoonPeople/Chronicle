#include "stubs/collisionmdt.hpp"

#include "vu0_ops.hpp"

namespace {

// vu_hold_box parks the query box in VU0 registers for the vu_box_missed calls that follow.
float g_held_max[4];
float g_held_min[4];

} // namespace

void vu_maxmin3(float *max, float *min, float *a, float *b, float *c) {
    float hi[4];
    float lo[4];
    vu0::Max(hi, a, b);
    vu0::Min(lo, a, b);
    vu0::Max(hi, hi, c);
    vu0::Min(lo, lo, c);
    vu0::Copy(max, hi);
    vu0::Copy(min, lo);
}

// Retail stores w from a register vopmsub never writes; nothing reads it, so it is zeroed.
void vu_normal(float *normal, float *a, float *b, float *c) {
    float edge_ab[4] = {b[0] - a[0], b[1] - a[1], b[2] - a[2], 0.0f};
    float edge_ac[4] = {c[0] - a[0], c[1] - a[1], c[2] - a[2], 0.0f};
    float cross[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vu0::Cross(cross, edge_ab, edge_ac);
    vu0::Copy(normal, cross);
}

void vu_hold_box(float *max, float *min) {
    vu0::Copy(g_held_max, max);
    vu0::Copy(g_held_min, min);
}

// Retail answers with the sticky zero and sign flags, so boxes that only touch count as a miss.
int vu_box_missed(float *max, float *min) {
    int status = 0;
    for (int i = 0; i < 3; i++) {
        status |= vu0::StatusFlags(g_held_max[i] - min[i]);
    }
    for (int i = 0; i < 3; i++) {
        status |= vu0::StatusFlags(max[i] - g_held_min[i]);
    }
    return status & (vu0::kStickyZero | vu0::kStickySign);
}
