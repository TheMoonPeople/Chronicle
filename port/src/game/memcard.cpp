#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <utility>

#include "battle_globals.hpp"
#include "battlemenu/strings.hpp"
#include "clsmes.hpp"
#include "dataread.hpp"
#include "eastking.hpp"
#include "editatra.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editmenu.hpp"
#include "editpartsinfo.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "menu_save.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "options/screen.hpp"
#include "rect.hpp"
#include "save/save_slots.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "text/caption_text.hpp"
#include "texture.hpp"
#include "ui/menu_pointer.hpp"
#include "userstatus.hpp"

namespace dc::game {

using namespace dc::battlemenu;
using namespace dc::options;
using namespace dc::save;
using namespace dc::text;
using namespace dc::ui;

CEditPartsInfo    BtEditPartsInfo;
MENU_ATORA_SEL    MenuAtoraSel;
CMemoryCardAccess McAccess;

CTexture *StayTex;
CTexture *AttachIcon;
CTexture *ItemIcon;
CTexture *ItemIcon2;
CTexture *WepIcon;
CTexture *Sozai;
CTexture *HoleGray;
CTexture *HoleGold;
CTexture *ObTip;
CTexture *ObPerson;
CTexture *CompleteTex;
CTexture *VillageBar;
CTexture *VillageName;
CTexture *SaveBoard;

CEditPartsInfo *CommonMenuAtoraInfo;
short          *GetAtraMsgReadBuf;
s32             CursorVibeCnt;

namespace {

/** The chip that the georama board's cursor has picked up. */
ATORA_TIP_HAVE *NowTipHavePt;

/** Whether the board screen's texture block has finished loading. */
int AtoraTextureEnterFlag;

/** The texture block that holds the board's town tags and names. */
int AtoraTextureBaseBlock;

/** The texture block the georama board screen loads for its own textures. */
int AtoraTextureReadBlock;

/** The buffer that the georama board screen reads its files into. */
u_long128 *AtoraOffsetBuf;

/**
 * Returns the configuration word that holds the game clear flag.
 */
s32 &GameClearWord() {
    return *reinterpret_cast<s32 *>(&static_cast<SV_CONFIG_SYS *>(SaveData->GetConfigData())->game_clear_area[2]);
}

/**
 * Draws the georama board screen's board, panels and cursor.
 */
void DrawAtoraSelect(int fade);

/**
 * Moves the board cursor and handles its buttons, returning 10 when a part is
 * picked for placing, 100 when the screen is to close and zero otherwise.
 */
int AtoraBoardKey();

/**
 * Moves the chip cursor and swaps, sorts or puts back chips, returning 100 when
 * the screen is to close and zero otherwise.
 */
int AtoraTipKey();

/**
 * Puts the chip that the cursor holds back where it was picked up.
 */
void AtoraMenuTipCancel();

/**
 * Draws the board's edge fade.
 */
void AtoraBoardFadeEffect();

} // namespace

int McCheckMCPs2(MC_CARD_INFO *card) {
    if (!card->present || card->type != sceMcTypePS2) {
        return 0;
    }

    return 1;
}

void DrawObjectVibe(int x, int y, CTexture *texture, CRect_i_ src_rect, unsigned char alpha, int flag) {
    float    dest_x = static_cast<float>(x) + 7.0f * cosf(0.08055365830659866f * static_cast<float>(CursorVibeCnt));
    float    dest_y = static_cast<float>(y) + 5.0f * sinf(0.1163552850484848f * static_cast<float>(CursorVibeCnt));
    CRect_i_ dest_rect(static_cast<s32>(dest_x), static_cast<s32>(dest_y), src_rect.width, src_rect.height);
    DrawMenu2DSprite(texture, dest_rect, src_rect, alpha, alpha, alpha, flag);
}

void DrawObjectVibe(int x, int y, CTexture *texture, RECT src_rect, unsigned char alpha, int flag) {
    CRect_i_ src(src_rect.x, src_rect.y, src_rect.width, src_rect.height);
    DrawObjectVibe(x, y, texture, src, alpha, flag);
}

// The menus' hand follows the mouse while a screen has taken it as a pointer (port/src/menu_mouse.cpp).
void DrawMenuObjectVibe(int x, int y, int shadow, int icon_u) {
    CTexture *texture = TexManager.GetTexture(AtoraVibeTextureName, -1);
    CRect_i_  src(icon_u, 0x28, 0x20, 0x20);
    float     pointer_x = 0.0f;
    float     pointer_y = 0.0f;

    if (MenuPointerHand(pointer_x, pointer_y)) {
        CRect_i_ dest(static_cast<int>(pointer_x), static_cast<int>(pointer_y), src.width, src.height);

        if (shadow != 0) {
            DrawMenu2DSprite(texture, CRect_i_(dest.x + 5, dest.y + 3, src.width, src.height), src, 0, 0, 0, 100);
        }

        DrawMenu2DSprite(texture, dest, src, 0x80, 0x80, 0x80, 0x80);
        return;
    }

    if (shadow != 0) {
        DrawObjectVibe(x + 5, y + 3, texture, src, 0, 100);
    }

    DrawObjectVibe(x, y, texture, src, 0x80, 0x80);
}

void DrawMenuHelpWindow(CTexture *texture, int style, int x, int y, float width, float height, int alpha) {
    int corner_u = 0;

    if (style == 0) {
        corner_u = 24;
    } else if (style == 1) {
        corner_u = 0;
    } else {
        corner_u = 48;
    }

    float middle_width = 24.0f * width;
    float middle_height = 24.0f * height;
    int   middle_y = y + 24;
    int   bottom_y = middle_y + middle_height;

    if (texture == nullptr) {
        return;
    }

    DrawMenu2DSprite(texture, CRect_i_(x, y, 24, 24), CRect_i_(corner_u, 0, 24, 24), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, 24, middle_height), CRect_i_(24, 24, 24, 24), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, 24, 24), CRect_i_(24, 48, 24, 24), alpha);
    x += 24;
    DrawMenu2DSprite(texture, CRect_i_(x, y, middle_width, 24), CRect_i_(48, 0, 24, 24), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, middle_width, middle_height), CRect_i_(48, 24, 24, 24), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, middle_width, 24), CRect_i_(48, 48, 24, 24), alpha);
    x += middle_width;
    DrawMenu2DSprite(texture, CRect_i_(x, y, 24, 24), CRect_i_(72, 0, 24, 24), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, 24, middle_height), CRect_i_(72, 24, 24, 24), alpha);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, 24, 24), CRect_i_(72, 48, 24, 24), alpha);
}

void MenuHelpWinDraw(int x, int y, float width, float height, int alpha, int u, int v, CTexture *texture) {
    int middle_width = width * 16.0f;
    int middle_height = height * 22.0f;
    int middle_y = y + 22;
    int bottom_y = middle_y + middle_height;

    if (texture == nullptr) {
        return;
    }

    DrawMenu2DSprite(texture, CRect_i_(x, y, 24, 22), CRect_i_(u, v, 24, 22), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, 24, middle_height), CRect_i_(u, v + 22, 24, 20), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, 24, 22), CRect_i_(u, v + 42, 24, 22), (alpha * 100) >> 7);
    x += 24;
    DrawMenu2DSprite(texture, CRect_i_(x, y, middle_width, 22), CRect_i_(u + 22, v, 16, 22), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, middle_width, middle_height), CRect_i_(u + 22, v + 22, 16, 20), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, middle_width, 22), CRect_i_(u + 22, v + 42, 16, 22), (alpha * 100) >> 7);
    x += middle_width;
    DrawMenu2DSprite(texture, CRect_i_(x, y, 24, 22), CRect_i_(u + 38, v, 24, 22), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, 24, middle_height), CRect_i_(u + 38, v + 22, 24, 20), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, 24, 22), CRect_i_(u + 38, v + 42, 24, 22), (alpha * 100) >> 7);
}

void MenuHelpWinDraw2(int x, int y, float width, float height, int alpha, int u, int v, CTexture *texture) {
    int middle_width = width;
    int middle_height = height;
    int middle_y = y + 22;
    int bottom_y = middle_y + middle_height;

    if (texture == nullptr) {
        return;
    }

    DrawMenu2DSprite(texture, CRect_i_(x, y, 24, 22), CRect_i_(u, v, 24, 22), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, 24, middle_height), CRect_i_(u, v + 22, 24, 20), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, 24, 22), CRect_i_(u, v + 42, 24, 22), (alpha * 100) >> 7);
    x += 24;
    DrawMenu2DSprite(texture, CRect_i_(x, y, middle_width, 22), CRect_i_(u + 22, v, 16, 22), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, middle_width, middle_height), CRect_i_(u + 22, v + 22, 16, 20), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, middle_width, 22), CRect_i_(u + 22, v + 42, 16, 22), (alpha * 100) >> 7);
    x += middle_width;
    DrawMenu2DSprite(texture, CRect_i_(x, y, 24, 22), CRect_i_(u + 38, v, 24, 22), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, middle_y, 24, middle_height), CRect_i_(u + 38, v + 22, 24, 20), (alpha * 100) >> 7);
    DrawMenu2DSprite(texture, CRect_i_(x, bottom_y, 24, 22), CRect_i_(u + 38, v + 42, 24, 22), (alpha * 100) >> 7);
}

void MenuHelpWinDraw(int x, int y, float width, float height, int alpha) {
    StayTex = TexManager.GetTexture(AtoraVibeTextureName, -1);
    MenuHelpWinDraw(x, y, width, height, alpha, 0, 0, StayTex);
}

void DrawMenuWaku(float x, float y, int width, int height, int type, CTexture *texture, int alpha) {
    if (texture != nullptr) {
        static int MenuWakuCnt = 0;

        float                     offset = 0.2f * MenuWakuCnt;
        int                       left = x + offset;
        int                       top = y + offset;
        int                       right = (x + width) - offset;
        int                       bottom = (y + height) - offset;
        const std::array<RECT, 2> corner = {
            {{74, 72, 16, 16}, {106, 72, 27, 24}}
        };
        const RECT &src = corner[type];
        DrawMenu2DSprite(texture, CRect_i_(left, top, src.width, src.height), CRect_i_(src.x, src.y, src.width, src.height), alpha);
        DrawMenu2DSprite(texture, CRect_i_(right, top, src.width, src.height), CRect_i_(src.x + src.width, src.y, src.width, src.height), alpha);
        DrawMenu2DSprite(texture, CRect_i_(left, bottom, src.width, src.height), CRect_i_(src.x, src.y + src.height, src.width, src.height), alpha);
        DrawMenu2DSprite(texture, CRect_i_(right, bottom, src.width, src.height), CRect_i_(src.x + src.width, src.y + src.height, src.width, src.height), alpha);
        MenuWakuCnt++;

        if (MenuWakuCnt < 0 || MenuWakuCnt >= 30) {
            MenuWakuCnt = 0;
        }
    }
}

int DrawMenuNumber(int number, int x, int y, CTexture *texture, RECT rect, int overlap, int flag) {
    return DrawMenuNumber(number, x, y, rect, texture, overlap, 0, SCREEN_HEIGHT, flag);
}

int DrawMenuNumber(int number, int x, int y, RECT rect, CTexture *texture, int overlap, unsigned char r, unsigned char g, unsigned char b, int flag) {
    if (texture == nullptr) {
        return 0;
    }

    for (int digits = GetNumberKeta(number); 0 < digits; digits--) {
        int clip_y = y;
        int digit = number % 10;
        int width = rect.width;
        x -= width - overlap;
        int src_x = rect.x + width * digit;
        int src_y = rect.y;
        int height = rect.height;
        MenuTextureClip(clip_y, src_y, height, 0, SCREEN_HEIGHT);
        CRect_i_ src(src_x, src_y, width, height);
        CRect_i_ dest(x, clip_y, width, height - 1);
        DrawMenu2DSprite(texture, dest, src, r, g, b, flag);
        number /= 10;
    }

    return x;
}

int DrawMenuNumber(int number, int x, int y, RECT rect, CTexture *texture, int overlap, int top, int bottom, int flag) {
    if (texture == nullptr) {
        return 0;
    }

    for (int digits = GetNumberKeta(number); 0 < digits; digits--) {
        int clip_y = y;
        int digit = number % 10;
        int width = rect.width;
        x -= width - overlap;
        int src_x = rect.x + width * digit;
        int src_y = rect.y;
        int height = rect.height;
        MenuTextureClip(clip_y, src_y, height, top, bottom);
        CRect_i_ src(src_x, src_y, width, height);
        CRect_i_ dest(x, clip_y, width, height - 1);
        DrawMenu2DSprite(texture, dest, src, flag);
        number /= 10;
    }

    return x;
}

int GetMsgLengthMenu(ClsMes *mes, int mes_no) {
    int    length = 0;
    short *code = mes->GetTextLineDataTop_system(mes_no);

    if (code != nullptr) {
        while (true) {
            short c = *code++;

            if (static_cast<unsigned int>(c + 0x100) <= 1U || code == nullptr) {
                break;
            }

            length++;
        }
    }

    return length;
}

namespace {

/**
 * Gives the texture and the cell within it that one georama element draws from.
 */
CTexture *RetCTexAtora(int tip_no, int &x, int &y) {
    int tex_no = GetEditAtraChipData(MenuAtoraSel.map_no, tip_no)->tex_no;
    x = ((tex_no + 7) % 7) * 36;
    y = (tex_no / 7) * 36;
    return tip_no < 40 ? ObTip : ObPerson;
}

} // namespace

void DrawAtoraParts(int x, int y, int tip_no, int top, int bottom, int alpha) {
    int src_x = 0;
    int src_y = 0;

    if (x < 340 || x > 600) {
        return;
    }

    if (y < 80 || y > 300) {
        return;
    }

    if (y < top - 35 || bottom <= y) {
        return;
    }

    if (tip_no < 0) {
        return;
    }

    CTexture *texture = RetCTexAtora(tip_no, src_x, src_y);

    if (texture == nullptr) {
        return;
    }

    int height = 36;
    MenuTextureClip(y, src_y, height, top, bottom);
    CRect_i_ src(src_x, src_y, 36, height);
    CRect_i_ shadow(x + 2, y + 1, 36, height);
    DrawMenu2DSprite(texture, shadow, src, 0, 0, 0, (alpha * 80) >> 7);
    CRect_i_ dest(x, y, 36, height);
    DrawMenu2DSprite(texture, dest, src, alpha);
}

