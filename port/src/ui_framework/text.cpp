#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "caption_text.hpp"
#include "captionfont.hpp"
#include "gfx/gfx.hpp"
#include "textwipe.hpp"
#include "ui_framework/internal.hpp"

namespace UI {

namespace {

/**
 * TrueType font slots managed by capfont.
 */
enum class FontSlot : int {
    Main = 0,
    Thin = 1,
    Status = 2,
};

/**
 * Parameters controlling multi-pass scaled TrueType text rendering.
 */
struct ScaledDrawParams {
    u32        color = ColorWhite;          /**< Fallback palette color constant. */
    FontWeight weight = FontWeight::Normal; /**< Font weight and slot selector. */
    bool       drop_shadow = true;          /**< Whether to cast a drop shadow. */
    bool       use_custom_rgb = false;      /**< Whether to use explicit RGB fill channels. */
    u8         fill_r = 240;                /**< Custom fill red channel. */
    u8         fill_g = 238;                /**< Custom fill green channel. */
    u8         fill_b = 232;                /**< Custom fill blue channel. */
    float      outline_width = 0.0f;        /**< Outer stroke width in logical pixels. */
    u8         outline_r = 16;              /**< Outer stroke red channel. */
    u8         outline_g = 28;              /**< Outer stroke green channel. */
    u8         outline_b = 68;              /**< Outer stroke blue channel. */
    bool       vertical_grad = false;       /**< Whether to apply a top-to-bottom highlight gradient. */
    bool       raised_bevel = false;        /**< Whether to render a raised 3D extrusion pass. */
    u8         bevel_r = 108;               /**< Raised bevel red channel. */
    u8         bevel_g = 134;               /**< Raised bevel green channel. */
    u8         bevel_b = 192;               /**< Raised bevel blue channel. */
    bool       recessed = false;            /**< Whether to render an engraved button text pass. */
    u8         recess_dark_r = 72;          /**< Engraved top-left shadow red channel. */
    u8         recess_dark_g = 34;          /**< Engraved top-left shadow green channel. */
    u8         recess_dark_b = 18;          /**< Engraved top-left shadow blue channel. */
    u8         recess_light_r = 250;        /**< Engraved bottom-right catch-light red channel. */
    u8         recess_light_g = 195;        /**< Engraved bottom-right catch-light green channel. */
    u8         recess_light_b = 155;        /**< Engraved bottom-right catch-light blue channel. */
    bool       painted = false;             /**< Whether to letter as the painted buttons are. */
    float      letter = 0.0f;               /**< Added to the gap between letters. */
};

/**
 * Decodes a UTF-8 string into UTF-32 code points.
 */
std::u32string DecodeUtf8(std::string_view in) {
    std::u32string out;
    for (size_t i = 0; i < in.size();) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        const int           extra = c < 0x80         ? 0
                                    : (c >> 5) == 6  ? 1
                                    : (c >> 4) == 14 ? 2
                                    : (c >> 3) == 30 ? 3
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

/**
 * Returns whether a label requires the multi-pass scaled TrueType renderer.
 */
bool UseScaledRenderer(std::string_view text, float scale, float xscale, float yscale,
                       FontWeight weight, bool use_custom_rgb, float outline_width) {
    if (text.find('{') != std::string::npos) {
        return false;
    }
    if (std::abs(scale - 1.0f) <= 0.001f &&
        std::abs(xscale - 1.0f) <= 0.001f &&
        std::abs(yscale - 1.0f) <= 0.001f &&
        weight == FontWeight::Normal &&
        !use_custom_rgb &&
        outline_width <= 0.001f) {
        return false;
    }
    return caption::Ready();
}

/**
 * Measures the horizontal advance width of a string in logical pixels.
 */
int MeasureScaledText(std::string_view text, float scale, float xscale, FontWeight weight, float letter = 0.0f) {
    if (text.empty()) {
        return 0;
    }

    const FontSlot slot = (weight == FontWeight::Status && caption::StatusReady()) ? FontSlot::Status
                          : (weight == FontWeight::Thin && caption::ThinReady())   ? FontSlot::Thin
                              : FontSlot::Main;

    /**
     * Scoped guard that activates a capfont slot and restores the main slot on exit.
     */
    struct VariantGuard {
        /**
         * Selects the requested font slot.
         */
        explicit VariantGuard(FontSlot s) {
            capfont::UseFontSlot(static_cast<int>(s));
        }

        /**
         * Restores the default main font slot.
         */
        ~VariantGuard() {
            capfont::UseFontSlot(static_cast<int>(FontSlot::Main));
        }
    } guard(slot);

    const gfx::LogicalMapping mapping = gfx::GetUiMapping(gfx::CurrentRenderTarget());
    const capfont::Metrics    metrics = capfont::GetMetrics();
    const float               cap_height = detail::kBaseCapHeight * scale;
    const float               em = cap_height / std::max(0.2f, metrics.cap);
    const int                 em_px = std::clamp(static_cast<int>(em * mapping.scale_x + 0.5f), 4, 512);
    const float               gap = cap_height * 0.075f + letter;

    float natural = 0.0f;
    int   drawn = 0;
    for (char32_t ch : DecodeUtf8(text)) {
        capfont::Glyph glyph;
        if (ch == U' ') {
            natural += cap_height * 0.32f + (3.0f * scale);
            drawn++;
        } else if (capfont::GetGlyph(ch, em_px, glyph) && glyph.width > 0) {
            natural += (glyph.width / mapping.scale_x * xscale) + gap;
            drawn++;
        }
    }
    if (drawn > 0) {
        natural -= gap;
    }
    return static_cast<int>(std::round(natural));
}

/**
 * Builds or retrieves a cached vertical gradient fill field for status labels.
 */
const caption::FillField *GetVerticalGradField(u8 base_r, u8 base_g, u8 base_b) {
    using Key = std::tuple<u8, u8, u8>;
    static std::map<Key, caption::FillField> fields;
    static unsigned                          next_id = 5000;

    const Key key{base_r, base_g, base_b};
    auto      it = fields.find(key);
    if (it != fields.end()) {
        return &it->second;
    }

    const float top_r = std::clamp(static_cast<float>(base_r) * 0.22f + 248.0f * 0.78f, 0.0f, 255.0f);
    const float top_g = std::clamp(static_cast<float>(base_g) * 0.22f + 252.0f * 0.78f, 0.0f, 255.0f);
    const float top_b = std::clamp(static_cast<float>(base_b) * 0.22f + 236.0f * 0.78f, 0.0f, 255.0f);

    caption::FillField field{};
    field.width = 2;
    field.height = 16;
    field.id = ++next_id;
    field.rgb.resize(static_cast<size_t>(field.width * field.height * 3));

    for (int y = 0; y < field.height; ++y) {
        const float v = static_cast<float>(y) / static_cast<float>(field.height - 1);
        const float raw = std::clamp((v - 0.25f) / 0.65f, 0.0f, 1.0f);
        const float t = raw * raw * (3.0f - 2.0f * raw);

        const u8 r = static_cast<u8>(std::lround(top_r + (static_cast<float>(base_r) - top_r) * t));
        const u8 g = static_cast<u8>(std::lround(top_g + (static_cast<float>(base_g) - top_g) * t));
        const u8 b = static_cast<u8>(std::lround(top_b + (static_cast<float>(base_b) - top_b) * t));

        for (int x = 0; x < field.width; ++x) {
            const size_t idx = static_cast<size_t>((y * field.width + x) * 3);
            field.rgb[idx + 0] = r;
            field.rgb[idx + 1] = g;
            field.rgb[idx + 2] = b;
        }
    }

    auto [ins_it, _] = fields.emplace(key, std::move(field));
    return &ins_it->second;
}

/**
 * Draws a single line of scaled TrueType text with optional shadow, bevel, or engraving passes.
 */
void DrawScaledText(std::string_view text, float center_x, float cap_top, float scale,
                    float xscale, float yscale, const ScaledDrawParams &params, int alpha) {
    if (text.empty() || alpha <= 0) {
        return;
    }

    const float snapped_x = std::round(center_x);
    // The painted buttons' lettering is placed to the screen's pixel, the rest to the game's.
    const float paint_y = gfx::GetUiMapping(gfx::CurrentRenderTarget()).scale_y;
    const float snapped_top = params.painted ? std::round(cap_top * paint_y) / paint_y : std::round(cap_top);
    const float cap_height = detail::kBaseCapHeight * scale;
    const float rows = draw2d::RowScale();
    const float fade = std::clamp(static_cast<float>(alpha) / 128.0f, 0.0f, 1.0f);

    const bool is_thin = (params.weight == FontWeight::Thin);
    const bool is_status = (params.weight == FontWeight::Status);

    caption::LineStyle style{};
    style.thin = is_thin && caption::ThinReady();
    style.status_font = (is_status && caption::StatusReady());
    style.weight = (params.weight == FontWeight::Bold) ? 1.1f : 0.0f;
    style.xscale = xscale;
    style.yscale = yscale;
    style.word = 3.0f * scale;
    style.letter = params.letter;
    style.ink_fit = false;

    if (params.outline_width > 0.0f) {
        style.edge[0] = params.outline_r;
        style.edge[1] = params.outline_g;
        style.edge[2] = params.outline_b;
        style.edge_width = params.outline_width;
    } else {
        style.edge_width = 0.0f;
    }

    if (params.painted) {
        // The painted buttons' lettering (wepstatus): cream letters with no outline, sunk in a broad soft dark halo
        // that is deepest below and right of them, lighter left of them and faint above: a grid of faint copies
        // spread round a point a little down and right.
        caption::LineStyle shade = style;
        shade.field = nullptr;
        shade.fill[0] = 58;
        shade.fill[1] = 30;
        shade.fill[2] = 18;
        shade.edge[0] = 58;
        shade.edge[1] = 30;
        shade.edge[2] = 18;
        shade.edge_width = 0.8f;
        shade.opacity = 1.0f;
        shade.alpha = 0.035f;
        for (float dy : {-0.3f, 0.3f, 0.9f, 1.5f, 2.1f}) {
            for (float dx : {-0.6f, 0.0f, 0.6f, 1.2f, 1.8f}) {
                caption::DrawLine(text, snapped_x + dx, snapped_top + dy, cap_height, 1.0f, 0.0f, shade, fade, rows, nullptr);
            }
        }
        style.edge_width = 0.0f;
    }

    if (params.drop_shadow && ConfigGet().text_shadow > 0 && !params.raised_bevel && !params.recessed && !params.painted) {
        caption::LineStyle shadow_style = style;
        shadow_style.fill[0] = 0;
        shadow_style.fill[1] = 0;
        shadow_style.fill[2] = 0;
        shadow_style.edge[0] = 0;
        shadow_style.edge[1] = 0;
        shadow_style.edge[2] = 0;
        shadow_style.edge_width = (is_thin || is_status) ? 0.0f : (0.5f * scale);

        const float base_op = std::clamp(static_cast<float>(ConfigGet().text_shadow) / 100.0f, 0.0f, 1.0f);
        shadow_style.opacity = is_thin ? (base_op * 0.55f) : (is_status ? (base_op * 0.65f) : base_op);

        const float s_mult = (is_thin || is_status) ? 1.25f : 1.85f;
        const float s_dx = std::round(s_mult * scale);
        const float s_dy = std::round(s_mult * scale);
        caption::DrawLine(text, snapped_x + s_dx, snapped_top + s_dy, cap_height, 1.0f, 0.0f, shadow_style, fade, rows, nullptr);
    }

    if (params.recessed) {
        caption::LineStyle light_lip = style;
        light_lip.field = nullptr;
        light_lip.fill[0] = params.recess_light_r;
        light_lip.fill[1] = params.recess_light_g;
        light_lip.fill[2] = params.recess_light_b;
        light_lip.edge_width = 0.0f;
        light_lip.opacity = 0.70f;
        caption::DrawLine(text, snapped_x + 1.0f, snapped_top + 1.0f, cap_height, 1.0f, 0.0f, light_lip, fade, rows, nullptr);

        caption::LineStyle dark_lip = style;
        dark_lip.field = nullptr;
        dark_lip.fill[0] = params.recess_dark_r;
        dark_lip.fill[1] = params.recess_dark_g;
        dark_lip.fill[2] = params.recess_dark_b;
        dark_lip.edge[0] = params.recess_dark_r;
        dark_lip.edge[1] = params.recess_dark_g;
        dark_lip.edge[2] = params.recess_dark_b;
        dark_lip.edge_width = 0.65f;
        dark_lip.opacity = 0.95f;
        caption::DrawLine(text, snapped_x - 1.0f, snapped_top - 1.0f, cap_height, 1.0f, 0.0f, dark_lip, fade, rows, nullptr);

        style.edge[0] = params.recess_dark_r;
        style.edge[1] = params.recess_dark_g;
        style.edge[2] = params.recess_dark_b;
        style.edge_width = 0.55f;
    }

    if (params.raised_bevel) {
        caption::LineStyle bevel_style = style;
        bevel_style.field = nullptr;
        bevel_style.fill[0] = params.bevel_r;
        bevel_style.fill[1] = params.bevel_g;
        bevel_style.fill[2] = params.bevel_b;
        bevel_style.edge[0] = params.outline_r;
        bevel_style.edge[1] = params.outline_g;
        bevel_style.edge[2] = params.outline_b;
        bevel_style.edge_width = std::max(1.35f, params.outline_width);
        bevel_style.opacity = 1.0f;
        caption::DrawLine(text, snapped_x + 0.9f, snapped_top + 0.9f, cap_height, 1.0f, 0.0f, bevel_style, fade, rows, nullptr);
    }

    if (params.use_custom_rgb) {
        style.fill[0] = params.fill_r;
        style.fill[1] = params.fill_g;
        style.fill[2] = params.fill_b;
        if (params.vertical_grad) {
            style.field = GetVerticalGradField(params.fill_r, params.fill_g, params.fill_b);
            style.field_x = snapped_x - 160.0f;
            style.field_w = 320.0f;
            style.field_y = snapped_top + cap_height * (1.0f - yscale);
            style.field_h = std::max(4.0f, cap_height * yscale);
        }
    } else {
        detail::ColorToRgb(params.color, style.fill);
    }
    style.opacity = 1.0f;
    caption::DrawLine(text, snapped_x, snapped_top, cap_height, 1.0f, 0.0f, style, fade, rows, nullptr);
}

} // namespace

std::vector<std::string> WrapText(std::string_view text, int max_width, float scale) {
    std::vector<std::string> lines;
    if (text.empty() || max_width <= 0) {
        return lines;
    }

    std::string current_line;
    size_t      i = 0;

    while (i < text.size()) {
        if (text[i] == '\r') {
            i++;
            continue;
        }
        if (text[i] == '\n') {
            lines.push_back(current_line);
            current_line.clear();
            i++;
            continue;
        }

        if (current_line.empty()) {
            while (i < text.size() && text[i] == ' ') {
                i++;
            }
            if (i >= text.size()) {
                break;
            }
        }

        const size_t word_start = i;
        while (i < text.size() && text[i] != ' ' && text[i] != '\n' && text[i] != '\r') {
            i++;
        }
        std::string word(text.substr(word_start, i - word_start));

        if (i < text.size() && text[i] == ' ') {
            i++;
        }

        std::string candidate = current_line.empty() ? word : (current_line + " " + word);

        Label test_lbl(candidate, ColorWhite, Alignment::Left, true, scale);
        if (test_lbl.GetWidth() > max_width && !current_line.empty()) {
            lines.push_back(current_line);
            current_line = word;
        } else {
            current_line = candidate;
        }
    }

    if (!current_line.empty()) {
        lines.push_back(current_line);
    }

    return lines;
}

Label::Label(std::string_view text, u32 color, Alignment align, bool drop_shadow,
             float scale, FontWeight weight, float xscale)
    : m_raw_text(text),
      m_color(color),
      m_align(align),
      m_drop_shadow(drop_shadow),
      m_scale(scale),
      m_xscale(xscale),
      m_weight(weight) {
    m_text.Set(m_raw_text.c_str());
}

Label::Label(std::string_view text, const TextStyle &style)
    : m_raw_text(text),
      m_color(style.color),
      m_align(style.align),
      m_drop_shadow(style.drop_shadow),
      m_scale(style.scale),
      m_xscale(style.xscale),
      m_yscale(style.yscale),
      m_weight(style.weight),
      m_dx(style.text_dx),
      m_dy(style.text_dy),
      m_use_custom_rgb(style.use_custom_rgb),
      m_fill_r(style.fill_r),
      m_fill_g(style.fill_g),
      m_fill_b(style.fill_b),
      m_outline_width(style.outline_width),
      m_outline_r(style.outline_r),
      m_outline_g(style.outline_g),
      m_outline_b(style.outline_b),
      m_raised_bevel(style.raised_bevel),
      m_bevel_r(style.bevel_r),
      m_bevel_g(style.bevel_g),
      m_bevel_b(style.bevel_b),
      m_recessed(style.recessed),
      m_recess_dark_r(style.recess_dark_r),
      m_recess_dark_g(style.recess_dark_g),
      m_recess_dark_b(style.recess_dark_b),
      m_recess_light_r(style.recess_light_r),
      m_recess_light_g(style.recess_light_g),
      m_recess_light_b(style.recess_light_b),
      m_painted(style.painted),
      m_letter(style.letter) {
    m_text.Set(m_raw_text.c_str());
}

void Label::SetText(std::string_view text) {
    m_raw_text = text;
    m_text.Set(m_raw_text.c_str());
    m_dirty = true;
}

void Label::SetColor(u32 color) {
    m_color = color;
    m_use_custom_rgb = false;
}

void Label::SetCustomRgb(u8 r, u8 g, u8 b) {
    m_use_custom_rgb = true;
    m_fill_r = r;
    m_fill_g = g;
    m_fill_b = b;
}

void Label::SetOutline(float width, u8 r, u8 g, u8 b) {
    m_outline_width = width;
    m_outline_r = r;
    m_outline_g = g;
    m_outline_b = b;
}

void Label::SetAlignment(Alignment align) {
    m_align = align;
}

void Label::SetDropShadow(bool enabled) {
    m_drop_shadow = enabled;
}

void Label::SetScale(float scale) {
    m_scale = scale;
    m_dirty = true;
}

void Label::SetXScale(float xscale) {
    m_xscale = xscale;
    m_dirty = true;
}

void Label::SetYScale(float yscale) {
    m_yscale = yscale;
    m_dirty = true;
}

void Label::SetWeight(FontWeight weight) {
    m_weight = weight;
    m_dirty = true;
}

void Label::SetOffset(int dx, int dy) {
    m_dx = dx;
    m_dy = dy;
}

void Label::SetStyle(const TextStyle &style) {
    m_color = style.color;
    m_align = style.align;
    m_drop_shadow = style.drop_shadow;
    m_scale = style.scale;
    m_xscale = style.xscale;
    m_yscale = style.yscale;
    m_weight = style.weight;
    m_dx = style.text_dx;
    m_dy = style.text_dy;
    m_use_custom_rgb = style.use_custom_rgb;
    m_fill_r = style.fill_r;
    m_fill_g = style.fill_g;
    m_fill_b = style.fill_b;
    m_outline_width = style.outline_width;
    m_outline_r = style.outline_r;
    m_outline_g = style.outline_g;
    m_outline_b = style.outline_b;
    m_raised_bevel = style.raised_bevel;
    m_bevel_r = style.bevel_r;
    m_bevel_g = style.bevel_g;
    m_bevel_b = style.bevel_b;
    m_recessed = style.recessed;
    m_recess_dark_r = style.recess_dark_r;
    m_recess_dark_g = style.recess_dark_g;
    m_recess_dark_b = style.recess_dark_b;
    m_recess_light_r = style.recess_light_r;
    m_recess_light_g = style.recess_light_g;
    m_recess_light_b = style.recess_light_b;
    m_painted = style.painted;
    m_letter = style.letter;
    m_dirty = true;
}

int Label::GetWidth() {
    if (UseScaledRenderer(m_raw_text, m_scale, m_xscale, m_yscale, m_weight, m_use_custom_rgb, m_outline_width)) {
        m_cached_width = MeasureScaledText(m_raw_text, m_scale, m_xscale, m_weight, m_letter);
        m_dirty = false;
        return m_cached_width;
    }
    m_text.RefreshGlyphs();
    m_cached_width = m_text.Width();
    m_dirty = false;
    return m_cached_width;
}

void Label::Draw(int x, int y, int alpha) {
    int       draw_x = x + m_dx;
    const int draw_y = y + m_dy;
    const int width = GetWidth();

    if (UseScaledRenderer(m_raw_text, m_scale, m_xscale, m_yscale, m_weight, m_use_custom_rgb, m_outline_width)) {
        float center_x = static_cast<float>(draw_x);
        if (m_align == Alignment::Left) {
            center_x = static_cast<float>(draw_x) + static_cast<float>(width) * 0.5f;
        } else if (m_align == Alignment::Right) {
            center_x = static_cast<float>(draw_x) - static_cast<float>(width) * 0.5f;
        }
        const float cap_height = detail::kBaseCapHeight * m_scale;
        const float cap_top = static_cast<float>(draw_y) + 3.0f + (detail::kBaseCapHeight - cap_height) * 0.5f;

        const ScaledDrawParams params{
            .color = m_color,
            .weight = m_weight,
            .drop_shadow = m_drop_shadow,
            .use_custom_rgb = m_use_custom_rgb,
            .fill_r = m_fill_r,
            .fill_g = m_fill_g,
            .fill_b = m_fill_b,
            .outline_width = m_outline_width,
            .outline_r = m_outline_r,
            .outline_g = m_outline_g,
            .outline_b = m_outline_b,
            .vertical_grad = (m_weight == FontWeight::Status && !m_raised_bevel),
            .raised_bevel = m_raised_bevel,
            .bevel_r = m_bevel_r,
            .bevel_g = m_bevel_g,
            .bevel_b = m_bevel_b,
            .recessed = m_recessed,
            .recess_dark_r = m_recess_dark_r,
            .recess_dark_g = m_recess_dark_g,
            .recess_dark_b = m_recess_dark_b,
            .recess_light_r = m_recess_light_r,
            .recess_light_g = m_recess_light_g,
            .recess_light_b = m_recess_light_b,
            .painted = m_painted,
            .letter = m_letter,
        };
        DrawScaledText(m_raw_text, center_x, cap_top, m_scale, m_xscale, m_yscale, params, alpha);
        return;
    }

    if (m_align == Alignment::Center) {
        draw_x -= (width / 2);
    } else if (m_align == Alignment::Right) {
        draw_x -= width;
    }

    MenuTextureReload(detail::kMessageTextureBlock);
    detail::ScopedShadow shadow_guard(m_drop_shadow);
    m_text.SetColour(m_color);
    m_text.Draw(draw_x, draw_y, alpha);
}

void Label::DrawInRect(const Rect &box, int alpha) {
    if (UseScaledRenderer(m_raw_text, m_scale, m_xscale, m_yscale, m_weight, m_use_custom_rgb, m_outline_width)) {
        const int width = GetWidth();
        float     center_x = static_cast<float>(box.x) + static_cast<float>(box.w) * 0.5f;
        if (m_align == Alignment::Left) {
            center_x = static_cast<float>(box.x + 6) + static_cast<float>(width) * 0.5f;
        } else if (m_align == Alignment::Right) {
            center_x = static_cast<float>(box.x + box.w - 6) - static_cast<float>(width) * 0.5f;
        }
        const float cap_height = detail::kBaseCapHeight * m_scale;
        const float cap_top = static_cast<float>(box.y) + (static_cast<float>(box.h) - cap_height) * 0.5f - 1.0f;

        const ScaledDrawParams params{
            .color = m_color,
            .weight = m_weight,
            .drop_shadow = m_drop_shadow,
            .use_custom_rgb = m_use_custom_rgb,
            .fill_r = m_fill_r,
            .fill_g = m_fill_g,
            .fill_b = m_fill_b,
            .outline_width = m_outline_width,
            .outline_r = m_outline_r,
            .outline_g = m_outline_g,
            .outline_b = m_outline_b,
            .vertical_grad = (m_weight == FontWeight::Status && !m_raised_bevel),
            .raised_bevel = m_raised_bevel,
            .bevel_r = m_bevel_r,
            .bevel_g = m_bevel_g,
            .bevel_b = m_bevel_b,
            .recessed = m_recessed,
            .recess_dark_r = m_recess_dark_r,
            .recess_dark_g = m_recess_dark_g,
            .recess_dark_b = m_recess_dark_b,
            .recess_light_r = m_recess_light_r,
            .recess_light_g = m_recess_light_g,
            .recess_light_b = m_recess_light_b,
            .painted = m_painted,
            .letter = m_letter,
        };
        DrawScaledText(m_raw_text, center_x + static_cast<float>(m_dx), cap_top + static_cast<float>(m_dy),
                       m_scale, m_xscale, m_yscale, params, alpha);
        return;
    }

    const textwipe::MessageContext msg_ctx(textwipe::Kind::Message);

    const int text_w = GetWidth();
    int       draw_x = box.x + 6;
    if (m_align == Alignment::Center) {
        draw_x = box.x + (box.w - text_w) / 2;
    } else if (m_align == Alignment::Right) {
        draw_x = box.x + box.w - text_w - 6;
    }

    const int draw_y = box.y + (box.h - 22) / 2;

    MenuTextureReload(detail::kMessageTextureBlock);
    detail::ScopedShadow shadow_guard(m_drop_shadow);
    m_text.SetColour(m_color);
    m_text.Draw(draw_x + m_dx, draw_y + m_dy, alpha);
}

void Label::DrawCenteredInRect(int x, int y, int width, int height, int alpha) {
    const Alignment prev = m_align;
    m_align = Alignment::Center;
    DrawInRect(Rect(x, y, width, height), alpha);
    m_align = prev;
}

void Label::DrawAnchored(const Rect &parent, Anchor anchor, int offset_x, int offset_y, int alpha) {
    const Alignment prev_align = m_align;
    int             px = parent.x;
    int             py = parent.y;

    switch (anchor) {
        case Anchor::TopLeft:
        case Anchor::TopWide:
        case Anchor::LeftWide:
        case Anchor::FullRect:
            m_align = Alignment::Left;
            px = parent.Left();
            py = parent.Top();
            break;
        case Anchor::TopCenter:
            m_align = Alignment::Center;
            px = parent.CenterX();
            py = parent.Top();
            break;
        case Anchor::TopRight:
        case Anchor::RightWide:
            m_align = Alignment::Right;
            px = parent.Right();
            py = parent.Top();
            break;
        case Anchor::CenterLeft:
            m_align = Alignment::Left;
            px = parent.Left();
            py = parent.CenterY() - 10;
            break;
        case Anchor::Center:
            m_align = Alignment::Center;
            px = parent.CenterX();
            py = parent.CenterY() - 10;
            break;
        case Anchor::CenterRight:
            m_align = Alignment::Right;
            px = parent.Right();
            py = parent.CenterY() - 10;
            break;
        case Anchor::BottomLeft:
        case Anchor::BottomWide:
            m_align = Alignment::Left;
            px = parent.Left();
            py = parent.Bottom() - 20;
            break;
        case Anchor::BottomCenter:
            m_align = Alignment::Center;
            px = parent.CenterX();
            py = parent.Bottom() - 20;
            break;
        case Anchor::BottomRight:
            m_align = Alignment::Right;
            px = parent.Right();
            py = parent.Bottom() - 20;
            break;
    }

    Draw(px + offset_x, py + offset_y, alpha);
    m_align = prev_align;
}

void Label::DrawWithStroke(int x, int y, u32 fill_color, u32 stroke_color, int stroke_px, int alpha) {
    int       draw_x = x + m_dx;
    const int draw_y = y + m_dy;
    const int width = GetWidth();

    if (m_align == Alignment::Center) {
        draw_x -= (width / 2);
    } else if (m_align == Alignment::Right) {
        draw_x -= width;
    }

    MenuTextureReload(detail::kMessageTextureBlock);
    detail::ScopedShadow shadow_guard(m_drop_shadow);
    m_text.SetColour(stroke_color);
    for (int dy = -stroke_px; dy <= stroke_px; ++dy) {
        for (int dx = -stroke_px; dx <= stroke_px; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            m_text.Draw(draw_x + dx, draw_y + dy, alpha);
        }
    }

    m_text.SetColour(fill_color);
    m_text.Draw(draw_x, draw_y, alpha);
    m_text.Draw(draw_x + 1, draw_y, alpha);
}

void Label::DrawWrapped(int x, int y, int max_width, int line_height, int alpha) {
    std::vector<std::string> lines = WrapText(m_raw_text, max_width, m_scale);
    for (size_t i = 0; i < lines.size(); ++i) {
        Label line_lbl(lines[i], m_color, m_align, m_drop_shadow, m_scale, m_weight, m_xscale);
        line_lbl.SetOffset(m_dx, m_dy);
        line_lbl.Draw(x, y + static_cast<int>(i) * line_height, alpha);
    }
}

void DrawTextBox(std::string_view text, const Rect &box, const TextStyle &style, int alpha) {
    Label lbl(text, style);
    lbl.DrawInRect(box, alpha);
}

void DrawCounter(int x, int y, int current_val, int max_val,
                 float scale, int alpha, u32 normal_color, u32 complete_color) {
    const u32 col = (current_val == max_val && max_val > 0) ? complete_color : normal_color;
    Label("[", col, Alignment::Left, true, scale).Draw(x, y, alpha);
    Label(std::to_string(current_val), col, Alignment::Right, true, scale).Draw(x + 28, y, alpha);
    Label("/", col, Alignment::Center, true, scale).Draw(x + 38, y, alpha);
    Label(std::to_string(max_val), col, Alignment::Right, true, scale).Draw(x + 73, y, alpha);
    Label("]", col, Alignment::Right, true, scale).Draw(x + 82, y, alpha);
}

void DrawStatusText(std::string_view text, int x, int y, Alignment align,
                    u8 fill_r, u8 fill_g, u8 fill_b,
                    float scale, int alpha,
                    u8 out_r, u8 out_g, u8 out_b) {
    const TextStyle style{
        .drop_shadow = false,
        .align = align,
        .scale = scale,
        .xscale = 0.88f,
        .yscale = 0.94f,
        .weight = FontWeight::Status,
        .use_custom_rgb = true,
        .fill_r = fill_r,
        .fill_g = fill_g,
        .fill_b = fill_b,
        .outline_width = 1.45f,
        .outline_r = out_r,
        .outline_g = out_g,
        .outline_b = out_b,
        .raised_bevel = false,
    };
    Label(text, style).Draw(x, y, alpha);
}

void DrawStatusNumber(int value, int right_x, int y,
                      float scale, int alpha, bool highlight_red) {
    const TextStyle style{
        .drop_shadow = false,
        .align = Alignment::Right,
        .scale = scale * 1.12f,
        .xscale = 1.04f,
        .yscale = 0.78f,
        .weight = FontWeight::Status,
        .use_custom_rgb = true,
        .fill_r = highlight_red ? static_cast<u8>(255) : static_cast<u8>(242),
        .fill_g = highlight_red ? static_cast<u8>(120) : static_cast<u8>(248),
        .fill_b = highlight_red ? static_cast<u8>(105) : static_cast<u8>(255),
        .outline_width = 1.55f,
        .outline_r = highlight_red ? static_cast<u8>(52) : static_cast<u8>(10),
        .outline_g = highlight_red ? static_cast<u8>(10) : static_cast<u8>(18),
        .outline_b = highlight_red ? static_cast<u8>(10) : static_cast<u8>(46),
        .raised_bevel = true,
        .bevel_r = highlight_red ? static_cast<u8>(175) : static_cast<u8>(108),
        .bevel_g = highlight_red ? static_cast<u8>(55) : static_cast<u8>(134),
        .bevel_b = highlight_red ? static_cast<u8>(55) : static_cast<u8>(192),
    };
    Label(std::to_string(value), style).Draw(right_x, y, alpha);
}

void DrawStatusSlashPair(int current_val, int max_val, int slash_x, int y,
                         float scale, int alpha) {
    DrawStatusNumber(current_val, slash_x - 2, y, scale, alpha, false);

    TextStyle slash_style{
        .drop_shadow = false,
        .align = Alignment::Center,
        .scale = scale * 1.05f,
        .xscale = 1.0f,
        .yscale = 0.78f,
        .weight = FontWeight::Status,
        .use_custom_rgb = true,
        .fill_r = 242,
        .fill_g = 248,
        .fill_b = 255,
        .outline_width = 1.55f,
        .outline_r = 10,
        .outline_g = 18,
        .outline_b = 46,
        .raised_bevel = true,
        .bevel_r = 108,
        .bevel_g = 134,
        .bevel_b = 192,
    };
    Label("/", slash_style).Draw(slash_x + 3, y, alpha);

    slash_style.align = Alignment::Left;
    slash_style.scale = scale * 1.12f;
    slash_style.xscale = 1.04f;
    Label(std::to_string(max_val), slash_style).Draw(slash_x + 8, y, alpha);
}

} // namespace UI
