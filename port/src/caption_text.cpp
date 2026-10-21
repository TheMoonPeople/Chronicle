#include "caption_text.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>

#include "captionfont.hpp"
#include "draw2d_port.hpp"
#include "gametext.hpp"
#include "gfx/gfx.hpp"
#include "localize.hpp"
#include "mainselect.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "platform/config.hpp"
#include "texture_port.hpp"
#include "textwipe.hpp"

namespace caption {

namespace {

#if defined(__has_embed)
#if __has_embed("../../tools/font/SourceSansProSemibold.ttf")
#define CHRONICLE_HAS_EMBEDDED_STATUS_FONT 1
alignas(16) constexpr unsigned char kEmbeddedStatusFont[] = {
#embed "../../tools/font/SourceSansProSemibold.ttf"
};
#elif __has_embed("../tools/font/SourceSansProSemibold.ttf")
#define CHRONICLE_HAS_EMBEDDED_STATUS_FONT 1
alignas(16) constexpr unsigned char kEmbeddedStatusFont[] = {
#embed "../tools/font/SourceSansProSemibold.ttf"
};
#endif
#endif

struct Entry {
    const char *key;
    const char *english;
};

constexpr Entry kEntries[kIcons] = {
    {"menu.caption.item",             "ITEM"            },
    {"menu.caption.weapon",           "WEAPON"          },
    {"menu.caption.allies",           "ALLIES"          },
    {"menu.caption.georama_parts",    "GEORAMA PARTS"   },
    {"menu.caption.option",           "OPTION"          },
    {"menu.caption.save",             "SAVE"            },
    {"menu.caption.manuals",          "MANUALS"         },
    {"menu.caption.leave_dungeon",    "LEAVE DUNGEON"   },
    {"menu.caption.other_area",       "GO TO OTHER AREA"},
    {"menu.caption.go_dungeon",       "GO TO DUNGEON"   },
    {"menu.caption.move_remove",      "MOVE/REMOVE"     },
    {"menu.caption.georama_analysis", "GEORAMA ANALYSIS"},
    {"menu.caption.go_outside",       "GO OUTSIDE"      },
    {"menu.caption.config_assembly",  "CONFIG/ASSEMBLY" },
};

const char *s_next_caption_key = nullptr;
const char *s_next_caption_english = nullptr;
std::string s_next_caption_text;

// Painted capitals occupy about 21 logical pixels of each 30-pixel label cell.
constexpr float kEm = 25.0f;

constexpr float kOutlineWidth = 1.6f;
constexpr float kLeastShrink = 0.62f;

std::array<std::optional<CaptionBounds>, kIcons> g_bounds;

std::filesystem::path FindStatusFontPath() {
    std::filesystem::path p = LocalizeFindFile("SourceSansProSemibold.ttf");
    if (!p.empty()) {
        return p;
    }

    static const char *kCandidates[] = {
        "tools/font/SourceSansProSemibold.ttf",
        "../tools/font/SourceSansProSemibold.ttf",
        "../../tools/font/SourceSansProSemibold.ttf",
        "/run/build/chronicle/tools/font/SourceSansProSemibold.ttf",
        "lang/SourceSansProSemibold.ttf"};
    for (const char *rel : kCandidates) {
        std::error_code ec;
        if (std::filesystem::exists(rel, ec)) {
            return std::filesystem::path(rel);
        }
    }
    return {};
}

bool FontReady() {
    // Captions in Chronicle Sharp (or the font put in its place), looked for once.
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (const std::filesystem::path file = LocalizeFindFile("font.ttf"); !file.empty()) {
            capfont::Load(file);
        }
    }
    if (capfont::Loaded() && !capfont::StatusFontLoaded()) {
        static bool tried_status = false;
        if (!tried_status) {
            tried_status = true;
            const std::filesystem::path status_path = FindStatusFontPath();
            if (!status_path.empty() && capfont::LoadStatusFont(status_path)) {
                std::fprintf(stderr, "font: status font from %s\n", status_path.string().c_str());
            }
#if defined(CHRONICLE_HAS_EMBEDDED_STATUS_FONT)
            if (!capfont::StatusFontLoaded()) {
                if (capfont::LoadStatusFontMemory(kEmbeddedStatusFont, sizeof(kEmbeddedStatusFont))) {
                    std::fprintf(stderr, "font: status font loaded from embedded SourceSansProSemibold.ttf (%zu bytes)\n",
                                 sizeof(kEmbeddedStatusFont));
                }
            }
#endif
        }
    }
    if (capfont::Loaded() && !capfont::VariantLoaded()) {
        static bool tried_thin = false;
        if (!tried_thin) {
            tried_thin = true;
            const std::filesystem::path thin = LocalizeFindFile("font_thin.ttf");
            if (!thin.empty() && capfont::LoadVariant(thin)) {
                std::fprintf(stderr, "font: thin variant from %s\n", thin.string().c_str());
            }
        }
    }
    return capfont::Loaded();
}

char32_t Upper(char32_t ch) {
    if ((ch >= U'a' && ch <= U'z') || (ch >= 0xE0 && ch <= 0xFE && ch != 0xF7)) {
        return ch - 0x20;
    }
    return ch == 0x153 ? 0x152 : ch;
}

std::string HelpTitle(int icon) {
    const MENU_ICON_INFO *info = GetMenuIconInfo(icon);
    if (info == nullptr || CommonMenuMes2.buff == nullptr) {
        return {};
    }
    const short *codes = CommonMenuMes2.GetTextLineDataTop(info->help_mes_no);
    if (codes == nullptr) {
        return {};
    }
    std::string  text = GameTextDecode(codes);
    const size_t end = text.find_first_of("\n{");
    if (end != std::string::npos) {
        text.erase(end);
    }
    while (!text.empty() && (text.front() == '"' || text.front() == ' ')) {
        text.erase(text.begin());
    }
    while (!text.empty() && (text.back() == '"' || text.back() == ' ')) {
        text.pop_back();
    }
    return text;
}

std::u32string Decode(std::string_view in) {
    std::u32string out;
    for (size_t i = 0; i < in.size();) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        const int           extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1
                                               : (c >> 4) == 14  ? 2
                                               : (c >> 3) == 30  ? 3
                                                                 : -1;
        if (extra < 0 || i + static_cast<size_t>(extra) >= in.size()) {
            out.push_back(U'?');
            i++;
            continue;
        }
        char32_t ch = extra == 0 ? c : (c & (0x3F >> extra));
        for (int k = 1; k <= extra; k++) {
            ch = (ch << 6) | (static_cast<unsigned char>(in[i + k]) & 0x3F);
        }
        out.push_back(ch);
        i += static_cast<size_t>(extra) + 1;
    }
    return out;
}

struct Layout {
    std::u32string text;
    float          em = kEm * textwipe::Get(textwipe::Kind::Caption).size;
    float          squeeze = 1.0f; // the share of its width a condensed caption keeps
    float          width = 0.0f;
};

float Natural(const std::u32string &text, float em, float scale_x) {
    const textwipe::Tuning &tune = textwipe::Get(textwipe::Kind::Caption);
    const int               em_px = std::max(4, static_cast<int>(em * scale_x + 0.5f));
    float                   total = 0.0f;
    for (char32_t ch : text) {
        capfont::Glyph glyph;
        if (ch == U' ') {
            total += em * 0.32f + tune.word;
            continue;
        }
        if (!capfont::GetGlyph(ch, em_px, glyph)) {
            total += em * 0.5f;
            continue;
        }
        total += glyph.width / scale_x * tune.xscale + em * 0.1f + tune.letter;
    }
    return total;
}

Layout Lay(std::string_view utf8, float box_width) {
    Layout layout;
    layout.text = Decode(utf8);
    for (char32_t &ch : layout.text) {
        ch = Upper(ch);
    }
    const float scale_x = gfx::GetUiMapping(gfx::CurrentRenderTarget()).scale_x;
    float       natural = Natural(layout.text, layout.em, scale_x);
    if (box_width > 0 && natural > box_width) {
        layout.em = kEm * textwipe::Get(textwipe::Kind::Caption).size * std::max(kLeastShrink, box_width / natural);
        natural = Natural(layout.text, layout.em, scale_x);
        if (natural > box_width) {
            layout.squeeze = box_width / natural;
            natural = box_width;
        }
    }
    layout.width = natural;
    return layout;
}

// A position on the screen's pixel grid, in logical units.
float Snap(float value, float scale, float offset) {
    return (std::round(value * scale + offset) - offset) / scale;
}

void Quad(std::vector<gfx::Vertex2D> &out, const capfont::Glyph &glyph, float x, float y, float width, float height,
          const unsigned char (&rgb)[3], int alpha) {
    // GS modulation doubles vertex RGB; halve font colors before drawing.
    const u_char r = static_cast<u_char>(rgb[0] / 2), g = static_cast<u_char>(rgb[1] / 2), b = static_cast<u_char>(rgb[2] / 2);
    const u_char a = static_cast<u_char>(std::clamp(alpha, 0, 0x80));
    out.push_back(draw2d::Vertex(x, y, 0.0f, glyph.u0, glyph.v0, r, g, b, a));
    out.push_back(draw2d::Vertex(x + width, y, 0.0f, glyph.u1, glyph.v0, r, g, b, a));
    out.push_back(draw2d::Vertex(x + width, y + height, 0.0f, glyph.u1, glyph.v1, r, g, b, a));
    out.push_back(draw2d::Vertex(x, y + height, 0.0f, glyph.u0, glyph.v1, r, g, b, a));
}

// The painted labels' own look, measured on them: capitals 17 pixels tall level with the picture's ink, a light
// shadow of the letters a little up and left over a dark one a little down and right, both under them, and the blue
// outline round all three. Every label at the one size; one too wide for its box is narrowed, never shrunk.
constexpr float         kLabelCap = 17.0f;
constexpr float         kLabelOutline = 1.5f;
constexpr float         kLabelRaise = 2.8f;   // the painted ink's top sits this far above the measured bounds' top
constexpr float         kLabelGap = 0.05f;    // between letters, in ems: the painted letters nearly touch
constexpr float         kLabelSpace = 0.28f;  // a space, in ems
constexpr unsigned char kLabelLight[3] = {173, 171, 167};
constexpr unsigned char kLabelDark[3] = {63, 63, 81};
struct LabelCopy {
    float dx, dy;
};
constexpr LabelCopy kLabelLightAt = {-1.3f, -0.4f};
constexpr LabelCopy kLabelDarkAt = {1.0f, 0.9f};

