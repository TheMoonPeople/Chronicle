#include "clsmes.hpp"
#include "mathconst.h"

#include <array>
#include <cmath>
#include <iterator>
#include <vector>

#include "draw2d_port.hpp"
#include "gametext.hpp"
#include "gameutil.hpp"
#include "localize.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "platform/ttffont.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

extern float RandTbl[64];
extern float RandTbl2[64];
extern MES_FUCHI FuchiTbl_E[];

namespace {

// ---- Message text from a TrueType font ----
// With font.ttf in a lang folder (the save's or the executable's), a character the font has is drawn
// from it at the screen's own resolution, in the cell the game laid the bitmap character out in,
// instead of from the 14x20 bitmaps of gaiji.img. Icons and characters the font lacks draw as before.

bool TtfReady() {
    static bool tried = false;
    if (!tried) {
        tried = true;
        std::filesystem::path file = LocalizeFindFile("font.ttf");
        if (!file.empty() && ttffont::Load(file)) {
            std::fprintf(stderr, "font: message text from %s\n", file.string().c_str());
        }
    }
    return ttffont::Loaded();
}

// What DrawGaijiFont tints a character with: the colour table's entry, the flashing cursor row's,
// and double for a lit row.
void GlyphTint(int clut, int dark, int wide, int &r, int &g, int &b, int &alpha) {
    alpha = 0x80;
    switch (FontColorTbl[clut]) {
        case FONT_COLOR_WHITE: r = g = b = 0x5F; break;
        case FONT_COLOR_BROWN: r = 0x22; g = 0x20; b = 0x18; break;
        case FONT_COLOR_YELLOW: r = 0x5E; g = 0x5E; b = 0x20; break;
        case FONT_COLOR_CYAN: r = 0x20; g = 0x5E; b = 0x5E; break;
        case FONT_COLOR_GREEN: r = 0x20; g = 0x5E; b = 0x20; break;
        case FONT_COLOR_BROWN_OPAQUE: r = 0x22; g = 0x20; b = 0x18; break;
        case FONT_COLOR_GOLD: r = 0x73; g = 0x67; b = 0x33; break;
        case FONT_COLOR_GREY: r = g = b = 0x47; break;
        case FONT_COLOR_MAGENTA: r = 0x5F; g = 0x1F; b = 0x5F; break;
        default: r = 0x28; g = 0x28; b = 0x20; break;
    }
    if (clut == 0xFF) {
        if (dark != 0) {
            r = 0x7F; g = 0x22; b = 0x22;
        } else {
            r = 0x40; g = 0x11; b = 0x11;
        }
    }
    if (wide == 1) {
        r *= 2;
        g *= 2;
        b *= 2;
    }
}

void TtfQuad(std::vector<gfx::Vertex2D> &out, const ttffont::Glyph &glyph, float x, float y, float width,
             float height, int r, int g, int b, int alpha) {
    auto clamp = [](int v) { return static_cast<u_char>(v < 0 ? 0 : v > 255 ? 255 : v); };
    const u_char red = clamp(r), green = clamp(g), blue = clamp(b), a = clamp(alpha);
    out.push_back(draw2d::Vertex(x, y, 0.0f, glyph.u0, glyph.v0, red, green, blue, a));
    out.push_back(draw2d::Vertex(x + width, y, 0.0f, glyph.u1, glyph.v0, red, green, blue, a));
    out.push_back(draw2d::Vertex(x + width, y + height, 0.0f, glyph.u1, glyph.v1, red, green, blue, a));
    out.push_back(draw2d::Vertex(x, y + height, 0.0f, glyph.u0, glyph.v1, red, green, blue, a));
}

// The em the font is drawn at: its line fits 82% of the cell's height and its widest glyph the cell's
// width, so a glyph never leaves the cell the game sized the window for.
float TtfEm(const ClsMes &mes, const ttffont::Metrics &metrics) {
    return std::min(mes.char_height * 0.82f, mes.char_width * 0.95f / metrics.advance);
}

// Where each laid-out character of a window starts, in the window's own coordinates. A word (a run of
// characters the font draws) keeps the place the game gave it: its letters go one after another by
// their own widths and the word is centred on the cells the game gave it, so rows, indents, centring and
// the icons between words stay where the game put them. A glyph is never given more than its cell, so
// a word is never wider than the game's.
void TtfLayoutRows(ClsMes &mes, std::vector<float> &xs) {
    const int count = mes.win_line_num;
    xs.assign(static_cast<size_t>(count), 0.0f);
    for (int i = 0; i < count; i++) {
        xs[static_cast<size_t>(i)] = static_cast<float>(mes.win_line[i].x);
    }
    if (mes.char_height <= 0 || mes.char_width <= 0) {
        return;
    }
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const ttffont::Metrics    metrics = ttffont::GetMetrics();
    const float               em = TtfEm(mes, metrics);
    const int                 em_px = static_cast<int>(em * mapping.scale_x + 0.5f);
    const float               gap = em * 0.1f;

    auto glyph_of = [&](int i, ttffont::Glyph &glyph) {
        const int code = mes.win_line[i].code;
        const char32_t ch = code >= -0x2DF && code < -0x251 ? GameTextChar(static_cast<s16>(code)) : 0;
        return ch != 0 && ttffont::GetGlyph(ch, em_px, glyph);
    };
    auto cell_of = [&](int i) {
        const bool next = i + 1 < count && mes.win_line[i + 1].y == mes.win_line[i].y;
        return next ? static_cast<float>(mes.win_line[i + 1].x - mes.win_line[i].x) : static_cast<float>(mes.char_width);
    };

    int i = 0;
    while (i < count) {
        ttffont::Glyph glyph;
        if (!glyph_of(i, glyph)) {
            i++;
            continue;
        }
        int                last = i;
        float              natural = 0.0f;
        std::vector<float> pen;
        for (; last < count && mes.win_line[last].y == mes.win_line[i].y && glyph_of(last, glyph); last++) {
            pen.push_back(natural);
            natural += std::min(glyph.width / mapping.scale_x + gap, cell_of(last));
        }
        const float span = static_cast<float>(mes.win_line[last - 1].x - mes.win_line[i].x) + cell_of(last - 1);
        const float start = static_cast<float>(mes.win_line[i].x) + (span - (natural - gap)) / 2;
        for (int k = i; k < last; k++) {
            xs[static_cast<size_t>(k)] = start + pen[static_cast<size_t>(k - i)];
        }
        i = last;
    }
}

// Queues the font's glyph for win_line[index]: an edge pass per outline and one fill. False where
// the font has none, and the bitmap character is drawn.
bool QueueTtfGlyph(ClsMes &mes, int index, int dark, int offset_x, int offset_y, const std::vector<float> &xs,
                   std::vector<gfx::Vertex2D> &edges, std::vector<gfx::Vertex2D> &fills) {
    const MES_WIN_LINE &line = mes.win_line[index];
    const char32_t      ch = GameTextChar(line.code);
    if (ch == 0 || mes.char_height <= 0 || mes.char_width <= 0 || static_cast<size_t>(index) >= xs.size()) {
        return false;
    }
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const ttffont::Metrics    metrics = ttffont::GetMetrics();
    const float               em = TtfEm(mes, metrics);
    const int                 em_px = static_cast<int>(em * mapping.scale_x + 0.5f);
    ttffont::Glyph            glyph;
    if (!ttffont::GetGlyph(ch, em_px, glyph)) {
        return false;
    }

    // The window's origin for this row, as the bitmap characters take it.
    const int row = line.y / mes.char_height;
    float     origin_x, cell_y;
    if (row >= static_cast<int>(std::size(mes.line_pos)) || mes.line_pos[row].x < 0 || mes.line_pos[row].y < 0) {
        origin_x = static_cast<float>(offset_x + mes.text_x);
        cell_y = static_cast<float>(offset_y + line.y + mes.text_y);
    } else {
        origin_x = static_cast<float>(mes.line_pos[row].x);
        cell_y = static_cast<float>(line.y + mes.line_pos[row].y - row * mes.char_height);
    }

    // The font's line is centred in the cell's height. The bitmap's pixels are screen pixels, so a quad
    // is as many logical units as they come to.
    const float width = glyph.width / mapping.scale_x;
    const float height = glyph.height / mapping.scale_y * rows;
    const float base = cell_y + (mes.char_height - em * (metrics.ascent + metrics.descent)) / 2 + em * metrics.ascent;
    float       x = origin_x + xs[static_cast<size_t>(index)];
    float       y = (base * rows) + glyph.y_offset / mapping.scale_y;

    // On whole pixels, so the edges of the strokes are not smeared across two.
    auto snap = [](float value, float scale, float offset) { return (std::round(value * scale + offset) - offset) / scale; };
    x = snap(x, mapping.scale_x, mapping.offset_x);
    y = snap(y, mapping.scale_y, mapping.offset_y);

    int wide = 0;
    if (mes.cursor_row >= 0) {
        wide = mes.cursor_lit == 1 ? (row == mes.cursor_row ? 1 : 0) : (row == mes.cursor_row ? 0 : 1);
    }
    int r, g, b, alpha;
    GlyphTint(line.clut, dark, wide, r, g, b, alpha);

    // The bitmaps' outlines were soft; a crisp glyph's reads thick at the same offsets, so they are
    // brought in to 60%.
    const int cap = mes.edge_alpha;
    auto      edge = [&](float dx, float dy, int er, int eg, int eb, int ea) {
        TtfQuad(edges, glyph, x + dx * 0.6f, y + dy * 0.6f * rows, width, height, er, eg, eb, std::min(cap, ea));
    };
    switch (mes.style) {
        case MES_EDGE_WHITE: edge(1, 1, 0xFF, 0xFF, 0xFF, 0x40); break;
        case MES_EDGE_BLACK: edge(1, 1, 0, 0, 0, 0x40); break;
        case MES_EDGE_TABLE:
            for (int i = 0; FuchiTbl_E[i].alpha > 0; i++) {
                edge(static_cast<float>(FuchiTbl_E[i].x), static_cast<float>(FuchiTbl_E[i].y), FuchiTbl_E[i].r,
                     FuchiTbl_E[i].g, FuchiTbl_E[i].b, FuchiTbl_E[i].alpha);
            }
            break;
        case MES_EDGE_DOUBLE:
            edge(1, 1, 0x40, 0x40, 0x40, 0x80);
            edge(2, 2, 0, 0, 0, 0x80);
            break;
    }
    TtfQuad(fills, glyph, x, y, width, height, r, g, b, std::min(cap, alpha));
    return true;
}

enum {
    GAIJI_CODE,
    GAIJI_U,
    GAIJI_V,
    GAIJI_WIDTH,
    GAIJI_HEIGHT,
    GAIJI_X_OFF,
    GAIJI_Y_OFF,
    GAIJI_CELLS,
};

struct Shape {
    gfx::Primitive             primitive;
    std::vector<gfx::Vertex2D> vertices;
};

// What MakeFukidashi_sub and DrawMaru put in the packet for MakeFukidashi to draw: every PRIM write
// starts a fan or a triangle list, every XYZF2 is a point in fukidashibase's texel rows.
std::vector<Shape> g_shapes;

void BeginShape(gfx::Primitive primitive) { g_shapes.push_back({primitive, {}}); }

void ShapePoint(int x, int y) {
    if (g_shapes.empty()) {
        BeginShape(gfx::Primitive::TriangleFan);
    }
    g_shapes.back().vertices.push_back(draw2d::Vertex(x, y, 0.0f, 0.0f, 0.0f, 0xBF, 0xBF, 0xBF, 0x80));
}

constexpr const char *kBubbleTarget = "fukidashibase";

gfx::TextureHandle BubbleTarget(CTexture *texture) {
    if (texture != nullptr) {
        PortTextureRef ref = draw2d::Get().texture(texture->tex0, 0);
        if (ref.valid) {
            std::optional<gfx::TextureInfo> info = gfx::GetTextureInfo(ref.binding.texture);
            if (info && info->render_target) {
                return ref.binding.texture;
            }
        }
    }
    return gfx::FindNamedRenderTarget(kBubbleTarget);
}

} // namespace

