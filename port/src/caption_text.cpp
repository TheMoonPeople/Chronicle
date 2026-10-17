#include "caption_text.hpp"

#include <algorithm>
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
#  if __has_embed("../../tools/font/SourceSansProSemibold.ttf")
#    define CHRONICLE_HAS_EMBEDDED_STATUS_FONT 1
alignas(16) constexpr unsigned char kEmbeddedStatusFont[] = {
#    embed "../../tools/font/SourceSansProSemibold.ttf"
};
#  elif __has_embed("../tools/font/SourceSansProSemibold.ttf")
#    define CHRONICLE_HAS_EMBEDDED_STATUS_FONT 1
alignas(16) constexpr unsigned char kEmbeddedStatusFont[] = {
#    embed "../tools/font/SourceSansProSemibold.ttf"
};
#  endif
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
    if (!p.empty()) return p;

    static const char* kCandidates[] = {
        "tools/font/SourceSansProSemibold.ttf",
        "../tools/font/SourceSansProSemibold.ttf",
        "../../tools/font/SourceSansProSemibold.ttf",
        "/run/build/chronicle/tools/font/SourceSansProSemibold.ttf",
        "lang/SourceSansProSemibold.ttf"
    };
    for (const char* rel : kCandidates) {
        std::error_code ec;
        if (std::filesystem::exists(rel, ec)) {
            return std::filesystem::path(rel);
        }
    }
    return {};
}

bool FontReady() {
    if (!ConfigGet().font_sharp) {
        return false;
    }
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (!capfont::Loaded()) {
            std::filesystem::path file = LocalizeFindFile("font.ttf");
            if (!file.empty()) {
                capfont::Load(file);
            }
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

} // namespace

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

bool Draw(int icon, int x, int y, int box_width, int alpha, int bright) {
    if (icon < 0 || icon >= kIcons || !FontReady()) {
        return false;
    }

    bool custom_caption = false;
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
    const float effective_box_w      = is_saveload_combined ? 124.0f : static_cast<float>(box_width);

    const Layout            layout = Lay(text, effective_box_w);
    const textwipe::Tuning &tune = textwipe::Get(textwipe::Kind::Caption);
    alpha = static_cast<int>(static_cast<float>(alpha) * tune.opacity);

    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const float               rows = draw2d::RowScale();
    const capfont::Metrics    metrics = capfont::GetMetrics();
    const int                 em_px = std::max(4, static_cast<int>(layout.em * mapping.scale_x + 0.5f));
    const int                 outline_px = std::max(1, static_cast<int>(kOutlineWidth * mapping.scale_x + 0.5f));

    constexpr float kCell = 30.0f;
    const float     base = static_cast<float>(y) + (kCell - layout.em * (metrics.ascent + metrics.descent)) / 2 +
                           layout.em * metrics.ascent + tune.dy;

    auto snap = [](float value, float scale, float offset) { return (std::round(value * scale + offset) - offset) / scale; };

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
            gx = snap(gx, mapping.scale_x, mapping.offset_x);
            gy = snap(gy, mapping.scale_y, mapping.offset_y);
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
    constexpr int kEmPx = 64;
    for (char32_t ch : Decode(utf8)) {
        capfont::Glyph glyph;
        if (ch == U' ') {
            out.push_back({kEmPx * 0.3f, true});
        } else if (capfont::GetGlyph(ch, kEmPx, glyph) && glyph.width > 0) {
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
    auto        snap = [](float value, float scale, float offset) { return (std::round(value * scale + offset) - offset) / scale; };
    const int   edge_px = std::clamp(static_cast<int>(style.edge_width * mapping.scale_x + 0.5f), 0, 60);
    const int   glow_px = style.glow_amount > 0.0f ? std::clamp(static_cast<int>(style.glow_width * mapping.scale_x + 0.5f), 0, 60) : 0;

    auto rect_of = [&](const capfont::Glyph &glyph, const capfont::Glyph &fill, float pen, float &x, float &y, float &w, float &h) {
        w = glyph.width / mapping.scale_x * squeeze * style.xscale;
        h = glyph.height * vertical_scale;
        x = pen + (glyph.x_offset - fill.x_offset) / mapping.scale_x * squeeze * style.xscale;
        y = baseline + glyph.y_offset * vertical_scale;
        x = snap(x, mapping.scale_x, mapping.offset_x);
        y = snap(y, mapping.scale_y, mapping.offset_y);
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
        const u_char a = static_cast<u_char>(std::clamp(alpha, 0, 0x80));
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