// With stretch, the letters are stretched or narrowed to fill the box (less the outline and copies), not only narrowed; cap
// and space are the capitals' height and a space's width in ems, and after[i] is added after letter i (spaces not
// counted), in pixels before the fit; each letter is drawn widen times as wide about its own middle, the spacing kept,
// and letter i (spaces not counted) narrow[i] times as wide as the font has it, where narrow has an entry for it.
void DrawLabel(const std::string &utf8, int x, int y, float box_width, const CaptionBounds &bounds, int alpha, int bright,
               float cap = kLabelCap, bool stretch = false, float space = kLabelSpace, std::span<const float> after = {},
               float widen = 1.0f, std::span<const float> narrow = {}) {
    std::u32string text = Decode(utf8);
    for (char32_t &ch : text) {
        ch = Upper(ch);
    }
    const textwipe::Tuning   &tune = textwipe::Get(textwipe::Kind::Caption);
    alpha = static_cast<int>(static_cast<float>(alpha) * tune.opacity);
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const capfont::Metrics    metrics = capfont::GetMetrics();
    const float               em = cap / std::max(metrics.cap, 0.1f);
    const int                 em_px = std::max(4, static_cast<int>(em * mapping.scale_x + 0.5f));
    const int                 outline_px = std::max(1, static_cast<int>(kLabelOutline * mapping.scale_x + 0.5f));

    // The letters' own width, then narrowed to the box less the outline and the two copies either side.
    const float room = box_width - 2 * kLabelOutline - (kLabelDarkAt.dx - kLabelLightAt.dx);
    auto extra = [&](std::size_t letter) { return letter < after.size() ? after[letter] : 0.0f; };
    auto        own = [&](std::size_t letter) { return letter < narrow.size() ? narrow[letter] : 1.0f; };
    float natural = 0.0f;
    std::size_t letter = 0;
    for (char32_t ch : text) {
        capfont::Glyph glyph;
        if (ch == U' ') {
            natural += em * space + tune.word;
        } else if (capfont::GetGlyph(ch, em_px, glyph)) {
            natural += glyph.width / mapping.scale_x * own(letter) + em * kLabelGap + tune.letter + extra(letter);
            letter++;
        } else {
            natural += em * 0.5f;
        }
    }
    const float squeeze = (stretch || natural > room) && room > 0 && natural > 0 ? room / natural : 1.0f;

    const float left = static_cast<float>(x) + bounds.left + kLabelOutline - kLabelLightAt.dx + tune.dx;
    const float base = static_cast<float>(y) + bounds.top + kLabelOutline + cap - kLabelRaise + tune.dy;

    std::vector<gfx::Vertex2D> edges, lights, darks, fills;
    float                      pen = left;
    letter = 0;
    for (char32_t ch : text) {
        capfont::Glyph fill;
        if (ch == U' ') {
            pen += (em * space + tune.word) * squeeze;
            continue;
        }
        if (!capfont::GetGlyph(ch, em_px, fill)) {
            pen += em * 0.5f * squeeze;
            continue;
        }
        capfont::Glyph edge;
        const bool     has_edge = capfont::GetOutlinedGlyph(ch, em_px, outline_px, edge);
        // A glyph at the letter's place on whole screen pixels, moved a whole number of screen pixels more: the letter
        // is put on the pixel grid once and every copy of it moved the same pixels, so no letter's shadow lands a pixel
        // nearer or further than another's.
        auto place = [&](const capfont::Glyph &glyph, std::vector<gfx::Vertex2D> &out, const unsigned char (&rgb)[3], int px_x,
                         int px_y) {
            if (glyph.width <= 0 || glyph.height <= 0) {
                return;
            }
            const float k = squeeze * own(letter);
            const float width = glyph.width / mapping.scale_x * k * widen;
            const float height = glyph.height / mapping.scale_y * rows;
            const float middle = pen + fill.width / mapping.scale_x * k / 2;
            float       gx = middle + (pen + (glyph.x_offset - fill.x_offset) / mapping.scale_x * k - middle) * widen;
            float       gy = base * rows + glyph.y_offset / mapping.scale_y;
            gx = Snap(gx, mapping.scale_x, mapping.offset_x) + static_cast<float>(px_x) / mapping.scale_x;
            gy = Snap(gy, mapping.scale_y, mapping.offset_y) + static_cast<float>(px_y) / mapping.scale_y;
            Quad(out, glyph, gx, gy, width, height, rgb, alpha);
        };
        // Each shadow is the letter swept from its own place to the shadow's, stamped on every pixel of the way, so it is
        // one solid shape joined to the letter; the outline goes round the whole of it.
        auto sweep = [&](const capfont::Glyph &glyph, std::vector<gfx::Vertex2D> &out, const unsigned char (&rgb)[3], LabelCopy to) {
            const int px_x = static_cast<int>(std::lround(to.dx * mapping.scale_x));
            const int px_y = static_cast<int>(std::lround(to.dy * rows * mapping.scale_y));
            const int steps = std::max({1, std::abs(px_x), std::abs(px_y)});
            for (int step = 1; step <= steps; step++) {
                place(glyph, out, rgb, static_cast<int>(std::lround(static_cast<float>(px_x * step) / steps)),
                      static_cast<int>(std::lround(static_cast<float>(px_y * step) / steps)));
            }
        };
        if (has_edge) {
            place(edge, edges, kOutline, 0, 0);
            sweep(edge, edges, kOutline, kLabelLightAt);
            sweep(edge, edges, kOutline, kLabelDarkAt);
        }
        sweep(fill, lights, kLabelLight, kLabelLightAt);
        sweep(fill, darks, kLabelDark, kLabelDarkAt);
        place(fill, fills, kFill, 0, 0);
        pen += (fill.width / mapping.scale_x * own(letter) + em * kLabelGap + tune.letter + extra(letter)) * squeeze;
        letter++;
    }
    if (fills.empty()) {
        return;
    }
    const gfx::TextureBinding binding = capfont::Binding();
    for (auto *layer : {&edges, &darks, &lights, &fills}) {
        for (gfx::Vertex2D &vertex : *layer) {
            for (int channel = 0; channel < 3; channel++) {
                vertex.color[channel] = static_cast<uint8_t>(std::clamp(vertex.color[channel] * bright / 0x80, 0, 255));
            }
        }
        if (!layer->empty()) {
            textwipe::Draw2D(gfx::Primitive::Quads, *layer, binding, draw2d::SpriteState());
        }
    }
    draw2d::RestoreTestZbuf();
}

// The name alphabet's look, in its sheet's pixels: each character swept half a pixel right and three quarters down in
// bright blue, its cream face stepping in from the blue in three strokes a fifth of a pixel wide, and a pixel of navy
// round the whole.
constexpr int           kAlphaCell = 22;
constexpr float         kAlphaDepthX = 0.5f;
constexpr float         kAlphaDepthY = 0.75f;
constexpr float         kAlphaStroke = 0.2f;
constexpr int           kAlphaStrokes = 3;
constexpr float         kAlphaOutline = 1.0f;
constexpr unsigned char kAlphaFill[3] = {232, 230, 216};
constexpr unsigned char kAlphaBody[3] = {52, 56, 192};
constexpr unsigned char kAlphaRing[3] = {0, 17, 121};

// Where each of the sheet's characters sits in its cell (row, column), fitted to the painting: the box its outline's
// bounds (with a piece, that piece's) are stretched to, in cell pixels. The letters and digits are at one scale, centred
// on the painted ones; the symbols each fitted to their own, mirrored pairs one size, the dots the font's roundness,
// the colon both its dots the one.
struct AlphaPlace {
    int      row, col;
    char32_t ch;
    int      part;
    float    x, y, w, h;
};