void GetPos_AbsPosSet(int x, int y, int width, int height, int win_width, int win_height, int align,
                      int *out_x, int *out_y);
void MyMenuHelpWinDraw(int x, int y, int width, int height, int shade, int u, int v, CTexture *texture);

// Named the other way round from set2DSprite: src is where the sprite lands, dst the texels.
PC_OVERRIDE void Myset2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &src, const CRect_i_ &dst, u8 r,
                               u8 g, u8 b, u8 a) {
    if (texture == nullptr) {
        return;
    }

    gfx::DrawState state = draw2d::Get().draw_state();
    state.blend = true;
    state.fog = false;
    state.cull = gfx::CullMode::None;
    MGPortApplyZbuf(state, mgZBuffer);

    const float x0 = static_cast<float>(src.x);
    const float y0 = static_cast<float>(src.y);
    const float x1 = static_cast<float>(src.x + src.width);
    const float y1 = static_cast<float>(src.y + src.height);
    const float u0 = static_cast<float>(dst.x);
    const float v0 = static_cast<float>(dst.y);
    const float u1 = static_cast<float>(dst.x + dst.width);
    const float v1 = static_cast<float>(dst.y + dst.height);

    std::array<gfx::Vertex2D, 4> quad = {
        draw2d::Vertex(x0, y0, 0.0f, u0, v0, r, g, b, a),
        draw2d::Vertex(x1, y0, 0.0f, u1, v0, r, g, b, a),
        draw2d::Vertex(x1, y1, 0.0f, u1, v1, r, g, b, a),
        draw2d::Vertex(x0, y1, 0.0f, u0, v1, r, g, b, a),
    };
    draw2d::DrawTextured(gfx::Primitive::Quads, quad, texture->tex0, 0x61, state);
    draw2d::Get().set_zbuf(nullptr);
}

