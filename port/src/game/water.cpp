#include "stubs/water.hpp"
#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "water.hpp"

#include <libpkt.h>

#include "boxvu0.hpp"
#include "dataalloc.hpp"
#include "mglib.hpp"
#include "texture.hpp"

int SetTEX0(u_int *packet, u_long tex0, u_long tex1);
void CWater::SetParam(float speed, float damping_rate, float scale, float shift) {
    wave_speed = speed;
    damping = damping_rate;
    height_scale = scale;
    distortion = shift;
}

void CWater::SetColor(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    color[0] = red;
    color[1] = green;
    color[2] = blue;
    color[3] = alpha;
}

extern "C" int DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(CWater *water, RenderInfo *info, sceVif1Packet *draw_packet, void *parent_info);

int CWater::CheckClip() {
    sceVu0FVECTOR box[2];
    sceVu0FVECTOR corner[4];

    frame.GetWorldPosition(corner[0], vertex[0]);
    frame.GetWorldPosition(corner[1], vertex[1]);
    frame.GetWorldPosition(corner[2], vertex[2]);
    frame.GetWorldPosition(corner[3], vertex[3]);
    VectorMaxMin(box[0], box[1], corner[0], corner[1], corner[2], corner[3]);
    return MGClipBox((CBoxVu0 *) box);
}

void CWater::Hamon() {
    int    i;
    int    j;
    float *source;
    float *target;

    // The two buffers hold this step and the last; the older one is written.
    if (height == height_a) {
        source = height_a;
        target = height_b;
    } else {
        target = height_a;
        source = height_b;
    }

    height = target;

    float speed = wave_speed * wave_speed;
    float centre = 2.0f * (1.0f - 2.0f * speed);
    float loss = damping;

    for (i = 1; i < rows - 1; i++) {
        for (j = 1; j < columns - 1; j++) {
            float *cell = &source[j + i * columns];
            float *out = &target[j + i * columns];
            float  around = *(cell - columns) + (cell[-1] + cell[1] + *(cell + columns));
            around *= speed;
            float centre_term = centre * *cell;
            float previous = *out;
            *out = around + (centre_term - previous) - loss * (*cell - previous);
        }
    }
}

void CWater::SetVertex(float *corner0, float *corner1, float *corner2, float *corner3) {
    sceVu0CopyVector(vertex[0], corner0);
    sceVu0CopyVector(vertex[1], corner1);
    sceVu0CopyVector(vertex[2], corner2);
    sceVu0CopyVector(vertex[3], corner3);
}

void CWater::Shake(int row, int column, float height_change) {
    row %= rows;
    column %= columns;

    // The edge cells hold the surface still, so a ripple starts inside them.
    if (row <= 0) {
        row = 1;
    }

    if (column <= 0) {
        column = 1;
    }

    if (row > rows - 2) {
        row = rows - 2;
    }

    if (column > columns - 2) {
        column = columns - 2;
    }

    float *cell = &height[column];
    cell[row * columns] += height_change;
}

void CWater::SetSize(int row_count, int column_count, CDataAlloc2<1> *arena) {
    if (arena == NULL) {
        arena = &WaterData;
    }

    int quads = ((row_count * column_count) >> 2) + 1;
    height_a = (float *) arena->Alloc(quads);
    height_b = (float *) arena->Alloc(quads);
    rows = row_count;
    columns = column_count;

    for (int i = 0; i < rows * columns; i++) {
        height_b[i] = 0.0f;
        height_a[i] = 0.0f;
    }

    height = height_a;

    RenderInfo info;
    tags_built = 0;
    arena->Align64();
    packet[1] = (u_int *) (arena->base + arena->used * 16);
    int packet_quads = CreateVUData(packet[1], &info);
    arena->Alloc(packet_quads);
    packet[2] = (u_int *) arena->Alloc64(packet_quads);
    CreateVUData(packet[2], &info);
    tags_built = 1;
}

CWater::CWater() {
    rows = 0;
    columns = 0;
    packet[2] = NULL;
    packet[1] = NULL;
    packet[0] = NULL;
    color[0] = 0x80;
    color[1] = 0x80;
    color[2] = 0x80;
    color[3] = 0x80;
    wave_speed = 0.1f;
    damping = 0.015f;
    height_scale = 0.0f;
    distortion = 0.0f;
}