constexpr AlphaPlace kAlphabet[] = {
    {0, 0, U'A',  -1, 2.167f, 3.000f,  17.167f, 15.167f},
    {0, 1, U'B',  -1, 2.833f, 3.167f,  16.833f, 14.833f},
    {0, 2, U'C',  -1, 3.000f, 3.167f,  17.500f, 14.833f},
    {0, 3, U'D',  -1, 2.333f, 3.000f,  16.833f, 15.167f},
    {0, 4, U'E',  -1, 2.500f, 3.167f,  17.500f, 15.000f},
    {0, 5, U'F',  -1, 3.000f, 3.000f,  16.500f, 15.167f},
    {0, 6, U'G',  -1, 2.333f, 3.000f,  18.000f, 15.167f},
    {0, 7, U'H',  -1, 2.167f, 3.000f,  17.167f, 15.167f},
    {0, 8, U'I',  -1, 7.333f, 3.000f,  7.000f,  15.333f},
    {0, 9, U'J',  -1, 1.167f, 3.000f,  17.167f, 15.167f},
    {1, 0, U'K',  -1, 1.667f, 3.500f,  18.000f, 15.167f},
    {1, 1, U'L',  -1, 2.667f, 3.500f,  17.000f, 15.167f},
    {1, 2, U'M',  -1, 2.000f, 3.833f,  17.667f, 14.667f},
    {1, 3, U'N',  -1, 2.000f, 3.667f,  17.667f, 15.000f},
    {1, 4, U'O',  -1, 3.000f, 3.667f,  16.667f, 14.833f},
    {1, 5, U'P',  -1, 4.000f, 3.000f,  16.500f, 15.333f},
    {1, 6, U'Q',  -1, 2.000f, 3.500f,  17.333f, 15.167f},
    {1, 7, U'R',  -1, 2.000f, 3.000f,  16.500f, 15.333f},
    {1, 8, U'S',  -1, 2.000f, 3.667f,  16.500f, 15.000f},
    {1, 9, U'T',  -1, 2.333f, 3.667f,  16.667f, 14.833f},
    {2, 0, U'U',  -1, 3.000f, 3.667f,  17.500f, 14.833f},
    {2, 1, U'V',  -1, 3.000f, 3.667f,  16.667f, 15.000f},
    {2, 2, U'W',  -1, 3.000f, 3.167f,  17.500f, 15.000f},
    {2, 3, U'X',  -1, 3.333f, 3.167f,  16.833f, 14.833f},
    {2, 4, U'Y',  -1, 3.333f, 3.167f,  15.667f, 15.000f},
    {2, 5, U'Z',  -1, 2.667f, 3.000f,  17.333f, 15.167f},
    {2, 6, U'a',  -1, 3.000f, 7.167f,  16.333f, 11.000f},
    {2, 7, U'b',  -1, 2.667f, 3.167f,  16.000f, 15.000f},
    {2, 8, U'c',  -1, 2.000f, 7.000f,  16.333f, 11.333f},
    {2, 9, U'd',  -1, 3.000f, 3.167f,  16.500f, 15.000f},
    {3, 0, U'e',  -1, 3.000f, 7.167f,  16.667f, 11.000f},
    {3, 1, U'f',  -1, 3.333f, 3.167f,  16.667f, 15.000f},
    {3, 2, U'g',  -1, 2.333f, 6.333f,  16.833f, 12.667f},
    {3, 3, U'h',  -1, 3.167f, 3.667f,  17.167f, 14.833f},
    {3, 4, U'i',  -1, 8.000f, 3.667f,  6.667f,  15.000f},
    {3, 5, U'j',  -1, 4.333f, 4.000f,  16.000f, 16.333f},
    {3, 6, U'k',  -1, 3.000f, 3.000f,  16.667f, 15.167f},
    {3, 7, U'l',  -1, 8.333f, 3.333f,  5.833f,  14.667f},
    {3, 8, U'm',  -1, 2.000f, 7.000f,  17.333f, 11.167f},
    {3, 9, U'n',  -1, 2.000f, 7.167f,  17.333f, 10.833f},
    {4, 0, U'o',  -1, 2.833f, 7.000f,  16.833f, 11.167f},
    {4, 1, U'p',  -1, 3.000f, 7.000f,  16.500f, 12.167f},
    {4, 2, U'q',  -1, 1.667f, 7.000f,  18.000f, 12.167f},
    {4, 3, U'r',  -1, 2.000f, 7.833f,  16.333f, 10.500f},
    {4, 4, U's',  -1, 2.167f, 7.000f,  17.167f, 11.333f},
    {4, 5, U't',  -1, 3.000f, 3.500f,  16.667f, 14.333f},
    {4, 6, U'u',  -1, 2.500f, 7.333f,  16.500f, 10.667f},
    {4, 7, U'v',  -1, 2.500f, 6.833f,  16.500f, 11.500f},
    {4, 8, U'w',  -1, 2.333f, 7.167f,  17.000f, 11.000f},
    {4, 9, U'x',  -1, 2.833f, 7.333f,  16.833f, 10.667f},
    {5, 0, U'y',  -1, 2.667f, 7.667f,  17.333f, 12.000f},
    {5, 1, U'z',  -1, 2.500f, 7.000f,  17.500f, 11.333f},
    {5, 2, U'\'', -1, 2.076f, 1.908f,  8.241f,  6.594f },
    {5, 3, U'=',  0,  2.886f, 7.325f,  17.257f, 3.440f },
    {5, 3, U'=',  1,  3.062f, 13.476f, 17.201f, 3.158f },
    {5, 4, U'"',  -1, 2.754f, 1.000f,  10.142f, 7.356f },
    {5, 5, U'!',  -1, 6.667f, 3.333f,  10.000f, 14.500f},
    {5, 6, U'?',  -1, 3.000f, 2.333f,  16.333f, 15.500f},
    {5, 7, U'#',  -1, 1.000f, 2.797f,  18.500f, 16.453f},
    {5, 8, U'&',  -1, 1.000f, 2.874f,  18.500f, 15.591f},
    {5, 9, U'+',  -1, 2.026f, 1.000f,  15.999f, 18.250f},
    {6, 0, U'-',  -1, 2.532f, 9.231f,  16.968f, 2.584f },
    {6, 1, U'*',  -1, 2.801f, 1.921f,  16.693f, 17.329f},
    {6, 2, U'/',  -1, 2.140f, 2.945f,  17.360f, 16.305f},
    {6, 3, U'%',  -1, 1.000f, 2.153f,  18.500f, 17.097f},
    {6, 4, U'~',  -1, 1.000f, 9.173f,  19.500f, 4.904f },
    {6, 5, U'(',  -1, 7.685f, 1.000f,  11.815f, 18.250f},
    {6, 6, U')',  -1, 1.035f, 1.000f,  11.815f, 18.250f},
    {6, 7, U'@',  -1, 1.000f, 1.000f,  18.500f, 17.314f},
    {6, 9, U'|',  -1, 8.112f, 2.023f,  4.908f,  17.045f},
    {7, 0, U'<',  -1, 1.000f, 3.417f,  18.500f, 15.833f},
    {7, 1, U'>',  -1, 1.000f, 3.417f,  18.500f, 15.833f},
    {7, 2, U'{',  -1, 4.667f, 3.167f,  9.333f,  15.833f},
    {7, 3, U'}',  -1, 6.167f, 3.167f,  9.167f,  15.833f},
    {7, 4, U'「', -1, 7.091f, 1.000f,  9.749f,  18.250f},
    {7, 5, U'」', -1, 5.730f, 1.000f,  9.749f,  18.250f},
    {7, 6, U'『', -1, 7.058f, 1.000f,  9.984f,  18.250f},
    {7, 7, U'』', -1, 5.021f, 1.000f,  9.984f,  18.250f},
    {7, 8, U'[',  -1, 5.458f, 1.000f,  11.123f, 18.250f},
    {7, 9, U']',  -1, 4.018f, 1.000f,  11.123f, 18.250f},
    {8, 0, U'0',  -1, 1.667f, 4.167f,  17.167f, 15.000f},
    {8, 1, U'1',  -1, 6.333f, 3.667f,  9.667f,  14.833f},
    {8, 2, U'2',  -1, 1.667f, 3.333f,  17.167f, 14.667f},
    {8, 3, U'3',  -1, 2.333f, 3.000f,  16.833f, 15.167f},
    {8, 4, U'4',  -1, 2.667f, 3.167f,  17.000f, 15.000f},
    {8, 5, U'5',  -1, 1.833f, 3.500f,  16.833f, 15.167f},
    {8, 6, U'6',  -1, 2.667f, 3.000f,  17.000f, 15.167f},
    {8, 7, U'7',  -1, 2.333f, 3.167f,  16.833f, 14.833f},
    {8, 8, U'8',  -1, 2.667f, 3.000f,  16.000f, 15.167f},
    {8, 9, U'9',  -1, 1.667f, 3.167f,  17.000f, 15.000f},
    {9, 0, U'・', -1, 8.237f, 8.039f,  4.709f,  4.546f },
    {9, 1, U'.',  -1, 5.959f, 14.704f, 4.709f,  4.546f },
    {9, 2, U'。', -1, 4.868f, 13.596f, 5.805f,  5.654f },
    {9, 3, U':',  0,  8.274f, 4.502f,  6.187f,  5.241f },
    {9, 3, U':',  0,  8.310f, 13.053f, 6.187f,  5.241f },
    {9, 4, U',',  -1, 4.799f, 14.704f, 5.500f,  5.546f },
    {9, 5, U'.',  -1, 4.865f, 14.704f, 4.709f,  4.546f },
    {9, 6, U'$',  -1, 1.345f, 1.614f,  18.155f, 17.636f},
};

// The dungeon-entry list's floor labels ("Floor", "Territory"...), painted as the game paints them: the bold letters in
// warm yellow, a navy outline round the word swept down and right for depth, the letters nearly touching (a gap of
// kFloorLabelGap font units, a space kFloorLabelSpace) and the word stretched to the painted one's box.
constexpr float         kFloorLabelGap = 30.0f;
constexpr float         kFloorLabelSpace = 140.0f;
constexpr float         kFloorLabelDepthX = 1.0f;
constexpr float         kFloorLabelDepthY = 0.8f;
constexpr float         kFloorLabelOutline = 1.0f;
constexpr unsigned char kFloorLabelFill[3] = {254, 212, 84};
constexpr unsigned char kFloorLabelRing[3] = {44, 44, 106};

// The weapon menu's painted button lettering (wepstatus), measured on it: cream letters of the narrower set in a faint
// dark brown glow all round them, a little lower than they are, darkest against them and fading with distance.
constexpr unsigned char kButtonFill[3] = {255, 244, 222};
constexpr unsigned char kButtonShade[3] = {58, 30, 18};
constexpr float         kButtonShadowX = 0.2f;
constexpr float         kButtonShadowY = 0.7f;
constexpr float         kButtonShadowGrow = 0.2f;
constexpr float         kButtonShadowSoft = 4.0f;
constexpr float         kButtonShadowAlpha = 0.45f;
constexpr float         kButtonTighten = 0.92f; // the letters drawn this much closer to their word's middle
// Pressed (EQUIP while a weapon is being chosen), the glow is light: cream, half strength against the letters, fading over
// six pixels, with a band of it through the word's middle.
constexpr unsigned char kButtonGlow[3] = {255, 244, 222};
constexpr float         kButtonGlowAlpha = 0.5f;
constexpr float         kButtonGlowSoft = 6.0f;
constexpr float         kButtonBandAlpha = 0.5f;
constexpr float         kButtonBandY = 13.0f;
constexpr float         kButtonBandCore = 2.25f; // full strength this far above and below its line, from the first letter's
                                                 // middle to the last's
constexpr float         kButtonBandFade = 12.0f; // then fading over this

// Where the painted buttons have each letter of their English words, fitted to the painting with the font's letters:
// its box in the 96 by 23 button, each letter the one width wherever it is but for its word's squeeze.
struct ButtonLetter {
    std::string_view word;
    char32_t         ch;
    float            left, top, right, bottom;
};