PC_OVERRIDE void DrawMaru(sceVif1Packet *packet, int x, int y, int width, int height, int lift, int unused, int rough,
                          int prim) {
    float across;
    float down;
    float turn;
    int   px;
    int   py;
    int   step;

    BeginShape(gfx::Primitive::TriangleFan);

    px = x + (width >> 1);
    py = y + (height >> 1) - lift;

    ShapePoint(px, py);

    turn = 0.0f;
    step = 0;

    while (turn < 128.0f) {
        float angle = TWO_PI * turn / 128.0f;

        across = cosf(angle);
        down = sinf(angle);

        if (rough == 1 && step % 2 == 1) {
            across *= 0.9f;
            down *= 0.85f;
        }

        across += 1.0f;
        down += 1.0f;
        across *= width;
        down *= height;
        across /= 2.0f;
        down /= 2.0f;

        px = (int) across;
        py = (int) down;
        px += x;
        py += y - lift;

        ShapePoint(px, py);

        if (rough == 1) {
            turn += RandTbl[step];
        } else {
            turn += 4.0f;
        }

        step++;
    }

    across = 1.0f;
    down = 0.0f;

    if (rough == 1 && step % 2 == 1) {
        across *= 0.9f;
        down *= 0.85f;
    }

    across += 1.0f;
    down += 1.0f;
    across *= width;
    down *= height;
    across /= 2.0f;
    down /= 2.0f;

    px = (int) across;
    py = (int) down;
    px += x;
    py += y - lift;

    ShapePoint(px, py);
}

