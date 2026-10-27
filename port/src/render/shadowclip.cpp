#include "render/shadowclip.hpp"

#include <libvu0.h>

#include <array>
#include <vector>

#include "character.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "renderinfo.hpp"
#include "vu0_ops.hpp"

namespace dc::render {

using namespace dc::game;

int ShadowClipBuild(ShadowClipTriangle *out, int capacity, unsigned int *model_data, RenderInfo *info,
                    float (*matrix)[4], int pass) {
    if (model_data == nullptr) {
        return 0;
    }

    sceVu0FVECTOR light = {info->light_direction[0][0], info->light_direction[1][0], info->light_direction[2][0], 0.0f};
    sceVu0FMATRIX transpose;
    sceVu0FVECTOR local_light;
    sceVu0FMATRIX local_to_eye;
    sceVu0TransposeMatrix(transpose, matrix);
    sceVu0ApplyMatrix(local_light, transpose, light);
    MulMatrix(local_to_eye, info->view_scaled, matrix);

    sceVu0FVECTOR point;
    sceVu0FVECTOR normal;
    sceVu0CopyVector(point, info->shadow_point);
    sceVu0CopyVector(normal, info->shadow_normal);
    sceVu0FVECTOR direction = {info->light_direction[0][0], info->light_direction[1][0], info->light_direction[2][0], 0.0f};
    float         plane_scale = sceVu0InnerProduct(normal, point);

    if (plane_scale == 0.0f) {
        point[0] -= 0.1f * normal[0];
        point[1] -= 0.1f * normal[1];
        point[2] -= 0.1f * normal[2];
        plane_scale = 1.0f;
    }

    plane_scale = 1.0f / plane_scale;
    float nx = normal[0] * plane_scale;
    float ny = normal[1] * plane_scale;
    float nz = normal[2] * plane_scale;

    sceVu0FMATRIX shadow_matrix;
    sceVu0FMATRIX shadow_to_eye;
    sceVu0FMATRIX clip;
    sceVu0Normalize(direction, direction);
    sceVu0DropShadowMatrix(shadow_matrix, direction, nx, ny, nz, 0);
    MulMatrix(shadow_matrix, shadow_matrix, matrix);
    MulMatrix(shadow_to_eye, info->view_scaled, shadow_matrix);
    sceVu0CopyMatrix(clip, info->perspective);

    auto *model = reinterpret_cast<MDT_HEADER *>(model_data);
    auto *shadow = reinterpret_cast<MDT_SHADOW *>(reinterpret_cast<unsigned char *>(model) + model->mesh_ofs);
    auto *vertices = reinterpret_cast<sceVu0FVECTOR *>(reinterpret_cast<unsigned char *>(model) + model->vertex_ofs);

    std::vector<std::array<float, 4>> eye(model->vertex_num);
    std::vector<std::array<float, 4>> projected(model->vertex_num);

    for (int i = 0; i < model->vertex_num; i++) {
        sceVu0ApplyMatrix(eye[i].data(), local_to_eye, vertices[i]);
        sceVu0ApplyMatrix(projected[i].data(), shadow_to_eye, vertices[i]);
    }

    int   total = 0;
    auto *corner = reinterpret_cast<MDT_SVERTEX *>(shadow->shape);

    for (int shape_index = 0; shape_index < shadow->shape_num; shape_index++) {
        int index_num = reinterpret_cast<MDT_SSHAPE *>(corner)->index_num;
        corner = reinterpret_cast<MDT_SSHAPE *>(corner)->vertex;

        for (int emitted = 0; emitted < index_num; emitted += 3, corner += 3) {
            int           a = corner[0].index;
            int           b = corner[1].index;
            int           c = corner[2].index;
            sceVu0FVECTOR edge0;
            sceVu0FVECTOR edge1;
            sceVu0FVECTOR face;

            sceVu0SubVector(edge0, vertices[b], vertices[a]);
            sceVu0SubVector(edge1, vertices[c], vertices[a]);
            sceVu0OuterProduct(face, edge0, edge1);

            if (sceVu0InnerProduct(face, local_light) > 0.0f) {
                continue;
            }

            if (total < capacity) {
                ShadowClipTriangle &record = out[total];

                record.edges[0] = corner[0].edge;
                record.edges[1] = corner[1].edge;
                record.edges[2] = corner[2].edge;
                record.edges[3] = 0;
                vu0::Copy(record.local[0], vertices[a]);
                vu0::Copy(record.local[1], vertices[b]);
                vu0::Copy(record.local[2], vertices[c]);
                vu0::Copy(record.local[3], vertices[c]);

                float near_side[4][4];
                float far_side[4][4];
                vu0::Copy(near_side[0], eye[a].data());
                vu0::Copy(near_side[1], eye[b].data());
                vu0::Copy(near_side[2], eye[c].data());
                vu0::Copy(near_side[3], eye[a].data());
                vu0::Copy(far_side[0], projected[a].data());
                vu0::Copy(far_side[1], projected[b].data());
                vu0::Copy(far_side[2], projected[c].data());
                vu0::Copy(far_side[3], projected[a].data());

                // scissior's three- and four-crossing case can write more than the five entries
                // the cap uses.
                float clipped[9][4] = {};
                int   clip_num = scissior(clipped, near_side, far_side, info->near[2]);

                float projected_cap[5][4];
                for (int k = 0; k < 5; k++) {
                    sceVu0ApplyMatrix(projected_cap[k], clip, clipped[k]);
                }

                record.counts[0] = clip_num;
                record.counts[1] = 0;
                record.counts[2] = 0;
                record.counts[3] = 1;

                if (clip_num > 4) {
                    record.counts[0] = 4;
                    record.counts[1] = 3;
                }

                if (pass != 0) {
                    record.counts[0] = 0;
                    record.counts[1] = 0;
                    for (auto &cap_entry : record.cap) {
                        vu0::Copy(cap_entry, projected_cap[0]);
                    }
                } else {
                    for (int k = 0; k < 5; k++) {
                        vu0::Copy(record.cap[k], projected_cap[4 - k]);
                    }
                }
            }

            total++;
        }
    }

    return total;
}

} // namespace dc::render