constexpr ButtonLetter kButtonLetters[] = {
    {"EQUIP",        U'E', 19.45f, 4.27f,  30.32f, 19.82f},
    {"EQUIP",        U'Q', 32.00f, 3.72f,  43.67f, 19.42f},
    {"EQUIP",        U'U', 45.79f, 4.50f,  57.55f, 19.60f},
    {"EQUIP",        U'I', 59.92f, 4.23f,  63.52f, 19.72f},
    {"EQUIP",        U'P', 67.41f, 3.54f,  78.34f, 19.43f},
    {"CUSTOMIZE",    U'C', 7.14f,  4.15f,  15.74f, 20.01f},
    {"CUSTOMIZE",    U'U', 17.63f, 4.20f,  26.60f, 19.94f},
    {"CUSTOMIZE",    U'S', 27.42f, 3.57f,  35.87f, 19.63f}, // as near the T as painted
    {"CUSTOMIZE",    U'T', 36.84f, 3.94f,  44.55f, 19.73f},
    {"CUSTOMIZE",    U'O', 44.07f, 3.88f,  52.97f, 19.73f},
    {"CUSTOMIZE",    U'M', 53.88f, 4.08f,  63.00f, 19.59f},
    {"CUSTOMIZE",    U'I', 65.61f, 3.75f,  68.35f, 19.88f},
    {"CUSTOMIZE",    U'Z', 70.08f, 3.81f,  79.13f, 19.94f},
    {"CUSTOMIZE",    U'E', 80.52f, 4.02f,  88.81f, 20.22f},
    {"REPAIR",       U'R', 15.40f, 2.20f,  25.91f, 18.41f},
    {"REPAIR",       U'E', 27.53f, 3.03f,  37.96f, 19.13f},
    {"REPAIR",       U'P', 39.99f, 2.15f,  50.48f, 18.59f},
    {"REPAIR",       U'A', 49.34f, 2.55f,  60.21f, 18.55f},
    {"REPAIR",       U'I', 63.43f, 2.71f,  66.89f, 18.74f},
    {"REPAIR",       U'R', 69.33f, 2.27f,  79.84f, 18.48f},
    {"UPGRADE",      U'U', 6.63f,  3.83f,  17.97f, 19.03f},
    {"UPGRADE",      U'P', 19.36f, 2.71f,  29.90f, 18.70f},
    {"UPGRADE",      U'G', 29.93f, 3.32f,  41.48f, 18.82f},
    {"UPGRADE",      U'R', 42.44f, 2.91f,  52.99f, 18.68f},
    {"UPGRADE",      U'A', 54.48f, 3.22f,  65.40f, 18.77f},
    {"UPGRADE",      U'D', 67.03f, 3.03f,  78.07f, 18.75f},
    {"UPGRADE",      U'E', 78.84f, 3.60f,  89.32f, 19.26f},
    // STATUS BREAK: its letters drawn a little wide and its words a little apart, as painted.
    {"STATUS BREAK", U'S', 5.38f,  3.53f,  12.41f, 19.80f},
    {"STATUS BREAK", U'T', 13.20f, 3.72f,  19.64f, 19.71f},
    {"STATUS BREAK", U'A', 18.71f, 3.22f,  25.88f, 19.53f},
    {"STATUS BREAK", U'T', 26.15f, 3.62f,  32.60f, 19.61f},
    {"STATUS BREAK", U'U', 31.92f, 4.27f,  39.36f, 20.21f},
    {"STATUS BREAK", U'S', 39.56f, 3.36f,  46.59f, 19.63f},
    {"STATUS BREAK", U'B', 50.41f, 3.63f,  57.55f, 19.70f},
    {"STATUS BREAK", U'R', 58.45f, 2.93f,  65.40f, 19.46f},
    {"STATUS BREAK", U'E', 66.58f, 3.84f,  73.48f, 20.25f},
    {"STATUS BREAK", U'A', 74.34f, 3.23f,  81.51f, 19.53f},
    {"STATUS BREAK", U'K', 82.97f, 3.51f,  90.37f, 19.76f},
    {"BUILD-UP",     U'B', 9.01f,  3.55f,  19.23f, 18.90f},
    {"BUILD-UP",     U'U', 20.26f, 4.02f,  30.94f, 19.24f},
    {"BUILD-UP",     U'I', 32.08f, 3.39f,  35.35f, 18.99f},
    {"BUILD-UP",     U'L', 36.98f, 3.47f,  46.51f, 19.25f},
    {"BUILD-UP",     U'D', 48.36f, 3.27f,  58.76f, 19.00f},
    {"BUILD-UP",     U'-', 60.25f, 12.25f, 64.75f, 13.75f},
    {"BUILD-UP",     U'U', 66.25f, 4.02f,  76.93f, 19.24f},
    {"BUILD-UP",     U'P', 78.35f, 2.85f,  88.28f, 18.86f},
    {"ATTRIBUTE",    U'A', 4.01f,  3.05f,  13.16f, 18.39f},
    {"ATTRIBUTE",    U'T', 13.52f, 3.39f,  21.69f, 18.43f},
    {"ATTRIBUTE",    U'T', 23.91f, 3.39f,  32.08f, 18.43f},
    {"ATTRIBUTE",    U'R', 32.62f, 2.84f,  41.46f, 18.38f},
    {"ATTRIBUTE",    U'I', 44.38f, 3.14f,  47.29f, 18.51f},
    {"ATTRIBUTE",    U'B', 49.79f, 3.33f,  58.88f, 18.44f},
    {"ATTRIBUTE",    U'U', 60.12f, 3.77f,  69.62f, 18.76f},
    {"ATTRIBUTE",    U'T', 71.42f, 3.38f,  79.60f, 18.42f},
    {"ATTRIBUTE",    U'E', 80.32f, 3.57f,  89.10f, 19.00f},
    // The yes/no and dump plates (btstatus2), measured on them.
    {"YES",          U'Y', 26.27f, 3.43f,  37.61f, 18.95f},
    {"YES",          U'E', 40.96f, 3.57f,  53.71f, 19.98f},
    {"YES",          U'S', 56.27f, 3.07f,  69.16f, 19.35f},
    {"NO",           U'N', 32.96f, 3.58f,  47.35f, 18.82f},
    {"NO",           U'O', 49.05f, 3.47f,  62.19f, 19.24f},
    {"DUMP",         U'D', 16.89f, 2.48f,  30.47f, 18.46f},
    {"DUMP",         U'U', 32.00f, 3.22f,  45.92f, 18.73f},
    {"DUMP",         U'M', 46.90f, 3.39f,  60.97f, 18.40f},
    {"DUMP",         U'P', 64.31f, 2.08f,  77.35f, 18.44f},
};

} // namespace

