#include "clsmes.hpp"
#include "mathconst.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <map>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

#include "caption_text.hpp"
#include "captionfont.hpp"
#include "clsmes_port.hpp"
#include "draw2d_port.hpp"
#include "gametext.hpp"
#include "gameutil.hpp"
#include "glyph_draw.hpp"
#include "localize.hpp"
#include "menu_pointer.hpp"
#include "mglib.hpp"
#include "mglib_port.hpp"
#include "platform/config.hpp"
#include "platform/glyphs.hpp"
#include "platform/ttffont.hpp"
#include "rect.hpp"
#include "screen.hpp"
#include "snd.hpp"
#include "text_shadow.hpp"
#include "texture.hpp"
#include "textwipe.hpp"

extern float RandTbl[64];
extern float RandTbl2[64];
extern MES_FUCHI FuchiTbl_E[];

namespace {

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

// The window ClsMesSetChoiceRows names, its first choice row, and whether its choices run on past it.
const ClsMes *g_choice_mes = nullptr;
int           g_choice_first_row = 0;
bool          g_choice_more_above = false;
bool          g_choice_more_below = false;

// The window ClsMesSetMessageOverlay names, and the messages laid over its own.
const ClsMes *g_overlay_mes = nullptr;
short        *g_overlay_file = nullptr;

// Message line's text in a message file, laid out as SetBuff reads one; null where it has none.
short *FindMessage(short *file, int line) {
    int    count = file[0];
    short *body = &file[1];

    for (int i = 0; i < count; i++) {
        if (line == body[i * 2 + 1]) {
            return &body[count] + body[i * 2 + 2];
        }
    }

    return nullptr;
}

// The wide argument DrawGaijiFont takes for a character of palette entry clut on row. 1 dims it, as retail
// does, by doubling its colour: dark text on the paper bubble pales. 2, the port's, halves its opacity
// instead: a coloured choice in a ClsMesSetChoiceRows window, which doubling would brighten rather than
// pale. With a cursor, every row but the cursor's dims (only the cursor's, with cursor_lit), save the text
// rows above a window's choices.
int ChoiceRowDim(const ClsMes &mes, int row, int clut) {
    const bool ours = &mes == g_choice_mes;
    if (mes.cursor_row < 0 || (ours && row < g_choice_first_row)) {
        return 0;
    }
    const bool dim = mes.cursor_lit == 1 ? row == mes.cursor_row : row != mes.cursor_row;
    if (!dim) {
        return 0;
    }
    return ours && clut != mes.clut_default ? 2 : 1;
}

// ---- Message text from a TrueType font ----
// Without Chronicle Sharp's letters (below: they need font.ttf and font_thin.ttf), a character font.ttf in a lang folder
// (Chronicle Sharp, the save's or the executable's, or the font --font names) has is drawn from it at the screen's own
// resolution, in the cell the game laid the bitmap character out in, instead of from the 14x20 bitmaps of gaiji.img.
// Icons and characters the font lacks draw as before.

bool TtfReady() {
    static bool tried = false;
    if (!tried) {
        tried = true;
        std::filesystem::path file = LocalizeFindFile("font.ttf");
        if (!file.empty() && ttffont::Load(file)) {
            std::fprintf(stderr, "font: message text from %s\n", file.string().c_str());
        } else if (file.empty()) {
            // Chronicle Sharp ships in lang/; this is someone who removed it, so say where one goes.
            std::fprintf(stderr,
                         "font: no font.ttf in lang/ beside the executable or in the save folder, and no --font; message "
                         "text uses the game's bitmaps (docs/LOCALIZATION.md)\n");
        }
    }
    return ttffont::Loaded();
}

// ---- Message letters from Chronicle Sharp ----
// The 88 letters of gaiji.img (nine 14x20 cells a row from u = 128 for the bold set and u = 256 for the narrower one),
// drawn from Chronicle Sharp at the screen's own resolution: font.ttf's letters for the bold set, font_thin.ttf's for the
// narrower. Each letter's cell is drawn once at the size the screen shows it, the narrower letters with the drop shadow
// they carry in their own texels, into the captions' font atlas, and the game's letters are cut from it as the game cuts
// them from its bitmaps, so it lays them out and edges them as it always has; only their shapes are the font's. The
// fonts lay each letter out on its cell, 50 units a cell pixel with the baseline at its row 15.

constexpr std::string_view kLetterOrder =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'=\"!?#&+-*/%()@|<>{}[]:,.$0123456789";
constexpr int kCellW = 14;
constexpr int kCellH = 20;
constexpr int kLettersU = 128; // the letters' left edge in gaiji.img
constexpr int kLettersW = 256; // both sets

// A letter DrawMesWin draws: the part of its cell (in gaiji.img's texels), where (logical), and its tint.
struct PendingLetter {
    CRect_i_ texel;
    float    x, y, width, height;
    u_char   r, g, b, a;
};

std::vector<PendingLetter> *g_letters = nullptr; // where DrawMesWin's letters go while it draws them

// The cells drawn so far, by set, letter and height in screen pixels, while the atlas they are in lasts.
std::map<std::tuple<int, int, int>, capfont::Glyph> g_cells;
unsigned                                            g_cells_generation = 0;

// The drop shadow the game's narrower letters carry in their own texels (the bold ones have none): the letter's
// coverage moved down and right, blurred a little and thickened, at most half dark. Its shape was measured on the
// game's 88 cells; `scale` is pixels per game pixel.
std::vector<float> BakedShadow(const std::vector<float> &cov, int w, int h, float scale) {
    constexpr float kShiftX = 0.866f, kShiftY = 0.841f, kBlur = 0.741f, kGain = 9.21f, kMost = 0.512f;
    const float     sigma = kBlur * scale;
    const int       reach = static_cast<int>(std::ceil(3 * sigma + std::max(kShiftX, kShiftY) * scale)) + 1;
    auto            kernel = [&](float shift) {
        std::vector<float> k(static_cast<size_t>(2 * reach + 1));
        float              sum = 0;
        for (int t = -reach; t <= reach; t++) {
            const float d = static_cast<float>(t) - shift;
            k[static_cast<size_t>(t + reach)] = std::exp(-d * d / (2 * sigma * sigma));
            sum += k[static_cast<size_t>(t + reach)];
        }
        for (float &v : k) {
            v /= sum;
        }
        return k;
    };
    // A pixel's shadow comes from the letter up and to the left of it: the kernels are centred that far back.
    const std::vector<float> kx = kernel(kShiftX * scale);
    const std::vector<float> ky = kernel(kShiftY * scale);
    std::vector<float>       across(cov.size(), 0.0f), out(cov.size(), 0.0f);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0;
            for (int t = -reach; t <= reach; t++) {
                if (x - t >= 0 && x - t < w) {
                    sum += kx[static_cast<size_t>(t + reach)] * cov[static_cast<size_t>(y) * w + (x - t)];
                }
            }
            across[static_cast<size_t>(y) * w + x] = sum;
        }
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float sum = 0;
            for (int t = -reach; t <= reach; t++) {
                if (y - t >= 0 && y - t < h) {
                    sum += ky[static_cast<size_t>(t + reach)] * across[static_cast<size_t>(y - t) * w + x];
                }
            }
            out[static_cast<size_t>(y) * w + x] = kMost * std::min(1.0f, kGain * sum);
        }
    }
    return out;
}

