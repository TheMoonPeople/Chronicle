#include "water.hpp"

#include <libpkt.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <optional>
#include <vector>

#include "boxvu0.hpp"
#include "dataalloc.hpp"
#include "mglib.hpp"
#include "render/draw3d.hpp"
#include "render/screen.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "vu0_ops.hpp"

namespace dc::game {

using namespace dc::render;

namespace {

// pretest parks the cell matrix and step in VU0 registers for the Trans_AddCell calls that follow.
float g_cell_matrix[4][4];
float g_cell_step[4];

void pretest(float matrix[4][4], float *translation) {
    std::memcpy(g_cell_matrix, matrix, sizeof(g_cell_matrix));
    vu0::Copy(g_cell_step, translation);
}

void Trans_AddCell(float *output, float *position) {
    float source[4];
    float result[4];
    vu0::Copy(source, position);
    vu0::Apply(result, g_cell_matrix, source);
    source[0] = source[0] + g_cell_step[0];
    source[2] = source[2] + g_cell_step[2];
    vu0::Copy(output, result);
    vu0::Copy(position, source);
}

} // namespace

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

// The height field is drawn from model space with the frame's mvp. Each vertex samples the copy of
// the frame where the vertex itself lands on the screen, shifted by the ripple's slope (offset from
// the field's centre (320, 120)), through the copy's logical mapping: the copy is a frame target,
// so water past the logical frame samples the copy's own sides.

namespace {

constexpr float kFieldCentreX = 320.0f;
constexpr float kFieldCentreY = SCREEN_QUARTER_HEIGHT_F;

// The game copies the frame into "water" (dungeon) or "water_buff" (town, title) with MGMoveImage
// and then draws with "work"'s TEX0, which retail's VRAM layout puts at the copy's address. The
// port's registry keeps them apart, so the copy the game made last wins and "work" is the
// fallback.
PortTextureRef FrameCopy() {
    gfx::TextureHandle copy = Draw3DLastFrameCopy();
    if (std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(copy); copy != gfx::kNullTexture && info) {
        PortTextureRef ref;
        ref.binding.texture = copy;
        ref.width = info->width;
        ref.height = info->height;
        ref.valid = true;
        return ref;
    }
    char      name[] = "work";
    CTexture *work = TexManager.GetTexture(TexManager.GetTextureHandle(name, -1));
    return Draw3DResolveTex0(work->tex0);
}

// Retail's surface samples the copy of the frame wherever the ripple's slope sends it, and the copy
// holds everything drawn so far: a bank or a character standing in front of the water is pulled
// into it. The copy is also a field, half the frame's rows. Here the surface is refracted from the
// frame at its own resolution, and only from where the water itself shows:
//   - Draw3DFramePicture is the frame as the game's copy last saw it;
//   - "water visible" takes the surface drawn flat (each pixel sampling itself) under the frame's
//     depth test, so it holds the frame where water shows and transparent black elsewhere;
//   - "water refracted" takes the flat surface again and, over it, the rippled samples of "water
//     visible" by their coverage, so a sample that lands on something in front of the water gives
//     way to the pixel's own colour;
//   - the frame takes the surface with the game's colour and blend, sampling "water refracted"
//     flat.
// The three share the frame's depth buffer and pixels, so every sample is taken where the vertex
// lands (gfx::kMeshScreenUv), under a display render's camera too.
struct Refraction {
    gfx::TextureHandle frame = gfx::kNullTexture;
    gfx::TextureHandle visible = gfx::kNullTexture;
    gfx::TextureHandle refracted = gfx::kNullTexture;
};

bool PrepareRefraction(Refraction &refraction) {
    static gfx::TextureHandle pictured = gfx::kNullTexture;
    static unsigned           copies = 0;

    if (gfx::CurrentRenderTarget() != gfx::kMainTarget) {
        return false;
    }
    refraction.frame = Draw3DFramePicture();
    if (refraction.frame == gfx::kNullTexture) {
        return false;
    }
    auto width = static_cast<uint32_t>(kScreenWidth);
    auto height = static_cast<uint32_t>(kScreenHeight);
    refraction.visible = gfx::NamedRenderTarget("water visible", width, height, true, true);
    refraction.refracted = gfx::NamedRenderTarget("water refracted", width, height, false, true);
    if (refraction.visible == gfx::kNullTexture || refraction.refracted == gfx::kNullTexture) {
        return false;
    }
    if (pictured != refraction.frame || copies != Draw3DFrameCopies()) {
        const uint8_t clear[4] = {};
        gfx::SetRenderTarget(refraction.visible);
        gfx::Clear(true, clear, false, 0.0f);
        gfx::SetRenderTarget(refraction.refracted);
        gfx::Clear(true, clear, false, 0.0f);
        gfx::SetRenderTarget(gfx::kMainTarget);
        pictured = refraction.frame;
        copies = Draw3DFrameCopies();
    }
    return true;
}

// The surface's vertices with the ripple's shift as an offset from where each lands, in the
// frame's normalised units; the shift down is in field rows, two to a row of the frame.
std::vector<gfx::Vertex3D> RippledVertices(const CWater &water, const std::vector<gfx::Vertex3D> &vertices) {
    gfx::LogicalMapping        mapping = gfx::GetLogicalMapping(gfx::kMainTarget);
    float                      across = mapping.pixel_width ? mapping.scale_x / static_cast<float>(mapping.pixel_width) : 0.0f;
    float                      down = mapping.pixel_height ? 2.0f * mapping.scale_y / static_cast<float>(mapping.pixel_height) : 0.0f;
    std::vector<gfx::Vertex3D> rippled = vertices;
    const float               *heights = water.height;
    for (int row = 0; row < water.rows; row++) {
        int row_start = row * water.columns;
        int above_start = row == 0 ? row_start : row_start - water.columns;
        for (int column = 0; column < water.columns; column++) {
            int            here = row_start + column;
            gfx::Vertex3D &vertex = rippled[static_cast<size_t>(here)];
            vertex.uv[0] = water.distortion * (heights[above_start + column] - heights[here]) * across;
            vertex.uv[1] = water.distortion * (heights[here] - heights[here + 1]) * down;
        }
    }
    return rippled;
}

void DrawRefracted(const CWater &water, const Refraction &refraction, const Draw3DVisual &visual,
                   gfx::MeshConstants constants, const gfx::MeshTransform &transform, const gfx::DrawState &state) {
    std::vector<gfx::Vertex3D> rippled = RippledVertices(water, visual.vertices);
    std::vector<gfx::Vertex3D> flat = rippled;
    for (gfx::Vertex3D &vertex : flat) {
        vertex.uv[0] = 0.0f;
        vertex.uv[1] = 0.0f;
    }

    gfx::MeshConstants plain = constants;
    plain.flags = gfx::kMeshScreenUv;
    constants.flags = gfx::kMeshVertexColor | gfx::kMeshScreenUv;

    gfx::DrawState copy = state;
    copy.blend = false;
    copy.depth_write = false;
    copy.texa_aem = false;
    copy.texa_ta0 = 0x80;
    gfx::DrawState over = copy;
    over.blend = true;

    gfx::TextureBinding binding;
    binding.filter = gfx::Filter::Linear;
    binding.wrap_u = gfx::Wrap::Clamp;
    binding.wrap_v = gfx::Wrap::Clamp;

    binding.texture = refraction.frame;
    gfx::SetRenderTarget(refraction.visible);
    gfx::DrawMeshImmediate(flat, visual.indices, plain, binding, copy, &transform);
    gfx::SetRenderTarget(refraction.refracted);
    gfx::DrawMeshImmediate(flat, visual.indices, plain, binding, copy, &transform);
    binding.texture = refraction.visible;
    over.alpha = {.a = 2, .b = 1, .c = 0, .d = 1, .fix = 0x80};
    gfx::DrawMeshImmediate(rippled, visual.indices, plain, binding, over, &transform);
    over.alpha = {.a = 0, .b = 2, .c = 2, .d = 1, .fix = 0x80};
    gfx::DrawMeshImmediate(rippled, visual.indices, plain, binding, over, &transform);

    gfx::SetRenderTarget(gfx::kMainTarget);
    gfx::DrawState surface = state;
    surface.texa_aem = false;
    surface.texa_ta0 = 0x80;
    binding.texture = refraction.refracted;
    gfx::DrawMeshImmediate(flat, visual.indices, constants, binding, surface, &transform);
}

} // namespace

int CWater::CreateVUData(unsigned int *output, RenderInfo *info) {
    if (output == nullptr || rows < 2 || columns < 2) {
        return kDraw3DBlockQuads;
    }
    Draw3DVisual &visual = Draw3DRegisterVisual(output, true);
    visual.vertex_colour = true;

    sceVu0FMATRIX local_to_world;
    sceVu0FMATRIX local_to_eye;
    frame.GetLWMatrix(local_to_world);
    Draw3DMul(local_to_eye, info->view_scaled, local_to_world);

    PortTextureRef      copy = FrameCopy();
    gfx::LogicalMapping texels = {.scale_x = 1.0f, .scale_y = 1.0f, .offset_x = 0.0f, .offset_y = 0.0f, .pixel_width = copy.width, .pixel_height = copy.height};
    if (copy.valid && gfx::GetTextureInfo(copy.binding.texture)) {
        texels = gfx::GetLogicalMapping(copy.binding.texture);
    }
    float inv_width = copy.valid && texels.pixel_width ? 1.0f / static_cast<float>(texels.pixel_width) : 0.0f;
    float inv_height = copy.valid && texels.pixel_height ? 1.0f / static_cast<float>(texels.pixel_height) : 0.0f;

    sceVu0FVECTOR row_step;
    sceVu0FVECTOR column_step;
    for (int axis = 0; axis < 3; axis++) {
        row_step[axis] = (vertex[1][axis] - vertex[0][axis]) / static_cast<float>(rows - 1);
        column_step[axis] = (vertex[2][axis] - vertex[0][axis]) / static_cast<float>(columns - 1);
    }
    row_step[1] = column_step[1] = 0.0f;

    uint8_t rgba[4] = {color[0], color[1], color[2], color[3]};
    float   row_f = 0.0f;
    for (int row = 0; row < rows; row++, row_f += 1.0f) {
        sceVu0FVECTOR position = {vertex[0][0] + row_f * row_step[0], vertex[0][1] + row_f * row_step[1],
                                  vertex[0][2] + row_f * row_step[2], 1.0f};
        int           row_start = row * columns;
        int           above_start = row == 0 ? row_start : row_start - columns;
        for (int column = 0; column < columns; column++) {
            int           here = row_start + column;
            int           above = above_start + column;
            gfx::Vertex3D out = {};
            std::memcpy(out.position, position, sizeof(out.position));
            std::memcpy(out.color, rgba, sizeof(rgba));

            sceVu0FVECTOR eye;
            sceVu0ApplyMatrix(eye, local_to_eye, position);
            float depth = eye[2] > 1.0f ? eye[2] : 1.0f;
            float screen_x = kFieldCentreX + info->scale[0] * eye[0] / depth;
            float screen_y = kFieldCentreY + info->scale[1] * eye[1] / depth * 0.5f;
            out.uv[0] = ((screen_x + distortion * (height[above] - height[here])) * texels.scale_x + texels.offset_x) * inv_width;
            out.uv[1] = ((screen_y + distortion * (height[here] - height[here + 1])) * texels.scale_y + texels.offset_y) * inv_height;
            visual.vertices.push_back(out);

            // Retail transforms the cell before lifting it, so each vertex carries the height of
            // the cell before it.
            position[0] += column_step[0];
            position[2] += column_step[2];
            position[1] = height[here] * height_scale;
        }
    }

    for (int row = 0; row < rows - 1; row++) {
        std::vector<uint32_t> strip;
        for (int column = 0; column < columns; column++) {
            strip.push_back(static_cast<uint32_t>(row * columns + column));
            strip.push_back(static_cast<uint32_t>((row + 1) * columns + column));
        }
        size_t first = visual.indices.size();
        Draw3DStripToList(visual.indices, 0, static_cast<uint32_t>(strip.size()));
        for (size_t index = first; index < visual.indices.size(); index++) {
            visual.indices[index] = strip[visual.indices[index]];
        }
    }

    Draw3DStrip strip;
    strip.index_count = static_cast<uint32_t>(visual.indices.size());
    visual.strips.push_back(strip);
    return kDraw3DBlockQuads;
}

extern "C" int DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(CWater *water, RenderInfo *info,
                                                                sceVif1Packet * /*draw_packet*/, void * /*parent_info*/) {
    if (water->CheckClip() != 0) {
        return 0;
    }
    u_int *block = water->packet[!DBuffID + 1];
    if (block == nullptr) {
        return 0;
    }

    sceGsTest test = mgPixelTest;
    test.bits.ate = 0;
    test.bits.date = 0;
    MGPortCurrent().test = test;

    sceVu0FMATRIX local_to_world;
    water->frame.GetLWMatrix(local_to_world);
    water->CreateVUData(block, &mgRenderInfo);
    info->fog_enabled = false;

    const Draw3DVisual *visual = Draw3DFindVisual(block);
    if (visual != nullptr && !visual->indices.empty()) {
        Draw3DIdentityScope identity(water, kDraw3DTeleportDistance);
        gfx::MeshConstants  constants = {};
        gfx::MeshTransform  transform{};
        Draw3DSceneConstants(constants, *info, local_to_world, &transform);
        constants.flags = gfx::kMeshVertexColor;
        for (float &value : constants.diffuse) {
            value = 1.0f;
        }
        gfx::DrawState state = Draw3DState(*info, false);
        Refraction     refraction;
        if (PrepareRefraction(refraction)) {
            DrawRefracted(*water, refraction, *visual, constants, transform, state);
        } else {
            PortTextureRef      copy = FrameCopy();
            gfx::TextureBinding binding;
            if (copy.valid && copy.binding.texture != gfx::CurrentRenderTarget() &&
                copy.binding.texture != gfx::kMainTarget) {
                binding = copy.binding;
                binding.filter = gfx::Filter::Linear;
                binding.wrap_u = gfx::Wrap::Clamp;
                binding.wrap_v = gfx::Wrap::Clamp;
            }
            gfx::DrawMeshImmediate(visual->vertices, visual->indices, constants, binding, state, &transform);
        }
    }

    MGPortCurrent().test = mgPixelTest;
    return 0;
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

} // namespace dc::game