bool DrawButtonWord(std::string_view word, float x, float y, int bright, int alpha, bool pressed) {
    if (!FontReady() || !capfont::VariantLoaded()) {
        return false;
    }
    auto first = std::find_if(std::begin(kButtonLetters), std::end(kButtonLetters),
                              [&](const ButtonLetter &letter) { return letter.word == word; });
    if (first == std::end(kButtonLetters)) {
        return false;
    }
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const float               kx = mapping.scale_x, ky = mapping.scale_y * rows, k = std::sqrt(kx * ky);
    capfont::Paint            paint = {0.0f, 0.0f, 0.0f, 0, 0.0f, {}, {}, {}};
    for (unsigned char (*colour)[3] : {&paint.fill, &paint.body, &paint.ring}) {
        std::copy_n(kButtonFill, 3, *colour);
    }
    std::copy_n(kButtonShade, 3, paint.shade);
    paint.shadow_x = kButtonShadowX * kx;
    paint.shadow_y = kButtonShadowY * ky;
    paint.shadow_grow = kButtonShadowGrow * k;
    paint.shadow_soft = kButtonShadowSoft * k;
    paint.shadow_alpha = kButtonShadowAlpha;
    paint.slot = 1;
    if (pressed) {
        std::copy_n(kButtonGlow, 3, paint.shade);
        paint.shadow_x = paint.shadow_y = paint.shadow_grow = 0.0f;
        paint.shadow_soft = kButtonGlowSoft * k;
        paint.shadow_alpha = kButtonGlowAlpha;
        paint.band_alpha = kButtonBandAlpha;
        paint.band_y = kButtonBandY * ky;
        paint.band_core = kButtonBandCore * ky;
        paint.band_fade = kButtonBandFade * ky;
    }
    // The word's middle: halfway between its first and last letters' middles.
    auto last = first;
    while (std::next(last) != std::end(kButtonLetters) && std::next(last)->word == word) {
        ++last;
    }
    const float middle = (first->left + first->right + last->left + last->right) / 4;
    const u_char               tint = static_cast<u_char>(std::clamp(bright, 0, 255)), a = static_cast<u_char>(std::clamp(alpha, 0, 0x80));
    std::vector<gfx::Vertex2D> quads;
    auto                       quad = [&](const capfont::Glyph &glyph, float left, float top) {
        const float right = left + glyph.width / mapping.scale_x, bottom = top + glyph.height / mapping.scale_y;
        quads.push_back(draw2d::Vertex(left, top, 0.0f, glyph.u0, glyph.v0, tint, tint, tint, a));
        quads.push_back(draw2d::Vertex(right, top, 0.0f, glyph.u1, glyph.v0, tint, tint, tint, a));
        quads.push_back(draw2d::Vertex(right, bottom, 0.0f, glyph.u1, glyph.v1, tint, tint, tint, a));
        quads.push_back(draw2d::Vertex(left, bottom, 0.0f, glyph.u0, glyph.v1, tint, tint, tint, a));
    };

    // Where each letter's box starts, to the screen's pixel, and how big it is in pixels.
    struct Placed {
        char32_t ch;
        float    left, top, w, h;
    };

    std::vector<Placed> placed;
    for (auto letter = first; letter != std::end(kButtonLetters) && letter->word == word; ++letter) {
        // The letter's outline stretched to the painted letter's box.
        const float shift = (middle - (letter->left + letter->right) / 2) * (1.0f - kButtonTighten);
        placed.push_back({letter->ch, Snap(x + letter->left + shift, mapping.scale_x, mapping.offset_x),
                          Snap((y + letter->top) * rows, mapping.scale_y, mapping.offset_y), (letter->right - letter->left) * kx,
                          (letter->bottom - letter->top) * ky});
    }
    // The word's shadow as one, from all its letters together, so where they meet it does not darken twice; then the
    // letters over it.
    const float                        origin_x = Snap(x, mapping.scale_x, mapping.offset_x);
    const float                        origin_y = Snap(y * rows, mapping.scale_y, mapping.offset_y);
    std::vector<capfont::PlacedLetter> together;
    for (const Placed &letter : placed) {
        together.push_back({letter.ch, (letter.left - origin_x) * mapping.scale_x, (letter.top - origin_y) * mapping.scale_y, letter.w, letter.h});
    }
    if (pressed && together.size() > 1) {
        paint.band_left = together.front().x + together.front().w / 2;
        paint.band_right = together.back().x + together.back().w / 2;
    }
    capfont::Paint shadow = paint;
    shadow.shadow_only = true;
    capfont::Paint face = {0.0f, 0.0f, 0.0f, 0, 0.0f, {}, {}, {}};
    std::copy_n(paint.fill, 3, face.fill);
    std::copy_n(paint.body, 3, face.body);
    std::copy_n(paint.ring, 3, face.ring);
    face.slot = paint.slot;
    // Made again, once, if the atlas started over while they were fetched (that would leave the first ones stale).
    for (int attempt = 0; attempt < 2; attempt++) {
        const unsigned generation = capfont::Generation();
        quads.clear();
        capfont::Glyph glyph;
        if (!capfont::GetPaintedWord(together, shadow, glyph)) {
            return false;
        }
        quad(glyph, origin_x + glyph.x_offset / mapping.scale_x, origin_y + glyph.y_offset / mapping.scale_y);
        for (const Placed &letter : placed) {
            if (!capfont::GetPaintedGlyph(letter.ch, -1, letter.w, letter.h, face, glyph)) {
                return false;
            }
            quad(glyph, letter.left + glyph.x_offset / mapping.scale_x, letter.top + glyph.y_offset / mapping.scale_y);
        }
        if (generation == capfont::Generation()) {
            break;
        }
    }
    textwipe::Draw2D(gfx::Primitive::Quads, quads, capfont::Binding(), draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
    return true;
}

bool DrawFloorLabel(std::string_view utf8, float left, float top, float right, float bottom, float clip_top, float clip_bottom,
                    int alpha, std::span<const FloorAdjust> adjust, bool by_capitals, float squeeze) {
    if (utf8.empty() || right <= left || bottom <= top || !FontReady()) {
        return false;
    }

    // The letters set by their own ink, then the word stretched to the box.
    struct Letter {
        char32_t ch;
        float    pen;
        float    bounds[4];
    };

    std::vector<Letter> letters;
    float               pen = 0.0f, low = 1e9f, high = -1e9f;
    for (char32_t ch : Decode(utf8)) {
        if (ch == U' ') {
            pen += kFloorLabelSpace;
            continue;
        }
        Letter letter = {ch, pen, {}};
        if (!capfont::GetGlyphBounds(ch, letter.bounds)) {
            return false;
        }
        low = std::min(low, letter.bounds[1]);
        high = std::max(high, letter.bounds[3]);
        pen += letter.bounds[2] - letter.bounds[0] + kFloorLabelGap;
        letters.push_back(letter);
    }
    if (letters.empty()) {
        return false;
    }
    // Across, the letters' ink fills left to right; down, their ink fills top to bottom, or with by_capitals the font's
    // H stands from top to bottom (the capitals' top to the baseline), a descender hanging below as the font has it.
    float h_box[4];
    if (by_capitals) {
        if (!capfont::GetGlyphBounds(U'H', h_box)) {
            return false;
        }
        high = h_box[3];
        low = h_box[1];
    }
    const float sy = (bottom - top) / (high - low);
    float       sx = (right - left) / (pen - kFloorLabelGap);
    if (squeeze > 0.0f) {
        sx = sy * squeeze;
    }

    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const float               kx = mapping.scale_x, ky = mapping.scale_y * rows, k = std::sqrt(kx * ky);
    capfont::Paint            outlined = {kFloorLabelDepthX * kx, kFloorLabelDepthY * ky, 0.0f, 0, kFloorLabelOutline * k, {}, {}, {}};
    capfont::Paint            face = {0.0f, 0.0f, 0.0f, 0, 0.0f, {}, {}, {}};
    std::copy_n(kFloorLabelFill, 3, outlined.fill);
    std::copy_n(kFloorLabelRing, 3, outlined.body);
    std::copy_n(kFloorLabelRing, 3, outlined.ring);
    for (unsigned char (*colour)[3] : {&face.fill, &face.body, &face.ring}) {
        std::copy_n(kFloorLabelFill, 3, *colour);
    }
    const u_char               a = static_cast<u_char>(std::clamp(alpha, 0, 0x80));
    std::vector<gfx::Vertex2D> quads;
    // Every letter outlined, then every face over all the outlines, so the word is outlined as one.
    // Made again, once, if the atlas started over while they were fetched (that would leave the first ones stale).
    for (int attempt = 0; attempt < 2; attempt++) {
        const unsigned generation = capfont::Generation();
        quads.clear();
        for (const capfont::Paint *paint : {&outlined, &face}) {
            float shift = 0.0f;
            for (std::size_t i = 0; i < letters.size(); i++) {
                const Letter &letter = letters[i];
                float         widen = 0.0f, raise = 0.0f;
                for (const FloorAdjust &fix : adjust) {
                    if (fix.letter == static_cast<int>(i)) {
                        shift += fix.move;
                        widen += fix.widen;
                        raise += fix.raise;
                    }
                }
                capfont::Glyph glyph;
                const float    w = (letter.bounds[2] - letter.bounds[0]) * sx + widen, h = (letter.bounds[3] - letter.bounds[1]) * sy;
                if (!capfont::GetPaintedGlyph(letter.ch, -1, w * kx, h * ky, *paint, glyph)) {
                    return false;
                }
                const float x0 = Snap(left + letter.pen * sx + shift, mapping.scale_x, mapping.offset_x) + glyph.x_offset / mapping.scale_x;
                shift += widen; // the letters after a widened one move along
                const float y0 = Snap((top + (high - letter.bounds[3]) * sy - raise) * rows, mapping.scale_y, mapping.offset_y) +
                                 glyph.y_offset / mapping.scale_y;
                const float x1 = x0 + glyph.width / mapping.scale_x;
                float       y1 = y0 + glyph.height / mapping.scale_y;
                // Cut to the rows the list shows.
                float       v0 = glyph.v0, v1 = glyph.v1, from = y0;
                const float cut_top = clip_top * rows, cut_bottom = clip_bottom * rows;
                if (y1 <= cut_top || from >= cut_bottom) {
                    continue;
                }
                if (from < cut_top) {
                    v0 += (glyph.v1 - glyph.v0) * (cut_top - y0) / (y1 - y0);
                    from = cut_top;
                }
                if (y1 > cut_bottom) {
                    v1 -= (glyph.v1 - glyph.v0) * (y1 - cut_bottom) / (y1 - y0);
                    y1 = cut_bottom;
                }
                quads.push_back(draw2d::Vertex(x0, from, 0.0f, glyph.u0, v0, 0x80, 0x80, 0x80, a));
                quads.push_back(draw2d::Vertex(x1, from, 0.0f, glyph.u1, v0, 0x80, 0x80, 0x80, a));
                quads.push_back(draw2d::Vertex(x1, y1, 0.0f, glyph.u1, v1, 0x80, 0x80, 0x80, a));
                quads.push_back(draw2d::Vertex(x0, y1, 0.0f, glyph.u0, v1, 0x80, 0x80, 0x80, a));
            }
        }
        if (generation == capfont::Generation()) {
            break;
        }
    }
    if (!quads.empty()) {
        textwipe::Draw2D(gfx::Primitive::Quads, quads, capfont::Binding(), draw2d::SpriteState());
        draw2d::RestoreTestZbuf();
    }
    return true;
}

bool DrawAlphabetCell(int u, int v, int texel_width, int texel_height, float x, float y, float width, float height, int r,
                      int g, int b, int alpha) {
    if (u % kAlphaCell != 0 || v % kAlphaCell != 0 || texel_width != kAlphaCell || texel_height < kAlphaCell - 1 ||
        texel_height > kAlphaCell + 1 || width <= 0 || height <= 0 || !FontReady()) {
        return false;
    }
    const int row = v / kAlphaCell, col = u / kAlphaCell;
    auto      cell = std::find_if(std::begin(kAlphabet), std::end(kAlphabet), [&](const AlphaPlace &place) {
        return place.row == row && place.col == col;
    });
    if (cell == std::end(kAlphabet)) {
        return false;
    }
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    // Screen pixels to a cell pixel, across and down.
    const float    kx = mapping.scale_x * width / static_cast<float>(texel_width);
    const float    ky = mapping.scale_y * rows * height / static_cast<float>(texel_height);
    const float    k = std::sqrt(kx * ky);
    capfont::Paint paint = {kAlphaDepthX * kx, kAlphaDepthY * ky, kAlphaStroke * k, kAlphaStrokes, kAlphaOutline * k, {}, {}, {}};
    std::copy_n(kAlphaFill, 3, paint.fill);
    std::copy_n(kAlphaBody, 3, paint.body);
    std::copy_n(kAlphaRing, 3, paint.ring);
    const u_char               red = static_cast<u_char>(std::clamp(r, 0, 255)), green = static_cast<u_char>(std::clamp(g, 0, 255));
    const u_char               blue = static_cast<u_char>(std::clamp(b, 0, 255)), a = static_cast<u_char>(std::clamp(alpha, 0, 0x80));
    std::vector<gfx::Vertex2D> quads;
    // Made again, once, if the atlas started over while they were fetched (that would leave the first ones stale).
    for (int attempt = 0; attempt < 2; attempt++) {
        const unsigned generation = capfont::Generation();
        quads.clear();
        for (auto place = cell; place != std::end(kAlphabet) && place->row == row && place->col == col; ++place) {
            capfont::Glyph glyph;
            if (!capfont::GetPaintedGlyph(place->ch, place->part, place->w * kx, place->h * ky, paint, glyph)) {
                return false;
            }
            const float left = Snap(x + place->x * width / static_cast<float>(texel_width), mapping.scale_x, mapping.offset_x) +
                               glyph.x_offset / mapping.scale_x;
            const float top = Snap((y + place->y * height / static_cast<float>(texel_height)) * rows, mapping.scale_y, mapping.offset_y) +
                              glyph.y_offset / mapping.scale_y;
            const float right = left + glyph.width / mapping.scale_x, bottom = top + glyph.height / mapping.scale_y;
            quads.push_back(draw2d::Vertex(left, top, 0.0f, glyph.u0, glyph.v0, red, green, blue, a));
            quads.push_back(draw2d::Vertex(right, top, 0.0f, glyph.u1, glyph.v0, red, green, blue, a));
            quads.push_back(draw2d::Vertex(right, bottom, 0.0f, glyph.u1, glyph.v1, red, green, blue, a));
            quads.push_back(draw2d::Vertex(left, bottom, 0.0f, glyph.u0, glyph.v1, red, green, blue, a));
        }
        if (generation == capfont::Generation()) {
            break;
        }
    }
    textwipe::Draw2D(gfx::Primitive::Quads, quads, capfont::Binding(), draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
    return true;
}

namespace {

// The floor-select board's painted names, measured on them (the font's letters laid over each painted one and their
// widths and spacing fitted, to a fraction of a pixel, across all of them): caps 17.7 pixels tall, strokes a little
// heavier than the font's, a pixel of navy round them, each letter its own width wherever it is, 0.73 pixels between
// letters and 7.12 more between words, the apostrophe close between its neighbours, its top level with the next letter's.
constexpr float         kBoardCap = 17.7f;
constexpr float         kBoardWeight = 0.1f; // added to each side of a stroke
constexpr float         kBoardOutline = 1.0f;
constexpr float         kBoardGap = 0.73f;
constexpr float         kBoardSpace = 7.12f;
constexpr float         kBoardLead = 1.00f;  // before an apostrophe
constexpr float         kBoardTuck = -4.79f; // after an apostrophe
constexpr unsigned char kBoardFill[3] = {228, 226, 131};
constexpr unsigned char kBoardRing[3] = {16, 16, 58};

// How wide a letter's fill is painted, unsqueezed.
float BoardWidth(char32_t ch) {
    switch (ch) {
        case U'\'':
            return 5.95f;
        case U'A':
            return 14.21f;
        case U'B':
            return 14.08f;
        case U'C':
            return 13.61f;
        case U'D':
            return 12.76f;
        case U'E':
            return 12.76f;
        case U'F':
            return 12.77f;
        case U'G':
            return 12.08f;
        case U'H':
            return 14.20f;
        case U'I':
            return 5.75f;
        case U'L':
            return 13.32f;
        case U'M':
            return 14.16f;
        case U'N':
            return 13.55f;
        case U'O':
            return 12.70f;
        case U'P':
            return 12.78f;
        case U'R':
            return 13.06f;
        case U'S':
            return 12.49f;
        case U'T':
            return 10.97f;
        case U'U':
            return 13.01f;
        case U'V':
            return 16.02f;
        case U'W':
            return 14.16f;
        case U'Y':
            return 11.04f;
        default:
            return 13.19f;
    }
}

// How much wider than its place a letter is drawn, into the gaps on its left and right: the painted E, S and O nearly
// touch their neighbours, the painted T, D and G reach back toward the letter before them, the painted R further both
// ways, the painted N, L and Y on toward the letter after them, the painted U and F both ways, and the painted
// apostrophe sits clear of the letter after it.
struct Spread {
    float left, right;
};

Spread BoardSpread(char32_t ch) {
    switch (ch) {
        case U'E':
        case U'S':
        case U'O':
            return {0.8f, 0.8f};
        case U'D':
        case U'T':
            return {1.6f, 0.0f};
        case U'G':
            return {2.4f, 0.0f};
        case U'R':
            return {1.0f, 1.6f};
        case U'N':
        case U'L':
            return {0.0f, 1.0f};
        case U'Y':
            return {0.0f, 2.4f};
        case U'F':
            return {0.6f, 0.8f};
        case U'U':
            return {0.6f, 1.0f};
        case U'\'':
            return {1.5f, -1.5f};
        default:
            return {0.0f, 0.0f};
    }
}

} // namespace

bool DrawBoardName(std::string_view utf8, float left_x, float cap_top, float squeeze, std::span<const BoardAdjust> adjust,
                   float clip_top, float clip_bottom, int alpha) {
    if (!FontReady() || !capfont::VariantLoaded() || utf8.empty()) {
        return false;
    }
    struct Variant {
        Variant() { capfont::UseFontSlot(1); }

        ~Variant() { capfont::UseFontSlot(0); }
    } variant;
    const std::u32string      text = Decode(utf8);
    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const capfont::Metrics    metrics = capfont::GetMetrics();
    const float               em = kBoardCap / std::max(metrics.cap, 0.1f);
    const int                 em_px = std::max(4, static_cast<int>(em * mapping.scale_x + 0.5f));
    // The capitals' tops at cap_top, wherever the font puts its baseline.
    const float               base = cap_top + kBoardCap * (1.0f - metrics.cap_drop / std::max(metrics.cap, 0.1f));
    // Which of the name's letters ch letter i is.
    auto nth_of = [&](std::size_t i) {
        return static_cast<int>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(i), text[i]));
    };
    // How far letter i reaches past its place either side (the same for every one of that letter), and how far it is
    // moved and moves the letters after it, in this name.
    auto changes_of = [&](std::size_t i, Spread &spread, float &advance, float &move) {
        spread = BoardSpread(text[i]);
        advance = 0.0f;
        move = 0.0f;
        for (const BoardAdjust &change : adjust) {
            if (change.ch == text[i]) {
                spread.left += change.left;
                spread.right += change.right;
                if (change.nth < 0 || change.nth == nth_of(i)) {
                    advance += change.advance;
                    move += change.move;
                }
            }
        }
    };
    // The stretch that makes letter i as wide as its fill is painted.
    auto stretch_of = [&](std::size_t i) {
        const char32_t ch = text[i];
        capfont::Glyph natural;
        if (!capfont::GetGlyph(ch, em_px, natural) || natural.width <= 0) {
            return 0.0f;
        }
        Spread spread;
        float  advance, move;
        changes_of(i, spread, advance, move);
        return std::clamp(((BoardWidth(ch) + spread.left + spread.right) * squeeze - 2 * kBoardWeight) * mapping.scale_x /
                              natural.width,
                          0.2f, 4.0f);
    };
    // The gap after letter i: none before a space or at the end.
    auto gap_after = [&](std::size_t i) {
        if (i + 1 >= text.size() || text[i + 1] == U' ') {
            return 0.0f;
        }
        return text[i] == U'\'' ? kBoardTuck : text[i + 1] == U'\'' ? kBoardLead : kBoardGap;
    };

    std::vector<gfx::Vertex2D> rings, fills;
    // Made again, once, if the atlas started over while they were fetched (that would leave the first ones stale).
    for (int attempt = 0; attempt < 2; attempt++) {
        const unsigned generation = capfont::Generation();
        rings.clear();
        fills.clear();
        float pen = 0.0f;     // unsqueezed, from left_x
        float shifted = 0.0f; // how far the word's adjustments have moved its letters so far
        for (std::size_t i = 0; i < text.size(); i++) {
            if (text[i] == U' ') {
                // A word's adjustments move the rest of it, not the words after it; a space's move the rest of the name.
                float widen = 0.0f;
                for (const BoardAdjust &change : adjust) {
                    if (change.ch == U' ') {
                        widen += change.advance;
                    }
                }
                pen += kBoardSpace - shifted + widen;
                shifted = 0.0f;
                continue;
            }
            const float stretch = stretch_of(i);
            Spread      spread;
            float       advance, move;
            changes_of(i, spread, advance, move);
            capfont::Glyph core, fill, ring;
            if (stretch > 0 && capfont::GetStretchedGlyph(text[i], em_px, 0, stretch, true, core) &&
                capfont::GetStretchedGlyph(text[i], em_px, kBoardWeight * mapping.scale_x, stretch, true, fill)) {
                const float core_x = left_x + (pen + move - spread.left) * squeeze + kBoardWeight;
                // An apostrophe goes up or down so its top is level with the next letter's, and a screen pixel more: the
                // next letter's top is a curve whose topmost row is faint.
                float lift = 0.0f;
                if (text[i] == U'\'' && i + 1 < text.size() && text[i + 1] != U' ') {
                    capfont::Glyph next;
                    const float    next_stretch = stretch_of(i + 1);
                    if (next_stretch > 0 &&
                        capfont::GetStretchedGlyph(text[i + 1], em_px, kBoardWeight * mapping.scale_x, next_stretch, true, next)) {
                        lift = (fill.y_offset - next.y_offset + 1.0f) / mapping.scale_y;
                    }
                }
                auto place = [&](const capfont::Glyph &glyph, std::vector<gfx::Vertex2D> &out, const unsigned char (&rgb)[3]) {
                    if (glyph.width <= 0 || glyph.height <= 0) {
                        return;
                    }
                    const float gx = Snap(core_x + (glyph.x_offset - core.x_offset) / mapping.scale_x, mapping.scale_x, mapping.offset_x);
                    const float gy = Snap(base * rows + (glyph.y_offset / mapping.scale_y - lift) * rows, mapping.scale_y, mapping.offset_y);
                    Quad(out, glyph, gx, gy, glyph.width / mapping.scale_x, glyph.height / mapping.scale_y * rows, rgb, alpha);
                };
                if (capfont::GetStretchedGlyph(text[i], em_px, (kBoardWeight + kBoardOutline) * mapping.scale_x, stretch, true, ring)) {
                    place(ring, rings, kBoardRing);
                }
                place(fill, fills, kBoardFill);
            }
            pen += BoardWidth(text[i]) + gap_after(i) + advance;
            shifted += advance;
        }
        if (generation == capfont::Generation()) {
            break;
        }
    }
    // Cut each quad (corners top-left, top-right, bottom-right, bottom-left) to the rows that are shown.
    const float top = clip_top * rows, bottom = clip_bottom * rows;
    for (auto *layer : {&rings, &fills}) {
        std::vector<gfx::Vertex2D> kept;
        for (std::size_t q = 0; q + 3 < layer->size(); q += 4) {
            gfx::Vertex2D quad[4] = {(*layer)[q], (*layer)[q + 1], (*layer)[q + 2], (*layer)[q + 3]};
            const float   y0 = quad[0].y, y1 = quad[2].y, v0 = quad[0].v, v1 = quad[2].v;
            const float   a = std::max(y0, top), b = std::min(y1, bottom);
            if (b <= a || y1 <= y0) {
                continue;
            }
            const float va = v0 + (v1 - v0) * (a - y0) / (y1 - y0), vb = v0 + (v1 - v0) * (b - y0) / (y1 - y0);
            quad[0].y = quad[1].y = a;
            quad[0].v = quad[1].v = va;
            quad[2].y = quad[3].y = b;
            quad[2].v = quad[3].v = vb;
            kept.insert(kept.end(), quad, quad + 4);
        }
        layer->swap(kept);
    }
    const gfx::TextureBinding binding = capfont::Binding();
    for (auto *layer : {&rings, &fills}) {
        if (!layer->empty()) {
            textwipe::Draw2D(gfx::Primitive::Quads, *layer, binding, draw2d::SpriteState());
        }
    }
    draw2d::RestoreTestZbuf();
    return !fills.empty();
}

