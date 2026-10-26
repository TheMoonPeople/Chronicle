#include "stubs/cloth.hpp"

#include <cstddef>

#include "bound.hpp"
#include "cloth.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "wind.hpp"

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
