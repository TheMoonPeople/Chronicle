#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "gfx/gfx.hpp"
#include "render/gs_state.hpp"
#include "render/texture_registry.hpp"

namespace dc::game {
struct RenderInfo;
} // namespace dc::game

namespace dc::render {

/**
 * The GS registers the game last sent (MGSetGs*, and the clears, fills, stretches and shadow
 * calls that leave them set) and the scissor MGSetWindowRect set, in the logical frame.
 */
struct MGPortRegisters {
    sceGsTest        test;   /**< TEST register. */
    sceGsZbuf        zbuf;   /**< ZBUF register. */
    sceGsAlpha       alpha;  /**< ALPHA register. */
    sceGsTexa        texa;   /**< TEXA register. */
    gfx::LogicalRect window; /**< Scissor rect in logical frame units. */
};

/**
 * Returns the registers the game's 3D drawing currently uses.
 */
MGPortRegisters &MGPortCurrent();

/**
 * Restores TEST, ZBUF and ALPHA to mgPixelTest, mgZBuffer and mgAlpha, as SetEnv and every
 * retail helper that borrows them do at their end.
 */
void MGPortRestoreRegisters();

/** Looks up the texture a TEX0 register value refers to. */
using Draw3DTex0Resolver = PortTextureRef (*)(u_long tex0);
/** Looks up the texture a TexManager handle refers to. */
using Draw3DHandleResolver = PortTextureRef (*)(int handle);

/**
 * Replaces the texture lookups, so tests can stand in for the texture phase's resolver.
 */
void Draw3DSetResolvers(Draw3DTex0Resolver tex0, Draw3DHandleResolver handle);

/**
 * Whether Draw3DSetResolvers installed a TEX0 resolver in place of the texture registry's.
 */
bool Draw3DHasTex0Resolver();

/**
 * Resolves a TEX0 register value through the current resolver.
 */
PortTextureRef Draw3DResolveTex0(u_long tex0);

/**
 * Resolves a TexManager handle through the current resolver.
 */
PortTextureRef Draw3DResolveHandle(int handle);

/**
 * The texture the game last copied the frame into (MGMoveImage, MGStretchMoveImage), if it is
 * still alive: the water samples it, as retail's "work" aliased the copy's VRAM.
 */
gfx::TextureHandle Draw3DLastFrameCopy();

/**
 * Records a copy of the main target into copy: the last frame copy, the copy count and a pending
 * Draw3DFramePicture.
 */
void Draw3DNoteFrameCopy(gfx::TextureHandle copy);

/**
 * Sets the last frame copy alone (a PSMCT24 stretch of the frame).
 */
void Draw3DSetLastFrameCopy(gfx::TextureHandle copy);

/**
 * How many times the game has copied the frame with MGMoveImage.
 */
unsigned Draw3DFrameCopies();

/**
 * The frame as it stood at the last MGMoveImage, whole and at the frame's own resolution (the
 * game's copy is a field, half its rows): what the water refracts.
 */
gfx::TextureHandle Draw3DFramePicture();

/**
 * Whether shadow passes 1 and 2 may draw: only between MGBeginDrawShadow and MGEndDrawShadow,
 * counting into the stencil of the target being drawn; outside them they are dropped.
 */
bool Draw3DShadowTargetActive();

/**
 * Opens (MGBeginDrawShadow) or closes (MGBeginFrame, MGEndDrawShadow) the shadow target.
 */
void Draw3DSetShadowTargetActive(bool active);

/**
 * Makes state a shadow counting draw: no colour, depth or blend, the stencil incremented (or
 * decremented, when subtract) wherever the depth test passes. Retail's Cs + Cd and Cd - Cs of 1.
 */
void Draw3DShadowCount(gfx::DrawState &state, bool subtract);

/**
 * The shadow microprogram the running MGDrawShadow* call stands for. All three extrude caster
 * triangles into prisms down to the shadow plane.
 */
enum class Draw3DShadowProgram {
    Every,         /**< Vu_shadow (MGDrawShadowFast): every triangle of the shadow mesh. */
    AwayFromLight, /**< Vu_shadow3 (MGDrawShadowFast2): triangles facing away from light 0. */
    Clipped,       /**< Vu_shadow2 (MGDrawShadow): CreateVUdataShadowCLIP's records, near-capped. */
};

/**
 * Returns the shadow microprogram of the running MGDrawShadow* call.
 */
Draw3DShadowProgram Draw3DCurrentShadowProgram();

/**
 * Sets the shadow microprogram the MGDrawShadow* call about to draw stands for.
 */
void Draw3DSetShadowProgram(Draw3DShadowProgram program);

/**
 * Multiplies 4x4 matrices, column-major [column][row] as sceVu0FMATRIX.
 */
void Draw3DMul(float out[4][4], const float a[4][4], const float b[4][4]);

/**
 * Half the extent of what the current render target shows, about the frame's centre, in logical
 * frame units (480 rows). Culls test against it.
 */
struct Draw3DExtent {
    float half_width;  /**< Half the visible width. */
    float half_height; /**< Half the visible height. */
};

/**
 * Returns the logical frame's half extent (320 by SCREEN_HALF_HEIGHT_F), or more where the
 * target shows past the frame (gfx::VisibleLogicalRect).
 */
Draw3DExtent Draw3DVisibleExtent();

/**
 * Builds the transform from eye space (what view_scaled produces: x right, y down, z forward) to
 * clip space of the current render target.
 */
void Draw3DEyeToClip(const game::RenderInfo &info, float clip[4][4]);

/**
 * Fills the mvp, normal matrix, lights, ambient and fog of one draw, as the VU1 header carried
 * them. On shadow passes 1 and 2 the planar projection info.shadow sits between the model and
 * the view. transform, when given, receives the factors of mvp for the display list's
 * interpolation.
 */
void Draw3DSceneConstants(gfx::MeshConstants &constants, const game::RenderInfo &info, const float model[4][4],
                          gfx::MeshTransform *transform = nullptr);

/** World units an object may move in one tick and still be interpolated; further is a teleport. */
inline constexpr float kDraw3DTeleportDistance = 200.0f;

/**
 * Identity of the mesh draws a CFrame makes, for interpolating them between ticks: set around the
 * frame's visual draw, restored after. blend_vertices is for an object whose immediate vertices
 * are the same points of the same space every tick (the cloth), which are then interpolated too.
 */
class Draw3DIdentityScope {
public:
    Draw3DIdentityScope(const void *object, float teleport_distance, bool blend_vertices = false);
    ~Draw3DIdentityScope();

