#include "snd.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <set>
#include <string>
#include <string_view>
#include <tuple>

#include "credits_text.hpp"
#include "draw2d_port.hpp"
#include "glyph_draw.hpp"
#include "mglib_port.hpp"
#include "rect.hpp"
#include "texture.hpp"
#include "ui_framework/buttons.hpp"

extern unsigned int *snd_read_buf;

namespace {

// Retail's linear__2, which starts on.
int g_linear = 1;

constexpr u_long kTex1Linear = SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0);

u_long SpriteTex1() { return (static_cast<u_long>(g_linear) << 5) | 0x41; }

struct Colour {
    u_char r;
    u_char g;
    u_char b;
    u_char a;
};

Colour ToColour(const spRGBA *colour) { return {.r = colour->r, .g = colour->g, .b = colour->b, .a = colour->a}; }

// A logical rect over a texel rect. The GS draws a sprite up to x + w - 1/16 so that it stops one
// pixel short of x + w; a quad to x + w covers the same pixels here.
std::array<gfx::Vertex2D, 4> RectQuad(const CRect_i_ &screen, const CRect_i_ &texel, Colour colour) {
    const auto x0 = static_cast<float>(screen.x);
    const auto y0 = static_cast<float>(screen.y);
    const auto x1 = static_cast<float>(screen.x + screen.width);
    const auto y1 = static_cast<float>(screen.y + screen.height);
    const auto u0 = static_cast<float>(texel.x);
    const auto v0 = static_cast<float>(texel.y);
    const auto u1 = static_cast<float>(texel.x + texel.width);
    const auto v1 = static_cast<float>(texel.y + texel.height);
    return {
        draw2d::Vertex(x0, y0, 0.0f, u0, v0, colour.r, colour.g, colour.b, colour.a),
        draw2d::Vertex(x1, y0, 0.0f, u1, v0, colour.r, colour.g, colour.b, colour.a),
        draw2d::Vertex(x1, y1, 0.0f, u1, v1, colour.r, colour.g, colour.b, colour.a),
        draw2d::Vertex(x0, y1, 0.0f, u0, v1, colour.r, colour.g, colour.b, colour.a),
    };
}

void RectSprite(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, Colour colour) {
    if (texture == nullptr) {
        return;
    }

    static const bool trace = std::getenv("DC_SPRITE_TRACE") != nullptr;
    if (trace) {
        static std::set<std::tuple<std::string, int, int, int, int>> seen;
        if (seen.emplace(texture->name, texel.x, texel.y, texel.width, texel.height).second) {
            std::println(stderr, "sprite {} texel {} {} {} {} screen {} {} {} {}", std::string_view(texture->name),
                         texel.x, texel.y, texel.width, texel.height, screen.x, screen.y, screen.width, screen.height);
        }
    }

    if (GlyphReplaceSprite(texture->name, screen, texel, colour.a) || credits::DrawBand(texture->name, screen, texel, colour.a) ||
        UI::DrawPaintedPlate(texture->name, screen, texel, colour.r, colour.a)) {
        return;
    }
    std::array<gfx::Vertex2D, 4> quad = RectQuad(screen, texel, colour);
    draw2d::DrawTextured(gfx::Primitive::Quads, quad, texture->tex0, SpriteTex1(), draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
    GlyphOverlaySprite(texture->name, screen, texel, colour.a);
}

// XYZF2 keeps 16 bits of x and y, 24 of z and 8 of the fog coefficient.
gfx::Vertex2D GsVertex(const int *position, float u, float v, Colour colour, bool fog, bool xyz2 = false) {
    const unsigned z = xyz2 ? std::min(static_cast<unsigned>(position[2]), 0xFFFFFFu)
                            : static_cast<unsigned>(position[2]) & 0xFFFFFF;
    return draw2d::Vertex(MGPortLogicalX(position[0] & 0xFFFF), MGPortLogicalY(position[1] & 0xFFFF),
                          MGPortDepth(z), u, v, colour.r, colour.g, colour.b, colour.a,
                          fog ? static_cast<u_char>(position[3]) : 0xFF);
}

gfx::DrawState CurrentState(bool fog) {
    gfx::DrawState state = draw2d::Get().draw_state();
    state.blend = true;
    state.fog = fog;
    state.cull = gfx::CullMode::None;
    return state;
}

void Sprite3D(CTexture *texture, const CRect_i_ &source, int *top_left, int *top_right, int *bottom_left,
              int *bottom_right, Colour colour, bool fog) {
    const auto u0 = static_cast<float>(source.x);
    const auto v0 = static_cast<float>(source.y);
    const auto u1 = static_cast<float>(source.x + source.width);
    const auto v1 = static_cast<float>(source.y + source.height);

    std::array<gfx::Vertex2D, 4> strip = {
        GsVertex(top_left, u0, v0, colour, fog),
        GsVertex(top_right, u1, v0, colour, fog),
        GsVertex(bottom_left, u0, v1, colour, fog),
        GsVertex(bottom_right, u1, v1, colour, fog),
    };
    draw2d::DrawTextured(gfx::Primitive::TriangleStrip, strip, texture->tex0, kTex1Linear, CurrentState(fog));
    draw2d::Get().set_test(nullptr);
}

// The GS draws a sprite at its second vertex's depth and fog.
void Sprite3D(CTexture *texture, const CRect_i_ &source, int *top_left, int *bottom_right, Colour colour,
              bool fog) {
    const std::array<int, 4> top_right = {bottom_right[0], top_left[1], bottom_right[2], bottom_right[3]};
    const std::array<int, 4> bottom_left = {top_left[0], bottom_right[1], bottom_right[2], bottom_right[3]};
    const std::array<int, 4> top_left_at_depth = {top_left[0], top_left[1], bottom_right[2], bottom_right[3]};

    const auto u0 = static_cast<float>(source.x);
    const auto v0 = static_cast<float>(source.y);
    const auto u1 = static_cast<float>(source.x + source.width);
    const auto v1 = static_cast<float>(source.y + source.height);

    std::array<gfx::Vertex2D, 4> quad = {
        GsVertex(top_left_at_depth.data(), u0, v0, colour, fog),
        GsVertex(top_right.data(), u1, v0, colour, fog),
        GsVertex(bottom_right, u1, v1, colour, fog),
        GsVertex(bottom_left.data(), u0, v1, colour, fog),
    };
    draw2d::DrawTextured(gfx::Primitive::Quads, quad, texture->tex0, kTex1Linear, CurrentState(fog));
    draw2d::Get().set_test(nullptr);
}

} // namespace