namespace {

/**
 * Returns the record of the n-th valid part in the georama's part list, or
 * null when there are fewer.
 */
EDITPARTS_INFO *SearchAtoraInfo(int index) {
    int parts = CommonMenuAtoraInfo->GetNextParts(-1);
    int count = -1;

    while (parts != -1) {
        count++;

        if (index == count) {
            return CommonMenuAtoraInfo->GetPartsInfo(parts);
        }

        parts = CommonMenuAtoraInfo->GetNextParts(parts);
    }

    return nullptr;
}

/**
 * Returns whether every chip of a georama part has been acquired.
 */
int AtoraAllTipGet(int parts_no) {
    EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(parts_no);

    if (info == nullptr) {
        return 0;
    }

    int result = 1;

    for (const EDITPARTS_ELEMENT &element : info->elements) {
        if (0 <= element.id && element.enabled == 0) {
            result = 0;
            break;
        }
    }

    return result;
}

/**
 * Returns whether the player has already spoken to a georama resident.
 */
int AlreadyPeopleTalk(int map_no, int chip_no) {
    SV_GRD_NPC *npc = SaveData->GetGrdNPCData(map_no, chip_no - 40);

    if (npc == nullptr || chip_no - 40 < 0) {
        return 1;
    }

    return npc->talk_message;
}

/**
 * Returns one when a georama part is fully built, its residents have all been
 * spoken to, and its completion event has not been flagged yet.
 */
int AtoraCompOrEvent(EDITPARTS_INFO *info) {
    EDIT_PARTS_ATRA *atra = GetEditAtraPartsData(MenuAtoraSel.map_no, info->parts_no);

    if (atra == nullptr) {
        return 0;
    }

    if (atra->kind == 0) {
        return 0;
    }

    int complete = CommonMenuAtoraInfo->CheckComplete(info->parts_no);
    int filled = 0;
    int talked = 1;
    int done = 0;

    if (info != nullptr) {
        if (info->placed == info->stock) {
            filled = 1;
        }

        for (const EDITPARTS_ELEMENT &element : info->elements) {
            if (element.id >= 40 && element.npc_no >= 0 && AlreadyPeopleTalk(MenuAtoraSel.map_no, element.id) == 0) {
                talked = 0;
            }
        }

        if (info->completion_flags & 1) {
            done = 1;
        }
    }

    int result = 0;

    if (complete && filled && talked && !done) {
        result = 1;
    }

    return result;
}

/**
 * Returns how many parts a georama ground defines, or zero for an invalid
 * ground.
 */
int AtraBoardMaxNum(int ground) {
    if (ground < 0 || ground >= 6) {
        return 0;
    }

    EDIT_PARTS_ATRA *parts = GetEditAtraPartsData(ground, 0);

    if (parts == nullptr) {
        return 0;
    }

    int count = 0;

    while (count < 40 && parts[count].max > 0) {
        count++;
    }

    return count;
}

/**
 * Returns the link code of a chip slot: -1 when empty, 0 without a pending
 * link, otherwise 1 or 2 by link kind plus 10 for each unplaced chip above it.
 */
int AtoraTipStatusSearch(EDITPARTS_INFO *info, int slot) {
    if (info == nullptr) {
        return ATORA_LINK_NONE;
    }

    if (info->elements[slot].id < 0) {
        return -1;
    }

    int link = info->elements[slot].required_element;

    if (link < 0 || info->elements[slot].enabled != 0) {
        return ATORA_LINK_NONE;
    }

    int code = (link < 3 && slot >= 3) ? ATORA_LINK_ABOVE : ATORA_LINK_BESIDE;

    if (info->elements[link].enabled == 0) {
        code += 10;
    }

    int next = info->elements[link].required_element;

    if (next >= 0 && info->elements[next].enabled == 0) {
        code += 10;
    }

    return code;
}

/**
 * Answers whether a georama chip attachment may be shown on the board: a
 * person's chip stays hidden until that resident has moved in.
 */
int AtraTipCanDisplay(EDIT_CHIP_ATTACH_DATA *attach) {
    int display = 1;

    if (attach != nullptr && attach->id < 40 && attach->npc_no >= 0 && (SaveData->GetGrdNPCData(MenuAtoraSel.map_no, attach->npc_no)->flags & 2) == 0) {
        display = 0;
    }

    return display;
}

/**
 * Draws the arrow from a chip slot to the chip it hangs off, covering the slot
 * while that chip is missing, as the slot's link code directs.
 */
void AtoraTipRelationDraw(int x, int y, EDITPARTS_INFO *info, int slot, int link, int alpha) {
    int dx = 0;
    int dy = 0;
    int u = 0;
    int v = 0;
    int height = 0;

    if (info != nullptr) {
        static int tipcurCnt = 0;

        u = 172;
        v = 390;
        height = 14;

        // Only link codes ending in 1 or 2 give the arrow an offset.
        if (link > 0) {
            switch (link % 10) {
                case ATORA_LINK_BESIDE:
                    dx = -10;
                    dy = 8;
                    u += 12;
                    break;
                case ATORA_LINK_ABOVE:
                    dx = 12;
                    dy = -12;
                    break;
            }
        }

        if (tipcurCnt % 460 > 200) {
            dy += 3;
            v += 14;
            height = 10;
        }

        if (link < 49 && link >= 20) {
            return;
        }

        if (link < 50 && link > 0) {
            DrawMenu2DSprite(Sozai, CRect_i_(x + dx, y + dy, 12, height), CRect_i_(u, v, 12, height), alpha);
        }

        if (link >= 10) {
            DrawMenu2DSprite(Sozai, CRect_i_(x, y, 36, 35), CRect_i_(220, 346, 36, 37), alpha);
        }

        tipcurCnt++;

        if (tipcurCnt < 0 || tipcurCnt > 999999) {
            tipcurCnt = 0;
        }
    }
}

/** Frame counter that DrawAtora's part plates animate with. */
int AtoraHeyCnt;

/** Frame counter that DrawAtora's completion sprites animate with. */
int CompMsgCt;

/**
 * Marks which of the six chip slots of a georama part can be selected.
 */
void AtoraBoardEnableMovePos(int parts_no, int *enable) {
    EDITPARTS_INFO *info = SearchAtoraInfo(parts_no);

    if (info == nullptr) {
        std::fill_n(enable, 6, 0);
        return;
    }

    for (int i = 0; i < 6; i++) {
        if (info->elements[i].id < 0) {
            enable[i] = 0;
            continue;
        }

        int link = info->elements[i].required_element;

        if (link >= 0) {
            link = info->elements[link].required_element;
        }

        enable[i] = (link < 0 || info->elements[link].enabled != 0) ? 1 : 0;
    }
}

/**
 * Returns the first index at or below a start whose entry in a slot table is
 * set, or the lower bound when none is.
 */
int AtoraBoardGoToPos(int *enable, int pos, int min) {
    while (min < pos) {
        if (enable[pos] != 0) {
            return pos;
        }

        pos--;
    }

    return pos;
}

} // namespace

int GetAtraMsgNo(int map_no, int element) {
    int mes_no = 0;

    if (element < 0 || element >= 100) {
        mes_no = 999;
    } else {
        EDIT_ELEMENT_ATRA *atra = GetEditAtraData(map_no, element);
        mes_no = atra->msg_no + (map_no * 200 + 1000);

        switch (map_no) {
            case TOWN_QUEENS:
                if (element == 1) {
                    int count = SaveData->GetGameIntFlag(1);

                    if (count > 0) {
                        mes_no += count + 0x1C;
                    }
                }

                break;
        }

        if (element >= 40) {
            mes_no += 40;
        }

        if (element >= 80) {
            mes_no += 40;
        }
    }

    return mes_no;
}

namespace {

/**
 * Returns the message number that the board shows for a georama part or one of
 * its chip slots, or -1 when the part has no record.
 */
int AtoraMsgNoGet(int map_no, int board_pos, int slot) {
    EDITPARTS_INFO *info = SearchAtoraInfo(board_pos);

    if (info == nullptr) {
        return -1;
    }

    EDIT_PARTS_ATRA *parts = GetEditAtraPartsData(map_no, info->parts_no);

    if (parts == nullptr) {
        return -1;
    }

    int msg_no = 0;

    switch (slot) {
        case 0:
            if (info->parts_no < 0) {
                msg_no = -1000;
                break;
            }

            msg_no = parts->msg_no + map_no * 200;

            switch (map_no) {
                case TOWN_QUEENS:
                    if (info->parts_no == 1) {
                        int flag = SaveData->GetGameIntFlag(1);

                        if (flag > 0) {
                            msg_no += flag + 28;
                        }
                    }

                    break;
            }

            break;
        default: {
            EDIT_CHIP_ATTACH_DATA *attach = &parts->elements[slot - 1];
            EDIT_ELEMENT_ATRA     *chip = GetEditAtraChipData(map_no, attach->id);
            EDIT_CHIP_ATTACH_DATA *shown = &GetEditAtraPartsData(MenuAtoraSel.map_no, info->parts_no)->elements[slot - 1];
            msg_no = chip->msg_no + (map_no * 200 + 40);

            if (attach->id >= 40) {
                msg_no += 40;
            }

            if (info->elements[slot - 1].enabled == 0) {
                msg_no += 60;
            }

            int link = info->elements[slot - 1].required_element;

            if ((link >= 0 && info->elements[link].enabled == 0) || AtraTipCanDisplay(shown) == 0) {
                msg_no = -788;
            }

            break;
        }
    }

    return msg_no;
}

/**
 * Returns the message number of a chip from its attribute record alone, or -1
 * when it has none.
 */
int AtoraTipOnlyMsgNoGet(int map_no, int number) {
    EDIT_ELEMENT_ATRA *chip = GetEditAtraChipData(map_no, number);

    if (chip == nullptr) {
        return -1;
    }

    int msg_no = chip->msg_no + 40;

    if (number >= 40) {
        msg_no += 40;
    }

    return msg_no;
}

/**
 * Gives the cell within the element sheet that one georama element draws from.
 */
void AtoraTipGetTexPos(int tip_no, int &x, int &y) {
    int tex_no = GetEditAtraChipData(MenuAtoraSel.map_no, tip_no)->tex_no;

    if (tip_no >= 40) {
        x = 144;
        y = 180;
    } else if (0 <= tex_no && tex_no < 40) {
        x = (tex_no % 7) * 36;
        y = (tex_no / 7) * 36;
    }
}

/**
 * Draws a chip's icon as either the object or the resident it stands for.
 */
void AtoraTipObjectOrPerson(int x, int y, int tip_no, int dark, int alpha) {
    int       u = 0;
    int       v = 0;
    CTexture *texture = RetCTexAtora(tip_no, u, v);
    CRect_i_  source(u, v, 0x24, 0x24);
    int       red = 0x80;
    int       green = 0x80;
    int       blue = 0x80;

    if (dark != 0) {
        red = 0x80;
        green = 0x44;
        blue = 0;
    }

    DrawMenu2DSprite(texture, CRect_i_(x, y, source.width, source.height - 1), source, red, green, blue, alpha);
}

/**
 * Returns the gold socket texture when its flag is set and the grey one
 * otherwise, and writes the colour to draw it with.
 */
CTexture *AtoraTipHoleTexInfoGet(int gold, unsigned char *color) {
    if (gold) {
        color[0] = 0x8C;
        color[1] = 0x80;
        color[2] = 0x50;
        return HoleGold;
    }

    color[0] = 0x80;
    color[1] = 0x80;
    color[2] = 0x80;
    return HoleGray;
}

/**
 * Draws the placement gauge of a georama part, filled in proportion to what has
 * been placed, with the placed-over-total count beside it.
 */
void AtoraPlateDrawHaichiBar(EDITPARTS_INFO *info, int x, int y, int flag) {
    int      empty = 83 - info->placed * 83 / info->stock;
    CRect_i_ src(244, 323 - empty, 12, empty);
    CRect_i_ dest(x + 12, y + 107 - empty, 12, empty);
    DrawMenu2DSprite(Sozai, dest, src, flag);
    DrawAtraBuildNum(info, x, y, flag);
}

} // namespace

void DrawAtraBuildNum(EDITPARTS_INFO *info, int x, int y, int alpha) {
    RECT digit = {0, 212, 12, 12};

    int num_x = x + 8;

    if ((info->stock - info->placed) / 10 > 0) {
        num_x += 24;
    } else {
        num_x += 12;
    }

    if (info->stock / 10 > 0) {
        num_x += 24;
    } else {
        num_x += 12;
    }

    num_x = DrawMenuNumber(info->stock, num_x, y + 5, StayTex, digit, 1, alpha);
    DrawMenu2DSprite(StayTex, CRect_i_(num_x - 10, y + 5, 12, 12), CRect_i_(120, digit.y, 12, 12), alpha);
    DrawMenuNumber(info->stock - info->placed, num_x - 7, y + 5, StayTex, digit, 1, alpha);
}

void DrawAtora(int x, int y, int parts_index, int alpha) {
    std::array<unsigned char, 4> colour{};

    if (y < -100 || y > 0x1CC) {
        return;
    }

    EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(parts_index);

    if (info == nullptr) {
        return;
    }

    EDIT_PARTS_ATRA *parts = GetEditAtraPartsData(MenuAtoraSel.map_no, info->parts_no);
    int              complete = CommonMenuAtoraInfo->CheckComplete(parts_index);
    int              all_tips = AtoraAllTipGet(parts_index);
    int              event = AtoraCompOrEvent(info);
    int              u = 0;
    int              v = 0;

    if (all_tips != 0 || (info->obtained != 0 && info->elements[0].id < 0)) {
        v = 0x78;
    }

    DrawMenu2DSprite(Sozai, CRect_i_(x, y, 0x100, 0x78), CRect_i_(0, v, 0x100, 0x79), alpha);

    if (0 < info->stock && MenuAtoraSel.map_no != 5) {
        AtoraPlateDrawHaichiBar(info, x, y, alpha);
    }

    int picture_x = x + 0x20;
    int picture_y = y + 8;
    int draw_picture = 1;
    int tex_no = parts->tex_no;
    u = (tex_no / 6) * 0x54 + 0x104;
    v = (tex_no + 6) % 6 * 0x54;

    if (MenuAtoraSel.map_no == 5) {
        if (complete == 0) {
            draw_picture = 0;
        } else {
            picture_x = x + 0x14;
            picture_y = y + 0xC;
        }
    }

    if (draw_picture != 0) {
        DrawMenu2DSprite(Sozai, CRect_i_(picture_x, picture_y, 0x54, 0x54), CRect_i_(u, v, 0x54, 0x54), alpha);
    }

    u = 0;
    v = 0;
    int slot_x = 0x78;
    int slot_y = 8;

    if (parts->elements[0].id < 0) {
        DrawMenu2DSprite(Sozai, CRect_i_(x + 0x88, y - 1, 0x78, 0x5D), CRect_i_(0, 0xF0, 0x78, 0x5C), alpha);
        DrawMenu2DSprite(Sozai, CRect_i_(x + 0x88, y + 7, 0x64, 0x1C), CRect_i_(0, 0x164, 0x64, 0x1C), alpha);
    } else if (event == 0) {
        for (int i = 0; i < 6; i++) {
            GetEditAtraChipData(MenuAtoraSel.map_no, parts->elements[i].id);
            EDIT_CHIP_ATTACH_DATA *chip = &parts->elements[i];

            if (chip->id >= 0) {
                int status = AtoraTipStatusSearch(info, i);

                if (AtraTipCanDisplay(chip) == 0) {
                    status = ATORA_LINK_HIDDEN;
                }

                int tip_x = x + slot_x;
                int tip_y = y + slot_y;
                AtoraTipRelationDraw(tip_x, tip_y, info, i, status, alpha);

                if ((chip->required_element < 0 || status < 3) && status != ATORA_LINK_HIDDEN) {
                    AtoraTipGetTexPos(chip->id, u, v);
                    CTexture *hole = AtoraTipHoleTexInfoGet(all_tips, colour.data());
                    DrawMenu2DSprite(hole, CRect_i_(tip_x, tip_y, 0x24, 0x24), CRect_i_(u, v, 0x24, 0x25), colour[0], colour[1], colour[2], alpha);
                }

                if (info->elements[i].enabled != 0) {
                    AtoraTipObjectOrPerson(tip_x, tip_y, info->elements[i].id, all_tips, alpha);

                    if (complete == 0 && chip->npc_no >= 0 && AlreadyPeopleTalk(MenuAtoraSel.map_no, chip->id) == 0 && AtoraHeyCnt % (chip->id + 0x4B) < 0x46) {
                        DrawMenu2DSprite(CompleteTex, CRect_i_(tip_x - 9, tip_y - 7, 0x20, 0x15), CRect_i_(0xE2, 0, 0x20, 0x16), alpha);
                    }
                }
            }

            slot_x += 0x2C;

            if (i == 2) {
                slot_x = 0x78;
                slot_y = 0x35;
            }
        }
    } else {
        int base_x = x + 0x70;
        int base_y = y + 6;
        int count = 0;

        for (const EDITPARTS_ELEMENT &element : info->elements) {
            if (element.id < 0) {
                break;
            }

            count++;
        }

        float wave = cosf(PI * CompMsgCt / (160.0f + (count >> 1)));
        base_x = static_cast<int>(static_cast<float>(base_x - 6) + 6.0f * wave);
        base_y = static_cast<int>(static_cast<float>(base_y) + 2.0f * wave);
        float width = 128.0f + 6.0f * wave;
        float height = 88.0f + 3.0f * wave;
        base_x = static_cast<int>(static_cast<float>(base_x) + 8.0f * cosf(PI * CompMsgCt / (180.0f + count)));
        base_y = static_cast<int>(static_cast<float>(base_y) + 4.0f * sinf(PI * CompMsgCt / (140.0f + count)));
        DrawMenu2DSprite(CompleteTex, CRect_i_(base_x, base_y, static_cast<int>(width), static_cast<int>(height)), CRect_i_(0, 0x28, 0x80, 0x58), alpha);
    }

    CompMsgCt++;

    if (CompMsgCt >= 320000 || CompMsgCt < 0) {
        CompMsgCt = 0;
    }
}

