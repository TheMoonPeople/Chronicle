#include <vector>

#include "draw2d_port.hpp"
#include "gameutil.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "texture_port.hpp"

namespace {

// Retail's sprite batches filter with a file-static flag nothing ever sets.
constexpr u_long kBatchTex1 = 0x41;

bool                       g_batch_open = false;
u_long                     g_batch_tex0 = 0;
std::vector<gfx::Vertex2D> g_batch;

} // namespace

// The batch draws when it closes, which is where retail's packet reaches the GS: nothing else may
// write to the packet between Start and End.
PC_OVERRIDE void set2DSprite_Start(sceVif1Packet *packet, CTexture *texture) {
    g_batch.clear();
    g_batch_open = texture != nullptr;
    if (texture != nullptr) {
        g_batch_tex0 = texture->tex0;
    }
}

PC_OVERRIDE void set2DSprite_Core(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                                  u8 red, u8 green, u8 blue, u8 alpha) {
    if (texture == nullptr || !g_batch_open) {
        return;
    }

    const float x0 = static_cast<float>(screen.x);
    const float y0 = static_cast<float>(screen.y);
    const float x1 = static_cast<float>(screen.x + screen.width);
    const float y1 = static_cast<float>(screen.y + screen.height);
    const float u0 = static_cast<float>(texel.x);
    const float v0 = static_cast<float>(texel.y);
    const float u1 = static_cast<float>(texel.x + texel.width);
    const float v1 = static_cast<float>(texel.y + texel.height);

    g_batch.push_back(draw2d::Vertex(x0, y0, 0.0f, u0, v0, red, green, blue, alpha));
    g_batch.push_back(draw2d::Vertex(x1, y0, 0.0f, u1, v0, red, green, blue, alpha));
    g_batch.push_back(draw2d::Vertex(x1, y1, 0.0f, u1, v1, red, green, blue, alpha));
    g_batch.push_back(draw2d::Vertex(x0, y1, 0.0f, u0, v1, red, green, blue, alpha));
}

PC_OVERRIDE void set2DSprite_End(sceVif1Packet *packet, CTexture *texture) {
    if (!g_batch_open) {
        return;
    }

    g_batch_open = false;
    draw2d::DrawTextured(gfx::Primitive::Quads, g_batch, g_batch_tex0, kBatchTex1, draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
    g_batch.clear();
}

PC_OVERRIDE void SetClut(sceVif1Packet *packet, CTexture *texture, i *clut) {
    if (texture == nullptr || clut == nullptr) {
        return;
    }

    const sceGsTex0 tex0 = *reinterpret_cast<const sceGsTex0 *>(&texture->tex0);
    PortLoadClut(static_cast<unsigned>(tex0.CBP), reinterpret_cast<const u_int *>(clut));
}