// Whether the fonts the letters are drawn from are both there.
bool LettersReady() {
    static const bool ready = caption::Ready() && caption::ThinReady();
    return ready;
}

// Letter `index` of `set` drawn into its cell `size` screen pixels high, in the atlas: the white letter over its black
// shadow.
bool CellGlyph(int set, int index, int size, capfont::Glyph &out) {
    if (g_cells_generation != capfont::Generation()) {
        g_cells.clear();
        g_cells_generation = capfont::Generation();
    }
    const auto key = std::make_tuple(set, index, size);
    if (const auto found = g_cells.find(key); found != g_cells.end()) {
        out = found->second;
        return true;
    }
    const float                scale = static_cast<float>(size) / kCellH;
    const int                  width = static_cast<int>(std::lround(kCellW * scale)), height = size;
    const int                  baseline = static_cast<int>(std::lround(15 * scale));
    std::vector<unsigned char> alpha;
    int                        w, h, x0, y0;
    if (!capfont::RenderGlyphScaled(static_cast<unsigned char>(kLetterOrder[static_cast<size_t>(index)]), set, scale / 50.0f,
                                    alpha, w, h, x0, y0)) {
        return false;
    }
    std::vector<float> cov(static_cast<size_t>(width) * height, 0.0f);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const int cx = x0 + x, cy = baseline + y0 + y;
            if (cx >= 0 && cx < width && cy >= 0 && cy < height) {
                cov[static_cast<size_t>(cy) * width + cx] = alpha[static_cast<size_t>(y) * w + x] / 255.0f;
            }
        }
    }
    const std::vector<float>   shadow = set == 1 ? BakedShadow(cov, width, height, scale) : std::vector<float>(cov.size(), 0.0f);
    std::vector<unsigned char> rgba(cov.size() * 4);
    for (size_t k = 0; k < cov.size(); k++) {
        const float a = cov[k] + (1 - cov[k]) * shadow[k];
        rgba[k * 4] = rgba[k * 4 + 1] = rgba[k * 4 + 2] = static_cast<unsigned char>(a > 0 ? std::lround(255 * cov[k] / a) : 255);
        rgba[k * 4 + 3] = static_cast<unsigned char>(std::lround(a * 255));
    }
    capfont::Glyph glyph;
    if (!capfont::AddImage(rgba, width, height, glyph)) {
        return false;
    }
    // Adding it may have started the atlas again.
    if (g_cells_generation != capfont::Generation()) {
        g_cells.clear();
        g_cells_generation = capfont::Generation();
    }
    g_cells[key] = glyph;
    out = glyph;
    return true;
}

// Whether a texel rect of gaiji.img lies in one of the letters' cells.
bool FontLetter(const CRect_i_ &texel) {
    const int across = texel.x - kLettersU;
    if (across < 0 || texel.width <= 0 || texel.height <= 0 || across + texel.width > kLettersW || texel.y < 0) {
        return false;
    }
    const int in_set = across % 128;
    const int column = in_set / kCellW;
    const int row = texel.y / kCellH;
    return column < 9 && (in_set + texel.width - 1) / kCellW == column && (texel.y + texel.height - 1) / kCellH == row &&
           row * 9 + column < static_cast<int>(kLetterOrder.size());
}

// The letter at texel, to be drawn at x, y (logical) from the font once DrawMesWin has laid out its window.
void LetterQuad(const CRect_i_ &texel, float x, float y, float width, float height, int r, int g, int b, int alpha) {
    auto clamp = [](int v) { return static_cast<u_char>(std::clamp(v, 0, 255)); };
    g_letters->push_back({texel, x, y, width, height, clamp(r), clamp(g), clamp(b), clamp(alpha)});
}

