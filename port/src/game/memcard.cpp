#include "stubs/memcard.hpp"

#include "memcard.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "battle_globals.hpp"
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
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

/** Holds the georama parts of a town the player is not standing in. */
CEditPartsInfo BtEditPartsInfo;

MENU_ATORA_SEL    MenuAtoraSel;
CMemoryCardAccess McAccess;

/** The StayTex menu texture. */
CTexture *StayTex;

/** The AttachIcon menu texture. */
CTexture *AttachIcon;

/** The texture of the item icons. */
CTexture *ItemIcon;

/** The ItemIcon2 menu texture. */
CTexture *ItemIcon2;

/** The texture of the weapon icons. */
CTexture *WepIcon;

CTexture *Sozai;
CTexture *HoleGray;
CTexture *HoleGold;
CTexture *ObTip;
CTexture *ObPerson;
CTexture *CompleteTex;
CTexture *VillageBar;
CTexture *VillageName;

/** The texture that the save screen's file boards draw from. */
CTexture *SaveBoard;

CEditPartsInfo *CommonMenuAtoraInfo;
short          *GetAtraMsgReadBuf;

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

s32 CursorVibeCnt;

/**
 * Draws the georama board screen's board, panels and cursor.
 *
 * @mangled DrawAtoraSelect__Fi
 * @address 0x21AE80
 * @size 0xFEC
 */
static void DrawAtoraSelect(int fade);

/**
 * Moves the board cursor and handles its buttons, returning 10 when a part is
 * picked for placing, 100 when the screen is to close and zero otherwise.
 *
 * @mangled AtoraBoardKey__Fv
 * @address 0x21CA90
 * @size 0xA30
 */
static int AtoraBoardKey();

/**
 * Moves the chip cursor and swaps, sorts or puts back chips, returning 100 when
 * the screen is to close and zero otherwise.
 *
 * @mangled AtoraTipKey__Fv
 * @address 0x21D4C0
 * @size 0x2F4
 */
static int AtoraTipKey();

/**
 * Puts the chip that the cursor holds back where it was picked up.
 *
 * @mangled AtoraMenuTipCancel__Fv
 * @address 0x21D7C0
 * @size 0xC8
 */
static void AtoraMenuTipCancel();

/**
 * Draws the board's edge fade.
 *
 * @mangled AtoraBoardFadeEffect__Fv
 * @address 0x21D890
 * @size 0x3C8
 */
static void AtoraBoardFadeEffect();

/**
 * Waits out the save screen's fade-in and then moves on to the save file
 * choice.
 *
 * @mangled SaveMenuKeyFadeIn__Fv
 * @address 0x220D90
 * @size 0x34
 */
static int SaveMenuKeyFadeIn();

/**
 * Waits out the save screen's fade-out, then closes the screen and records the
 * result that MenuSaveKey returns.
 *
 * @mangled SaveMenuKeyFadeOut__Fv
 * @address 0x220DD0
 * @size 0x68
 */
static int SaveMenuKeyFadeOut();

/**
 * Lets the player choose between saving and loading, then moves on to the slot
 * choice or closes the screen.
 *
 * @mangled SaveMenuKeyModeSelect__Fv
 * @address 0x220E40
 * @size 0x12C
 */
static int SaveMenuKeyModeSelect();

/**
 * Lets the player pick memory card slot 1 or 2, then starts the card type check
 * on it or backs out.
 *
 * @mangled SaveMenuKeyMcSelect__Fv
 * @address 0x220F70
 * @size 0x228
 */
static int SaveMenuKeyMcSelect();

/**
 * Moves on to reading the save directory when the chosen slot holds a
 * PlayStation 2 card, and to an alert otherwise.
 *
 * @mangled SaveMenuKeyCheckMcType__Fv
 * @address 0x2211A0
 * @size 0xC0
 */
static int SaveMenuKeyCheckMcType();

/**
 * Checks the chosen card again and starts reading its configuration for a load
 * or its save files for a save, returning zero when the card is gone.
 *
 * @mangled SaveMenuKeyCheckMc__Fv
 * @address 0x221260
 * @size 0x248
 */
static int SaveMenuKeyCheckMc();

/**
 * Starts reading the save files of the card and moves on to the file choice,
 * with the cursor on the last file that the configuration records.
 *
 * @mangled SaveMenuKeyLoadConfig__Fv
 * @address 0x2214B0
 * @size 0x74
 */
static int SaveMenuKeyLoadConfig();

/**
 * Lets the player pick one of the twelve save files for the save check or the
 * load confirmation, or go back to the slot choice.
 *
 * @mangled SaveMenuKeyFileSelect__Fv
 * @address 0x221530
 * @size 0x1FC
 */
static int SaveMenuKeyFileSelect();

int McCheckMCPs2(MC_CARD_INFO *card) {
    if (!card->present || card->type != sceMcTypePS2) {
        return 0;
    }

    return 1;
}

void DrawObjectVibe(int x, int y, CTexture *texture, CRect_i_ src_rect, unsigned char alpha, int flag) {
    float    dest_x = (float) x + 7.0f * cosf(0.08055365830659866f * (float) CursorVibeCnt);
    float    dest_y = (float) y + 5.0f * sinf(0.1163552850484848f * (float) CursorVibeCnt);
    CRect_i_ dest_rect((s32) dest_x, (s32) dest_y, src_rect.width, src_rect.height);
    DrawMenu2DSprite(texture, dest_rect, src_rect, alpha, alpha, alpha, flag);
}

void DrawObjectVibe(int x, int y, CTexture *texture, RECT src_rect, unsigned char alpha, int flag) {
    CRect_i_ src(src_rect.x, src_rect.y, src_rect.width, src_rect.height);
    DrawObjectVibe(x, y, texture, src, alpha, flag);
}

/**
 * Draws a swaying 32-pixel icon from the stay-frame texture, with a shadow beneath it when asked.
 */
void DrawMenuObjectVibe(int x, int y, int shadow, int icon_u);

