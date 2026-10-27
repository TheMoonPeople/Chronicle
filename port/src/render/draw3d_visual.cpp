#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mglib.hpp"
#include "platform/load_work_profile.hpp"
#include "render/draw3d.hpp"
#include "renderinfo.hpp"
#include "texture.hpp"

namespace dc::render {

using namespace dc::game;

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

void BuildStripRuns(Draw3DVisual &visual) {
    visual.strip_runs.clear();
    for (size_t strip_index = 0; strip_index < visual.strips.size();) {
        const size_t       first_strip = strip_index;
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
        visual.strip_runs.push_back(
            {.first_strip = first_strip, .strip_count = strip_index - first_strip, .index_count = index_count});
    }
    visual.strip_runs_cached = true;
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

} // namespace

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
        const auto *block = static_cast<const unsigned int *>(entry.first);
        if (block[8] == kBlockTag) {
            return false;
        }
        Release(*entry.second);
        return true;
    });
}

void Draw3DFinishVisual(Draw3DVisual &visual) {
    const dc::platform::load_profile::Span profile(dc::platform::load_profile::Kind::Mesh);
    if (visual.immediate) {
        return;
    }
    if (visual.cache_strip_runs_on_finish) {
        BuildStripRuns(visual);
    }
    if (visual.vertices.empty() || visual.indices.empty()) {
        return;
    }
    visual.mesh = gfx::CreateMesh(visual.vertices, visual.indices);
    visual.indices.clear();
    visual.indices.shrink_to_fit();
}

void Draw3DInvalidateStripRuns(Draw3DVisual &visual) {
    visual.strip_runs.clear();
    visual.strip_runs_cached = false;
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
void Draw3DDrawVisual(const Draw3DVisual &visual, const float model[4][4], const RenderInfo &info,
                      [[maybe_unused]] int program) {
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
    gfx::MeshTransform transform = {};
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
    if (projected || (info.shadow_pass == 8 && Draw3DShadowTargetActive())) {
        // The first of the two passes SetShadowData hands VU1, counted once wherever the
        // projection covers; a shade frame covers the volumes' stencil the same way.
        Draw3DShadowCount(state, false);
    }

    static const float kWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t           base_flags = constants.flags;
    size_t             strip_run_index = 0;
    for (size_t strip_index = 0; strip_index < visual.strips.size();) {
        const Draw3DStrip *strip_ptr = nullptr;
        uint32_t           index_count = 0;
        if (visual.strip_runs_cached) {
            const Draw3DStripRun &run = visual.strip_runs[strip_run_index++];
            strip_ptr = &visual.strips[run.first_strip];
            index_count = run.index_count;
            strip_index += run.strip_count;
        } else {
            strip_ptr = &visual.strips[strip_index++];
            index_count = strip_ptr->index_count;
            while (strip_index < visual.strips.size()) {
                const Draw3DStrip &next = visual.strips[strip_index];
                if (strip_ptr->first_index + index_count != next.first_index || !SameMaterial(*strip_ptr, next)) {
                    break;
                }
                index_count += next.index_count;
                ++strip_index;
            }
        }
        const Draw3DStrip &strip = *strip_ptr;
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
                std::string_view(TexManager.GetTexture(handle)->name) != strip.texture_name) {
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
            strip_state.alpha = {.a = 0, .b = 1, .c = 0, .d = 1, .fix = 0x80};
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
}

Draw3DIdentityScope::Draw3DIdentityScope(const void *object, float teleport_distance, bool blend_vertices)
    : key(gfx::CurrentInterpKey()), no_interpolation(gfx::CurrentNoInterpolation()) {
    gfx::SetInterpKey(reinterpret_cast<uintptr_t>(object), false, teleport_distance, blend_vertices);
}

Draw3DIdentityScope::~Draw3DIdentityScope() {
    gfx::SetInterpKey(key, no_interpolation);
}

} // namespace dc::render