// The window's letters, each from its cell drawn at the screen's height for a cell.
void DrawLetters(const std::vector<PendingLetter> &letters) {
    if (letters.empty()) {
        return;
    }
    const gfx::LogicalMapping  mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float                rows = draw2d::RowScale();
    const int                  size = std::clamp(static_cast<int>(std::lround(kCellH * mapping.scale_y * rows)), kCellH, 512);
    std::vector<gfx::Vertex2D> quads;
    // Made again, once, if the atlas started over while the cells were drawn into it (that would leave the first ones
    // stale).
    for (int attempt = 0; attempt < 2; attempt++) {
        const unsigned generation = capfont::Generation();
        quads.clear();
        for (const PendingLetter &letter : letters) {
            const int      across = letter.texel.x - kLettersU;
            const int      set = across / 128, in_set = across % 128;
            const int      cell_u = kLettersU + set * 128 + in_set / kCellW * kCellW, cell_v = letter.texel.y / kCellH * kCellH;
            capfont::Glyph cell;
            if (!CellGlyph(set, (letter.texel.y / kCellH) * 9 + in_set / kCellW, size, cell)) {
                continue;
            }
            const float du = (cell.u1 - cell.u0) / kCellW, dv = (cell.v1 - cell.v0) / kCellH;
            const float u0 = cell.u0 + static_cast<float>(letter.texel.x - cell_u) * du, u1 = u0 + static_cast<float>(letter.texel.width) * du;
            const float v0 = cell.v0 + static_cast<float>(letter.texel.y - cell_v) * dv, v1 = v0 + static_cast<float>(letter.texel.height) * dv;
            const float x0 = letter.x, x1 = letter.x + letter.width, y0 = letter.y * rows, y1 = (letter.y + letter.height) * rows;
            quads.push_back(draw2d::Vertex(x0, y0, 0.0f, u0, v0, letter.r, letter.g, letter.b, letter.a));
            quads.push_back(draw2d::Vertex(x1, y0, 0.0f, u1, v0, letter.r, letter.g, letter.b, letter.a));
            quads.push_back(draw2d::Vertex(x1, y1, 0.0f, u1, v1, letter.r, letter.g, letter.b, letter.a));
            quads.push_back(draw2d::Vertex(x0, y1, 0.0f, u0, v1, letter.r, letter.g, letter.b, letter.a));
        }
        if (generation == capfont::Generation()) {
            break;
        }
    }
    if (!quads.empty()) {
        textwipe::Draw2D(gfx::Primitive::Quads, quads, capfont::Binding(), draw2d::SpriteState());
        draw2d::RestoreTestZbuf();
    }
}

// Myset2DSprite_Fuchi's outline and shadow passes for a font letter. Retail's pass is the 14x20 letter again, a whole
// pixel off: its soft one-texel edges make the shadow a soft one. The font's letter is sharp, so each pass is spread
// over that pixel instead (wider for the outline table), as a grid of fainter copies that together come to the pass's
// strength. False where the letter is not the font's.
bool LetterFuchi(const ClsMes &mes, const CRect_i_ &screen, const CRect_i_ &texel) {
    if (g_letters == nullptr || !FontLetter(texel)) {
        return false;
    }
    constexpr int kSteps = 4;
    float         spread = 1.0f, weaken = 1.0f;
    auto          pass = [&](float dx, float dy, int r, int g, int b, int alpha) {
        const float strength = weaken * static_cast<float>(std::min(alpha, 0x80)) / 128.0f;
        const float each = 1.0f - std::pow(1.0f - std::min(strength, 0.999f), 1.0f / (kSteps * kSteps));
        const int   tap_alpha = static_cast<int>(std::lround(each * 128.0f));
        for (int i = 0; i < kSteps; i++) {
            for (int j = 0; j < kSteps; j++) {
                const float sx = ((static_cast<float>(i) + 0.5f) / kSteps - 0.5f) * spread;
                const float sy = ((static_cast<float>(j) + 0.5f) / kSteps - 0.5f) * spread;
                LetterQuad(texel, static_cast<float>(screen.x) + dx + sx, static_cast<float>(screen.y) + dy + sy,
                           static_cast<float>(screen.width), static_cast<float>(screen.height), r, g, b, tap_alpha);
            }
        }
    };
    const int cap = mes.edge_alpha;
    switch (mes.style) {
        case MES_EDGE_WHITE:
            pass(1, 1, 0xFF, 0xFF, 0xFF, std::min(cap, 0x40));
            break;
        case MES_EDGE_BLACK:
            pass(1, 1, 0, 0, 0, std::min(cap, 0x40));
            break;
        case MES_EDGE_TABLE:
            // The outline the table makes (a weapon's name), two pixels out each way in the game: the game's soft letters
            // fade it and keep it short of black, so the sharp letters' copies sit 1.6 pixels out, spread a little wider
            // and a little lighter.
            spread = 1.2f;
            weaken = 0.85f;
            for (int i = 0; FuchiTbl_E[i].alpha > 0; i++) {
                constexpr float kReach = 0.8f;
                pass(static_cast<float>(FuchiTbl_E[i].x) * kReach, static_cast<float>(FuchiTbl_E[i].y) * kReach, FuchiTbl_E[i].r,
                     FuchiTbl_E[i].g, FuchiTbl_E[i].b, std::min(cap, FuchiTbl_E[i].alpha));
            }
            break;
        case MES_EDGE_DOUBLE:
            pass(1, 1, 0x40, 0x40, 0x40, std::min(cap, 0x80));
            pass(2, 2, 0, 0, 0, std::min(cap, 0x80));
            break;
    }
    return true;
}

// What DrawGaijiFont tints a character with: the colour table's entry, the flashing cursor row's,
// and double for a lit row.
void GlyphTint(int clut, int dark, int wide, int &r, int &g, int &b, int &alpha) {
    alpha = 0x80;
    switch (FontColorTbl[clut]) {
        case FONT_COLOR_WHITE:
            r = g = b = 0x5F;
            break;
        case FONT_COLOR_BROWN:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            break;
        case FONT_COLOR_YELLOW:
            r = 0x5E;
            g = 0x5E;
            b = 0x20;
            break;
        case FONT_COLOR_CYAN:
            r = 0x20;
            g = 0x5E;
            b = 0x5E;
            break;
        case FONT_COLOR_GREEN:
            r = 0x20;
            g = 0x5E;
            b = 0x20;
            break;
        case FONT_COLOR_BROWN_OPAQUE:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            break;
        case FONT_COLOR_GOLD:
            r = 0x73;
            g = 0x67;
            b = 0x33;
            break;
        case FONT_COLOR_GREY:
            r = g = b = 0x47;
            break;
        case FONT_COLOR_MAGENTA:
            r = 0x5F;
            g = 0x1F;
            b = 0x5F;
            break;
        default:
            r = 0x28;
            g = 0x28;
            b = 0x20;
            break;
    }
    if (clut == 0xFF) {
        if (dark != 0) {
            r = 0x7F;
            g = 0x22;
            b = 0x22;
        } else {
            r = 0x40;
            g = 0x11;
            b = 0x11;
        }
    }
    if (wide == 1) {
        r *= 2;
        g *= 2;
        b *= 2;
    }
    if (wide == 2) {
        alpha /= 2;
    }
}