void SetNextCaption(const char *key, const char *english) {
    s_next_caption_key = key;
    s_next_caption_english = english;
}

void SetNextCaptionText(std::string_view text) {
    s_next_caption_text = text;
}

std::vector<std::pair<std::string, std::string>> Strings() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const Entry &entry : kEntries) {
        out.emplace_back(entry.key, entry.english);
    }
    return out;
}

bool Ready() {
    return FontReady();
}

bool ThinReady() {
    return FontReady() && capfont::VariantLoaded();
}

bool StatusReady() {
    return FontReady() && capfont::StatusFontLoaded();
}

void PreparePainted(const char *name, const PortDecodedTexture &texture) {
    if (name == nullptr || std::string_view(name) != "stayframe") {
        return;
    }
    g_bounds.fill(std::nullopt);
    if (texture.width == 0 || texture.height == 0 || texture.levels.empty() ||
        (texture.format != gfx::TextureFormat::Index8 && texture.format != gfx::TextureFormat::Rgba8)) {
        return;
    }
    const size_t bytes = texture.format == gfx::TextureFormat::Index8 ? 1 : 4;
    if (texture.levels[0].size() < static_cast<size_t>(texture.width) * texture.height * bytes) {
        return;
    }
    const float scale = static_cast<float>(texture.width) / 640.0f;
    for (int icon = 0; icon < kIcons; icon++) {
        const MENU_ICON_INFO &info = *GetMenuIconInfo(icon);
        const int             x0 = std::clamp(static_cast<int>(std::floor(info.label_u * scale)), 0, static_cast<int>(texture.width));
        const int             y0 = std::clamp(static_cast<int>(std::floor(info.label_v * scale)), 0, static_cast<int>(texture.height));
        const int             x1 = std::clamp(static_cast<int>(std::ceil((info.label_u + info.label_width) * scale)), 0, static_cast<int>(texture.width));
        const int             y1 = std::clamp(static_cast<int>(std::ceil((info.label_v + info.label_height) * scale)), 0, static_cast<int>(texture.height));
        int                   left = x1, right = x0, top = y1, bottom = y0;
        for (int y = y0; y < y1; y++) {
            for (int x = x0; x < x1; x++) {
                const size_t   at = static_cast<size_t>(y) * texture.width + x;
                const unsigned alpha = bytes == 1 ? texture.palette[texture.levels[0][at]] >> 24 : texture.levels[0][at * 4 + 3];
                if (alpha <= 8) {
                    continue;
                }
                left = std::min(left, x);
                right = std::max(right, x + 1);
                top = std::min(top, y);
                bottom = std::max(bottom, y + 1);
            }
        }
        if (right > left && bottom > top && info.label_height > 0) {
            const float rows = static_cast<float>(info.label_height - 1) / info.label_height;
            g_bounds[icon] = CaptionBounds{left / scale - info.label_u, (top / scale - info.label_v) * rows,
                                           (right - left) / scale, (bottom - top) / scale * rows};
        }
    }
}

std::optional<CaptionBounds> PaintedBounds(int icon) {
    return icon >= 0 && icon < kIcons ? g_bounds[icon] : std::nullopt;
}

// The menu captions' letters a little wider than the font's, each about its own middle.
constexpr float kCaptionWiden = 1.06f;