// Retail aligns through a 32-bit int, which truncates a buffer above 4 GiB.
void SndSetReadBuffer(unsigned int *buffer) {
    const auto address = reinterpret_cast<std::uintptr_t>(buffer);
    const auto misalign = address % 64;
    snd_read_buf = misalign != 0 ? reinterpret_cast<unsigned int *>(address + 64 - misalign) : buffer;
}

void setbilinear(int on) { g_linear = on; }

int PortBilinear() { return g_linear; }

void setAlphaFlag(sceVif1Packet * /*packet*/, sceGsAlpha *alpha) { draw2d::Get().set_alpha(alpha); }

void set2DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, int u, int v) {
    RectSprite(texture, screen, CRect_i_(u, v, screen.width, screen.height),
               {.r = 0x80, .g = 0x80, .b = 0x80, .a = 0x80});
}

void set2DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel) {
    RectSprite(texture, screen, texel, {.r = 0x80, .g = 0x80, .b = 0x80, .a = 0x80});
}

void set2DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                 unsigned char alpha) {
    RectSprite(texture, screen, texel, {.r = 0x80, .g = 0x80, .b = 0x80, .a = alpha});
}

void set2DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                 unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    RectSprite(texture, screen, texel, {.r = red, .g = green, .b = blue, .a = alpha});
}

// A gouraud strip, so the corner order decides the diagonal the colours interpolate across.
void set2DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                 spRGBA *top_left, spRGBA *top_right, spRGBA *bottom_left, spRGBA *bottom_right, int mode) {
    if (texture == nullptr) {
        return;
    }

    std::array<gfx::Vertex2D, 4> quad = RectQuad(screen, texel, {});
    const std::array<Colour, 4>  colours = {ToColour(top_left), ToColour(top_right), ToColour(bottom_right),
                                            ToColour(bottom_left)};
    for (std::size_t corner = 0; corner < quad.size(); corner++) {
        quad[corner].color[0] = colours[corner].r;
        quad[corner].color[1] = colours[corner].g;
        quad[corner].color[2] = colours[corner].b;
        quad[corner].color[3] = colours[corner].a;
    }

    std::array<gfx::Vertex2D, 4> strip;
    if (mode != 0) {
        strip = {quad[0], quad[1], quad[3], quad[2]};
    } else {
        strip = {quad[1], quad[0], quad[2], quad[3]};
    }
    draw2d::DrawTextured(gfx::Primitive::TriangleStrip, strip, texture->tex0, SpriteTex1(),
                         draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
}

void set3DColSprite(sceVif1Packet * /*packet*/, int *top_left, int *top_right, int *bottom_left, int *bottom_right,
                    spRGBA *top_left_colour, spRGBA *top_right_colour, spRGBA *bottom_left_colour,
                    spRGBA *bottom_right_colour) {
    std::array<gfx::Vertex2D, 4> strip = {
        GsVertex(top_left, 0.0f, 0.0f, ToColour(top_left_colour), false),
        GsVertex(top_right, 0.0f, 0.0f, ToColour(top_right_colour), false),
        GsVertex(bottom_left, 0.0f, 0.0f, ToColour(bottom_left_colour), false),
        GsVertex(bottom_right, 0.0f, 0.0f, ToColour(bottom_right_colour), false),
    };
    draw2d::DrawUntextured(gfx::Primitive::TriangleStrip, strip, CurrentState(false));
}

void set3DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &source, int *top_left,
                 int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha) {
    spRGBA colour = {.r = 0x80, .g = 0x80, .b = 0x80, .a = alpha};
    set3DSprite(packet, texture, source, top_left, top_right, bottom_left, bottom_right, &colour);
}