namespace {

/**
 * Draws an empty plate of the georama board with a caption in its centre.
 */
void DrawAtoraNothing(int x, int y, int alpha) {
    DrawMenu2DSprite(Sozai, CRect_i_(x, y, 18, 18), CRect_i_(184, 346, 18, 18), alpha);
    DrawMenu2DSprite(Sozai, CRect_i_(x + 18, y, 220, 18), CRect_i_(200, 346, 4, 18), alpha);
    DrawMenu2DSprite(Sozai, CRect_i_(x + 238, y, 18, 18), CRect_i_(202, 346, 18, 18), alpha);
    DrawMenu2DSprite(Sozai, CRect_i_(x, y + 18, 18, 84), CRect_i_(184, 360, 18, 4), alpha);
    DrawMenu2DSprite(Sozai, CRect_i_(x + 238, y + 18, 18, 84), CRect_i_(202, 360, 18, 4), alpha);
    DrawMenu2DSprite(Sozai, CRect_i_(x, y + 102, 18, 18), CRect_i_(184, 364, 18, 18), alpha);
    DrawMenu2DSprite(Sozai, CRect_i_(x + 18, y + 102, 220, 18), CRect_i_(200, 364, 4, 18), alpha);
    DrawMenu2DSprite(Sozai, CRect_i_(x + 238, y + 102, 18, 18), CRect_i_(202, 364, 18, 18), alpha);
    int lang = GetMenuLangFlag();
    // Horizontal offset of the message, by menu language.
    constexpr std::array<s8, 7> message_x = {64, 64, 64, 64, 54, 64, 64};
    DrawMenu2DSprite(Sozai, CRect_i_(x + message_x[lang], y + 44, 132, 30), CRect_i_(124, 418, 132, 30), alpha);
}

/**
 * Shows message 200 in a message window at a position.
 */
void DrawMsgAtraWarning(ClsMes *mes, int x, int y) {
    if (mes == nullptr) {
        return;
    }

    if (mes->mes_made != 200) {
        mes->MakeMesWin(200);
    }

    mes->MakeMesWin(200);
    MenuTextureReload(mes->tex_block);
    mes->stay_frame = true;
    mes->text_x = x;
    mes->text_y = y;
    mes->Step();
    mes->DrawMesWin();
}

/**
 * Clears the georama chip selection.
 */
void AtoraTipInfoInit() {
    NowTipHavePt->mode = ATORA_SIDE_BOARD;
    NowTipHavePt->parts_no = -1;
    NowTipHavePt->slot = -1;
    NowTipHavePt->tip_no = -1;
}

} // namespace

int GetMenuAtraEventFlag() {
    return MenuAtoraSel.event_flag;
}

namespace {

/**
 * Records whether the georama menu is running an event.
 */
void SetMenuAtraEventFlag(int flag) {
    MenuAtoraSel.event_flag = flag;
}

/**
 * Opens the board's message window on the message of the selected part.
 */
void MenuAtoraAfterFadeIn() {
    CommonMenuMes2.SetBuff(GetAtraMsgReadBuf);
    CommonMenuMes2.mes_made = -1;
    int msg_no = AtoraMsgNoGet(MenuAtoraSel.map_no, MenuAtoraSel.board_pos, 0);
    CommonMenuMes2.MakeMesWin(msg_no >= 0 ? msg_no + 1000 : 0);
}

} // namespace

void InitMenuAtora1(int open_mode, int edit_map, int *texture_blocks, u_long128 *buffer) {
    MenuAtoraSel.open_mode = open_mode;
    InitPersonalBoardMode(SaveData->GetDngStatus(), &MenuAtoraSel.board, PERSONAL_BOARD_ATLA, edit_map + BOARD_PAGE_ATLA_VILLAGE);
    MenuAtoraSel.prev_mes_buff = CommonMenuMes2.buff;
    AtoraTextureBaseBlock = texture_blocks[0];
    AtoraTextureReadBlock = texture_blocks[1];
    AtoraOffsetBuf = buffer;
    AtoraOffsetBuf = MenuCalcBufAlignment(buffer);
    AtoraTextureEnterFlag = 0;
    MenuAtoraSel.edit_map = edit_map;
    CommonMenuMes3.Preset(MES_PRESET_NAME);
    CommonMenuMes3.style = MES_EDGE_NONE;
    CommonMenuMes3.end_mark = false;
    CommonMenuMes3.auto_pos = MES_POS_NONE;
    CommonMenuMes3.mes_made = -1;
    AtoraNameMes.Preset(MES_PRESET_NAME);
    AtoraNameMes.tex_buff = MesWinTexBuff_12;
    memset(AtoraNameMes.tex_buff, 0, 0x100);
    AtoraNameMes.rows = 4;
    constexpr std::array<s8, 7> widths = {0x10, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C};
    AtoraNameMes.char_width = widths[GetMenuLangFlag()];
    AtoraNameMes.narrow_gaiji_set = 2;

    for (int i = 0; i < 10; i++) {
        AtoraNameMes.mes_no[i] = 999;
        AtoraNameMes.values[i] = -1;
    }

    AtoraNameMes.mes_made = -1;
    AtoraNameMes.narrow_gaiji = true;
    NowTipHavePt = reinterpret_cast<ATORA_TIP_HAVE *>(&MenuAtoraSel.board.held_item);
    MenuAtoraSel.last_board_pos = 0;
    MenuAtoraSel.board.cursor = 0;
    MenuAtoraSel.step = ATORA_STEP_FADE_IN;
    MenuAtoraSel.step_count = 0;
    SetMenuAtraEventFlag(0);
    CMenuCursor *cursor = SaveData->GetMenuCursor();

    if (cursor->reset_pos == 0) {
        MenuAtoraSel.mode = cursor->mode[3];

        if (MenuAtoraSel.mode < 0 || MenuAtoraSel.mode > 1) {
            MenuAtoraSel.mode = ATORA_SIDE_BOARD;
        }

        int pos = cursor->pos[3];

        switch (MenuAtoraSel.mode) {
            case ATORA_SIDE_BOARD:
                MenuAtoraSel.board_pos = pos;
                break;
            case ATORA_SIDE_CHIP_LIST:
                MenuAtoraSel.board.cursor = pos;
                MenuAtoraSel.board.top_row = MenuAtoraSel.board.cursor / 5 - 2;

                if (MenuAtoraSel.board.top_row < 0) {
                    MenuAtoraSel.board.top_row = 0;
                }

                break;
        }
    } else {
        MenuAtoraSel.mode = ATORA_SIDE_BOARD;
        MenuAtoraSel.board_pos = 0;
        MenuAtoraSel.board.cursor = 0;
    }

    if (GetMenuAtraEventFlag() != 0) {
        MenuAtoraSel.board_pos = MenuAtoraSel.event_board_pos;
    }
}

void InitMenuAtoraSelect(int map_no) {
    MenuAtoraSel.map_no = map_no;
    int map = MenuAtoraSel.map_no;

    if (AtoraTextureEnterFlag == 0) {
        std::array<char, 64> lang_dir{};
        GetPathReadDifferntLang(lang_dir.data());
        std::string path = std::format("{}a{}.pak", lang_dir.data(), map + 1);

        if (ReadBGSync()) {
            BreakReadBG();
        }

        StartReadBG();
        LoadFileBG(path.data(), AtoraOffsetBuf, nullptr);
        MenuAtoraSel.load_state = 0;

        if ((MenuAtoraSel.open_mode == ATORA_OPEN_PLACE || MenuAtoraSel.open_mode == ATORA_OPEN_WARN) && MenuAtoraSel.map_no == MapNo) {
            CommonMenuAtoraInfo = &EditPartsInfo;
        } else {
            CommonMenuAtoraInfo = &BtEditPartsInfo;
            CommonMenuAtoraInfo->Load(MenuAtoraSel.map_no, SaveData, 1);
        }
    }

    MenuAtoraSel.board.atla_elements = SaveData->GetElemData(map);
    int next = CommonMenuAtoraInfo->GetNextPartsNum(-1);
    AtoraTipInfoInit();
    int count = AtraBoardMaxNum(MenuAtoraSel.map_no);

    if (MenuAtoraSel.board_pos > count - 1) {
        MenuAtoraSel.board_pos = count - 1;
    }

    EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);

    if (MenuAtoraSel.mode == ATORA_SIDE_BOARD) {
        if (info != nullptr) {
            if (MenuAtoraSel.board.cursor > 0 && info->elements[MenuAtoraSel.board.cursor - 1].id < 0) {
                MenuAtoraSel.board.cursor = 0;
            }
        } else {
            MenuAtoraSel.board.cursor = 0;
        }
    }

    MenuAtoraSel.scroll_y = 146.0f - 130.0f * MenuAtoraSel.board_pos;

    if (0 < next) {
        MenuAtoraSel.cursor_x = 38.0f;
        MenuAtoraSel.cursor_y = 175.0f;
    } else {
        int place = MenuAtoraSel.board.cursor % 4;
        MenuAtoraSel.cursor_x = place * 58 + 334;
        place = (MenuAtoraSel.board.cursor >> 2) - MenuAtoraSel.board.top_row;

        if (place < 0) {
            place = 0;
        }

        if (place > 3) {
            place = 3;
        }

        MenuAtoraSel.cursor_y = place * 50 + 118;
    }

    if (next > 0) {
        int pos = MenuAtoraSel.board_pos - 1;
        int y = MenuAtoraSel.scroll_y + 80 + 130.0f * pos;

        for (int i = 0; i < 3; i++) {
            info = SearchAtoraInfo(pos);

            if (info != nullptr) {
                AtoraNameMes.mes_no[i] = info->parts_no + (MenuAtoraSel.map_no * 200 + 1000);
                AtoraNameMes.line_pos[0].x = 134;
                AtoraNameMes.line_pos[0].y = y;
                y += 130.0f;
            } else {
                AtoraNameMes.mes_no[i] = 999;
            }

            pos++;
        }

        AtoraNameMes.mes_made = -1;
        AtoraNameMes.MakeMesWin(210);
    }
}

namespace {

/**
 * Stores the mode of a menu in the saved menu cursors.
 */
void SetCursorMode(CMenuCursor *cursor, int menu, int mode) {
    cursor->mode[menu] = mode;
}

/**
 * Closes the georama board screen, saving the cursor position unless the
 * menus were told to forget it, and restores the shared message windows.
 */
void ExitAtoraSelect() {
    CMenuCursor *cursor = SaveData->GetMenuCursor();

    if (cursor->reset_pos == 0) {
        SetCursorMode(cursor, 3, MenuAtoraSel.mode);
        int pos = 0;

        switch (MenuAtoraSel.mode) {
            case ATORA_SIDE_BOARD:
                pos = MenuAtoraSel.board_pos;
                break;
            case ATORA_SIDE_CHIP_LIST:
                pos = MenuAtoraSel.board.cursor;
                break;
        }

        cursor->pos[3] = pos;
    }

    CommonMenuMes2.SetBuff(MenuAtoraSel.prev_mes_buff);
    CommonMenuMes3.auto_pos = MES_POS_NONE;
    CommonMenuMes3.Preset(MES_PRESET_SYSTEM);
    AtoraNameMes.narrow_gaiji = false;
}

/**
 * Looks up the textures the georama board draws from.
 */
void AtoraTexInfoGet() {
    CompleteTex = TexManager.GetTexture("complete", -1);
    Sozai = TexManager.GetTexture("sozai", AtoraTextureReadBlock);
    HoleGray = TexManager.GetTexture("holegray", AtoraTextureReadBlock);
    HoleGold = TexManager.GetTexture("holegold", AtoraTextureReadBlock);
    ObTip = TexManager.GetTexture("obtip", AtoraTextureReadBlock);
    ObPerson = TexManager.GetTexture("obperson", AtoraTextureReadBlock);
    VillageBar = TexManager.GetTexture("viltag", AtoraTextureBaseBlock);
    VillageName = TexManager.GetTexture("vilname", AtoraTextureBaseBlock);
}

} // namespace

