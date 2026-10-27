#pragma once

namespace dc::game {
struct RenderInfo;
} // namespace dc::game

namespace dc::render {

/**
 * One shadow-volume triangle in the 11-quadword layout the VU1 shadow program consumes.
 *
 * Every record is drawn twice: pass 0 with ALPHA_1 = 0x8000000068 (Cs * 0x80/128 + Cd), pass 1 with
 * ALPHA_1 = 0x8000000062 (Cd - Cs * 0x80/128). Pass 1 records have counts {0, 0, 0, 1} and all five
 * cap entries equal to cap[4] of pass 0. The VU1 program additionally takes the triangle flag
 * (1 on pass 1) and the matrices CVisualMDTVu1::DrawVu1 sends.
 */
struct ShadowClipTriangle {
    unsigned int edges[4];    /**< corner[0..2].edge of MDT_SVERTEX (no silhouette across that edge), then 0 */
    float        local[4][4]; /**< model-space a, b, c, c as the MDT stores them, w included */
    int          counts[4];   /**< near-cap strip lengths: {min(n, 4), n > 4 ? 3 : 0, 0, 1}, n from the clip */
    float        cap[5][4];   /**< near-cap polygon in clip space (perspective * eye), last clipped vertex first */
};

/**
 * Builds the CPU half of CVisualShadow::CreateVUdataShadowCLIP for one pass of an MDT shadow mesh.
 *
 * Emits one record per triangle facing away from the light (face . light <= 0 in model space), in
 * shape order, writing at most capacity records into out. Uses the render info's view_scaled,
 * perspective, light_direction column 0, shadow_point, shadow_normal and near[2]. Returns how many
 * triangles the pass has, which may exceed capacity.
 */
int ShadowClipBuild(ShadowClipTriangle *out, int capacity, unsigned int *model_data, game::RenderInfo *info,
                    float (*matrix)[4], int pass);

} // namespace dc::render