void set3DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &source, int *top_left,
                 int *top_right, int *bottom_left, int *bottom_right, spRGBA *colour) {
    if (texture == nullptr) {
        return;
    }

    Sprite3D(texture, source, top_left, top_right, bottom_left, bottom_right, ToColour(colour), false);
}

void set3DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &source, int *top_left,
                 int *bottom_right, spRGBA *colour) {
    if (texture == nullptr) {
        return;
    }

    Sprite3D(texture, source, top_left, bottom_right, ToColour(colour), false);
}

void set3DSpriteFog(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &source, int *top_left,
                    int *top_right, int *bottom_left, int *bottom_right, unsigned char alpha) {
    if (texture == nullptr) {
        return;
    }

    Sprite3D(texture, source, top_left, top_right, bottom_left, bottom_right,
             {.r = 0x80, .g = 0x80, .b = 0x80, .a = alpha}, true);
}

void set3DSpriteFog(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &source, int *top_left,
                    int *bottom_right, spRGBA *colour) {
    if (texture == nullptr) {
        return;
    }

    Sprite3D(texture, source, top_left, bottom_right, ToColour(colour), true);
}

void setColSprite(sceVif1Packet * /*packet*/, int *top_left, int *top_right, int *bottom_left, int *bottom_right,
                  unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha) {
    const Colour                 colour = {.r = red, .g = green, .b = blue, .a = alpha};
    std::array<gfx::Vertex2D, 4> strip = {
        GsVertex(top_left, 0.0f, 0.0f, colour, false, true),
        GsVertex(top_right, 0.0f, 0.0f, colour, false, true),
        GsVertex(bottom_left, 0.0f, 0.0f, colour, false, true),
        GsVertex(bottom_right, 0.0f, 0.0f, colour, false, true),
    };
    draw2d::DrawUntextured(gfx::Primitive::TriangleStrip, strip, CurrentState(false));
}