void TtfQuad(std::vector<gfx::Vertex2D> &out, const ttffont::Glyph &glyph, float x, float y, float width,
             float height, int r, int g, int b, int alpha) {
    auto         clamp = [](int v) { return static_cast<u_char>(v < 0 ? 0 : v > 255 ? 255
                                                                                    : v); };
    const u_char red = clamp(r), green = clamp(g), blue = clamp(b), a = clamp(alpha);
    out.push_back(draw2d::Vertex(x, y, 0.0f, glyph.u0, glyph.v0, red, green, blue, a));
    out.push_back(draw2d::Vertex(x + width, y, 0.0f, glyph.u1, glyph.v0, red, green, blue, a));
    out.push_back(draw2d::Vertex(x + width, y + height, 0.0f, glyph.u1, glyph.v1, red, green, blue, a));
    out.push_back(draw2d::Vertex(x, y + height, 0.0f, glyph.u0, glyph.v1, red, green, blue, a));
}

// The em the font is drawn at: its line fits 82% of the cell's height and its widest glyph the cell's
// width, so a glyph never leaves the cell the game sized the window for.
// Height and baseline follow the bitmap font's proportions.
constexpr float kTextHeight = 1.28f;
constexpr float kMinSqueeze = 0.78f;
constexpr float kBaselineShift = 0.6f;

float TtfEm(const ClsMes &mes, const ttffont::Metrics &) {
    return mes.char_height * 0.82f * kTextHeight * textwipe::Get(textwipe::MessageKind()).size;
}

// Where each laid-out character of a window starts, in the window's own coordinates. A word (a run of
// characters the font draws) keeps the place the game gave it: its letters go one after another by
// their own widths and the word is centred on the cells the game gave it, so rows, indents, centring and
// the icons between words stay where the game put them. A glyph is never given more than its cell, so
// a word is never wider than the game's.
void TtfLayoutRows(ClsMes &mes, std::vector<float> &xs, std::vector<float> &squeeze) {
    const int count = std::clamp(mes.win_line_num, 0, MES_WIN_LINE_MAX);
    xs.assign(static_cast<size_t>(count), 0.0f);
    squeeze.assign(static_cast<size_t>(count), 1.0f);
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
    const textwipe::Tuning   &tune = textwipe::Get(textwipe::MessageKind());
    const float               gap = em * 0.1f + tune.letter;

    auto glyph_of = [&](int i, ttffont::Glyph &glyph) {
        const int      code = mes.win_line[i].code;
        const char32_t ch = code >= -0x2DF && code < -0x251 ? GameTextChar(static_cast<s16>(code)) : 0;
        return ch != 0 && ttffont::GetGlyph(ch, em_px, glyph);
    };
    auto cell_of = [&](int i) {
        const bool next = i + 1 < count && mes.win_line[i + 1].y == mes.win_line[i].y;
        return next ? static_cast<float>(mes.win_line[i + 1].x - mes.win_line[i].x) : static_cast<float>(mes.char_width);
    };

    int i = 0;
    int word = 0;
    int word_y = -1;
    while (i < count) {
        ttffont::Glyph glyph;
        if (!glyph_of(i, glyph)) {
            i++;
            continue;
        }
        if (mes.win_line[i].y != word_y) {
            word_y = mes.win_line[i].y;
            word = 0;
        }
        int                last = i;
        float              natural = 0.0f;
        std::vector<float> pen;
        for (; last < count && mes.win_line[last].y == mes.win_line[i].y && glyph_of(last, glyph); last++) {
            pen.push_back(natural);
            natural += glyph.width / mapping.scale_x * tune.xscale + gap;
        }
        const float span = static_cast<float>(mes.win_line[last - 1].x - mes.win_line[i].x) + cell_of(last - 1);
        const float width = natural - gap;
        const bool  fit = tune.fit >= 0.5f;
        float       sx = fit ? std::clamp(span / std::max(width, 1.0f), kMinSqueeze, 1.0f) : 1.0f;
        float       extra = 0.0f;
        if (fit && width * sx < span && last - i > 1) {
            extra = std::min((span - width * sx) / static_cast<float>(last - i - 1), em * 0.25f);
        }
        const float used = width * sx + extra * static_cast<float>(last - i - 1);
        const float start = static_cast<float>(mes.win_line[i].x) + (fit ? (span - used) / 2 : (span - used) * tune.align) +
                            tune.word * static_cast<float>(word);
        word++;
        for (int k = i; k < last; k++) {
            xs[static_cast<size_t>(k)] = start + pen[static_cast<size_t>(k - i)] * sx + extra * static_cast<float>(k - i);
            squeeze[static_cast<size_t>(k)] = sx * tune.xscale;
        }
        if (tune.lock > 0.0f) {
            // Sprite centering preserves retail's narrow-character offsets and integer rounding.
            for (int k = i; k < last; k++) {
                ttffont::Glyph letter;
                if (!glyph_of(k, letter)) {
                    continue;
                }
                const float ink = letter.width / mapping.scale_x * squeeze[static_cast<size_t>(k)];
                const auto &data = GaijiDataTbl[mes.win_line[k].code + 0x300];
                int         dx = (mes.char_width * data[GAIJI_CELLS] - data[GAIJI_WIDTH]) / 2 + data[GAIJI_X_OFF];
                if (mes.narrow_gaiji && (data[GAIJI_CODE] == -0x2BD || data[GAIJI_CODE] == -0x2BA)) {
                    dx -= 3;
                }
                const float centred = static_cast<float>(mes.win_line[k].x + dx) +
                                      (static_cast<float>(data[GAIJI_WIDTH]) - ink) / 2;
                float      &place = xs[static_cast<size_t>(k)];
                place += (centred - place) * std::min(1.0f, tune.lock);
            }
        }
        i = last;
    }
}