    Draw3DIdentityScope(const Draw3DIdentityScope &) = delete;
    Draw3DIdentityScope &operator=(const Draw3DIdentityScope &) = delete;

private:
    gfx::InterpKey key;
    bool           no_interpolation;
};

/**
 * Applies what SetMaterial uploaded: diffuse (alpha in w), ambient and specular. The diffuse
 * colours the ambient light as well as the lights, as VU1 lit with it; ambient is not used.
 */
void Draw3DMaterial(gfx::MeshConstants &constants, const game::RenderInfo &info, const float *diffuse,
                    const float *ambient, const float *specular);

/**
 * Builds a draw state from the current registers, PRMODE.ABE on, the window scissor on the
 * current target, and fog when asked.
 */
gfx::DrawState Draw3DState(const game::RenderInfo &info, bool fog);

/** Quadwords of the game's vu_data block that tag a built visual, keyed by the block's address. */
inline constexpr unsigned kDraw3DBlockQuads = 4;

/**
 * One strip of a built visual: an index range and the material and texture it draws with.
 */
struct Draw3DStrip {
    uint32_t first_index = 0;                        /**< First index into the visual's indices. */
    uint32_t index_count = 0;                        /**< Number of indices. */
    float    diffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};  /**< Diffuse colour, alpha in w. */
    float    ambient[4] = {1.0f, 1.0f, 1.0f, 1.0f};  /**< Ambient colour. */
    float    specular[4] = {0.0f, 0.0f, 0.0f, 0.0f}; /**< Specular colour. */
    int      texture = -1;                           /**< TexManager handle; -1 draws untextured. */
    /**
     * The name the handle held when the visual was built. Retail bakes the texture's VRAM address,
     * which a reloaded block refills in place; a table slot is not kept, so the draw looks the name
     * up again when the slot has since been given to another texture.
     */
    std::string texture_name;
    u_long      tex0 = 0;                  /**< TEX0 register value. */
    u_long      tex1 = 0;                  /**< TEX1 register value; 0 uses the environment's. */
    bool        ground_transition = false; /**< Blended ground transition band, skipped by projections. */
    bool        ground_world_uv = false;   /**< Whether texture coordinates come from world position. */
    float       ground_uv_period = 160.0f; /**< World units per texture repeat for world UVs. */
    bool        ground_uv_flip_v = false;  /**< Whether world UVs flip the v axis. */
};

