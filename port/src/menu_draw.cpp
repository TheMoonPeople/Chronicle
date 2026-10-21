#include "menu_draw.hpp"

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "caption_text.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "itemdata.hpp"
#include "localize.hpp"
#include "memcard.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_pointer.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "name_mouse.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "textwipe.hpp"

extern PERSONAL_BOARD *PerBoardPt;
extern CTexture       *DunLogBoard;

namespace {

// The boss floors' names on the floor-select board, a row each of a 220x24 strip of the board's sheet, drawn in the
// text font (English only: the port has no other language's names for them): where each painted name starts, from
// the strip's left, and how much the painting squeezed it.
struct BoardName {
    const char                       *english;
    float                             left;
    float                             squeeze;
    std::vector<caption::BoardAdjust> adjust; // where this name's painting departs from the others'
};

const BoardName kBoardNames[] = {
    {"DIVINE BEAST DRAN'S CHAMBER", 5.02f, 0.636f, {{U'B', 0.0f, 0.0f, 0.0f, -0.6f}, {U'R', 0.0f, 0.0f, 0.0f, -0.6f, 0}}},
    {"DWELLING OF FOREST GUARD", 4.10f, 0.714f, {{U'R', 0.0f, -1.2f}, {U'D', -1.4f, 0.0f}, {U'N', 1.6f, -1.6f}, {U'A', 0.9f, 0.0f, 0.0f, -1.0f}}},
    {"CHURCH OF MEMORY", 3.57f, 0.954f, {{U'R', 0.0f, -2.0f, -2.0f}, {U'H', 0.0f, 0.5f}, {U'U', 0.0f, 0.0f, 0.8f}}},
    {"COFFIN'S CHAMBER", 3.00f, 0.978f, {{U'N', 0.8f, -0.8f}, {U'S', -0.8f, 0.8f}, {U' ', 0.0f, 0.0f, 2.0f}, {U'R', 0.0f, -2.2f}, {U'E', 0.0f, -0.8f}}},
    {"COLISEUM", 3.70f, 0.944f, {{U'C', 0.8f, -0.8f}, {U'S', 0.8f, -0.8f}, {U'U', -0.8f, 0.8f}}},
    {"NIGHT OF INCIDENT", 3.26f, 1.000f, {{U'I', 0.0f, 0.0f, 0.0f, -0.8f}, {U'I', 0.0f, 0.0f, 0.0f, 0.8f, 1}, {U'G', -0.8f, 0.8f}, {U'C', 0.0f, 0.0f, 0.0f, -1.2f}, {U'D', -0.6f, 0.0f, 0.0f, 0.6f}, {U'E', 0.0f, -1.4f}, {U'N', 0.0f, 0.0f, 0.0f, -1.4f, 2}}},
    {"TOP FLOOR", 4.46f, 0.956f, {{U'O', 0.0f, -0.6f}, {U'P', 1.6f, 0.0f}, {U'R', 0.0f, -2.2f}, {U'O', 0.0f, 0.0f, 0.0f, 0.6f, 1}, {U'O', 0.0f, 0.0f, 0.0f, 0.6f, 2}}},
};

bool DrawBoardName(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || texture != DunLogBoard || texel.x != 0xFC || texel.width != 0xDC || texel.y < 0x20 ||
        texel.height <= 0 || texel.height > 0x18 || !caption::Ready() || LocalizeLanguageFile().rfind("en_", 0) != 0) {
        return false;
    }
    // The row the strip is cut from; the list clips a row at its edges by starting the strip lower or ending it sooner.
    const int row = (texel.y - 0x20) / 0x18;
    if (row >= static_cast<int>(std::size(kBoardNames)) || texel.y + texel.height > 0x20 + (row + 1) * 0x18) {
        return false;
    }
    const float      row_top = static_cast<float>(screen.y - (texel.y - (0x20 + row * 0x18)));
    const BoardName &name = kBoardNames[row];
    return caption::DrawBoardName(name.english, static_cast<float>(screen.x) + name.left, row_top + 4.17f, name.squeeze,
                                  name.adjust, static_cast<float>(screen.y), static_cast<float>(screen.y + screen.height),
                                  alpha);
}

// The dungeon-entry list's floor labels, in the 0x70 by 0x1C rows of the sheet "kaisou" at (u, v) the list cuts each
// dungeon's from (English: the other languages' painted words stay): each word's painted letters' box in its row (or,
// by_capitals, from where they run from and to, their capitals' top and baseline).
struct FloorLabel {
    int         u, v;
    const char *english;
    float       left, top, right, bottom;
    std::vector<caption::FloorAdjust> adjust;      // where this word's painting departs from the font's spacing
    bool                              by_capitals{}; // top and bottom are the capitals' top and the baseline
    float                             squeeze{};     // its letters this much as wide as tall as the font's, from left
};

