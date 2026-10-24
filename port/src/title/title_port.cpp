#include "title_port.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

#include "draw2d_port.hpp"
#include "draw3d.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "screen.hpp"
#include "snd.hpp"
#include "texture.hpp"

// The title units declare their own rectangle template where every other unit takes CRect_i_.
// MWCC mangled both the same, so retail linked the title's calls to the shared definitions; clang
// mangles CRect<int> apart, so these give the title's spelling a body (clothread.cpp has MoveImageTest's).
template <class T>
class CRect {
public:
    T x;
    T y;
    T w;
    T h;
};

class RenderInfo;

namespace {

CRect_i_ Rect(const CRect<int> &rect) { return CRect_i_(rect.x, rect.y, rect.w, rect.h); }

// TEX1 0x61: LCM 1, linear both ways, as setbilinear(1) leaves the title's sprites.
constexpr u_long kFogTex1 = 0x61;

gfx::TextureHandle fog_frame = gfx::kNullTexture;
gfx::LogicalRect   fog_layout = {};

gfx::TextureHandle FogFrameForLayout(const gfx::LogicalRect &visible) {
    if (visible.y >= 0.0f || visible.x != fog_layout.x || visible.y != fog_layout.y ||
        visible.w != fog_layout.w || visible.h != fog_layout.h) {
        // Fall back to the continuously refreshed frame_image until this layout is captured.
        fog_frame = gfx::kNullTexture;
    }
    return fog_frame == gfx::FindNamedRenderTarget("title fog frame") ? fog_frame : gfx::kNullTexture;
}

} // namespace

// The previous frame is 640x480 logical; its TEX0 counts field rows, as the frame buffer's did.
void TitlePortFeedback(u_char alpha) {
    sceGsTex0 back;
    MGGetFBuffBackTex(&back);
    CTexture texture;
    texture.tex0 = std::bit_cast<u_long>(back);
    set2DSprite(Vif1Packet, &texture, CRect_i_(0, 0, 640, SCREEN_HEIGHT), CRect_i_(0, 0, 640, SCREEN_HALF_HEIGHT), 128,
                128, 128, alpha);
}

/**
 * Captures the title background for feedback beyond the logical frame's vertical edges.
 */
void TitlePortCaptureFog() {
    const gfx::LogicalRect visible = gfx::VisibleLogicalRect(gfx::kMainTarget);
    fog_frame = gfx::kNullTexture;
    if (visible.y >= 0.0f) {
        return;
    }
    gfx::TextureHandle frame = gfx::NamedRenderTarget("title fog frame", 640, 480, false, false, true);
    if (frame != gfx::kNullTexture) {
        gfx::SnapshotFrame(frame);
        fog_layout = visible;
        fog_frame = frame;
    }
}