/**
 * One adjacent set of strips with compatible materials and contiguous indices.
 * Runs keep the mesh's original strip order for drawing.
 */
struct Draw3DStripRun {
    std::size_t first_strip = 0;
    std::size_t strip_count = 0;
    uint32_t    index_count = 0;
};

/**
 * A built visual: the mesh, strips and geometry a tagged vu_data block stands for.
 */
struct Draw3DVisual {
    gfx::MeshHandle             mesh = gfx::kNullMesh; /**< Uploaded mesh. */
    std::vector<Draw3DStrip>    strips;                /**< Strips in draw order. */
    std::vector<Draw3DStripRun> strip_runs;            /**< Cached merged strip runs. */
    std::vector<gfx::Vertex3D>  vertices;              /**< Vertices as built. */
    std::vector<uint32_t>       indices;               /**< Kept only for immediate (per-frame) visuals. */
    /**
     * A shadow mesh's triangles' neighbours across each edge (corner i to i + 1): the one other
     * triangle sharing it, as triangle * 4 + its edge, else -1.
     */
    std::vector<std::array<int32_t, 3>> shadow_neighbours;
    bool                                vertex_colour = false;              /**< Whether vertices carry their own colour. */
    bool                                immediate = false;                  /**< Whether the geometry is drawn per frame. */
    bool                                cache_strip_runs_on_finish = false; /**< Model builders opt in before finalization. */
    bool                                strip_runs_cached = false;          /**< Whether cached runs match the current strips. */
};

/**
 * Returns the visual registered for block, or nullptr when the block holds none.
 */
Draw3DVisual *Draw3DFindVisual(const void *block);

/**
 * Creates a fresh record for block, replacing (and destroying) what an earlier build left there.
 * A transient record lives until the next MGBeginFrame, a lasting one until its block loses its tag.
 */
Draw3DVisual &Draw3DRegisterVisual(unsigned int *block, bool transient);

/**
 * Drops transient records every frame and sweeps cleared blocks every sixteen frames; lookups
 * reject cleared blocks immediately.
 */
void Draw3DSweepVisuals();

/**
 * Uploads the record's vertices and indices as a mesh, or keeps them for immediate draws.
 */
void Draw3DFinishVisual(Draw3DVisual &visual);

/**
 * Discards cached strip boundaries after a visual's materials or strip layout changes.
 * Future draws use the current strip data directly.
 */
void Draw3DInvalidateStripRuns(Draw3DVisual &visual);

/**
 * Draws a built visual with the given model matrix and VU1 program.
 */
void Draw3DDrawVisual(const Draw3DVisual &visual, const float model[4][4], const game::RenderInfo &info, int program);

/**
 * Appends a strip's indices as a triangle list, every triangle wound as the strip's first.
 */
void Draw3DStripToList(std::vector<uint32_t> &indices, uint32_t first_vertex, uint32_t count);

} // namespace dc::render