const FloorLabel kFloorLabels[] = {
    {0x00, 0x00, "B",         29.0f, 5.0f, 43.4f,  23.2f},
    {0x80, 0x00, "Floor",     17.0f, 4.75f, 62.4f, 22.7f},
    {0x00, 0x20, "Territory", 6.0f,  4.0f, 69.4f,  24.2f, {{0, 0.6f}, {1, 0.0f, -2.5f}}},
    // Layer by its capitals: 18 pixels tall, half a pixel above Territory's baseline.
    {0x00, 0x40, "Layer",     16.0f, 4.4f, 62.4f,  22.4f, {}, true},
    // Level as Layer, its letters as wide, its L where the painting's is, the rest a little further on, the v on the line
    // (the font's dips below it).
    {0x00, 0x60, "Level",     16.0f, 4.4f, 60.4f,  22.4f, {{1, 0.0f, 0.6f}, {2, 0.0f, 1.0f, 0.5f}, {3, 0.0f, 0.2f}, {4, 0.0f, 0.8f}}, true, 0.852f},
    {0x00, 0x80, "No.",       3.0f,  5.0f, 20.4f,  22.9f, {{0, 0.25f}, {1, 0.25f, -0.25f}, {2, 0.0f, -0.55f, -3.15f}}}, // its N and o a little wider, its stop down by the o
    {0x00, 0x80, "Craters",   58.0f, 5.0f, 109.4f, 22.9f},
    {0x00, 0xA0, "Years ago", 50.0f, 4.75f, 110.4f, 24.2f, {{1, 0.0f, -1.1f}, {3, 0.0f, 0.25f}, {4, 0.0f, -0.5f, 0.25f}, {5, 0.0f, 0.5f}, {6, 0.0f, 0.0f, -1.1f}}}, // its g's top level with the a's
};

bool DrawFloorLabels(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || std::string_view(texture->name) != "kaisou" || texel.width != 0x70 || texel.height <= 0 ||
        texel.height > 0x1C || texel.y >= 0xC0 || !caption::Ready() || LocalizeLanguageFile().rfind("en_", 0) != 0) {
        return false;
    }
    // The row the strip is cut from; the list clips a row at its edges by starting the strip lower or ending it sooner.
    const int row_v = texel.y / 0x20 * 0x20;
    if (texel.y + texel.height > row_v + 0x1C) {
        return false;
    }
    const float left = static_cast<float>(screen.x), row_top = static_cast<float>(screen.y - (texel.y - row_v));
    bool        drew = false;
    for (const FloorLabel &label : kFloorLabels) {
        if (label.u == texel.x && label.v == row_v) {
            drew = caption::DrawFloorLabel(label.english, left + label.left, row_top + label.top, left + label.right,
                                           row_top + label.bottom, static_cast<float>(screen.y),
                                           static_cast<float>(screen.y + screen.height), alpha, label.adjust, label.by_capitals,
                                           label.squeeze) ||
                   drew;
        }
    }
    return drew;
}

// The name entry's painted title, REGISTER NAME (0xAE by 0x20 at 0xC0, 0x178 of "nametemp"), in the captions' look.
bool DrawNameTitle(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || std::string_view(texture->name) != "nametemp" || texel.x != 0xC0 || texel.y != 0x178 ||
        texel.width != 0xAE || texel.height != 0x20 || LocalizeLanguageFile().rfind("en_", 0) != 0) {
        return false;
    }
    // The painted letters run from about 3 to 164 of it, their capitals from row 8 to 27.6, the words 5 pixels apart.
    const float rows = static_cast<float>(screen.height) / static_cast<float>(texel.height);
    // English's RE, EG, GI, IS and ER closer, and ST, TE and NA closer still, as painted, NAME where it was.
    static constexpr float kClose = -0.8f, kCloser = -1.6f;
    static constexpr float kEnglish[] = {kClose + 0.4f, kClose, kClose, kClose, kCloser, kCloser, kClose, 0.1f - 2 * kClose, kCloser};
    // NAME half a pixel further on than the painting's.
    return caption::DrawTitle("REGISTER NAME", screen.x, screen.y, 3.2f, 163.7f, 8.0f * rows + 0.2f, 26.2f * rows + 1.4f, 0.1f,
                              kEnglish, alpha);
}

// A painted title the game draws whole with its icon (a strip of a texture): the icon as painted, the word in the
// captions' look over the painted word, whose cream letters run from left to right of the strip and whose capitals
// from top to bottom, a space space ems; the English word's letters (spaces not counted) moved on by after[i] and drawn
// narrow[i] times as wide. English, or a language with the word; the painted strip otherwise.
struct PaintedTitle {
    const char            *texture;
    int                    u, v, width, height;
    int                    icon_width;
    const char            *key;
    const char            *english;
    int                    left, right, top, bottom;
    float                  space = 0.1f;
    std::span<const float> after = {}, narrow = {};
};

