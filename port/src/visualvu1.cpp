#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "dataalloc.hpp"
#include "dataset.hpp"
#include "draw3d.hpp"
#include "mdt.hpp"
#include "mglib.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"
#include "visual.hpp"

namespace {

constexpr uint32_t kBlockTag = 0x44334443; // "CD3D"

std::unordered_map<const void *, std::unique_ptr<Draw3DVisual>> g_visuals;
std::unordered_map<const void *, std::unique_ptr<Draw3DVisual>> g_transient;
unsigned                                                        g_visual_sweep_frame = 0;

void Release(Draw3DVisual &visual) {
    if (visual.mesh != gfx::kNullMesh) {
        gfx::DestroyMesh(visual.mesh);
        visual.mesh = gfx::kNullMesh;
    }
}

bool SameMaterial(const Draw3DStrip &a, const Draw3DStrip &b) {
    return a.texture == b.texture && a.tex0 == b.tex0 && a.tex1 == b.tex1 &&
           a.texture_name == b.texture_name &&
           std::memcmp(a.diffuse, b.diffuse, sizeof(a.diffuse)) == 0 &&
           std::memcmp(a.ambient, b.ambient, sizeof(a.ambient)) == 0 &&
           std::memcmp(a.specular, b.specular, sizeof(a.specular)) == 0 &&
           a.ground_transition == b.ground_transition && a.ground_world_uv == b.ground_world_uv &&
           a.ground_uv_period == b.ground_uv_period && a.ground_uv_flip_v == b.ground_uv_flip_v;
}

// CreateVisual places the block right behind a visual whose host size outgrows the two or three
// quadwords retail allocates for it, and then writes the visual's pointer fields over the block's
// first two quadwords; the tag sits past them.
void TagBlock(unsigned int *block) {
    block[8] = kBlockTag;
    block[9] = 0;
    block[10] = 0;
    block[11] = 0;
}

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
    auto   at = [&](int offset) { return reinterpret_cast<float (*)[4]>(reinterpret_cast<u_char *>(data) + offset); };
    u_int *mesh = reinterpret_cast<u_int *>(reinterpret_cast<u_char *>(data) + view.header->mesh_ofs);
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
    float scaled = value * 128.0f;
    return static_cast<uint8_t>(scaled <= 0.0f ? 0.0f : scaled >= 255.0f ? 255.0f
                                                                         : scaled + 0.5f);
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
    CTexture *texture = TexManager.GetTexture(strip.texture);
    unsigned  tw = static_cast<unsigned>((strip.tex0 >> 26) & 0xF);
    unsigned  th = static_cast<unsigned>((strip.tex0 >> 30) & 0xF);
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
    std::array<gfx::Vertex3D, 3> target;
    std::array<gfx::Vertex3D, 3> source;
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
    float         fraction = std::min(4.0f / altitude, 0.25f);
    gfx::Vertex3D band[4] = {target[edge_a], target[edge_b], target[edge_b], target[edge_a]};
    for (int axis = 0; axis < 3; axis++) {
        band[2].position[axis] += fraction * (target[third].position[axis] - band[2].position[axis]);
        band[3].position[axis] += fraction * (target[third].position[axis] - band[3].position[axis]);
    }
    for (int i = 0; i < 4; i++) {
        gfx::Vertex3D &vertex = band[i];
        GroundUvAt(source, vertex.position, vertex.uv);
        vertex.position[1] += 0.02f;
        vertex.color[0] = 0x80;
        vertex.color[1] = 0x80;
        vertex.color[2] = 0x80;
        vertex.color[3] = i < 2 ? 0x40 : 0;
    }

