#pragma once

#include <string_view>

#include "texture.hpp"
#include "ui_framework/text.hpp"
#include "ui_framework/types.hpp"

namespace UI {

/**
 * Procedural surface material presets for beveled UI buttons.
 */
enum class ButtonPreset {
    Terracotta,
    AmberGold,
    StoneGreen,
    SlateBlue,
};

/**
 * Visual and typographic styling parameters for a beveled button.
 */
struct ButtonStyle {
    ButtonPreset preset = ButtonPreset::Terracotta; /**< Surface material preset. */
    u32          text_color = ColorWhite;           /**< Fallback palette text color. */
    bool         drop_shadow = false;               /**< Whether to draw a standard text drop shadow. */
    bool         recessed_text = true;              /**< Whether to engrave the label into the button surface. */
    bool         painted_text = false;              /**< Whether to letter the label as the painted buttons are. */
    bool         pressed = false;                   /**< Lit from below (the plate turned round), its painted lettering glowing light. */
    Alignment    align = Alignment::Center;         /**< Horizontal label alignment inside the button. */
    float        text_scale = 1.01f;                /**< Uniform label font scale factor. */
    float        text_xscale = 1.20f;               /**< Horizontal glyph stretch factor. */
    FontWeight   text_weight = FontWeight::Normal;  /**< Label font weight variant. */
    bool         rounded = true;                    /**< Whether the button corners are rounded. */
    int          brightness = 0x80;                 /**< Surface brightness modulation (0x80 = neutral). */
    int          text_dx = 0;                       /**< Horizontal pixel offset for the label. */
    int          text_dy = 0;                       /**< Vertical pixel offset for the label. */

    /**
     * Returns the wide, thick-font button style used by YES/NO confirmation modals.
     */
    static constexpr ButtonStyle Thick(ButtonPreset p = ButtonPreset::Terracotta, int bright = 0x80) {
        return ButtonStyle{
            .preset = p,
            .text_color = ColorWhite,
            .drop_shadow = false,
            .recessed_text = true,
            .align = Alignment::Center,
            .text_scale = 1.01f,
            .text_xscale = 1.20f,
            .text_weight = FontWeight::Normal,
            .rounded = true,
            .brightness = bright,
        };
    }

    /**
     * Returns the slender-font button style used by Weapon Menu action buttons.
     */
    static constexpr ButtonStyle Thin(ButtonPreset p = ButtonPreset::Terracotta, int bright = 0x80) {
        return ButtonStyle{
            .preset = p,
            .text_color = ColorWhite,
            .drop_shadow = false,
            .recessed_text = false,
            .painted_text = true,
            .align = Alignment::Center,
            .text_scale = 1.10f,
            .text_xscale = 1.32f,
            .text_weight = FontWeight::Thin,
            .rounded = true,
            .brightness = bright,
            .text_dx = 1,
            .text_dy = 1,
        };
    }
};

/**
 * Procedural 3D beveled button renderer with engraved text support.
 */
class BevelButton {
public:
    /**
     * Draws a beveled button inside the specified rectangle using a ButtonStyle descriptor.
     */
    static void Draw(const Rect &box, std::string_view text,
                     const ButtonStyle &style = {}, int alpha = 0x80);

    /**
     * Draws a beveled button at (x, y) with explicit dimensions, preset, and font weight.
     */
    static void Draw(int x, int y, int width, int height, std::string_view text,
                     u32 text_color = ColorWhite, int alpha = 0x80,
                     ButtonPreset preset = ButtonPreset::Terracotta,
                     Alignment    align = Alignment::Center,
                     bool         drop_shadow = false,
                     FontWeight   weight = FontWeight::Normal);
};

/**
 * Draws a painted plate the game takes whole from a texture (btstatus2's YES, NO and DUMP, the fishing points exchange's
 * YES and NO) as the weapon menu's buttons
 * are drawn instead, the plate procedural and its word from the font. False for any other sprite.
 */
bool DrawPaintedPlate(const char *texture, const CRect_i_ &screen, const CRect_i_ &texel, int bright, int alpha);

/**
 * Sprite-backed button renderer with optional TrueType text overlay.
 */
class TextureButton {
public:
    /**
     * Draws a textured button inside box sampling from uv with an optional text label.
     */
    static void Draw(const Rect &box, CTexture *texture, const Rect &uv,
                     int alpha = 0x80, std::string_view text = {},
                     const TextStyle &text_style = {});

    /**
     * Draws a textured button at (x, y) with explicit UV coordinates and an optional text label.
     */
    static void Draw(int x, int y, int width, int height,
                     CTexture *texture, int u, int v, int uv_w, int uv_h,
                     int alpha = 0x80, std::string_view text = {},
                     const TextStyle &text_style = {});
};

} // namespace UI
