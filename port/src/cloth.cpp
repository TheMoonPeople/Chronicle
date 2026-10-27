#include "cloth.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include "bound.hpp"
#include "frame.hpp"
#include "mglib.hpp"
#include "render/draw3d.hpp"
#include "renderinfo.hpp"
#include "stubs/cloth.hpp"
#include "texture.hpp"
#include "vu0_ops.hpp"
#include "wind.hpp"

namespace {

// Cut interpolation for attachment or local deformation jumps beyond the simulation's reset distance.
constexpr float kClothResetDistance = 10.0f;

void ClothDrawMatrix(CFrame *frame, sceVu0FMATRIX matrix) {
    sceVu0UnitMatrix(matrix);
    if (frame == nullptr) {
        return;
    }
    sceVu0FMATRIX world;
    frame->GetLWMatrix(world);
    sceVu0CopyVector(matrix[3], world[3]);

    // Use a rigid basis so scale and shear stay in the vertices and the inverse remains exact.
    sceVu0FVECTOR x;
    sceVu0FVECTOR y;
    sceVu0FVECTOR z;
    sceVu0Normalize(x, world[0]);
    sceVu0OuterProduct(z, x, world[1]);
    if (sceVu0InnerProduct(x, x) == 0.0f || sceVu0InnerProduct(z, z) == 0.0f) {
        return;
    }
    sceVu0Normalize(z, z);
    sceVu0OuterProduct(y, z, x);
    sceVu0CopyVector(matrix[0], x);
    sceVu0CopyVector(matrix[1], y);
    sceVu0CopyVector(matrix[2], z);
    matrix[0][3] = matrix[1][3] = matrix[2][3] = 0.0f;
}

void Follow(float *vertex, float *velocity, float *world_home, const float *home, const float *delta,
            const float matrix[4][4], const float *stiffness) {
    float carried[4];
    vu0::Apply(carried, matrix, home);
    vertex[3] = 1.0f;
    velocity[3] = 1.0f;
    vu0::Copy(world_home, carried);

    float force[4];
    for (int i = 0; i < 4; i++) {
        force[i] = delta[i] + (carried[i] - vertex[i]) * stiffness[i];
    }

    for (int i = 0; i < 3; i++) {
        vertex[i] = (vertex[i] + velocity[i]) + force[i];
        velocity[i] = 0.0f - force[i];
    }
}

} // namespace

// The grid is rebuilt every frame from the simulation, so it is drawn as an immediate triangle
// list: one strip per row pair, each row drawn from the far side (wound the other way) when
// polygon_divide says so, all with the fixed cloth material and the model's texture. The vertices
// are relative to the attachment frame, so display frames carry them with the body's rotation
// while interpolating the simulated deformation separately.
int CCloth::CreateVUData(u_int *packet) {
    if (packet == nullptr) {
        return kDraw3DBlockQuads;
    }
    if (num_i <= 0 || num_i > 16 || num_j <= 0 || num_j > 16) {
        return kDraw3DBlockQuads;
    }
    Draw3DVisual &visual = Draw3DRegisterVisual(packet, true);
    sceVu0FMATRIX model;
    sceVu0FMATRIX inverse;
    ClothDrawMatrix(frame, model);
    sceVu0InversMatrix(inverse, model);

    Draw3DStrip strip;
    float       cloth_ambient[4] = {0.3f, 0.3f, 0.3f, 0.0f};
    std::memcpy(strip.ambient, cloth_ambient, sizeof(strip.ambient));
    for (float &value : strip.specular) {
        value = 1.0f;
    }
    float     su = 1.0f;
    float     sv = 1.0f;
    CTexture *texture = TexManager.GetTexture(material.texture, -1);
    if (texture != nullptr) {
        strip.texture = TexManager.GetTextureHandle(material.texture, -1);
        strip.tex0 = texture->tex0;
        strip.tex1 = texture->tex1;
        if (texture->width > 0 && texture->height > 0) {
            su = static_cast<float>(1u << ((texture->tex0 >> 26) & 0xF)) / static_cast<float>(texture->width);
            sv = static_cast<float>(1u << ((texture->tex0 >> 30) & 0xF)) / static_cast<float>(texture->height);
        }
    }

    auto add = [&](int i, int j) {
        gfx::Vertex3D vertex = {};
        sceVu0FVECTOR world_point = {point[i][j][0], point[i][j][1], point[i][j][2], 1.0f};
        sceVu0FVECTOR local_point;
        sceVu0FVECTOR local_normal;
        sceVu0ApplyMatrix(local_point, inverse, world_point);
        sceVu0FVECTOR world_normal = {normal_grid[i][j][0], normal_grid[i][j][1], normal_grid[i][j][2], 0.0f};
        sceVu0ApplyMatrix(local_normal, inverse, world_normal);
        std::memcpy(vertex.position, local_point, sizeof(vertex.position));
        std::memcpy(vertex.normal, local_normal, sizeof(vertex.normal));
        vertex.uv[0] = texture_coord[i][j][0] * su;
        vertex.uv[1] = texture_coord[i][j][1] * sv;
        vertex.color[0] = vertex.color[1] = vertex.color[2] = vertex.color[3] = 0x80;
        visual.vertices.push_back(vertex);
    };

    for (int i = 0; i < num_i - 1; i++) {
        auto first = static_cast<uint32_t>(visual.vertices.size());
        for (int j = 0; j < num_j; j++) {
            if (polygon_divide[i] != 0) {
                add(i + 1, j);
                add(i, j);
            } else {
                add(i, j);
                add(i + 1, j);
            }
        }
        Draw3DStripToList(visual.indices, first, static_cast<uint32_t>(num_j * 2));
    }
    strip.index_count = static_cast<uint32_t>(visual.indices.size());
    visual.strips.push_back(strip);
    return kDraw3DBlockQuads;
}

