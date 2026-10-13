#include "dispctrl.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#include "debugfont.hpp"
#include "draw2d_port.hpp"
#include "mglib.hpp"
#include "texture.hpp"

PORT_OVERRIDE void openGiftag(sceVif1Packet *packet) {}

PORT_OVERRIDE void closeGiftag(sceVif1Packet *packet) {}

namespace {

constexpr int kCellWidth = 8;
constexpr int kCellHeight = 16;

// The boot/loading screen the port starts on in debug builds is MenuInit's developer menu, which
// draws into "frame_buff". It gets a dark panel, a light border and a bar behind the selected row
// so it reads as a menu instead of blue debug text on a blue field. Every other CDebugFont user
// (the town editor's "font_buff", a dungeon's "dbgwork", the item preview) keeps retail's look.
bool IsDeveloperMenu(const char *texture_name) {
    return texture_name != nullptr && std::strcmp(texture_name, "frame_buff") == 0;
}

} // namespace

// Retail clears width x height of the named texture to (1,1,1), blits each 8x16 glyph of the
// 24-bit ankfnt24 into it and draws the texture with alpha 0x40. Sampled through TEXA (AEM, TA0 =
// alpha), the near-black backdrop keeps TA0 while the font's black texels vanish, so the glyph
// cells are holes in the backdrop wherever the glyph is black. Here the backdrop is drawn around
// the cells and each cell straight from ankfnt24 with the same TEXA, through the same mapping of
// width x height texels onto (width - 1) x (height - 1) pixels. Glyphs sample nearest: retail
// filtered the composed texture by setbilinear's flag, which only blurred cell edges.
PORT_OVERRIDE void CDebugFont::Draw() {
    const draw2d::Services &services = draw2d::Get();
    CTexture               *font = services.find_texture("ankfnt24");
    CTexture               *target = services.find_texture(this->texture_name);

    mgTexa.AEM = 1;
    mgTexa.TA0 = static_cast<u_char>(this->alpha);
    services.set_texa(nullptr);

    const int width = this->width;
    const int height = this->height;
    this->length = 0;

    gfx::DrawState  state = draw2d::SpriteState();
    draw2d::Texture glyphs;
    if (font == nullptr || target == nullptr || width <= 0 || height <= 0 ||
        !draw2d::Resolve(font->tex0, 0x41, state, glyphs)) {
        draw2d::RestoreTestZbuf();
        return;
    }
    // The texture retail samples is the target, so the font's own TCC has no say.
    state.texa_aem = true;
    state.texa_ta0 = static_cast<u_char>(this->alpha);

    const float scale_x = static_cast<float>(width - 1) / static_cast<float>(width);
    const float scale_y = static_cast<float>(height - 1) / static_cast<float>(height);
    const float rows = draw2d::RowScale();
    auto        screen_x = [&](int texel_x) { return static_cast<float>(this->x) + texel_x * scale_x; };
    auto        screen_y = [&](int texel_y) { return (static_cast<float>(this->y) + texel_y * scale_y) * rows; };

    const bool menu = IsDeveloperMenu(this->texture_name);
    // The panel, its border, the divider under the version line and the bar behind the cursor row,
    // all as untextured quads drawn before the glyphs.
    std::vector<gfx::Vertex2D> skin;
    auto skin_rect = [&](int left, int top, int right, int bottom, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                         std::uint8_t a) {
        const float x0 = screen_x(left);
        const float x1 = screen_x(right);
        const float y0 = screen_y(top);
        const float y1 = screen_y(bottom);
        skin.push_back(draw2d::Vertex(x0, y0, 0.0f, 0.0f, 0.0f, r, g, b, a));
        skin.push_back(draw2d::Vertex(x1, y0, 0.0f, 0.0f, 0.0f, r, g, b, a));
        skin.push_back(draw2d::Vertex(x1, y1, 0.0f, 0.0f, 0.0f, r, g, b, a));
        skin.push_back(draw2d::Vertex(x0, y1, 0.0f, 0.0f, 0.0f, r, g, b, a));
    };
    if (menu) {
        skin_rect(0, 0, width, height, 0x10, 0x12, 0x1c, 0xd0);
        int line = 0;
        for (const char *text = this->text;; text++) {
            if ((text == this->text || text[-1] == '\n') && *text == '>') {
                skin_rect(0, line * kCellHeight, width, (line + 1) * kCellHeight, 0xe8, 0xb0, 0x4a, 0x28);
            }
            if (*text == '\0') {
                break;
            }
            if (*text == '\n') {
                line++;
            }
        }
        skin_rect(0, 0, width, 1, 0x54, 0x5e, 0x78, 0xff);
        skin_rect(0, height - 1, width, height, 0x54, 0x5e, 0x78, 0xff);
        skin_rect(0, 0, 1, height, 0x54, 0x5e, 0x78, 0xff);
        skin_rect(width - 1, 0, width, height, 0x54, 0x5e, 0x78, 0xff);
        skin_rect(1, kCellHeight, width - 1, kCellHeight + 1, 0x54, 0x5e, 0x78, 0x80);
    }

    const int         columns = (width + kCellWidth - 1) / kCellWidth;
    const int         lines = (height + kCellHeight - 1) / kCellHeight;
    std::vector<bool> covered(static_cast<size_t>(columns) * lines, false);

    std::vector<gfx::Vertex2D> glyph_quads;
    int                        pen_x = 0;
    int                        pen_y = 0;
    for (const char *text = this->text; *text != 0; text++) {
        if (*text == 0x20) {
            pen_x += kCellWidth;
            continue;
        }

        if (*text == 0x0A) {
            pen_y += kCellHeight;
            pen_x = 0;
            continue;
        }

        const int x0 = pen_x;
        const int y0 = pen_y;
        pen_x += kCellWidth;
        if (x0 >= width || y0 >= height) {
            continue;
        }

        covered[static_cast<size_t>(y0 / kCellHeight) * columns + x0 / kCellWidth] = true;

        const int   index = static_cast<u_char>(*reinterpret_cast<const u_char *>(text) - 0x21);
        const int   cell_width = std::min(kCellWidth, width - x0);
        const int   cell_height = std::min(kCellHeight, height - y0);
        const float u0 = static_cast<float>((index % 16) * kCellWidth);
        const float v0 = static_cast<float>((index >> 4) * kCellHeight) * glyphs.v_scale;
        const float u1 = u0 + cell_width;
        const float v1 = v0 + cell_height * glyphs.v_scale;
        const float left = screen_x(x0);
        const float right = screen_x(x0 + cell_width);
        const float top = screen_y(y0);
        const float bottom = screen_y(y0 + cell_height);
        const std::uint8_t grey = menu ? 0xff : 0x80;
        const std::uint8_t glyph_alpha = menu ? 0x80 : 0x40;
        glyph_quads.push_back(draw2d::Vertex(left, top, 0.0f, u0, v0, grey, grey, grey, glyph_alpha));
        glyph_quads.push_back(draw2d::Vertex(right, top, 0.0f, u1, v0, grey, grey, grey, glyph_alpha));
        glyph_quads.push_back(draw2d::Vertex(right, bottom, 0.0f, u1, v1, grey, grey, grey, glyph_alpha));
        glyph_quads.push_back(draw2d::Vertex(left, bottom, 0.0f, u0, v1, grey, grey, grey, glyph_alpha));
    }

    // MODULATE of the backdrop's TA0 by the sprite's 0x40. The menu's dark panel replaces it.
    const u_char               alpha = static_cast<u_char>((state.texa_ta0 * 0x40) >> 7);
    std::vector<gfx::Vertex2D> backdrop;
    for (int line = 0; !menu && line < lines; line++) {
        const float top = screen_y(line * kCellHeight);
        const float bottom = screen_y(std::min((line + 1) * kCellHeight, height));
        for (int column = 0; column < columns;) {
            if (covered[static_cast<size_t>(line) * columns + column]) {
                column++;
                continue;
            }

            int run = column;
            while (run < columns && !covered[static_cast<size_t>(line) * columns + run]) {
                run++;
            }

            const float left = screen_x(column * kCellWidth);
            const float right = screen_x(std::min(run * kCellWidth, width));
            backdrop.push_back(draw2d::Vertex(left, top, 0.0f, 0.0f, 0.0f, 1, 1, 1, alpha));
            backdrop.push_back(draw2d::Vertex(right, top, 0.0f, 0.0f, 0.0f, 1, 1, 1, alpha));
            backdrop.push_back(draw2d::Vertex(right, bottom, 0.0f, 0.0f, 0.0f, 1, 1, 1, alpha));
            backdrop.push_back(draw2d::Vertex(left, bottom, 0.0f, 0.0f, 0.0f, 1, 1, 1, alpha));
            column = run;
        }
    }

    if (!skin.empty()) {
        gfx::Draw2D(gfx::Primitive::Quads, skin, {}, state);
    }
    if (!backdrop.empty()) {
        gfx::Draw2D(gfx::Primitive::Quads, backdrop, {}, state);
    }
    if (!glyph_quads.empty()) {
        gfx::Draw2D(gfx::Primitive::Quads, glyph_quads, glyphs.binding, state);
    }
    draw2d::RestoreTestZbuf();
}