// The English captions' letters spaced as painted: added after each letter (spaces not counted), in pixels.
struct CaptionSpacing {
    std::string_view   text;
    std::vector<float> after;
};
const CaptionSpacing kCaptionSpacing[] = {
    {"ITEM",          {2.4f, -0.8f}},
    {"WEAPON",        {0.8f, -0.8f, 1.6f, -0.8f}},
    {"ALLIES",        {0.0f, 0.8f, 1.2f, 0.8f}},
    {"GEORAMA PARTS", {-0.4f, -0.8f, -0.8f, 0.0f, 0.8f, 0.4f, 0.0f, -2.0f, 0.8f, 0.0f, -2.0f}},
    {"LEAVE DUNGEON", {0.0f, 0.0f, 0.8f, 0.0f, -2.0f, 0.0f, 0.0f, 0.0f, -0.8f, -1.2f}},
    {"OPTION",        {1.6f, 0.0f}},
    {"OPTIONS",       {1.6f, 0.0f, 0.0f, 0.0f, 0.0f, -0.8f}},
    {"JOURNAL",       {0.0f, 0.0f, -0.8f, 0.0f, -0.8f}},
};

std::span<const float> CaptionSpacingOf(const std::string &text) {
    // The caption as drawn: in capitals.
    std::string upper = text;
    for (char &ch : upper) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    for (const CaptionSpacing &spacing : kCaptionSpacing) {
        if (spacing.text == upper) {
            return spacing.after;
        }
    }
    return {};
}

bool DrawTitle(std::string_view utf8, int x, int y, float left, float right, float top, float bottom, float space,
               std::span<const float> after, int alpha, int bright, std::span<const float> narrow) {
    if (utf8.empty() || !FontReady()) {
        return false;
    }
    // Where the fill lands against the painted one's, measured over the load screen's LOAD: the letters run from 0.8 in
    // to 2 on, the capitals from 3.5 up and 0.7 short.
    const float fill_left = left + 0.8f, fill_right = right + 2.0f, cap_top = top - 3.5f, cap_height = bottom - top - 0.7f;
    // DrawLabel's box, outline and copies round the letters, from the letters' own span and capitals.
    const float box_left = fill_left - kLabelOutline + kLabelLightAt.dx;
    const float width = fill_right - fill_left + 2 * kLabelOutline + (kLabelDarkAt.dx - kLabelLightAt.dx);
    const float box_top = cap_top - kLabelOutline + kLabelRaise;
    DrawLabel(std::string(utf8), x, y, width, CaptionBounds{box_left, box_top, width, cap_height}, alpha, bright, cap_height, true,
              space, after, 1.0f, narrow);
    return true;
}

bool Draw(int icon, int x, int y, int box_width, int alpha, int bright) {
    if (icon < 0 || icon >= kIcons || !FontReady()) {
        return false;
    }

    bool        custom_caption = false;
    std::string text;
    if (!s_next_caption_text.empty()) {
        custom_caption = true;
        text = std::move(s_next_caption_text);
        s_next_caption_text.clear();
    } else if (s_next_caption_key != nullptr) {
        custom_caption = true;
        text = LocalizeText(s_next_caption_key, "");
        if (text.empty()) {
            text = (LanguageCode == LANG_ENGLISH_US || LanguageCode == LANG_ENGLISH_UK)
                       ? s_next_caption_english
                       : HelpTitle(icon);
        }
        if (text.empty()) {
            text = s_next_caption_english;
        }
        s_next_caption_key = nullptr;
        s_next_caption_english = nullptr;
    } else {
        // English help titles differ from the painted labels, including OPTION and GEORAMA PARTS.
        text = LocalizeText(kEntries[icon].key, "");
        if (text.empty()) {
            text = LanguageCode == LANG_ENGLISH_US || LanguageCode == LANG_ENGLISH_UK ? kEntries[icon].english : HelpTitle(icon);
        }
        if (text.empty()) {
            text = kEntries[icon].english;
        }
    }

    // Only expand the box width when the Save slot (icon 5, whose vanilla box_width is 56px for "SAVE")
    // is given a combined "SAVE/LOAD" caption; all other icons keep their exact vanilla box_width.
    const bool  is_saveload_combined = custom_caption && icon == 5 && text.find('/') != std::string::npos;
    const float effective_box_w = is_saveload_combined ? 124.0f : static_cast<float>(box_width);
    if (const auto bounds = PaintedBounds(icon)) {
        DrawLabel(text, x, y, effective_box_w, *bounds, alpha, bright, kLabelCap, false, kLabelSpace, CaptionSpacingOf(text),
                  kCaptionWiden);
        return true;
    }

    const Layout            layout = Lay(text, effective_box_w);
    const textwipe::Tuning &tune = textwipe::Get(textwipe::Kind::Caption);
    alpha = static_cast<int>(static_cast<float>(alpha) * tune.opacity);

    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const capfont::Metrics    metrics = capfont::GetMetrics();
    const int                 em_px = std::max(4, static_cast<int>(layout.em * mapping.scale_x + 0.5f));
    const int                 outline_px = std::max(1, static_cast<int>(kOutlineWidth * mapping.scale_x + 0.5f));

    constexpr float cell = 30.0f;
    const float     base = static_cast<float>(y) + (cell - layout.em * (metrics.ascent + metrics.descent)) / 2 +
                           layout.em * metrics.ascent + tune.dy;

    std::vector<gfx::Vertex2D> edges, fills;
    float                      pen = static_cast<float>(x) + tune.dx;
    for (char32_t ch : layout.text) {
        capfont::Glyph fill;
        if (ch == U' ') {
            pen += (layout.em * 0.32f + tune.word) * layout.squeeze;
            continue;
        }
        if (!capfont::GetGlyph(ch, em_px, fill)) {
            pen += layout.em * 0.5f * layout.squeeze;
            continue;
        }
        capfont::Glyph edge;
        const bool     has_edge = capfont::GetOutlinedGlyph(ch, em_px, outline_px, edge);
        auto           place = [&](const capfont::Glyph &glyph, std::vector<gfx::Vertex2D> &out, const unsigned char (&rgb)[3], float shift_x = 0.0f, float shift_y = 0.0f) {
            if (glyph.width <= 0 || glyph.height <= 0) {
                return;
            }
            const float width = glyph.width / mapping.scale_x * layout.squeeze * tune.xscale;
            const float height = glyph.height / mapping.scale_y * rows * tune.yscale;
            float       gx = pen + (glyph.x_offset - fill.x_offset) / mapping.scale_x * layout.squeeze;
            float       gy = base * rows + glyph.y_offset / mapping.scale_y * tune.yscale;
            gx = Snap(gx, mapping.scale_x, mapping.offset_x);
            gy = Snap(gy, mapping.scale_y, mapping.offset_y);
            Quad(out, glyph, gx + shift_x, gy + shift_y * rows, width, height, rgb, alpha);
        };
        if (has_edge) {
            place(edge, edges, kOutline);
        }
        if (tune.weight > 0.0f) {
            const float half = tune.weight / 2;
            place(fill, fills, kFill, -half, 0.0f);
            place(fill, fills, kFill, half, 0.0f);
            place(fill, fills, kFill, 0.0f, -half);
            place(fill, fills, kFill, 0.0f, half);
        }
        place(fill, fills, kFill);
        pen += (fill.width / mapping.scale_x * tune.xscale + layout.em * 0.1f + tune.letter) * layout.squeeze;
    }
    if (!fills.empty()) {
        if (!custom_caption) {
            if (const auto bounds = PaintedBounds(icon); bounds && !edges.empty() && tune.fit >= 0.5f) {
                float left = edges.front().x, right = left;
                float top = edges.front().y, bottom = top;
                for (const gfx::Vertex2D &vertex : edges) {
                    left = std::min(left, vertex.x);
                    right = std::max(right, vertex.x);
                    top = std::min(top, vertex.y);
                    bottom = std::max(bottom, vertex.y);
                }
                const float width = bounds->width * tune.size * tune.xscale;
                const float height = bounds->height * tune.size * tune.yscale;
                const float sx = width / (right - left);
                const float sy = height * rows / (bottom - top);
                for (auto *layer : {&edges, &fills}) {
                    for (gfx::Vertex2D &vertex : *layer) {
                        vertex.x = x + bounds->left + (bounds->width - width) / 2 + tune.dx + (vertex.x - left) * sx;
                        vertex.y = (y + bounds->top + tune.dy) * rows + (vertex.y - top) * sy;
                    }
                }
            }
        }
        for (auto *layer : {&edges, &fills}) {
            for (gfx::Vertex2D &vertex : *layer) {
                for (int channel = 0; channel < 3; channel++) {
                    vertex.color[channel] = static_cast<uint8_t>(std::clamp(vertex.color[channel] * bright / 0x80, 0, 255));
                }
            }
        }
        const gfx::TextureBinding binding = capfont::Binding();
        if (!edges.empty()) {
            textwipe::Draw2D(gfx::Primitive::Quads, edges, binding, draw2d::SpriteState());
        }
        textwipe::Draw2D(gfx::Primitive::Quads, fills, binding, draw2d::SpriteState());
        draw2d::RestoreTestZbuf();
    }
    return true;
}

std::vector<LetterWidth> LetterWidths(std::string_view utf8) {
    std::vector<LetterWidth> out;
    if (!FontReady()) {
        return out;
    }
    constexpr int em_px = 64;
    for (char32_t ch : Decode(utf8)) {
        capfont::Glyph glyph;
        if (ch == U' ') {
            out.push_back({em_px * 0.3f, true});
        } else if (capfont::GetGlyph(ch, em_px, glyph) && glyph.width > 0) {
            out.push_back({static_cast<float>(glyph.width), false});
        }
    }
    return out;
}

