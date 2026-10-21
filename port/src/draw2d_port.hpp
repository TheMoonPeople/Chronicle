#pragma once

#include <libgraph.h>

#include <span>

#include "gfx/gfx.hpp"
#include "texture_port.hpp"

class CTexture;
class CRect_i_;

// Shared by the 2D replacement units (snd, gameutil_sprite, clsmes, spritetable, dispctrl,
// editloop_sprite): sprite state, texture resolution and the mapping into the current target.
namespace draw2d {

// The services the 2D units take from units other phases own. Tests swap them to run the 2D
// units without the 3D path or the texture manager.
struct Services {
    gfx::DrawState (*draw_state)();
    PortTextureRef (*texture)(u_long tex0, u_long tex1);
    CTexture *(*find_texture)(const char *name);
    // The GS register writes the sprites leave behind; null writes mglib's shadow, as retail.
    void (*set_test)(sceGsTest *test);
    void (*set_zbuf)(sceGsZbuf *zbuf);
    void (*set_alpha)(sceGsAlpha *alpha);
    void (*set_texa)(sceGsTexa *texa);
};

const Services &Get();
// Null restores the real services.
void SetServices(const Services *services);

struct Texture {
    gfx::TextureBinding binding;
    // Texel rows per GS texel row: the frame buffer's rows are field rows.
    float v_scale = 1.0f;
};

gfx::Filter FilterFromTex1(u_long tex1);

// Resolves TEX0 for sampling. A TEX0 naming the frame being drawn samples a snapshot of it. TCC=0
// on a texture without alpha leaves the vertex alpha alone. False when nothing can be sampled.
bool Resolve(u_long tex0, u_long tex1, gfx::DrawState &state, Texture &texture);

// The current state with what every 2D sprite writes over it: blending on, no alpha test, depth
// ALWAYS without writes, no fog.
gfx::DrawState SpriteState();
// Sprites put TEST and ZBUF back to mglib's shadows, not to what they replaced.
void RestoreTestZbuf();

// Logical rows (640x480) to rows of the current target. A render target holds the GS's texel
// rows, which are field rows: the GS halves logical y on its way into any buffer.
float RowScale();

gfx::Vertex2D Vertex(float x, float y, float z, float u, float v, u_char r, u_char g, u_char b, u_char a,
                     u_char fog = 0xFF);

// y in logical rows; both are mapped to the current target before drawing.
void DrawTextured(gfx::Primitive primitive, std::span<gfx::Vertex2D> vertices, u_long tex0, u_long tex1,
                  gfx::DrawState state);
void DrawUntextured(gfx::Primitive primitive, std::span<gfx::Vertex2D> vertices, const gfx::DrawState &state);

// set2DSprite's turned sprite with its corners scale times as far from where the pivot lands.
void TurnedSprite(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y,
                  float angle, float scale);

} // namespace draw2d

// What setbilinear last set (retail's linear__2): the filter the 2D units' sprite batches use.
int PortBilinear();
