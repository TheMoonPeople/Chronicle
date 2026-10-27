#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dataalloc.hpp"
#include "dataset.hpp"
#include "mathutil.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "platform/load_work_profile.hpp"
#include "render/draw3d.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "tim2.hpp"
#include "visual.hpp"
#include "vu0_ops.hpp"

namespace dc::game {

using namespace dc::render;

namespace {

// Rounds a texture dimension up to the next power of two and gives its exponent.
int CeilLog2(int size) {
    int log2 = 0;

    for (int rest = size; rest >= 2; rest >>= 1) {
        log2++;
    }

    if (size != (1 << log2)) {
        log2++;
    }

    return log2;
}

int SetShadowData(u_int *packet, float (*matrix)[4]) {
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0x6C080029;
    packet[4] = 0x8001;
    packet[5] = 0x102E8000;
    packet[6] = 0xE;
    packet[7] = 0;
    packet[8] = 0x68;
    packet[9] = 0x80;
    packet[10] = 0x42;
    packet[11] = 0;
    packet[12] = 0x8001;
    packet[13] = 0x102E8000;
    packet[14] = 0xE;
    packet[15] = 0;
    packet[16] = 0x62;
    packet[17] = 0x80;
    packet[18] = 0x42;
    packet[19] = 0;

    auto       *quads = reinterpret_cast<u_long128 *>(packet);
    const auto *rows = reinterpret_cast<const u_long128 *>(matrix);
    quads[5] = rows[0];
    quads[6] = rows[1];
    quads[7] = rows[2];
    quads[8] = rows[3];
    return 0x24;
}

int SetMaterial(u_int *packet, MDT_MATERIAL *material) {
    if (material == nullptr) {
        return 0x14;
    }

    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;
    packet[3] = 0x6C040025;
    u_int last_row[4] = {3, 0, 0, 0};
    auto *quads = reinterpret_cast<u_long128 *>(packet);
    quads[1] = *reinterpret_cast<u_long128 *>(material->diffuse);
    quads[2] = *reinterpret_cast<u_long128 *>(material->ambient);
    quads[3] = *reinterpret_cast<u_long128 *>(material->specular);
    quads[4] = *reinterpret_cast<u_long128 *>(last_row);
    return 0x14;
}

int SetTEX0(u_int *packet, u_long tex0, u_long tex1) {
    packet[0] = 0;
    packet[1] = 0;
    packet[2] = 0;

    if (tex1 == 0) {
        tex1 = std::bit_cast<u_long>(mgTEX1Env);
    }

    if (tex1 != 0) {
        packet[3] = 0x50000003;
        packet[4] = 0x8002;
    } else {
        packet[3] = 0x50000002;
        packet[4] = 0x8001;
    }

    packet[5] = 0x10000000;
    packet[6] = 0xE;
    packet[7] = 0;

    packet[8] = static_cast<u_int>(tex0);
    packet[9] = static_cast<u_int>((tex0 >> 32) & 0xFFFFFFFF);
    packet[10] = SCE_GS_TEX0_1;
    packet[11] = 0;

    if (tex1 != 0) {
        packet[12] = static_cast<u_int>(tex1);
        packet[13] = static_cast<u_int>((tex1 >> 32) & 0xFFFFFFFF);
        packet[14] = SCE_GS_TEX1_1;
        packet[15] = 0;
        return 0x10;
    }

    return 0xC;
}

// Writes one vector-unit upload header followed by the gathered vertex streams and gives back
// its length in quadwords.
int SetVuData(int count, u_long128 *block, u_int *index, u_long128 *vertex, u_long128 *normal, u_long128 *uv, u_long128 *colour, int prim) {
    auto *words = reinterpret_cast<u_int *>(block);

    words[0] = count | 0x8000;
    words[1] = prim != 4 ? 0x302DC000 : 0x302E4000;
    words[2] = 0x412;
    words[3] = 0;
    words[4] = count;
    words[5] = prim;
    words[6] = colour != nullptr ? 0x100 : 0;

    const int vertex_start = 2;
    const int normal_start = vertex_start + count;
    const int uv_start = normal_start + count;
    const int colour_start = uv_start + count;
    int       next_index = 0;

    for (int out = 0; out < count; out++) {
        block[vertex_start + out] = vertex[index[next_index]];
        block[normal_start + out] = normal[index[next_index + 1]];
        block[uv_start + out] = uv[index[next_index + 2]];
        next_index += 3;

        if (colour != nullptr) {
            block[colour_start + out] = colour[index[next_index]];
            next_index++;
        }
    }

    if (colour != nullptr) {
        return count * 4 + 2;
    }

    return count * 3 + 2;
}

} // namespace

void SetTextureInfo(CTexture *tex, char *name, TM2_head *head) {
    auto     *picture = reinterpret_cast<TM2_picture *>(reinterpret_cast<u_char *>(head) + 16);
    const int width = head->image_width;
    const int height = head->image_height;
    int       bpp;
    int       psm = SCE_GS_PSMCT32;

    switch (head->image_type) {
        case TIM2_RGB16:
            psm = SCE_GS_PSMCT16;
            bpp = 2;
            break;
        case TIM2_RGB24:
            psm = SCE_GS_PSMCT24;
            bpp = 3;
            break;
        case TIM2_RGB32:
            bpp = 4;
            break;
        case TIM2_IDTEX4:
            psm = SCE_GS_PSMT4;
            bpp = 0;
            break;
        case TIM2_IDTEX8:
            psm = SCE_GS_PSMT8;
            bpp = 1;
            break;
    }

    u_char *image = reinterpret_cast<u_char *>(picture) + picture->header_size;
    u_char *clut = bpp < 2 ? image + picture->image_size : nullptr;

    std::array<u_char *, 4> mip{};

    if (picture->mipmap_count > 1) {
        u_char *next = image + picture->mipmap_size[0];

        for (int level = 1; std::cmp_less(level, picture->mipmap_count); level++) {
            mip[level] = next;
            next += picture->mipmap_size[level];
        }
    }

    const std::string_view source_name{name};
    std::ranges::copy(source_name, tex->name);
    tex->name[source_name.size()] = '\0';
    tex->width = width;
    tex->height = height;
    tex->bpp = bpp;
    tex->image[0] = reinterpret_cast<u_int *>(image);

    for (int level = 1; level < 4; level++) {
        tex->image[level] = reinterpret_cast<u_int *>(mip[level]);
    }

    tex->clut = reinterpret_cast<u_int *>(clut);

    const int width_log2 = CeilLog2(width);
    const int height_log2 = CeilLog2(height);
    const int buffer_width = std::max(width >> 6, 1);

    if (tex->bpp > 1) {
        tex->tex0 = SCE_GS_SET_TEX0(0, buffer_width, psm, width_log2, height_log2, 1, 0, 0, 0, 0, 0, 0);
    } else {
        tex->tex0 = SCE_GS_SET_TEX0(0, buffer_width, psm, width_log2, height_log2, 1, 0, 0, 0, 0, 0, 1);
    }
}

void SetTextureInfo(CTexture *tex, char *name, u_char *buffer) {
    auto                *head = reinterpret_cast<IMG_head *>(buffer);
    std::span<IMG_entry> entries{reinterpret_cast<IMG_entry *>(head + 1), head->pictures};

    for (IMG_entry &entry : entries) {
        if (name != nullptr && name[0] != '\0' && std::string_view{entry.name} != std::string_view{name}) {
            continue;
        }

        SetTextureInfo(tex, entry.name, reinterpret_cast<TM2_head *>(buffer + entry.offset));
    }
}

void CVisual::Initialize() {
    unk_00 = 0;
    unk_04 = 0;
}

CVisual::CVisual() {
    Initialize();
}

int CVisualVu1::RemakeData(unsigned int * /*data*/) {
    return 0;
}

namespace {

float InverseLength(float *vector) {
    return vu0::Rsqrt(1.0f, vu0::LengthSquared(vector));
}

} // namespace

int CVisualVu1::DrawVu1(sceVif1Packet * /*packet*/, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                        u_long128 *draw_state, int unknown1, int unknown2) {
    return CVisualVu1::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

void CVisualVu1::Initialize() {
    CVisual::Initialize();
    vu_data = nullptr;
    vu_size = 0;
    unk_00 = 0;
    unk_04 = 0;
}

CVisualVu1::CVisualVu1() {
    Initialize();
}

void CVisualMDTVu1::SetMDTDataAddress(u_int *data) {
    this->data = data;
}

u_int *CVisualMDTVu1::GetMDTDataAddress() {
    return data;
}

void CVisualMDTVu1::Initialize() {
    CVisualVu1::Initialize();
    copy_on_draw = false;
    data = nullptr;
    vu_data_buffer[1] = nullptr;
    vu_data_buffer[0] = nullptr;
}

CVisualMDTVu1::CVisualMDTVu1() {
    CVisualMDTVu1::Initialize();
}

namespace {

struct MdtView {
    MDT_HEADER *header;
    u_int      *strip;
    int         strips;
    float (*vertex)[4];
    float (*normal)[4];
    float (*uv)[4];
    float (*colour)[4];
    MDT_MATERIAL *materials;
    int           stride;
};

MdtView View(u_int *data) {
    MdtView view = {};
    view.header = reinterpret_cast<MDT_HEADER *>(data);
    auto  at = [&](int offset) { return reinterpret_cast<float (*)[4]>(reinterpret_cast<u_char *>(data) + offset); };
    auto *mesh = reinterpret_cast<u_int *>(reinterpret_cast<u_char *>(data) + view.header->mesh_ofs);
    view.vertex = at(view.header->vertex_ofs);
    view.normal = at(view.header->normal[1]);
    view.uv = view.header->uv[1] <= 0 ? view.vertex : at(view.header->uv[1]);
    view.colour = view.header->colour_ofs <= 0 ? nullptr : at(view.header->colour_ofs);
    view.stride = view.colour ? 4 : 3;
    view.materials = reinterpret_cast<MDT_MATERIAL *>(reinterpret_cast<u_char *>(data) + view.header->info_ofs);
    view.strips = static_cast<int>(mesh[2]);
    view.strip = mesh + 4;
    return view;
}

uint8_t ColourByte(float value) {
    const float scaled = value * 128.0f;
    if (scaled <= 0.0f) {
        return 0;
    }
    if (scaled >= 255.0f) {
        return 255;
    }
    return static_cast<uint8_t>(scaled + 0.5f);
}

void TakeMaterial(Draw3DStrip &strip, const MDT_MATERIAL &material) {
    std::memcpy(strip.diffuse, material.diffuse, sizeof(strip.diffuse));
    std::memcpy(strip.ambient, material.ambient, sizeof(strip.ambient));
    std::memcpy(strip.specular, material.specular, sizeof(strip.specular));
}

// VU1 took normalised STQ, and the GS scales S and T by 2^TW and 2^TH; the renderer normalises to
// the texture's own size, which only differs for a texture that is not a power of two.
void UvScale(const Draw3DStrip &strip, float &su, float &sv) {
    su = 1.0f;
    sv = 1.0f;
    if (strip.texture < 0) {
        return;
    }
    CTexture  *texture = TexManager.GetTexture(strip.texture);
    const auto tw = static_cast<unsigned>((strip.tex0 >> 26) & 0xF);
    const auto th = static_cast<unsigned>((strip.tex0 >> 30) & 0xF);
    if (texture->width > 0) {
        su = static_cast<float>(1u << tw) / static_cast<float>(texture->width);
    }
    if (texture->height > 0) {
        sv = static_cast<float>(1u << th) / static_cast<float>(texture->height);
    }
}

struct TextureRun {
    uint32_t first_vertex;
    uint32_t count;
    int      texture;
};

struct GroundFace {
    std::array<uint32_t, 3> vertices;
    size_t                  strip;
    bool                    grassy;
};

bool SamePosition(const gfx::Vertex3D &a, const gfx::Vertex3D &b) {
    float distance = 0.0f;
    for (int axis = 0; axis < 3; axis++) {
        float difference = a.position[axis] - b.position[axis];
        distance += difference * difference;
    }
    return distance < 0.0004f;
}

float GroundAltitude(const gfx::Vertex3D &a, const gfx::Vertex3D &b, const gfx::Vertex3D &c) {
    float dx = b.position[0] - a.position[0];
    float dz = b.position[2] - a.position[2];
    float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.001f) {
        return 0.0f;
    }
    return std::fabs(dx * (c.position[2] - a.position[2]) - dz * (c.position[0] - a.position[0])) / length;
}

void GroundUvAt(const std::array<gfx::Vertex3D, 3> &source, const float position[3], float uv[2]) {
    float ax = source[1].position[0] - source[0].position[0];
    float az = source[1].position[2] - source[0].position[2];
    float bx = source[2].position[0] - source[0].position[0];
    float bz = source[2].position[2] - source[0].position[2];
    float px = position[0] - source[0].position[0];
    float pz = position[2] - source[0].position[2];
    float determinant = ax * bz - az * bx;
    if (std::fabs(determinant) < 0.000001f) {
        uv[0] = source[0].uv[0];
        uv[1] = source[0].uv[1];
        return;
    }
    float u = (px * bz - pz * bx) / determinant;
    float v = (ax * pz - az * px) / determinant;
    for (int axis = 0; axis < 2; axis++) {
        uv[axis] = source[0].uv[axis] + u * (source[1].uv[axis] - source[0].uv[axis]) +
                   v * (source[2].uv[axis] - source[0].uv[axis]);
    }
}

// Draw each ground texture a short distance into its neighbour at a fading opacity. The two
// half-opacity edges meet without a hard material boundary while the original art stays intact.
void AddGroundTransition(Draw3DVisual &visual, const GroundFace &target_face, const GroundFace &source_face,
                         int edge_a, int edge_b) {
    std::array<gfx::Vertex3D, 3> target = {};
    std::array<gfx::Vertex3D, 3> source = {};
    for (int i = 0; i < 3; i++) {
        target[i] = visual.vertices[target_face.vertices[i]];
        source[i] = visual.vertices[source_face.vertices[i]];
    }

    for (int edge = 0; edge < 3; edge++) {
        int next = (edge + 1) % 3;
        if ((edge == edge_a && next == edge_b) || (edge == edge_b && next == edge_a)) {
            edge_a = edge;
            edge_b = next;
            break;
        }
    }
    int   third = 3 - edge_a - edge_b;
    float altitude = GroundAltitude(target[edge_a], target[edge_b], target[third]);
    if (altitude < 0.01f) {
        return;
    }
    float                        fraction = std::min(4.0f / altitude, 0.25f);
    std::array<gfx::Vertex3D, 4> band = {target[edge_a], target[edge_b], target[edge_b], target[edge_a]};
    for (int axis = 0; axis < 3; axis++) {
        band[2].position[axis] += fraction * (target[third].position[axis] - band[2].position[axis]);
        band[3].position[axis] += fraction * (target[third].position[axis] - band[3].position[axis]);
    }
    for (size_t i = 0; i < band.size(); i++) {
        gfx::Vertex3D &vertex = band[i];
        GroundUvAt(source, vertex.position, vertex.uv);
        vertex.position[1] += 0.02f;
        vertex.color[0] = 0x80;
        vertex.color[1] = 0x80;
        vertex.color[2] = 0x80;
        vertex.color[3] = i < 2 ? 0x40 : 0;
    }

    const auto first_vertex = static_cast<uint32_t>(visual.vertices.size());
    visual.vertices.insert(visual.vertices.end(), band.begin(), band.end());
    Draw3DStrip strip = visual.strips[source_face.strip];
    strip.first_index = static_cast<uint32_t>(visual.indices.size());
    strip.index_count = 6;
    strip.ground_transition = true;
    visual.indices.insert(visual.indices.end(), {first_vertex, first_vertex + 1, first_vertex + 2,
                                                 first_vertex, first_vertex + 2, first_vertex + 3});
    visual.strips.push_back(strip);
    visual.vertex_colour = true;
}

void AddForestGroundTransitions(Draw3DVisual &visual) {
    std::vector<GroundFace> faces;
    size_t                  base_strips = visual.strips.size();
    for (size_t strip_no = 0; strip_no < base_strips; strip_no++) {
        const Draw3DStrip &strip = visual.strips[strip_no];
        if (strip.texture < 0) {
            continue;
        }
        const std::string_view name = TexManager.GetTexture(strip.texture)->name;
        const bool             grassy = name == "d02b05";
        if (!grassy && name != "d02b06") {
            continue;
        }
        for (uint32_t i = strip.first_index; i + 2 < strip.first_index + strip.index_count; i += 3) {
            GroundFace face = {
                .vertices = {visual.indices[i], visual.indices[i + 1], visual.indices[i + 2]},
                .strip = strip_no,
                .grassy = grassy
            };
            faces.push_back(face);
        }
    }

    for (size_t a = 0; a < faces.size(); a++) {
        for (size_t b = a + 1; b < faces.size(); b++) {
            if (faces[a].grassy == faces[b].grassy) {
                continue;
            }
            std::array<int, 2> first = {-1, -1};
            std::array<int, 2> second = {-1, -1};
            int                shared = 0;
            for (int va = 0; va < 3; va++) {
                for (int vb = 0; vb < 3; vb++) {
                    if (SamePosition(visual.vertices[faces[a].vertices[va]],
                                     visual.vertices[faces[b].vertices[vb]])) {
                        if (shared < 2) {
                            first[shared] = va;
                            second[shared] = vb;
                        }
                        shared++;
                    }
                }
            }
            if (shared == 2) {
                bool ground_edge = true;
                for (const int corner : first) {
                    const gfx::Vertex3D &vertex = visual.vertices[faces[a].vertices[corner]];
                    ground_edge = ground_edge && std::fabs(vertex.position[1]) < 1.5f &&
                                  vertex.normal[1] > 0.75f;
                }
                if (!ground_edge) {
                    continue;
                }
                AddGroundTransition(visual, faces[a], faces[b], first[0], first[1]);
                AddGroundTransition(visual, faces[b], faces[a], second[0], second[1]);
            }
        }
    }
}

void AddGroundBorderBand(Draw3DVisual &visual, const std::array<gfx::Vertex3D, 3> &face,
                         const Draw3DStrip &material, int edge_a, int edge_b, int soil_texture) {
    int   third = 3 - edge_a - edge_b;
    float altitude = GroundAltitude(face[edge_a], face[edge_b], face[third]);
    if (altitude < 0.01f) {
        return;
    }
    float                        fraction = std::min(1.0f / altitude, 0.25f);
    std::array<gfx::Vertex3D, 4> band = {face[edge_a], face[edge_b], face[edge_b], face[edge_a]};
    for (int axis = 0; axis < 3; axis++) {
        band[2].position[axis] += fraction * (face[third].position[axis] - band[2].position[axis]);
        band[3].position[axis] += fraction * (face[third].position[axis] - band[3].position[axis]);
    }
    for (size_t i = 0; i < band.size(); i++) {
        band[i].position[1] += 0.03f;
        band[i].color[0] = 0x80;
        band[i].color[1] = 0x80;
        band[i].color[2] = 0x80;
        band[i].color[3] = i < 2 ? 0x80 : 0;
    }

    const auto first_vertex = static_cast<uint32_t>(visual.vertices.size());
    visual.vertices.insert(visual.vertices.end(), band.begin(), band.end());
    Draw3DStrip     strip = material;
    const CTexture *soil = TexManager.GetTexture(soil_texture);
    strip.texture = soil_texture;
    strip.tex0 = soil->tex0;
    strip.tex1 = soil->tex1;
    strip.first_index = static_cast<uint32_t>(visual.indices.size());
    strip.index_count = 6;
    strip.ground_transition = true;
    strip.ground_world_uv = true;
    visual.indices.insert(visual.indices.end(), {first_vertex, first_vertex + 1, first_vertex + 2,
                                                 first_vertex, first_vertex + 2, first_vertex + 3});
    visual.strips.push_back(strip);
    visual.vertex_colour = true;
}

// Forest cells use different soil images at some matching outer edges. A shared world-space
// sample covers the edge itself and fades into each cell's original image a short way inward.
void AddForestGroundBorders(Draw3DVisual &visual) {
    std::string soil_name = "d02b04";
    const int   soil_texture = TexManager.GetTextureHandle(soil_name.data(), -1);
    if (soil_texture < 0) {
        return;
    }
    size_t base_strips = visual.strips.size();
    for (size_t strip_no = 0; strip_no < base_strips; strip_no++) {
        Draw3DStrip strip = visual.strips[strip_no];
        if (strip.texture < 0 || strip.ground_transition) {
            continue;
        }
        const std::string_view name = TexManager.GetTexture(strip.texture)->name;
        if (name != "d02b04" && name != "d02b05" && name != "d02b06") {
            continue;
        }
        for (uint32_t i = strip.first_index; i + 2 < strip.first_index + strip.index_count; i += 3) {
            std::array<gfx::Vertex3D, 3> face = {visual.vertices[visual.indices[i]],
                                                 visual.vertices[visual.indices[i + 1]],
                                                 visual.vertices[visual.indices[i + 2]]};
            for (int edge = 0; edge < 3; edge++) {
                int                  next = (edge + 1) % 3;
                const gfx::Vertex3D &a = face[edge];
                const gfx::Vertex3D &b = face[next];
                bool                 x_border = std::fabs(std::fabs(a.position[0]) - 80.0f) < 0.5f &&
                                                std::fabs(a.position[0] - b.position[0]) < 0.5f;
                bool                 z_border = std::fabs(std::fabs(a.position[2]) - 80.0f) < 0.5f &&
                                                std::fabs(a.position[2] - b.position[2]) < 0.5f;
                if ((x_border || z_border) && std::fabs(a.position[1]) < 2.0f &&
                    std::fabs(b.position[1]) < 2.0f && a.normal[1] > 0.6f && b.normal[1] > 0.6f) {
                    AddGroundBorderBand(visual, face, strip, edge, next, soil_texture);
                }
            }
        }
    }
}

// The models map a texture that tiles from one texel inside its near edge to two inside its far
// one (1 to size - 2), so the GS's clamp never filters past an edge. Where two such panels meet,
// the texels left out are a step in the picture: the title's sky dome repeats its clouds every
// quarter turn and shows the joints. A texture whose coordinates span exactly that range along an
// axis is given edge to edge along it, texel centre to texel centre, which is where it tiles.
void SpanWholeTexture(Draw3DVisual &visual, std::span<const TextureRun> runs) {
    for (size_t r = 0; r < runs.size(); r++) {
        int texture = runs[r].texture;
        if (texture < 0) {
            continue;
        }
        bool earlier = false;
        for (size_t k = 0; k < r; k++) {
            earlier = earlier || runs[k].texture == texture;
        }
        if (earlier) {
            continue;
        }
        const CTexture            *info = TexManager.GetTexture(texture);
        const std::array<float, 2> size = {static_cast<float>(info->width), static_cast<float>(info->height)};
        std::array<float, 2>       low = {INFINITY, INFINITY};
        std::array<float, 2>       high = {-INFINITY, -INFINITY};
        for (const TextureRun &run : runs) {
            if (run.texture != texture) {
                continue;
            }
            for (uint32_t i = run.first_vertex; i < run.first_vertex + run.count; i++) {
                for (int axis = 0; axis < 2; axis++) {
                    low[axis] = std::min(low[axis], visual.vertices[i].uv[axis] * size[axis]);
                    high[axis] = std::max(high[axis], visual.vertices[i].uv[axis] * size[axis]);
                }
            }
        }
        for (int axis = 0; axis < 2; axis++) {
            if (size[axis] < 8.0f || std::fabs(low[axis] - 1.0f) > 0.01f ||
                std::fabs(high[axis] - (size[axis] - 2.0f)) > 0.01f) {
                continue;
            }
            const float scale = (size[axis] - 1.0f) / (size[axis] - 3.0f);
            for (const TextureRun &run : runs) {
                if (run.texture != texture) {
                    continue;
                }
                for (uint32_t i = run.first_vertex; i < run.first_vertex + run.count; i++) {
                    float &uv = visual.vertices[i].uv[axis];
                    uv = ((uv * size[axis] - 1.0f) * scale + 0.5f) / size[axis];
                }
            }
        }
    }
}

} // namespace

int CVisualVu1::DrawVu1(u_int * /*packet*/, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                        u_long128 * /*draw_state*/, int /*unknown1*/, int /*unknown2*/) {
    if (vu_data == nullptr || vu_size == 0) {
        return 0;
    }
    if (const Draw3DVisual *visual = Draw3DFindVisual(vu_data)) {
        Draw3DDrawVisual(*visual, matrix, *info, program);
    }
    return 0;
}

// Every {v, n, uv[, colour]} entry of a strip becomes one vertex; strips (GS prim 4) become lists
// wound as their first triangle, other prims are lists already. A strip whose material is -1 keeps
// the previous strip's material and texture, as VU1's memory did.
int CVisualVu1::CreateVUdataFromMDT(u_int *block, u_int *data, int /*unknown0*/, int /*unknown1*/) {
    const dc::platform::load_profile::Span profile(dc::platform::load_profile::Kind::Visual);
    vu_data = block;
    vu_size = kDraw3DBlockQuads;
    Draw3DVisual &visual = Draw3DRegisterVisual(block, false);
    MdtView       view = View(data);
    visual.vertex_colour = view.colour != nullptr;

    Draw3DStrip             current;
    std::vector<TextureRun> runs;
    u_int                  *index = view.strip;
    for (int s = 0; s < view.strips; s++) {
        uint32_t count = index[1];
        u_int    prim = index[0];
        int      material = static_cast<int>(index[2]);
        index += 3;

        if (material != -1) {
            MDT_MATERIAL &entry = view.materials[material];
            int           handle = TexManager.GetTextureHandle(entry.texture, -1);
            current.texture = handle;
            current.texture_name = handle >= 0 ? TexManager.GetTexture(handle)->name : "";
            current.tex0 = TexManager.GetTexture(handle)->tex0;
            current.tex1 = TexManager.GetTexture(handle)->tex1;
            TakeMaterial(current, entry);
        }

        float su = 1.0f;
        float sv = 1.0f;
        UvScale(current, su, sv);
        const auto first_vertex = static_cast<uint32_t>(visual.vertices.size());
        for (uint32_t i = 0; i < count; i++, index += view.stride) {
            gfx::Vertex3D vertex = {};
            std::memcpy(vertex.position, view.vertex[index[0]], sizeof(vertex.position));
            std::memcpy(vertex.normal, view.normal[index[1]], sizeof(vertex.normal));
            vertex.uv[0] = view.uv[index[2]][0] * su;
            vertex.uv[1] = view.uv[index[2]][1] * sv;
            for (int c = 0; c < 4; c++) {
                vertex.color[c] = view.colour ? ColourByte(view.colour[index[3]][c]) : 0x80;
            }
            visual.vertices.push_back(vertex);
        }
        runs.push_back({.first_vertex = first_vertex, .count = count, .texture = current.texture});

        Draw3DStrip strip = current;
        strip.first_index = static_cast<uint32_t>(visual.indices.size());
        if (prim == 4) {
            Draw3DStripToList(visual.indices, first_vertex, count);
        } else {
            for (uint32_t i = 0; i + 2 < count; i += 3) {
                visual.indices.insert(visual.indices.end(), {first_vertex + i, first_vertex + i + 1, first_vertex + i + 2});
            }
        }
        strip.index_count = static_cast<uint32_t>(visual.indices.size()) - strip.first_index;
        visual.strips.push_back(strip);
    }

    SpanWholeTexture(visual, runs);
    AddForestGroundTransitions(visual);
    AddForestGroundBorders(visual);
    for (Draw3DStrip &strip : visual.strips) {
        if (strip.texture >= 0 && std::string_view(TexManager.GetTexture(strip.texture)->name) == "e04b01") {
            strip.ground_world_uv = true;
            strip.ground_uv_period = 100.0f;
            strip.ground_uv_flip_v = true;
        }
    }
    visual.cache_strip_runs_on_finish = true;
    Draw3DFinishVisual(visual);
    return vu_size;
}

// Retail rewrites positions, colours and materials in place after skinning, morphs and material
// animation; normals and UVs stay as built.
int CVisualVu1::CreateVUdataFromMDTRemake(u_int *block, u_int *data, int /*unknown0*/) {
    vu_data = block;
    vu_size = kDraw3DBlockQuads;
    Draw3DVisual *visual = Draw3DFindVisual(block);
    if (visual == nullptr) {
        return vu_size;
    }
    MdtView view = View(data);
    u_int  *index = view.strip;
    size_t  vertex = 0;
    size_t  strip_number = 0;
    for (int s = 0; s < view.strips; s++) {
        uint32_t count = index[1];
        int      material = static_cast<int>(index[2]);
        index += 3;
        if (strip_number < visual->strips.size()) {
            Draw3DStrip &strip = visual->strips[strip_number];
            if (material != -1) {
                TakeMaterial(strip, view.materials[material]);
            } else if (strip_number > 0) {
                const Draw3DStrip &previous = visual->strips[strip_number - 1];
                std::memcpy(strip.diffuse, previous.diffuse, sizeof(strip.diffuse));
                std::memcpy(strip.ambient, previous.ambient, sizeof(strip.ambient));
                std::memcpy(strip.specular, previous.specular, sizeof(strip.specular));
            }
        }
        strip_number++;
        for (uint32_t i = 0; i < count && vertex < visual->vertices.size(); i++, index += view.stride, vertex++) {
            gfx::Vertex3D &out = visual->vertices[vertex];
            std::memcpy(out.position, view.vertex[index[0]], sizeof(out.position));
            if (view.colour) {
                for (int c = 0; c < 4; c++) {
                    out.color[c] = ColourByte(view.colour[index[3]][c]);
                }
            }
        }
    }
    // Record stationary poses too: a display frame needs both ends when animation starts or stops.
    Draw3DInvalidateStripRuns(*visual);
    if (visual->mesh != gfx::kNullMesh) {
        gfx::UpdateMeshVertices(visual->mesh, 0, visual->vertices);
    }
    return vu_size;
}

// One host mesh serves every display buffer, so consecutive poses keep the same surface identity;
// the renderer orders a remake after a draw of the same frame by itself.
int CVisualMDTVu1::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                           u_long128 *draw_state, int unknown1, int unknown2) {
    vu_data = vu_data_buffer[0];
    int result = CVisualVu1::DrawVu1(packet, matrix, info, program, draw_state, unknown1, unknown2);
    vu_data = vu_data_buffer[0];
    return result;
}

int CVisualMDTVu1::DrawVu1(sceVif1Packet * /*packet*/, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                           u_long128 *draw_state, int unknown1, int unknown2) {
    vu_data = vu_data_buffer[0];
    return CVisualVu1::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

int CVisualMDTVu1::RemakeData(u_int * /*block*/) {
    if (data == nullptr) {
        return 0;
    }
    return CreateVUdataFromMDTRemake(vu_data_buffer[0], data, 1);
}

} // namespace dc::game
