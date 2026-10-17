#include "menu_draw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "caption_text.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "memcard.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_pointer.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "name_mouse.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "textwipe.hpp"

extern PERSONAL_BOARD *PerBoardPt;

PORT_OVERRIDE u_long128 *MenuCalcBufAlignment(u_long128 *buffer) {
    // Align the buffer address to a 64-byte boundary across the full 64-bit address space.
    auto address = reinterpret_cast<std::uintptr_t>(buffer);
    return reinterpret_cast<u_long128 *>((address + 63) & ~std::uintptr_t{63});
}

PORT_OVERRIDE void PersonalBoardDrawWaku(int x, int y, CTexture *texture, int alpha) {
    // Draw the top strip at 1:1 texel height so the frame border meets the clipped grid at y=129.
    DrawMenu2DSprite(texture, CRect_i_(x, y + 1, 0x14, 0xBF), CRect_i_(0, 0, 0x14, 0xC0), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x14, y + 1, 0xC8, 0x14), CRect_i_(0x14, 0, 0xC8, 0x14), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0xDC, y + 1, 0x24, 0xBF), CRect_i_(0xDC, 0, 0x24, 0xC0), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x + 0x14, y + 0xA1, 0xC8, 0x1F), CRect_i_(0x14, 0xA0, 0xC8, 0x20), alpha);
}

PORT_OVERRIDE void DrawFullSizePicture(CTexture *texture, int x, int y, int alpha) {
    if (texture == nullptr) {
        return;
    }
    // Stretch full-screen backdrops across the visible logical target to fill widescreen viewports.
    gfx::LogicalRect visible = gfx::VisibleLogicalRect(gfx::kMainTarget);
    int              left    = x + static_cast<int>(std::floor(visible.x));
    int              top     = y + static_cast<int>(std::floor(visible.y));
    int              right   = x + static_cast<int>(std::ceil(visible.x + visible.w));
    int              bottom  = y + static_cast<int>(std::ceil(visible.y + visible.h));
    DrawMenu2DSprite(texture, CRect_i_(left, top, right - left, bottom - top), MenuDispRc, alpha);
}

PORT_OVERRIDE void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, unsigned char r, unsigned char g,
                                    unsigned char b, int alpha) {
    // Clamp 1:1 sprite source heights so adjacent atlas rows are not sampled when scaled.
    if (screen.width > 0 && screen.height > 0 &&
        texel.width == screen.width &&
        (texel.height == screen.height + 1 || texel.height == screen.height + 2)) {
        texel.height = screen.height;
        if (screen.width == 0x100 && screen.height == 0x78 && texel.x == 0 && texel.y == 0x78) {
            screen.height = 0x77;
            texel.height  = 0x77;
        }
    }

    NameMouseHand(texture, screen, texel, r == 0 && g == 0 && b == 0);
    set2DSprite(GetVif1Packet(), texture, screen, texel, r, g, b, alpha);
}

PORT_OVERRIDE void DrawMainMenuIcon(int x, int y, int icon, int selected, int bright, int alpha) {
    if (StayTex == nullptr) {
        StayTex = TexManager.GetTexture(const_cast<char *>("stayframe"), -1);
        if (StayTex == nullptr) {
            return;
        }
    }
    MENU_ICON_INFO *info = GetMenuIconInfo(icon);
    if (info == nullptr) {
        return;
    }

    int u = info->u;
    int v = info->v;
    s16 frame_size[2][2] = {
        {0x30, 0x20},
        {0x3A, 0x28}
    };
    int width  = frame_size[selected][0];
    int height = frame_size[selected][1];
    if (selected) {
        u = info->selected_u;
        v = info->selected_v;
        if (icon == info->id) {
            width = 0x38;
        }
    }
    set2DSprite(GetVif1Packet(), StayTex, CRect_i_(x, y, width, height - 1), CRect_i_(u, v, width, height), bright, bright, bright, alpha);

    if (selected) {
        x += 0x44;
        y += 1;
    } else {
        x += 0x40;
        y -= 1;
    }
    auto painted = [&](CTexture *texture) {
        set2DSprite(GetVif1Packet(), texture, CRect_i_(x, y, info->label_width, info->label_height - 1), CRect_i_(info->label_u, info->label_v, info->label_width, info->label_height), bright, bright, bright, alpha);
    };
    if (caption::Ready()) {
        textwipe::Both([&](bool runtime) {
            if (!runtime || !caption::Draw(icon, x, y, info->label_width, alpha, bright)) {
                painted(StayTex);
            }
        });
        return;
    }
    painted(StayTex);
}

PORT_OVERRIDE void DrawMenuVibeItem(int x, int y, int offset_x, int offset_y, int alpha) {
    float pointer_x = 0.0f;
    float pointer_y = 0.0f;

    if (MenuPointerHand(pointer_x, pointer_y)) {
        x = static_cast<int>(pointer_x);
        y = static_cast<int>(pointer_y);
    }

    int       item_x  = x + offset_x;
    int       item_y  = y + offset_y;
    s16       item_no = PerBoardPt->held_item.item_no;
    CRect_i_  shadow(0x80, 0x28, 0x20, 0x20);
    int       u       = 0;
    int       v       = 0;
    CTexture *texture = RetCTex(item_no, u, v);

    if (texture != nullptr) {
        DrawObjectVibe(x + 4, y + 2, TexManager.GetTexture(const_cast<char *>("StayTex"), -1), shadow, 0, 0x50);
        CRect_i_ source(u, v, 0x20, 0x20);
        DrawObjectVibe(item_x + 4, item_y + 2, texture, source, 0, 0x50);
        DrawObjectVibe(item_x, item_y, texture, source, 0x80, 0x80);
        int number = GetAttachVolumeForMsg(&PerBoardPt->held_attach);

        if (item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
            number = PerBoardPt->held_attach.sphere_weapon_no;
        }

        DrawAttachNumberOrWeapon(item_x, item_y, 0, 0x280, item_no, number, 0x80, 1);
    }
}