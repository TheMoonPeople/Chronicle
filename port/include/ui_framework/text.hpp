#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "clsmes.hpp"
#include "gametext.hpp"
#include "ui_framework/types.hpp"

namespace UI {

/**
 * Styling parameters for TrueType and bitmap UI text labels.
 */
struct TextStyle {
    u32        color = ColorWhite;          /**< Palette color constant when custom RGB is disabled. */
    bool       drop_shadow = true;          /**< Whether to render a drop shadow behind the text. */
    Alignment  align = Alignment::Center;   /**< Horizontal alignment mode. */
    float      scale = 1.0f;                /**< Uniform font scale multiplier. */
    float      xscale = 1.0f;               /**< Horizontal glyph stretch multiplier. */
    float      yscale = 1.0f;               /**< Vertical glyph stretch multiplier. */
    FontWeight weight = FontWeight::Normal; /**< Font weight and slot variant. */
    int        line_height = 24;            /**< Vertical line spacing for wrapped text in pixels. */
    int        text_dx = 0;                 /**< Horizontal pixel offset applied before drawing. */
    int        text_dy = 0;                 /**< Vertical pixel offset applied before drawing. */
    bool       use_custom_rgb = false;      /**< Whether to use explicit RGB fill channels. */
    u8         fill_r = 240;                /**< Custom fill red channel. */
    u8         fill_g = 238;                /**< Custom fill green channel. */
    u8         fill_b = 232;                /**< Custom fill blue channel. */
    float      outline_width = 0.0f;        /**< Outer stroke width in logical pixels. */
    u8         outline_r = 16;              /**< Outer stroke red channel. */
    u8         outline_g = 28;              /**< Outer stroke green channel. */
    u8         outline_b = 68;              /**< Outer stroke blue channel. */
    bool       raised_bevel = false;        /**< Whether to render a raised 3D bevel extrusion pass. */
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
    bool       painted = false;             /**< Whether to letter as the painted buttons are: no outline, a soft brown shadow. */
    float      letter = 0.0f;               /**< Added to the gap between letters, in logical pixels. */
    bool       see_through = false;         /**< Whether alpha makes the text see-through, as retail's sprites fade, instead of darkening it. */
    float      halo = 0.0f;                 /**< How far a soft dark halo reaches out from the letters, fading as it goes, in logical pixels (0 for none). */
    float      halo_strength = 0.5f;        /**< How dark the halo is next to the letters, 0 to 1 (1 is all but black there). */
    float      kern = 0.0f;                 /**< The font's kerning between letters, at this strength (0 for none). */
    bool       ink_spacing = false;         /**< Whether letters are spaced by their outlines, the same at any window size, instead of their pictures. */
};

/**
 * Splits a UTF-8 string into lines that fit within max_width logical pixels at the given scale.
 */
std::vector<std::string> WrapText(std::string_view text, int max_width, float scale = 1.0f);

/**
 * Single-line or multi-line UI text label supporting TrueType scaling and special effects.
 */
class Label {
public:
    /**
     * Constructs an empty label with default styling.
     */
    Label() = default;

    /**
     * Constructs a label with explicit color, alignment, shadow, scale, and weight parameters.
     */
    explicit Label(std::string_view text, u32 color = ColorWhite,
                   Alignment align = Alignment::Left, bool drop_shadow = true,
                   float scale = 1.0f, FontWeight weight = FontWeight::Normal,
                   float xscale = 1.0f);

    /**
     * Constructs a label configured from a TextStyle descriptor.
     */
    Label(std::string_view text, const TextStyle &style);

    /**
     * Updates the label's UTF-8 text string.
     */
    void SetText(std::string_view text);

    /**
     * Sets the palette color constant and disables custom RGB fill.
     */
    void SetColor(u32 color);

    /**
     * Enables custom RGB fill with the specified 8-bit channels.
     */
    void SetCustomRgb(u8 r, u8 g, u8 b);

    /**
     * Sets the outer stroke width and RGB color.
     */
    void SetOutline(float width, u8 r = 16, u8 g = 28, u8 b = 68);

    /**
     * Sets the horizontal alignment mode.
     */
    void SetAlignment(Alignment align);

    /**
     * Enables or disables the drop shadow pass.
     */
    void SetDropShadow(bool enabled);

    /**
     * Sets the uniform font scale multiplier.
     */
    void SetScale(float scale);

    /**
     * Sets the horizontal glyph stretch multiplier.
     */
    void SetXScale(float xscale);

    /**
     * Sets the vertical glyph stretch multiplier.
     */
    void SetYScale(float yscale);

    /**
     * Sets the font weight and slot variant.
     */
    void SetWeight(FontWeight weight);

    /**
     * Sets the pixel offset applied to all draw calls.
     */
    void SetOffset(int dx, int dy);

    /**
     * Applies all styling properties from a TextStyle descriptor.
     */
    void SetStyle(const TextStyle &style);

    /**
     * Lays the text out without drawing on the game's random numbers (GameText::KeepGameRandom),
     * for a label laid out where the retail game lays nothing out. Set it before the text.
     */
    void KeepGameRandom(bool keep) { m_text.KeepGameRandom(keep); }

    /**
     * Returns the raw UTF-8 text string.
     */
    const std::string &GetText() const { return m_raw_text; }

    /**
     * Measures and returns the rendered width of the label in logical pixels.
     */
    int GetWidth();

    /**
     * Draws the label at the logical coordinates (x, y).
     */
    void Draw(int x, int y, int alpha = 0x80);