void DrawMenuAtoraSelect() {
    int alpha = 0x80;

    switch (MenuAtoraSel.step) {
        case ATORA_STEP_FADE_IN:
            alpha = MenuAtoraSel.step_count * 8;
            break;
        case ATORA_STEP_FADE_OUT:
            alpha = 0x80 - MenuAtoraSel.step_count * 8;
            break;
    }

    if (alpha < 0) {
        alpha = 0;
    }

    if (alpha > 0x80) {
        alpha = 0x80;
    }

    if (MenuAtoraSel.step != ATORA_STEP_EVENT_PLAY) {
        DrawAtoraSelect(alpha);

        if (MenuAtoraSel.step == ATORA_STEP_RUN) {
            DrawMenuObjectVibe(MenuAtoraSel.cursor_x, MenuAtoraSel.cursor_y, 1, MenuAtoraSel.cursor_icon_u);
        }

        if (MenuAtoraSel.step != ATORA_STEP_RUN && AtoraTextureEnterFlag != 0) {
            MenuAtoraSel.step_count++;
        } else {
            MenuAtoraSel.step_count = 0;
        }

        CTexture frame = *TexManager.GetTexture("frame_image", -1);
        reinterpret_cast<sceGsTex0 *>(&frame.tex0)->bits.tcc = 0;
        sceGsTexa texa = mgTexa;
        texa.AEM = 1;
        texa.TA0 = 0x80;
        MGSetGsTEXA(&texa);
        int tint = 0;
        int fade = 0;

        switch (MenuAtoraSel.step) {
            case ATORA_STEP_FADE_IN:
                tint = 0x80 - MenuAtoraSel.step_count * 7;
                break;
            case ATORA_STEP_EVENT_FADE_OUT:
                fade = MenuAtoraSel.step_count * 2;
                break;
            case ATORA_STEP_EVENT_FADE_IN:
                fade = 0x80 - MenuAtoraSel.step_count * 2;
                break;
            case ATORA_STEP_FADE_OUT:
                tint = MenuAtoraSel.step_count * 5 + 0x40;
                break;
        }

        if (tint < 0) {
            tint = 0;
        }

        if (tint > 0x80) {
            tint = 0x80;
        }

        if (fade < 0) {
            fade = 0;
        }

        if (fade > 0x80) {
            fade = 0x80;
        }

        CRect_i_ rect(320, 0, 320, SCREEN_HEIGHT);
        DrawMenu2DSprite(&frame, rect, rect, 0x40, 0x40, 0x40, tint);
        MGSetGsTEXA(nullptr);

        if (MenuAtoraSel.step == ATORA_STEP_EVENT_FADE_OUT || MenuAtoraSel.step == ATORA_STEP_EVENT_FADE_IN) {
            AllFadeForMenu(fade);
        }

        if (MenuAtoraSel.step == ATORA_STEP_WARNING) {
            CommonMenuMes3.auto_pos = MES_POS_CENTRE;
            CommonMenuMes3.edge_alpha = 0x80;
            DrawMsgAtraWarning(&CommonMenuMes3, 184, 150);
        } else {
            CommonMenuMes3.mes_made = -1;
        }
    }

    if (MenuAtoraSel.step == ATORA_STEP_EVENT_PLAY) {
        EastKingEventDraw();
    }
}

namespace {

/**
 * Draws the Atla town board, its inventory, and the active cursor.
 */
void DrawAtoraSelect(int fade) {
    float cursor_x = 0.0f;
    float cursor_y = 0.0f;
    int   waku_size = 0;
    int   waku_x = 0;
    int   waku_y = 0;
    int   u = 0;
    int   v = 0;

    AtoraTexInfoGet();
    int alpha = 0x80;

    switch (MenuAtoraSel.step) {
        case ATORA_STEP_FADE_IN:
        case ATORA_STEP_FADE_OUT:
            alpha = fade;
            break;
        case ATORA_STEP_CHANGE_MAP:
            alpha = MenuAtoraSel.step_count * 5;
            break;
    }

    if (alpha > 0x80) {
        alpha = 0x80;
    }

    int open_mode = MenuAtoraSel.open_mode;
    MenuAtoraSel.name_alpha = alpha;
    EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);
    int             event = 0;
    int             all_tips = 0;

    if (info != nullptr) {
        event = AtoraCompOrEvent(info);
        all_tips = AtoraAllTipGet(info->parts_no);
    }

    MenuTextureReload(AtoraTextureReadBlock);
    float target = 146.0f - 130.0f * MenuAtoraSel.board_pos;
    float step = (target - MenuAtoraSel.scroll_y) / 2.0f;
    int   odd = static_cast<int>(step) % 2;
    step /= 2.0f;
    step += odd;
    MenuAtoraSel.scroll_y += step;
    int settled = 1;

    if (!(static_cast<float>(abs(static_cast<int>(static_cast<float>(MenuAtoraSel.scroll_y) - target))) <= 4.0f)) {
        settled = 0;
        event = 0;
        all_tips = 0;
    }

    if (MenuAtoraSel.mode != ATORA_SIDE_BOARD || open_mode != ATORA_OPEN_PLACE) {
        settled = 0;
    }

    if (NowEditMap != MenuAtoraSel.map_no) {
        settled = 0;
    }

    if (info != nullptr) {
        if (info->placed == info->stock) {
            settled = 0;
        }
    } else {
        settled = 0;
    }

    int   remaining = AtraBoardMaxNum(MenuAtoraSel.map_no);
    int   parts = CommonMenuAtoraInfo->GetNextParts(-1);
    float y = MenuAtoraSel.scroll_y;

    if (AtoraTextureEnterFlag != 0) {
        AtoraHeyCnt++;

        while (0 <= parts) {
            DrawAtora(0x38, static_cast<int>(y), parts, alpha);
            parts = CommonMenuAtoraInfo->GetNextParts(parts);
            y += 130.0f;
            remaining--;
        }

        while (remaining > 0) {
            DrawAtoraNothing(0x38, static_cast<int>(y), alpha);
            y += 130.0f;
            remaining--;
        }

        if (AtoraHeyCnt > 10000000) {
            AtoraHeyCnt = 0;
        }

        if (settled != 0 || (MenuAtoraSel.map_no == 5 && all_tips != 0)) {
            float phase = CursorVibeCnt % 89;
            phase -= 45.0f;
            float    sign_y = 208.0f + 8.0f * sinf(PI * phase / 45.0f);
            CRect_i_ source(0, 0, 0x50, 0x20);

            if (MenuAtoraSel.map_no == 5 && all_tips != 0) {
                source.x += 0x50;
            }

            DrawMenu2DSprite(CompleteTex, CRect_i_(0x83, static_cast<int>(1.0f + sign_y), source.width, source.height), source, 6, 6, 6, (alpha * 0x50) >> 7);
            DrawMenu2DSprite(CompleteTex, CRect_i_(0x80, static_cast<int>(sign_y), source.width, source.height), source, alpha);
        }

        if (MenuAtoraSel.step == ATORA_STEP_COMPLETE_FLASH) {
            sceGsAlpha blend = mgAlpha;
            blend.bits.a = 0;
            blend.bits.b = 2;
            blend.bits.c = 0;
            blend.bits.d = 1;
            MGSetGsALPHA(&blend);
            int flash = static_cast<int>(128.0f - 3.0f * MenuAtoraSel.step_count);

            if (flash < 5) {
                flash = 0;
                MenuAtoraSel.step = ATORA_STEP_RUN;
            }

            DrawMenu2DSprite(Sozai, CRect_i_(0x34, 0x8B, 0x10C, 0x82), CRect_i_(0x8C, 0x1C0, 0x74, 0x40), flash);
            MGSetGsALPHA(nullptr);
        }

        AtoraBoardFadeEffect();
    }

    int count = PersonalRetMax(MenuAtoraSel.board.page);
    int board_y = 0x7F - MenuAtoraSel.board.top_row * 0x28;
    MenuAtoraSel.board.y += (board_y - MenuAtoraSel.board.y) / 4.0f;
    board_y = static_cast<int>(MenuAtoraSel.board.y);
    MenuTextureReload(PerBoardTex->block);
    DrawPerBoardDraw(0, count, 0x168, board_y, 0x81, 0x121, PerBoardTex, 0x80);

    if (AtoraTextureEnterFlag != 0) {
        MenuTextureReload(AtoraTextureReadBlock);
        CommonIconDraw(MenuAtoraSel.board.page, count, 0x168, board_y + 1, 0x81, 0x121, alpha);
    }

    MenuTextureReload(PerBoardTex->block);
    PersonalBoardTagDraw(MenuAtoraSel.board.page, 0x154, 0x78, PerBoardTex, 0, 0x80);

    if (PerBoardTex != nullptr) {
        MenuTextureReload(PerBoardTex->block);
        PersonalBoardDrawWaku(0x154, 0x78, PerBoardTex, 0x80);
        PersonalBoardScrlBarDraw(count, 0x154, 0x78, MenuAtoraSel.board.scroll, MenuAtoraSel.board.top_row, PerBoardTex, 0x80);
        PersonalBoardMaxDraw(count, 0x154, 0x78, PerBoardTex, 0x80);
    }

    AtoraNameDraw(0);

    if (0 < GetAtoraMaxVillage() - 3) {
        int arrow_y = static_cast<int>(66.0f + 4.0f * sinf(PI * static_cast<float>(CursorVibeCnt % 79 - 40) / 40.0f));
        DrawMenu2DSprite(PerBoardTex, CRect_i_(0x146, arrow_y, 0x1A, 0x18), CRect_i_(0x62, 0x14, 0x1A, 0x18), 0x80);
        DrawMenu2DSprite(PerBoardTex, CRect_i_(0x20C, arrow_y, 0x1A, 0x18), CRect_i_(0x7C, 0x14, 0x1A, 0x18), 0x80);
    }

    if (PerBoardTex != nullptr) {
        MenuTextureReload(PerBoardTex->block);
    }

    switch (MenuAtoraSel.mode) {
        case ATORA_SIDE_BOARD:
            if (info == nullptr) {
                cursor_x = 76.0f;
                cursor_y = 176.0f;
            } else if (MenuAtoraSel.board.cursor == 0) {
                cursor_x = 56.0f;
                cursor_y = 176.0f;
                waku_x = static_cast<int>(cursor_x + 2.0f);
                waku_y = static_cast<int>(cursor_y - 15.0f);
                waku_size = 0x56;
            } else {
                waku_size = 0x24;
                cursor_x = (MenuAtoraSel.board.cursor + 2) % 3 * 0x2C + 0x92;

                if (MenuAtoraSel.board.cursor > 0 && MenuAtoraSel.board.cursor < 4) {
                    cursor_y = 167.0f;
                } else {
                    cursor_y = 210.0f;
                }

                waku_x = static_cast<int>(3.0f + cursor_x);
                waku_y = static_cast<int>(cursor_y - 5.0f);
            }

            break;
        case ATORA_SIDE_CHIP_LIST: {
            cursor_x = MenuAtoraSel.board.cursor % 5 * 0x28 + 0x14E;
            int row = MenuAtoraSel.board.cursor / 5 - MenuAtoraSel.board.top_row;

            if (row < 0) {
                row = 0;
            }

            if (row > 3) {
                row = 3;
            }

            cursor_y = row * 0x28 + 0x8A;
            waku_x = static_cast<int>(cursor_x);
            waku_y = static_cast<int>(cursor_y);
            waku_size = 0x24;
            break;
        }
    }

    MenuAtoraSel.cursor_icon_u = 0x40;

    if (NowTipHavePt->tip_no > -1) {
        MenuAtoraSel.cursor_icon_u = 0x80;
        cursor_x += 25.0f;
        cursor_y += 12.0f;
    } else {
        switch (MenuAtoraSel.mode) {
            case ATORA_SIDE_BOARD:
                if (info != nullptr) {
                    if (0 < MenuAtoraSel.board.cursor && info->elements[MenuAtoraSel.board.cursor - 1].enabled != 0) {
                        cursor_x += 27.0f;
                        cursor_y += 12.0f;
                        MenuAtoraSel.cursor_icon_u = 0x60;
                    } else {
                        MenuAtoraSel.cursor_icon_u = 0x40;
                    }
                }

                break;
            case ATORA_SIDE_CHIP_LIST:
                if (MenuAtoraSel.board.cursor > -1 && MenuAtoraSel.board.atla_elements[MenuAtoraSel.board.cursor] > -1) {
                    cursor_x += 25.0f;
                    cursor_y += 12.0f;
                    MenuAtoraSel.cursor_icon_u = 0x60;
                }

                break;
        }
    }

    MenuAtoraSel.cursor_x += (cursor_x - MenuAtoraSel.cursor_x) / 4.0f;
    MenuAtoraSel.cursor_y += (cursor_y - MenuAtoraSel.cursor_y) / 4.0f;

    if (MenuAtoraSel.step == ATORA_STEP_RUN) {
        MenuTextureReload(AtoraTextureReadBlock);
        int tip_no = NowTipHavePt->tip_no;

        if (tip_no > -1) {
            CTexture *texture = RetCTexAtora(tip_no, u, v);

            if (texture != nullptr) {
                float    sway_x = 7.0f * cosf(0.0805536583f * CursorVibeCnt);
                float    sway_y = 5.0f * sinf(0.116355285f * CursorVibeCnt);
                int      tip_x = static_cast<int>(2.0f + (MenuAtoraSel.cursor_x + sway_x));
                int      tip_y = static_cast<int>(MenuAtoraSel.cursor_y + sway_y - 14.0f);
                CRect_i_ source(u, v, 0x24, 0x24);
                DrawMenu2DSprite(texture, CRect_i_(tip_x + 5, tip_y + 3, 0x24, 0x24), source, 10, 10, 10, 0x50);
                float hand_x = MenuAtoraSel.cursor_x + sway_x;
                float hand_y = MenuAtoraSel.cursor_y + sway_y;
                DrawMenu2DSprite(StayTex, CRect_i_(static_cast<int>(hand_x) + 6, static_cast<int>(hand_y) + 3, 0x20, 0x20), CRect_i_(0x80, 0x28, 0x20, 0x20), 10, 10, 10, 0x50);
                DrawMenu2DSprite(texture, CRect_i_(tip_x, tip_y, 0x24, 0x24), source, 0x80);
            }
        }

        int draw_waku = 1;

        switch (MenuAtoraSel.mode) {
            case ATORA_SIDE_BOARD:
                if (info == nullptr) {
                    draw_waku = 0;
                }

                if (open_mode == ATORA_OPEN_PLACE && 0 < MenuAtoraSel.board.cursor && event != 0) {
                    draw_waku = 0;
                }

                break;
        }

        if (MenuAtoraSel.step == ATORA_STEP_EVENT_START || (MenuAtoraSel.mode == ATORA_SIDE_BOARD && MenuAtoraSel.board.cursor == 0 && MenuAtoraSel.map_no == 5)) {
            draw_waku = 0;
        }

        if (draw_waku != 0) {
            DrawMenuWaku(waku_x + 0x15, waku_y - 0xE, waku_size, waku_size, 0, StayTex, 0x80);
        }
    }
}

/**
 * Loads the georama board screen's texture block.
 */
int AtoraTextureEnter() {
    LOADTEXTURE_INFO2 tex[3] = {
        {"#frame_image3#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
        {nullptr,                                     0, 0},
        {nullptr,                                     0, 0}
    };

    tex[1].block_no = tex[0].block_no = AtoraTextureReadBlock;
    BG_READ_INFO *bg = GetReadBGFile(0);
    std::string   name = std::format("a{}.img", MenuAtoraSel.map_no + 1);
    tex[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(bg->buffer), name.data(), nullptr));
    TexManager.DeleteTextureBlock(AtoraTextureReadBlock);
    TexManager.LoadTextureBlockEX(-1, tex);
    CompleteTex = TexManager.GetTexture("complete", -1);
    Sozai = TexManager.GetTexture("sozai", AtoraTextureReadBlock);
    HoleGray = TexManager.GetTexture("holegray", AtoraTextureReadBlock);
    HoleGold = TexManager.GetTexture("holegold", AtoraTextureReadBlock);
    ObTip = TexManager.GetTexture("obtip", AtoraTextureReadBlock);
    ObPerson = TexManager.GetTexture("obperson", AtoraTextureReadBlock);
    return 1;
}

/**
 * The chip group that the board's sort ranks first.
 */