void set2DSpriteC4(sceVif1Packet * /*packet*/, const CRect_i_ &screen, spRGBA *top_left, spRGBA *top_right,
                   spRGBA *bottom_left, spRGBA *bottom_right) {
    const auto x0 = static_cast<float>(screen.x);
    const auto y0 = static_cast<float>(screen.y);
    const auto x1 = static_cast<float>(screen.x + screen.width);
    const auto y1 = static_cast<float>(screen.y + screen.height);

    std::array<gfx::Vertex2D, 4> strip = {
        draw2d::Vertex(x0, y0, 0.0f, 0.0f, 0.0f, top_left->r, top_left->g, top_left->b, top_left->a),
        draw2d::Vertex(x1, y0, 0.0f, 0.0f, 0.0f, top_right->r, top_right->g, top_right->b, top_right->a),
        draw2d::Vertex(x0, y1, 0.0f, 0.0f, 0.0f, bottom_left->r, bottom_left->g, bottom_left->b,
                       bottom_left->a),
        draw2d::Vertex(x1, y1, 0.0f, 0.0f, 0.0f, bottom_right->r, bottom_right->g, bottom_right->b,
                       bottom_right->a),
    };
    draw2d::DrawUntextured(gfx::Primitive::TriangleStrip, strip, draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
}

// Retail's rotation: offsets in whole pixels (the far edges one past the rect), turned with the
// axes swapped at angle 0. Retail truncates each turned corner to a pixel, so a slow turn (the
// title's backdrop, half a milliradian a tick) moves the corners one at a time; they are kept as
// turned here.
void set2DSprite(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                 int pivot_x, int pivot_y, float angle) {
    draw2d::TurnedSprite(texture, screen, texel, pivot_x, pivot_y, angle, 1.0f);
}

void draw2d::TurnedSprite(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x,
                          int pivot_y, float angle, float scale) {
    if (texture == nullptr) {
        return;
    }

    std::array<float, 4> x{};
    std::array<float, 4> y{};

    x[0] = x[2] = static_cast<float>(-pivot_x);
    x[1] = x[3] = static_cast<float>((screen.x + screen.width + 1) - (screen.x + pivot_x));
    y[0] = y[1] = static_cast<float>(-pivot_y);
    y[2] = y[3] = static_cast<float>((screen.y + screen.height + 1) - (screen.y + pivot_y));

    for (int i = 0; i < 4; i++) {
        const float turned_x = y[i] * cosf(angle) + x[i] * sinf(angle);
        const float turned_y = x[i] * cosf(angle) - y[i] * sinf(angle);

        x[i] = turned_x * scale + static_cast<float>(screen.x);
        y[i] = turned_y * scale + static_cast<float>(screen.y);
    }

    const auto u0 = static_cast<float>(texel.x);
    const auto v0 = static_cast<float>(texel.y);
    const auto v1 = static_cast<float>(texel.y + texel.height);

    // The second corner's u spans the screen width, not the texel width, as in retail.
    std::array<gfx::Vertex2D, 4> strip = {
        draw2d::Vertex(x[0], y[0], 0.0f, u0, v0, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(x[1], y[1], 0.0f, static_cast<float>(texel.x + screen.width), v0, 0x80, 0x80, 0x80,
                       0x80),
        draw2d::Vertex(x[2], y[2], 0.0f, u0, v1, 0x80, 0x80, 0x80, 0x80),
        draw2d::Vertex(x[3], y[3], 0.0f, static_cast<float>(texel.x + texel.width), v1, 0x80, 0x80, 0x80,
                       0x80),
    };
    draw2d::DrawTextured(gfx::Primitive::TriangleStrip, strip, texture->tex0, SpriteTex1(),
                         draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
}

// Retail's rotation in 12.4 units: the far edges a sixteenth short, mirrored about the pivot at
// angle 0, y halved for the field after turning.
void set2DSpriteRot(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                    int pivot_x, int pivot_y, float angle, unsigned char red, unsigned char green,
                    unsigned char blue, unsigned char alpha) {
    if (texture == nullptr) {
        return;
    }

    std::array<float, 4> x{};
    std::array<float, 4> y{};

    x[0] = x[2] = static_cast<float>(pivot_x * -16);
    x[1] = x[3] = static_cast<float>(((screen.width - pivot_x) << 4) - 1);
    y[0] = y[1] = static_cast<float>(pivot_y * -16);
    y[2] = y[3] = static_cast<float>(((screen.height - pivot_y) << 4) - 1);

    for (int i = 0; i < 4; i++) {
        const float turned_x = -y[i] * sinf(angle) - x[i] * cosf(angle);
        const float turned_y = -x[i] * sinf(angle) + y[i] * cosf(angle);

        x[i] = static_cast<float>(screen.x) + static_cast<float>(static_cast<int>(turned_x)) / 16.0f;
        y[i] = static_cast<float>(screen.y) + static_cast<float>(static_cast<int>(0.5f * turned_y)) / 8.0f;
    }

    const auto u0 = static_cast<float>(texel.x);
    const auto v0 = static_cast<float>(texel.y);
    const auto v1 = static_cast<float>(texel.y + texel.height);

    std::array<gfx::Vertex2D, 4> strip = {
        draw2d::Vertex(x[0], y[0], 0.0f, u0, v0, red, green, blue, alpha),
        draw2d::Vertex(x[1], y[1], 0.0f, static_cast<float>(texel.x + screen.width), v0, red, green, blue,
                       alpha),
        draw2d::Vertex(x[2], y[2], 0.0f, u0, v1, red, green, blue, alpha),
        draw2d::Vertex(x[3], y[3], 0.0f, static_cast<float>(texel.x + texel.width), v1, red, green, blue,
                       alpha),
    };
    draw2d::DrawTextured(gfx::Primitive::TriangleStrip, strip, texture->tex0, SpriteTex1(),
                         draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
}

void set2DSprite(sceVif1Packet *packet, CTexture *texture, RECT *screen, RECT *texel, unsigned char alpha) {
    set2DSprite(packet, texture, CRect_i_(screen->x, screen->y, screen->width, screen->height),
                CRect_i_(texel->x, texel->y, texel->width, texel->height), alpha);
}