// CHECK-INCOUNTER's letters are painted nearly touching, its dash a short one tucked under the K's leg.
constexpr float kCheckInAfter[] = {-0.6f, -0.6f, -0.6f, -0.6f, -2.2f, -1.0f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f, -0.6f};
constexpr float kCheckInNarrow[] = {1, 1, 1, 1, 1, 0.6f};

constexpr PaintedTitle kPaintedTitles[] = {
    // the shop's title, and the check-in counter's (painted CHECK-INCOUNTER)
    {"shopbrd", 0x14, 0x50, 0xA2, 0x28, 58, "shop.title.shop", "SHOP", 64, 117, 9, 27},
    {"shopbrd", 0x14, 0x78, 0xC8, 0x28, 58, "shop.title.check_in", "CHECK-IN COUNTER", 63, 194, 9, 28, 0.1f, kCheckInAfter, kCheckInNarrow},
    // the fishing points exchange's title, and the fishing log's
    {"fishbrd", 0, 0xD0, 0xF0, 0x28, 58, "fishing.title.exchange", "FP EXCHANGE", 72, 234, 12, 30, 0.35f},
    {"fprecbrd", 0, 0xD0, 0x10C, 0x28, 58, "fishing.title.log", "FISHING LOG", 70, 219, 11, 30, 0.25f},
};

bool DrawPaintedTitle(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int alpha) {
    if (texture == nullptr || screen.width != texel.width || screen.height != texel.height) {
        return false;
    }
    for (const PaintedTitle &title : kPaintedTitles) {
        if (texel.x != title.u || texel.y != title.v || texel.width != title.width || texel.height != title.height ||
            std::strcmp(texture->name, title.texture) != 0) {
            continue;
        }
        std::string word = LocalizeText(title.key, "");
        const bool  english = word.empty() && LocalizeLanguageFile().rfind("en_", 0) == 0;
        if (english) {
            word = title.english;
        }
        if (word.empty() || !caption::Ready()) {
            return false;
        }
        set2DSprite(GetVif1Packet(), texture, CRect_i_(screen.x, screen.y, title.icon_width, screen.height),
                    CRect_i_(texel.x, texel.y, title.icon_width, texel.height), alpha);
        return caption::DrawTitle(word, screen.x, screen.y, static_cast<float>(title.left), static_cast<float>(title.right),
                                  static_cast<float>(title.top), static_cast<float>(title.bottom), title.space,
                                  english ? title.after : std::span<const float>{}, alpha, 0x80,
                                  english ? title.narrow : std::span<const float>{});
    }
    return false;
}

// The name alphabet's cells (the keyboard, the name being entered, the save slots, the ally card) drawn from the font.
bool DrawAlphabetCell(CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int r, int g, int b, int alpha) {
    return texture != nullptr && std::string_view(texture->name) == "alphabet" &&
           caption::DrawAlphabetCell(texel.x, texel.y, texel.width, texel.height, static_cast<float>(screen.x),
                                     static_cast<float>(screen.y), static_cast<float>(screen.width),
                                     static_cast<float>(screen.height), r, g, b, alpha);
}

} // namespace

PORT_OVERRIDE void AttachDataListToHaveCopy(int attachment_no, ATTACH_LIST *attachment) {
    if (attachment == nullptr) {
        return;
    }

    ATTACH_DATA *data = GetAttachData(attachment_no);
    if (data != nullptr) {
        std::memcpy(attachment, data, sizeof(*attachment));
    }
}

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
    // PAL artwork has 448 rows; the renderer expands full-screen sprites to the viewport.
    DrawMenu2DSprite(texture, CRect_i_(x, y, MenuDispRc.width, MenuDispRc.height),
                     CRect_i_(0, 0, 640, 448), alpha);
}

PORT_OVERRIDE void DrawMenu2DSprite(CTexture *texture, CRect_i_ screen, CRect_i_ texel, int alpha) {
    if (DrawAlphabetCell(texture, screen, texel, 0x80, 0x80, 0x80, alpha) || DrawBoardName(texture, screen, texel, alpha) ||
        DrawFloorLabels(texture, screen, texel, alpha) || DrawNameTitle(texture, screen, texel, alpha) ||
        DrawPaintedTitle(texture, screen, texel, alpha)) {
        return;
    }
    set2DSprite(GetVif1Packet(), texture, screen, texel, alpha);
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
            texel.height = 0x77;
        }
    }

    NameMouseHand(texture, screen, texel, r == 0 && g == 0 && b == 0);
    if (DrawAlphabetCell(texture, screen, texel, r, g, b, alpha) || DrawBoardName(texture, screen, texel, alpha) ||
        DrawFloorLabels(texture, screen, texel, alpha)) {
        return;
    }
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
    int width = frame_size[selected][0];
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

    int       item_x = x + offset_x;
    int       item_y = y + offset_y;
    s16       item_no = PerBoardPt->held_item.item_no;
    CRect_i_  shadow(0x80, 0x28, 0x20, 0x20);
    int       u = 0;
    int       v = 0;
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