// The title's fog is a feedback loop: each tick the last frame's copy is drawn back in bands that
// squeeze 45 rows into 42, so whatever each pass does to the picture compounds. On the PS2 each
// pass blurred it. The GS samples a pixel at its top-left corner, half a texel short of where the
// renderer here samples, so a band blended neighbouring texels of the 640-wide field copy about
// evenly. MGStretchMoveImage's copy into frame_image was also offset by a different part of a row
// on each field (dyy and the half-line offset), which put the copy 1.125 rows below the band's
// rows on one field and 1.375 above on the other. And a band's edge was a step between two field
// rows, each two rows of the frame tall. The renderer keeps frame_image at the window's resolution
// and copies it exactly, so the same bands come out sharp: the smear shows as streaks and the
// edges between bands as hairlines.
//
// Only the fog is softened, and here rather than in set2DSprite or MGStretchMoveImage, which every
// other sprite and copy share. The bands are drawn eight times over, as taps that average to
// retail's footprint: across, half a texel step back and half a texel either side; down, half a
// row step back, each field's copy offset, and half a field row either side. Both fields are drawn
// at once since alternating them tick by tick, as the interlaced picture did, would shimmer on a
// progressive one. A tap moves down the texture by moving the bands up the screen, so the edges
// between bands land on different rows for each tap and fade into each other; an edge on the
// frame's own border stays put and the tap moves the texture there instead, stopping at the last
// row of the copy: frame_image is taller than the field and black below it. The taps are blended
// in turn over every band, with alphas that give each an eighth of the band's alpha in the result.
void TitlePortFog(std::span<const TitleFogBand> bands) {
    CTexture *texture = TexManager.GetTexture(const_cast<char *>("frame_image"), -1);
    if (texture == nullptr) {
        return;
    }

    const gfx::LogicalRect    visible = gfx::VisibleLogicalRect(gfx::kMainTarget);
    const gfx::TextureHandle  full_frame = FogFrameForLayout(visible);
    const bool                full_height = full_frame != gfx::kNullTexture;
    const float               last_texel = (full_height ? (visible.y + visible.h) * 0.5f : SCREEN_HALF_HEIGHT_F) - 0.5f;
    const gfx::TextureBinding binding{.texture = full_frame, .ignore_alpha = true};

    constexpr int   taps = 8;
    constexpr float across[2] = {-0.5f, 0.5f};
    constexpr float field_offset[2] = {1.125f, -1.375f};
    constexpr float field_step[2] = {-0.5f, 0.0f};
    constexpr float down[2] = {-0.5f, 0.5f};

    for (int tap = 0; tap < taps; tap++) {
        for (const TitleFogBand &band : bands) {
            const float  u_step = static_cast<float>(band.texel.width) / static_cast<float>(band.screen.width);
            const float  v_step = static_cast<float>(band.texel.height) / static_cast<float>(band.screen.height);
            const float  du = -0.5f * u_step + across[tap & 1];
            const float  dv = field_offset[(tap >> 1) & 1] + 2.0f * v_step * field_step[(tap >> 1) & 1] + down[tap >> 2];
            const float  shift = -dv / v_step;
            const float  share = static_cast<float>(band.alpha) / 128.0f / static_cast<float>(taps);
            const float  blend = share / (1.0f - static_cast<float>(taps - 1 - tap) * share);
            const u_char a = static_cast<u_char>(std::lround(std::clamp(blend, 0.0f, 1.0f) * 128.0f));

            const float top = static_cast<float>(band.screen.y);
            const float bottom = static_cast<float>(band.screen.y + band.screen.height);
            const float x0 = static_cast<float>(band.screen.x);
            const float x1 = static_cast<float>(band.screen.x + band.screen.width);
            const float first_row = full_height ? visible.y : top;
            const float last_row = full_height ? visible.y + visible.h : bottom;
            const float y0 = band.screen.y <= 0 ? first_row : top + shift;
            const float y1 = band.screen.y + band.screen.height >= SCREEN_HEIGHT ? last_row : bottom + shift;
            const float u0 = static_cast<float>(band.texel.x) + du;
            const float u1 = static_cast<float>(band.texel.x + band.texel.width) + du;
            const float v0 = static_cast<float>(band.texel.y) + (y0 - top - shift) * v_step;
            const float v1 = std::min(static_cast<float>(band.texel.y) + (y1 - top - shift) * v_step,
                                      last_texel);
            if (y1 <= y0) {
                continue;
            }

            std::array<gfx::Vertex2D, 4> quad = {
                draw2d::Vertex(x0, y0, 0.0f, u0, v0, 0x80, 0x80, 0x80, a),
                draw2d::Vertex(x1, y0, 0.0f, u1, v0, 0x80, 0x80, 0x80, a),
                draw2d::Vertex(x1, y1, 0.0f, u1, v1, 0x80, 0x80, 0x80, a),
                draw2d::Vertex(x0, y1, 0.0f, u0, v1, 0x80, 0x80, 0x80, a),
            };
            if (full_height) {
                for (gfx::Vertex2D &vertex : quad) {
                    vertex.v *= 2.0f;
                }
                gfx::Draw2D(gfx::Primitive::Quads, quad, binding, draw2d::SpriteState());
            } else {
                draw2d::DrawTextured(gfx::Primitive::Quads, quad, texture->tex0, kFogTex1, draw2d::SpriteState());
            }
        }
    }
    draw2d::RestoreTestZbuf();
}

float TitlePortBackdropScale() {
    return std::max(1.0f, Draw3DVisibleExtent().half_width / (kScreenWidth * 0.5f));
}

float TitlePortBackdropHeightScale() {
    return std::max(1.0f, gfx::VisibleLogicalRect(gfx::kMainTarget).h / kScreenHeight);
}

void TitlePortCard(CTexture *texture, float angle) {
    draw2d::TurnedSprite(texture, CRect_i_(320, 224, 768, 768), CRect_i_(0, 0, 768, 768), 384, 384, angle,
                         TitlePortBackdropScale());
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen, const CRect<int> &texel,
                 u_char alpha) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), alpha);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen, const CRect<int> &texel,
                 u_char red, u_char green, u_char blue, u_char alpha) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), red, green, blue, alpha);
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &screen, const CRect<int> &texel,
                 int pivot_x, int pivot_y, float angle) {
    set2DSprite(packet, texture, Rect(screen), Rect(texel), pivot_x, pivot_y, angle);
}

void MGMoveImage(sceGsTex0 *src, const CRect<int> &rect, sceGsTex0 *dst, int dst_x, int dst_y, int direction) {
    MGMoveImage(src, Rect(rect), dst, dst_x, dst_y, direction);
}

void MGStretchMoveImage(sceGsTex0 *src, const CRect<int> &src_rect, sceGsTex0 *dst, const CRect<int> &dst_rect) {
    MGStretchMoveImage(src, Rect(src_rect), dst, Rect(dst_rect));
}

void MGFillBox(const CRect<int> &rect, u_char r, u_char g, u_char b, u_char a) { MGFillBox(Rect(rect), r, g, b, a); }

// The title's own CWater declares DrawVu1 with a u_long128 * where water.hpp has a RenderInfo *;
// both reach the one body retail has.
class CWater {
public:
    int DrawVu1(RenderInfo *info, sceVif1Packet *packet, u_long128 *parent_info);
};

extern "C" int DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(CWater *water, RenderInfo *info,
                                                                sceVif1Packet *draw_packet, void *parent_info);

int CWater::DrawVu1(RenderInfo *info, sceVif1Packet *packet, u_long128 *parent_info) {
    return DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(this, info, packet, parent_info);
}
