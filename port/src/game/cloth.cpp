#include "stubs/cloth.hpp"

#include "cloth.hpp"

#include <cstring>

#include "bound.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "wind.hpp"

/**
 * Draws the simulated cloth through a temporary world-space frame.
 */
void CCloth::Draw() {
    CFrameVu1 draw_frame;
    int       i;

    draw_frame.SetVisual(this);
    draw_frame.max[0] = 10000.0f;
    draw_frame.max[1] = 10000.0f;
    draw_frame.max[2] = 10000.0f;
    draw_frame.min[0] = -10000.0f;
    draw_frame.min[1] = -10000.0f;
    draw_frame.min[2] = -10000.0f;

    for (i = 0; i < 8; i++) {
        float *corner[2];

        corner[0] = draw_frame.min;
        corner[1] = draw_frame.max;
        draw_frame.corner[i][3] = 1.0f;
        draw_frame.corner[i][0] = corner[(i & 1) != 0][0];
        draw_frame.corner[i][1] = corner[(i & 2) != 0][1];
        draw_frame.corner[i][2] = corner[(i & 4) != 0][2];
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

/**
 * Puts every cloth vertex back where its frame says it rests.
 *
 * @mangled Clear__6CClothFv
 * @address 0x13B7A0
 * @size 0x100
 */
void CCloth::Clear() {
    sceVu0FMATRIX matrix;
    int           i;
    int           j;

    if (frame != NULL) {
        frame->GetLWMatrix(matrix);
    }

    sceVu0ApplyMatrix(last_position, matrix, position);

    for (j = 0; j < num_j; j++) {
        for (i = 0; i < num_i; i++) {
            float *velocity = speed[i][j];
            float *vertex = point[i][j];
            velocity[0] = 0.0f;
            velocity[1] = 0.0f;
            velocity[2] = 0.0f;

            if (frame != NULL) {
                sceVu0ApplyMatrix(point[i][j], matrix, home[i][j]);
                sceVu0CopyVector(last[i][j], point[i][j]);
            }
        }
    }
}
/**
 * Draws the cloth through the vector unit.
 *
 * @mangled DrawVu1__6CClothFPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
 * @address 0x13C470
 * @size 0xC0
 */

/**
 * Draws the cloth into a VIF packet.
 *
 * @mangled DrawVu1__6CClothFP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
 * @address 0x13C530
 * @size 0xC0
 */

/**
 * Builds the packet that draws the cloth and gives back its size.
 *
 * @mangled CreateVUData__6CClothFPUi
 * @address 0x13C5F0
 * @size 0x3B8
 */
int SetMaterial(u_int *packet, MDT_MATERIAL *material);
int SetTEX0(u_int *packet, u_long tex0, u_long tex1);

/**
 * Gives the cloth its default grid size, stiffness and gravity.
 *
 * @mangled InitParam__6CClothFv
 * @address 0x13C9B0
 * @size 0x1C0
 */
void CCloth::InitParam() {
    float side_follow = 0.9f;
    num_i = 16;
    num_j = 16;
    pitch = 1.0f;
    init_20 = 0;
    vu_block[1] = NULL;
    vu_block[0] = NULL;
    frame = NULL;
    init_40 = 0;
    flags = 0;
    wind = NULL;
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

    bound = NULL;
    material.texture[0] = 0;
}

/**
 * Constructs a cloth grid with the requested dimensions and spacing.
 */
CCloth::CCloth(int grid_i, int grid_j, float grid_pitch) {
    InitParam();
    num_i = grid_i;
    num_j = grid_j;
    pitch = grid_pitch;
}

/**
 * Takes the cloth's vertex storage out of an arena.
 *
 * @mangled Initialize__6CClothFP14CDataAlloc2_1_
 * @address 0x13CBF0
 * @size 0x458
 */
void CCloth::Initialize(CDataAlloc2<1> *alloc) {
    int i;
    int j;

    if (alloc == NULL) {
        alloc = &VisualData;
    }

    alloc->Align64();
    vu_block[0] = (u_int *) (alloc->base + alloc->used * 16);
    vu_size = CreateVUData(vu_block[0]);
    alloc->Alloc(vu_size);
    vu_block[1] = (u_int *) alloc->Alloc64(vu_size);
    vu_size = CreateVUData(vu_block[1]);

    for (i = 0; i < num_i; i++) {
        polygon_divide[i] = 0;

        for (j = 0; j < num_j; j++) {
            sceVu0FVECTOR zero = {0.0f, 0.0f, 0.0f, 0.0f};
            sceVu0CopyVector(last[j][i], zero);
            sceVu0CopyVector(speed[j][i], zero);
            sceVu0CopyVector(normal_grid[j][i], zero);
            sceVu0CopyVector(rest[j][i], zero);
            point[j][i][3] = 1.0f;
            last[j][i][3] = 1.0f;
        }
    }

    for (i = 0; i < num_j; i++) {
        for (j = 0; j < num_i; j++) {
            sceVu0CopyVector(last[j][i], point[j][i]);
            sceVu0CopyVector(home[j][i], point[j][i]);
        }
    }

    sceVu0FVECTOR centre = {0.0f, 0.0f, 0.0f, 0.0f};

    for (i = 0; i < num_i; i++) {
        sceVu0AddVector(centre, centre, home[0][i]);
    }

    position[0] = centre[0] / (float) num_i;
    position[1] = centre[1] / (float) num_i;
    position[2] = centre[2] / (float) num_i;
    position[3] = 1.0f;

    // Record each vertex's rest distance to its neighbours down and across the grid.
    sceVu0FVECTOR edge;

    for (i = 0; i < num_j; i++) {
        for (j = 0; j < num_i; j++) {
            int next_j = j + 1;
            int prev_j = j - 1;
            int prev_i = i - 1;
            int next_i = i + 1;

            if (prev_j > 0) {
                sceVu0SubVector(edge, point[j][i], point[prev_j][i]);
            } else if (next_j >= num_i) {
                sceVu0SubVector(edge, point[j][i], point[prev_j][i]);
            } else {
                sceVu0SubVector(edge, point[j][i], point[next_j][i]);
            }

            rest[j][i][0] = vuabs(edge);

            if (next_i >= num_j) {
                sceVu0SubVector(edge, point[j][i], point[j][prev_i]);
            } else if (prev_i < 0) {
                sceVu0SubVector(edge, point[j][i], point[j][next_i]);
            } else {
                sceVu0SubVector(edge, point[j][i], point[j][next_i]);
            }

            rest[j][i][1] = vuabs(edge);
        }
    }
}

/**
 * Builds the cloth's grid from a model's mesh and takes its storage out of an arena.
 *
 * @mangled Initialize__6CClothFP10MDT_HEADERP14CDataAlloc2_1_
 * @address 0x13D050
 * @size 0x200
 */
void CCloth::Initialize(MDT_HEADER *header, CDataAlloc2<1> *alloc) {
    int *strip;

    if (header == NULL) {
        return;
    }

    sceVu0FVECTOR *vertices = (sceVu0FVECTOR *) ((u_char *) header + header->vertex_ofs);
    sceVu0FVECTOR *uvs = (sceVu0FVECTOR *) ((u_char *) header + header->uv[1]);
    MDT_MATERIAL  *materials = (MDT_MATERIAL *) ((u_char *) header + header->info_ofs);
    int           *mesh = (int *) ((u_char *) header + header->mesh_ofs);
    strip = mesh + 4;
    int *strips = strip;
    int  strip_count = mesh[2];
    int  match_count = 0;

    for (int i = 0; i < num_i; i++) {
        for (int j = 0; j < num_j; j++) {
            int vertex_no = j + i * num_j;
            sceVu0CopyVector(point[i][j], vertices[vertex_no]);
            strip = strips;

            for (int strip_no = 0; strip_no < strip_count; strip_no++) {
                int corner_count = strip[1];
                memcpy(&material, &materials[strip[2]], sizeof(MDT_MATERIAL));
                strip += 3;
                int *corner = strip;

                for (int corner_no = 0; corner_no < corner_count; corner_no++) {
                    if (corner[0] == vertex_no) {
                        match_count++;
                        sceVu0CopyVector(texture_coord[i][j], uvs[corner[2]]);
                    }

                    corner += 3;
                }

                strip += corner_count * 3;
            }
        }
    }

    Initialize(alloc);
}
