#include "glyph_draw.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>

#include "draw2d_port.hpp"
#include "platform/config.hpp"
#include "text_shadow.hpp"
#include "textwipe.hpp"

namespace {

struct Baked {
    const char    *texture;
    int            x, y, w, h; // the art's box in the texture
    glyphs::Button button;
};

// Boxes found by dumping the textures (DC_TEXTURE_DUMP) and tracing the draws (DC_SPRITE_TRACE).
constexpr Baked kBaked[] = {
    // name entry: the L1/R1 hints beside the name (the strip also holds L2 and R2)
    {"nametemp",  0,   472, 24, 24, glyphs::Button::L2    },
    {"nametemp",  24,  472, 24, 24, glyphs::Button::L1    },
    {"nametemp",  49,  472, 24, 24, glyphs::Button::R1    },
    {"nametemp",  73,  472, 24, 24, glyphs::Button::R2    },
    // the character board's tab hints
    {"perbrd",    72,  20,  24, 20, glyphs::Button::L2    },
    {"perbrd",    98,  20,  24, 20, glyphs::Button::L1    },
    {"perbrd",    124, 20,  24, 20, glyphs::Button::R1    },
    {"perbrd",    150, 20,  26, 20, glyphs::Button::R2    },
    // the cutscene skip prompt: PAL (YES cross, NO circle) and the other layout
    {"pause_e",   46,  35,  16, 16, glyphs::Button::Cross },
    {"pause_e",   80,  52,  16, 16, glyphs::Button::Circle},
    {"pause_g",   46,  35,  16, 16, glyphs::Button::Cross },
    {"pause_g",   80,  52,  16, 16, glyphs::Button::Circle},
    {"pause_s",   46,  35,  16, 16, glyphs::Button::Cross },
    {"pause_s",   80,  52,  16, 16, glyphs::Button::Circle},
    {"pause_f",   38,  38,  16, 16, glyphs::Button::Cross },
    {"pause_f",   89,  49,  16, 16, glyphs::Button::Circle},
    {"pause_i",   38,  38,  16, 16, glyphs::Button::Cross },
    {"pause_i",   89,  48,  16, 16, glyphs::Button::Circle},
    {"skip_bord", 62,  77,  16, 16, glyphs::Button::Circle},
    {"skip_bord", 86,  94,  16, 16, glyphs::Button::Cross },
};

int Overlap(const Baked &b, const CRect_i_ &texel) {
    int x0 = std::max(b.x, texel.x);
    int y0 = std::max(b.y, texel.y);
    int x1 = std::min(b.x + b.w, texel.x + texel.width);
    int y1 = std::min(b.y + b.h, texel.y + texel.height);
    return x1 > x0 && y1 > y0 ? (x1 - x0) * (y1 - y0) : 0;
}

} // namespace

