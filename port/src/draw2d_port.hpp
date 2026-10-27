#pragma once

#include <libgraph.h>

#include <span>

#include "gfx/gfx.hpp"
#include "texture_port.hpp"

class CTexture;
class CRect_i_;

/**
 * Shared by the 2D replacement units (snd, gameutil_sprite, clsmes, spritetable, dispctrl,
 * editloop_sprite): sprite state, texture resolution and the mapping into the current target.
 */
namespace draw2d {

/**
 * The services the 2D units take from units other phases own. Tests swap them to run the 2D
 * units without the 3D path or the texture manager.
 */
struct Services {
    gfx::DrawState (*draw_state)();                      /**< Current mglib draw state. */
    PortTextureRef (*texture)(u_long tex0, u_long tex1); /**< Texture a TEX0/TEX1 pair names. */
    CTexture *(*find_texture)(const char *name);         /**< Texture manager lookup by name. */
    void (*set_test)(sceGsTest *test);                   /**< TEST write; null writes mglib's shadow. */
    void (*set_zbuf)(sceGsZbuf *zbuf);                   /**< ZBUF write; null writes mglib's shadow. */
    void (*set_alpha)(sceGsAlpha *alpha);                /**< ALPHA write; null writes mglib's shadow. */
    void (*set_texa)(sceGsTexa *texa);                   /**< TEXA write; null writes mglib's shadow. */
};

/**
 * The services currently in use.
 */
const Services &Get();

/**
 * Replaces the services; null restores the real ones.
 */
void SetServices(const Services *services);

/**
 * A resolved texture ready for sampling by a 2D draw.
 */
struct Texture {
    gfx::TextureBinding binding;        /**< Texture, filter and wrap to sample with. */
    float               v_scale = 1.0f; /**< Texel rows per GS texel row: frame buffer rows are field rows. */
};

/**
 * The sampling filter TEX1's MMAG bit selects.
 */
gfx::Filter FilterFromTex1(u_long tex1);

/**
 * Resolves TEX0 for sampling. A TEX0 naming the frame being drawn samples a snapshot of it. TCC=0
 * on a texture without alpha leaves the vertex alpha alone. False when nothing can be sampled.
 */
bool Resolve(u_long tex0, u_long tex1, gfx::DrawState &state, Texture &texture);

/**
 * The current state with what every 2D sprite writes over it: blending on, no alpha test, depth
 * ALWAYS without writes, no fog.
 */
gfx::DrawState SpriteState();

/**
 * Puts TEST and ZBUF back to mglib's shadows, not to what the sprites replaced.
 */
void RestoreTestZbuf();

/**
 * Logical rows (640x480) to rows of the current target. A render target holds the GS's texel
 * rows, which are field rows: the GS halves logical y on its way into any buffer.
 */
float RowScale();

/**
 * Builds a 2D vertex from position, texel coordinates, RGBA colour and fog.
 */
gfx::Vertex2D Vertex(float x, float y, float z, float u, float v, u_char r, u_char g, u_char b, u_char a,
                     u_char fog = 0xFF);

/**
 * Draws textured vertices; y is in logical rows and is mapped to the current target.
 */
void DrawTextured(gfx::Primitive primitive, std::span<gfx::Vertex2D> vertices, u_long tex0, u_long tex1,
                  gfx::DrawState state);

/**
 * Draws untextured vertices; y is in logical rows and is mapped to the current target.
 */
void DrawUntextured(gfx::Primitive primitive, std::span<gfx::Vertex2D> vertices, const gfx::DrawState &state);

/**
 * set2DSprite's turned sprite with its corners scale times as far from where the pivot lands.
 */
void TurnedSprite(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y,
                  float angle, float scale);

} // namespace draw2d

/**
 * What setbilinear last set (retail's linear__2): the filter the 2D units' sprite batches use.
 */
int PortBilinear();
