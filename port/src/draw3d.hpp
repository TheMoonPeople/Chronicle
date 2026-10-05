#pragma once

#include <cstdint>
#include <vector>

#include "gfx/gfx.hpp"
#include "mglib_port.hpp"
#include "texture_port.hpp"

struct RenderInfo;

// What the 3D replacement units (mglib, frame_draw, visualvu1, visualshadow, cloth_draw,
// water_draw) share with each other and with their tests, and nobody else needs.

// The GS registers the game last sent (MGSetGs*, and the clears, fills, stretches and shadow
// calls that leave them set as retail does) and the scissor MGSetWindowRect set, in the logical
// frame.
struct MGPortRegisters {
    sceGsTest        test;
    sceGsZbuf        zbuf;
    sceGsAlpha       alpha;
    sceGsTexa        texa;
    gfx::LogicalRect window;
};

MGPortRegisters &MGPortCurrent();
// TEST, ZBUF and ALPHA back to mgPixelTest, mgZBuffer and mgAlpha, as SetEnv and every retail
// helper that borrows them end.
void MGPortRestoreRegisters();

// The texture lookups, swappable so tests can stand in for the texture phase's resolver.
using Draw3DTex0Resolver = PortTextureRef (*)(u_long tex0);
using Draw3DHandleResolver = PortTextureRef (*)(int handle);
void           Draw3DSetResolvers(Draw3DTex0Resolver tex0, Draw3DHandleResolver handle);
PortTextureRef Draw3DResolveTex0(u_long tex0);
PortTextureRef Draw3DResolveHandle(int handle);

// The texture the game last copied the frame into (MGMoveImage, MGStretchMoveImage), if it is
// still alive: the water samples it, as retail's "work" aliased the copy's VRAM.
gfx::TextureHandle Draw3DLastFrameCopy();
// How many times the game has copied the frame with MGMoveImage, and the frame as it stood at the
// last of them, whole and at the frame's own resolution (the game's copy is a field, half its
// rows): what the water refracts.
unsigned           Draw3DFrameCopies();
gfx::TextureHandle Draw3DFramePicture();

// Shadow passes 1 and 2 draw only between MGBeginDrawShadow and MGEndDrawShadow, into the target
// the former picked; with no target they are dropped rather than brightening the frame.
bool Draw3DShadowTargetActive();

// The shadow microprogram the running MGDrawShadow* call stands for. All three extrude caster
// triangles into prisms down to the shadow plane: Vu_shadow (MGDrawShadowFast) every triangle of
// the shadow mesh, Vu_shadow3 (MGDrawShadowFast2) those facing away from light 0, Vu_shadow2
// (MGDrawShadow) the records CreateVUdataShadowCLIP selects, with a near-plane cap.
enum class Draw3DShadowProgram {
    Every,
    AwayFromLight,
    Clipped,
};
Draw3DShadowProgram Draw3DCurrentShadowProgram();

// 4x4 matrices are column-major [column][row], as sceVu0FMATRIX.
void Draw3DMul(float out[4][4], const float a[4][4], const float b[4][4]);

// Half the extent of what the current render target shows, about the frame's centre, in logical
// frame units (480 rows): the logical frame's (320 by SCREEN_HALF_HEIGHT_F), or more where the
// target shows past the frame (gfx::VisibleLogicalRect). Culls test against it.
struct Draw3DExtent {
    float half_width;
    float half_height;
};

Draw3DExtent Draw3DVisibleExtent();

// Eye space (what view_scaled produces: x right, y down, z forward) to clip space of the current
// render target.
void Draw3DEyeToClip(const RenderInfo &info, float clip[4][4]);

// mvp, normal matrix, lights, ambient and fog of one draw, as the VU1 header carried them. On
// shadow passes 1 and 2 the planar projection info.shadow sits between the model and the view.
// transform, when given, receives the factors of mvp for the display list's interpolation.
void Draw3DSceneConstants(gfx::MeshConstants &constants, const RenderInfo &info, const float model[4][4],
                          gfx::MeshTransform *transform = nullptr);

// World units an object may move in one tick and still be interpolated; further is a teleport.
inline constexpr float kDraw3DTeleportDistance = 200.0f;

// Identity of the mesh draws a CFrame makes, for interpolating them between ticks: set around the
// frame's visual draw, restored after. blend_vertices is for an object whose immediate vertices
// are the same points of the same space every tick (the cloth), which are then interpolated too.
class Draw3DIdentityScope {
public:
    Draw3DIdentityScope(const void *object, float teleport_distance, bool blend_vertices = false);
    ~Draw3DIdentityScope();

    Draw3DIdentityScope(const Draw3DIdentityScope &) = delete;
    Draw3DIdentityScope &operator=(const Draw3DIdentityScope &) = delete;

private:
    gfx::InterpKey key_;
    bool           no_interpolation_;
};

// What SetMaterial uploaded: diffuse (alpha in w), ambient and specular. The diffuse colours the
// ambient light as well as the lights, as VU1 lit with it; ambient is not used.
void Draw3DMaterial(gfx::MeshConstants &constants, const RenderInfo &info, const float *diffuse,
                    const float *ambient, const float *specular);
// Current registers, PRMODE.ABE on, the window scissor on the current target, fog when asked.
gfx::DrawState Draw3DState(const RenderInfo &info, bool fog);

// A built visual. The game's vu_data block holds kDraw3DBlockQuads quadwords with a tag; the record
// lives here, keyed by the block's address.
inline constexpr unsigned kDraw3DBlockQuads = 4;

struct Draw3DStrip {
    uint32_t first_index = 0;
    uint32_t index_count = 0;
    float    diffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float    ambient[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float    specular[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int      texture = -1; // TexManager handle; -1 draws untextured
    // The name the handle held when the visual was built. Retail bakes the texture's VRAM address,
    // which a reloaded block refills in place; a table slot is not kept, so the draw looks the name
    // up again when the slot has since been given to another texture.
    char     texture_name[32] = {};
    u_long   tex0 = 0;
    u_long   tex1 = 0;
};

struct Draw3DVisual {
    gfx::MeshHandle            mesh = gfx::kNullMesh;
    std::vector<Draw3DStrip>   strips;
    std::vector<gfx::Vertex3D> vertices;
    std::vector<uint32_t>      indices; // kept only for immediate (per-frame) visuals
    bool                       vertex_colour = false;
    bool                       immediate = false;
};

Draw3DVisual *Draw3DFindVisual(const void *block);
// A fresh record for block, replacing (and destroying) what an earlier build left there. A
// transient record lives until the next MGBeginFrame, a lasting one until its block loses its tag.
Draw3DVisual &Draw3DRegisterVisual(unsigned int *block, bool transient);
// MGBeginFrame's: drops transient records and those whose block was cleared or reused.
void Draw3DSweepVisuals();
// Uploads the record's vertices and indices as a mesh, or keeps them for immediate draws.
void Draw3DFinishVisual(Draw3DVisual &visual);
void Draw3DDrawVisual(const Draw3DVisual &visual, const float model[4][4], const RenderInfo &info, int program);

// Strip index order into a triangle list, every triangle wound as the strip's first.
void Draw3DStripToList(std::vector<uint32_t> &indices, uint32_t first_vertex, uint32_t count);
