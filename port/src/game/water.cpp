#include "water.hpp"

#include <libpkt.h>

#include <algorithm>
#include <cstddef>

#include "boxvu0.hpp"
#include "dataalloc.hpp"
#include "mglib.hpp"
#include "texture.hpp"

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

int CWater::CheckClip() {
    sceVu0FVECTOR box[2];
    sceVu0FVECTOR corner[4];

    frame.GetWorldPosition(corner[0], vertex[0]);
    frame.GetWorldPosition(corner[1], vertex[1]);
    frame.GetWorldPosition(corner[2], vertex[2]);
    frame.GetWorldPosition(corner[3], vertex[3]);
    VectorMaxMin(box[0], box[1], corner[0], corner[1], corner[2], corner[3]);
    return MGClipBox(reinterpret_cast<CBoxVu0 *>(box));
}

void CWater::Hamon() {
    // The two buffers hold this step and the last; the older one is written.
    const bool drawing_a = height == height_a;
    float     *source = drawing_a ? height_a : height_b;
    float     *target = drawing_a ? height_b : height_a;

    height = target;

    float speed = wave_speed * wave_speed;
    float centre = 2.0f * (1.0f - 2.0f * speed);
    float loss = damping;

    for (int row = 1; row < rows - 1; row++) {
        for (int column = 1; column < columns - 1; column++) {
            int   index = column + row * columns;
            float cell = source[index];
            float around = source[index - columns] + (source[index - 1] + source[index + 1] + source[index + columns]);
            around *= speed;
            float centre_term = centre * cell;
            float previous = target[index];
            target[index] = around + (centre_term - previous) - loss * (cell - previous);
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
    row = std::max(row, 1);
    column = std::max(column, 1);
    row = std::min(row, rows - 2);
    column = std::min(column, columns - 2);

    height[column + row * columns] += height_change;
}

void CWater::SetSize(int row_count, int column_count, CDataAlloc2<1> *arena) {
    if (arena == nullptr) {
        arena = &WaterData;
    }

    int quads = ((row_count * column_count) >> 2) + 1;
    height_a = reinterpret_cast<float *>(arena->Alloc(quads));
    height_b = reinterpret_cast<float *>(arena->Alloc(quads));
    rows = row_count;
    columns = column_count;

    for (int cell = 0; cell < rows * columns; cell++) {
        height_b[cell] = 0.0f;
        height_a[cell] = 0.0f;
    }

    height = height_a;

    RenderInfo info;
    tags_built = 0;
    arena->Align64();
    packet[1] = reinterpret_cast<u_int *>(&arena->base[static_cast<std::ptrdiff_t>(arena->used) * 16]);
    int packet_quads = CreateVUData(packet[1], &info);
    arena->Alloc(packet_quads);
    packet[2] = reinterpret_cast<u_int *>(arena->Alloc64(packet_quads));
    CreateVUData(packet[2], &info);
    tags_built = 1;
}

CWater::CWater() : rows(0), columns(0), wave_speed(0.1f), damping(0.015f), height_scale(0.0f), distortion(0.0f) {
    packet[2] = nullptr;
    packet[1] = nullptr;
    packet[0] = nullptr;
    color[0] = 0x80;
    color[1] = 0x80;
    color[2] = 0x80;
    color[3] = 0x80;
}