bool GlyphDrawSlot(glyphs::Button button, const CRect_i_ &slot, int alpha, float grow) {
    glyphs::Image image;
    if (!glyphs::Find(button, image)) {
        return false;
    }
    float slot_w = static_cast<float>(slot.width) + 2.0f * grow;
    float slot_h = static_cast<float>(slot.height) + 2.0f * grow;
    // In text the symbol sits inside its slot with a margin, so it never touches the letters beside it;
    // painted over art (grow > 0) it must cover the art instead.
    // Round buttons are sized by their diameter (the atlas keeps their proportions to the shoulder buttons
    // and the d-pad); keycaps and the mouse by their height, so a short wide key (Tab, Space) is as tall
    // as a letter key. A key may reach into the spaces beside it, not further.
    float       scale = slot_h * (image.key ? (grow > 0.0f ? 1.3f : 0.92f) : (grow > 0.0f ? 1.0f : 0.84f)) /
                        (image.key ? image.height : glyphs::kReference);
    float       width = image.width * scale;
    float       height = image.height * scale;
    const float max_w = slot_w * (grow > 0.0f ? 2.2f : (image.key ? 1.7f : 0.96f));
    if (width > max_w) {
        height *= max_w / width;
        width = max_w;
    }
    float x = static_cast<float>(slot.x) - grow + (slot_w - width) * 0.5f;
    float y = static_cast<float>(slot.y) - grow + (slot_h - height) * 0.5f;
    auto  a = static_cast<u_char>(alpha);
    // In text the symbol casts a shadow (text_shadow.hpp) in its own shape, so it lifts off the
    // picture like the words beside it; config glyph_shadow sets how strong, apart from the letters'. Painted over art it has none (the art has its own).
    std::vector<gfx::Vertex2D> quad;
    auto                       add = [&](float ox, float oy, u_char r, u_char g, u_char b, u_char alpha_byte) {
        quad.push_back(draw2d::Vertex(x + ox, y + oy, 0.0f, image.u0, image.v0, r, g, b, alpha_byte));
        quad.push_back(draw2d::Vertex(x + ox + width, y + oy, 0.0f, image.u1, image.v0, r, g, b, alpha_byte));
        quad.push_back(draw2d::Vertex(x + ox + width, y + oy + height, 0.0f, image.u1, image.v1, r, g, b, alpha_byte));
        quad.push_back(draw2d::Vertex(x + ox, y + oy + height, 0.0f, image.u0, image.v1, r, g, b, alpha_byte));
    };
    if (grow <= 0.0f) {
        const float rows = draw2d::RowScale();
        TextShadowEach(ConfigGet().glyph_shadow, [&](const TextShadowTap &tap) {
            add(tap.dx, tap.dy * rows, static_cast<u_char>(tap.grey), static_cast<u_char>(tap.grey),
                static_cast<u_char>(tap.grey), static_cast<u_char>(std::min<int>(alpha, tap.alpha)));
        });
    }
    add(0.0f, 0.0f, 0x80, 0x80, 0x80, a);
    textwipe::Draw2D(gfx::Primitive::Quads, quad, image.binding, draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
    return true;
}

int GlyphBakedCount(const char *texture) {
    int n = 0;
    for (const Baked &b : kBaked) {
        n += std::strcmp(b.texture, texture) == 0;
    }
    return n;
}

bool GlyphReplaceSprite(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || texel.width <= 0 || texel.height <= 0) {
        return false;
    }
    for (const Baked &b : kBaked) {
        if (std::strcmp(b.texture, texture) != 0) {
            continue;
        }
        int overlap = Overlap(b, texel);
        // the drawn rectangle is the art: most of it is inside the box, and the box is mostly inside it
        if (overlap * 10 >= texel.width * texel.height * 6 && overlap * 10 >= b.w * b.h * 6) {
            return GlyphDrawSlot(b.button, screen, alpha);
        }
    }
    return false;
}

void GlyphOverlaySprite(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || texel.width <= 0 || texel.height <= 0) {
        return;
    }
    const float sx = static_cast<float>(screen.width) / static_cast<float>(texel.width);
    const float sy = static_cast<float>(screen.height) / static_cast<float>(texel.height);
    for (const Baked &b : kBaked) {
        if (std::strcmp(b.texture, texture) != 0) {
            continue;
        }
        // only a box wholly inside a larger rectangle: whole-rectangle matches were replaced
        if (b.x < texel.x || b.y < texel.y || b.x + b.w > texel.x + texel.width ||
            b.y + b.h > texel.y + texel.height || b.w * b.h * 2 > texel.width * texel.height) {
            continue;
        }
        CRect_i_ slot(screen.x + static_cast<int>(static_cast<float>(b.x - texel.x) * sx),
                      screen.y + static_cast<int>(static_cast<float>(b.y - texel.y) * sy),
                      static_cast<int>(static_cast<float>(b.w) * sx), static_cast<int>(static_cast<float>(b.h) * sy));
        GlyphDrawSlot(b.button, slot, alpha, 1.0f);
    }
}