PC_OVERRIDE void ClsMes::MakeFukidashi_sub(sceVif1Packet *packet, int prim) {
    float grown_width;
    float grown_height;
    int   x;
    int   y;
    int   top;
    int   i;
    int   mirror;

    top = this->win_y;

    if (this->grow_y < top) {
        top = this->grow_y;
    }

    if (this->tail_on != 0) {
        if (this->tail_left_y < top) {
            top = this->tail_left_y;
        }

        if (this->tail_right_y < top) {
            top = this->tail_right_y;
        }

        if (this->tail_tip_y < top) {
            top = this->tail_tip_y;
        }

        if (this->fukidashi_shape == 2) {
            if (this->tail_tip_y - 5 < top) {
                top = this->tail_tip_y - 5;
            }
        }
    }

    grown_width = this->win_width * this->fade;
    grown_height = this->win_height * this->fade;

    float shape[15][2] = {
        {0.5f,  0.5f },
        {0.02f, 0.29f},
        {0.22f, 0.03f},
        {0.51f, 0.06f},
        {0.73f, 0.0f },
        {0.92f, 0.1f },
        {0.98f, 0.36f},
        {0.98f, 0.74f},
        {0.9f,  0.91f},
        {0.7f,  0.99f},
        {0.43f, 0.91f},
        {0.2f,  0.97f},
        {0.03f, 0.8f },
        {0.0f,  0.52f},
        {0.02f, 0.29f},
    };

    if (this->fukidashi_shape == 0) {
        BeginShape(gfx::Primitive::TriangleFan);

        int base_x = (int) LinerInterpolation(this->grow_x, this->win_x, this->fade);
        int base_y = (int) LinerInterpolation(this->grow_y, this->win_y, this->fade);

        mirror = (int) RandTbl[0];

        for (i = 0; i < 15; i++) {
            if (mirror % 2 != 0) {
                x = (int) (grown_width * (1.0f - shape[i][0]));
            } else {
                x = (int) (grown_width * shape[i][0]);
            }

            if (mirror > 1) {
                y = (int) (grown_height * (1.0f - shape[i][1]));
            } else {
                y = (int) (grown_height * shape[i][1]);
            }

            x += base_x;
            y += base_y;
            y -= top;

            ShapePoint(x, y);
        }
    } else if (this->fukidashi_shape == 1 || this->fukidashi_shape == 2) {
        x = (int) LinerInterpolation(this->grow_x, this->win_x, this->fade);
        y = (int) LinerInterpolation(this->grow_y, this->win_y, this->fade);

        int bubble_shape = this->fukidashi_shape;

        if (bubble_shape == 1) {
            DrawMaru(packet, x, y, (int) grown_width, (int) grown_height, top, 0x20, bubble_shape, prim);
        } else {
            int   upper_width = (int) grown_width - this->char_width;
            int   half_height = (int) grown_height >> 1;
            float half_height_float = (float) half_height;

            DrawMaru(packet, x, y, upper_width, (int) (half_height_float + 1.5f * this->char_height), top,
                     0x20, 0, prim);

            float pad = 1.5f * this->char_height;
            int   indent = this->char_width;

            y = (int) (y + half_height - pad);
            DrawMaru(packet, x + indent, y, (int) grown_width - indent, (int) (half_height + pad), top, 0x20,
                     0, prim);
        }
    } else {
        x = (int) LinerInterpolation(this->grow_x, this->win_x, this->fade);
        y = (int) LinerInterpolation(this->grow_y, this->win_y, this->fade);

        BeginShape(gfx::Primitive::TriangleFan);

        int oval_width = (int) grown_width;
        int oval_height = (int) grown_height;
        int px;
        int py;

        px = x + (oval_width >> 1);
        py = y + (oval_height >> 1) - top;

        ShapePoint(px, py);

        float across;
        float down;
        float turn = 0.0f;
        int   step = 0;

        while (turn < 128.0f) {
            float angle = TWO_PI * turn / 128.0f;

            across = cosf(angle);
            down = sinf(angle);

            across = across * RandTbl2[step];
            down = down * RandTbl2[step];
            across += 0.5f;
            down += 0.5f;
            across = across * oval_width;
            down = down * oval_height;

            px = (int) across;
            py = (int) down;

            px += x;
            py += y - top;

            ShapePoint(px, py);

            turn += RandTbl[step];
            step++;
        }

        across = 1.0f;
        down = 0.0f;

        across = across * RandTbl2[0];
        down = down * RandTbl2[0];
        across += 0.5f;
        down += 0.5f;
        across = across * oval_width;
        down = down * oval_height;

        px = (int) across;
        py = (int) down;

        px += x;
        py += y - top;

        ShapePoint(px, py);
    }

    if (this->tail_on != 0) {
        if (this->fukidashi_shape == 2) {
            x = (int) LinerInterpolation(this->grow_x, (this->tail_x + this->tail_tip_x) >> 1, this->fade);
            y = (int) LinerInterpolation(this->grow_y, (this->tail_y + this->tail_tip_y) >> 1, this->fade);
            DrawMaru(packet, x - 10, y - 10, 20, 20, top, 0x20, 0, prim);

            x = (int) LinerInterpolation(this->grow_x, this->tail_tip_x, this->fade);
            y = (int) LinerInterpolation(this->grow_y, this->tail_tip_y, this->fade);
            DrawMaru(packet, x - 5, y - 5, 10, 10, top, 0x20, 0, prim);
        } else {
            BeginShape(gfx::Primitive::Triangles);

            x = (int) LinerInterpolation(this->grow_x, this->tail_tip_x, this->fade);
            y = (int) LinerInterpolation(this->grow_y, this->tail_tip_y, this->fade);
            y -= top;
            ShapePoint(x, y);

            x = (int) LinerInterpolation(this->grow_x, this->tail_left_x, this->fade);
            y = (int) LinerInterpolation(this->grow_y, this->tail_left_y, this->fade);
            y -= top;
            ShapePoint(x, y);

            x = (int) LinerInterpolation(this->grow_x, this->tail_right_x, this->fade);
            y = (int) LinerInterpolation(this->grow_y, this->tail_right_y, this->fade);
            y -= top;
            ShapePoint(x, y);
        }
    }
}

