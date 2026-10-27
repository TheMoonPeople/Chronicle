#include "text/glyph_draw.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

#include "platform/config.hpp"
#include "render/draw2d.hpp"
#include "text/text_shadow.hpp"
#include "text/textwipe.hpp"

namespace dc::text {

using namespace dc::game;
using namespace dc::platform;
using namespace dc::render;

namespace {

/**
 * A button symbol painted into a game texture:
 * the texture's name and the art's box in its texels.
 */
struct Baked {
    std::string_view texture;
    int              x, y, w, h;
    glyphs::Button   button;
};

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

int Overlap(const Baked &baked, const CRect_i_ &texel) {
    const int left = std::max(baked.x, texel.x);
    const int top = std::max(baked.y, texel.y);
    const int right = std::min(baked.x + baked.w, texel.x + texel.width);
    const int bottom = std::min(baked.y + baked.h, texel.y + texel.height);
    return right > left && bottom > top ? (right - left) * (bottom - top) : 0;
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
    const bool over_art = grow > 0.0f;
    float      fill = over_art ? 1.0f : 0.84f;
    float      reach = image.key ? 1.7f : 0.96f;
    if (image.key) {
        fill = over_art ? 1.3f : 0.92f;
    }
    if (over_art) {
        reach = 2.2f;
    }
    float       scale = slot_h * fill / (image.key ? image.height : glyphs::kReference);
    float       width = image.width * scale;
    float       height = image.height * scale;
    const float max_w = slot_w * reach;
    if (width > max_w) {
        height *= max_w / width;
        width = max_w;
    }
    float      x = static_cast<float>(slot.x) - grow + (slot_w - width) * 0.5f;
    float      y = static_cast<float>(slot.y) - grow + (slot_h - height) * 0.5f;
    const auto alpha_byte = static_cast<u_char>(alpha);
    // In text the symbol casts a shadow in its own shape, like the words beside it; config glyph_shadow
    // sets its strength. Painted over art it has none (the art has its own).
    std::vector<gfx::Vertex2D> quad;
    auto                       add = [&](float ox, float oy, u_char r, u_char g, u_char b, u_char a) {
        quad.push_back(draw2d::Vertex(x + ox, y + oy, 0.0f, image.u0, image.v0, r, g, b, a));
        quad.push_back(draw2d::Vertex(x + ox + width, y + oy, 0.0f, image.u1, image.v0, r, g, b, a));
        quad.push_back(draw2d::Vertex(x + ox + width, y + oy + height, 0.0f, image.u1, image.v1, r, g, b, a));
        quad.push_back(draw2d::Vertex(x + ox, y + oy + height, 0.0f, image.u0, image.v1, r, g, b, a));
    };
    if (grow <= 0.0f) {
        const float rows = draw2d::RowScale();
        TextShadowEach(ConfigGet().glyph_shadow, [&](const TextShadowTap &tap) {
            add(tap.dx, tap.dy * rows, static_cast<u_char>(tap.grey), static_cast<u_char>(tap.grey),
                static_cast<u_char>(tap.grey), static_cast<u_char>(std::min<int>(alpha, tap.alpha)));
        });
    }
    add(0.0f, 0.0f, 0x80, 0x80, 0x80, alpha_byte);
    textwipe::Draw2D(gfx::Primitive::Quads, quad, image.binding, draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
    return true;
}

int GlyphBakedCount(const char *texture) {
    const std::string_view name = texture;
    int                    count = 0;
    for (const Baked &baked : kBaked) {
        count += baked.texture == name ? 1 : 0;
    }
    return count;
}

bool GlyphReplaceSprite(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || texel.width <= 0 || texel.height <= 0) {
        return false;
    }
    const std::string_view name = texture;
    for (const Baked &baked : kBaked) {
        if (baked.texture != name) {
            continue;
        }
        const int overlap = Overlap(baked, texel);
        // the drawn rectangle is the art: most of it is inside the box, and the box is mostly inside it
        if (overlap * 10 >= texel.width * texel.height * 6 && overlap * 10 >= baked.w * baked.h * 6) {
            return GlyphDrawSlot(baked.button, screen, alpha);
        }
    }
    return false;
}

void GlyphOverlaySprite(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || texel.width <= 0 || texel.height <= 0) {
        return;
    }
    const float            scale_x = static_cast<float>(screen.width) / static_cast<float>(texel.width);
    const float            scale_y = static_cast<float>(screen.height) / static_cast<float>(texel.height);
    const std::string_view name = texture;
    for (const Baked &baked : kBaked) {
        if (baked.texture != name) {
            continue;
        }
        // only a box wholly inside a larger rectangle: whole-rectangle matches were replaced
        if (baked.x < texel.x || baked.y < texel.y || baked.x + baked.w > texel.x + texel.width ||
            baked.y + baked.h > texel.y + texel.height || baked.w * baked.h * 2 > texel.width * texel.height) {
            continue;
        }
        CRect_i_ slot(screen.x + static_cast<int>(static_cast<float>(baked.x - texel.x) * scale_x),
                      screen.y + static_cast<int>(static_cast<float>(baked.y - texel.y) * scale_y),
                      static_cast<int>(static_cast<float>(baked.w) * scale_x), static_cast<int>(static_cast<float>(baked.h) * scale_y));
        GlyphDrawSlot(baked.button, slot, alpha, 1.0f);
    }
}

} // namespace dc::text
