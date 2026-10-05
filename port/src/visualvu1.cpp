#include <algorithm>
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

void Release(Draw3DVisual &visual) {
    if (visual.mesh != gfx::kNullMesh) {
        gfx::DestroyMesh(visual.mesh);
        visual.mesh = gfx::kNullMesh;
    }
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
    auto   at = [&](int offset) { return reinterpret_cast<float(*)[4]>(reinterpret_cast<u_char *>(data) + offset); };
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
    for (const Draw3DStrip &strip : visual.strips) {
        if (strip.index_count == 0) {
            continue;
        }
        if (projected) {
            Draw3DMaterial(constants, info, kWhite, kWhite, strip.specular);
            constants.diffuse[3] = 1.0f;
        } else {
            Draw3DMaterial(constants, info, strip.diffuse, strip.ambient, strip.specular);
        }

        gfx::TextureBinding binding;
        if (lit && strip.texture >= 0) {
            PortTextureRef ref = Draw3DResolveHandle(strip.texture);
            if (ref.valid) {
                binding = ref.binding;
                // SetEnv sends CLAMP_1 = 5 every frame; TEX1's MMAG picks the filter.
                binding.wrap_u = gfx::Wrap::Clamp;
                binding.wrap_v = gfx::Wrap::Clamp;
                u_long tex1 = strip.tex1 ? strip.tex1 : *reinterpret_cast<const u_long *>(&mgTEX1Env);
                binding.filter = (tex1 >> 5) & 1 ? gfx::Filter::Linear : gfx::Filter::Nearest;
            }
        }

        if (visual.immediate) {
            gfx::DrawMeshImmediate(visual.vertices,
                                   std::span<const uint32_t>(visual.indices).subspan(strip.first_index, strip.index_count),
                                   constants, binding, state, &transform);
        } else {
            gfx::DrawMesh(visual.mesh, strip.first_index, strip.index_count, constants, binding, state,
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

PC_OVERRIDE int CVisualVu1::DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                    u_long128 *draw_state, int unknown1, int unknown2) {
    return CVisualVu1::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

PC_OVERRIDE int CVisualVu1::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
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
PC_OVERRIDE int CVisualVu1::CreateVUdataFromMDT(u_int *block, u_int *data, int unknown0, int unknown1) {
    vu_data = block;
    vu_size = kDraw3DBlockQuads;
    Draw3DVisual &visual = Draw3DRegisterVisual(block, false);
    MdtView       view = View(data);
    visual.vertex_colour = view.colour != nullptr;

    Draw3DStrip             current;
    std::vector<TextureRun> runs;
    u_int      *index = view.strip;
    for (int s = 0; s < view.strips; s++) {
        uint32_t count = index[1];
        u_int    prim = index[0];
        int      material = static_cast<int>(index[2]);
        index += 3;

        if (material != -1) {
            MDT_MATERIAL &entry = view.materials[material];
            int           handle = TexManager.GetTextureHandle(entry.texture, -1);
            current.texture = handle;
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
    Draw3DFinishVisual(visual);
    return vu_size;
}

// Retail rewrites positions, colours and materials in place after skinning, morphs and material
// animation; normals and UVs stay as built.
PC_OVERRIDE int CVisualVu1::CreateVUdataFromMDTRemake(u_int *block, u_int *data, int unknown0) {
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
    if (visual->mesh != gfx::kNullMesh) {
        gfx::UpdateMeshVertices(visual->mesh, 0, visual->vertices);
    }
    return vu_size;
}

// ---- CVisualMDTVu1 ---------------------------------------------------------------------------

// copy_on_draw kept a private copy of the block for each draw because the DMA read it at the end
// of the frame; the renderer orders a remake after a draw of the same frame by itself.
PC_OVERRIDE int CVisualMDTVu1::DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                       u_long128 *draw_state, int unknown1, int unknown2) {
    vu_data = vu_data_buffer[DBuffID];
    int result = CVisualVu1::DrawVu1(packet, matrix, info, program, draw_state, unknown1, unknown2);
    vu_data = vu_data_buffer[DBuffID];
    return result;
}

PC_OVERRIDE int CVisualMDTVu1::DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program,
                                       u_long128 *draw_state, int unknown1, int unknown2) {
    vu_data = vu_data_buffer[DBuffID];
    return CVisualVu1::DrawVu1(static_cast<u_int *>(nullptr), matrix, info, program, draw_state, unknown1, unknown2);
}

PC_OVERRIDE int CVisualMDTVu1::RemakeData(u_int *block) {
    if (data == nullptr) {
        return 0;
    }
    return CreateVUdataFromMDTRemake(vu_data_buffer[DBuffID], data, 1);
}