void DrawMenuHelpWindow(CTexture *texture, int style, int x, int y, float width, float height, int alpha) {
    int   corner_u;
    float middle_width;
    float middle_height;
    int   middle_y;
    int   bottom_y;

    if (style == 0) {
        corner_u = 24;
    } else if (style == 1) {
        corner_u = 0;
    } else {
        corner_u = 48;
    }

    middle_width = 24.0f * width;
    middle_height = 24.0f * height;
    middle_y = y + 24;
    bottom_y = middle_y + middle_height;

    if (texture == NULL) {
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
    int middle_width;
    int middle_height;
    int middle_y;
    int bottom_y;

    middle_width = width * 16.0f;
    middle_height = height * 22.0f;
    middle_y = y + 22;
    bottom_y = middle_y + middle_height;

    if (texture == NULL) {
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
    int middle_width;
    int middle_height;
    int middle_y;
    int bottom_y;

    middle_width = width;
    middle_height = height;
    middle_y = y + 22;
    bottom_y = middle_y + middle_height;

    if (texture == NULL) {
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

/**
 * Draws the framed help window used by the georama menu.
 */
void MenuHelpWinDraw(int x, int y, float width, float height, int alpha) {
    StayTex = TexManager.GetTexture(AtoraVibeTextureName, -1);
    MenuHelpWinDraw(x, y, width, height, alpha, 0, 0, StayTex);
}

void DrawMenuWaku(float x, float y, int width, int height, int type, CTexture *texture, int alpha) {
    RECT *src;
    float offset;
    int   left;
    int   top;
    int   right;
    int   bottom;

    if (texture != NULL) {
        static int MenuWakuCnt = 0;

        offset = 0.2f * MenuWakuCnt;
        left = x + offset;
        top = y + offset;
        right = (x + width) - offset;
        bottom = (y + height) - offset;
        RECT corner[2] = {
            {74,  72, 16, 16},
            {106, 72, 27, 24}
        };
        src = &corner[type];
        DrawMenu2DSprite(texture, CRect_i_(left, top, src->width, src->height), CRect_i_(src->x, src->y, src->width, src->height), alpha);
        DrawMenu2DSprite(texture, CRect_i_(right, top, src->width, src->height), CRect_i_(src->x + src->width, src->y, src->width, src->height), alpha);
        DrawMenu2DSprite(texture, CRect_i_(left, bottom, src->width, src->height), CRect_i_(src->x, src->y + src->height, src->width, src->height), alpha);
        DrawMenu2DSprite(texture, CRect_i_(right, bottom, src->width, src->height), CRect_i_(src->x + src->width, src->y + src->height, src->width, src->height), alpha);
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
    int digits;
    int digit;
    int width;
    int src_x;
    int dest_y;
    int dest_height;
    int clip_y;
    int src_y;
    int height;

    if (texture == NULL) {
        return 0;
    }

    for (digits = GetNumberKeta(number); 0 < digits; digits--) {
        clip_y = y;
        digit = number % 10;
        width = rect.width;
        x -= width - overlap;
        src_x = rect.x + width * digit;
        src_y = rect.y;
        height = rect.height;
        MenuTextureClip(clip_y, src_y, height, 0, SCREEN_HEIGHT);
        CRect_i_ dest;
        CRect_i_ src(src_x, src_y, width, height);
        dest_height = height - 1;
        dest_y = clip_y;
        dest.x = x;
        dest.y = dest_y;
        dest.width = width;
        dest.height = dest_height;
        DrawMenu2DSprite(texture, dest, src, r, g, b, flag);
        number /= 10;
    }

    return x;
}

int DrawMenuNumber(int number, int x, int y, RECT rect, CTexture *texture, int overlap, int top, int bottom, int flag) {
    int digits;
    int digit;
    int width;
    int src_x;
    int dest_y;
    int dest_height;
    int clip_y;
    int src_y;
    int height;

    if (texture == NULL) {
        return 0;
    }

    for (digits = GetNumberKeta(number); 0 < digits; digits--) {
        clip_y = y;
        digit = number % 10;
        width = rect.width;
        x -= width - overlap;
        src_x = rect.x + width * digit;
        src_y = rect.y;
        height = rect.height;
        MenuTextureClip(clip_y, src_y, height, top, bottom);
        CRect_i_ dest;
        CRect_i_ src(src_x, src_y, width, height);
        dest_height = height - 1;
        dest_y = clip_y;
        dest.x = x;
        dest.y = dest_y;
        dest.width = width;
        dest.height = dest_height;
        DrawMenu2DSprite(texture, dest, src, flag);
        number /= 10;
    }

    return x;
}

int GetMsgLengthMenu(ClsMes *mes, int mes_no) {
    int    length = 0;
    short *code = mes->GetTextLineDataTop_system(mes_no);

    if (code != NULL) {
        while (1) {
            short c = *code++;

            if ((unsigned int) (c + 0x100) <= 1U || code == NULL) {
                break;
            }

            length++;
        }
    }

    return length;
}

/**
 * Gives the texture and the cell within it that one georama element draws from.
 *
 * @mangled RetCTexAtora__FiRiRi
 * @address 0x2181E0
 * @size 0xD8
 */
static CTexture *RetCTexAtora(int tip_no, int &x, int &y) {
    CTexture *texture;
    int       tex_no;

    tex_no = GetEditAtraChipData(MenuAtoraSel.map_no, tip_no)->tex_no;
    x = ((tex_no + 7) % 7) * 36;
    y = (tex_no / 7) * 36;

    if (tip_no < 40) {
        texture = ObTip;
    } else if (tip_no >= 40) {
        texture = ObPerson;
    }

    return texture;
}

void DrawAtoraParts(int x, int y, int tip_no, int top, int bottom, int alpha) {
    CTexture *texture;
    int       src_x;
    int       src_y;
    int       height;

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

    texture = RetCTexAtora(tip_no, src_x, src_y);

    if (texture == NULL) {
        return;
    }

    height = 36;
    MenuTextureClip(y, src_y, height, top, bottom);
    CRect_i_ src(src_x, src_y, 36, height);
    CRect_i_ shadow(x + 2, y + 1, 36, height);
    DrawMenu2DSprite(texture, shadow, src, 0, 0, 0, (alpha * 80) >> 7);
    CRect_i_ dest(x, y, 36, height);
    DrawMenu2DSprite(texture, dest, src, alpha);
}

/**
 * Returns the record of the n-th valid part in the georama's part list, or NULL
 * when there are fewer.
 *
 * @mangled SearchAtoraInfo__Fi
 * @address 0x218470
 * @size 0x8C
 */
static EDITPARTS_INFO *SearchAtoraInfo(int index) {
    int parts;
    int count;

    parts = CommonMenuAtoraInfo->GetNextParts(-1);
    count = -1;

    while (parts != -1) {
        count++;

        if (index == count) {
            return CommonMenuAtoraInfo->GetPartsInfo(parts);
        }

        parts = CommonMenuAtoraInfo->GetNextParts(parts);
    }

    return NULL;
}

/**
 * Returns whether every chip of a georama part has been acquired.
 *
 * @mangled AtoraAllTipGet__Fi
 * @address 0x218500
 * @size 0x9C
 */
static int AtoraAllTipGet(int parts_no) {
    EDITPARTS_INFO    *info;
    int                result;
    int                i;
    EDITPARTS_ELEMENT *element;

    info = CommonMenuAtoraInfo->GetPartsInfo(parts_no);

    if (info == NULL) {
        return 0;
    }

    result = 1;

    for (i = 0; i < 6; i++) {
        element = &info->elements[i];

        if (element == NULL) {
            break;
        }

        if (0 <= element->id && element->enabled == 0) {
            result = 0;
            break;
        }
    }

    return result;
}

/**
 * Returns whether the player has already spoken to a georama resident.
 *
 * @mangled AlreadyPeopleTalk__Fii
 * @address 0x2185A0
 * @size 0x64
 */
static int AlreadyPeopleTalk(int map_no, int chip_no) {
    SV_GRD_NPC *npc;

    npc = SaveData->GetGrdNPCData(map_no, chip_no - 40);

    if (npc == NULL || chip_no - 40 < 0) {
        return 1;
    }

    return npc->talk_message;
}

/**
 * Returns one when a georama part is fully built, its residents have all been
 * spoken to, and its completion event has not been flagged yet.
 *
 * @mangled AtoraCompOrEvent__FP14EDITPARTS_INFO
 * @address 0x218610
 * @size 0x164
 */
static int AtoraCompOrEvent(EDITPARTS_INFO *info) {
    EDIT_PARTS_ATRA   *atra;
    EDITPARTS_ELEMENT *element;
    int                complete;
    int                filled;
    int                talked;
    int                done;
    int                result;
    int                i;

    atra = GetEditAtraPartsData(MenuAtoraSel.map_no, info->parts_no);

    if (atra == NULL) {
        return 0;
    }

    if (atra->kind == 0) {
        return 0;
    }

    complete = CommonMenuAtoraInfo->CheckComplete(info->parts_no);
    filled = 0;
    talked = 1;
    done = 0;

    if (info != NULL) {
        if (info->placed == info->stock) {
            filled = 1;
        }

        for (i = 0; i < 6; i++) {
            element = &info->elements[i];

            if (element->id >= 40) {
                if (element->npc_no >= 0) {
                    if (AlreadyPeopleTalk(MenuAtoraSel.map_no, element->id) == 0) {
                        talked = 0;
                    }
                }
            }
        }

        if (info->completion_flags & 1) {
            done = 1;
        }
    }

    result = 0;

    if (complete && filled && talked && !done) {
        result = 1;
    }

    return result;
}

/**
 * Returns how many parts a georama ground defines, or zero for an invalid
 * ground.
 *
 * @mangled AtraBoardMaxNum__Fi
 * @address 0x218780
 * @size 0x88
 */
static int AtraBoardMaxNum(int ground) {
    EDIT_PARTS_ATRA *parts;
    int              count;

    if (ground < 0 || ground >= 6) {
        return 0;
    }

    parts = GetEditAtraPartsData(ground, 0);

    if (parts == NULL) {
        return 0;
    }

    for (count = 0; count < 40 && parts->max > 0; count++) {
        parts++;
    }

    return count;
}

/**
 * Returns the link code of a chip slot: -1 when empty, 0 without a pending
 * link, otherwise 1 or 2 by link kind plus 10 for each unplaced chip above it.
 *
 * @mangled AtoraTipStatusSearch__FP14EDITPARTS_INFOi
 * @address 0x218810
 * @size 0xC8
 */
static int AtoraTipStatusSearch(EDITPARTS_INFO *info, int slot) {
    int next;
    int link;
    int code;

    if (info == NULL) {
        return ATORA_LINK_NONE;
    }

    if (info->elements[slot].id < 0) {
        return -1;
    }

    link = info->elements[slot].required_element;

    if (link < 0 || info->elements[slot].enabled != 0) {
        return ATORA_LINK_NONE;
    }

    if (link < 3 && slot >= 3) {
        code = ATORA_LINK_ABOVE;
    } else {
        code = ATORA_LINK_BESIDE;
    }

    if (info->elements[link].enabled == 0) {
        code += 10;
    }

    next = info->elements[link].required_element;

    if (next >= 0 && info->elements[next].enabled == 0) {
        code += 10;
    }

    return code;
}

/**
 * Answers whether a georama chip attachment may be shown on the board: a
 * person's chip stays hidden until that resident has moved in.
 */
static int AtraTipCanDisplay(EDIT_CHIP_ATTACH_DATA *attach) {
    int display;

    display = 1;

    if (attach != NULL && attach->id < 40 && attach->npc_no >= 0 && (SaveData->GetGrdNPCData(MenuAtoraSel.map_no, attach->npc_no)->flags & 2) == 0) {
        display = 0;
    }

    return display;
}

/**
 * Draws the arrow from a chip slot to the chip it hangs off, covering the slot
 * while that chip is missing, as the slot's link code directs.
 *
 * @mangled AtoraTipRelationDraw__FiiP14EDITPARTS_INFOiii
 * @address 0x218960
 * @size 0x1F4
 */
static void AtoraTipRelationDraw(int x, int y, EDITPARTS_INFO *info, int slot, int link, int alpha) {
    int dx;
    int dy;
    int u;
    int v;
    int height;

    if (info != NULL) {
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
 *
 * @mangled AtoraBoardEnableMovePos__FiPi
 * @address 0x218B60
 * @size 0x10C
 */
static void AtoraBoardEnableMovePos(int parts_no, int *enable) {
    EDITPARTS_INFO *info;
    int             link;
    int             i;

    info = SearchAtoraInfo(parts_no);

    if (info == NULL) {
        for (int j = 0; j < 6; j++) {
            enable[j] = 0;
        }

        return;
    }

    for (i = 0; i < 6; i++) {
        enable[i] = 1;

        if (info->elements[i].id < 0) {
            enable[i] = 0;
        } else {
            link = info->elements[i].required_element;

            if (link < 0) {
                enable[i] = 1;
            } else {
                link = info->elements[link].required_element;

                if (link < 0) {
                    enable[i] = 1;
                } else if (info->elements[link].enabled != 0) {
                    enable[i] = 1;
                } else {
                    enable[i] = 0;
                }
            }
        }
    }
}

/**
 * Returns the first index at or below a start whose entry in a slot table is
 * set, or the lower bound when none is.
 *
 * @mangled AtoraBoardGoToPos__FPiii
 * @address 0x218C70
 * @size 0x44
 */
static int AtoraBoardGoToPos(int *enable, int pos, int min) {
    while (min < pos) {
        if (enable[pos] != 0) {
            return pos;
        }

        pos--;
    }

    return pos;
}

int GetAtraMsgNo(int map_no, int element) {
    int mes_no;

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

/**
 * Returns the message number that the board shows for a georama part or one of
 * its chip slots, or -1 when the part has no record.
 *
 * @mangled AtoraMsgNoGet__Fiii
 * @address 0x218DA0
 * @size 0x1F4
 */
static int AtoraMsgNoGet(int map_no, int board_pos, int slot) {
    EDITPARTS_INFO        *info;
    EDIT_PARTS_ATRA       *parts;
    int                    msg_no;
    EDIT_CHIP_ATTACH_DATA *attach;
    EDIT_CHIP_ATTACH_DATA *elements;
    EDIT_ELEMENT_ATRA     *chip;
    EDIT_CHIP_ATTACH_DATA *shown;
    int                    link;
    int                    flag;

    info = SearchAtoraInfo(board_pos);

    if (info == NULL) {
        return -1;
    }

    parts = GetEditAtraPartsData(map_no, info->parts_no);

    if (parts == NULL) {
        return -1;
    }

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
                        flag = SaveData->GetGameIntFlag(1);

                        if (flag > 0) {
                            msg_no += flag + 28;
                        }
                    }

                    break;
            }

            break;
        default:
            elements = parts->elements;
            attach = &elements[slot] - 1;
            chip = GetEditAtraChipData(map_no, attach->id);
            shown = &GetEditAtraPartsData(MenuAtoraSel.map_no, info->parts_no)->elements[slot - 1];
            msg_no = chip->msg_no + (map_no * 200 + 40);

            if (attach->id >= 40) {
                msg_no += 40;
            }

            if (info->elements[slot - 1].enabled == 0) {
                msg_no += 60;
            }

            link = info->elements[slot - 1].required_element;

            if ((link >= 0 && info->elements[link].enabled == 0) || AtraTipCanDisplay(shown) == 0) {
                msg_no = -788;
            }

            break;
    }

    return msg_no;
}

/**
 * Returns the message number of a chip from its attribute record alone, or -1
 * when it has none.
 *
 * @mangled AtoraTipOnlyMsgNoGet__Fii
 * @address 0x218FA0
 * @size 0x58
 */
static int AtoraTipOnlyMsgNoGet(int map_no, int number) {
    EDIT_ELEMENT_ATRA *chip;
    int                msg_no;

    chip = GetEditAtraChipData(map_no, number);

    if (chip == NULL) {
        return -1;
    }

    msg_no = chip->msg_no + 40;

    if (number >= 40) {
        msg_no += 40;
    }

    return msg_no;
}

/**
 * Gives the cell within the element sheet that one georama element draws from.
 *
 * @mangled AtoraTipGetTexPos__FiRiRi
 * @address 0x219000
 * @size 0xD8
 */
static void AtoraTipGetTexPos(int tip_no, int &x, int &y) {
    int tex_no;

    tex_no = GetEditAtraChipData(MenuAtoraSel.map_no, tip_no)->tex_no;

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
 *
 * @mangled AtoraTipObjectOrPerson__Fiiiii
 * @address 0x2190E0
 * @size 0xDC
 */
static void AtoraTipObjectOrPerson(int x, int y, int tip_no, int dark, int alpha) {
    int       u;
    int       v;
    CTexture *texture = RetCTexAtora(tip_no, u, v);
    CRect_i_  source(u, v, 0x24, 0x24);
    int       red, green, blue;
    blue = green = red = 0x80;

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
 *
 * @mangled AtoraTipHoleTexInfoGet__FiPUc
 * @address 0x2191C0
 * @size 0x4C
 */
static CTexture *AtoraTipHoleTexInfoGet(int gold, unsigned char *color) {
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
 *
 * @mangled AtoraPlateDrawHaichiBar__FP14EDITPARTS_INFOiii
 * @address 0x219210
 * @size 0xE0
 */
static void AtoraPlateDrawHaichiBar(EDITPARTS_INFO *info, int x, int y, int flag) {
    int      empty = 83 - info->placed * 83 / info->stock;
    CRect_i_ dest;
    CRect_i_ src(244, 323 - empty, 12, empty);

    dest.x = x + 12;
    dest.y = y + 107 - empty;
    dest.width = 12;
    dest.height = empty;
    DrawMenu2DSprite(Sozai, dest, src, flag);
    DrawAtraBuildNum(info, x, y, flag);
}

void DrawAtraBuildNum(EDITPARTS_INFO *info, int x, int y, int alpha) {
    RECT digit = {0, 212, 12, 12};
    int  num_x;

    num_x = x + 8;

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
    int           u;
    int           v;
    unsigned char colour[4];

    if (y < -100 || y > 0x1CC) {
        return;
    }

    EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(parts_index);

    if (info == NULL) {
        return;
    }

    EDIT_PARTS_ATRA *parts = GetEditAtraPartsData(MenuAtoraSel.map_no, info->parts_no);
    int              complete = CommonMenuAtoraInfo->CheckComplete(parts_index);
    int              all_tips = AtoraAllTipGet(parts_index);
    int              event = AtoraCompOrEvent(info);
    u = 0;
    v = 0;

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
                    CTexture *hole = AtoraTipHoleTexInfoGet(all_tips, colour);
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

        for (int i = 0; i < 6; i++) {
            if (info->elements[i].id < 0) {
                break;
            }

            if (0 <= info->elements[i].id) {
                count++;
            }
        }

        float wave = cosf(PI * CompMsgCt / (160.0f + (count >> 1)));
        base_x = (int) ((float) (base_x - 6) + 6.0f * wave);
        base_y = (int) ((float) base_y + 2.0f * wave);
        float width = 128.0f + 6.0f * wave;
        (int) width;
        float height = 88.0f + 3.0f * wave;
        (int) height;
        base_x = (int) ((float) base_x + 8.0f * cosf(PI * CompMsgCt / (180.0f + count)));
        base_y = (int) ((float) base_y + 4.0f * sinf(PI * CompMsgCt / (140.0f + count)));
        DrawMenu2DSprite(CompleteTex, CRect_i_(base_x, base_y, (int) width, (int) height), CRect_i_(0, 0x28, 0x80, 0x58), alpha);
    }

    CompMsgCt++;

    if (CompMsgCt >= 320000 || CompMsgCt < 0) {
        CompMsgCt = 0;
    }
}

/**
 * Draws an empty plate of the georama board with a caption in its centre.
 *
 * @mangled DrawAtoraNothing__Fiii
 * @address 0x219CC0
 * @size 0x2F4
 */
static void DrawAtoraNothing(int x, int y, int alpha) {
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
    s8 message_x[7] = {64, 64, 64, 64, 54, 64, 64};
    DrawMenu2DSprite(Sozai, CRect_i_(x + message_x[lang], y + 44, 132, 30), CRect_i_(124, 418, 132, 30), alpha);
}

/**
 * Shows message 200 in a message window at a position.
 *
 * @mangled DrawMsgAtraWarning__FP6ClsMesii
 * @address 0x219FC0
 * @size 0xA0
 */
static void DrawMsgAtraWarning(ClsMes *mes, int x, int y) {
    if (mes == NULL) {
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
 *
 * @mangled AtoraTipInfoInit__Fv
 * @address 0x21A060
 * @size 0x2C
 */
static void AtoraTipInfoInit() {
    NowTipHavePt->mode = ATORA_SIDE_BOARD;
    NowTipHavePt->parts_no = -1;
    NowTipHavePt->slot = -1;
    NowTipHavePt->tip_no = -1;
}

int GetMenuAtraEventFlag() {
    return MenuAtoraSel.event_flag;
}

/**
 * Records whether the georama menu is running an event.
 *
 * @mangled SetMenuAtraEventFlag__Fi
 * @address 0x21A0A0
 * @size 0x10
 */
static void SetMenuAtraEventFlag(int flag) {
    MenuAtoraSel.event_flag = flag;
}

/**
 * Opens the board's message window on the message of the selected part.
 *
 * @mangled MenuAtoraAfterFadeIn__Fv
 * @address 0x21A0B0
 * @size 0x7C
 */
static void MenuAtoraAfterFadeIn() {
    int msg_no;

    CommonMenuMes2.SetBuff(GetAtraMsgReadBuf);
    CommonMenuMes2.mes_made = -1;
    msg_no = AtoraMsgNoGet(MenuAtoraSel.map_no, MenuAtoraSel.board_pos, 0);
    CommonMenuMes2.MakeMesWin(msg_no >= 0 ? msg_no + 1000 : 0);
}

void InitMenuAtora1(int open_mode, int edit_map, int *texture_blocks, u_long128 *buffer) {
    MenuAtoraSel.open_mode = open_mode;
    InitPersonalBoardMode((CUserStatus *) SaveData->GetDngStatus(), &MenuAtoraSel.board, PERSONAL_BOARD_ATLA, edit_map + BOARD_PAGE_ATLA_VILLAGE);
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
    s8 widths[7] = {0x10, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C};
    AtoraNameMes.char_width = widths[GetMenuLangFlag()];
    AtoraNameMes.narrow_gaiji_set = 2;

    for (int i = 0; i < 10; i++) {
        AtoraNameMes.mes_no[i] = 999;
        AtoraNameMes.values[i] = -1;
    }

    AtoraNameMes.mes_made = -1;
    AtoraNameMes.narrow_gaiji = true;
    NowTipHavePt = (ATORA_TIP_HAVE *) &MenuAtoraSel.board.held_item;
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
    int             place;
    char            path[64];
    int             map;
    int             next;
    int             count;
    EDITPARTS_INFO *info;
    int             pos;
    int             y;
    int             i;

    MenuAtoraSel.map_no = map_no;
    map = MenuAtoraSel.map_no;

    if (AtoraTextureEnterFlag == 0) {
        GetPathReadDifferntLang(path);
        strcat(path, "a%d.pak");
        sprintf(path, path, map + 1);

        if (ReadBGSync()) {
            BreakReadBG();
        }

        StartReadBG();
        LoadFileBG(path, AtoraOffsetBuf, NULL);
        MenuAtoraSel.load_state = 0;

        if ((MenuAtoraSel.open_mode == ATORA_OPEN_PLACE || MenuAtoraSel.open_mode == ATORA_OPEN_WARN) && MenuAtoraSel.map_no == MapNo) {
            CommonMenuAtoraInfo = &EditPartsInfo;
        } else {
            CommonMenuAtoraInfo = &BtEditPartsInfo;
            CommonMenuAtoraInfo->Load(MenuAtoraSel.map_no, SaveData, 1);
        }
    }

    MenuAtoraSel.board.atla_elements = SaveData->GetElemData(map);
    next = CommonMenuAtoraInfo->GetNextPartsNum(-1);
    AtoraTipInfoInit();
    count = AtraBoardMaxNum(MenuAtoraSel.map_no);

    if (MenuAtoraSel.board_pos > count - 1) {
        MenuAtoraSel.board_pos = count - 1;
    }

    info = SearchAtoraInfo(MenuAtoraSel.board_pos);

    if (MenuAtoraSel.mode == ATORA_SIDE_BOARD) {
        if (info != NULL) {
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
        place = MenuAtoraSel.board.cursor % 4;
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
        pos = MenuAtoraSel.board_pos - 1;
        y = MenuAtoraSel.scroll_y + 80 + 130.0f * pos;

        for (i = 0; i < 3; i++) {
            info = SearchAtoraInfo(pos);

            if (info != NULL) {
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

/**
 * Stores the mode of a menu in the saved menu cursors.
 */
static inline void SetCursorMode(CMenuCursor *cursor, int menu, int mode) {
    cursor->mode[menu] = mode;
}

/**
 * Closes the georama board screen, saving the cursor position unless the
 * menus were told to forget it, and restores the shared message windows.
 */
static void ExitAtoraSelect() {
    CMenuCursor *cursor;
    int          pos;

    cursor = SaveData->GetMenuCursor();

    if (cursor->reset_pos == 0) {
        SetCursorMode(cursor, 3, MenuAtoraSel.mode);

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
 *
 * @mangled AtoraTexInfoGet__Fv
 * @address 0x21A960
 * @size 0x118
 */
static void AtoraTexInfoGet() {
    CompleteTex = TexManager.GetTexture("complete", -1);
    Sozai = TexManager.GetTexture("sozai", AtoraTextureReadBlock);
    HoleGray = TexManager.GetTexture("holegray", AtoraTextureReadBlock);
    HoleGold = TexManager.GetTexture("holegold", AtoraTextureReadBlock);
    ObTip = TexManager.GetTexture("obtip", AtoraTextureReadBlock);
    ObPerson = TexManager.GetTexture("obperson", AtoraTextureReadBlock);
    VillageBar = TexManager.GetTexture("viltag", AtoraTextureBaseBlock);
    VillageName = TexManager.GetTexture("vilname", AtoraTextureBaseBlock);
}

void DrawMenuAtoraSelect() {
    int alpha;
    int tint;
    int fade;

    alpha = 0x80;

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
        ((sceGsTex0 *) &frame.tex0)->bits.tcc = 0;
        sceGsTexa texa = mgTexa;
        texa.AEM = 1;
        texa.TA0 = 0x80;
        MGSetGsTEXA(&texa);
        tint = 0;
        fade = 0;

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
        MGSetGsTEXA(NULL);

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

/**
 * Draws the Atla town board, its inventory, and the active cursor.
 *
 * @mangled DrawAtoraSelect__Fi
 * @address 0x0021AE80
 * @size 0xFEC
 */
static void DrawAtoraSelect(int fade) {
    int             open_mode;
    int             event;
    int             alpha;
    EDITPARTS_INFO *info;
    int             settled;
    int             remaining;
    int             all_tips;
    int             parts;
    float           target;
    float           y;
    float           cursor_x;
    float           cursor_y;
    float           step;
    int             waku_size;
    int             waku_x;
    int             waku_y;
    int             odd;
    int             count;
    int             board_y;
    int             u;
    int             v;

    AtoraTexInfoGet();
    alpha = 0x80;

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

    open_mode = MenuAtoraSel.open_mode;
    MenuAtoraSel.name_alpha = alpha;
    info = SearchAtoraInfo(MenuAtoraSel.board_pos);
    event = 0;
    all_tips = 0;

    if (info != NULL) {
        event = AtoraCompOrEvent(info);
        all_tips = AtoraAllTipGet(info->parts_no);
    }

    MenuTextureReload(AtoraTextureReadBlock);
    target = 146.0f - 130.0f * MenuAtoraSel.board_pos;
    step = (target - MenuAtoraSel.scroll_y) / 2.0f;
    odd = (int) step % 2;
    step /= 2.0f;
    step += odd;
    MenuAtoraSel.scroll_y += step;
    settled = 1;

    if (!((float) abs((int) ((float) MenuAtoraSel.scroll_y - target)) <= 4.0f)) {
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

    if (info != NULL) {
        if (info->placed == info->stock) {
            settled = 0;
        }
    } else {
        settled = 0;
    }

    remaining = AtraBoardMaxNum(MenuAtoraSel.map_no);
    parts = CommonMenuAtoraInfo->GetNextParts(-1);
    y = MenuAtoraSel.scroll_y;

    if (AtoraTextureEnterFlag != 0) {
        AtoraHeyCnt++;

        while (0 <= parts) {
            DrawAtora(0x38, (int) y, parts, alpha);
            parts = CommonMenuAtoraInfo->GetNextParts(parts);
            y += 130.0f;
            remaining--;
        }

        while (remaining > 0) {
            DrawAtoraNothing(0x38, (int) y, alpha);
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

            DrawMenu2DSprite(CompleteTex, CRect_i_(0x83, (int) (1.0f + sign_y), source.width, source.height), source, 6, 6, 6, (alpha * 0x50) >> 7);
            DrawMenu2DSprite(CompleteTex, CRect_i_(0x80, (int) sign_y, source.width, source.height), source, alpha);
        }

        if (MenuAtoraSel.step == ATORA_STEP_COMPLETE_FLASH) {
            sceGsAlpha blend = mgAlpha;
            blend.bits.a = 0;
            blend.bits.b = 2;
            blend.bits.c = 0;
            blend.bits.d = 1;
            MGSetGsALPHA(&blend);
            int flash = (int) (128.0f - 3.0f * MenuAtoraSel.step_count);

            if (flash < 5) {
                flash = 0;
                MenuAtoraSel.step = ATORA_STEP_RUN;
            }

            DrawMenu2DSprite(Sozai, CRect_i_(0x34, 0x8B, 0x10C, 0x82), CRect_i_(0x8C, 0x1C0, 0x74, 0x40), flash);
            MGSetGsALPHA(NULL);
        }

        AtoraBoardFadeEffect();
    }

    count = PersonalRetMax(MenuAtoraSel.board.page);
    board_y = 0x7F - MenuAtoraSel.board.top_row * 0x28;
    board_y = (int) ((MenuAtoraSel.board.y += (board_y - MenuAtoraSel.board.y) / 4.0f), MenuAtoraSel.board.y);
    MenuTextureReload(PerBoardTex->block);
    DrawPerBoardDraw(0, count, 0x168, board_y, 0x81, 0x121, PerBoardTex, 0x80);

    if (AtoraTextureEnterFlag != 0) {
        MenuTextureReload(AtoraTextureReadBlock);
        CommonIconDraw(MenuAtoraSel.board.page, count, 0x168, board_y + 1, 0x81, 0x121, alpha);
    }

    MenuTextureReload(PerBoardTex->block);
    PersonalBoardTagDraw(MenuAtoraSel.board.page, 0x154, 0x78, PerBoardTex, 0, 0x80);

    if (PerBoardTex != NULL) {
        MenuTextureReload(PerBoardTex->block);
        PersonalBoardDrawWaku(0x154, 0x78, PerBoardTex, 0x80);
        PersonalBoardScrlBarDraw(count, 0x154, 0x78, MenuAtoraSel.board.scroll, MenuAtoraSel.board.top_row, PerBoardTex, 0x80);
        PersonalBoardMaxDraw(count, 0x154, 0x78, PerBoardTex, 0x80);
    }

    AtoraNameDraw(0);

    if (0 < GetAtoraMaxVillage() - 3) {
        int arrow_y = (int) (66.0f + 4.0f * sinf(PI * (float) (CursorVibeCnt % 79 - 40) / 40.0f));
        DrawMenu2DSprite(PerBoardTex, CRect_i_(0x146, arrow_y, 0x1A, 0x18), CRect_i_(0x62, 0x14, 0x1A, 0x18), 0x80);
        DrawMenu2DSprite(PerBoardTex, CRect_i_(0x20C, arrow_y, 0x1A, 0x18), CRect_i_(0x7C, 0x14, 0x1A, 0x18), 0x80);
    }

    if (PerBoardTex != NULL) {
        MenuTextureReload(PerBoardTex->block);
    }

    switch (MenuAtoraSel.mode) {
        case ATORA_SIDE_BOARD:
            if (info == NULL) {
                cursor_x = 76.0f;
                cursor_y = 176.0f;
            } else if (MenuAtoraSel.board.cursor == 0) {
                cursor_x = 56.0f;
                cursor_y = 176.0f;
                waku_x = (int) (cursor_x + 2.0f);
                waku_y = (int) (cursor_y - 15.0f);
                waku_size = 0x56;
            } else {
                waku_size = 0x24;
                cursor_x = (MenuAtoraSel.board.cursor + 2) % 3 * 0x2C + 0x92;

                if (MenuAtoraSel.board.cursor > 0 && MenuAtoraSel.board.cursor < 4) {
                    cursor_y = 167.0f;
                } else {
                    cursor_y = 210.0f;
                }

                waku_x = (int) (3.0f + cursor_x);
                waku_y = (int) (cursor_y - 5.0f);
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
            waku_x = (int) cursor_x;
            waku_y = (int) cursor_y;
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
                if (info != NULL) {
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

            if (texture != NULL) {
                float    sway_x = 7.0f * cosf(0.0805536583f * CursorVibeCnt);
                float    sway_y = 5.0f * sinf(0.116355285f * CursorVibeCnt);
                int      tip_x = (int) (2.0f + (MenuAtoraSel.cursor_x + sway_x));
                int      tip_y = (int) (MenuAtoraSel.cursor_y + sway_y - 14.0f);
                CRect_i_ source(u, v, 0x24, 0x24);
                DrawMenu2DSprite(texture, CRect_i_(tip_x + 5, tip_y + 3, 0x24, 0x24), source, 10, 10, 10, 0x50);
                float hand_x = MenuAtoraSel.cursor_x + sway_x;
                int   hand_ix = (int) hand_x;
                float hand_y = MenuAtoraSel.cursor_y + sway_y;
                int   hand_iy = (int) hand_y;
                DrawMenu2DSprite(StayTex, CRect_i_((int) hand_x + 6, (int) hand_y + 3, 0x20, 0x20), CRect_i_(0x80, 0x28, 0x20, 0x20), 10, 10, 10, 0x50);
                DrawMenu2DSprite(texture, CRect_i_(tip_x, tip_y, 0x24, 0x24), source, 0x80);
            }
        }

        int draw_waku = 1;

        switch (MenuAtoraSel.mode) {
            case ATORA_SIDE_BOARD:
                if (info == NULL) {
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
 *
 * @mangled AtoraTextureEnter__Fv
 * @address 0x21BE70
 * @size 0x1A4
 */
static int AtoraTextureEnter() {
    LOADTEXTURE_INFO2 tex[3] = {
        {"#frame_image3#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
        {NULL,                                        0, 0},
        {NULL,                                        0, 0}
    };
    BG_READ_INFO *bg;

    tex[1].block_no = tex[0].block_no = AtoraTextureReadBlock;
    bg = GetReadBGFile(0);
    char name[16] = "a%d.img";
    sprintf(name, name, MenuAtoraSel.map_no + 1);
    tex[1].name = (char *) GetPackFile((u_int *) bg->buffer, name, NULL);
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
int tip_table[3] = {3, 1, 2};

/**
 * Returns which of the three chip groups a chip number belongs to.
 *
 * @mangled GetTipKind__Fi
 * @address 0x21C020
 * @size 0x64
 */
static int GetTipKind(int tip_no) {
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
 *
 * @mangled CompTip__Fii
 * @address 0x21C090
 * @size 0xCC
 */
static int CompTip(int tip_a, int tip_b) {
    int rank_a;
    int rank_b;

    rank_a = tip_table[GetTipKind(tip_a)];
    rank_b = tip_table[GetTipKind(tip_b)];

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
 *
 * @mangled SeitonAtoraTipBoardSub__Fv
 * @address 0x21C160
 * @size 0x108
 */
static int SeitonAtoraTipBoardSub() {
    int  rank;
    int  kind;
    s16 *list;
    int  i;
    int  j;
    int  moved;

    kind = tip_sort_type;

    for (rank = 0; rank < 3; rank++) {
        tip_table[kind] = rank;
        kind++;

        if (kind >= 3) {
            kind = 0;
        }
    }

    tip_table[0] = 3;
    moved = 0;
    list = MenuAtoraSel.board.atla_elements;

    for (i = 0; i < 119; i++) {
        for (j = i + 1; j < 120; j++) {
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
 *
 * @mangled SeitonAtoraTipBoard__Fv
 * @address 0x21C270
 * @size 0x70
 */
static void SeitonAtoraTipBoard() {
    int i;

    for (i = 0; i < 3; i++) {
        if (SeitonAtoraTipBoardSub()) {
            break;
        }

        tip_sort_type++;

        if (tip_sort_type >= 3) {
            tip_sort_type = 1;
        }
    }
}

int MenuAtoraSelectKey() {
    int             result;
    int             max_village;
    int             prev_village;
    int             msg_no;
    int             msg_base;
    int             max_pos;
    int             event_no;
    int             tip_no;
    int             held;
    EDITPARTS_INFO *info;

    result = ATORA_SELECT_NONE;

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
                int block[1] = {0};
                block[0] = AtoraTextureReadBlock;
                event_no = SearchAtoraInfo(MenuAtoraSel.board_pos)->parts_no;
                MenuAtoraSel.event_board_pos = MenuAtoraSel.board_pos;
                MenuAtoraSel.last_board_pos = MenuAtoraSel.board_pos;
                InitEastKingEvent(event_no, block, AtoraOffsetBuf);
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

    msg_no = 0;

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

static int AtoraBoardKey() {
    int movable[8];
    int open_mode = MenuAtoraSel.open_mode;
    int max = AtraBoardMaxNum(MenuAtoraSel.map_no);
    int old_cursor = MenuAtoraSel.board.cursor;
    int old_pos = MenuAtoraSel.board_pos;
    int moved = 0;
    int result = ATORA_SELECT_NONE;
    int se = -1;
    int to;

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
            AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable);
            MenuAtoraSel.board.cursor += 3;
            to = MenuAtoraSel.board.cursor - 1;
            to = AtoraBoardGoToPos(movable, to, 3);

            if (movable[to] != 0) {
                MenuAtoraSel.board.cursor = to + 1;
            } else {
                MenuAtoraSel.board.cursor -= 3;
                to = MenuAtoraSel.board.cursor - 1;
                to = AtoraBoardGoToPos(movable, to, 0);

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
                AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable);
                to = MenuAtoraSel.board.cursor - 1;
                to = AtoraBoardGoToPos(movable, to, 0);

                if (movable[to] != 0) {
                    MenuAtoraSel.board.cursor = to + 1;
                } else {
                    MenuAtoraSel.board.cursor = 0;
                }
            }
        } else {
            AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable);
            MenuAtoraSel.board.cursor += 3;
            to = MenuAtoraSel.board.cursor - 1;
            to = AtoraBoardGoToPos(movable, to, 3);

            if (movable[to] != 0) {
                MenuAtoraSel.board.cursor = to + 1;
            } else if (MenuAtoraSel.board_pos < max - 1) {
                MenuAtoraSel.board_pos++;
                MenuAtoraSel.board.cursor -= 3;
                AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable);
                to = MenuAtoraSel.board.cursor - 1;
                to = AtoraBoardGoToPos(movable, to, 0);

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
        AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable);
        int event = 0;

        if (info != NULL) {
            event = AtoraCompOrEvent(info);
        }

        int to_list = 0;

        if (info == NULL || event != 0) {
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

        if (info != NULL) {
            event = AtoraCompOrEvent(info);
        }

        if (event != 0) {
            MenuAtoraSel.board.cursor = 0;
        }
    }

    if (moved == 0) {
        if (GamePad.Down(PAD_CROSS) != 0) {
            EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);

            if (info != NULL) {
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
                    AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, movable);
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

                            if (info == NULL) {
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
            if (parts != NULL) {
                for (int i = 0; i < 6; i++) {
                    if (parts->elements[i].id >= 0) {
                        parts->elements[i].enabled = true;
                    }
                }

                parts->completion_flags = 1;
            }

            MenuAtoraSel.board.cursor = 0;
        }

        if (GamePad.Down2(PAD_CIRCLE)) {
            if (parts != NULL) {
                for (int i = 0; i < 6; i++) {
                    if (parts->elements[i].id >= 0) {
                        parts->elements[i].enabled = false;

                        for (int j = 0; j < 100; j++) {
                            if (MenuAtoraSel.board.atla_elements[j] < 0) {
                                MenuAtoraSel.board.atla_elements[j] = parts->elements[i].id;
                                break;
                            }
                        }
                    }
                }
            }

            parts->completion_flags = 0;
        }

        if (GamePad.Down2(PAD_R1) && parts != NULL) {
            printf("info----------,,,\tID \t\t%d\n", parts->parts_no);
            printf("\t\tcomplete_event\t\t%d\n", parts->kind);

            for (int i = 0; i < 6; i++) {
                if (parts->elements[i].id < 0) {
                    break;
                }

                if (parts->elements[i].enabled != 0) {
                    for (int j = 0; j < 120; j++) {
                        if (MenuAtoraSel.board.atla_elements[j] < 0) {
                            MenuAtoraSel.board.atla_elements[j] = parts->elements[i].id;
                            parts->elements[i].enabled = false;
                            break;
                        }
                    }
                }
            }

            parts->completion_flags = 0;
        }

        if (GamePad.Down2(PAD_L1)) {
            if (parts != NULL) {
                for (int i = 0; i < 6; i++) {
                    if (parts->elements[i].id >= 0) {
                        parts->elements[i].enabled = true;
                        int npc_no = parts->elements[i].npc_no;

                        if (npc_no >= 0) {
                            SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                            if (npc != NULL) {
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

            if (info != NULL) {
                for (int i = 0; i < 6; i++) {
                    EDITPARTS_ELEMENT *element = &info->elements[i];

                    if (info->elements[i].id >= 0) {
                        if (info->elements[i].id >= 0) {
                            element->enabled = true;
                        }

                        int npc_no = element->npc_no;

                        if (npc_no >= 0) {
                            SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                            if (npc != NULL) {
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

                if (info != NULL) {
                    info->obtained = 1;

                    for (int i = 0; i < 6; i++) {
                        EDITPARTS_ELEMENT *element = &info->elements[i];

                        if (info->elements[i].id >= 0) {
                            if (info->elements[i].id >= 0) {
                                element->enabled = true;
                            }

                            int npc_no = element->npc_no;

                            if (npc_no >= 0) {
                                SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                                if (npc != NULL) {
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

                if (info != NULL) {
                    for (int i = 0; i < 6; i++) {
                        EDITPARTS_ELEMENT *element = &info->elements[i];

                        if (info->elements[i].id >= 0) {
                            int npc_no = element->npc_no;

                            if (npc_no >= 0) {
                                SV_GRD_NPC *npc = SaveData->GetGrdNPCData(MapNo, npc_no);

                                if (npc != NULL) {
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

static int AtoraTipKey() {
    int result = ATORA_SELECT_NONE;
    int pos = MenuAtoraSel.board.cursor;
    int mode = MenuAtoraSel.mode;
    int page = MenuAtoraSel.board.page;

    switch (PersonalBoardKey()) {
        case 1: {
            EDITPARTS_INFO *info = SearchAtoraInfo(MenuAtoraSel.board_pos);
            int             event = 0;

            if (info != NULL) {
                event = AtoraCompOrEvent(info);
            }

            int enable[6];
            AtoraBoardEnableMovePos(MenuAtoraSel.board_pos, enable);
            MenuAtoraSel.mode = ATORA_SIDE_BOARD;

            if (info == NULL || event != 0) {
                MenuAtoraSel.board.cursor = 0;
            } else {
                int slot = AtoraBoardGoToPos(enable, MenuAtoraSel.board.cursor < MenuAtoraSel.board.top_row * 5 + 10 ? 2 : 5, 0);

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

static void AtoraMenuTipCancel() {
    EDITPARTS_INFO    *info;
    EDITPARTS_ELEMENT *element;
    s16                tip_no;

    switch (NowTipHavePt->mode) {
        case ATORA_SIDE_CHIP_LIST:
            tip_no = MenuAtoraSel.board.atla_elements[NowTipHavePt->slot];
            MenuAtoraSel.board.atla_elements[NowTipHavePt->slot] = NowTipHavePt->tip_no;
            NowTipHavePt->tip_no = tip_no;
            break;
        case ATORA_SIDE_BOARD:
            info = SearchAtoraInfo(NowTipHavePt->parts_no);
            element = &info->elements[NowTipHavePt->slot];

            if (element->id == NowTipHavePt->tip_no && element->enabled == 0) {
                element->enabled = true;
            }

            MenuAtoraSel.board_pos = NowTipHavePt->parts_no;
            NowTipHavePt->tip_no = -1;
            break;
    }
}

static void AtoraBoardFadeEffect() {
    CTexture frame = *TexManager.GetTexture("frame_image", -1);

    ((sceGsTex0 *) &frame.tex0)->bits.tcc = 0;
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
    MGSetGsTEXA(NULL);
}

void AtoraNameDraw(int unused) {
    int             prev_mes_no[3];
    int             pos;
    int             y;
    int             i;
    int             mes_no;
    int             x;
    int             alpha;
    EDITPARTS_INFO *info;

    MenuTextureReload(AtoraNameMes.tex_block);
    pos = MenuAtoraSel.board_pos - 1;

    for (int j = 0; j < 3; j++) {
        prev_mes_no[j] = AtoraNameMes.mes_no[j];
    }

    y = MenuAtoraSel.scroll_y + 92 + 130.0f * pos;

    for (i = 0; i < 3; i++) {
        if (y < 96 || y > 341) {
            mes_no = 999;
        } else {
            info = SearchAtoraInfo(pos);

            if (info != NULL) {
                mes_no = GetAtraMsgNo(MenuAtoraSel.map_no, info->parts_no);
            } else {
                mes_no = 999;
            }
        }

        AtoraNameMes.mes_no[i] = mes_no;
        x = GetMsgLengthMenu(&AtoraNameMes, mes_no);

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

    alpha = MenuAtoraSel.name_alpha;

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

int InitMenuSave(int mode, int block_no, u_long128 *buffer) {
    u_long128 *data;
    int        clear;

    data = buffer;

    if (buffer == NULL) {
        data = (u_long128 *) read_buffer;
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
            clear = SaveMenu.mode == SAVE_MENU_MODE_ENDING;
            *(s32 *) &((SV_CONFIG_SYS *) SaveData->GetConfigData())->game_clear_area[2] = clear;
            printf("SaveData clear flag = %d\n", *(s32 *) &((SV_CONFIG_SYS *) SaveData->GetConfigData())->game_clear_area[2]);
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
 *
 * @mangled ExitSaveSelect__Fv
 * @address 0x21FD80
 * @size 0x144
 */
static void ExitSaveSelect() {
    s32 *config;

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
                config = (s32 *) SaveData->GetConfigData();

                if (config != NULL) {
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

// The save menu's steps that menu_save.cpp defines.
int SaveMenuKeySaveCheck();
int SaveMenuKeySaveDecide();
int SaveMenuKeySave();
int SaveMenuKeyEndSave();
int SaveMenuKeyLoadDecide();
int SaveMenuKeyLoad();
int SaveMenuKeyArart();
int SaveMenuKeyNewDir();
int SaveMenuKeyNewDirSelect();
int SaveMenuKeyFormat();
int SaveMenuKeyUnFormat();
int SaveMenuKeyDifVersion();
int SaveMenuKeyDelete();
int SaveMenuKeyCopy();
int SaveMenuKeyAfterEnding();
int SaveMenuKeySaveEnding();
int SaveMenuKeySaveDecideEnding();
int SaveMenuKeyEndSaveEnding();

/** The save menu's steps, by SAVE_MENU_STATE::key_no. */
extern int (*SaveMenuFunc[26])();

int MenuSaveKey() {
    int            prev_func_no;
    int            result;
    int            now_func_no;
    int            msg_no;
    int            mes_value;
    MC_ERROR_INFO *error;
    MC_ERROR_INFO *last_error;

    if (!SaveMenu.texture_ready) {
        SaveMenu.texture_ready = SaveMenuTextureEnter();
    }

    prev_func_no = McAccess.GetFuncNo();
    result = McAccess.Step();
    now_func_no = McAccess.GetFuncNo();

    if (prev_func_no == now_func_no) {
        last_error = &McAccess.error;

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
                last_error->code = MC_ERROR_NONE;
            case MC_OPERATION_GET_ALL_SAVE_FILE_INFO:
                if (result < 0) {
                    SaveMenu.key_no = SAVE_KEY_ALERT;
                    SaveMenu.alert_no = SAVE_ALERT_CARD_ERROR;
                    McAccess.SetFuncNo(MC_OPERATION_IDLE);
                    break;
                }

                switch (last_error->code) {
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
                error = &McAccess.error;

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

    msg_no = GetSaveMenuMsgNo();

    if (CommonMenuMes2.mes_made != msg_no) {
        mes_value = SaveMenu.file_no + 1;

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

void DrawMenuSave(char *frame_name);

static int SaveMenuKeyFadeIn() {
    if (SaveMenu.step_time > 14) {
        SaveMenu.key_no = SAVE_KEY_FILE_SELECT;
        SaveMenu.step_time = 0;
    }

    return 1;
}

static int SaveMenuKeyFadeOut() {
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

static int SaveMenuKeyModeSelect() {
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

static int SaveMenuKeyMcSelect() {
    int prev_slot;

    prev_slot = SaveMenu.file_no;

    if (GamePad.Down(PAD_UP | PAD_DOWN)) {
        if (SaveMenu.file_no) {
            SaveMenu.file_no = 0;
        } else {
            SaveMenu.file_no = 1;
        }
    }

    if (prev_slot != SaveMenu.file_no) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        switch (SaveMenu.mode) {
            case SAVE_MENU_MODE_LOAD:
                SaveMenu.key_no = SAVE_KEY_FADE_OUT;
                ExitSaveSelect();
                break;
            case SAVE_MENU_MODE_SAVE:
                SaveMenu.key_no = SAVE_KEY_FADE_OUT;
                CommonMenuMes2.stay_frame = false;
                break;
            case SAVE_MENU_MODE_ENDING:
                SaveMenu.key_no = SAVE_KEY_FADE_OUT;
                ExitSaveSelect();
                break;
        }

        SaveMenu.step_time = 0;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 1;
    }

    if (GamePad.Down(PAD_CROSS) && SaveMenu.texture_ready) {
        SaveMenu.key_no = SAVE_KEY_CHECK_MC_TYPE;
        McAccess.port = SaveMenu.file_no;
        McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        return 1;
    }

    if (GamePad.Down2(PAD_CROSS) && GamePad.Down2(PAD_CIRCLE)) {
        SaveMenu.key_no = SAVE_KEY_UNFORMAT;
        McAccess.SetFuncNo(MC_OPERATION_UNFORMAT);
        McAccess.port = SaveMenu.file_no;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    }

    return 1;
}

static int SaveMenuKeyCheckMcType() {
    MC_CARD_INFO *card;

    card = &McAccess.card[McAccess.port];

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

static int SaveMenuKeyCheckMc() {
    MC_CARD_INFO *card;

    printf("check end !!\n");
    card = &McAccess.card[McAccess.port];
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
        SaveMenu.file_no = ((s32 *) SaveData->GetConfigData())[17];
    }

    return 1;
}

static int SaveMenuKeyLoadConfig() {
    McAccess.SetFuncNo(MC_OPERATION_GET_ALL_SAVE_FILE_INFO);
    SaveMenu.key_no = SAVE_KEY_FILE_SELECT;

    if (*(s32 *) &((SV_CONFIG_SYS *) SaveData->GetConfigData())->game_clear_area[2] != 0) {
        GameClearFlag = 1;
    }

    SaveMenu.file_no = ((s32 *) SaveData->GetConfigData())[17];
    return 1;
}

static int SaveMenuKeyFileSelect() {
    int            prev_file;
    SAVEDATA_INFO *info;

    prev_file = SaveMenu.file_no;

    if (GamePad.Down(PAD_DOWN)) {
        SaveMenu.file_no++;

        if (SaveMenu.file_no >= 12) {
            SaveMenu.file_no--;
        }
    }

    if (GamePad.Down(PAD_UP) && 0 < SaveMenu.file_no) {
        SaveMenu.file_no--;
    }

    if (prev_file != SaveMenu.file_no) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        SaveMenu.key_no = SAVE_KEY_MC_SELECT;
        SaveMenu.file_no = McAccess.port;
        SaveMenu.step_time = 0;
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return 1;
    }

    if (GamePad.Down(PAD_CROSS)) {
        switch (SaveMenu.access_kind) {
            case SAVE_ACCESS_SAVE:
                SaveMenu.key_no = SAVE_KEY_SAVE_CHECK;
                McAccess.SetFuncNo(MC_OPERATION_SEARCH_TYPE);
                break;
            case SAVE_ACCESS_LOAD:
                info = &McAccess.file_info[SaveMenu.file_no];

                if (info->state) {
                    SaveMenu.key_no = SAVE_KEY_LOAD_DECIDE;
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
        }

        ComMenuSePlay(MENU_SOUND_CONFIRM);
        return 1;
    }

    return 1;
}
