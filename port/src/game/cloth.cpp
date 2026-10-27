#include "cloth.hpp"

#include <libvu0.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>

#include "bound.hpp"
#include "chararead.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mglib.hpp"
#include "render/draw3d.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "vu0_ops.hpp"
#include "wind.hpp"

namespace dc::game {

using namespace dc::render;

void CCloth::Draw() {
    CFrameVu1 draw_frame;

    draw_frame.SetVisual(this);
    draw_frame.max[0] = 10000.0f;
    draw_frame.max[1] = 10000.0f;
    draw_frame.max[2] = 10000.0f;
    draw_frame.min[0] = -10000.0f;
    draw_frame.min[1] = -10000.0f;
    draw_frame.min[2] = -10000.0f;

    for (int corner_no = 0; corner_no < 8; corner_no++) {
        const float *bounds[2] = {draw_frame.min, draw_frame.max};

        draw_frame.corner[corner_no][3] = 1.0f;
        draw_frame.corner[corner_no][0] = bounds[(corner_no & 1) != 0][0];
        draw_frame.corner[corner_no][1] = bounds[(corner_no & 2) != 0][1];
        draw_frame.corner[corner_no][2] = bounds[(corner_no & 4) != 0][2];
    }

    CFrameAttr attr;

    attr.fog_enable = false;
    attr.program_option = 0;
    attr.clip_enable = false;
    float origin = 0.0f;

    draw_frame.SetAttr(attr, 1, 0);
    draw_frame.SetPosition(origin, origin, origin);
    MGDraw(&draw_frame);
}

void CCloth::Clear() {
    sceVu0FMATRIX matrix;

    if (frame != nullptr) {
        frame->GetLWMatrix(matrix);
    }

    sceVu0ApplyMatrix(last_position, matrix, position);

    for (int j = 0; j < num_j; j++) {
        for (int i = 0; i < num_i; i++) {
            float *velocity = speed[i][j];
            velocity[0] = 0.0f;
            velocity[1] = 0.0f;
            velocity[2] = 0.0f;

            if (frame != nullptr) {
                sceVu0ApplyMatrix(point[i][j], matrix, home[i][j]);
                sceVu0CopyVector(last[i][j], point[i][j]);
            }
        }
    }
}

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

void CCloth::InitParam() {
    float side_follow = 0.9f;
    num_i = 16;
    num_j = 16;
    pitch = 1.0f;
    init_20 = 0;
    vu_block[1] = nullptr;
    vu_block[0] = nullptr;
    frame = nullptr;
    init_40 = 0;
    flags = 0;
    wind = nullptr;
    wind_effect = 1.0f;
    floor_y = 0.0f;
    last_position[0] = 0.0f;
    last_position[1] = 0.0f;
    last_position[2] = 0.0f;
    stiffness[0] = 0.1f;
    stiffness[1] = 0.1f;
    stiffness[2] = 0.1f;
    follow[0] = side_follow;
    follow[1] = 0.4f;
    follow[2] = side_follow;
    gravity[0] = 0.0f;
    gravity[1] = -0.1f;
    gravity[2] = 0.0f;
    normal_scale = 1.0f;
    stop = false;
    floor_on = true;
    sceVu0FVECTOR zero = {0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < num_i; i++) {
        for (int j = 0; j < num_j; j++) {
            mask[i][j] = 0xFFFF;
            sceVu0CopyVector(home[i][j], zero);
            sceVu0CopyVector(point[i][j], zero);
            sceVu0CopyVector(last[i][j], zero);
            sceVu0CopyVector(rest[i][j], zero);
            sceVu0CopyVector(speed[i][j], zero);
            sceVu0CopyVector(normal_grid[i][j], zero);
            sceVu0CopyVector(texture_coord[i][j], zero);
        }
    }

    bound = nullptr;
    material.texture[0] = 0;
}

CCloth::CCloth(int grid_i, int grid_j, float grid_pitch) {
    InitParam();
    num_i = grid_i;
    num_j = grid_j;
    pitch = grid_pitch;
}

void CCloth::Initialize(CDataAlloc2<1> *alloc) {
    if (alloc == nullptr) {
        alloc = &VisualData;
    }

    alloc->Align64();
    vu_block[0] = reinterpret_cast<u_int *>(&alloc->base[static_cast<std::ptrdiff_t>(alloc->used) * 16]);
    vu_size = CreateVUData(vu_block[0]);
    alloc->Alloc(vu_size);
    vu_block[1] = reinterpret_cast<u_int *>(alloc->Alloc64(vu_size));
    vu_size = CreateVUData(vu_block[1]);

    for (int i = 0; i < num_i; i++) {
        polygon_divide[i] = 0;

        for (int j = 0; j < num_j; j++) {
            sceVu0FVECTOR zero = {0.0f, 0.0f, 0.0f, 0.0f};
            sceVu0CopyVector(last[j][i], zero);
            sceVu0CopyVector(speed[j][i], zero);
            sceVu0CopyVector(normal_grid[j][i], zero);
            sceVu0CopyVector(rest[j][i], zero);
            point[j][i][3] = 1.0f;
            last[j][i][3] = 1.0f;
        }
    }

    for (int i = 0; i < num_j; i++) {
        for (int j = 0; j < num_i; j++) {
            sceVu0CopyVector(last[j][i], point[j][i]);
            sceVu0CopyVector(home[j][i], point[j][i]);
        }
    }

    sceVu0FVECTOR centre = {0.0f, 0.0f, 0.0f, 0.0f};

    for (int i = 0; i < num_i; i++) {
        sceVu0AddVector(centre, centre, home[0][i]);
    }

    position[0] = centre[0] / static_cast<float>(num_i);
    position[1] = centre[1] / static_cast<float>(num_i);
    position[2] = centre[2] / static_cast<float>(num_i);
    position[3] = 1.0f;

    // Record each vertex's rest distance to its neighbours down and across the grid.
    sceVu0FVECTOR edge;

    for (int i = 0; i < num_j; i++) {
        for (int j = 0; j < num_i; j++) {
            int next_j = j + 1;
            int prev_j = j - 1;
            int prev_i = i - 1;
            int next_i = i + 1;

            if (prev_j > 0 || next_j >= num_i) {
                sceVu0SubVector(edge, point[j][i], point[prev_j][i]);
            } else {
                sceVu0SubVector(edge, point[j][i], point[next_j][i]);
            }

            rest[j][i][0] = vuabs(edge);

            if (next_i >= num_j) {
                sceVu0SubVector(edge, point[j][i], point[j][prev_i]);
            } else {
                sceVu0SubVector(edge, point[j][i], point[j][next_i]);
            }

            rest[j][i][1] = vuabs(edge);
        }
    }
}

void CCloth::Initialize(MDT_HEADER *header, CDataAlloc2<1> *alloc) {
    if (header == nullptr) {
        return;
    }

    auto *resource = reinterpret_cast<u_char *>(header);
    auto *vertices = reinterpret_cast<sceVu0FVECTOR *>(&resource[header->vertex_ofs]);
    auto *uvs = reinterpret_cast<sceVu0FVECTOR *>(&resource[header->uv[1]]);
    auto *materials = reinterpret_cast<MDT_MATERIAL *>(&resource[header->info_ofs]);
    auto *mesh = reinterpret_cast<int *>(&resource[header->mesh_ofs]);
    // Each strip: a three-word header (word 1 corner count, word 2 material), then three-word corners
    // (word 0 vertex, word 2 texture coordinate).
    const int *strips = &mesh[4];
    int        strip_count = mesh[2];

    for (int i = 0; i < num_i; i++) {
        for (int j = 0; j < num_j; j++) {
            int vertex_no = j + i * num_j;
            sceVu0CopyVector(point[i][j], vertices[vertex_no]);
            int strip_pos = 0;

            for (int strip_no = 0; strip_no < strip_count; strip_no++) {
                int corner_count = strips[strip_pos + 1];
                material = materials[strips[strip_pos + 2]];
                strip_pos += 3;

                for (int corner_no = 0; corner_no < corner_count; corner_no++) {
                    const int *corner = &strips[strip_pos + corner_no * 3];

                    if (corner[0] == vertex_no) {
                        sceVu0CopyVector(texture_coord[i][j], uvs[corner[2]]);
                    }
                }

                strip_pos += corner_count * 3;
            }
        }
    }

    Initialize(alloc);
}

} // namespace dc::game