int CCloth::DrawVu1(u_int * /*packet*/, float (* /*matrix*/)[4], RenderInfo *info, VU1_PROGRAM program,
                    u_long128 * /*draw_state*/, int /*unknown1*/, int /*unknown2*/) {
    CreateVUData(vu_block[DBuffID]);
    vu_data = vu_block[DBuffID];
    if (const Draw3DVisual *visual = Draw3DFindVisual(vu_data)) {
        sceVu0FMATRIX model;
        ClothDrawMatrix(frame, model);
        Draw3DIdentityScope identity(this, kClothResetDistance, true);
        Draw3DDrawVisual(*visual, model, *info, program);
    }
    return 0;
}

int CCloth::DrawVu1(sceVif1Packet * /*packet*/, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                    u_long128 *draw_state, int unknown1, int unknown2) {
    return CCloth::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

void CCloth::Step(int step) {
    if (num_i <= 0 || num_i > 16 || num_j <= 0 || num_j > 16) {
        return;
    }

    if (step < 0) {
        Clear();
        return;
    }

    sceVu0FVECTOR root;
    sceVu0FVECTOR delta = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FMATRIX matrix = {};
    sceVu0FVECTOR scratch = {};

    // The simulation works on a copy of the grid; point is only updated once the step completes.
    auto work = std::make_unique<sceVu0FVECTOR[]>(static_cast<std::size_t>(num_i) * static_cast<std::size_t>(num_j));
    auto work_at = [&](int row, int column) -> float * {
        return work[(static_cast<std::size_t>(row) * static_cast<std::size_t>(num_j)) + static_cast<std::size_t>(column)];
    };
    auto  stretch_params = std::make_unique<sceVu0FVECTOR[]>(static_cast<std::size_t>(num_j));
    float hit_point[4] = {};

    bool        reset = false;
    const float ground = floor_y;

    for (int i = 0; i < num_i; i++) {
        for (int j = 0; j < num_j; j++) {
            vu0::Copy(work_at(i, j), point[i][j]);
        }
    }

    if (frame != nullptr) {
        frame->GetLWMatrix(matrix);
        sceVu0ApplyMatrix(root, matrix, this->position);
        sceVu0SubVector(delta, root, last_position);

        if (vuabs(delta) > 10.0f || stop != 0) {
            reset = true;
        } else {
            delta[0] *= follow[0];
            delta[1] *= follow[1];
            delta[2] *= follow[2];
        }

        last_position[0] = root[0];
        last_position[1] = root[1];
        last_position[2] = root[2];
    }

    float stiffness_held[4];
    vu0::Copy(stiffness_held, stiffness);

    for (int j = 0; j < num_j; j++) {
        for (int i = 0; i < num_i; i++) {
            float *vertex = work_at(i, j);
            float *velocity = speed[i][j];

            if (reset) {
                vertex[0] += delta[0];
                vertex[1] += delta[1];
                vertex[2] += delta[2];
            } else if (frame != nullptr) {
                Follow(vertex, velocity, world_home[i][j], home[i][j], delta, matrix, stiffness_held);
            } else {
                vertex[0] += velocity[0];
                vertex[1] += velocity[1];
                vertex[2] += velocity[2];
            }
        }
    }

    scratch[1] = 0.5f;
    scratch[2] = 0.5f;

    for (int j = 0; j < num_j - 1; j++) {
        stretch_params[j][1] = 0.3f + 0.2f * (static_cast<float>(j) / static_cast<float>(num_j));
        stretch_params[j][2] = 1.0f - stretch_params[j][1];
    }

    for (int pass = 0; pass < 4 && !reset; pass++) {
        for (int j = 0; j < num_j - 1; j++) {
            for (int i = 0; i < num_i; i++) {
                if (i > 1) {
                    scratch[0] = 2.0f * rest[i][j][0];
                    StretchBind2(work_at(i, j), work_at(i - 2, j), scratch);
                } else {
                    scratch[0] = 0.0f;
                }

                stretch_params[j][0] = rest[i][j][1];
                StretchBind2(work_at(i, j), work_at(i, j + 1), stretch_params[j]);
            }
        }

        for (int row = 0; row < num_i; row++) {
            if (frame == nullptr) {
                sceVu0CopyVector(work_at(row, 0), home[row][0]);
            } else {
                vu0::Copy(work_at(row, 0), world_home[row][0]);
            }
        }
    }

    if (bound != nullptr) {
        bound->UpDate();
    }

    for (int j = 1; j < num_j; j++) {
        for (int i = 0; i < num_i; i++) {
            bool          touched = false;
            float         hit_count = 0.0f;
            sceVu0FVECTOR hit_sum = {};
            float         friction = 0.0f;

            for (CBound *box = bound; box != nullptr; box = box->next) {
                if ((mask[i][j] & (1 << box->mask_bit)) != 0 && box->InCheck(work_at(i, j), hit_point) != 0) {
                    hit_count += 1.0f;
                    sceVu0AddVector(hit_sum, hit_sum, hit_point);
                    friction += box->friction;
                    touched = true;
                }
            }

            if (touched) {
                work_at(i, j)[0] = hit_sum[0] / hit_count;
                work_at(i, j)[1] = hit_sum[1] / hit_count;
                work_at(i, j)[2] = hit_sum[2] / hit_count;
                rest[i][j][3] = friction / hit_count;
            } else {
                rest[i][j][3] = -1.0f;
            }
        }
    }

    sceVu0FVECTOR wind_force = {0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < num_i; i++) {
        if (wind != nullptr) {
            static_cast<CWind *>(wind)->GetWindNoise(wind_force);
            sceVu0ScaleVector(wind_force, wind_force, wind_effect);
        }

        for (int j = 0; j < num_j; j++) {
            speed[i][j][0] += gravity[0] + (work_at(i, j)[0] - last[i][j][0]);
            speed[i][j][1] += gravity[1] + (work_at(i, j)[1] - last[i][j][1]);
            speed[i][j][2] += gravity[2] + (work_at(i, j)[2] - last[i][j][2]);
            sceVu0CopyVector(last[i][j], work_at(i, j));
            sceVu0Normalize(scratch, speed[i][j]);
            float facing = sceVu0InnerProduct(scratch, normal_grid[i][j]);

            if (facing < 0.0f) {
                facing *= -1.0f;
            }

            const float damping = std::min(0.6f + (1.0f - facing), 1.0f);

            speed[i][j][0] += facing * wind_force[0];
            speed[i][j][1] += facing * wind_force[1];
            speed[i][j][2] += facing * wind_force[2];
            speed[i][j][0] *= damping;
            speed[i][j][1] *= damping;
            speed[i][j][2] *= damping;

            if (reset) {
                speed[i][j][0] = 0.0f;
                speed[i][j][1] = 0.0f;
                speed[i][j][2] = 0.0f;
            } else {
                if (rest[i][j][3] > 0.0f) {
                    speed[i][j][0] *= rest[i][j][3];
                    speed[i][j][1] *= rest[i][j][3];
                    speed[i][j][2] *= rest[i][j][3];
                }

                if (work_at(i, j)[1] <= ground && floor_on != 0) {
                    work_at(i, j)[1] = ground;
                    speed[i][j][0] *= 0.4f;
                    speed[i][j][2] *= 0.4f;
                }
            }
        }
    }

    sceVu0FVECTOR edge[4];
    sceVu0FVECTOR normal = {};
    sceVu0FVECTOR cross[4];

    for (int j = 0; j < num_j; j++) {
        for (int i = 0; i < num_i; i++) {
            const int next_i = std::min(i + 1, num_i - 1);
            const int prev_i = std::max(i - 1, 0);
            const int prev_j = std::max(j - 1, 0);
            const int next_j = std::min(j + 1, num_j - 1);

            sceVu0SubVector(edge[0], work_at(next_i, j), work_at(i, j));
            sceVu0SubVector(edge[1], work_at(prev_i, j), work_at(i, j));
            sceVu0SubVector(edge[2], work_at(i, prev_j), work_at(i, j));
            sceVu0SubVector(edge[3], work_at(i, next_j), work_at(i, j));
            sceVu0OuterProduct(cross[0], edge[3], edge[0]);
            sceVu0OuterProduct(cross[1], edge[1], edge[3]);
            sceVu0OuterProduct(cross[2], edge[2], edge[1]);
            sceVu0OuterProduct(cross[3], edge[0], edge[2]);
            sceVu0AddVector(normal, cross[0], cross[1]);
            sceVu0AddVector(normal, normal, cross[2]);
            sceVu0AddVector(normal, normal, cross[3]);
            sceVu0ScaleVector(normal, normal, normal_scale);
            sceVu0Normalize(normal_grid[i][j], normal);
        }
    }

    for (int i = 0; i < num_i; i++) {
        for (int j = 0; j < num_j; j++) {
            vu0::Copy(point[i][j], work_at(i, j));
        }
    }
}