int tip_sort_type = 1;

/** The rank that the board's sort gives each chip group, by group. */
std::array<int, 3> tip_table = {3, 1, 2};

/**
 * Returns which of the three chip groups a chip number belongs to.
 */
int GetTipKind(int tip_no) {
    if (tip_no < 0 || tip_no >= 100) {
        return 0;
    }

    if (0 <= tip_no && tip_no < 40) {
        return 1;
    }

    if (tip_no >= 40) {
        return 2;
    }

    return 0;
}

/**
 * Compares two chips for the board's sort by group, with empty slots last, and
 * then by number.
 */
int CompTip(int tip_a, int tip_b) {
    int rank_a = tip_table[GetTipKind(tip_a)];
    int rank_b = tip_table[GetTipKind(tip_b)];

    if (tip_a < 0) {
        rank_a = 3;
    }

    if (tip_b < 0) {
        rank_b = 3;
    }

    if (rank_a > rank_b) {
        return 1;
    }

    if (rank_a < rank_b) {
        return -1;
    }

    if (tip_a > tip_b) {
        return 1;
    }

    return (tip_a < tip_b) ? -1 : 0;
}

/**
 * Sorts the chip list with the current group first and empty slots last, and
 * returns whether any chip moved.
 */
int SeitonAtoraTipBoardSub() {
    int kind = tip_sort_type;

    for (int rank = 0; rank < 3; rank++) {
        tip_table[kind] = rank;
        kind++;

        if (kind >= 3) {
            kind = 0;
        }
    }

    tip_table[0] = 3;
    int  moved = 0;
    s16 *list = MenuAtoraSel.board.atla_elements;

    for (int i = 0; i < 119; i++) {
        for (int j = i + 1; j < 120; j++) {
            if (CompTip(list[i], list[j]) > 0) {
                MenuDataSwap(&list[i], &list[j]);
                moved = 1;
            }
        }
    }

    return moved;
}

/**
 * Sorts the chip list, switching which chip group comes first whenever the list
 * is already in order, up to three times.
 */
void SeitonAtoraTipBoard() {
    for (int i = 0; i < 3; i++) {
        if (SeitonAtoraTipBoardSub()) {
            break;
        }

        tip_sort_type++;

        if (tip_sort_type >= 3) {
            tip_sort_type = 1;
        }
    }
}

} // namespace

int MenuAtoraSelectKey() {
    int             max_village = 0;
    int             prev_village = 0;
    int             msg_base = 0;
    int             max_pos = 0;
    int             event_no = 0;
    int             tip_no = 0;
    int             held = 0;
    EDITPARTS_INFO *info = nullptr;

    int result = ATORA_SELECT_NONE;

    if (ReadBGSync() == 0 && AtoraTextureEnterFlag == 0) {
        AtoraTextureEnterFlag = AtoraTextureEnter();
    }

    switch (MenuAtoraSel.step) {
        case ATORA_STEP_FADE_IN:
            if (MenuAtoraSel.step_count > 15 && AtoraTextureEnterFlag) {
                MenuAtoraSel.step = ATORA_STEP_RUN;
                MenuAtoraAfterFadeIn();
            }

            break;
        case ATORA_STEP_FADE_OUT:
            if (MenuAtoraSel.step_count > 18) {
                ExitAtoraSelect();
                MenuAtoraSel.step = ATORA_STEP_RUN;
            }

            break;
        case ATORA_STEP_EVENT_START:
            if (MenuAtoraSel.map_no != 5) {
                switch (MenuAtoraSel.open_mode) {
                    case ATORA_OPEN_PLACE:
                        CommonMenuAtoraInfo->GetPartsInfo(MenuAtoraSel.board_pos);
                        MenuAtoraSel.step = ATORA_STEP_RUN;
                        EditMenuStatus.mode = EDIT_MENU_MODE_EVENT;
                        MenuAtoraSel.last_board_pos = EditMenuStatus.event_no = MenuAtoraSel.board_pos;
                        ExitAtoraSelect();
                        GamePad.AutoRepeatOff();
                        GamePad.MenuModeOff();
                        return ATORA_SELECT_PLACE_EXIT;
                }
            } else {
                SetMenuAtraEventFlag(1);
                MenuAtoraSel.step = ATORA_STEP_EVENT_FADE_OUT;
                GetPrevEastKingSndVol();
                SndBgmFadeOut(45, 0);
            }

            break;
        case ATORA_STEP_EVENT_FADE_OUT:
            if (MenuAtoraSel.step_count < 50) {
                SndStep();
            }

            if (MenuAtoraSel.step_count == 50) {
                SndBgmStop();
            }

            if (MenuAtoraSel.step_count > 70) {
                CommonMenuAtoraInfo->Save(MenuAtoraSel.map_no, SaveData);
                MenuAtoraSel.step = ATORA_STEP_EVENT_PLAY;
                std::array<int, 1> block = {AtoraTextureReadBlock};
                event_no = SearchAtoraInfo(MenuAtoraSel.board_pos)->parts_no;
                MenuAtoraSel.event_board_pos = MenuAtoraSel.board_pos;
                MenuAtoraSel.last_board_pos = MenuAtoraSel.board_pos;
                InitEastKingEvent(event_no, block.data(), AtoraOffsetBuf);
            }

            break;
        case ATORA_STEP_EVENT_PLAY:
            if (EastKingEventKey() == 1) {
                MenuAtoraSel.step = ATORA_STEP_EVENT_FADE_IN;
                AtoraTextureEnterFlag = 0;
                InitMenuAtoraSelect(MenuAtoraSel.map_no);
                SetMenuAtraEventFlag(0);
                MenuAtoraSel.step_count = 0;
            }

            break;
        case ATORA_STEP_EVENT_FADE_IN:
            SndStep();

            if (MenuAtoraSel.step_count > 64) {
                MenuAtoraSel.step = ATORA_STEP_RUN;
            }

            break;
        case ATORA_STEP_COMPLETE_FLASH:
            if (MenuAtoraSel.step_count > 40) {
                MenuAtoraSel.step = ATORA_STEP_RUN;
            }

            break;
        case ATORA_STEP_WARNING:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS)) {
                MenuAtoraSel.step = ATORA_STEP_RUN;
            }

            break;
        case ATORA_STEP_CHANGE_MAP:
            if (MenuAtoraSel.step_count > 24 && AtoraTextureEnterFlag) {
                MenuAtoraSel.step = ATORA_STEP_RUN;
            }

            [[fallthrough]];
        case ATORA_STEP_RUN:
            switch (MenuAtoraSel.mode) {
                case ATORA_SIDE_BOARD:
                    result = AtoraBoardKey();
                    break;
                case ATORA_SIDE_CHIP_LIST:
                    result = AtoraTipKey();
                    break;
            }

            max_village = GetAtoraMaxVillage();
            prev_village = MenuAtoraSel.board.page;

            if (GamePad.Down(PAD_R2 | PAD_R1)) {
                if (NowTipHavePt->tip_no < 0) {
                    MenuAtoraSel.board.page++;

                    if (MenuAtoraSel.board.page > max_village) {
                        MenuAtoraSel.board.page = BOARD_PAGE_ATLA_VILLAGE;
                    }
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            }

            if (GamePad.Down(PAD_L2 | PAD_L1)) {
                if (NowTipHavePt->tip_no < 0) {
                    MenuAtoraSel.board.page--;

                    if (MenuAtoraSel.board.page < BOARD_PAGE_ATLA_VILLAGE) {
                        MenuAtoraSel.board.page = max_village;
                    }
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            }

            if (prev_village != MenuAtoraSel.board.page) {
                result = ATORA_SELECT_CHANGE_PAGE;
            }

            switch (result) {
                case ATORA_SELECT_PLACE:
                    info = SearchAtoraInfo(MenuAtoraSel.board_pos);
                    EditMenuStatus.mode = EDIT_MENU_MODE_PLACE;
                    EditMenuStatus.parts = info->parts_no;
                    MenuAtoraSel.last_board_pos = MenuAtoraSel.board_pos;
                    ExitAtoraSelect();
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    break;
                case ATORA_SELECT_CHANGE_PAGE:
                    if (NowTipHavePt->tip_no >= 0) {
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                        break;
                    }

                    CommonMenuAtoraInfo->Save(MenuAtoraSel.map_no, SaveData);
                    MenuAtoraSel.map_no = MenuAtoraSel.board.page - BOARD_PAGE_ATLA_VILLAGE;
                    AtoraTextureEnterFlag = 0;
                    max_pos = AtraBoardMaxNum(MenuAtoraSel.map_no);

                    if (max_pos <= MenuAtoraSel.board_pos) {
                        MenuAtoraSel.board_pos = max_pos;
                    }

                    MenuAtoraSel.last_board_pos = MenuAtoraSel.board_pos;
                    InitMenuAtoraSelect(MenuAtoraSel.map_no);
                    MenuAtoraSel.step = ATORA_STEP_CHANGE_MAP;
                    MenuAtoraSel.step_count = 0;
                    MenuAtoraSel.name_alpha = 0;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    break;
                case ATORA_SELECT_CLOSE:
                    MenuAtoraSel.step = ATORA_STEP_FADE_OUT;
                    MenuAtoraSel.step_count = 0;
                    CommonMenuAtoraInfo->Save(MenuAtoraSel.map_no, SaveData);
                    ExitAtoraSelect();
                    break;
            }

            break;
    }

    int msg_no = 0;

    switch (MenuAtoraSel.mode) {
        case ATORA_SIDE_BOARD:
            tip_no = AtoraMsgNoGet(MenuAtoraSel.map_no, MenuAtoraSel.board_pos, MenuAtoraSel.board.cursor);
            msg_no = tip_no + 1000;

            if (tip_no < 0) {
                msg_no = 0;
            }

            if (tip_no == -788) {
                msg_no = 212;
            }

            break;
        case ATORA_SIDE_CHIP_LIST:
            held = MenuAtoraSel.board.atla_elements[MenuAtoraSel.board.cursor];
            msg_base = MenuAtoraSel.map_no * 200 + 1000;

            if (held > -1) {
                msg_no = msg_base + AtoraTipOnlyMsgNoGet(MenuAtoraSel.map_no, held);
            } else if (NowTipHavePt->tip_no > -1) {
                msg_no = msg_base + AtoraTipOnlyMsgNoGet(MenuAtoraSel.map_no, NowTipHavePt->tip_no);
            }

            if (msg_no < 0) {
                msg_no = 0;
            }

            break;
    }

    if (AtoraTextureEnterFlag == 0 || MenuAtoraSel.step == ATORA_STEP_FADE_IN) {
        msg_no = 0;
    }

    if (CommonMenuMes2.mes_made != msg_no) {
        CommonMenuMes2.MakeMesWin(msg_no);
    }

    return result;
}

namespace {

int AtoraBoardKey() {
    std::array<int, 8> movable{};
    int                open_mode = MenuAtoraSel.open_mode;
    int                max = AtraBoardMaxNum(MenuAtoraSel.map_no);
    int                old_cursor = MenuAtoraSel.board.cursor;
    int                old_pos = MenuAtoraSel.board_pos;
    int                moved = 0;
    int                result = ATORA_SELECT_NONE;
    int                se = -1;
    int                to = 0;

    if (GamePad.Down(PAD_UP) != 0) {
        moved = 1;

        if (MenuAtoraSel.board.cursor == 0) {
            if (0 < MenuAtoraSel.board_pos) {
                MenuAtoraSel.board_pos--;
            }

            MenuAtoraSel.board.cursor = 0;
        } else if (MenuAtoraSel.board.cursor >= 4 && MenuAtoraSel.board.cursor < 7) {
            MenuAtoraSel.board.cursor -= 3;
        } else if (0 < MenuAtoraSel.board_pos) {
            MenuAtoraSel.board_pos--;
            AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable.data());
            MenuAtoraSel.board.cursor += 3;
            to = MenuAtoraSel.board.cursor - 1;
            to = AtoraBoardGoToPos(movable.data(), to, 3);

            if (movable[to] != 0) {
                MenuAtoraSel.board.cursor = to + 1;
            } else {
                MenuAtoraSel.board.cursor -= 3;
                to = MenuAtoraSel.board.cursor - 1;
                to = AtoraBoardGoToPos(movable.data(), to, 0);

                if (movable[to] != 0) {
                    MenuAtoraSel.board.cursor = to + 1;
                } else {
                    MenuAtoraSel.board.cursor = 0;
                }
            }
        }
    } else if (GamePad.Down(PAD_DOWN) != 0) {
        moved = 1;

        if (MenuAtoraSel.board.cursor == 0) {
            if (MenuAtoraSel.board_pos < max - 1) {
                MenuAtoraSel.board_pos++;
            }
        } else if (MenuAtoraSel.board.cursor >= 4 && MenuAtoraSel.board.cursor < 7) {
            if (MenuAtoraSel.board_pos < max - 1) {
                MenuAtoraSel.board_pos++;
                MenuAtoraSel.board.cursor -= 3;
                AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable.data());
                to = MenuAtoraSel.board.cursor - 1;
                to = AtoraBoardGoToPos(movable.data(), to, 0);

                if (movable[to] != 0) {
                    MenuAtoraSel.board.cursor = to + 1;
                } else {
                    MenuAtoraSel.board.cursor = 0;
                }
            }
        } else {
            AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable.data());
            MenuAtoraSel.board.cursor += 3;
            to = MenuAtoraSel.board.cursor - 1;
            to = AtoraBoardGoToPos(movable.data(), to, 3);

            if (movable[to] != 0) {
                MenuAtoraSel.board.cursor = to + 1;
            } else if (MenuAtoraSel.board_pos < max - 1) {
                MenuAtoraSel.board_pos++;
                MenuAtoraSel.board.cursor -= 3;
                AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable.data());
                to = MenuAtoraSel.board.cursor - 1;
                to = AtoraBoardGoToPos(movable.data(), to, 0);