// Queues the font's glyph for win_line[index]: an edge pass per outline and one fill. False where
// the font has none, and the bitmap character is drawn.
bool QueueTtfGlyph(ClsMes &mes, int index, int dark, int offset_x, int offset_y, const std::vector<float> &xs,
                   const std::vector<float>   &squeeze,
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
    if (row < 0 || row >= static_cast<int>(std::size(mes.line_pos)) || mes.line_pos[row].x < 0 ||
        mes.line_pos[row].y < 0) {
        origin_x = static_cast<float>(offset_x + mes.text_x);
        cell_y = static_cast<float>(offset_y + line.y + mes.text_y);
    } else {
        origin_x = static_cast<float>(mes.line_pos[row].x);
        // Explicit row positions omit the default bitmap path's three-pixel upward offset.
        cell_y = static_cast<float>(line.y + mes.line_pos[row].y - row * mes.char_height + 3);
    }

    // The font's line is centred in the cell's height. The bitmap's pixels are screen pixels, so a quad
    // is as many logical units as they come to.
    const textwipe::Tuning &tune = textwipe::Get(textwipe::MessageKind());
    const float             width = glyph.width / mapping.scale_x * squeeze[static_cast<size_t>(index)];
    const float             height = glyph.height / mapping.scale_y * rows * tune.yscale;
    const float             base = cell_y + (mes.char_height - em * (metrics.ascent + metrics.descent)) / 2 + em * metrics.ascent + kBaselineShift;
    float                   x = origin_x + xs[static_cast<size_t>(index)] + tune.dx;
    float                   y = ((base + tune.dy) * rows) + glyph.y_offset / mapping.scale_y * tune.yscale;

    // On whole pixels, so the edges of the strokes are not smeared across two.
    auto snap = [](float value, float scale, float offset) { return (std::round(value * scale + offset) - offset) / scale; };
    x = snap(x, mapping.scale_x, mapping.offset_x);
    y = snap(y, mapping.scale_y, mapping.offset_y);

    const int wide = ChoiceRowDim(mes, row, line.clut);
    int       r, g, b, alpha;
    GlyphTint(line.clut, dark, wide, r, g, b, alpha);

    // The outline and the drop shadow sit at the offsets the bitmaps' use: the shadow is part of the
    // look (it lifts the text off the picture), and at a fraction of the offset it vanishes under the glyph.
    const int cap = static_cast<int>(static_cast<float>(mes.edge_alpha) * tune.opacity);
    // A thin stroke's outline covers little of a pixel, so it is laid down nine times to read as dark as the bitmaps'.
    constexpr int passes = 9;
    auto          edge = [&](float dx, float dy, int er, int eg, int eb, int ea) {
        for (int pass = 0; pass < passes; ++pass) {
            TtfQuad(edges, glyph, x + dx, y + dy * rows, width, height, er, eg, eb, std::min(cap, ea));
        }
    };
    auto cast = [&] {
        // Retail's shadow falls off softly because its bitmaps are soft. A crisp glyph stacked at two
        // offsets reads as a hard outline instead, so it is laid in as taps the texture filter blends.
        TextShadowEach(ConfigGet().text_shadow, [&](const TextShadowTap &tap) {
            TtfQuad(edges, glyph, x + tap.dx, y + tap.dy * rows, width, height, tap.grey, tap.grey, tap.grey,
                    std::min(cap, static_cast<int>(static_cast<float>(tap.alpha) * tune.shadow)));
        });
    };
    switch (mes.style) {
        case MES_EDGE_WHITE:
            edge(1, 1, 0xFF, 0xFF, 0xFF, 0x40);
            break;
        case MES_EDGE_BLACK:
            cast();
            break;
        case MES_EDGE_TABLE:
            cast();
            break;
        case MES_EDGE_DOUBLE:
            cast();
            break;
    }
    if (tune.weight > 0.0f) {
        const float half = tune.weight / 2;
        const float shifts[4][2] = {
            {-half, 0.0f },
            {half,  0.0f },
            {0.0f,  -half},
            {0.0f,  half }
        };
        for (const auto &shift : shifts) {
            TtfQuad(fills, glyph, x + shift[0], y + shift[1] * rows, width, height, r, g, b, std::min(cap, alpha));
        }
    }
    TtfQuad(fills, glyph, x, y, width, height, r, g, b, std::min(cap, alpha));
    return true;
}

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

PORT_OVERRIDE void MakeRandTbl(float lo, float hi) {
    if (GameTextKeepsRandom()) {
        return;
    }
    for (int i = 0; i < 64; ++i) {
        RandTbl[i] = ((hi - lo) * static_cast<float>(std::rand())) / 2147483648.0f;
        RandTbl[i] += lo;
    }
}

PORT_OVERRIDE void MakeRandTbl2(float lo, float hi) {
    if (GameTextKeepsRandom()) {
        return;
    }
    for (int i = 0; i < 64; ++i) {
        RandTbl2[i] = ((hi - lo) * static_cast<float>(std::rand())) / 2147483648.0f;
        RandTbl2[i] += lo;
    }
}

void GetPos_AbsPosSet(int x, int y, int width, int height, int win_width, int win_height, int align,
                      int *out_x, int *out_y);
void MyMenuHelpWinDraw(int x, int y, int width, int height, int shade, int u, int v, CTexture *texture);

// Named the other way round from set2DSprite: src is where the sprite lands, dst the texels.
PORT_OVERRIDE void Myset2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &src, const CRect_i_ &dst, u8 r,
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

PORT_OVERRIDE void DrawMaru(sceVif1Packet *packet, int x, int y, int width, int height, int lift, int unused, int rough,
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

        px = static_cast<int>(across);
        py = static_cast<int>(down);
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

    px = static_cast<int>(across);
    py = static_cast<int>(down);
    px += x;
    py += y - lift;

    ShapePoint(px, py);
}