    /**
     * Draws the label vertically centered and horizontally aligned inside a bounding rectangle.
     */
    void DrawInRect(const Rect &box, int alpha = 0x80);

    /**
     * Draws the label centered both horizontally and vertically inside the given bounds.
     */
    void DrawCenteredInRect(int x, int y, int width, int height, int alpha = 0x80);

    /**
     * Draws the label anchored to a region of a parent rectangle.
     */
    void DrawAnchored(const Rect &parent, Anchor anchor, int offset_x = 0, int offset_y = 0, int alpha = 0x80);

    /**
     * Draws the label with a multi-pass bitmap stroke outline.
     */
    void DrawWithStroke(int x, int y, u32 fill_color, u32 stroke_color, int stroke_px = 2, int alpha = 0x80);

    /**
     * Word-wraps and draws the label across multiple lines within max_width.
     */
    void DrawWrapped(int x, int y, int max_width, int line_height = 24, int alpha = 0x80);

private:
    std::string m_raw_text;                   /**< Raw UTF-8 text string. */
    GameText    m_text;                       /**< Underlying message decoder and glyph layout instance. */
    u32         m_color{ColorWhite};          /**< Active palette color constant. */
    Alignment   m_align{Alignment::Left};     /**< Horizontal alignment mode. */
    bool        m_drop_shadow{true};          /**< Whether drop shadows are enabled. */
    float       m_scale{1.0f};                /**< Uniform scale factor. */
    float       m_xscale{1.0f};               /**< Horizontal stretch multiplier. */
    float       m_yscale{1.0f};               /**< Vertical stretch multiplier. */
    FontWeight  m_weight{FontWeight::Normal}; /**< Active font weight variant. */
    int         m_dx{0};                      /**< Horizontal draw offset in pixels. */
    int         m_dy{0};                      /**< Vertical draw offset in pixels. */
    bool        m_use_custom_rgb{false};      /**< Whether custom RGB fill is active. */
    u8          m_fill_r{240};                /**< Custom fill red channel. */
    u8          m_fill_g{238};                /**< Custom fill green channel. */
    u8          m_fill_b{232};                /**< Custom fill blue channel. */
    float       m_outline_width{0.0f};        /**< Outer stroke width in pixels. */
    u8          m_outline_r{16};              /**< Outer stroke red channel. */
    u8          m_outline_g{28};              /**< Outer stroke green channel. */
    u8          m_outline_b{68};              /**< Outer stroke blue channel. */
    bool        m_raised_bevel{false};        /**< Whether 3D bevel extrusion is enabled. */
    u8          m_bevel_r{108};               /**< Raised bevel red channel. */
    u8          m_bevel_g{134};               /**< Raised bevel green channel. */
    u8          m_bevel_b{192};               /**< Raised bevel blue channel. */
    bool        m_recessed{false};            /**< Whether recessed button engraving is enabled. */
    u8          m_recess_dark_r{72};          /**< Engraved top-left shadow red channel. */
    u8          m_recess_dark_g{34};          /**< Engraved top-left shadow green channel. */
    u8          m_recess_dark_b{18};          /**< Engraved top-left shadow blue channel. */
    u8          m_recess_light_r{250};        /**< Engraved bottom-right catch-light red channel. */
    u8          m_recess_light_g{195};        /**< Engraved bottom-right catch-light green channel. */
    u8          m_recess_light_b{155};        /**< Engraved bottom-right catch-light blue channel. */
    bool        m_painted{false};             /**< Whether the painted buttons' lettering is used. */
    float       m_letter{0.0f};               /**< Added to the gap between letters. */
    bool        m_see_through{false};         /**< Whether alpha makes the text see-through. */
    float       m_halo{0.0f};                 /**< Reach of the soft dark halo. */
    float       m_halo_strength{0.5f};        /**< Darkness of the halo next to the letters. */
    float       m_kern{0.0f};                 /**< Strength of the font's kerning. */
    bool        m_ink_spacing{false};         /**< Whether letters are spaced by their outlines. */
    bool        m_dirty{true};                /**< Whether the cached width needs recomputation. */
    int         m_cached_width{0};            /**< Cached horizontal width in logical pixels. */
};

/**
 * Draws a single-line styled label inside a bounding rectangle.
 */
void DrawTextBox(std::string_view text, const Rect &box, const TextStyle &style = {}, int alpha = 0x80);

/**
 * Draws a bracketed "[current/max]" numeric counter at (x, y).
 */
void DrawCounter(int x, int y, int current_val, int max_val,
                 float scale = Metrics::kScaleBody, int alpha = 0x80,
                 u32 normal_color = ColorWhite, u32 complete_color = ColorGold);

/**
 * Draws a weapon status abbreviation label with a vertical highlight gradient and dark outline.
 */
void DrawStatusText(std::string_view text, int x, int y, Alignment align,
                    u8 fill_r, u8 fill_g, u8 fill_b,
                    float scale = 0.78f, int alpha = 0x80,
                    u8 out_r = 14, u8 out_g = 26, u8 out_b = 64);

/**
 * Draws a right-aligned weapon status number with a raised 3D slate-blue bevel.
 */
void DrawStatusNumber(int value, int right_x, int y,
                      float scale = 0.78f, int alpha = 0x80,
                      bool highlight_red = false);

/**
 * Draws a "current/max" weapon status numeric pair centered around slash_x.
 */
void DrawStatusSlashPair(int current_val, int max_val, int slash_x, int y,
                         float scale = 0.76f, int alpha = 0x80);

} // namespace UI