// Retail renders the bubble into fukidashibase in three GS passes: a full-height fill with alpha 0
// that leaves the colour and clears destination alpha, the bubble's fans in 0xBFBFBF with alpha
// 0x80 (once antialiased, once solid), then fuki256 tiled over the whole target under DATE/DATM=1,
// which only lets the tiles through where destination alpha has bit 7 set: inside the fans. The
// destination alpha test is a shape mask. Here the target's own depth buffer carries the mask:
// cleared to near, the fans write far, and the tiles test GEQUAL at far, which passes exactly on
// the fan pixels and nowhere else, once per pixel as DATE does. Colour and alpha come out as
// retail's: fans written through the current ALPHA, tiles blended over them, alpha 0 outside.
// Lost: the antialiased fan edges, and retail's tiles writing Z=0 into the frame's own Z buffer at
// the target's coordinates.
PC_OVERRIDE void ClsMes::MakeFukidashi(sceVif1Packet *packet) {
    const draw2d::Services &services = draw2d::Get();

    g_shapes.clear();
    this->MakeFukidashi_sub(packet, 1);
    this->MakeFukidashi_sub(packet, 0);

    const gfx::TextureHandle target = BubbleTarget(services.find_texture(kBubbleTarget));
    if (target != gfx::kNullTexture) {
        const gfx::TextureHandle previous = gfx::CurrentRenderTarget();
        gfx::SetRenderTarget(target);

        gfx::DrawState state = services.draw_state();
        state.blend = true;
        state.alpha_test = false;
        state.depth_test = gfx::DepthTest::Always;
        state.depth_write = false;
        state.fog = false;
        state.cull = gfx::CullMode::None;
        state.scissor = false;

        const uint8_t none[4] = {};
        gfx::Clear(false, none, true, 1.0f);

        const float                  fill_bottom = static_cast<float>(SCREEN_HALF_HEIGHT);
        std::array<gfx::Vertex2D, 4> fill = {
            draw2d::Vertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(gfx::kLogicalWidth, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(gfx::kLogicalWidth, fill_bottom, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(0.0f, fill_bottom, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
        };
        gfx::Draw2D(gfx::Primitive::Quads, fill, {}, state);

        gfx::DrawState fans = state;
        fans.depth_write = true;
        for (const Shape &shape : g_shapes) {
            if (shape.vertices.size() >= 3) {
                gfx::Draw2D(shape.primitive, shape.vertices, {}, fans);
            }
        }

        CTexture *paper = services.find_texture("fuki256");
        if (paper != nullptr) {
            gfx::DrawState tiles = state;
            tiles.depth_test = gfx::DepthTest::GEqual;
            const u_char               alpha = this->edge_alpha < 0x80 ? this->edge_alpha : 0x80;
            std::vector<gfx::Vertex2D> quads;
            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 5; col++) {
                    const float x0 = static_cast<float>(col * 0x80);
                    const float y0 = static_cast<float>(row * 0x80);
                    quads.push_back(draw2d::Vertex(x0, y0, 0.0f, 0.0f, 0.0f, 0x80, 0x80, 0x80, alpha));
                    quads.push_back(draw2d::Vertex(x0 + 0x80, y0, 0.0f, 0x80, 0.0f, 0x80, 0x80, 0x80, alpha));
                    quads.push_back(
                        draw2d::Vertex(x0 + 0x80, y0 + 0x80, 0.0f, 0x80, 0x80, 0x80, 0x80, 0x80, alpha));
                    quads.push_back(draw2d::Vertex(x0, y0 + 0x80, 0.0f, 0.0f, 0x80, 0x80, 0x80, 0x80, alpha));
                }
            }
            draw2d::DrawTextured(gfx::Primitive::Quads, quads, paper->tex0, 0x61, tiles);
        }

        gfx::SetRenderTarget(previous);
    }

    g_shapes.clear();
    services.set_test(nullptr);
    services.set_zbuf(nullptr);
    services.set_alpha(nullptr);
}

// Keep the message layout within the fixed character table.
PC_OVERRIDE int ClsMes::SetMesWinTbl(int code, int mode, short x, short y) {
    if (code >= -0x200 && code <= -0x101) {
        if (this->win_line_num > 0) {
            this->win_line[this->win_line_num - 1].space += code + 0x200;
        }

        return 0;
    }

    if (code >= -0x400 && code <= -0x301) {
        if (code + 0x400 == 0) {
            this->clut_now = this->clut_default;
        } else if (code == -0x301) {
            this->clut_now = 0xFF;
        } else {
            this->clut_now = code + 0x401;
        }

        return 0;
    }

    if (code >= -0xA00 && code <= -0x901) {
        this->fukidashi_shape = code + 0xA00;
        return 0;
    }

    if (this->win_line_num >= MES_WIN_LINE_MAX) {
        return 0;
    }

    if (mode == 1) {
        this->win_line[this->win_line_num].code = code + 0x1000;
        this->win_line[this->win_line_num].x = x;
        this->win_line[this->win_line_num].y = y;
        this->win_line[this->win_line_num].clut = this->clut_now;
        this->win_line_num++;
    } else {
        this->win_line[this->win_line_num].code = code;
        this->win_line[this->win_line_num].x = x;
        this->win_line[this->win_line_num].y = y;
        this->win_line[this->win_line_num].clut = this->clut_now;
        this->win_line_num++;
    }

    return 1;
}

PC_OVERRIDE void ClsMes::DrawMesWin() {
    CTexture *texture;
    int       offset_x;
    int       offset_y;
    int       dark;
    int       index;

    if (this->mes_made < 0) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, this->tex_block);

    offset_x = 0;

    if (this->fukidashi != 0 && this->text_columns < 4) {
        offset_x = (4 - this->text_columns) * this->char_width >> 1;
    }

    offset_y = 0;

    if (this->fukidashi == 0 && this->centre_rows != 0) {
        offset_y = this->char_height * (this->rows - this->text_rows) >> 1;
    }

    if (this->fukidashi != 0) {
        this->MakeFukidashi(Vif1Packet);

        texture = draw2d::Get().find_texture("fukidashibase");
        setbilinear(1);
        this->DrawMesWin_sub(texture, -1, -1, 0);
        this->DrawMesWin_sub(texture, 1, -1, 0);
        this->DrawMesWin_sub(texture, -1, 1, 0);
        this->DrawMesWin_sub(texture, 1, 1, 0);
        this->DrawMesWin_sub(texture, 5, 5, 0);
        this->DrawMesWin_sub(texture, 0, 0, 1);

        if (this->fade_in != 0 && this->fade < 1.0f) {
            return;
        }
    } else {
        this->fade = 1.0f;

        if (this->stay_frame != 0) {
            int px;
            int py;
            int width = this->char_width * this->text_columns + 0x20;
            int height = this->char_height * this->text_rows + 0x1C;

            if (0 < this->stay_width) {
                width = this->stay_width;
            }

            if (0 < this->stay_height) {
                height = this->stay_height;
            }

            px = this->text_x + offset_x - 0x10;
            py = this->text_y + offset_y - 0xE;

            if (0 < this->auto_pos) {
                GetPos_AbsPosSet(0x10, 0x10, 0x260, 0x1A0, width, height, this->auto_pos, &px, &py);
                this->text_x = px - offset_x + 0x10;
                this->text_y = py - offset_y + 0xE;
            }

            if (MesAbsDrawOff == 0) {
                MyMenuHelpWinDraw(px, py, width, height, 0x80, 0, 0, draw2d::Get().find_texture("stayframe"));
            }
        }
    }

    this->MyTextureMake();

    texture = draw2d::Get().find_texture("fontbase");

    if (this->clut == 0) {
        return;
    }

    if ((this->blink / 0x1C) % 2 != 0) {
        this->clut[100] = 0x80222280;
        dark = 0;
    } else {
        this->clut[100] = 0x804444FF;
        dark = 1;
    }

    SetClut(Vif1Packet, texture, (i *) this->clut);
    this->blink++;

    texture = draw2d::Get().find_texture("gaiji");
    setbilinear(0);
    set2DSprite_Start(Vif1Packet, texture);
    const bool                 ttf = TtfReady();
    std::vector<gfx::Vertex2D> ttf_edges;
    std::vector<gfx::Vertex2D> ttf_fills;
    std::vector<float>         ttf_xs;
    if (ttf) {
        TtfLayoutRows(*this, ttf_xs);
    }

    for (index = this->text_from; index < this->text_no; index++) {
        if (MesAbsDrawOff != 0) {
            continue;
        }

        int code = this->win_line[index].code;

        if (code < -0x300 || code >= -0x251) {
            continue;
        }

        if (ttf && code >= -0x2DF && QueueTtfGlyph(*this, index, dark, offset_x, offset_y, ttf_xs, ttf_edges, ttf_fills)) {
            continue;
        }

        int gaiji_index = code + 0x300;
        int u = GaijiDataTbl[gaiji_index][GAIJI_U];

        if (code >= -0x2DF && code < -0x251) {
            if (this->narrow_gaiji_set != 1) {
                if (this->narrow_gaiji_set == 2) {
                    u += 0x80;
                } else if (this->char_width < 0xB) {
                    u += 0x80;
                }
            }
        }

        int v = GaijiDataTbl[gaiji_index][GAIJI_V];
        int glyph_width = GaijiDataTbl[gaiji_index][GAIJI_WIDTH];
        int glyph_height = GaijiDataTbl[gaiji_index][GAIJI_HEIGHT];
        int dx = (this->char_width * GaijiDataTbl[gaiji_index][GAIJI_CELLS] - glyph_width) / 2;
        int dy = (this->char_height - glyph_height) / 2;

        dx += GaijiDataTbl[gaiji_index][GAIJI_X_OFF];

        if (this->narrow_gaiji != 0) {
            if (GaijiDataTbl[gaiji_index][GAIJI_CODE] == -0x2BD ||
                GaijiDataTbl[gaiji_index][GAIJI_CODE] == -0x2BA) {
                dx -= 3;
            }
        }

        dy += GaijiDataTbl[gaiji_index][GAIJI_Y_OFF];

        if (this->char_height == 0x14) {
            dy += 1;
        }

        int row = this->win_line[index].y / this->char_height;

        if (row >= static_cast<int>(std::size(this->line_pos)) || this->line_pos[row].x < 0 ||
            this->line_pos[row].y < 0) {
            int      line_x = this->win_line[index].x + this->text_x;
            int      line_y = this->win_line[index].y + this->text_y;
            int      screen_x = offset_x + (line_x + dx);
            CRect_i_ screen;
            screen.x = screen_x;
            int screen_y = offset_y + (line_y + dy) - 3;
            screen.y = screen_y;
            screen.width = glyph_width;
            screen.height = glyph_height;
            CRect_i_ texel(u, v, glyph_width, glyph_height);

            if (this->win_line[index].code >= -0x2DF) {
                this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen_x, screen_y, glyph_width, glyph_height,
                                          u, v, glyph_width, glyph_height);
            }

            if (this->cursor_row < 0) {
                this->DrawGaijiFont(texture, index, texel, screen, 0, dark);
            } else if (this->cursor_lit == 1) {
                if (row == this->cursor_row) {
                    this->DrawGaijiFont(texture, index, texel, screen, 1, dark);
                } else {
                    this->DrawGaijiFont(texture, index, texel, screen, 0, dark);
                }
            } else {
                if (row == this->cursor_row) {
                    this->DrawGaijiFont(texture, index, texel, screen, 0, dark);
                } else {
                    this->DrawGaijiFont(texture, index, texel, screen, 1, dark);
                }
            }
        } else {
            int screen_x = this->win_line[index].x + this->line_pos[row].x;
            int screen_y = this->win_line[index].y + this->line_pos[row].y;
            screen_y += dy;
            CRect_i_ screen(screen_x + dx, screen_y - row * this->char_height, glyph_width, glyph_height);
            CRect_i_ texel(u, v, glyph_width, glyph_height);

            if (this->win_line[index].code >= -0x2DF) {
                this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen_x + dx,
                                          screen_y - row * this->char_height, glyph_width, glyph_height, u, v,
                                          glyph_width, glyph_height);
            }

            if (this->cursor_row < 0) {
                this->DrawGaijiFont(texture, index, texel, screen, 0, dark);
            } else if (this->cursor_lit == 1) {
                if (row == this->cursor_row) {
                    this->DrawGaijiFont(texture, index, texel, screen, 1, dark);
                } else {
                    this->DrawGaijiFont(texture, index, texel, screen, 0, dark);
                }
            } else {
                if (row == this->cursor_row) {
                    this->DrawGaijiFont(texture, index, texel, screen, 0, dark);
                } else {
                    this->DrawGaijiFont(texture, index, texel, screen, 1, dark);
                }
            }
        }
    }

    set2DSprite_End(Vif1Packet, texture);

    if (!ttf_fills.empty()) {
        const gfx::TextureBinding binding = ttffont::Binding();
        if (!ttf_edges.empty()) {
            gfx::Draw2D(gfx::Primitive::Quads, ttf_edges, binding, draw2d::SpriteState());
        }
        gfx::Draw2D(gfx::Primitive::Quads, ttf_fills, binding, draw2d::SpriteState());
        draw2d::RestoreTestZbuf();
    }

    if (this->page_arrow != 0) {
        if ((this->text_no >= this->text_len && this->cursor_row < 0) || this->waiting != 0) {
            if ((this->blink >> 4) % 2 != 0) {
                texture = draw2d::Get().find_texture("syst04");

                int screen_x = this->text_x + (this->char_width * this->text_columns >> 1) - 8;
                int screen_y = this->text_y + this->char_height * this->text_rows;

                CRect_i_ screen(screen_x + offset_x, screen_y + offset_y, 0x10, 0x10);
                CRect_i_ texel(0, 0, 0x10, 0x10);

                if (MesAbsDrawOff == 0) {
                    set2DSprite(Vif1Packet, texture, screen, texel,
                                this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
                }
            }
        }
    }

    if (this->end_mark != 0) {
        texture = draw2d::Get().find_texture("syst04");

        int screen_x = this->text_x + (this->char_width * this->text_columns >> 1) - 8;
        int screen_y = this->text_y + this->char_height * this->text_rows;

        CRect_i_ screen(screen_x + offset_x, screen_y + offset_y, 0x10, 0x10);
        CRect_i_ texel(0, 0, 0x10, 0x10);

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, screen, texel,
                        this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
        }
    }

    if (this->cursor_row >= 0) {
        texture = draw2d::Get().find_texture("gaiji");

        // The cursor stands two characters further left, clear of the choice text.
        int screen_x = this->text_x;
        screen_x -= 0xC;
        screen_x -= this->char_width * 2;

        this->cursor_y = (this->cursor_y + this->char_height * this->cursor_row) / 2;

        int screen_y = this->cursor_y + this->text_y;

        CRect_i_ screen(screen_x + offset_x, screen_y - 4 + offset_y, 0x20, 0x20);
        CRect_i_ texel(0x60, 0x60, 0x20, 0x20);

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, screen, texel,
                        this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
        }
    } else {
        this->cursor_y = 0;
    }
}