                if (movable[to] != 0) {
                    MenuAtoraSel.board.cursor = to + 1;
                } else {
                    MenuAtoraSel.board.cursor = 0;
                }
            }
        }
    } else if (GamePad.Down(PAD_LEFT) != 0) {
        moved = 1;
        MenuAtoraSel.board.cursor--;

        if (MenuAtoraSel.board.cursor <= 0 || MenuAtoraSel.board.cursor == 3) {
            MenuAtoraSel.board.cursor = 0;
        }
    } else if (GamePad.Down(PAD_RIGHT) != 0) {
        moved = 1;
        EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);
        AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable.data());
        int event = 0;

        if (info != nullptr) {
            event = AtoraCompOrEvent(info);
        }

        int to_list = 0;

        if (info == nullptr || event != 0) {
            to_list = 1;
        } else if (movable[MenuAtoraSel.board.cursor] != 0) {
            MenuAtoraSel.board.cursor++;

            if (MenuAtoraSel.board.cursor == 4 || MenuAtoraSel.board.cursor == 7) {
                to_list = 1;
            }
        } else {
            to_list = 1;
        }

        if (to_list != 0) {
            MenuAtoraSel.mode = ATORA_SIDE_CHIP_LIST;
            MenuAtoraSel.board.cursor = MenuAtoraSel.board.top_row * 5 + 5;
        }

        if (MenuAtoraSel.mode == ATORA_SIDE_CHIP_LIST) {
            MenuAtoraSel.board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
        }
    }

    if (MenuAtoraSel.mode == ATORA_SIDE_BOARD) {
        EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);
        int             event = 0;

        if (info != nullptr) {
            event = AtoraCompOrEvent(info);
        }

        if (event != 0) {
            MenuAtoraSel.board.cursor = 0;
        }
    }

    if (moved == 0) {
        if (GamePad.Down(PAD_CROSS) != 0) {
            EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);

            if (info != nullptr) {
                int full = 0;

                if (info->placed == info->stock) {
                    full = 1;
                }

                AtoraCompOrEvent(info);
                int all_tips = AtoraAllTipGet(info->parts_no);

                if (MenuAtoraSel.open_mode == ATORA_OPEN_WARN) {
                    MenuAtoraSel.step = ATORA_STEP_WARNING;
                    se = 2;
                } else if (MenuAtoraSel.board.cursor == 0) {
                    int held = NowTipHavePt->tip_no;

                    if (0 <= held) {
                        se = 2;
                    } else if (full == 0 && held < 0 && open_mode == ATORA_OPEN_PLACE && NowEditMap == MenuAtoraSel.map_no) {
                        MenuAtoraSel.last_board_pos = MenuAtoraSel.board_pos;
                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                        return ATORA_SELECT_PLACE;
                    } else if (all_tips != 0 && MenuAtoraSel.map_no == 5) {
                        MenuAtoraSel.step = ATORA_STEP_EVENT_START;
                        MenuAtoraSel.step_count = 0;
                        MenuAtoraSel.last_board_pos = MenuAtoraSel.board_pos;
                    } else {
                        se = 2;
                    }
                } else if (all_tips != 0) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    return ATORA_SELECT_NONE;
                } else {
                    AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable.data());
                    int slot = MenuAtoraSel.board.cursor - 1;

                    if (NowTipHavePt->tip_no < 0) {
                        if (info->elements[slot].enabled == 0) {
                            se = 2;
                        } else {
                            int free = 1;

                            for (int i = slot; i < 6; i++) {
                                if (slot == info->elements[i].required_element && info->elements[i].enabled != 0) {
                                    free = 0;
                                }
                            }

                            if (free != 0) {
                                NowTipHavePt->tip_no = info->elements[slot].id;
                                info->elements[slot].enabled = false;
                                NowTipHavePt->slot = slot;
                                NowTipHavePt->mode = ATORA_SIDE_BOARD;
                                NowTipHavePt->parts_no = MenuAtoraSel.board_pos;
                                se = 6;
                            } else {
                                se = 2;
                            }
                        }
                    } else {
                        EDITPARTS_ELEMENT     *element = &info->elements[slot];
                        EDIT_CHIP_ATTACH_DATA *chip = &GetEditAtraPartsData(MenuAtoraSel.map_no, info->parts_no)->elements[slot];
                        int                    fits = 1;

                        if (NowTipHavePt->tip_no != element->id) {
                            fits = 0;
                        }

                        if (element->enabled != 0) {
                            fits = 0;
                        }

                        int linked = element->required_element;

                        if (linked >= 0 && info->elements[linked].enabled == 0) {
                            fits = 0;
                        }

                        if (AtraTipCanDisplay(chip) == 0) {
                            fits = 0;
                        }

                        if (fits != 0) {
                            element->enabled = true;

                            if (info == nullptr) {
                                return ATORA_SELECT_NONE;
                            }

                            if (AtoraAllTipGet(info->parts_no) != 0) {
                                ComMenuSePlay(SE_SPARKLE);
                                MenuAtoraSel.step = ATORA_STEP_COMPLETE_FLASH;
                                MenuAtoraSel.board.cursor = 0;
                                MenuAtoraSel.step_count = 0;
                            } else {
                                se = 5;
                            }

                            AtoraTipInfoInit();
                        } else {
                            se = 2;
                        }
                    }
                }
            }
        } else if (GamePad.Down(PAD_CIRCLE) != 0) {
            se = 2;

            if (NowTipHavePt->tip_no < 0) {
                MenuAtoraSel.last_board_pos = MenuAtoraSel.board_pos;
                result = ATORA_SELECT_CLOSE;
            } else {
                AtoraMenuTipCancel();
            }
        }
    }

    // Debug shortcuts that fill, empty, dump or complete the parts of the board.
    if (DebugMode) {
        EDITPARTS_INFO *parts = CommonMenuAtoraInfo->GetPartsInfo(MenuAtoraSel.board_pos);

        if (GamePad.Down2(PAD_CROSS)) {
            if (parts != nullptr) {
                for (EDITPARTS_ELEMENT &element : parts->elements) {
                    if (element.id >= 0) {
                        element.enabled = true;
                    }
                }

                parts->completion_flags = 1;
            }

            MenuAtoraSel.board.cursor = 0;
        }

        if (GamePad.Down2(PAD_CIRCLE)) {
            if (parts != nullptr) {
                for (EDITPARTS_ELEMENT &element : parts->elements) {
                    if (element.id >= 0) {
                        element.enabled = false;

                        for (int j = 0; j < 100; j++) {
                            if (MenuAtoraSel.board.atla_elements[j] < 0) {
                                MenuAtoraSel.board.atla_elements[j] = element.id;
                                break;
                            }
                        }
                    }
                }
            }

            parts->completion_flags = 0;
        }

        if (GamePad.Down2(PAD_R1) && parts != nullptr) {
            printf("info----------,,,\tID \t\t%d\n", parts->parts_no);
            printf("\t\tcomplete_event\t\t%d\n", parts->kind);

            for (EDITPARTS_ELEMENT &element : parts->elements) {
                if (element.id < 0) {
                    break;
                }

                if (element.enabled != 0) {
                    for (int j = 0; j < 120; j++) {
                        if (MenuAtoraSel.board.atla_elements[j] < 0) {
                            MenuAtoraSel.board.atla_elements[j] = element.id;
                            element.enabled = false;
                            break;
                        }
                    }
                }
            }

            parts->completion_flags = 0;
        }

        if (GamePad.Down2(PAD_L1)) {
            if (parts != nullptr) {
                for (EDITPARTS_ELEMENT &element : parts->elements) {
                    if (element.id >= 0) {
                        element.enabled = true;
                        int npc_no = element.npc_no;

                        if (npc_no >= 0) {
                            SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                            if (npc != nullptr) {
                                npc->talk_message++;
                            }
                        }
                    }
                }

                parts->completion_flags = 0;
            }

            MenuAtoraSel.board.cursor = 0;
        }

        if (GamePad.Down2(PAD_TRIANGLE)) {
            EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);

            if (info != nullptr) {
                for (EDITPARTS_ELEMENT &element : info->elements) {
                    if (element.id >= 0) {
                        element.enabled = true;

                        int npc_no = element.npc_no;

                        if (npc_no >= 0) {
                            SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                            if (npc != nullptr) {
                                npc->talk_message++;
                            }
                        }
                    }
                }

                if (AtoraCompOrEvent(info) != 0 && open_mode == ATORA_OPEN_PLACE && NowEditMap == MenuAtoraSel.map_no) {
                    MenuAtoraSel.step = ATORA_STEP_EVENT_START;
                }

                ComMenuSePlay(MENU_SOUND_CONFIRM);
            }
        }

        if (GamePad.Down2(PAD_CROSS) && GamePad.Down2(PAD_CIRCLE)) {
            for (int k = 0; k < max; k++) {
                EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(k);

                if (info != nullptr) {
                    info->obtained = 1;

                    for (EDITPARTS_ELEMENT &element : info->elements) {
                        if (element.id >= 0) {
                            element.enabled = true;

                            int npc_no = element.npc_no;

                            if (npc_no >= 0) {
                                SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                                if (npc != nullptr) {
                                    npc->talk_message++;
                                }
                            }
                        }
                    }
                }
            }

            ComMenuSePlay(MENU_SOUND_CONFIRM);
        }

        if (GamePad.Down2(PAD_CROSS) && GamePad.Down2(PAD_DOWN)) {
            for (int k = 0; k < max; k++) {
                EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(k);

                if (info != nullptr) {
                    for (EDITPARTS_ELEMENT &element : info->elements) {
                        if (element.id >= 0) {
                            int npc_no = element.npc_no;

                            if (npc_no >= 0) {
                                SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                                if (npc != nullptr) {
                                    npc->flags |= 2;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (old_cursor != MenuAtoraSel.board.cursor || old_pos != MenuAtoraSel.board_pos) {
        se = 0;
    }

    ComMenuSePlay(se);
    return result;
}

int AtoraTipKey() {
    int result = ATORA_SELECT_NONE;
    int pos = MenuAtoraSel.board.cursor;
    int mode = MenuAtoraSel.mode;
    int page = MenuAtoraSel.board.page;

    switch (PersonalBoardKey()) {
        case 1: {
            EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);
            int             event = 0;

            if (info != nullptr) {
                event = AtoraCompOrEvent(info);
            }

            std::array<int, 6> enable{};
            AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, enable.data());
            MenuAtoraSel.mode = ATORA_SIDE_BOARD;

            if (info == nullptr || event != 0) {
                MenuAtoraSel.board.cursor = 0;
            } else {
                int slot = AtoraBoardGoToPos(enable.data(), MenuAtoraSel.board.cursor < MenuAtoraSel.board.top_row * 5 + 10 ? 2 : 5, 0);

                if (enable[slot]) {
                    MenuAtoraSel.board.cursor = slot + 1;
                } else {
                    MenuAtoraSel.board.cursor = 0;
                }
            }

            ComMenuSePlay(MENU_SOUND_CURSOR);
            break;
        }
    }

    if (MenuAtoraSel.mode == ATORA_SIDE_CHIP_LIST) {
        if (pos != MenuAtoraSel.board.cursor || mode != MenuAtoraSel.mode || page != MenuAtoraSel.board.page) {
            ComMenuSePlay(MENU_SOUND_CURSOR);
        }

        if (GamePad.Down(PAD_CROSS)) {
            s16 *tip = &MenuAtoraSel.board.atla_elements[MenuAtoraSel.board.cursor];

            if (NowTipHavePt->tip_no == *tip) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
            } else {
                ComMenuSePlay(MENU_SOUND_CONFIRM);
                s16 tip_no = *tip;
                *tip = NowTipHavePt->tip_no;
                NowTipHavePt->tip_no = tip_no;
                NowTipHavePt->slot = MenuAtoraSel.board.cursor;
                NowTipHavePt->mode = ATORA_SIDE_CHIP_LIST;
                NowTipHavePt->parts_no = -1;
            }

            return ATORA_SELECT_NONE;
        } else if (GamePad.Down(PAD_CIRCLE)) {
            ComMenuSePlay(MENU_SOUND_REFUSE);

            if (NowTipHavePt->tip_no < 0) {
                result = ATORA_SELECT_CLOSE;
            } else {
                AtoraMenuTipCancel();
            }
        } else if (GamePad.Down(PAD_SQUARE)) {
            ComMenuSePlay(MENU_SOUND_CONFIRM);
            SeitonAtoraTipBoard();
        }
    }

    return result;
}

void AtoraMenuTipCancel() {
    switch (NowTipHavePt->mode) {
        case ATORA_SIDE_CHIP_LIST: {
            s16 tip_no = MenuAtoraSel.board.atla_elements[NowTipHavePt->slot];
            MenuAtoraSel.board.atla_elements[NowTipHavePt->slot] = NowTipHavePt->tip_no;
            NowTipHavePt->tip_no = tip_no;
            break;
        }
        case ATORA_SIDE_BOARD: {
            EDITPARTS_INFO    *info = SearchAtoraInfo(NowTipHavePt->parts_no);
            EDITPARTS_ELEMENT &element = info->elements[NowTipHavePt->slot];

            if (element.id == NowTipHavePt->tip_no && element.enabled == 0) {
                element.enabled = true;
            }

            MenuAtoraSel.board_pos = NowTipHavePt->parts_no;
            NowTipHavePt->tip_no = -1;
            break;
        }
    }
}

void AtoraBoardFadeEffect() {
    CTexture frame = *TexManager.GetTexture("frame_image", -1);

    reinterpret_cast<sceGsTex0 *>(&frame.tex0)->bits.tcc = 0;
    sceGsTexa texa = mgTexa;
    texa.AEM = 1;
    texa.TA0 = 0x80;
    MGSetGsTEXA(&texa);
    spRGBA top;
    spRGBA bottom;
    bottom.r = bottom.g = bottom.b = 0x40;
    top.r = top.g = top.b = 0x40;
    bottom.a = 0x80;
    top.a = 0x80;
    set2DSprite(Vif1Packet, &frame, CRect_i_(0, 0, 320, 1), CRect_i_(0, 0, 320, 1), &top, &top, &bottom, &bottom, 1);
    set2DSprite(Vif1Packet, &frame, CRect_i_(0, 1, 320, 60), CRect_i_(0, 0, 320, 61), &top, &top, &bottom, &bottom, 1);
    top.a = 0x80;
    bottom.a = 0;
    set2DSprite(Vif1Packet, &frame, CRect_i_(0, 61, 320, 59), CRect_i_(0, 60, 320, 60), &top, &top, &bottom, &bottom, 1);
    top.a = 0;
    bottom.a = 0x80;
    set2DSprite(Vif1Packet, &frame, CRect_i_(0, 277, 320, 89), CRect_i_(0, 276, 320, 90), &top, &top, &bottom, &bottom, 1);
    bottom.a = 0x80;
    top.a = 0x80;
    set2DSprite(Vif1Packet, &frame, CRect_i_(0, 366, 320, 1), CRect_i_(0, 366, 320, 1), &top, &top, &bottom, &bottom, 1);
    set2DSprite(Vif1Packet, &frame, CRect_i_(0, 367, 320, 113), CRect_i_(0, 366, 320, 114), &top, &top, &bottom, &bottom, 1);
    MGSetGsTEXA(nullptr);
}

} // namespace

void AtoraNameDraw(int unused) {
    std::array<int, 3> prev_mes_no{};

    MenuTextureReload(AtoraNameMes.tex_block);
    int pos = MenuAtoraSel.board_pos - 1;

    for (int j = 0; j < 3; j++) {
        prev_mes_no[j] = AtoraNameMes.mes_no[j];
    }

    int y = MenuAtoraSel.scroll_y + 92 + 130.0f * pos;

    for (int i = 0; i < 3; i++) {
        int mes_no = 999;

        if (y >= 96 && y <= 341) {
            EDITPARTS_INFO *info = SearchAtoraInfo(pos);

            if (info != nullptr) {
                mes_no = GetAtraMsgNo(MenuAtoraSel.map_no, info->parts_no);
            }
        }

        AtoraNameMes.mes_no[i] = mes_no;
        int x = GetMsgLengthMenu(&AtoraNameMes, mes_no);

        if (GetMenuLangFlag() == LANG_JAPANESE && mes_no == 1000) {
            x = GetMsgLengthCharaName(0) + 3;
        }

        x = 186.0f - 58.0f * (x / 10.0f);

        if (GetMenuLangFlag() > LANG_JAPANESE) {
            if (mes_no == 2001) {
                x += 6;
            }

            if (mes_no == 2002) {
                x += 8;
            }
        }

        if (i >= 0 && i < 10) {
            AtoraNameMes.line_pos[i].x = x;
            AtoraNameMes.line_pos[i].y = y;
        }

        y += 130.0f;
        pos++;
    }

    for (int k = 0; k < 3; k++) {
        if (AtoraNameMes.mes_no[k] != prev_mes_no[k]) {
            AtoraNameMes.mes_made = -1;
            AtoraNameMes.MakeMesWin(210);
            break;
        }
    }

    int alpha = MenuAtoraSel.name_alpha;

    switch (MenuAtoraSel.step) {
        case ATORA_STEP_EVENT_FADE_OUT:
            alpha = 0x80 - MenuAtoraSel.step_count * 2;

            if (alpha < 0) {
                alpha = 0;
            }

            break;
        case ATORA_STEP_EVENT_FADE_IN:
            alpha = MenuAtoraSel.step_count * 2;

            if (alpha > 0x80) {
                alpha = 0x80;
            }

            break;
        case ATORA_STEP_EVENT_PLAY:
            alpha = 0;
            break;
    }

    AtoraNameMes.edge_alpha = alpha;
    AtoraNameMes.Step();
    AtoraNameMes.DrawMesWin();

    if (GetMenuAtraEventFlag() == 0) {
        AtoraBoardFadeEffect();
    }
}

int InitMenuOption(int mode, int block_no, u_long128 *buffer) {
    return options::Open(mode, block_no, buffer);
}

int MenuOptionKey() {
    return options::Run();
}

void DrawMenuOption() {
    options::Draw();
}

int OptionMenuFadeOutStart() {
    return options::g_screen.step == OPTION_STEP_FADE_OUT;
}

int InitMenuSave(int mode, int block_no, u_long128 *buffer) {
    u_long128 *data = buffer;

    if (buffer == nullptr) {
        data = reinterpret_cast<u_long128 *>(read_buffer);
    }

    data = MenuCalcBufAlignment(data);
    SaveMenu.mode = mode;
    SaveMenu.block_no = block_no;
    SaveMenu.result = MENU_SAVE_RUNNING;
    SaveMenu.step_time = 0;
    SaveMenu.texture_ready = false;
    SaveMenu.file_no = 0;
    SaveMenu.loaded = false;
    StartReadBG();
    LoadFileBGMenuData("savetex.pak", data);

    if (McAccess.InitForMC()) {
        return 0;
    }

    switch (SaveMenu.mode) {
        case SAVE_MENU_MODE_LOAD:
            GamePad.SetAutoRepeat(PAD_RIGHT | PAD_LEFT, 30, 5);
            GamePad.MenuModeOn(120);
            SaveMenu.key_no = SAVE_KEY_MC_SELECT;
            SaveMenu.access_kind = SAVE_ACCESS_LOAD;
            break;
        case SAVE_MENU_MODE_SAVE:
            SaveMenu.key_no = SAVE_KEY_MC_SELECT;
            SaveMenu.access_kind = SAVE_ACCESS_SAVE;
            EditSave();
            break;
        case SAVE_MENU_MODE_ENDING:
        case SAVE_MENU_MODE_ENDING_NO_CLEAR:
            GameClearWord() = SaveMenu.mode == SAVE_MENU_MODE_ENDING;
            printf("SaveData clear flag = %d\n", GameClearWord());
            GameClearFlag = SaveMenu.mode == SAVE_MENU_MODE_ENDING;
            SaveMenu.mode = SAVE_MENU_MODE_ENDING;
            GamePad.SetAutoRepeat(PAD_RIGHT | PAD_LEFT, 30, 5);
            GamePad.MenuModeOn(120);
            SaveMenu.key_no = SAVE_KEY_AFTER_ENDING;
            SaveMenu.access_kind = SAVE_ACCESS_SAVE;
            break;
    }

    SaveMenu.return_key_no = -1;
    CommonMenuMes2.stay_frame = true;
    CommonMenuMes2.value_show = true;
    CommonMenuMes2.auto_pos = MES_POS_CENTRE;
    McAccess.SetFuncNo(MC_OPERATION_IDLE);
    CommonMenuMes2.cursor_lit = true;
    return 1;
}

/**
 * Closes the save screen's message window and restores the pad, and after a
 * load sets the stereo mode from the loaded configuration.
 */
void ExitSaveSelect() {
    CommonMenuMes2.stay_frame = false;
    CommonMenuMes2.value_show = false;
    CommonMenuMes2.value_signed = true;
    CommonMenuMes2.auto_pos = MES_POS_NONE;

    switch (SaveMenu.mode) {
        case SAVE_MENU_MODE_LOAD:
            GamePad.AutoRepeatOff();
            GamePad.MenuModeOff();
            GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 9);
            GamePad.MenuModeOn(120);

            if (SaveMenu.loaded) {
                s32 *config = static_cast<s32 *>(SaveData->GetConfigData());

                if (config != nullptr) {
                    if (config[5]) {
                        CSnd.SetStereoMode(0);
                    } else {
                        CSnd.SetStereoMode(1);
                    }
                }
            }

            break;
        case SAVE_MENU_MODE_SAVE:
            break;
        case SAVE_MENU_MODE_ENDING:
            GamePad.AutoRepeatOff();
            GamePad.MenuModeOff();
            break;
    }

    CommonMenuMes2.cursor_lit = false;
}

// The save screens without a memory card: no card is chosen, and the boards are every save of
// SaveSlots in file order, then on the save screen one "New file" board for a new save.
// SaveMenu.file_no names the file a step works on.

namespace {

// Vertical distance between two boards, and the screen y of the chosen one.
constexpr float kBoardStep = 150.0f;

void LeaveSaveMenu() {
    switch (SaveMenu.mode) {
        case SAVE_MENU_MODE_LOAD:
        case SAVE_MENU_MODE_ENDING:
            SaveMenu.key_no = SAVE_KEY_FADE_OUT;
            ExitSaveSelect();
            break;
        case SAVE_MENU_MODE_SAVE:
            SaveMenu.key_no = SAVE_KEY_FADE_OUT;
            CommonMenuMes2.stay_frame = false;
            break;
        default:
            break;
    }

    SaveMenu.step_time = 0;
    ComMenuSePlay(MENU_SOUND_REFUSE);
}

bool Listed(int file_no) {
    return std::ranges::any_of(SaveSlots.saves, [file_no](const SAVEDATA_INFO &info) { return info.file_no == file_no + 1; });
}

bool NewSaveBoard() {
    return SaveMenu.access_kind == SAVE_ACCESS_SAVE;
}

// Replaces the card choice: the only card is taken at once.
int SaveMenuKeyOpen() {
    if (!SaveMenu.texture_ready) {
        return 1;
    }

    // The town and dungeon menus that open the save screen already repeat the d-pad; the title
    // repeats only left and right.
    if (SaveMenu.mode == SAVE_MENU_MODE_LOAD) {
        GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 30, 5);
    }

    SaveMenu.key_no = SAVE_KEY_CHECK_MC_TYPE;
    McAccess.port = 0;
    McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
    return 1;
}

int SaveMenuKeyFileSelect() {
    bool new_save = NewSaveBoard();
    int  rows = SaveSlotRows(SaveSlots, new_save);
    int  row = SaveSlotRow(SaveSlots, SaveMenu.file_no, new_save);
    int  prev_row = row;

    if (GamePad.Down(PAD_DOWN) && row < rows - 1) {
        row++;
    }

    if (GamePad.Down(PAD_UP) && row > 0) {
        row--;
    }

    // A long list has no other way to its ends.
    if (GamePad.Down(PAD_L1)) {
        row = 0;
    }

    if (GamePad.Down(PAD_R1) && rows > 0) {
        row = rows - 1;
    }

    if (rows > 0) {
        SaveMenu.file_no = SaveSlotFileAt(SaveSlots, row);
    }

    if (prev_row != row) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        LeaveSaveMenu();
        return 1;
    }

    if (GamePad.Down(PAD_CROSS)) {
        switch (SaveMenu.access_kind) {
            case SAVE_ACCESS_SAVE:
                SaveSlotNew = std::cmp_equal(row, SaveSlots.saves.size());
                SaveMenu.key_no = SAVE_KEY_SAVE_CHECK;
                McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
                ComMenuSePlay(MENU_SOUND_CONFIRM);
                break;
            case SAVE_ACCESS_LOAD:
                if (Listed(SaveMenu.file_no)) {
                    SaveMenu.key_no = SAVE_KEY_LOAD_DECIDE;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
            default:
                break;
        }
    }

    return 1;
}

// Ends the save with the cursor on the file written, which a new save may have moved on from.
int SaveMenuKeySaved() {
    SaveMenu.file_no = McAccess.file_no;
    return SaveMenuKeyEndSave();
}

// Confirms the load of the chosen save, looked up in SaveSlots.
int SaveMenuKeyLoadDecide() {
    if (GamePad.Down(PAD_CROSS)) {
        if (Listed(SaveMenu.file_no)) {
            McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
            McAccess.file_no = SaveMenu.file_no;
            SaveMenu.key_no = SAVE_KEY_LOAD;
            ComMenuSePlay(MENU_SOUND_CONFIRM);
        } else {
            ComMenuSePlay(MENU_SOUND_REFUSE);
        }

        return 1;
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
        ComMenuSePlay(MENU_SOUND_REFUSE);
    }

    return 1;
}

// Alerts after the ending: Cross asks again and Circle closes the screen, since the card choice
// would save again at once.
int SaveMenuKeyAlert() {
    if (SaveMenu.mode != SAVE_MENU_MODE_ENDING) {
        return SaveMenuKeyArart();
    }

    if (GamePad.Down(PAD_CROSS)) {
        SaveMenu.key_no = SAVE_KEY_AFTER_ENDING;
        ComMenuSePlay(MENU_SOUND_REFUSE);
    } else if (GamePad.Down(PAD_CIRCLE)) {
        LeaveSaveMenu();
    }

    return 1;
}

// The save after the ending writes the configuration alone and finishes the write here, so a
// failure reaches the alert.
int SaveMenuKeySaveEnding() {
    if (McAccess.SaveSysConfig() == 1) {
        SaveMenu.key_no = SAVE_KEY_END_SAVE_ENDING;
    } else {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_SAVE_FAILED;
    }

    return 1;
}

// Closes the screen after the ending's save; the card choice would save again.
int SaveMenuKeyEndSaveEnding() {
    if (GamePad.Down(PAD_CROSS)) {
        McAccess.SetFuncNo(MC_OPERATION_IDLE);
        SaveMenu.key_no = SAVE_KEY_FADE_OUT;
        SaveMenu.step_time = 0;
        ExitSaveSelect();
    }

    return 1;
}

} // namespace