PORT_OVERRIDE void ClsMes::MakeFukidashi_sub(sceVif1Packet *packet, int prim) {
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

        int base_x = static_cast<int>(LinerInterpolation(this->grow_x, this->win_x, this->fade));
        int base_y = static_cast<int>(LinerInterpolation(this->grow_y, this->win_y, this->fade));

        mirror = static_cast<int>(RandTbl[0]);

        for (i = 0; i < 15; i++) {
            if (mirror % 2 != 0) {
                x = static_cast<int>(grown_width * (1.0f - shape[i][0]));
            } else {
                x = static_cast<int>(grown_width * shape[i][0]);
            }

            if (mirror > 1) {
                y = static_cast<int>(grown_height * (1.0f - shape[i][1]));
            } else {
                y = static_cast<int>(grown_height * shape[i][1]);
            }

            x += base_x;
            y += base_y;
            y -= top;

            ShapePoint(x, y);
        }
    } else if (this->fukidashi_shape == 1 || this->fukidashi_shape == 2) {
        x = static_cast<int>(LinerInterpolation(this->grow_x, this->win_x, this->fade));
        y = static_cast<int>(LinerInterpolation(this->grow_y, this->win_y, this->fade));

        int bubble_shape = this->fukidashi_shape;

        if (bubble_shape == 1) {
            DrawMaru(packet, x, y, static_cast<int>(grown_width), static_cast<int>(grown_height), top, 0x20, bubble_shape, prim);
        } else {
            int   upper_width = static_cast<int>(grown_width) - this->char_width;
            int   half_height = static_cast<int>(grown_height) >> 1;
            float half_height_float = static_cast<float>(half_height);

            DrawMaru(packet, x, y, upper_width, static_cast<int>(half_height_float + 1.5f * this->char_height), top,
                     0x20, 0, prim);

            float pad = 1.5f * this->char_height;
            int   indent = this->char_width;

            y = static_cast<int>(y + half_height - pad);
            DrawMaru(packet, x + indent, y, static_cast<int>(grown_width) - indent, static_cast<int>(half_height + pad), top, 0x20,
                     0, prim);
        }
    } else {
        x = static_cast<int>(LinerInterpolation(this->grow_x, this->win_x, this->fade));
        y = static_cast<int>(LinerInterpolation(this->grow_y, this->win_y, this->fade));

        BeginShape(gfx::Primitive::TriangleFan);

        int oval_width = static_cast<int>(grown_width);
        int oval_height = static_cast<int>(grown_height);
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

            px = static_cast<int>(across);
            py = static_cast<int>(down);

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

        px = static_cast<int>(across);
        py = static_cast<int>(down);

        px += x;
        py += y - top;

        ShapePoint(px, py);
    }

    if (this->tail_on != 0) {
        if (this->fukidashi_shape == 2) {
            x = static_cast<int>(LinerInterpolation(this->grow_x, (this->tail_x + this->tail_tip_x) >> 1, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, (this->tail_y + this->tail_tip_y) >> 1, this->fade));
            DrawMaru(packet, x - 10, y - 10, 20, 20, top, 0x20, 0, prim);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_tip_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_tip_y, this->fade));
            DrawMaru(packet, x - 5, y - 5, 10, 10, top, 0x20, 0, prim);
        } else {
            BeginShape(gfx::Primitive::Triangles);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_tip_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_tip_y, this->fade));
            y -= top;
            ShapePoint(x, y);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_left_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_left_y, this->fade));
            y -= top;
            ShapePoint(x, y);

            x = static_cast<int>(LinerInterpolation(this->grow_x, this->tail_right_x, this->fade));
            y = static_cast<int>(LinerInterpolation(this->grow_y, this->tail_right_y, this->fade));
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
PORT_OVERRIDE void ClsMes::MakeFukidashi(sceVif1Packet *packet) {
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
            draw2d::Vertex(kScreenWidth, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(kScreenWidth, fill_bottom, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
            draw2d::Vertex(0.0f, fill_bottom, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0),
        };
        textwipe::Draw2D(gfx::Primitive::Quads, fill, {}, state);

        gfx::DrawState fans = state;
        fans.depth_write = true;
        for (const Shape &shape : g_shapes) {
            if (shape.vertices.size() >= 3) {
                textwipe::Draw2D(shape.primitive, shape.vertices, {}, fans);
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
PORT_OVERRIDE int ClsMes::SetMesWinTbl(int code, int mode, short x, short y) {
    if (code >= -0x200 && code <= -0x101) {
        if (this->win_line_num > 0 && this->win_line_num <= MES_WIN_LINE_MAX) {
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

    if (this->win_line_num < 0 || this->win_line_num >= MES_WIN_LINE_MAX) {
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

// The pad buttons among the font's icons (kNamed in gametext.cpp), drawn from the button symbol
// files (platform/glyphs.hpp) when input.glyphs is "new". The symbol keeps the proportions of its
// art, scaled so a round face button fills the icon's height, and sits in the middle of the icon's
// slot; a wide one (a key) may reach out of it, up to twice the slot's width.
namespace {

bool DrawPadGlyph(int code, const CRect_i_ &screen, int alpha) {
    glyphs::Button button;
    switch (code) {
        case -0x300:
            button = glyphs::Button::Select;
            break;
        case -0x2FF:
            button = glyphs::Button::Start;
            break;
        case -0x2FE:
            button = glyphs::Button::L1;
            break;
        case -0x2FD:
            button = glyphs::Button::R1;
            break;
        case -0x2FC:
            button = glyphs::Button::L2;
            break;
        case -0x2FB:
            button = glyphs::Button::R2;
            break;
        case -0x2FA:
            button = glyphs::Button::Circle;
            break;
        case -0x2F9:
            button = glyphs::Button::Triangle;
            break;
        case -0x2F8:
            button = glyphs::Button::Cross;
            break;
        case -0x2F7:
            button = glyphs::Button::Square;
            break;
        case -0x2F6:
            button = glyphs::Button::Dpad;
            break;
        case -0x2F5:
            button = glyphs::Button::DpadUpDown;
            break;
        case -0x2F4:
            button = glyphs::Button::DpadSides;
            break;
        default:
            return false;
    }
    return GlyphDrawSlot(button, screen, alpha);
}

} // namespace

// Read a colour only for a palette entry FontColorTbl has. Retail looks up the red text's 0xFF too,
// 240 entries past the table, and then sets that colour itself.
PORT_OVERRIDE void ClsMes::DrawGaijiFont(CTexture *texture, int index, const CRect_i_ &texel, const CRect_i_ &screen,
                                         int wide, int dark) {
    u8  r;
    u8  g;
    u8  b;
    u8  alpha;
    u8  clut;
    int code;
    int u;
    int v;

    code = this->win_line[index].code;

    if (code >= -0x300 && code < -0x2DF) {
        if (DrawPadGlyph(code, screen, this->edge_alpha < 0x80 ? this->edge_alpha : 0x80)) {
            return;
        }
        set2DSprite_Core(Vif1Packet, texture, screen, texel, 0x80, 0x80, 0x80,
                         this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
        return;
    }

    clut = this->win_line[index].clut;

    switch (clut < std::size(FontColorTbl) ? FontColorTbl[clut] : 0) {
        case FONT_COLOR_WHITE:
            r = 0x5F;
            g = 0x5F;
            b = 0x5F;
            alpha = 0x80;
            break;

        case FONT_COLOR_BROWN:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            alpha = 0x80;
            break;

        case FONT_COLOR_YELLOW:
            r = 0x5E;
            g = 0x5E;
            b = 0x20;
            alpha = 0x80;
            break;

        case FONT_COLOR_CYAN:
            r = 0x20;
            g = 0x5E;
            b = 0x5E;
            alpha = 0x80;
            break;

        case FONT_COLOR_GREEN:
            r = 0x20;
            g = 0x5E;
            b = 0x20;
            alpha = 0x80;
            break;

        case FONT_COLOR_BROWN_OPAQUE:
            r = 0x22;
            g = 0x20;
            b = 0x18;
            alpha = 0x80;
            break;

        case FONT_COLOR_GOLD:
            r = 0x73;
            g = 0x67;
            b = 0x33;
            alpha = 0x80;
            break;

        case FONT_COLOR_GREY:
            r = 0x47;
            g = 0x47;
            b = 0x47;
            alpha = 0x80;
            break;

        case FONT_COLOR_MAGENTA:
            r = 0x5F;
            g = 0x1F;
            b = 0x5F;
            alpha = 0x80;
            break;

        default:
            r = 0x28;
            g = 0x28;
            b = 0x20;
            alpha = 0x80;
            break;
    }

    if (clut == 0xFF) {
        if (dark != 0) {
            alpha = 0x80;
            b = 0x22;
            g = 0x22;
            r = 0x7F;
        } else {
            alpha = 0x80;
            b = 0x11;
            g = 0x11;
            r = 0x40;
        }
    }

    if (wide == 1) {
        r = r * 2;
        g = g * 2;
        b = b * 2;
    }

    if (wide == 2) {
        alpha /= 2;
    }

    if (g_letters != nullptr && FontLetter(texel)) {
        LetterQuad(texel, static_cast<float>(screen.x), static_cast<float>(screen.y), static_cast<float>(screen.width),
                   static_cast<float>(screen.height), r, g, b, this->edge_alpha < alpha ? this->edge_alpha : alpha);
    } else {
        set2DSprite_Core(Vif1Packet, texture, screen, texel, r, g, b, this->edge_alpha < alpha ? this->edge_alpha : alpha);
    }

    switch (this->win_line[index].code) {
        case -0x284:
        case -0x280:
        case -0x27F:
        case -0x27A:
        case -0x275:
        case -0x271:
        case -0x26C:
        case -0x268:
        case -0x25E:
        case -0x258:
            u = 0xE2;
            v = 0xB4;
            break;

        case -0x282:
        case -0x27D:
        case -0x278:
        case -0x274:
        case -0x26F:
        case -0x26B:
        case -0x260:
        case -0x25C:
        case -0x25A:
        case -0x256:
            u = 0xF0;
            v = 0xB4;
            break;

        case -0x281:
        case -0x27C:
        case -0x277:
        case -0x273:
        case -0x26E:
        case -0x26A:
        case -0x267:
        case -0x266:
        case -0x264:
        case -0x25D:
            u = 0x80;
            v = 0xC8;
            break;

        case -0x27B:
        case -0x276:
        case -0x272:
        case -0x26D:
        case -0x269:
        case -0x25F:
        case -0x25B:
        case -0x259:
        case -0x257:
        case -0x255:
            u = 0x9C;
            v = 0xC8;
            break;

        case -0x270:
        case -0x254:
            u = 0x8E;
            v = 0xC8;
            break;

        default:
            return;
    }

    if (this->narrow_gaiji_set != 1) {
        if (this->narrow_gaiji_set == 2) {
            u += 0x80;
        } else if (this->char_width < 0xB) {
            u += 0x80;
        }
    }

    this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen.x, screen.y, screen.width, screen.height, u, v, 14, 20);
    set2DSprite_Core(Vif1Packet, texture, screen, CRect_i_(u, v, 14, 20), r, g, b,
                     this->edge_alpha < alpha ? this->edge_alpha : alpha);
}

PORT_OVERRIDE void ClsMes::DrawMesWin() {
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

    if (this->clut == nullptr) {
        return;
    }

    if ((this->blink / 0x1C) % 2 != 0) {
        this->clut[100] = 0x80222280;
        dark = 0;
    } else {
        this->clut[100] = 0x804444FF;
        dark = 1;
    }

    SetClut(Vif1Packet, texture, reinterpret_cast<i *>(this->clut));
    this->blink++;

    texture = draw2d::Get().find_texture("gaiji");
    setbilinear(0);
    // The letters drawn from Chronicle Sharp, where both its fonts are there, in place of the font drawn in the TrueType
    // path's cells.
    const bool letters_ready = LettersReady();
    auto       text_pass = [&](bool ttf) {
        std::vector<PendingLetter> letters;
        g_letters = letters_ready ? &letters : nullptr;
        set2DSprite_Start(Vif1Packet, texture);
        std::vector<gfx::Vertex2D> ttf_edges;
        std::vector<gfx::Vertex2D> ttf_fills;
        std::vector<float>         ttf_xs;
        std::vector<float>         ttf_squeeze;
        if (ttf) {
            TtfLayoutRows(*this, ttf_xs, ttf_squeeze);
        }

        const int line_count = std::clamp(this->win_line_num, 0, MES_WIN_LINE_MAX);
        const int first_line = std::clamp(this->text_from, 0, line_count);
        const int end_line = std::clamp(this->text_no, first_line, line_count);
        for (index = first_line; index < end_line; index++) {
            if (MesAbsDrawOff != 0) {
                continue;
            }

            int code = this->win_line[index].code;

            if (code < -0x300 || code >= -0x251) {
                continue;
            }

            if (ttf && code >= -0x2DF && QueueTtfGlyph(*this, index, dark, offset_x, offset_y, ttf_xs, ttf_squeeze, ttf_edges, ttf_fills)) {
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

            if (row < 0 || row >= static_cast<int>(std::size(this->line_pos)) || this->line_pos[row].x < 0 ||
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

                if (this->win_line[index].code >= -0x2DF && !LetterFuchi(*this, screen, texel)) {
                    this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen_x, screen_y, glyph_width, glyph_height,
                                              u, v, glyph_width, glyph_height);
                }

                this->DrawGaijiFont(texture, index, texel, screen, ChoiceRowDim(*this, row, this->win_line[index].clut), dark);
            } else {
                int screen_x = this->win_line[index].x + this->line_pos[row].x;
                int screen_y = this->win_line[index].y + this->line_pos[row].y;
                screen_y += dy;
                CRect_i_ screen(screen_x + dx, screen_y - row * this->char_height, glyph_width, glyph_height);
                CRect_i_ texel(u, v, glyph_width, glyph_height);

                if (this->win_line[index].code >= -0x2DF && !LetterFuchi(*this, screen, texel)) {
                    this->Myset2DSprite_Fuchi(Vif1Packet, texture, screen_x + dx,
                                              screen_y - row * this->char_height, glyph_width, glyph_height, u, v,
                                              glyph_width, glyph_height);
                }

                this->DrawGaijiFont(texture, index, texel, screen, ChoiceRowDim(*this, row, this->win_line[index].clut), dark);
            }
        }

        set2DSprite_End(Vif1Packet, texture);
        g_letters = nullptr;
        DrawLetters(letters);

        if (!ttf_fills.empty()) {
            const gfx::TextureBinding binding = ttffont::Binding();
            if (!ttf_edges.empty()) {
                textwipe::Draw2D(gfx::Primitive::Quads, ttf_edges, binding, draw2d::SpriteState());
            }
            textwipe::Draw2D(gfx::Primitive::Quads, ttf_fills, binding, draw2d::SpriteState());
            draw2d::RestoreTestZbuf();
        }
    };
    if (textwipe::Active() && !letters_ready && TtfReady()) {
        textwipe::Both(text_pass);
    } else {
        text_pass(!letters_ready && TtfReady());
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

    // The dialog framework's choice window, its choices running on past it: the page arrow, blinking as
    // it does, where the page arrow goes for more below, and turned over above the first choice for
    // more above (ClsMesSetChoiceScroll).
    if (this == g_choice_mes && this->cursor_row >= 0 && (g_choice_more_above || g_choice_more_below) &&
        (this->blink >> 4) % 2 != 0 && MesAbsDrawOff == 0) {
        texture = draw2d::Get().find_texture("syst04");

        int       screen_x = this->text_x + (this->char_width * this->text_columns >> 1) - 8 + offset_x;
        const int alpha = this->edge_alpha < 0x80 ? this->edge_alpha : 0x80;

        if (g_choice_more_above) {
            int screen_y = this->text_y + this->char_height * g_choice_first_row - 0x10;
            set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y + offset_y, 0x10, 0x10),
                        CRect_i_(0, 0x10, 0x10, -0x10), alpha);
        }

        if (g_choice_more_below) {
            int screen_y = this->text_y + this->char_height * this->text_rows;
            set2DSprite(Vif1Packet, texture, CRect_i_(screen_x, screen_y + offset_y, 0x10, 0x10),
                        CRect_i_(0, 0, 0x10, 0x10), alpha);
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

        // The dialog framework's window: the hand is at the mouse pointer while one is in use.
        float pointer_x = 0.0f;
        float pointer_y = 0.0f;
        if (this == g_choice_mes && MenuPointerHand(pointer_x, pointer_y)) {
            screen = CRect_i_(static_cast<int>(pointer_x), static_cast<int>(pointer_y), 0x20, 0x20);
        }

        if (MesAbsDrawOff == 0) {
            set2DSprite(Vif1Packet, texture, screen, texel,
                        this->edge_alpha < 0x80 ? this->edge_alpha : 0x80);
        }
    } else {
        this->cursor_y = 0;
    }
}

void ClsMesSetChoiceRows(const ClsMes *mes, int first_row) {
    g_choice_mes = mes;
    g_choice_first_row = mes != nullptr && first_row > 0 ? first_row : 0;
    if (mes == nullptr) {
        g_choice_more_above = false;
        g_choice_more_below = false;
    }
}

void ClsMesSetChoiceScroll(bool more_above, bool more_below) {
    g_choice_more_above = more_above;
    g_choice_more_below = more_below;
}

void ClsMesSetMessageOverlay(const ClsMes *mes, short *file) {
    g_overlay_mes = mes != nullptr && file != nullptr ? mes : nullptr;
    g_overlay_file = g_overlay_mes != nullptr ? file : nullptr;
}

PORT_OVERRIDE short *ClsMes::GetTextLineDataTop(int line) {
    if (this == g_overlay_mes) {
        if (short *text = FindMessage(g_overlay_file, line)) {
            return text;
        }
    }

    return FindMessage(this->buff, line);
}
