#pragma once

// battlemenu.hpp and memcard.hpp for the game's own menu drawing: its colour gradients and the
// menus' frame (DrawMenuWaku).
#include "battlemenu.hpp"
#include "draw2d_port.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "platform/config.hpp"
#include "texture.hpp"
#include "ui_framework.hpp"

/**
 * Texture name of the shared menu frame and cursor sprite sheet.
 */
extern char AtoraVibeTextureName[];

/**
 * Texture holding standard inventory item icons.
 */
extern CTexture *ItemIcon;

/**
 * Texture holding weapon inventory icons.
 */
extern CTexture *WepIcon;

/**
 * Texture holding shared menu frames, numbers, and cursors.
 */
extern CTexture *StayTex;

/**
 * Texture holding the personal inventory board frame.
 */
extern CTexture *PerBoardTex;

/**
 * Texture holding weapon status bars, tags, and banners.
 */
extern CTexture *WepStatus;

/**
 * Draws the 9-slice translucent help window frame.
 */
extern void MenuHelpWinDraw(int x, int y, float w, float h, int alpha);

/**
 * Sets whether textures are sampled with bilinear filtering.
 */
extern void setbilinear(int n);

namespace UI::detail {

/**
 * Default capital letter height in logical pixels at unit scale.
 */
inline constexpr float kBaseCapHeight = 14.5f;

/**
 * Texture block index used by system message windows.
 */
inline constexpr int kMessageTextureBlock = 0x1A;

/**
 * Temporary override that disables global text drop shadows while in scope.
 */
struct ScopedShadow {
    int  saved_shadow = 0; /**< Previous text shadow configuration value. */
    bool active = false;   /**< Whether the shadow configuration was modified. */

    /**
     * Suppresses global text drop shadows when enable_shadow is false.
     */
    explicit ScopedShadow(bool enable_shadow) {
        if (!enable_shadow) {
            Config &cfg = const_cast<Config &>(ConfigGet());
            saved_shadow = cfg.text_shadow;
            cfg.text_shadow = 0;
            active = true;
        }
    }

    /**
     * Restores the previous text shadow setting upon destruction.
     */
    ~ScopedShadow() {
        if (active) {
            Config &cfg = const_cast<Config &>(ConfigGet());
            cfg.text_shadow = saved_shadow;
        }
    }
};

/**
 * Finds a loaded menu texture by name and ensures its texture block is resident.
 */
inline CTexture *FindMenuTexture(const char *name, CTexture *cached = nullptr) {
    if (cached != nullptr) {
        if (cached->block >= 0) {
            MenuTextureReload(cached->block);
        }
        return cached;
    }
    CTexture *tex = draw2d::Get().find_texture(name);
    if (tex == nullptr) {
        tex = TexManager.GetTexture(const_cast<char *>(name), -1);
    }
    if (tex != nullptr && tex->block >= 0) {
        MenuTextureReload(tex->block);
    }
    return tex;
}

/**
 * Returns the gaiji font texture containing a solid white texel for untextured fills.
 */
inline CTexture *GetWhitePixelTex() {
    return FindMenuTexture("gaiji");
}

/**
 * Returns the personal board texture and ensures its block is resident.
 */
inline CTexture *GetPerBoardTex() {
    return FindMenuTexture("perbrd", PerBoardTex);
}

/**
 * Returns the weapon status texture and ensures its block is resident.
 */
inline CTexture *GetWepStatusTex() {
    return FindMenuTexture("wepstatus", WepStatus);
}

/**
 * Draws a filled rectangle with optional 2-pixel stepped corner rounding.
 */
inline void DrawRoundedRect(CTexture *tex, int x, int y, int w, int h,
                            unsigned char r, unsigned char g, unsigned char b, int alpha, bool rounded) {
    if (tex == nullptr || w <= 0 || h <= 0 || alpha <= 0) {
        return;
    }
    const CRect_i_ uv_white(0x6A, 0x6E, 1, 1);

    if (!rounded || w < 6 || h < 6) {
        DrawMenu2DSprite(tex, CRect_i_(x, y, w, h), uv_white, r, g, b, alpha);
        return;
    }

    DrawMenu2DSprite(tex, CRect_i_(x + 2, y, w - 4, 1), uv_white, r, g, b, alpha);
    DrawMenu2DSprite(tex, CRect_i_(x + 1, y + 1, w - 2, 1), uv_white, r, g, b, alpha);
    DrawMenu2DSprite(tex, CRect_i_(x, y + 2, w, h - 4), uv_white, r, g, b, alpha);
    DrawMenu2DSprite(tex, CRect_i_(x + 1, y + h - 2, w - 2, 1), uv_white, r, g, b, alpha);
    DrawMenu2DSprite(tex, CRect_i_(x + 2, y + h - 1, w - 4, 1), uv_white, r, g, b, alpha);
}

/**
 * Converts a UI color constant into 8-bit RGB channels.
 */
inline void ColorToRgb(u32 color, unsigned char (&out)[3]) {
    switch (color) {
        case ColorGold:
            out[0] = 235;
            out[1] = 192;
            out[2] = 75;
            break;
        case ColorYellow:
            out[0] = 240;
            out[1] = 232;
            out[2] = 120;
            break;
        case ColorBlue:
            out[0] = 110;
            out[1] = 175;
            out[2] = 255;
            break;
        case ColorWhite:
        default:
            out[0] = 240;
            out[1] = 238;
            out[2] = 232;
            break;
    }
}

} // namespace UI::detail