bool DrawLine(std::string_view utf8, float center_x, float cap_top, float cap_height, float xsq, float max_width,
              const LineStyle &style, float fade, float rows, LineBounds *bounds) {
    if (utf8.empty() || cap_height <= 0.0f || !FontReady()) {
        return false;
    }

    struct Variant {
        explicit Variant(int slot) { capfont::UseFontSlot(slot); }

        ~Variant() { capfont::UseFontSlot(0); }
    } variant(style.status_font ? 2 : (style.thin ? 1 : 0));

    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const capfont::Metrics    metrics = capfont::GetMetrics();
    const std::u32string      text = Decode(utf8);
    float                     em = cap_height / std::max(0.2f, metrics.cap);
    int                       em_px = std::clamp(static_cast<int>(em * mapping.scale_x + 0.5f), 4, 512);
    float                     ink_top = 0.0f;
    float                     ink_height = 0.0f;
    if (style.ink_fit) {
        // Re-measure after integer rasterization, then fit the ink exactly to the requested height.
        for (int pass = 0; pass < 2; pass++) {
            float top = 1e9f, bottom = -1e9f;
            for (char32_t ch : text) {
                capfont::Glyph glyph;
                if (ch != U' ' && capfont::GetGlyph(ch, em_px, glyph) && glyph.width > 0) {
                    top = std::min(top, static_cast<float>(glyph.y_offset));
                    bottom = std::max(bottom, static_cast<float>(glyph.y_offset + glyph.height));
                }
            }
            const float ink = (bottom - top) / mapping.scale_y;
            if (ink <= 0.0f) {
                break;
            }
            ink_top = top;
            ink_height = bottom - top;
            if (pass == 0) {
                em *= cap_height / ink;
                em_px = std::clamp(static_cast<int>(em * mapping.scale_x + 0.5f), 4, 512);
            }
        }
    }
    const float gap = cap_height * 0.075f + style.letter;

    struct Letter {
        capfont::Glyph fill;
        char32_t       ch;
        bool           blank;
    };

    std::vector<Letter> letters;
    float               natural = 0.0f;
    for (char32_t ch : text) {
        Letter letter = {};
        letter.ch = ch;
        if (ch == U' ') {
            letter.blank = true;
            natural += cap_height * 0.32f + style.word;
        } else if (capfont::GetGlyph(ch, em_px, letter.fill) && letter.fill.width > 0) {
            natural += letter.fill.width / mapping.scale_x * style.xscale + gap;
        } else {
            continue;
        }
        letters.push_back(letter);
    }
    if (letters.empty()) {
        return false;
    }
    natural -= gap;
    float squeeze = xsq;
    if (max_width > 0.0f && natural * squeeze > max_width) {
        squeeze = max_width / natural;
    }
    const float width = natural * squeeze;
    const float left = center_x + style.dx - width / 2;

    const float vertical_scale = style.ink_fit && ink_height > 0.0f
                                     ? cap_height * rows * style.yscale / ink_height
                                     : rows * style.yscale / mapping.scale_y;
    const float baseline = style.ink_fit && ink_height > 0.0f
                               ? (cap_top + style.dy) * rows - ink_top * vertical_scale
                               : (cap_top + cap_height + style.dy) * rows;
    const int   edge_px = std::clamp(static_cast<int>(style.edge_width * mapping.scale_x + 0.5f), 0, 60);
    const int   glow_px = style.glow_amount > 0.0f ? std::clamp(static_cast<int>(style.glow_width * mapping.scale_x + 0.5f), 0, 60) : 0;

    auto rect_of = [&](const capfont::Glyph &glyph, const capfont::Glyph &fill, float pen, float &x, float &y, float &w, float &h) {
        w = glyph.width / mapping.scale_x * squeeze * style.xscale;
        h = glyph.height * vertical_scale;
        x = pen + (glyph.x_offset - fill.x_offset) / mapping.scale_x * squeeze * style.xscale;
        y = baseline + glyph.y_offset * vertical_scale;
        x = Snap(x, mapping.scale_x, mapping.offset_x);
        y = Snap(y, mapping.scale_y, mapping.offset_y);
    };
    // Colored glyphs depend on their position in the painted fill field and on atlas generation.
    static std::map<std::array<int, 9>, capfont::Glyph> textured;
    static unsigned                                     textured_generation = 0;
    auto                                                textured_letter = [&](const Letter &letter, size_t index, float pen, capfont::Glyph &out) {
        if (style.field == nullptr || style.field->width <= 0 || style.field->rgb.size() < static_cast<size_t>(style.field->width) * style.field->height * 3) {
            return false;
        }
        if (textured_generation != capfont::Generation() || textured.size() > 96) {
            textured.clear();
            textured_generation = capfont::Generation();
        }
        float x, y, w, h;
        rect_of(letter.fill, letter.fill, pen, x, y, w, h);
        const std::array<int, 9> key = {static_cast<int>(style.field->id), static_cast<int>(index), em_px,
                                        static_cast<int>(std::lround(x * 8)), static_cast<int>(std::lround(y * 8)),
                                        static_cast<int>(std::lround(w * 8)), static_cast<int>(std::lround(h * 8)),
                                        static_cast<int>(std::lround(style.field_x * 8)), static_cast<int>(std::lround(style.field_y * 8))};
        if (auto found = textured.find(key); found != textured.end()) {
            out = found->second;
            return true;
        }
        std::vector<unsigned char> coverage;
        int                        gw, gh, gx0, gy0;
        if (!capfont::RenderGlyph(letter.ch, em_px, coverage, gw, gh, gx0, gy0) || gw <= 0 || gh <= 0 ||
            gw != static_cast<int>(letter.fill.width) || gh != static_cast<int>(letter.fill.height)) {
            return false;
        }
        const FillField           &field = *style.field;
        std::vector<unsigned char> rgba(static_cast<size_t>(gw) * gh * 4);
        for (int j = 0; j < gh; j++) {
            for (int i = 0; i < gw; i++) {
                const float    lx = x + (static_cast<float>(i) + 0.5f) / gw * w;
                const float    ly = y + (static_cast<float>(j) + 0.5f) / gh * h;
                const float    u = std::clamp((lx - style.field_x) / style.field_w * field.width - 0.5f, 0.0f, field.width - 1.001f);
                const float    v = std::clamp((ly - style.field_y) / style.field_h * field.height - 0.5f, 0.0f, field.height - 1.001f);
                const int      u0 = static_cast<int>(u), v0 = static_cast<int>(v);
                const float    fu = u - u0, fv = v - v0;
                unsigned char *texel = &rgba[(static_cast<size_t>(j) * gw + i) * 4];
                for (int c = 0; c < 3; c++) {
                    auto        at = [&](int xx, int yy) { return static_cast<float>(field.rgb[(static_cast<size_t>(yy) * field.width + xx) * 3 + c]); };
                    const float top = at(u0, v0) * (1 - fu) + at(u0 + 1, v0) * fu;
                    const float bottom = at(u0, v0 + 1) * (1 - fu) + at(u0 + 1, v0 + 1) * fu;
                    texel[c] = static_cast<unsigned char>(std::clamp(top * (1 - fv) + bottom * fv, 0.0f, 255.0f));
                }
                texel[3] = coverage[static_cast<size_t>(j) * gw + i];
            }
        }
        capfont::Glyph image;
        if (!capfont::AddImage(rgba, gw, gh, image)) {
            return false;
        }
        image.x_offset = letter.fill.x_offset;
        image.y_offset = letter.fill.y_offset;
        image.advance = letter.fill.advance;
        textured[key] = image;
        out = image;
        return true;
    };
    auto quad = [&](std::vector<gfx::Vertex2D> &out, const capfont::Glyph &glyph, const capfont::Glyph &fill, float pen,
                    const unsigned char (&rgb)[3], int alpha, float shift_x = 0.0f, float shift_y = 0.0f) {
        if (glyph.width <= 0 || glyph.height <= 0) {
            return;
        }
        float x, y, w, h;
        rect_of(glyph, fill, pen, x, y, w, h);
        x += shift_x;
        y += shift_y * rows;
        const float  k = std::clamp(fade, 0.0f, 1.0f) * style.opacity * 0.5f;
        const u_char r = static_cast<u_char>(rgb[0] * k), g = static_cast<u_char>(rgb[1] * k), b = static_cast<u_char>(rgb[2] * k);
        const u_char a = static_cast<u_char>(std::clamp(static_cast<int>(std::lround(alpha * std::clamp(style.alpha, 0.0f, 1.0f))), 0, 0x80));
        out.push_back(draw2d::Vertex(x, y, 0.0f, glyph.u0, glyph.v0, r, g, b, a));
        out.push_back(draw2d::Vertex(x + w, y, 0.0f, glyph.u1, glyph.v0, r, g, b, a));
        out.push_back(draw2d::Vertex(x + w, y + h, 0.0f, glyph.u1, glyph.v1, r, g, b, a));
        out.push_back(draw2d::Vertex(x, y + h, 0.0f, glyph.u0, glyph.v1, r, g, b, a));
    };

    size_t drawn = 0;
    for (const Letter &letter : letters) {
        drawn += letter.blank ? 0 : 1;
    }
    const bool                 use_slots = style.slots.size() == drawn;
    size_t                     slot = 0;
    std::vector<gfx::Vertex2D> glows, edges, fills;
    float                      pen = left;
    size_t                     letter_index = 0;
    for (const Letter &letter : letters) {
        if (letter.blank) {
            pen += (cap_height * 0.32f + style.word) * squeeze;
            continue;
        }
        if (use_slots) {
            const std::array<float, 2> &place = style.slots[slot++];
            const float                 ink = letter.fill.width / mapping.scale_x * style.xscale;
            const float                 slot_width = place[1] - place[0];
            squeeze = slot_width / std::max(ink, 0.01f);
            pen = place[0];
        }
        capfont::Glyph extra;
        if (glow_px > 0 && capfont::GetOutlinedGlyph(letter.ch, em_px, glow_px, extra)) {
            quad(glows, extra, letter.fill, pen, style.glow, static_cast<int>(0x80 * std::min(1.0f, style.glow_amount) * 0.55f));
        }
        if (edge_px > 0 && capfont::GetOutlinedGlyph(letter.ch, em_px, edge_px, extra)) {
            quad(edges, extra, letter.fill, pen, style.edge, 0x80);
        }
        if (style.weight > 0.0f) {
            const float half = style.weight / 2;
            quad(fills, letter.fill, letter.fill, pen, style.fill, 0x80, -half, 0.0f);
            quad(fills, letter.fill, letter.fill, pen, style.fill, 0x80, half, 0.0f);
            quad(fills, letter.fill, letter.fill, pen, style.fill, 0.0f, -half);
            quad(fills, letter.fill, letter.fill, pen, style.fill, 0x80, 0.0f, half);
        }
        {
            static const unsigned char white[3] = {255, 255, 255};
            capfont::Glyph             coloured;
            if (textured_letter(letter, letter_index, pen, coloured)) {
                quad(fills, coloured, letter.fill, pen, white, 0x80);
            } else {
                quad(fills, letter.fill, letter.fill, pen, style.fill, 0x80);
            }
        }
        letter_index++;
        pen += (letter.fill.width / mapping.scale_x * style.xscale + gap) * squeeze;
    }
    const gfx::TextureBinding binding = capfont::Binding();
    for (std::vector<gfx::Vertex2D> *layer : {&glows, &edges, &fills}) {
        if (!layer->empty()) {
            textwipe::Draw2D(gfx::Primitive::Quads, *layer, binding, draw2d::SpriteState());
        }
    }
    draw2d::RestoreTestZbuf();
    if (bounds != nullptr) {
        bounds->left = left;
        bounds->right = left + width;
        if (use_slots) {
            bounds->left = style.slots.front()[0];
            bounds->right = style.slots.front()[1];
            for (const auto &place : style.slots) {
                bounds->left = std::min(bounds->left, place[0]);
                bounds->right = std::max(bounds->right, place[1]);
            }
        }
    }
    return true;
}

} // namespace caption