/** The save menu's steps, by SAVE_MENU_STATE::key_no. */
int (*SaveMenuFunc[26])() = {
    SaveMenuKeyFadeIn,
    SaveMenuKeyFadeOut,
    SaveMenuKeyModeSelect,
    SaveMenuKeyOpen,
    SaveMenuKeyCheckMcType,
    SaveMenuKeyCheckMc,
    SaveMenuKeyLoadConfig,
    SaveMenuKeyFileSelect,
    SaveMenuKeySaveCheck,
    SaveMenuKeySaveDecide,
    SaveMenuKeySave,
    SaveMenuKeySaved,
    SaveMenuKeyLoadDecide,
    SaveMenuKeyLoad,
    SaveMenuKeyAlert,
    SaveMenuKeyNewDir,
    SaveMenuKeyNewDirSelect,
    SaveMenuKeyFormat,
    SaveMenuKeyUnFormat,
    SaveMenuKeyDifVersion,
    SaveMenuKeyDelete,
    SaveMenuKeyCopy,
    SaveMenuKeyAfterEnding,
    SaveMenuKeySaveEnding,
    SaveMenuKeySaveDecideEnding,
    SaveMenuKeyEndSaveEnding,
};

int MenuSaveKey() {
    if (!SaveMenu.texture_ready) {
        SaveMenu.texture_ready = SaveMenuTextureEnter();
    }

    int            prev_func_no = McAccess.GetFuncNo();
    int            result = McAccess.Step();
    int            now_func_no = McAccess.GetFuncNo();
    MC_ERROR_INFO *error = &McAccess.error;

    if (prev_func_no == now_func_no) {
        switch (now_func_no) {
            case MC_OPERATION_FORMAT:
                if (result < 0) {
                    SaveMenu.key_no = SAVE_KEY_ALERT;
                    SaveMenu.alert_no = SAVE_ALERT_FORMAT_FAILED;
                    McAccess.SetFuncNo(MC_OPERATION_IDLE);
                }

                break;
            case MC_OPERATION_MAKE_DIR:
            case MC_OPERATION_SAVE:
                if (result < 0) {
                    SaveMenu.key_no = SAVE_KEY_ALERT;
                    SaveMenu.alert_no = SAVE_ALERT_SAVE_FAILED;
                    McAccess.SetFuncNo(MC_OPERATION_IDLE);
                }

                break;
            case MC_OPERATION_LOAD:
                if (result < 0) {
                    SaveMenu.key_no = SAVE_KEY_ALERT;
                    SaveMenu.alert_no = SAVE_ALERT_LOAD_FAILED;
                    McAccess.SetFuncNo(MC_OPERATION_IDLE);
                }

                break;
            case MC_OPERATION_GET_DIR:
                error->code = MC_ERROR_NONE;
                [[fallthrough]];
            case MC_OPERATION_GET_ALL_SAVE_FILE_INFO:
                if (result < 0) {
                    SaveMenu.key_no = SAVE_KEY_ALERT;
                    SaveMenu.alert_no = SAVE_ALERT_CARD_ERROR;
                    McAccess.SetFuncNo(MC_OPERATION_IDLE);
                    break;
                }

                switch (error->code) {
                    case MC_ERROR_NONE:
                        break;
                    case MC_ERROR_VERSION:
                        SaveMenu.return_key_no = SaveMenu.key_no;
                        SaveMenu.key_no = SAVE_KEY_DIF_VERSION;
                        McAccess.SetFuncNo(MC_OPERATION_IDLE);
                        break;
                    case MC_ERROR_UNK_2:
                    case MC_ERROR_SHORT_READ:
                        break;
                    case MC_ERROR_FULL:
                        SaveMenu.key_no = SAVE_KEY_ALERT;
                        SaveMenu.alert_no = SAVE_ALERT_CARD_FULL;
                        break;
                }

                break;
        }
    } else {
        switch (prev_func_no) {
            case MC_OPERATION_GET_ALL_SAVE_FILE_INFO:
                break;
            case MC_OPERATION_LOAD:
                switch (SaveMenu.mode) {
                    case SAVE_MENU_MODE_LOAD:
                        SaveMenu.key_no = SAVE_KEY_FADE_OUT;
                        SaveMenu.loaded = true;
                        McAccess.DmySync();
                        break;
                    case SAVE_MENU_MODE_SAVE:
                        EditLoad();
                        break;
                    case SAVE_MENU_MODE_ENDING:
                        break;
                }

                break;
            case MC_OPERATION_DELETE:
                switch (error->code) {
                    case MC_ERROR_VERSION:
                        McAccess.SetFuncNo(MC_OPERATION_GET_ALL_SAVE_FILE_INFO);
                        McAccess.step = error->step;

                        if (SaveMenu.return_key_no >= 0) {
                            SaveMenu.key_no = SaveMenu.return_key_no;
                        }

                        if (McAccess.step > 49) {
                            McAccess.step = 0;
                        }

                        error->code = MC_ERROR_NONE;
                        error->retry_count = 0;
                        break;
                    case MC_ERROR_NONE:
                        McAccess.SetFuncNo(MC_OPERATION_GET_ALL_SAVE_FILE_INFO);
                        break;
                }

                break;
            case MC_OPERATION_SAVE:
                ComMenuSePlay(SE_SAVE_START);
                McAccess.SetFuncNo(MC_OPERATION_GET_ALL_SAVE_FILE_INFO);
                break;
        }
    }

    switch (McAccess.GetFuncNo()) {
        case MC_OPERATION_IDLE:
            SaveMenuFunc[SaveMenu.key_no]();

            // Debug shortcuts that jump the card access to one of its steps.
            if (DebugMode) {
                if (GamePad.Down2(PAD_R1)) {
                    McAccess.SetFuncNo(MC_OPERATION_UNFORMAT);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }

                if (GamePad.Down2(PAD_TRIANGLE)) {
                    McAccess.SetFuncNo(MC_OPERATION_FORMAT);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }

                if (GamePad.Down2(PAD_CROSS)) {
                    McAccess.SetFuncNo(MC_OPERATION_WRITE_TEST);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    McAccess.Step();
                }

                if (GamePad.Down2(PAD_L3)) {
                    McAccess.SetFuncNo(MC_OPERATION_CONVERT);
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    McAccess.Step();
                }
            }

            break;
    }

    int msg_no = GetSaveMenuMsgNo();

    if (CommonMenuMes2.mes_made != msg_no) {
        int mes_value = SaveMenu.file_no + 1;

        switch (msg_no - 250) {
            case 3:
            case 6:
            case 7:
            case 17:
            case 29:
            case 30:
                mes_value = McAccess.port + 1;
                break;
        }

        CommonMenuMes2.value = mes_value;

        if (msg_no == 299) {
            CommonMenuMes2.value = McAccess.error.file_no;
        }

        printf("msgno = %d\n", msg_no);
        CommonMenuMes2.MakeMesWin(msg_no);
    }

    return SaveMenu.result;
}