    uint32_t first_vertex = static_cast<uint32_t>(visual.vertices.size());
    visual.vertices.insert(visual.vertices.end(), std::begin(band), std::end(band));
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
        const char *name = TexManager.GetTexture(strip.texture)->name;
        bool        grassy = std::strcmp(name, "d02b05") == 0;
        if (!grassy && std::strcmp(name, "d02b06") != 0) {
            continue;
        }
        for (uint32_t i = strip.first_index; i + 2 < strip.first_index + strip.index_count; i += 3) {
            GroundFace face = {{{visual.indices[i], visual.indices[i + 1], visual.indices[i + 2]}}, strip_no, grassy};
            faces.push_back(face);
        }
    }

    for (size_t a = 0; a < faces.size(); a++) {
        for (size_t b = a + 1; b < faces.size(); b++) {
            if (faces[a].grassy == faces[b].grassy) {
                continue;
            }
            int first[2] = {-1, -1};
            int second[2] = {-1, -1};
            int shared = 0;
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
                for (int edge = 0; edge < 2; edge++) {
                    const gfx::Vertex3D &vertex = visual.vertices[faces[a].vertices[first[edge]]];
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
    float         fraction = std::min(1.0f / altitude, 0.25f);
    gfx::Vertex3D band[4] = {face[edge_a], face[edge_b], face[edge_b], face[edge_a]};
    for (int axis = 0; axis < 3; axis++) {
        band[2].position[axis] += fraction * (face[third].position[axis] - band[2].position[axis]);
        band[3].position[axis] += fraction * (face[third].position[axis] - band[3].position[axis]);
    }
    for (int i = 0; i < 4; i++) {
        band[i].position[1] += 0.03f;
        band[i].color[0] = 0x80;
        band[i].color[1] = 0x80;
        band[i].color[2] = 0x80;
        band[i].color[3] = i < 2 ? 0x80 : 0;
    }

    uint32_t first_vertex = static_cast<uint32_t>(visual.vertices.size());
    visual.vertices.insert(visual.vertices.end(), std::begin(band), std::end(band));
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
    char soil_name[] = "d02b04";
    int  soil_texture = TexManager.GetTextureHandle(soil_name, -1);
    if (soil_texture < 0) {
        return;
    }
    size_t base_strips = visual.strips.size();
    for (size_t strip_no = 0; strip_no < base_strips; strip_no++) {
        Draw3DStrip strip = visual.strips[strip_no];
        if (strip.texture < 0 || strip.ground_transition) {
            continue;
        }
        const char *name = TexManager.GetTexture(strip.texture)->name;
        if (std::strcmp(name, "d02b04") != 0 && std::strcmp(name, "d02b05") != 0 &&
            std::strcmp(name, "d02b06") != 0) {
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
        const CTexture *info = TexManager.GetTexture(texture);
        const float     size[2] = {static_cast<float>(info->width), static_cast<float>(info->height)};
        float           low[2] = {INFINITY, INFINITY};
        float           high[2] = {-INFINITY, -INFINITY};
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

// ---- Records ---------------------------------------------------------------------------------

Draw3DVisual *Draw3DFindVisual(const void *block) {
    if (block == nullptr) {
        return nullptr;
    }
    if (auto it = g_transient.find(block); it != g_transient.end()) {
        return it->second.get();
    }
    if (auto it = g_visuals.find(block); it != g_visuals.end()) {
        if (static_cast<const unsigned int *>(block)[8] != kBlockTag) {
            Release(*it->second);
            g_visuals.erase(it);
            return nullptr;
        }
        return it->second.get();
    }
    return nullptr;
}

Draw3DVisual &Draw3DRegisterVisual(unsigned int *block, bool transient) {
    auto &table = transient ? g_transient : g_visuals;
    auto &slot = table[block];
    if (slot) {
        Release(*slot);
    }
    slot = std::make_unique<Draw3DVisual>();
    slot->immediate = transient;
    TagBlock(block);
    return *slot;
}

// Arenas are cleared or reused when the game moves on (BufferAllClear, a mode's own SetDataBuffer),
// and nothing tells the records; a block whose tag is gone no longer holds a visual.
void Draw3DSweepVisuals() {
    g_transient.clear();
    if ((++g_visual_sweep_frame & 15u) != 0) {
        return;
    }
    std::erase_if(g_visuals, [](auto &entry) {
        const unsigned int *block = static_cast<const unsigned int *>(entry.first);
        if (block[8] == kBlockTag) {
            return false;
        }
        Release(*entry.second);
        return true;
    });
}

void Draw3DFinishVisual(Draw3DVisual &visual) {
    if (visual.immediate || visual.vertices.empty() || visual.indices.empty()) {
        return;
    }
    visual.mesh = gfx::CreateMesh(visual.vertices, visual.indices);
    visual.indices.clear();
    visual.indices.shrink_to_fit();
}

void Draw3DStripToList(std::vector<uint32_t> &indices, uint32_t first_vertex, uint32_t count) {
    for (uint32_t k = 0; k + 2 < count; k++) {
        uint32_t a = first_vertex + k;
        if (k & 1) {
            indices.insert(indices.end(), {a + 1, a, a + 2});
        } else {
            indices.insert(indices.end(), {a, a + 1, a + 2});
        }
    }
}

// The VU1 header's PRMODE: TME only when lit (not on shadow passes, not with unlit), IIP off on
// shadow passes, FGE when the frame takes fog and it is not a shadow pass, ABE always.
void Draw3DDrawVisual(const Draw3DVisual &visual, const float model[4][4], const RenderInfo &info, int program) {
    if (visual.strips.empty() || (!visual.immediate && visual.mesh == gfx::kNullMesh)) {
        return;
    }
    bool projected = info.shadow_pass == 1 || info.shadow_pass == 2;
    if (projected && !Draw3DShadowTargetActive()) {
        return;
    }
    bool lit = info.shadow_pass == 0 && info.unlit == 0;
    bool fog = info.fog_enabled != 0 && info.shadow_pass == 0;

    gfx::MeshConstants constants = {};
    gfx::MeshTransform transform;
    Draw3DSceneConstants(constants, info, model, &transform);
    // A visual drawn outside a CFrame is known by its record, unless the record lasts one tick.
    bool unkeyed = gfx::CurrentInterpKey() == 0 && !visual.immediate;
    if (unkeyed) {
        gfx::SetInterpKey(reinterpret_cast<uintptr_t>(&visual));
    }
    if (projected) {
        constants.flags = gfx::kMeshShadow;
    } else {
        constants.flags = (lit ? gfx::kMeshLit : 0u) | (visual.vertex_colour ? gfx::kMeshVertexColor : 0u) |
                          (fog ? gfx::kMeshFog : 0u);
    }

    gfx::DrawState state = Draw3DState(info, fog);
    // Clip flag 4 (the 'S' frame-name code, program_option) sends the vertices through Vu_prog0f's
    // culling outputs (OUTPUTCN, OUTPUTCN_STR and the scissored kick): a triangle whose screen
    // cross product has negative z, clockwise on the y-down GS window, is kicked with ADC set and
    // not drawn. The title's sky dome is one; drawn two-sided, its near half hides the clouds.
    if (!projected && (info.clip_flags & 4) != 0) {
        state.cull = gfx::CullMode::Back;
    }
    if (projected) {
        // The first of the two passes SetShadowData hands VU1: Cs * 0x80 / 128 + Cd, so whatever
        // the projection covers turns the black target non-black.
        state.alpha = {0, 2, 2, 1, 0x80};
    }

    static const float kWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t           base_flags = constants.flags;
    for (size_t strip_index = 0; strip_index < visual.strips.size();) {
        const Draw3DStrip &strip = visual.strips[strip_index++];
        uint32_t           index_count = strip.index_count;
        while (strip_index < visual.strips.size()) {
            const Draw3DStrip &next = visual.strips[strip_index];
            if (strip.first_index + index_count != next.first_index || !SameMaterial(strip, next)) {
                break;
            }
            index_count += next.index_count;
            ++strip_index;
        }
        if (index_count == 0) {
            continue;
        }
        if (projected && strip.ground_transition) {
            continue;
        }
        constants.flags = base_flags | (strip.ground_world_uv ? gfx::kMeshWorldUv : 0u);
        if (strip.ground_world_uv) {
            for (int axis = 0; axis < 4; axis++) {
                constants.world_uv[0][axis] = model[axis][0] / strip.ground_uv_period;
                constants.world_uv[1][axis] = model[axis][2] / strip.ground_uv_period *
                                              (strip.ground_uv_flip_v ? -1.0f : 1.0f);
            }
        }
        if (projected) {
            Draw3DMaterial(constants, info, kWhite, kWhite, strip.specular);
            constants.diffuse[3] = 1.0f;
        } else {
            Draw3DMaterial(constants, info, strip.diffuse, strip.ambient, strip.specular);
        }

        gfx::TextureBinding binding;
        if (lit && strip.texture >= 0) {
            int handle = strip.texture;
            if (!strip.texture_name.empty() &&
                std::strcmp(TexManager.GetTexture(handle)->name, strip.texture_name.c_str()) != 0) {
                std::string name = strip.texture_name;
                int         moved = TexManager.GetTextureHandle(name.data(), -1);
                if (moved >= 0) {
                    handle = moved;
                }
            }
            PortTextureRef ref = Draw3DResolveHandle(handle);
            if (ref.valid) {
                binding = ref.binding;
                // SetEnv sends CLAMP_1 = 5 every frame; TEX1's MMAG picks the filter.
                binding.wrap_u = gfx::Wrap::Clamp;
                binding.wrap_v = gfx::Wrap::Clamp;
                if (strip.ground_transition || strip.ground_world_uv) {
                    binding.wrap_u = gfx::Wrap::Repeat;
                    binding.wrap_v = gfx::Wrap::Repeat;
                }
                u_long tex1 = strip.tex1 ? strip.tex1 : *reinterpret_cast<const u_long *>(&mgTEX1Env);
                binding.filter = (tex1 >> 5) & 1 ? gfx::Filter::Linear : gfx::Filter::Nearest;
            }
        }
        gfx::DrawState strip_state = state;
        if (strip.ground_transition) {
            strip_state.alpha = {0, 1, 0, 1, 0x80};
            strip_state.depth_write = false;
        }

        if (visual.immediate) {
            gfx::DrawMeshImmediate(visual.vertices,
                                   std::span<const uint32_t>(visual.indices).subspan(strip.first_index, index_count),
                                   constants, binding, strip_state, &transform);
        } else {
            gfx::DrawMesh(visual.mesh, strip.first_index, index_count, constants, binding, strip_state,
                          &transform);
        }
    }
    if (unkeyed) {
        gfx::SetInterpKey(0);
    }

    if (projected) {
        // The GS is left with the second pass's ALPHA, Cd - Cs.
        MGPortCurrent().alpha.bits.a = 2;
        MGPortCurrent().alpha.bits.b = 0;
        MGPortCurrent().alpha.bits.c = 2;
        MGPortCurrent().alpha.bits.d = 1;
        MGPortCurrent().alpha.bits.fix = 0x80;
    }
    (void) program;
}

// ---- CVisualVu1 ------------------------------------------------------------------------------

PORT_OVERRIDE int CVisualVu1::DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                      u_long128 *draw_state, int unknown1, int unknown2) {
    return CVisualVu1::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

PORT_OVERRIDE int CVisualVu1::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                      u_long128 *draw_state, int unknown1, int unknown2) {
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
PORT_OVERRIDE int CVisualVu1::CreateVUdataFromMDT(u_int *block, u_int *data, int unknown0, int unknown1) {
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

        float su;
        float sv;
        UvScale(current, su, sv);
        uint32_t first_vertex = static_cast<uint32_t>(visual.vertices.size());
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
        runs.push_back({first_vertex, count, current.texture});

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
        if (strip.texture >= 0 && std::strcmp(TexManager.GetTexture(strip.texture)->name, "e04b01") == 0) {
            strip.ground_world_uv = true;
            strip.ground_uv_period = 100.0f;
            strip.ground_uv_flip_v = true;
        }
    }
    Draw3DFinishVisual(visual);
    return vu_size;
}

// Retail rewrites positions, colours and materials in place after skinning, morphs and material
// animation; normals and UVs stay as built.
PORT_OVERRIDE int CVisualVu1::CreateVUdataFromMDTRemake(u_int *block, u_int *data, int unknown0) {
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
    if (visual->mesh != gfx::kNullMesh) {
        gfx::UpdateMeshVertices(visual->mesh, 0, visual->vertices);
    }
    return vu_size;
}

// ---- CVisualMDTVu1 ---------------------------------------------------------------------------

// copy_on_draw kept a private copy of the block for each draw because the DMA read it at the end
// of the frame; the renderer orders a remake after a draw of the same frame by itself.
// Use one host mesh across display buffers so consecutive poses retain the same surface identity.
PORT_OVERRIDE int CVisualMDTVu1::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                         u_long128 *draw_state, int unknown1, int unknown2) {
    vu_data = vu_data_buffer[0];
    int result = CVisualVu1::DrawVu1(packet, matrix, info, program, draw_state, unknown1, unknown2);
    vu_data = vu_data_buffer[0];
    return result;
}

PORT_OVERRIDE int CVisualMDTVu1::DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                         u_long128 *draw_state, int unknown1, int unknown2) {
    vu_data = vu_data_buffer[0];
    return CVisualVu1::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

PORT_OVERRIDE int CVisualMDTVu1::RemakeData(u_int *block) {
    if (data == nullptr) {
        return 0;
    }
    return CreateVUdataFromMDTRemake(vu_data_buffer[0], data, 1);
}