// Draws the boards of SaveSlots: the chosen one at a fixed place and the rest scrolling past above
// and below it, only those on screen drawn. The boards stay up while the list is read again after
// a save.
void DrawMenuSave(char * /*frame_name*/) {
    if (SaveMenu.texture_ready == 0) {
        return;
    }

    setbilinear(0);

    switch (SaveMenu.mode) {
        case SAVE_MENU_MODE_LOAD:
        case SAVE_MENU_MODE_SAVE:
            break;
        case SAVE_MENU_MODE_ENDING:
            AllFillBoxForMenu(0, 0, 0, 0x80);
            break;
        default:
            break;
    }

    int alpha = 0x80;

    switch (SaveMenu.key_no) {
        case SAVE_KEY_FADE_IN:
            alpha = std::min(SaveMenu.step_time * 6, 0x80);
            break;
        case SAVE_KEY_FADE_OUT:
            alpha = std::max(0x80 - SaveMenu.step_time * 4, 0);
            break;
        default:
            break;
    }

    MenuTextureReload(SaveMenu.block_no);
    bool  new_save = NewSaveBoard();
    int   rows = SaveSlotRows(SaveSlots, new_save);
    float y = kBoardStep - kBoardStep * SaveSlotRow(SaveSlots, SaveMenu.file_no, new_save);
    SaveMenu.board_y += (y - SaveMenu.board_y) / 4.0f;
    y = SaveMenu.board_y;
    float board_x = 140.0f;
    int   bright = 0x80;

    switch (SaveMenu.key_no) {
        case SAVE_KEY_SAVE_DECIDE:
        case SAVE_KEY_LOAD_DECIDE:
        case SAVE_KEY_LOAD:
        case SAVE_KEY_ALERT:
        case SAVE_KEY_FORMAT:
        case SAVE_KEY_DIF_VERSION:
            bright = 0x40;
            break;
        default:
            break;
    }

    int show = 0;

    switch (SaveMenu.key_no) {
        case SAVE_KEY_FILE_SELECT:
        case SAVE_KEY_SAVE_CHECK:
        case SAVE_KEY_SAVE_DECIDE:
        case SAVE_KEY_LOAD_DECIDE:
        case SAVE_KEY_LOAD:
            show = 1;
            break;
        default:
            break;
    }

    if (SaveMenu.key_no == SAVE_KEY_FADE_OUT && SaveMenu.mode == SAVE_MENU_MODE_LOAD && SaveMenu.loaded != 0) {
        show = 1;
    }

    if (show != 0) {
        for (int row = 0; row < rows; row++, y += kBoardStep) {
            if (y <= -kBoardStep || y >= SCREEN_HEIGHT) {
                continue;
            }

            if (std::cmp_less(row, SaveSlots.saves.size())) {
                DrawSaveBoard(&SaveSlots.saves[row], SaveMenuMojiTextbl, static_cast<int>(board_x), static_cast<int>(y), bright, alpha);
            } else {
                DrawNewFileTemplete(static_cast<int>(board_x), static_cast<int>(y), alpha);
            }
        }
    }

    float text_x = -20.0f;
    float text_y = -20.0f;
    CommonMenuMes2.auto_pos = MES_POS_NONE;

    switch (McAccess.GetFuncNo()) {
        case MC_OPERATION_IDLE:
            switch (SaveMenu.key_no) {
                case SAVE_KEY_MODE_SELECT:
                    text_x = 240.0f;
                    text_y = 154.0f;
                    CommonMenuMes2.auto_pos = MES_POS_CENTRE;
                    break;
                case SAVE_KEY_NEW_DIR:
                case SAVE_KEY_NEW_DIR_SELECT:
                case SAVE_KEY_MC_SELECT:
                case SAVE_KEY_FORMAT:
                    text_x = 184.0f;
                    text_y = 152.0f;
                    CommonMenuMes2.auto_pos = MES_POS_CENTRE;
                    break;
                case SAVE_KEY_SAVE_DECIDE:
                case SAVE_KEY_LOAD_DECIDE:
                    text_x = 246.0f;
                    text_y = 156.0f;
                    CommonMenuMes2.auto_pos = MES_POS_CENTRE;
                    break;
                case SAVE_KEY_DIF_VERSION:
                case SAVE_KEY_ALERT:
                case SAVE_KEY_AFTER_ENDING:
                case SAVE_KEY_END_SAVE_ENDING:
                case SAVE_KEY_END_SAVE:
                    text_x = 196.0f;
                    text_y = 140.0f;
                    CommonMenuMes2.auto_pos = MES_POS_CENTRE;
                    break;
                default:
                    break;
            }

            break;
        case MC_OPERATION_DELETE:
            text_x = 216.0f;
            text_y = 180.0f;
            CommonMenuMes2.auto_pos = MES_POS_CENTRE;
            break;
        default:
            text_x = 184.0f;
            text_y = 152.0f;
            CommonMenuMes2.auto_pos = MES_POS_CENTRE;
            break;
    }

    MenuTextureReload(CommonMenuMes2.tex_block);
    CommonMenuMes2.edge_alpha = std::clamp(alpha, 0, 0x80);

    DrawMenuClsMes(&CommonMenuMes2, static_cast<int>(text_x), static_cast<int>(text_y));
    int hand_x = -1;
    int hand_y = -1;
    CommonMenuMes2.cursor_row = -1;

    switch (SaveMenu.key_no) {
        case SAVE_KEY_MODE_SELECT:
            CommonMenuMes2.cursor_row = SaveMenu.file_no + 2;
            break;
        case SAVE_KEY_FILE_SELECT:
            if (rows > 0) {
                hand_x = 0x78;
                hand_y = 0xBC;
            }

            break;
        default:
            break;
    }

    if (hand_x > 0 && hand_y > 0) {
        static int hand_frame = 0;
        float      draw_x = static_cast<float>(hand_x) + 7.0f * cosf(0.0805536583f * hand_frame);
        float      draw_y = static_cast<float>(hand_y) + 5.0f * sinf(0.116355285f * hand_frame);
        CRect_i_   source(0x160, 0xD6, 0x20, 0x20);
        DrawMenu2DSprite(SaveBoard, CRect_i_(static_cast<int>(5.0f + draw_x), static_cast<int>(3.0f + draw_y), source.width, source.height), source, 0, 0, 0, alpha);
        DrawMenu2DSprite(SaveBoard, CRect_i_(static_cast<int>(draw_x), static_cast<int>(draw_y), source.width, source.height), source, alpha);
        hand_frame++;

        if (!(static_cast<float>(hand_frame) < 105299.0f)) {
            hand_frame = 0;
        }
    }

    switch (SaveMenu.key_no) {
        case SAVE_KEY_FADE_OUT:
        case SAVE_KEY_MC_SELECT:
        case SAVE_KEY_FADE_IN:
            SaveMenu.step_time++;
            break;
        default:
            SaveMenu.step_time = 0;
            break;
    }

    switch (SaveMenu.mode) {
        case SAVE_MENU_MODE_LOAD: {
            DrawMenu2DSprite(SaveBoard, CRect_i_(0x46, 0x32, 0x3A, 0x27), CRect_i_(0x110, 0xD8, 0x3A, 0x28), alpha);
            // LOAD in the menu captions' lettering over the painted one: its letters span x 6 to 72 of
            // the picture and its capitals rows 5 to 24. Without the font the painted LOAD is drawn.
            std::string title;
            std::string desc;
            std::string save_word;
            std::string load_word;
            std::string prompt;
            GetLocalizedSaveLoadStrings(title, desc, save_word, load_word, prompt);
            if (caption::DrawTitle(load_word, 0x86, 0x37, 6, 72, 5, 24, 0.1f, {}, alpha)) {
                return;
            }
            DrawMenu2DSprite(SaveBoard, CRect_i_(0x86, 0x37, 0x50, 0x1E), CRect_i_(0x110, 0x100, 0x50, 0x1E), alpha);
            return;
        }
        case SAVE_MENU_MODE_ENDING:
            DrawMainMenuIcon(0x46, 0x32, 5, 1, 0x80, alpha);
            break;
        default:
            break;
    }
}

/**
 * Waits out the save screen's fade-in and then moves on to the save file
 * choice.
 */
int SaveMenuKeyFadeIn() {
    if (SaveMenu.step_time > 14) {
        SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
        SaveMenu.step_time = 0;
    }

    return 1;
}

/**
 * Waits out the save screen's fade-out, then closes the screen and records the
 * result that MenuSaveKey returns.
 */
int SaveMenuKeyFadeOut() {
    if (SaveMenu.step_time > 32) {
        ExitSaveSelect();

        if (SaveMenu.loaded) {
            SaveMenu.result = MENU_SAVE_LOADED;
        } else {
            SaveMenu.result = MENU_SAVE_CLOSED;
        }
    }

    return 1;
}

/**
 * Lets the player choose between saving and loading, then moves on to the slot
 * choice or closes the screen.
 */
int SaveMenuKeyModeSelect() {
    if (GamePad.Down(PAD_UP | PAD_DOWN)) {
        if (SaveMenu.file_no) {
            SaveMenu.file_no = 0;
        } else {
            SaveMenu.file_no = 1;
        }
    }

    if (GamePad.Down(PAD_CROSS)) {
        if (SaveMenu.file_no) {
            SaveMenu.access_kind = SAVE_ACCESS_LOAD;
        } else {
            SaveMenu.access_kind = SAVE_ACCESS_SAVE;
        }

        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = 0;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        return 1;
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        SaveMenu.key_no = SAVE_KEY_FADE_OUT;
        ExitSaveSelect();
        SaveMenu.step_time = 0;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 1;
    }

    return 1;
}

/**
 * Moves on to reading the save directory when the chosen slot holds a
 * PlayStation 2 card, and to an alert otherwise.
 */
int SaveMenuKeyCheckMcType() {
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];

    if (card->present) {
        switch (card->type) {
            case sceMcTypePS2:
                McAccess.SetFuncNo(MC_OPERATION_GET_DIR);
                SaveMenu.key_no = SAVE_KEY_CHECK_MC;
                break;
            default:
                SaveMenu.key_no = SAVE_KEY_ALERT;
                SaveMenu.alert_no = SAVE_ALERT_NO_CARD;
                break;
        }
    } else {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_NO_CARD;
    }

    return 1;
}

/**
 * Checks the chosen card again and starts reading its configuration for a load
 * or its save files for a save, returning zero when the card is gone.
 */
int SaveMenuKeyCheckMc() {
    printf("check end !!\n");
    MC_CARD_INFO *card = &McAccess.card[McAccess.port];
    SaveMenu.alert_no = SAVE_ALERT_NONE;

    if (card->present == 0) {
        printf("not \n");
        SaveMenu.alert_no = SAVE_ALERT_CARD_MISSING;
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = McAccess.port;
        return 0;
    }

    if (card->type != sceMcTypePS2) {
        printf("type is not PS2\n");
        SaveMenu.alert_no = SAVE_ALERT_CARD_MISSING;
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = McAccess.port;
        return 0;
    }

    if ((SaveMenu.access_kind == SAVE_ACCESS_LOAD || SaveMenu.mode == SAVE_MENU_MODE_ENDING) && (card->dir_exists == 0 || card->formatted == 0)) {
        SaveMenu.alert_no = SAVE_ALERT_NO_SAVE_DATA;
        SaveMenu.key_no = SAVE_KEY_ALERT;
        return 1;
    }

    if (SaveMenu.access_kind == SAVE_ACCESS_SAVE && card->dir_exists == 0 && card->free_size < 400 && card->formatted != 0) {
        SaveMenu.alert_no = SAVE_ALERT_NO_SPACE_DIR;
        SaveMenu.key_no = SAVE_KEY_ALERT;
        return 1;
    }

    if (SaveMenu.mode == SAVE_MENU_MODE_ENDING) {
        SaveMenu.key_no = SAVE_KEY_SAVE_ENDING;
    } else if (SaveMenu.access_kind == SAVE_ACCESS_LOAD) {
        SaveMenu.key_no = SAVE_KEY_LOAD_CONFIG;
        McAccess.SetFuncNo(MC_OPERATION_LOAD_CONFIG);
    } else {
        McAccess.SetFuncNo(MC_OPERATION_GET_ALL_SAVE_FILE_INFO);
        SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
        SaveMenu.file_no = static_cast<s32 *>(SaveData->GetConfigData())[17];
    }

    return 1;
}

/**
 * Starts reading the save files of the card and moves on to the file choice,
 * with the cursor on the last file that the configuration records.
 */
int SaveMenuKeyLoadConfig() {
    McAccess.SetFuncNo(MC_OPERATION_GET_ALL_SAVE_FILE_INFO);
    SaveMenu.key_no = SAVE_KEY_FILE_SELECT;

    if (GameClearWord() != 0) {
        GameClearFlag = 1;
    }

    SaveMenu.file_no = static_cast<s32 *>(SaveData->GetConfigData())[17];
    return 1;
}

} // namespace dc::game
