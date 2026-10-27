#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

#include "battle_globals.hpp"
#include "battlemenu.hpp"
#include "battlemenu/internal.hpp"
#include "btactstatus.hpp"
#include "camera.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "eastking.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_manual.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "menuetc.hpp"
#include "menuiconautoget.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "monstorunit.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "shot_effect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "vutext.hpp"
#include "weapon_buildup.hpp"
#include "weaponlevelup.hpp"

namespace dc::game {

using namespace dc::battlemenu;

namespace {

int  GetVisitInfo(int place, int menu_mode);
void DrawStatusNumberNowAndMax(int *values, int x, int y, int color, int alpha);
void BattleMenuAppear();
int  BattleMenuExit();
void InitItemTrushStart();
int  BattleMenuAtoraKey();
void BattleMenuOptionKey();
void BattleMenuSaveKey();
void BattleManualDraw();
int  WeaponSelectKey();
void WeaponMenuActWepKey();
void WeaponMenuAttachWepKey();
void WeaponMenuAttachKey();
int  WorldMapMoveKey();
void DrawWorldMap(int alpha);

} // namespace

MenuClsMes          MenuMes;
CDataAlloc2<1>      MenuExCashBuffer(-1);
MENU_ICON_POS       NorMenuIcon[8];
float               SysCur[3];
MENU_CHARA_INFO     MenuChara;
CCharacter          MenuCharaFrame;
CCharacter          DngWeaponFrm[12];
WEP_MENU_INFO       WepMenu;
ITEM_MENU_MODE_INFO ItemMenuMode;
CMenuIconAutoGet    IconAutoGet;
MENU_MOVE_INFO      MenuMove;

RECT NumberSprite[2] = {
    {0, 176, 12, 12},
    {0, 158, 12, 18}
};

s16 BtlDrawTbl[2][8] = {
    {0, 1, 2, 3, 7, 4, 6, 0},
    {0, 1, 2, 3, 8, 5, 4, 6}
};

SYS_CHARA_INFO SysChara[6] = {
    {0, 0, {0, 0}, 0.0f, 0.0f},
    {1, 0, {0, 0}, 0.0f, 0.0f},
    {2, 0, {0, 0}, 0.0f, 0.0f},
    {3, 0, {0, 0}, 0.0f, 0.0f},
    {4, 0, {0, 0}, 0.0f, 0.0f},
    {5, 0, {0, 0}, 0.0f, 0.0f},
};

s16 MenuDefaultWeaponNo[7] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN, 81};

s16 MenuCharaWeaponMax[6] = {42, 15, 17, 16, 16, 14};

MENU_WEP_POLY_OFFSET MenuWepPolyOffset[6] = {
    {{0.0f, 0.0f, 0.0f},  1.0f, {-HALF_PI, 0.0f, 0.62831855f},         {0.0f, 0.028559932f, 0.0f}},
    {{-0.3f, 0.5f, 0.0f}, 1.4f, {-HALF_PI, 0.0f, 0.62831855f},         {0.0f, 0.028559932f, 0.0f}},
    {{0.0f, 0.0f, 0.0f},  0.8f, {-HALF_PI, 0.0f, 0.62831855f},         {0.0f, 0.028559932f, 0.0f}},
    {{-1.4f, 2.1f, 0.0f}, 4.2f, {-HALF_PI, -0.16453463f, 0.62831855f}, {0.0f, 0.028559932f, 0.0f}},
    {{-0.8f, 0.7f, 0.0f}, 0.7f, {-HALF_PI, -0.8005375f, 0.62831855f},  {0.0f, 0.028559932f, 0.0f}},
    {{-0.4f, 0.5f, 0.0f}, 1.0f, {-HALF_PI, 0.0f, 0.62831855f},         {0.0f, 0.028559932f, 0.0f}},
};

GRADATION_COLOR_INFO2 MenuColorInfo2[26] = {
    {{{0xF5, 0xAB, 0x48, 0x80}, {0x8C, 0x8E, 0x64, 0x80}, {0x57, 0x59, 0x20, 0x80}, {0x1F, 0x2E, 0x2A, 0x80}}},
    {{{0x7D, 0xB2, 0xFF, 0x80}, {0x73, 0xA1, 0xC6, 0x80}, {0x2E, 0x5B, 0x78, 0x80}, {0x2D, 0x4C, 0x57, 0x80}}},
    {{{0x69, 0x9E, 0xF7, 0x80}, {0x5F, 0x8D, 0xB2, 0x80}, {0x24, 0x51, 0x6E, 0x80}, {0x0F, 0x2E, 0x39, 0x80}}},
    {{{0x1D, 0xF7, 0xA1, 0x80}, {0x61, 0xBA, 0x9D, 0x80}, {0x0A, 0x7B, 0x0A, 0x80}, {0x10, 0x44, 0x2F, 0x80}}},
    {{{0xFE, 0xC4, 0x00, 0x80}, {0x99, 0xA9, 0x63, 0x80}, {0x55, 0x62, 0x00, 0x80}, {0x22, 0x3C, 0x16, 0x80}}},
    {{{0x21, 0xBF, 0xFD, 0x80}, {0x40, 0xBA, 0xB4, 0x80}, {0x0B, 0x5F, 0x6E, 0x80}, {0x05, 0x44, 0x38, 0x80}}},
    {{{0xDD, 0x96, 0xFD, 0x80}, {0x91, 0x95, 0xB1, 0x80}, {0x46, 0x4B, 0x6E, 0x80}, {0x1F, 0x32, 0x37, 0x80}}},
    {{{0xED, 0x00, 0x00, 0x80}, {0x98, 0x6F, 0x64, 0x80}, {0x65, 0x00, 0x00, 0x80}, {0x37, 0x2C, 0x24, 0x80}}},
    {{{0x1F, 0x2E, 0xF1, 0x80}, {0x54, 0x94, 0xC9, 0x80}, {0x0D, 0x1B, 0x7D, 0x80}, {0x1A, 0x42, 0x58, 0x80}}},
    {{{0xEE, 0xBE, 0x00, 0x80}, {0xB3, 0xD7, 0x62, 0x80}, {0x65, 0x6E, 0x00, 0x80}, {0x42, 0x69, 0x23, 0x80}}},
    {{{0x0F, 0x95, 0x00, 0x80}, {0x4D, 0xD7, 0x65, 0x80}, {0x06, 0x57, 0x00, 0x80}, {0x17, 0x69, 0x24, 0x80}}},
    {{{0xEE, 0x15, 0xF1, 0x80}, {0xAB, 0x91, 0xB9, 0x80}, {0x65, 0x0C, 0x7D, 0x80}, {0x3E, 0x40, 0x50, 0x80}}},
    {{{0x38, 0xC3, 0x0B, 0x80}, {0x59, 0xC4, 0x6D, 0x80}, {0x15, 0x66, 0x00, 0x80}, {0x1B, 0x55, 0x2A, 0x80}}},
    {{{0x77, 0x56, 0xE5, 0x80}, {0x6F, 0x94, 0xA1, 0x80}, {0x2C, 0x2B, 0x6E, 0x80}, {0x22, 0x3D, 0x3E, 0x80}}},
    {{{0x52, 0xC2, 0xED, 0x80}, {0x62, 0xC3, 0xA7, 0x80}, {0x1F, 0x66, 0x71, 0x80}, {0x1E, 0x55, 0x40, 0x80}}},
    {{{0xB0, 0x7D, 0x0C, 0x80}, {0x82, 0x97, 0x6A, 0x80}, {0x41, 0x41, 0x02, 0x80}, {0x29, 0x3E, 0x24, 0x80}}},
    {{{0x8C, 0xEE, 0x08, 0x80}, {0x76, 0xA8, 0x69, 0x80}, {0x34, 0x85, 0x00, 0x80}, {0x25, 0x47, 0x24, 0x80}}},
    {{{0xCC, 0x80, 0x07, 0x80}, {0x8C, 0xA2, 0x63, 0x80}, {0x4C, 0x42, 0x00, 0x80}, {0x2D, 0x43, 0x21, 0x80}}},
    {{{0x24, 0xD4, 0xF3, 0x80}, {0x52, 0x9C, 0xB3, 0x80}, {0x0D, 0x76, 0x73, 0x80}, {0x18, 0x40, 0x45, 0x80}}},
    {{{0xC0, 0xBC, 0xBC, 0x80}, {0x87, 0xC3, 0xA1, 0x80}, {0x47, 0x62, 0x59, 0x80}, {0x2B, 0x55, 0x1A, 0x80}}},
    {{{0xCC, 0xB1, 0x04, 0x80}, {0x8C, 0xC3, 0x54, 0x80}, {0x4C, 0x5C, 0x00, 0x80}, {0x2C, 0x55, 0x1A, 0x80}}},
    {{{0xD4, 0x38, 0xCC, 0x80}, {0x8E, 0x86, 0xA2, 0x80}, {0x4F, 0x1A, 0x61, 0x80}, {0x2D, 0x36, 0x3E, 0x80}}},
    {{{0x40, 0xC1, 0x82, 0x80}, {0x00, 0x81, 0xFF, 0x80}, {0x40, 0xC1, 0x82, 0x80}, {0x00, 0x81, 0xFF, 0x80}}},
    {{{0xFF, 0x00, 0xC6, 0x80}, {0x3F, 0x00, 0x31, 0x80}, {0xFF, 0x00, 0xC6, 0x80}, {0x3F, 0x00, 0x31, 0x80}}},
    {{{0xFF, 0x00, 0x00, 0x80}, {0x3F, 0x00, 0x00, 0x80}, {0xFF, 0x00, 0x00, 0x80}, {0x3F, 0x00, 0x00, 0x80}}},
    {{{0xFF, 0x80, 0x00, 0x80}, {0xFF, 0x20, 0x00, 0x80}, {0xFF, 0x80, 0x00, 0x80}, {0xFF, 0x20, 0x00, 0x80}}},
};

s16   BtlMenuNowLang = 1;
s32   BattleMenuFlag = BTLMENU_STATE_APPEAR;
float chara_r_long = 118.0f;
s16   TrushMoveMax[3] = {3, 1, 3};
s32   BtlEffectFlag = BTLEFFECT_NONE;

s32             MenuMapJumpMode;
CSaveData      *BtlMenuSaveDataPt;
CDngStatusData *BtlMenuStatusPt;
s32             BtlMenuMode;
s32             BtlMenuBGMvol;
s32             BtlMenuReadBlock;
u_long128      *BtlMenuReadBuf;
s32             BtlMenuReadEndFlag;
s32             ItemMenuWeaponIconReadBlock;
s32             ItemMenuAlreadyReadWepIconTexFlag;
s32             MenuShadowReadBlock;
s32             BtlMenuExtBlock;
s32             BtlMenuExReadBlock;
s32             BtlMenuExReadFlag;
s32             MenuExtendReadBlock;
s32             MenuExTextureReadFlag;
u_long128      *ItemMenuCharaReadBuf;
s32             BtlMDSBuildCnt;
s32             BtlMenuManualReadBlock;
WEAPON_HAVE    *DngWepHavePt;
CWeaponLevelUp *WepMenuEffectReadBuf;
CTexture       *CharaStatus;
CTexture       *MenuCharaFace;
CTexture       *NonCharaFace;
CTexture       *BtStatus;
CTexture       *WepStatus;
CTexture       *BtlAlpha;
CTexture       *BtlHira;
CTexture       *BtlKata;
CTexture       *BtlEuro;
float           HelpWinHead[2];
u8              BtlWakuMake2;
s32             BtlHelpWinAlpha;
float           BtlHelpWinW;
float           BtlHelpWinH;
s32             MenuSelect[2];
ITEM_PACK      *MenuItemPackPt;
s16             EscapeDngFlg;
s16             RoomOutFlag;
float           MenuCharaMove;
float           PosAngle;
IHAVEITEM      *BtlHaveItemPt;
float           WepFrameRate;
float           WeaponPos;
float           WepPolyPos;
float           BtlEffectCt;
s32             MenuWarningMsgFlag;
u_long128      *BtlMenuCharaChangeBuf;

int GetDefaultWeaponNo(int character_no) {
    return MenuDefaultWeaponNo[character_no];
}

int IsDefaultWeapon(int weapon_no) {
    int owner = -1;
    int chara = 0;

    for (chara = 0; chara <= 5; chara++) {
        int default_weapon_no = GetDefaultWeaponNo(chara);

        if (default_weapon_no == weapon_no || default_weapon_no + 1 == weapon_no) {
            owner = chara;
            break;
        }
    }

    return owner;
}

void SetNowEquipWeaponDataForMsg(int item_no, int slot) {
    ItemMenuMode.message_item_no = item_no;
    ItemMenuMode.message_slot = slot;
}

void GetNowEquipWeaponDataForMsg(int &item_no, int &slot) {
    COM_ITEM_INFO *item_info = GetCommonItemInfo(ItemMenuMode.message_item_no);

    if (item_info != nullptr) {
        item_no = item_info->msg;
    } else {
        item_no = 0;
    }

    slot = ItemMenuMode.message_slot;
}

GRADATION_COLOR_INFO2 *GetGradationColorInfo2(int index) {
    return &MenuColorInfo2[index];
}

namespace {

/**
 * Gives the weapon the weapon menu's cursor is on.
 */
WEAPON_HAVE *GetNowSelectWeapon() {
    int             chara = WepMenu.chara;
    CDngStatusData *status = BtlMenuStatusPt;
    WEAPON_HAVE    *row = status->chara_weapons[chara];
    return &row[WepMenu.weapon_slot];
}

/**
 * Reports whether the party may leave the floor: freely, with an escape item, or not at
 * all.
 */
int EscapeDungeonMode() {
    int mode = ESCAPE_DNG_PAY;

    if (BtlMenuStatusPt->SearchItemIndexNo(ITEM_ESCAPE_POWDER) >= 0) {
        mode = ESCAPE_DNG_POWDER;
    }

    if (NowMonstorUnit->GetMonstorNum() <= 0) {
        mode = ESCAPE_DNG_FREE;
    }

    return mode;
}

} // namespace

void SetEscapeDngFlag(int flag) {
    EscapeDngFlg = flag;
}

void SetInteriorOutFlag(int flag) {
    RoomOutFlag = flag;
}

int GetInteriorOutFlag() {
    return RoomOutFlag;
}

namespace {

/**
 * Draws the two-line yes-or-no plate the dungeon menu asks with.
 */
void DrawDngYesNoDialog(int x, int y, int alpha) {
    CRect_i_ dst(x, y, 0x60, 0x20);
    CRect_i_ src(0, 0, 0x60, 0x20);

    for (int row = 0; row < 2; row++) {
        DrawMenu2DSprite(BtStatus, dst, src, alpha);
        dst.y += 0x1C;
        src.y += 0x20;
    }
}

/**
 * Gives how many icons the battle menu ring shows, which depends on the menu mode and
 * whether the manual entry is enabled.
 */
int GetMenuModeMax() {
    int icon_count = 0;

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON:
            icon_count = 7;
            break;
        case BATTLE_MENU_MODE_TOWN:
            icon_count = 8;
            break;
        default:
            break;
    }

    if (GetGameFlagForManualMenu() == 0) {
        icon_count--;
    }

    return icon_count;
}

/**
 * Writes the screen coordinates of one icon on the battle menu ring.
 */
void GetMenuIconPos(int icon, int *position) {
    int next = icon + 1;
    int x = next * 16 + 80;
    int y = icon * 40 + 54;

    if (next > 4) {
        x -= ((icon - 4) << 5) + 16;
    }

    position[0] = x;
    position[1] = y;
}

/**
 * Fills in the icon numbers the menu bar draws for one mode.
 */
void BtlMenuMekeIconInfo(int *icons, int menu_mode) {
    int icon_count = GetMenuModeMax();

    for (int i = 0; i < icon_count; i++) {
        icons[i] = BtlDrawTbl[BtlMenuMode][i];
    }

    if (BtlMenuMode != BATTLE_MENU_MODE_DUNGEON) {
        int special_icon = 0;

        switch (NowGetGameFlagForBtlMenu(BtlMenuMode)) {
            case MENU_MOVE_LOCKED:
            case MENU_MOVE_FIRST_DUNGEON:
                special_icon = 9;
                break;
            case MENU_MOVE_WORLD_MAP:
                special_icon = 8;
                break;
            case MENU_MOVE_INTERIOR_OUT:
                special_icon = 0xC;
                break;
            default:
                break;
        }

        icons[4] = special_icon;
    }
}

/**
 * Draws the battle menu ring and its icons, fading them out with the closing effect and
 * dimming them while a warning message is shown.
 */
void DrawBtlMenuBar() {
    int alpha = 0;
    int icon_alpha = 0;
    int i = 0;
    int x = 0;
    int menu_flag = 0;
    int brightness = 0;
    int selected = 0;
    int y = 0;

    if (BtlEffectFlag == BTLEFFECT_PAGE_CLOSE || BattleMenuFlag == BTLMENU_STATE_APPEAR) {
        alpha = 10.0f * BtlEffectCt;

        alpha = std::min(alpha, 128);
    }

    if (BtlEffectFlag == BTLEFFECT_PAGE_OPEN || BattleMenuFlag == BTLMENU_STATE_EXIT) {
        alpha = 128.0f - 10.0f * BtlEffectCt;

        alpha = std::max(alpha, 0);
    } else {
        alpha = 128;
    }

    NowGetGameFlagForBtlMenu(BtlMenuMode);
    int icon_count = GetMenuModeMax();
    int icons[8];
    BtlMenuMekeIconInfo(icons, BtlMenuMode);

    for (i = 0; i < icon_count; i++) {
        icon_alpha = alpha;
        brightness = 128;

        if (MenuWarningMsgFlag != 0) {
            brightness = 64;
        }

        menu_flag = BattleMenuFlag;

        if (BTLMENU_STATE_MAIN < menu_flag && menu_flag < BTLMENU_STATE_ITEM_OPEN) {
            icon_alpha = 0;
        }

        selected = 0;
        float icon_x = NorMenuIcon[i].x;
        x = icon_x;
        float icon_y = NorMenuIcon[i].y;
        float lowered_y = 2.0f + icon_y;
        y = 1.0f + lowered_y;

        if (i == MenuSelect[1]) {
            if ((menu_flag == BTLMENU_STATE_APPEAR || menu_flag == BTLMENU_STATE_EXIT) && menu_flag == BTLMENU_STATE_MOVE_OPEN) {
                icon_alpha = alpha;
            }

            selected = 1;
            x = icon_x - 10.0f;
            y = 1.0f + (icon_y - 2.0f);
            icon_alpha = 128;
        }

        if (icons[i] >= 0) {
            DrawMainMenuIcon(x, y, icons[i], selected, brightness, icon_alpha);
        }
    }
}

} // namespace

int GetLimmitMsg() {
    int message = 0;

    switch (BtlMenuStatusPt->res_limit_zone_current) {
        case RES_LIMIT_ZONE_NONE:
            message = -1;
            break;
        case RES_LIMIT_ZONE_NO_WEAPON_CHANGE:
            message = 0x19E;
            break;
        case RES_LIMIT_ZONE_THIRST:
            message = 0x19F;
            break;
        case RES_LIMIT_ZONE_TOAN:
        case RES_LIMIT_ZONE_XIAO:
        case RES_LIMIT_ZONE_GORO:
        case RES_LIMIT_ZONE_RUBY:
        case RES_LIMIT_ZONE_UNGAGA:
        case RES_LIMIT_ZONE_OSMOND:
            message = 0x1A0;
            break;
        default:
            break;
    }

    return message;
}

void DrawBattleMain() {
    PortDrawBattleMenuMain();
}

void DrawOtherCharaStatus(int x, int y, int chara, int alpha) {
    int bar_x = x - 5;
    int bar_y = y + 3;

    if (bar_y > 0xDC) {
        bar_y--;
    }

    CDngStatusData *status = BtlMenuStatusPt;
    float           hp = status->hp[chara];
    float           max_hp = status->max_hp[chara];

    max_hp = std::max(max_hp, 1.0f);

    int          slot = status->equipped_weapon_slot[chara];
    WEAPON_HAVE *weapons = status->chara_weapons[chara];
    WEAPON_HAVE *weapon = &weapons[slot];

    if (weapon == nullptr) {
        return;
    }

    float whp = weapon->durability_f;
    float max_whp = weapon->durability;

    max_whp = std::max(max_whp, 1.0f);

    int   water = static_cast<int>(status->water_now[chara]);
    int   max_water = static_cast<int>(status->water_max[chara]);
    float values[4] = {hp, max_hp, whp, max_whp};
    MGFillBox(CRect_i_((bar_x - 3) * 16, ((bar_y - 2) >> 1) * 16, 0x3A0, 0x30), 0x14, 0x14, 0x14, alpha);
    int hp_len = static_cast<int>(52.0f * hp / max_hp);
    MGFillBox(CRect_i_(bar_x * 16, (bar_y >> 1) * 16, 0x340, 0x20), 0x4F, 0x4F, 0x4F, alpha);
    CRect_i_ hp_rect(bar_x * 16, (bar_y >> 1) * 16, hp_len * 16, 0x20);
    int      r = 0x4D;
    int      g = 0xE7;
    int      b = 0xA5;

    if (!(15.6f < hp_len)) {
        r = 0xD6;
        g = 0x76;
        b = 0xEA;
    }

    MGFillBox(hp_rect, r, g, b, alpha);
    MGFillBox(CRect_i_((bar_x + 1) * 16, ((bar_y + 4) >> 1) * 16, 0x3A0, 0x30), 0x14, 0x14, 0x14, alpha);
    float whp_len = 52.0f * whp / max_whp;
    int   whp_width = static_cast<int>(whp_len);
    MGFillBox(CRect_i_((bar_x + 4) * 16, ((bar_y + 6) >> 1) * 16, 0x340, 0x20), 0x4F, 0x4F, 0x4F, alpha);
    MGFillBox(CRect_i_((bar_x + 4) * 16, ((bar_y + 6) >> 1) * 16, static_cast<int>(whp_len) * 16, 0x20), 0xFF, 0xB1, 0x49, alpha);
    MGFillBox(CRect_i_((bar_x + 5) * 16, ((bar_y + 10) >> 1) * 16, 0x3A0, 0x40), 0x14, 0x14, 0x14, alpha);
    int water_len = water * 0x34 / max_water;
    MGFillBox(CRect_i_((bar_x + 8) * 16, ((bar_y + 12) >> 1) * 16, 0x340, 0x20), 0x4F, 0x4F, 0x4F, alpha);
    MGFillBox(CRect_i_((bar_x + 8) * 16, ((bar_y + 12) >> 1) * 16, water_len * 16, 0x20), 0xC8, 0xEF, 0xF2, alpha);
}

void DngComStatus(int x, int y, int chara, int alpha) {
    CTexture *texture = TexManager.GetTexture("charastb", -1);
    int       left = x - 6;
    int       fill = 0;
    int       row_y = 0;
    float     unused_scale[2] = {0.0f, 0.0f};
    float     hp = BtlMenuStatusPt->hp[chara];
    float     max_hp = BtlMenuStatusPt->max_hp[chara];

    if (max_hp < 1.0f) {
        max_hp = 32.0f;
    }

    int          slot = BtlMenuStatusPt->equipped_weapon_slot[chara];
    WEAPON_HAVE *weapons = BtlMenuStatusPt->chara_weapons[chara];
    WEAPON_HAVE *weapon = &weapons[slot];

    if (weapon == nullptr) {
        return;
    }

    float whp = weapon->durability_f;
    float max_whp = weapon->durability;

    if (max_whp < 1.0f) {
        max_whp = 32.0f;
    }

    float values[2][2] = {
        {hp,  max_hp },
        {whp, max_whp}
    };
    float full[2] = {200.0f, 99.0f};
    int   unused_colour[6] = {64, 212, 162, 255, 128, 0};
    int   colour_no[2] = {22, 25};
    row_y = 0x50;

    if (y % 2 == 0) {
        y--;
    }

    for (int i = 0; i < 2; i++) {
        int length = static_cast<int>(148.0f * values[i][1] / full[i]);
        DrawMenu2DSprite(texture, CRect_i_(left + 0x10, y + 5, 4, 0xF), CRect_i_(0x10, 0xB0, 4, 0x10), alpha);
        DrawMenu2DSprite(texture, CRect_i_(left + 0x14, y + 5, length, 0xF), CRect_i_(0x14, 0xB0, 4, 0x10), alpha);
        DrawMenu2DSprite(texture, CRect_i_(left + 0x13 + length, y + 5, 4, 0xF), CRect_i_(0x18, 0xB0, 4, 0x10), alpha);
        fill = static_cast<int>(static_cast<float>(length) * values[i][0] / values[i][1]);

        if (i == 0) {
            if (hp < 0.3f * max_hp) {
                colour_no[i]++;
            }

            if (hp < 0.15f * max_hp) {
                colour_no[i]++;
            }
        }

        GRADATION_COLOR_INFO2 colours{};
        memcpy(&colours, GetGradationColorInfo2(colour_no[i]), sizeof(colours));
        colours.colors[3].a = alpha;
        colours.colors[2].a = alpha;
        colours.colors[1].a = alpha;
        colours.colors[0].a = alpha;
        DrawMenuColorGradation(Ps2Lvalue(CRect_i_(left + 0x14, y + 6, fill, 6)), &colours.colors[0], &colours.colors[1], &colours.colors[2], &colours.colors[3]);
        int number_x = left + 0x4C + length;

        while (x + 0x9A < number_x) {
            number_x--;
        }

        int numbers[2];
        numbers[0] = GetDispVolumeForFloat(values[i][0]);
        numbers[1] = GetDispVolumeForFloat(values[i][1]);
        DrawStatusNumberNowAndMax(numbers, number_x, y + 6, 1, alpha);
        y += 0x10;
        row_y += 0x10;
    }

    int drops = static_cast<int>(BtlMenuStatusPt->water_max[chara]) / 10;
    int drop_x = left + 0x12;

    for (int i = 0; i < drops - 1; i++) {
        DrawMenu2DSprite(texture, CRect_i_(drop_x, y + 5, 0x12, 0x13), CRect_i_(0x22, 0x9C, 0x12, 0x14), alpha);
        drop_x += 0x12;
    }

    DrawMenu2DSprite(texture, CRect_i_(drop_x, y + 5, 0x18, 0x14), CRect_i_(0x34, 0x9C, 0x18, 0x14), alpha);
    int water_alpha = (alpha * 0x50) >> 7;
    int full_drops = static_cast<int>(BtlMenuStatusPt->water_now[chara]) / 10;

    for (int i = 0; i < full_drops; i++) {
        DrawMenu2DSprite(texture, CRect_i_(left + 0x13 + i * 0x12, y, 0x10, 0x14), CRect_i_(0, 0x88, 0x10, 0x14), water_alpha);
    }

    int rest = static_cast<int>(BtlMenuStatusPt->water_now[chara]) % 10;

    if (rest != 0) {
        DrawMenu2DSprite(texture, CRect_i_(left + 0x13 + full_drops * 0x12, y, 0x10, 0x14), CRect_i_((3 - static_cast<int>(static_cast<float>(rest) / 2.5f)) * 16, 0x88, 0x10, 0x14), water_alpha);
    }
}

void DrawSelCharaStatus(float x, float y, int chara, int alpha, int face_size, int face_alpha, int face_x, int face_y) {
    int       u = 0;
    int       v = 0;
    int       px = static_cast<int>(x - 144.0f);
    float     top = 1.0f + y;
    int       py = static_cast<int>(top - 24.0f);
    CTexture *texture = TexManager.GetTexture("charastb", -1);

    DrawMenu2DSprite(texture, CRect_i_(px, py - 1, 0x100, 0x88), CRect_i_(0, 0, 0x100, 0x88), alpha);
    RECT digits = {0x40, 0x88, 0xC, 0xE};
    DrawMenuNumber(static_cast<int>(BtlMenuStatusPt->defense[chara]), px + 0x5E, py + 0x72, texture, digits, 1, alpha);

    px = static_cast<int>(x - 132.0f);
    py = static_cast<int>(10.0f + y);
    DngComStatus(px, py, chara, alpha);
    px = static_cast<int>(x - 110.0f);
    py = static_cast<int>(top - 18.0f);
    CTexture *fonts[4] = {BtlAlpha, BtlKata, BtlHira, BtlEuro};
    CharaSelectNameDraw2(px, py, static_cast<short *>(BtlMenuSaveDataPt->GetCharaName(chara)), fonts, alpha);
    px = static_cast<int>(x - 118.0f - 12.0f);
    py = static_cast<int>(70.0f + y);
    int          slot = BtlMenuStatusPt->equipped_weapon_slot[chara];
    WEAPON_HAVE *row = BtlMenuStatusPt->chara_weapons[chara];
    WEAPON_HAVE *weapon = &row[slot];

    if (weapon != nullptr) {
        CTexture *icon = RetCTex(weapon->item_no, u, v);

        if (icon != nullptr) {
            WEAPON_HAVE total;
            WeaponAllValueSet(weapon, &total, 0);
            DrawMenu2DSprite(icon, CRect_i_(px + 2, py + 2, 0x20, 0x20), CRect_i_(u, v, 0x20, 0x20), alpha);
            DrawMenuNumber(total.attack, px + 0x20, py + 0x14, texture, digits, 1, alpha);
        }
    }

    px = static_cast<int>(x - 34.0f);
    py = static_cast<int>(64.0f + y);
    px = static_cast<int>(x - 40.0f);
    CharaStatusMsgDraw(px, static_cast<int>(68.0f + y), chara, 1, alpha);
}

namespace {

/**
 * Draws the battle menu's Atora selection screen.
 */
void DrawBtlAtoraSelect() {
    DrawMenuAtoraSelect();
}

/**
 * Draws the battle menu options and restores unfiltered rendering.
 */
void BtlDrawOption() {
    DrawMenuOption();
    setbilinear(0);
}

/**
 * Draws the saving notice over the menu and restores unfiltered rendering.
 */
void BtlDrawSave() {
    DrawMenuSave("frame_image");
    setbilinear(0);
}

/**
 * Looks the battle menu's fixed textures up in the texture manager and keeps them.
 */
void BtlMenuTexBlockEnter() {
    MenuCharaFace = TexManager.GetTexture("charaface", BtlMenuReadBlock);
    BtStatus = TexManager.GetTexture("btstatus2", BtlMenuReadBlock);
    WepIcon = TexManager.GetTexture("wepicon", -1);
    ItemIcon = TexManager.GetTexture("itemicon", BtlMenuReadBlock);
    PerBoardTex = TexManager.GetTexture("perbrd", BtlMenuReadBlock);
    WepStatus = TexManager.GetTexture("wepstatus", BtlMenuExtBlock);
    VillageName = TexManager.GetTexture("vilname", -1);
    VillageBar = TexManager.GetTexture("viltag", -1);
}

} // namespace

void BattleMenuTexEnter() {
    LOADTEXTURE_INFO2 textures[6] = {
        {"#frame_image#640#480#4",   0, 0},
        {"#dbgwork_menu#256#256#3",  0, 0},
        {nullptr,                    0, 0},
        {"#frame_image_2#640#480#4", 0, 0},
        {nullptr,                    0, 0},
        {nullptr,                    0, 0}
    };

    textures[0].block_no = BtlMenuReadBlock;
    textures[1].block_no = BtlMenuReadBlock;
    textures[2].block_no = BtlMenuReadBlock;
    textures[3].block_no = BtlMenuExtBlock;
    textures[4].block_no = BtlMenuExtBlock;
    BG_READ_INFO *file = GetReadBGFile(0);
    textures[2].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "btlmenu.img", nullptr));
    textures[4].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "btlmenu2.img", nullptr));
    int blocks[3] = {0, 0, -1};
    blocks[0] = BtlMenuReadBlock;
    blocks[1] = BtlMenuExtBlock;
    MenuTextureDelete(blocks);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, textures);
    BtlMenuTexBlockEnter();
    GetAtraMsgReadBuf = reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "atrames.bin", nullptr));
    auto *mes = reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "allmenu.mes", nullptr));
    InitMenuMesSet(MENU_MES_SET_ALLMENU, mes);
    CommonMenuMes2.stay_frame = false;
    BtlMenuReadEndFlag = 1;
    MenuMes.SetBuffInfo(mes);
}

void ExitBattleMenu(int unused) {
    int count = 0;

    for (int i = 0; i < MenuItemPackPt->num; i++) {
        s16 &item = MenuItemPackPt->item[i];

        if (item >= ITEM_DUNGEON_START) {
            count++;
        } else {
            item = -1;
        }
    }

    for (int i = 0; i < 3; i++) {
        s16 &item = MenuItemPackPt->quick_item_slot[i];

        if (item >= ITEM_DUNGEON_START) {
            count += MenuItemPackPt->quick_item_qty[i];
        } else {
            item = -1;
            MenuItemPackPt->quick_item_qty[i] = 0;
        }
    }

    MenuItemPackPt->item_count = count;
    int blocks[7] = {0, 0, 0, 0, 0, 0, -1};
    blocks[0] = BtlMenuReadBlock;
    blocks[1] = MenuExtendReadBlock;
    blocks[2] = BtlMenuExReadBlock;
    blocks[3] = BtlMenuManualReadBlock;
    blocks[4] = MenuShadowReadBlock;
    blocks[5] = BtlMenuExtBlock;
    MenuTextureDelete(blocks);
    TexManager.CleanUpBuffer();
    TexManager.CleanUpTextureList();
    ItemVolumeStep.Initialize();
    MenuWepLevelUp.CheckSnd();

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON: {
            LOADTEXTURE_INFO2 textures[2] = {
                {"#frame_image0#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
                {nullptr,                                     0, 0}
            };
            textures[0].block_no = BtlMenuReadBlock;
            TexManager.LoadTextureBlock(-1, textures);
            break;
        }
        default:
            break;
    }

    SndSetBgmVol(BtlMenuBGMvol);
    InitReadBG();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

void BattleMenuInit(int *texture_blocks, int mode) {
    BtlMenuMode = mode;
    BtlMenuReadBlock = texture_blocks[0];
    MenuExtendReadBlock = texture_blocks[1];
    BtlMenuExReadBlock = texture_blocks[2];
    BtlMenuManualReadBlock = texture_blocks[3];
    ItemMenuWeaponIconReadBlock = texture_blocks[3];
    MenuShadowReadBlock = texture_blocks[4];
    BtlMenuExtBlock = texture_blocks[5];
    BtlMenuNowLang = GetMenuLangFlag();
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 5);
    GamePad.MenuModeOn(0x78);

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON: {
            LOADTEXTURE_INFO2 texture[2] = {
                {"#frame_image#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
                {nullptr,                                    0, 0}
            };
            texture[0].block_no = BtlMenuReadBlock;
            int blocks[7] = {0, 0, 0, 0, 0, 0, -1};
            blocks[0] = BtlMenuReadBlock;
            blocks[1] = MenuExtendReadBlock;
            blocks[2] = BtlMenuExReadBlock;
            blocks[3] = BtlMenuManualReadBlock;
            blocks[4] = MenuShadowReadBlock;
            blocks[5] = BtlMenuExtBlock;
            MenuTextureDelete(blocks);
            TexManager.CleanUpBuffer();
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, texture);
            break;
        }
        default:
            break;
    }

    BtlMenuReadBuf = BtlMenuBufferSet(BtlMenuMode);
    BtlMenuReadBuf = MenuCalcBufAlignment(BtlMenuReadBuf);
    StartReadBG();
    int size = LoadFileBGMenuData("dungeon/dunmenu5.pak", BtlMenuReadBuf);
    ReadBG();
    BtlMenuReadBuf += (size >> 4) + 1;
    BtlMenuReadBuf = MenuCalcBufAlignment(BtlMenuReadBuf);
    BtlMenuExReadFlag = 1;
    BtlMenuReadEndFlag = 0;
    MenuExTextureReadFlag = MENU_EX_TEX_UNREAD;
    MenuMes.InitMes();
    MenuMes.work_area = BtlMenuReadBuf;
    printf("msg work area\n");
    BtlMenuReadBuf += 0x11;
    BtlMenuReadBuf = MenuCalcBufAlignment(BtlMenuReadBuf);
    StayTex = TexManager.GetTexture("stayframe", -1);
    WepIcon = nullptr;
    ItemIcon = nullptr;
    ItemIcon2 = nullptr;
    AttachIcon = nullptr;
    BtlMenuSaveDataPt = SaveData;

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON:
            MenuMapJumpMode = 0;
            BtlMenuStatusPt = static_cast<CDngStatusData *>(UserStatus);
            SetEscapeDngFlag(EscapeDungeonMode());
            break;
        case BATTLE_MENU_MODE_TOWN:
            MenuMapJumpMode = -1;
            BtlMenuStatusPt = SaveData->GetDngStatus();
            SetEscapeDngFlag(ESCAPE_DNG_PAY);
            break;
        default:
            break;
    }

    SetInteriorOutFlag(0);
    DngWepHavePt = BtlMenuStatusPt->chara_weapons[0];
    MenuItemPackPt = &BtlMenuStatusPt->item_pack;
    MenuChara.slot_count = 6;
    PosAngle = TWO_PI / MenuChara.slot_count;

    // Debug builds hand the controlled character a place in the party along with a weapon.
    if (DebugMode && BtlMenuStatusPt->cur_chara > BtlMenuStatusPt->party_size - 1) {
        BtlMenuStatusPt->party_size = BtlMenuStatusPt->cur_chara + 1;
        BtlMenuStatusPt->GetItem(GetDefaultWeaponNo(BtlMenuStatusPt->party_size - 1) + 1, 0);
        EquipDefaultWeapon(BtlMenuStatusPt->party_size - 1);
        printf("Debug Party Member Get\n");
    }

    ItemVolumeStep.CheckItemVolume();
    CursorVibeCnt = 0;
    BtlHelpWinAlpha = 0;
    GetMainMenuRightHelpWinLangOffset(HelpWinHead[0], HelpWinHead[1], BtlHelpWinW, BtlHelpWinH);
    BtlWakuMake2 = 1;
    MenuSelect[0] = BTLMENU_PAGE_ITEM;
    MenuSelect[1] = 0;

    for (int i = 0; i < 8; i++) {
        NorMenuIcon[i].x = -150.0f;
        NorMenuIcon[i].y = i * 0x28 + 0x30;
    }

    if (CheckItemThrow(nullptr, nullptr) != 0) {
        BattleMenuFlag = BTLMENU_STATE_ITEM_TRUSH;
        BtlEffectFlag = BTLEFFECT_NONE;
        BtlMenuStatusPt->overflow_flag = true;

        for (int i = 1; i < 8; i++) {
            NorMenuIcon[i].x = -230.0f;
        }

        NorMenuIcon[0].x = 70.0f;
        NorMenuIcon[0].y = 28.0f;
    } else {
        BattleMenuFlag = BTLMENU_STATE_APPEAR;
        BtlEffectFlag = BTLEFFECT_NONE;
    }

    MenuChara.state = MENU_CHARA_OPEN;
    MenuWepLevelUp.Initialize();
    BtlEffectCt = 0;
    MenuWarningMsgFlag = 0;
    BtlMenuBGMvol = SndGetBgmVol();
}

namespace {

/**
 * Suppresses a draw flag while an Atla event or a character page is showing.
 */
int BtlMenuDrawSpecialFlag(int flag) {
    if ((BattleMenuFlag == BTLMENU_STATE_ATLA_OPEN || BattleMenuFlag == BTLMENU_STATE_ATLA || BattleMenuFlag == BTLMENU_STATE_ATLA_CLOSE) && GetMenuAtraEventFlag() != 0) {
        flag = 0;
    }

    if (MenuChara.state == MENU_CHARA_CHANGE_LOAD || MenuChara.state == MENU_CHARA_CHANGE_EXIT) {
        flag = 0;
    }

    return flag;
}

} // namespace

void BattleMenuDraw() {
    PortDrawBattleMenu();
}

int BattleMenuCursor() {
    int result = 0;

    ReadBG();
    static_cast<CUserStatus *>(BtlMenuStatusPt)->Step(1);
    ItemVolumeStep.LoopStep(0x3C);
    ItemVolumeStep.CheckItemVolume();
    result = 1;

    switch (BattleMenuFlag) {
        case BTLMENU_STATE_APPEAR:
            BattleMenuAppear();
            break;
        case BTLMENU_STATE_EXIT:
            result = BattleMenuExit();
            break;
        case BTLMENU_STATE_MAIN:
            result = BattleMenuSelect();
            break;
        case BTLMENU_STATE_WEAPON:
        case BTLMENU_STATE_WEAPON_OPEN:
        case BTLMENU_STATE_WEAPON_CLOSE:
            WeaponMenuSelect();
            break;
        case BTLMENU_STATE_ITEM_TRUSH:
            InitItemTrushStart();
            break;
        case BTLMENU_STATE_ITEM:
        case BTLMENU_STATE_ITEM_OPEN:
        case BTLMENU_STATE_ITEM_CLOSE:
            ItemMenuMainKey();
            break;
        case BTLMENU_STATE_CHARA:
        case BTLMENU_STATE_CHARA_OPEN:
        case BTLMENU_STATE_CHARA_CLOSE:
        case BTLMENU_STATE_UNK_1A:
            result = BattleMenuCharaKey();
            break;
        case BTLMENU_STATE_ATLA:
        case BTLMENU_STATE_ATLA_OPEN:
        case BTLMENU_STATE_ATLA_CLOSE:
            BattleMenuAtoraKey();
            break;
        case BTLMENU_STATE_MOVE:
        case BTLMENU_STATE_MOVE_OPEN:
        case BTLMENU_STATE_MOVE_CLOSE:
            result = MenuMoveKey();
            break;
        case BTLMENU_STATE_OPTION:
        case BTLMENU_STATE_OPTION_OPEN:
        case BTLMENU_STATE_OPTION_CLOSE:
            BattleMenuOptionKey();
            break;
        case BTLMENU_STATE_SAVE:
        case BTLMENU_STATE_SAVE_OPEN:
        case BTLMENU_STATE_SAVE_CLOSE:
            BattleMenuSaveKey();
            break;
        case BTLMENU_STATE_MANUAL:
        case BTLMENU_STATE_MANUAL_OPEN:
        case BTLMENU_STATE_MANUAL_CLOSE:
            BattleManualKey();
            break;
        default:
            break;
    }

    MenuWepLevelUp.StepSnd();

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON:
            SetEscapeDngFlag(EscapeDungeonMode());
            break;
        default:
            break;
    }

    return result;
}

namespace {

/**
 * Slides the bar icons into place and opens the menu once they have all arrived.
 */
void BattleMenuAppear() {
    int arrived = false;
    int i = 0;
    int position[2];

    for (i = 0; i < GetMenuModeMax(); i++) {
        GetMenuIconPos(i, position);
        float x = NorMenuIcon[i].x;
        float dx = position[0] - x;
        NorMenuIcon[i].x = x + dx / 4.0f;
        NorMenuIcon[i].y += (position[1] - NorMenuIcon[i].y) / 4.2f;

        if (dx < 3.4f) {
            NorMenuIcon[i].x = position[0];
            arrived++;
        }
    }

    BtlEffectCt += 1.0f;

    if (!(BtlEffectCt < 19.0f)) {
        BtlEffectCt = 19.0f;
    }

    int reading = ReadBGSync();

    if (arrived >= 6 && reading == 0) {
        BattleMenuTexEnter();
        BtlMenuReadEndFlag = 1;
        BtlEffectCt = 0.0f;
        BtlEffectFlag = BTLEFFECT_NONE;
        BattleMenuFlag = BTLMENU_STATE_MAIN;
        MenuSelect[0] = BTLMENU_PAGE_ITEM;
        MenuSelect[1] = 0;
        SysCur[0] = 64.0f;
        SysCur[1] = 54.0f;
    }
}

/**
 * Slides the bar icons off the screen and closes the menu once they are gone.
 */
int BattleMenuExit() {
    int i = 0;

    for (i = 0; i < GetMenuModeMax(); i++) {
        NorMenuIcon[i].x += (-198.0f - NorMenuIcon[i].x) / 4.0f;
    }

    BtlEffectCt += 1.0f;

    if (!(BtlEffectCt <= 20.0f) && ReadBGSync() == 0) {
        ExitBattleMenu(1);
        return 0;
    }

    return 1;
}

} // namespace

int BattleMenuSelect() {
    return PortBattleMenuSelect();
}

int ToFromSelect(int out) {
    if (out < 0 || out > 1) {
        return -1;
    }

    int home[8][2] = {
        {70,  28},
        {100, 48},
        {86,  38},
        {50,  40},
        {90,  40},
        {94,  30},
        {80,  40},
        {70,  28}
    };
    int   pos[2];
    float hide_x = -240.0f;

    if (BtlMenuNowLang > LANG_JAPANESE) {
        hide_x = -264.0f;
    }

    int done = 0;
    int arrived = false;

    for (int i = 0; i < GetMenuModeMax(); i++) {
        int axes = 0;

        if (out != 0) {
            GetMenuIconPos(i, pos);
        } else if (i != MenuSelect[1]) {
            pos[0] = static_cast<int>(hide_x);
            pos[1] = i * 0x28 + 0x48;
        } else {
            pos[0] = home[i][0];
            pos[1] = home[i][1];
        }

        float dx = static_cast<float>(pos[0]) - NorMenuIcon[i].x;
        float dy = static_cast<float>(pos[1]) - NorMenuIcon[i].y;
        NorMenuIcon[i].x += dx / 4.0f;
        NorMenuIcon[i].y += dy / 4.0f;

        if (static_cast<float>(abs(static_cast<int>(dx))) < 2.0f) {
            NorMenuIcon[i].x = pos[0];
            axes++;
        }

        if (static_cast<float>(abs(static_cast<int>(dy))) < 2.0f) {
            NorMenuIcon[i].y = pos[1];
            axes++;
        }

        if (axes >= 2) {
            arrived++;
        }
    }

    if (arrived >= 6) {
        done = 1;
    }

    return done;
}

namespace {

/**
 * Puts the menu cursor back on the icon of the mode the menu is returning to.
 */
void ForBackMenu() {
    int position[2];

    BtlEffectCt = 0.0f;
    BtlEffectFlag = BTLEFFECT_NONE;
    BtlWakuMake2 = 1;
    CommonMenuMes1.mes_made = -1;
    CommonMenuMes1.auto_pos = MES_POS_NONE;
    CommonMenuMes1.edge_alpha = 128;

    for (int i = 0; i < 10; i++) {
        CommonMenuMes1.mes_no[i] = -1;
        CommonMenuMes1.values[i] = -1;
    }

    MenuSelect[0] = BattleMenuFlag - BTLMENU_STATE_CLOSE_OFFSET;
    GetMenuIconPos(MenuSelect[1], position);
    SysCur[0] = position[0] - 40;
    SysCur[1] = position[1] + 20;
}

} // namespace

/**
 * Reads the character page's models and textures and starts its turntable.
 */
void Btl_InitMenuChara(u_long128 *buffer) {
    StartReadBG();
    LoadFileBGMenuData("charatex.img", buffer);
    ReadBG();
    BtlMenuExReadFlag = 0;
    MenuChara.state = MENU_CHARA_OPEN;
    MenuChara.timer = 0.0f;
    chara_r_long = 118.0f;
    BtlEffectCt = 0.0f;
    MenuCharaMove = 0.0f;
    SysCur[0] = 60.0f;
    SysCur[1] = 160.0f;
    CDngStatusData *status = BtlMenuStatusPt;
    int             chara = status->cur_chara;
    MenuChara.chara = chara;

    for (int i = 0; i < MenuChara.slot_count; i++) {
        SysChara[i].place = i - chara;
        SysChara[i].chara = i;
        SysChara[i].x = 0;
        SysChara[i].y = 0;

        if (SysChara[i].place < 0) {
            SysChara[i].place = MenuChara.slot_count + SysChara[i].place;
        }
    }

    int message_no = chara + 30;

    if (status->party_size - 1 < chara) {
        message_no = 39;
    }

    CommonMenuMes2.mes_made = -1;
    CommonMenuMes2.MakeMesWin(message_no);
}

int BattleMenuCharaKey() {
    switch (MenuChara.state) {
        case MENU_CHARA_CHANGE_LOAD:
            MenuChara.change_counter++;

            if (ReadBGSync() == 0 && MenuChara.change_counter > 20) {
                BtMenuLoadChara();
                BreakReadBG();
                StartReadBG();
                CharaChangeInitToGL2(1);
                MenuChara.state = MENU_CHARA_CHANGE_EXIT;
            }

            break;
        case MENU_CHARA_CHANGE_EXIT:
            MenuChara.change_counter++;

            if (ReadBGSync() == 0 && MenuChara.change_counter > 20) {
                BtMenuLoad2(1);
                LockOffTargte();
                ExitBattleMenu(1);
                return 0;
            }

            break;
        case MENU_CHARA_OPEN: {
            if (BtlMenuExReadFlag == 0) {
                if (ReadBGSync() == 0) {
                    LOADTEXTURE_INFO2 texture[3] = {
                        {"#frame_image_charamenu#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
                        {nullptr,                                              0, 0},
                        {nullptr,                                              0, 0}
                    };
                    texture[0].block_no = BtlMenuExReadBlock;
                    texture[1].block_no = BtlMenuExReadBlock;
                    BG_READ_INFO *file = GetReadBGFile(0);
                    texture[1].name = reinterpret_cast<char *>(file->buffer);
                    BtlMenuCharaChangeBuf = file->buffer + (file->size >> 4) + 1;
                    BtlMenuCharaChangeBuf = MenuCalcBufAlignment(BtlMenuCharaChangeBuf);
                    int blocks[2] = {0, -1};
                    blocks[0] = BtlMenuExReadBlock;
                    MenuTextureDelete(blocks);
                    TexManager.CleanUpTextureList();
                    TexManager.LoadTextureBlockEX(-1, texture);
                    BtlMenuTexBlockEnter();
                    MenuCharaFace = TexManager.GetTexture("charaface", BtlMenuExReadBlock);
                    BtStatus = TexManager.GetTexture("btstatus2", BtlMenuExReadBlock);
                    NonCharaFace = TexManager.GetTexture("nonchara", BtlMenuExReadBlock);
                    BtlAlpha = TexManager.GetTexture("alphabet", BtlMenuExReadBlock);
                    BtlHira = TexManager.GetTexture("hira", BtlMenuExReadBlock);
                    BtlKata = TexManager.GetTexture("kata", BtlMenuExReadBlock);
                    BtlEuro = TexManager.GetTexture("euro", BtlMenuExReadBlock);
                    BtlMenuExReadFlag = 1;
                } else {
                    return 1;
                }
            }

            int done = ToFromSelect(0);

            if (BtlMenuExReadFlag != 0) {
                MenuChara.timer += 1.0f;
            }

            if (!(MenuChara.timer < 20.0f)) {
                MenuChara.timer = 20.0f;
            }

            if (!(MenuChara.timer < 20.0f) && done == 1 && BtlMenuExReadFlag != 0) {
                MenuCharaMove = 0.0f;
                MenuChara.state = MENU_CHARA_SELECT;
                MenuChara.timer = 0.0f;
                BattleMenuFlag = BTLMENU_STATE_CHARA;
            }

            break;
        }
        case MENU_CHARA_CLOSE: {
            int done = ToFromSelect(1);
            MenuChara.timer += 1.0f;

            if (!(MenuChara.timer <= 20.0f) && done == 1) {
                ForBackMenu();
                int blocks[2] = {0, -1};
                blocks[0] = BtlMenuExReadBlock;
                MenuTextureDelete(blocks);
                TexManager.CleanUpTextureList();
                BtlMenuTexBlockEnter();
                MenuCharaFace = TexManager.GetTexture("charaface", BtlMenuReadBlock);
                BtStatus = TexManager.GetTexture("btstatus2", BtlMenuReadBlock);
                BattleMenuFlag = BTLMENU_STATE_MAIN;
            }

            break;
        }
        case MENU_CHARA_TURN:
            MenuChara.timer += 1.0f;

            if (!(MenuChara.timer < 21.0f)) {
                if (!(MenuCharaMove <= 0.0f)) {
                    for (int i = 0; i < MenuChara.slot_count; i++) {
                        SysChara[i].place++;

                        if (MenuChara.slot_count == SysChara[i].place) {
                            SysChara[i].place = 0;
                        }
                    }
                } else {
                    for (int i = 0; i < MenuChara.slot_count; i++) {
                        SysChara[i].place--;

                        if (SysChara[i].place == -1) {
                            SysChara[i].place = MenuChara.slot_count - 1;
                        }
                    }
                }

                for (auto &sys_chara : SysChara) {
                    if (sys_chara.place == 0) {
                        int chara = sys_chara.chara;
                        int mes_no = chara + 0x1E;

                        if (BtlMenuStatusPt->party_size - 1 < chara) {
                            mes_no = 0x27;
                        }

                        if (CommonMenuMes2.mes_made != mes_no) {
                            CommonMenuMes2.MakeMesWin(mes_no);
                        }
                    }
                }

                MenuChara.state = MENU_CHARA_SELECT;
                MenuChara.timer = 0.0f;
                MenuCharaMove = 0.0f;
            }

            break;
        case MENU_CHARA_MESSAGE:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                MenuChara.state = MENU_CHARA_SELECT;
                ComMenuSePlay(MENU_SOUND_CONFIRM);
            }

            break;
        case MENU_CHARA_SELECT: {
            int old = MenuChara.chara;

            if (GamePad.On(PAD_UP | PAD_RIGHT) != 0) {
                MenuCharaMove = PosAngle / 20.0f;
                MenuChara.state = MENU_CHARA_TURN;
                MenuChara.timer = 0.0f;
                MenuChara.chara--;

                if (MenuChara.chara < 0) {
                    MenuChara.chara = 5;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            } else if (GamePad.On(PAD_DOWN | PAD_LEFT) != 0) {
                MenuCharaMove = -PosAngle / 20.0f;
                MenuChara.state = MENU_CHARA_TURN;
                MenuChara.timer = 0.0f;
                MenuChara.chara++;

                if (MenuChara.chara > 5) {
                    MenuChara.chara = 0;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                BattleMenuFlag = BTLMENU_STATE_CHARA_CLOSE;
                MenuChara.state = MENU_CHARA_CLOSE;
                MenuChara.timer = 0.0f;
                BtlEffectFlag = BTLEFFECT_PAGE_CLOSE;
                BtlEffectCt = 0;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            } else if (GamePad.Down(PAD_CROSS) != 0) {
                switch (BtlMenuMode) {
                    case BATTLE_MENU_MODE_DUNGEON: {
                        int             i = 0;
                        CDngStatusData *status = BtlMenuStatusPt;
                        s8              cur = 0;
                        char           *active = &status->cur_chara;
                        cur = *active;

                        if (cur == CHARA_OSMOND && BtActStatus.ground_kind == 10) {
                            CommonMenuMes1.MakeMesWin(0x1A1);
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else {
                            for (i = 0; i < MenuChara.slot_count; i++) {
                                if (SysChara[i].place != 0) {
                                    continue;
                                }

                                int chara = SysChara[i].chara;

                                if (status->party_size - 1 < chara) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                    return 1;
                                }

                                s16 hp = status->hp[chara];
                                int condition = status->GetActiveCharaStatus(chara);

                                if (hp <= 0 || (condition & AILMENT_KNOCKED_OUT)) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                    return 1;
                                }

                                if (chara != cur) {
                                    *active = chara;
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                    MenuChara.state = MENU_CHARA_CHANGE_LOAD;
                                    MenuChara.change_counter = 0;
                                    return 1;
                                }
                            }
                        }

                        break;
                    }
                    default:
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                        break;
                }
            }

            if (old != MenuChara.chara && MenuChara.chara < BtlMenuStatusPt->party_size && BtlMenuCharaChangeBuf != nullptr && BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) {
                BreakReadBG();
                CharaChangeInitToGL(BtlMenuCharaChangeBuf, MenuChara.chara);
            }

            // Debug builds raise the front party member's maximum life and add or remove party members.
            if (DebugMode) {
                if (GamePad.On(PAD_R2 | PAD_R1) != 0) {
                    for (int i = 0; i < MenuChara.slot_count; i++) {
                        if (SysChara[i].place == 0) {
                            int raised = BtlMenuStatusPt->max_hp[i] + 1;

                            if (raised < 0x80) {
                                BtlMenuStatusPt->max_hp[i] = raised;
                            }
                        }
                    }
                }

                if (GamePad.Down2(PAD_CROSS) != 0 && BtlMenuStatusPt->party_size < 6) {
                    BtlMenuStatusPt->party_size++;
                    BtlMenuStatusPt->GetItem(GetDefaultWeaponNo(BtlMenuStatusPt->party_size - 1) + 1, 0);
                    int chara = BtlMenuStatusPt->party_size - 1;
                    BtlMenuStatusPt->equipped_weapon_slot[chara] = 0;
                    EquipDefaultWeapon(BtlMenuStatusPt->party_size - 1);
                    NameDefaultSet(BtlMenuStatusPt->party_size - 1);
                }

                if (GamePad.Down2(PAD_CIRCLE) != 0 && BtlMenuStatusPt->party_size > 1) {
                    BtlMenuStatusPt->party_size--;
                }
            }

            break;
        }
        default:
            break;
    }

    return 1;
}

void DrawCharaSelect() {
    if (BtlMenuExReadFlag == 0) {
        return;
    }

    int bright = 0x80;
    int ring_alpha = 0x80;
    int status_alpha = 0;
    int face_alpha = 0;
    int panel_alpha = 0;

    switch (MenuChara.state) {
        case MENU_CHARA_OPEN:
            bright = static_cast<int>(10.0f * MenuChara.timer);
            ring_alpha = bright;
            break;
        case MENU_CHARA_CLOSE:
            bright = static_cast<int>(128.0f - 12.0f * MenuChara.timer);
            ring_alpha = bright;
            break;
        case MENU_CHARA_CHANGE_LOAD:
        case MENU_CHARA_CHANGE_EXIT:
            bright = 0x80 - MenuChara.change_counter * 3;
            ring_alpha = 0x80 - MenuChara.change_counter * 10;
            break;
        default:
            break;
    }

    ring_alpha = std::min(ring_alpha, 0x80);

    ring_alpha = std::max(ring_alpha, 0);

    bright = std::max(bright, 0);

    MenuTextureReload(BtlMenuExReadBlock);
    sceGsAlpha blend = mgAlpha;
    blend.bits.a = 0;
    blend.bits.b = 2;
    blend.bits.c = 0;
    blend.bits.d = 1;
    MGSetGsALPHA(&blend);
    DrawMenu2DSprite(TexManager.GetTexture("dgring", -1), CRect_i_(0x12A, 0x32, 0x118, 0x118), CRect_i_(0, 0, 0x80, 0x80), (ring_alpha * 0x8C) >> 7);
    MGSetGsALPHA(nullptr);

    switch (MenuChara.state) {
        case MENU_CHARA_OPEN:
            for (int i = 0; i < MenuChara.slot_count; i++) {
                float radius = chara_r_long / 20.0f * MenuChara.timer;
                float angle = PI + (PosAngle * (SysChara[i].place - 1.0f) + PosAngle / 20.0f * MenuChara.timer);
                SysChara[i].x = radius * cosf(angle);
                SysChara[i].y = radius * sinf(angle);
            }

            break;
        case MENU_CHARA_TURN:
            for (int i = 0; i < MenuChara.slot_count; i++) {
                float radius = chara_r_long;
                float angle = PI + (PosAngle * SysChara[i].place + MenuCharaMove * MenuChara.timer);
                SysChara[i].x = radius * cos(angle);
                SysChara[i].y = radius * sin(angle);
            }

            break;
        case MENU_CHARA_CHANGE_EXIT:
        case MENU_CHARA_CHANGE_LOAD:
            chara_r_long += MenuChara.change_counter;

            for (int i = 0; i < MenuChara.slot_count; i++) {
                float radius = chara_r_long;
                float angle = PI + (PosAngle * SysChara[i].place + MenuCharaMove * MenuChara.change_counter);
                SysChara[i].x = radius * cos(angle);
                SysChara[i].y = radius * sin(angle);
            }

            break;
        default:
            break;
    }

    float size = 0.0f;
    float draw_x = 0.0f;
    float move = 0.0f;
    float face_x = 0.0f;
    float time = 0.0f;
    float grow = 0.0f;
    float face_y = 0.0f;

    for (int i = 0; i < MenuChara.slot_count; i++) {
        face_x = 394.0f + SysChara[i].x;
        draw_x = face_x;
        face_y = 120.0f + SysChara[i].y;
        size = 90.0f;
        face_alpha = bright;
        status_alpha = bright;
        panel_alpha = 0;

        move = MenuCharaMove;

        if ((!(move <= 0.0f) && MenuChara.slot_count - 1 == SysChara[i].place) || (move < 0.0f && SysChara[i].place == 1)) {
            float step = 0.6666667f * MenuChara.timer;
            draw_x = face_x - step;
            size += step;
            panel_alpha = static_cast<int>(128.0f * MenuChara.timer / 21.0f);
            status_alpha = 0x80 - panel_alpha;
        }

        if (SysChara[i].place == 0) {
            time = MenuChara.timer;
            float scale = 0.6666667f;
            draw_x = face_x + scale * (time - 20.0f - 1.0f);
            grow = scale * time;
            size = 106.0f - grow;
            status_alpha = static_cast<int>(static_cast<float>(bright) * time / 21.0f);

            if (move == 0.0f) {
                face_alpha = 0x80;
                panel_alpha = 0x80;
            } else if (MenuChara.state == MENU_CHARA_TURN) {
                panel_alpha = static_cast<int>(128.0f - 10.0f * time);
            }

            switch (MenuChara.state) {
                case MENU_CHARA_CHANGE_EXIT:
                case MENU_CHARA_CHANGE_LOAD:
                case MENU_CHARA_OPEN:
                case MENU_CHARA_CLOSE:
                    size += grow;
                    draw_x -= grow;
                    status_alpha = 0;
                    face_alpha = bright;
                    panel_alpha = bright;
                    break;
                default:
                    break;
            }
        }

        if (i < BtlMenuStatusPt->party_size) {
            float box_x = 23.0f + face_x;
            float box_y = 86.0f + face_y;

            status_alpha = std::min(status_alpha, 0x80);

            status_alpha = std::max(status_alpha, 0);

            DrawOtherCharaStatus(static_cast<int>(box_x), static_cast<int>(box_y), i, status_alpha);
            box_x = 394.0f + SysChara[i].x - 29.0f;
            box_y = 120.0f + SysChara[i].y - 3.0f;

            panel_alpha = std::min(panel_alpha, 0x80);

            panel_alpha = std::max(panel_alpha, 0);

            if (box_x < -300.0f || static_cast<float>(0x28A) < box_x || face_y < -130.0f || 1280.0f < face_y) {
                continue;
            }

            MenuTextureReload(BtlMenuExReadBlock);
            DrawSelCharaStatus(box_x, box_y, i, panel_alpha, static_cast<int>(size), face_alpha, static_cast<int>(draw_x), static_cast<int>(face_y));
        }

        MenuTextureReload(BtlMenuExReadBlock);
        CTexture *face = MenuCharaFace;
        int       u = 0;
        int       v = 0;
        int       width = 0x6A;
        v = i * 0x6A;

        if (i > 2) {
            u = 0x6A;
            v = (i - 3) * 0x6A;
        }

        int shade = 0x80;
        s16 hp = BtlMenuStatusPt->hp[i];
        int condition = BtlMenuStatusPt->GetActiveCharaStatus(i);

        if (hp <= 0 || (condition & AILMENT_KNOCKED_OUT)) {
            shade = 0x50;
        }

        if (BtlMenuStatusPt->party_size <= i) {
            face = NonCharaFace;

            if (SysChara[i].place == 0) {
                face = TexManager.GetTexture("nonchara2", -1);
                v = 0xC;
                u = 0xC;
                width = 0x6A;
            } else {
                v = 0x13;
                u = 0x13;
                width = 0x5A;
                size -= 1.0f;
            }

            shade = 0x80;
        }

        face_alpha = std::min(face_alpha, 0x80);

        face_alpha = std::max(face_alpha, 0);

        if (MenuChara.state == MENU_CHARA_CHANGE_EXIT || MenuChara.state == MENU_CHARA_CHANGE_LOAD) {
            face_alpha = 0x80 - MenuChara.change_counter * 2;

            face_alpha = std::max(face_alpha, 0);
        }

        if (0.0f < draw_x && draw_x < 640.0f && 0.0f < face_y && face_y < SCREEN_HEIGHT_F) {
            DrawMenu2DSprite(face, CRect_i_(static_cast<int>(draw_x), static_cast<int>(face_y), static_cast<int>(size), static_cast<int>(size)), CRect_i_(u, v, width, width), shade, shade, shade, face_alpha);
        }
    }

    CommonMenuMes1.auto_pos = MES_POS_NONE;
    CommonMenuMes1.stay_frame = false;

    switch (MenuChara.state) {
        case MENU_CHARA_MESSAGE:
            AllFadeForMenu(0x40);
            CommonMenuMes1.auto_pos = MES_POS_CENTRE;
            CommonMenuMes1.stay_frame = true;
            CommonMenuMes1.Step();
            CommonMenuMes1.DrawMesWin();
            break;
        case MENU_CHARA_OPEN:
        case MENU_CHARA_CLOSE:
        case MENU_CHARA_CHANGE_LOAD:
        case MENU_CHARA_CHANGE_EXIT:
            break;
        default: {
            float angle = PI;
            float waku_x = 394.0f + chara_r_long * cosf(angle) - 184.0f;
            face_y = 120.0f + chara_r_long * sinf(angle) - 26.0f;
            int width = 0x102;
            int cursor_x = 0x3C;
            int i = 0;

            for (i = 0; i < 6; i++) {
                if (SysChara[i].place == 0) {
                    break;
                }
            }

            if (BtlMenuStatusPt->party_size <= SysChara[i].chara) {
                cursor_x = 0xA0;
                waku_x = static_cast<float>(0x18A + chara_r_long * cos(angle) - 30.0);
                width = 0x6E;
            }

            SysCur[0] += (static_cast<float>(cursor_x) - SysCur[0]) / 4.0f;
            int draw_x = static_cast<int>(SysCur[0]);
            DrawMenuWaku(waku_x, face_y, width, 0x82, 1, StayTex, 0x80);
            DrawMenuObjectVibe(draw_x, static_cast<int>(SysCur[1]), 1, 0x40);
            break;
        }
    }
}

void DrawWepDamageDraw(RECT rect, WEAPON_HAVE *weapon, int alpha) {
    int x = rect.x;
    int max_exp = 0;
    int exp = 0;
    int y = rect.y;
    int durability = weapon->durability;
    int now = GetDispVolumeForFloat(weapon->durability_f);
    max_exp = GetWeaponMaxExp(weapon);
    exp = weapon->experience;

    exp = std::min(max_exp, exp);

    exp = std::max(exp, 0);

    if (max_exp <= 0) {
        max_exp = 99;
    }

    int exp_len = exp * 99 / max_exp;
    DrawMenu2DSprite(WepStatus, CRect_i_(rect.x, y, durability, 8), CRect_i_(0x100, 0x70, 8, 8), alpha);
    static int warningcnt = 0;
    warningcnt++;

    if (weapon->durability_f <= 0.1f * weapon->durability || max_exp == exp) {
        abs(static_cast<int>(60.0f * sinf(static_cast<float>(warningcnt) / 18.0f)));
        warningcnt++;

        if (warningcnt > 1000000) {
            warningcnt = 0;
        }
    }

    MGFillBox(CRect_i_(rect.x * 16, ((y - 1) >> 1) * 16, (durability + 1) * 16, 0x50), 0x4F, 0x4F, 0x4F, alpha);
    GRADATION_COLOR_INFO2 colours{};
    memcpy(&colours, GetGradationColorInfo2(25), sizeof(colours));
    colours.colors[3].a = alpha;
    colours.colors[2].a = alpha;
    colours.colors[1].a = alpha;
    colours.colors[0].a = alpha;
    DrawMenuColorGradation(Ps2Lvalue(CRect_i_(rect.x, y, now, 8)), &colours.colors[0], &colours.colors[1], &colours.colors[2], &colours.colors[3]);
    y += rect.height;
    RECT digits = {0xD4, 0x1F4, 0xC, 0xD};
    x += rect.width;
    DrawMenuNumber(durability, x, y, WepStatus, digits, 1, alpha);
    DrawMenu2DSprite(WepStatus, CRect_i_(x - 0x20, y, 0xC, 0xB), CRect_i_(digits.x + 0x78, digits.y, 0xC, 0xC), alpha);
    DrawMenuNumber(now, x - 0x20, y, WepStatus, digits, 1, alpha);
    rect.y += 0x14;
    y = rect.y;
    DrawMenu2DSprite(WepStatus, CRect_i_(rect.x, y, exp_len, 8), CRect_i_(0x100, 0x78, 8, 8), alpha);
    int digits_count = GetNumberKeta(max_exp);
    x = rect.x;
    y += rect.height;
    x += digits_count * 0x18;
    DrawMenuNumber(max_exp, x, y, WepStatus, digits, 1, alpha);
    x -= digits_count * 0x10;
    DrawMenu2DSprite(WepStatus, CRect_i_(x, y, 0xC, 0xB), CRect_i_(digits.x + 0x78, digits.y, 0xC, 0xC), alpha);
    DrawMenuNumber(exp, x, y, WepStatus, digits, 1, alpha);
}

void DrawWepStatus(int x, int y, WEAPON_HAVE *weapon, int selected, int alpha) {
    int v = 0;

    if (selected != 0) {
        v += 0x34;
    }

    DrawMenu2DSprite(WepStatus, CRect_i_(x, y + 1, 0xAC, 0x33), CRect_i_(0xD4, v, 0xAC, 0x34), alpha);
    RECT rect = {0, 0, 0x30, -7};
    rect.x = x + 0x3E;
    rect.y = y + 0xF;
    DrawWepDamageDraw(rect, weapon, alpha);
}

void DrawWepVolumeDisplay(int x, int y, WEAPON_HAVE *weapon, int alpha) {
    WEAPON_DATA *data = GetWeaponData(weapon->item_no);

    if (data == nullptr) {
        return;
    }

    WEAPON_HAVE total;
    WeaponAllValueSet(weapon, &total, 0);
    CRect_i_ source(0x110, 0xCC, 0x10, 0x10);
    CRect_i_ dest(x, y, source.width, source.height);
    RECT     digits = {0xD4, 0x1F4, 0xC, 0xD};

    if (weapon->best_elem < 5) {
        // The face sheet lives in the main block and WepStatus in the extension block; each is made resident before use.
        MenuTextureReload(BtlMenuReadBlock);
        DrawMenu2DSprite(MenuCharaFace, CRect_i_(x, y - 0x10, 0x20, 0x10), CRect_i_(0xE0, weapon->best_elem * 16, 0x20, 0x10), alpha);
        MenuTextureReload(BtlMenuExtBlock);
    }

    int values[4] = {0, 0, 0, 0};
    values[0] = total.attack;
    values[1] = total.endurance;
    values[2] = total.speed;
    values[3] = total.magic;
    float rate = GetNowWeaponRate(&total);
    int   limit[4] = {data->attack_max, 99, 99, data->magic_max};
    limit[0] = static_cast<int>(static_cast<float>(limit[0]) * rate);
    limit[3] = static_cast<int>(static_cast<float>(limit[3]) * rate);

    for (int i = 0; i < 4; i++, dest.y += source.height, source.y += source.height) {
        values[i] = std::min(limit[i], values[i]);

        DrawMenu2DSprite(WepStatus, dest, source, alpha);
        DrawMenuNumber(values[i], dest.x + source.width + digits.width * 3 - 2, dest.y + 2, WepStatus, digits, 1, alpha);
    }
}

namespace {

/**
 * Draws the board a weapon's name sits on.
 */
void DrawWeaponNameBoard(int x, int y, int width, int alpha, int brightness) {
    CRect_i_ left(x, y + 1, 0x28, 0x1F);
    CRect_i_ middle(x + 0x28, y + 1, 0x99, 0x1F);
    CRect_i_ right(x + 0xAC, y + 1, 0x20, 0x1F);

    DrawMenu2DSprite(WepStatus, left, CRect_i_(0xE0, 0x1A8, 0x28, 0x20), brightness, brightness, brightness, alpha);
    DrawMenu2DSprite(WepStatus, middle, CRect_i_(0x108, 0x1A8, 0x78, 0x20), brightness, brightness, brightness, alpha);
    DrawMenu2DSprite(WepStatus, right, CRect_i_(0xD4, 0x1D4, 0x20, 0x20), brightness, brightness, brightness, alpha);
}

} // namespace

s32 GetWeaponNamePutX(s32 center_x, s32 width) {
    return center_x - (width >> 1);
}

namespace {

/**
 * Places one line of a message window, ignoring lines past the window's ten.
 */
void SetLinePos(ClsMes *window, int line, int x, int y) {
    if (line >= 0 && line < 10) {
        window->line_pos[line].x = x;
        window->line_pos[line].y = y;
    }
}

/**
 * Draws the names of the weapons either side of the cursor as they slide past.
 *
 * Weapons off screen, or hidden by the page's current mode, get the blank message. The window
 * is laid out again whenever a name or level changed, and the name frame is drawn under it while
 * the page is at its top level.
 */
void WeaponNameDraw(int y, int selected_only, int alpha) {
    int previous_mes[4];
    int previous_level[4];

    MenuTextureReload(AtoraNameMes.tex_block);
    int slot = WepMenu.weapon_slot - 1;

    for (int i = 0; i < 4; i++) {
        previous_mes[i] = AtoraNameMes.mes_no[i];
        previous_level[i] = AtoraNameMes.values[i];
    }

    int x = 22.0f + WeaponPos + slot * 0xD6;
    AtoraNameMes.value_signed = true;

    for (int i = 0; i < 3; i++) {
        int mes_no = 0;
        int level = 0;

        if (x < -140 || x > 630 || (selected_only == 1 && slot != WepMenu.weapon_slot) || slot < 0 || slot > 9 || ((WepMenu.state == WEP_STATE_STATUS_BREAK_EFFECT || WepMenu.state == WEP_STATE_BUILDUP_EFFECT) && slot == WepMenu.weapon_slot) || (WepMenu.mode > WEP_MENU_LIST && slot != WepMenu.weapon_slot) || (WepMenu.state != WEP_STATE_REPAIR_EFFECT && WepMenu.state >= WEP_STATE_STATUS_BREAK_EFFECT && WepMenu.state < WEP_STATE_WARNING) || (WepMenu.state == WEP_STATE_LEVELUP_EFFECT && slot != WepMenu.weapon_slot)) {
            mes_no = 999;
        } else {
            mes_no = GetWeaponMsgNo(&DngWepHavePt[slot]);

            if (mes_no < 0 || slot < 0) {
                mes_no = 999;
            }

            if (mes_no > 100) {
                AtoraNameMes.GetMesWidth_system(mes_no);
                level = DngWepHavePt[slot].level;
            }
        }

        int width = AtoraNameMes.GetMesWidth_system(mes_no);
        AtoraNameMes.mes_no[i] = mes_no;
        AtoraNameMes.values[i] = level;
        int name_x = GetWeaponNamePutX(x + 0x42, width);

        if (level > 0) {
            name_x -= 8;

            if (level / 10 > 0) {
                name_x -= 8;
            }
        }

        SetLinePos(&AtoraNameMes, i, name_x, y);
        x += 0xD6;
        slot++;
    }

    for (int i = 0; i < 4; i++) {
        if (AtoraNameMes.line_pos[i].y > 180) {
            AtoraNameMes.line_pos[i].y = y;
            AtoraNameMes.mes_no[i] = 999;
        }
    }

    for (int i = 0; i < 4; i++) {
        if (AtoraNameMes.mes_no[i] != previous_mes[i] || AtoraNameMes.values[i] != previous_level[i]) {
            AtoraNameMes.mes_made = -1;
            AtoraNameMes.MakeMesWin(150);
            break;
        }
    }

    AtoraNameMes.edge_alpha = alpha;
    AtoraNameMes.Step();
    AtoraNameMes.DrawMesWin();

    if (WepMenu.mode == WEP_MENU_LIST && WepMenu.state == WEP_STATE_IDLE) {
        CTexture *frame = TexManager.GetTexture("frame_image", -1);

        if (frame != nullptr) {
            TexManager.ReloadTexture(GetVif1Packet(), frame->block);
            (reinterpret_cast<sceGsTex0 *>(&frame->tex0))->bits.tcc = 0;
            sceGsTexa texa = mgTexa;
            texa.AEM = 1;
            texa.TA0 = 0x80;
            MGSetGsTEXA(&texa);
            CRect_i_ rect(0, 0x14A, 0x104, 0x76);
            set2DSprite(GetVif1Packet(), frame, rect, rect, 0x40, 0x40, 0x40, alpha);
            MGSetGsTEXA(nullptr);
        }
    }
}

} // namespace

RECT WeaponVolumeNumberRect = {212, 500, 12, 13};

namespace {

/**
 * Draws one weapon-status bar, filled to the value's share of its maximum.
 */
void WepStatusVolumeDraw(RECT rect, int width, int *volume, int color, int alpha, int build, int gain) {
    int y = rect.y;
    int fill = static_cast<float>(volume[1] * width) / static_cast<float>(volume[0]);

    fill = std::min(fill, width);

    GRADATION_COLOR_INFO2 gradation{};
    memcpy(&gradation, GetGradationColorInfo2(color), sizeof(gradation));
    gradation.colors[3].a = alpha;
    gradation.colors[2].a = alpha;
    gradation.colors[1].a = alpha;
    gradation.colors[0].a = alpha;
    CRect_i_ bar(rect.x, y, fill, 8);
    DrawMenuColorGradation(bar, &gradation.colors[0], &gradation.colors[1], &gradation.colors[2], &gradation.colors[3]);
    fill = static_cast<float>(volume[2] * width) / static_cast<float>(volume[0]);

    fill = std::min(fill, width);

    int number_alpha = 1.5f * alpha;

    if (build == 1 && gain > 0) {
        DrawMenuNumber(volume[1], rect.x + rect.width, y + rect.height, WeaponVolumeNumberRect, WepStatus, 2, 0xDC, 0, 0, alpha);
    } else {
        DrawMenuNumber(volume[1], rect.x + rect.width, y + rect.height, WepStatus, WeaponVolumeNumberRect, 2, alpha);
    }
}

} // namespace

void DrawWeaponStatusWaku(int x, int y, int width, int limit) {
    spRGBA colours[2][2] = {
        {{0x3B, 0x68, 0x56, 0x80}, {0x3B, 0x68, 0x56, 0x80}},
        {{0x00, 0x00, 0xE6, 0x80}, {0x00, 0x00, 0xE6, 0x80}}
    };
    CRect_i_ rect(x - 1, y - 1, width + 2, 10);
    DrawMenuSideGradation(rect, &colours[limit][0], &colours[limit][1]);
}

namespace {

/**
 * Draws the mark that says a weapon value has reached its ceiling.
 */
void DrawLimmitMax(int x, int y, int alpha) {
    DrawMenu2DSprite(WepStatus, CRect_i_(x, y, 0x1E, 0xA), CRect_i_(0xF8, 0x158, 0x1E, 0xA), alpha);
}

} // namespace

void DrawBtlMenuLRCursor(int x, int y, int width, int alpha) {
    int      count = CursorVibeCnt % 79;
    double   pi = PI;
    int      draw_y = static_cast<int>(static_cast<float>(y) + 4.0f * sinf(static_cast<float>(pi * (static_cast<double>(count) - 40.0) / 40.0)));
    CRect_i_ source(0x62, 0x14, 0x1A, 0x18);
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x, draw_y, source.width, source.height), source, alpha);
    source.x += source.width;
    DrawMenu2DSprite(PerBoardTex, CRect_i_(x + width, draw_y, source.width, source.height), source, alpha);
}

void DrawWeaponStatusTag(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha) {
    WEAPON_DATA *built = nullptr;

    if (build == 1) {
        built = GetWeaponData(weapon_no);
    }

    RECT pos = {0, 0, 0x1E, -8};
    pos.x = x + 0x4B;
    pos.y = y + 0x30;
    s16   gauge[2] = {0, 0};
    int   number_x = 0;
    int   bar_y = 0;
    float now = weapon->durability_f;
    gauge[0] = static_cast<int>(now);
    int value = weapon->durability;
    gauge[1] = value;
    number_x = pos.x;
    bar_y = pos.y;
    int shown = static_cast<int>(now);

    if (static_cast<float>(shown) < now) {
        shown = static_cast<int>(1.0f + now);
    }

    int full = value * 0x7E / 99;
    int fill = shown * full / value;

    fill = std::min(fill, 0x7E);

    MGFillBox(CRect_i_(pos.x * 16, (bar_y >> 1) * 16, full * 16, 0x40), 0x4F, 0x4F, 0x4F, alpha);
    GRADATION_COLOR_INFO2 colours{};
    memcpy(&colours, GetGradationColorInfo2(25), sizeof(colours));
    colours.colors[3].a = alpha;
    colours.colors[2].a = alpha;
    colours.colors[1].a = alpha;
    colours.colors[0].a = alpha;
    DrawMenuColorGradation(Ps2Lvalue(CRect_i_(pos.x, bar_y, fill, 8)), &colours.colors[0], &colours.colors[1], &colours.colors[2], &colours.colors[3]);
    bar_y += pos.height;

    for (int digits = GetNumberKeta(value) - 1; digits > 0; digits--) {
        int width = pos.width;
        number_x += width;
    }

    DrawMenuNumber(weapon->durability, number_x, bar_y, WepStatus, WeaponVolumeNumberRect, 1, alpha);
    DrawMenu2DSprite(WepStatus, CRect_i_(number_x - 0x20, bar_y, 0xC, 0xB), CRect_i_(WeaponVolumeNumberRect.x + 0x78, WeaponVolumeNumberRect.y, 0xC, 0xC), alpha);
    DrawMenuNumber(shown, number_x - 0x20, bar_y, WepStatus, WeaponVolumeNumberRect, 1, alpha);
    pos.y += 0x18;
    number_x = pos.x;
    bar_y = pos.y;
    int max_exp = GetWeaponMaxExp(weapon);
    int exp_len = (weapon->experience << 7) / max_exp;

    exp_len = std::min(exp_len, 0x7E);

    DrawMenu2DSprite(WepStatus, CRect_i_(pos.x, bar_y, exp_len, 8), CRect_i_(0x100, 0x78, 8, 8), alpha);
    bar_y += pos.height;

    for (int digits = GetNumberKeta(max_exp); digits >= 2; digits--) {
        number_x += WeaponVolumeNumberRect.width * 2;
    }

    int left = DrawMenuNumber(max_exp, number_x, bar_y, WepStatus, WeaponVolumeNumberRect, 1, alpha);
    DrawMenu2DSprite(WepStatus, CRect_i_(left - 0xB, bar_y, 0xC, 0xB), CRect_i_(WeaponVolumeNumberRect.x + 0x78, WeaponVolumeNumberRect.y, 0xC, 0xC), alpha);
    DrawMenuNumber(weapon->experience, left - 0xB, bar_y, WepStatus, WeaponVolumeNumberRect, 2, alpha);
    int values[4] = {0, 0, 0, 0};
    values[0] = weapon->attack;
    values[1] = weapon->endurance;
    values[2] = weapon->speed;
    values[3] = weapon->magic;
    int bonus[4] = {0, 0, 0, 0};
    int target[4] = {0, 0, 0, 0};

    if (build == 1 && built != nullptr) {
        target[0] = built->attack;
        target[1] = built->endurance;
        target[2] = built->speed;
        target[3] = built->magic;
    }

    WEAPON_DATA *data = GetWeaponData(weapon->item_no);
    int          limit[4] = {data->attack_max, 99, 99, data->magic_max};

    if (build == 0) {
        for (int i = 0; i < 4; i++) {
            bonus[i] = GetWeaponAttachStatusUp(weapon, i + 1);

            bonus[i] = std::max(bonus[i], 0);
        }
    }

    float rate = GetNowWeaponRate(weapon);

    if (build == 1) {
        rate = 1.0f;
    }

    if (!(rate <= 1.0f)) {
        limit[0] = static_cast<int>(static_cast<float>(limit[0]) * rate);
        limit[3] = static_cast<int>(static_cast<float>(limit[3]) * rate);
    }

    RECT row = {0, 0, -4, -2};
    row.x = x + 0x4A;
    row.y = y + 0x60;
    int volume[3] = {0, 0, 0};
    volume[0] = 100;

    for (int i = 0; i < 4; i++) {
        int limited = 0;
        value = values[i];
        volume[1] = value + bonus[i];
        volume[1] = static_cast<int>(static_cast<float>(volume[1]) * rate);

        if (volume[1] >= limit[i]) {
            volume[1] = limit[i];
            limited = 1;
        }

        volume[2] = value;

        volume[2] = std::min(volume[2], limit[i]);

        int length = static_cast<int>(static_cast<float>(volume[1] << 7) / static_cast<float>(volume[0]));

        length = std::min(length, 0x80);

        int diff = target[i] - value;
        DrawWeaponStatusWaku(row.x, row.y, length, limited);
        WepStatusVolumeDraw(row, 0x80, volume, i + 3, alpha, build, diff);

        if (limited != 0) {
            DrawLimmitMax(row.x + 4, row.y + 3, 0x80);
        }

        row.y += 0x10;
    }
}

void DrawWeaponElemTag(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha) {
    WEAPON_DATA *built = nullptr;

    if (build == 1) {
        built = GetWeaponData(weapon_no);
    }

    int best = weapon->best_elem;

    if (best < 5) {
        int src_y = best * 0x18 + 0x68;
        int tag_y = y + 0x24 + best * 0x18;
        DrawMenu2DSprite(WepStatus, CRect_i_(x + 6, tag_y + 1, 0x2C, 0x18), CRect_i_(0xD4, src_y, 0x2C, 0x18), alpha);
    }

    RECT row = {0, 0, -4, -4};
    row.x = x + 0x4A;
    row.y = y + 0x30;
    float rate = GetNowWeaponRate(weapon);
    int   bonus[5] = {0, 0, 0, 0, 0};
    int   target[5] = {0, 0, 0, 0, 0};

    if (build == 0) {
        for (int i = 0; i < 5; i++) {
            bonus[i] = GetWeaponAttachStatusUp(weapon, i + 8);
        }
    }

    if (build == 1 && built != nullptr) {
        for (int i = 0; i < 5; i++) {
            target[i] = built->elem[i];
        }
    }

    int volume[3] = {0, 0, 0};
    volume[0] = 100;

    for (int i = 0; i < 5; i++) {
        int limited = 0;
        volume[1] = weapon->elem[i] + bonus[i];
        volume[1] = static_cast<int>(static_cast<float>(volume[1]) * rate);

        if (volume[1] >= 99) {
            volume[1] = 99;
            limited = 1;
        }

        volume[2] = weapon->elem[i];

        volume[2] = std::min(volume[2], 99);

        float length = static_cast<float>(volume[1] << 7) / static_cast<float>(volume[0]);
        int   width = static_cast<int>(length);
        DrawWeaponStatusWaku(row.x, row.y, static_cast<int>(length), limited);
        int diff = target[i] - weapon->elem[i];
        WepStatusVolumeDraw(row, 0x80, volume, i + 7, alpha, build, diff);

        if (limited != 0) {
            DrawLimmitMax(row.x + 2, row.y + 4, 0x80);
        }

        row.y += 0x18;
    }

    if (best < 5) {
        int mark_y = y + 0x30 + best * 0x18;
        DrawMenu2DSprite(WepStatus, CRect_i_(x + 0x8A, mark_y, 0x42, 0x15), CRect_i_(0xD4, 0x190, 0x42, 0x16), (alpha * 0x6E) >> 7);
    }
}

void DrawWeaponVsMonster(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha) {
    WEAPON_DATA *built = nullptr;

    if (build == 1) {
        built = GetWeaponData(weapon_no);
    }

    int  top = y + 0x30;
    RECT row = {0, 0, 0x10, -8};
    row.x = x + 0x22;
    row.y = top;
    int volume[3] = {100, 0, 0};
    int bonus[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int target[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if (build == 0) {
        for (int i = 0; i < 10; i++) {
            bonus[i] = GetWeaponAttachStatusUp(weapon, i + 14);
        }
    }

    if (build == 1) {
        for (int i = 0; i < 10; i++) {
            target[i] = built->vs_monster[i];
        }
    }

    float rate = GetNowWeaponRate(weapon);

    for (int i = 0; i < 10; i++) {
        int limited = 0;
        volume[1] = weapon->vs_monster[i] + bonus[i];
        volume[1] = static_cast<int>(static_cast<float>(volume[1]) * rate);

        if (volume[1] >= 99) {
            limited = 1;
            volume[1] = 99;
        }

        volume[2] = weapon->vs_monster[i];

        volume[2] = std::min(volume[2], 99);

        float length = static_cast<float>(volume[1] << 6) / static_cast<float>(volume[0]);
        int   width = static_cast<int>(length);
        DrawWeaponStatusWaku(row.x, row.y, static_cast<int>(length), limited);
        int diff = target[i] - weapon->vs_monster[i];
        WepStatusVolumeDraw(row, 0x40, volume, i + 12, alpha, build, diff);

        if (limited != 0) {
            DrawLimmitMax(row.x + 0x1E, row.y + 4, 0x80);
        }

        row.y += 0x18;

        if (i == 4) {
            row.x = x + 0x8A;
            row.y = top;
        }
    }
}

namespace {

/**
 * Draws the three tag pages of a weapon and highlights the one the cursor is on.
 */
void DrawWeaponTagBoard(int x, int y, WEAPON_HAVE *weapon, int build, int weapon_no, int alpha) {
    int page = WepMenu.tag_page;
    int tag = 0;
    int i = 0;
    int step = 1;
    int wrapped = 0;

    for (; i < 3; i++) {
        int brightness = 128;

        if (page == tag) {
            if (wrapped != 0) {
                tag = page;
            } else {
                tag = 2;
                step = -1;
                wrapped = 1;
            }
        }

        if (WepMenu.mode == WEP_MENU_ELEMENT && tag != WEP_TAG_ELEMENT) {
            brightness = 64;
        }

        int top = tag * 0xAA;
        DrawMenu2DSprite(WepStatus, CRect_i_(x, y, 0xD4, 0xAA), CRect_i_(0, top, 0xD4, 0xAA), brightness, brightness, brightness, alpha);
        tag += step;
    }

    if (weapon != nullptr) {
        switch (page) {
            case WEP_TAG_STATUS:
                DrawWeaponStatusTag(x, y, weapon, build, weapon_no, alpha);
                break;
            case WEP_TAG_ELEMENT:
                DrawWeaponElemTag(x, y, weapon, build, weapon_no, alpha);
                break;
            case WEP_TAG_VS_MONSTER:
                DrawWeaponVsMonster(x, y, weapon, build, weapon_no, alpha);
                break;
            default:
                break;
        }
    }
}

} // namespace

void DrawAallWeapon(int x, int y, float depth, CCharacter *model,
                    WEAPON_HAVE *weapon, int equipped, int selected, int alpha) {
    PortDrawAallWeapon(x, y, depth, model, weapon, equipped, selected, alpha);
}

void BtlWeaponDraw(int x, float depth, int chara, int alpha) {
    PortBtlWeaponDraw(x, depth, chara, alpha);
}

namespace {

/**
 * Reports whether a weapon can be repaired, built up, or neither, as a set of flags.
 */
int NowWeaponStatusValue(WEAPON_HAVE *weapon) {
    if (weapon == nullptr) {
        return 0;
    }

    int status = 0;

    if (weapon->experience >= GetWeaponMaxExp(weapon) || GetNowItemNum(ITEM_POWERUP_POWDER, MenuItemPackPt) > 0) {
        status |= 2;
    }

    if (WeaponStatusBreakEnable(weapon) != 0) {
        status |= 4;
    }

    status |= 8;
    int build_up = 0;

    if (WeaponStatusBuildUp(weapon, build_up) > 0 && GetNowWeaponAttachNum(weapon) == 0 && build_up > 0) {
        status |= 0x10;
    }

    if (IsNotBuildUpWeapon(weapon->item_no) != 0) {
        if (status & 8) {
            status &= ~8;
        }

        if (status & 0x10) {
            status &= ~0x10;
        }
    }

    return status;
}

/**
 * Returns the restriction on selecting no element for a weapon.
 */
int EnableWeaponElemNone(int weapon_no) {
    if (WhoIsWeaponEquip(weapon_no) == CHARA_RUBY) {
        return 1;
    }

    if (static_cast<unsigned int>(weapon_no - ITEM_WEAPON_BLESSING_GUN) <= 1U || weapon_no == ITEM_WEAPON_HEXA_BLASTER || weapon_no == ITEM_WEAPON_SUPERNOVA) {
        return 2;
    }

    return 0;
}

/**
 * Drops a weapon's active element where the attachment no longer supports it.
 */
void WeaponMenuCheckElemValue(WEAPON_HAVE *weapon, WEAPON_HAVE *attachment) {
    if (0 <= weapon->best_elem && weapon->best_elem < 5 && attachment->elem[weapon->best_elem] <= 0) {
        weapon->best_elem = 5;
    }

    if (weapon->best_elem == 5 && EnableWeaponElemNone(weapon->item_no) != 0) {
        SetWeaponElementStatus(weapon);
    }

    printf("now active elem = %d\n", weapon->best_elem);
    printf("now active vol  = %d\n", weapon->elem[weapon->best_elem]);
}

/**
 * Reports whether an element may be put on a weapon, and complains where it may not.
 */
int WeaponMenuCheckEnableSetElem(WEAPON_HAVE *weapon, WEAPON_HAVE *attachment, int elem) {
    int refused = 0;

    if (weapon->best_elem == elem) {
        if (EnableWeaponElemNone(weapon->item_no) != 0) {
            WepMenu.state = WEP_STATE_WARNING;
            WepMenu.warning_no = 9;
            refused = 1;
        } else {
            weapon->best_elem = 5;
        }
    } else if (attachment->elem[elem] > 0) {
        weapon->best_elem = elem;
    } else {
        WepMenu.state = WEP_STATE_WARNING;
        WepMenu.warning_no = 0;
        refused = 1;
    }

    if (weapon != nullptr) {
        printf("now elem = %d\n", weapon->best_elem);

        if (weapon->best_elem != 5) {
            printf("now elem vol = %d\n", attachment->elem[weapon->best_elem]);
        }
    }

    return refused;
}

} // namespace

void DrawWeaponSelectDialog(int x, int y, int alpha) {
    PortDrawWeaponSelectDialog(x, y, alpha);
}

/**
 * Opens the weapon page in one of its modes and puts the cursor where it was left.
 */
void Btl_InitWeaponSelect(int mode, int chara) {
    InitPersonalBoardMode(static_cast<CUserStatus *>(BtlMenuStatusPt), &WepMenu.board, PERSONAL_BOARD_WEAPON_ATTACH, BOARD_PAGE_ATTACH);
    WepMenu.open_mode = mode;
    WepMenu.mode = WEP_MENU_LIST;
    WepMenu.chara = chara;

    switch (mode) {
        case WEP_OPEN_FROM_BAR:
            WepMenu.state = WEP_STATE_OPEN;
            BtlHaveItemPt = &WepMenu.board.held_item;
            break;
        case WEP_OPEN_IN_PLACE:
            WepMenu.state = WEP_STATE_IDLE;
            break;
        case WEP_OPEN_VIEW_ONLY:
        default:
            break;
    }

    WepMenu.counter = 0;
    StartReadWepMDS(BtlMenuReadBuf, chara);
    MenuExTextureReadFlag = MENU_EX_TEX_UNREAD;
    CDngStatusData *status = BtlMenuStatusPt;
    DngWepHavePt = status->chara_weapons[chara];

    for (int i = 0; i < 10; i++) {
        if (DngWepHavePt[i].item_no < ITEM_WEAPON_START) {
            memset(&DngWepHavePt[i], 0, sizeof(WEAPON_HAVE));
            DngWepHavePt[i].item_no = -1;
        }
    }

    WepMenu.weapon_slot = BtlMenuStatusPt->equipped_weapon_slot[chara];

    if (WepMenu.weapon_slot < 0 || WepMenu.weapon_slot >= 10) {
        WepMenu.weapon_slot = 0;
    }

    SysCur[0] = 235.0f;
    SysCur[1] = 165.0f;
    WepMenu.chara = chara;

    for (auto &sys_chara : SysChara) {
        sys_chara.x = (sys_chara.chara - WepMenu.chara) * 110 + 270;
        sys_chara.y = 14.0f;
    }

    WeaponPos = 0xE8 - WepMenu.weapon_slot * 0xD6;
    WepPolyPos = 2.0f + 16.0f * -WepMenu.weapon_slot;
    WepFrameRate = PI;
    WepMenu.warning_no = -1;
    MenuWepLevelUp.buildup_complete = 0;
    CommonMenuMes3.Preset(MES_PRESET_SYSTEM);
    CommonMenuMes3.char_width = GetMenuCommonFontW(BtlMenuNowLang, -1);
    CommonMenuMes3.style = MES_EDGE_NONE;
    AtoraNameMes.value_show = false;
    AtoraNameMes.value_narrow = false;
    AtoraNameMes.char_width = GetMenuCommonFontW(BtlMenuNowLang, -1);
    AtoraNameMes.style = MES_EDGE_NONE;
    AtoraNameMes.narrow_gaiji = true;
    memset(AtoraNameMes.tex_buff, 0, 0x100);

    for (int i = 0; i < 10; i++) {
        AtoraNameMes.mes_no[i] = -1;
        AtoraNameMes.values[i] = 0;
    }

    AtoraNameMes.columns = 16;
    AtoraNameMes.Preset(MES_PRESET_NAME);
    AtoraNameMes.mes_made = -1;
    MenuMes.InitData();
    MenuMes.message->unk_02C = 0x10;
    MenuMes.message->init_030 = 0x10;
}

namespace {

/**
 * Rebuilds the player's equipped weapon model and gives the menu's textures back.
 */
void ExitWeaponMenuSelect() {
    int chara = BtlMenuStatusPt->cur_chara;

    if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && chara == WepMenu.chara) {
        DngWeaponEquipModelBuild(chara, MenuExtendReadBlock, BtlMenuReadBuf);
    }

    TexManager.DeleteTextureBlock(MenuExtendReadBlock);
    TexManager.CleanUpTextureList();
    BtlMenuTexBlockEnter();
    BattleMenuFlag = BTLMENU_STATE_WEAPON_CLOSE;
    CommonMenuMes3.text_columns = MES_WIN_COLUMNS;
    CommonMenuMes3.text_rows = 10;
    CommonMenuMes3.text_len = 0;
    CommonMenuMes3.text_width = 0;
    CommonMenuMes3.text_height = 0;
    CommonMenuMes3.fade = 0.0f;
    CommonMenuMes3.fade_in = true;
    CommonMenuMes3.text_rate = CommonMenuMes3.text_rate_set;
    CommonMenuMes3.waiting = false;
    CommonMenuMes3.text_at = 0.0f;
    CommonMenuMes3.text_no = 0;
    CommonMenuMes3.text_from = 0;
    CommonMenuMes3.page_from = 0;
    CommonMenuMes3.InitMesWinTbl();
    CommonMenuMes3.clut_now = CommonMenuMes3.clut_default;
    CommonMenuMes3.wait = 0;
    CommonMenuMes3.blink = 0;
    CommonMenuMes3.auto_page_wait = 0;
    CommonMenuMes3.mes_made = -1;
    CommonMenuMes3.edge_alpha = 128;

    for (int &message : CommonMenuMes3.mes_no) {
        message = -1;
    }

    for (int &value : CommonMenuMes3.values) {
        value = 0;
    }

    CommonMenuMes3.value = 0;
    CommonMenuMes3.value_signed = false;
    CommonMenuMes3.value_show = true;
    CommonMenuMes3.value_narrow = false;
    CommonMenuMes3.space_width = -1;
    CommonMenuMes3.space_area = -1;
    CommonMenuMes3.cursor_row = -1;
    CommonMenuMes3.cursor_y = 0;
    CommonMenuMes3.cursor_lit = false;

    for (auto &position : CommonMenuMes3.line_pos) {
        position.x = -1;
        position.y = -1;
    }

    CommonMenuMes3.Preset(MES_PRESET_SYSTEM);
    CommonMenuMes3.char_width = GetMenuCommonFontW(BtlMenuNowLang, -1);
    CommonMenuMes3.char_height = 22;
    CommonMenuMes3.value_narrow = false;
    AtoraNameMes.value_show = true;
    AtoraNameMes.value_narrow = false;
    AtoraNameMes.narrow_gaiji = false;
    AtoraNameMes.char_width = GetMenuCommonFontW(BtlMenuNowLang, -1);
    CommonMenuMes1.value_narrow = false;
    ForBackMenu();
    BattleMenuFlag = BTLMENU_STATE_MAIN;
}

} // namespace

void WeaponMenuSelect() {
    int direction = -1;

    switch (WepMenu.state) {
        case WEP_STATE_OPEN:
            direction = 0;
            break;
        case WEP_STATE_CLOSE:
            direction = 1;
            break;
        default:
            break;
    }

    int done = ToFromSelect(direction);

    switch (MenuExTextureReadFlag) {
        case MENU_EX_TEX_UNREAD:
            if (MenuWepLevelUp.buildup_complete == 0) {
                if (ReadBGSync() == 0) {
                    if (MenuExTextureReadFlag != MENU_EX_TEX_UNREAD) {
                        break;
                    }

                    MenuExTextureReadFlag = EnterWeaponModel(WepMenu.chara, MenuExtendReadBlock, WepMenu.weapon_slot);
                    BtlMenuTexBlockEnter();
                    BtlMDSBuildCnt = 0;
                    WepMenu.counter = 0;
                } else {
                    return;
                }
            }

            if (MenuWepLevelUp.buildup_complete == 1) {
                WeaponModelBuildFunc(WepMenu.chara, MenuExtendReadBlock);
                BtlMDSBuildCnt = 0;
                MenuWepLevelUp.buildup_complete = 0;
                MenuExTextureReadFlag++;
            }

            break;
        case MENU_EX_TEX_BUILDING:
            BtlMDSBuildCnt++;

            if (BtlMDSBuildCnt > 3) {
                MenuExTextureReadFlag = MENU_EX_TEX_READY;
            }

            break;
        default:
            break;
    }

    if (WepMenu.state != WEP_STATE_IDLE) {
        WepMenu.counter++;
    } else {
        WepMenu.counter = 0;
    }

    switch (WepMenu.state) {
        case WEP_STATE_OPEN:
            if (!(static_cast<float>(WepMenu.counter) <= 18.0f) && done != 0 && MenuExTextureReadFlag != MENU_EX_TEX_UNREAD) {
                BattleMenuFlag = BTLMENU_STATE_WEAPON;
                WepMenu.state = WEP_STATE_IDLE;
                WepMenu.counter = 0;
            }

            break;
        case WEP_STATE_CLOSE:
            if (WepMenu.counter > 20 && done != 0) {
                ExitWeaponMenuSelect();
            }

            break;
        case WEP_STATE_LEVELUP_EFFECT:
        case WEP_STATE_STATUS_BREAK_EFFECT:
        case WEP_STATE_BUILDUP_EFFECT:
        case WEP_STATE_REPAIR_EFFECT:
            if (MenuWepLevelUp.effect_active == 0 && GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0 && (MenuWepLevelUp.effect_state == WEP_EFFECT_STATE_BREAK_DONE || MenuWepLevelUp.effect_state == WEP_EFFECT_STATE_LEVELUP_DONE || MenuWepLevelUp.effect_state == WEP_EFFECT_STATE_BUILDUP_DONE || MenuWepLevelUp.effect_state == WEP_EFFECT_STATE_RECOVER_DONE)) {
                WEAPON_HAVE *weapon = MenuWepLevelUp.weapon;

                switch (WepMenu.state) {
                    case WEP_STATE_LEVELUP_EFFECT:
                        MenuWepLevelUp.SetLevelUpWeaponData();
                        weapon->level++;
                        weapon->synthesis_count += MenuWepLevelUp.synthesis_count;

                        for (auto &attachment : weapon->attach) {
                            memset(&attachment, 0, sizeof(attachment));
                        }

                        break;
                    case WEP_STATE_STATUS_BREAK_EFFECT: {
                        int equipped = BtlMenuStatusPt->GetEquipWeaponSlot(WepMenu.chara);

                        if (WepMenu.weapon_slot == equipped || DngWepHavePt[equipped].item_no < ITEM_WEAPON_START) {
                            EquipDefaultWeapon(WepMenu.chara);
                        }

                        MenuWepLevelUp.lost_default_weapon = false;
                        break;
                    }
                    case WEP_STATE_BUILDUP_EFFECT:
                    case WEP_STATE_REPAIR_EFFECT:
                    default:
                        break;
                }

                if (WepMenu.state != WEP_STATE_REPAIR_EFFECT) {
                    MenuWepLevelUp.SetSnd(MenuWepLevelUp.snd_volume, MenuWepLevelUp.snd_from, 7);
                }

                MenuWepLevelUp.Initialize();
                WepMenu.state = WEP_STATE_IDLE;
            }

            break;
        case WEP_STATE_WARNING:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                WepMenu.state = WEP_STATE_IDLE;
                WepMenu.warning_no = -1;
            }

            break;
        case WEP_STATE_SCRAP_REFUSED:
            if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                WepMenu.state = WEP_STATE_IDLE;
            }

            break;
        case WEP_STATE_SCRAP_PROMPT:
            if (GamePad.Down(PAD_CROSS) != 0) {
                WEAPON_HAVE *weapon = GetNowSelectWeapon();

                if (0 <= IsDefaultWeapon(weapon->item_no)) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    WepMenu.state = WEP_STATE_SCRAP_REFUSED;
                } else if (weapon->item_no == ITEM_WEAPON_SERPENT_SWORD && GetMenuHebikiriFlag() == 0) {
                    WepMenu.state = WEP_STATE_SCRAP_REFUSED;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else {
                    WepMenu.state = WEP_STATE_SCRAP_CONFIRM;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }
            } else if (GamePad.Down(PAD_CIRCLE | PAD_LEFT) != 0) {
                WepMenu.state = WEP_STATE_IDLE;
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (WepMenu.state == WEP_STATE_IDLE) {
                WepMenu.board.cursor = 0;
            }

            break;
        case WEP_STATE_SCRAP_CONFIRM:
            if (GamePad.Down(PAD_UP | PAD_DOWN) != 0) {
                if (WepMenu.confirm[0] != 0) {
                    WepMenu.confirm[0] = 0;
                } else {
                    WepMenu.confirm[0] = 1;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                if (WepMenu.confirm[0] == 0) {
                    WEAPON_HAVE *weapon = GetNowSelectWeapon();

                    if (0 <= IsDefaultWeapon(weapon->item_no)) {
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                    } else {
                        InitHaveWep(weapon);
                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                        WepMenu.state = WEP_STATE_IDLE;

                        if (WepMenu.weapon_slot == BtlMenuStatusPt->GetEquipWeaponSlot(WepMenu.chara)) {
                            EquipDefaultWeapon(WepMenu.chara);
                        }

                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                    }
                } else {
                    WepMenu.state = WEP_STATE_IDLE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            } else if (GamePad.Down(PAD_CIRCLE | PAD_LEFT) != 0) {
                WepMenu.state = WEP_STATE_IDLE;
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (WepMenu.state == WEP_STATE_IDLE) {
                WepMenu.board.cursor = 0;
                WepMenu.mode = WEP_MENU_LIST;
            }

            break;
        case WEP_STATE_EQUIP:
            if (!(WepFrameRate < PI)) {
                WepMenu.state = WEP_STATE_IDLE;
            }
        case WEP_STATE_CHARA_CHANGE:
            if (WepMenu.counter >= 26) {
                WepMenu.state = WEP_STATE_IDLE;
            }
        case WEP_STATE_IDLE: {
            int slot = 0;
            int cursor = 0;
            int chara = 0;
            int elem = 0;
            int page = 0;
            slot = WepMenu.weapon_slot;
            cursor = WepMenu.board.cursor;
            page = WepMenu.mode;
            chara = WepMenu.chara;
            elem = WepMenu.element;

            switch (page) {
                case WEP_MENU_LIST:
                case WEP_MENU_ACTION:
                case WEP_MENU_ELEMENT:
                case WEP_MENU_LEVELUP_CONFIRM:
                case WEP_MENU_STATUS_BREAK_CONFIRM:
                case WEP_MENU_BUILDUP_SELECT:
                case WEP_MENU_BUILDUP_CONFIRM:
                case WEP_MENU_BUILDUP_PREVIEW:
                    if (GetMenuCharaEffectReadFlag() != 0) {
                        if (ReadBGSync() == 0) {
                            SetMenuCharaEffectReadFlag(0);
                        }
                    } else {
                        WeaponSelectKey();
                    }

                    break;
                case WEP_MENU_ATTACH_WEAPON:
                case WEP_MENU_ATTACH_SOCKETS:
                case WEP_MENU_ATTACH_BOARD:
                case WEP_MENU_ATTACH_TAGS:
                    WeaponMenuAttachModeKey();
                    break;
                default:
                    break;
            }

            if (slot != WepMenu.weapon_slot || (page == WepMenu.mode && cursor != WepMenu.board.cursor) || chara != WepMenu.chara || elem != WepMenu.element) {
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            break;
        }
        default:
            break;
    }

    int              help_no = 0;
    int              mes_no = 0;
    int              name_mes = -1;
    int              name_no = -0x1F3;
    int              value = 0;
    WEP_BUILDUP_INFO builds[4];
    auto            *attachments = reinterpret_cast<ATTACH_LIST *>(static_cast<CUserStatus *>(BtlMenuStatusPt)->consumable_items);
    WEAPON_HAVE     *weapon = &DngWepHavePt[WepMenu.weapon_slot];

    if (weapon != nullptr) {
        name_no = GetWeaponMsgNo2(weapon->item_no);
    }

    s16 page_mes[12] = {0, 0, 0, 0, 0, 0, 0, 0xAC, 0x73, 0, 0, 0};
    page_mes[1] = WepMenu.board.cursor + 0x68;
    page_mes[2] = WepMenu.element + 0x82;
    int name_msg = name_no + 500;
    page_mes[3] = name_msg;
    page_mes[4] = name_msg;
    page_mes[5] = name_msg;
    page_mes[6] = name_msg;
    page_mes[11] = WepMenu.tag_row + ((WepMenu.tag_page + 2) * 10 + 100);
    mes_no = page_mes[WepMenu.mode];

    if (WepMenu.mode >= WEP_MENU_LEVELUP_CONFIRM && WepMenu.mode < WEP_MENU_ATTACH_WEAPON) {
        s16 dialog[5] = {0x74, 0x76, 0x75, 0x77, 0x75};
        help_no = dialog[WepMenu.mode - WEP_MENU_LEVELUP_CONFIRM];

        if (MenuWepLevelUp.operation_kind != WEP_EFFECT_NONE) {
            help_no = CommonMenuMes1.mes_made;
        }
    }

    switch (WepMenu.mode) {
        case WEP_MENU_ACTION:
            switch (WepMenu.state) {
                case WEP_STATE_SCRAP_PROMPT:
                case WEP_STATE_SCRAP_CONFIRM:
                case WEP_STATE_SCRAP_REFUSED:
                    mes_no = 0xA4;
                    break;
                default:
                    break;
            }

            break;
        case WEP_MENU_BUILDUP_SELECT:
        case WEP_MENU_BUILDUP_PREVIEW: {
            EnableBuildUpModel(builds, weapon);
            int i = 0;

            while (builds[i].weapon_no != -1) {
                COM_ITEM_INFO *info = GetCommonItemInfo(builds[i].weapon_no);

                if (info != nullptr) {
                    builds[i].weapon_no = info->msg;

                    if (builds[i].enabled == 0) {
                        builds[i].weapon_no = 2;
                    }
                }

                i++;

                if (i > 4) {
                    break;
                }
            }

            if (WepMenu.mode == WEP_MENU_BUILDUP_SELECT) {
                s16 enabled = builds[WepMenu.board.cursor].enabled;

                if (enabled != 0) {
                    mes_no = 0xAE;
                    name_mes = builds[WepMenu.board.cursor].weapon_no;
                }

                if (enabled == 0) {
                    mes_no = 0xAD;
                }
            }

            break;
        }
        case WEP_MENU_LIST: {
            int item_no = weapon->item_no;

            if (item_no >= ITEM_WEAPON_START) {
                mes_no = GetWeaponMsgNo2(item_no) + 500;
                value = weapon->level;
            }

            break;
        }
        case WEP_MENU_ATTACH_SOCKETS: {
            int item_no = weapon->attach[WepMenu.board.cursor].item_no;

            if (item_no >= ITEM_ATTACH_START) {
                mes_no = item_no + 500;
                value = GetAttachVolumeForMsg(&weapon->attach[WepMenu.board.cursor]);

                if (item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                    name_mes = GetWeaponMsgNo2(weapon->attach[WepMenu.board.cursor].sphere_weapon_no);
                }
            } else {
                WEAPON_DATA *data = GetWeaponData(weapon->item_no);

                if (data->hole[WepMenu.board.cursor] == 2) {
                    mes_no = 0x70;
                } else {
                    mes_no = 0x71;
                }
            }

            break;
        }
        case WEP_MENU_ELEMENT:
            if (weapon->best_elem == WepMenu.element) {
                mes_no += 0x15;
            }

            break;
        case WEP_MENU_ATTACH_TAGS:
            if (WepMenu.tag_page == WEP_TAG_ELEMENT && weapon->best_elem == WepMenu.tag_row) {
                mes_no += 0x15;
            }

            break;
        case WEP_MENU_ATTACH_BOARD:
            switch (WepMenu.board.cursor_area) {
                case PERSONAL_BOARD_AREA_CELLS: {
                    int held = BtlHaveItemPt->item_no;

                    if (held >= ITEM_ATTACH_START) {
                        mes_no = held + 500;
                        value = GetAttachVolumeForMsg(&WepMenu.board.held_attach);

                        if (BtlHaveItemPt->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                            name_mes = GetWeaponMsgNo2(WepMenu.board.held_attach.sphere_weapon_no);
                        }
                    } else {
                        ATTACH_LIST *attach = &attachments[WepMenu.board.cursor];
                        int          item_no = attach->item_no;

                        if (item_no >= ITEM_ATTACH_START) {
                            mes_no = item_no + 500;
                            value = GetAttachVolumeForMsg(attach);

                            if (attachments[WepMenu.board.cursor].item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                                name_mes = GetWeaponMsgNo2(attachments[WepMenu.board.cursor].sphere_weapon_no);
                            }
                        }
                    }

                    break;
                }
                case PERSONAL_BOARD_AREA_TRASH:
                    mes_no = 0xBE;
                    break;
                default:
                    break;
            }

            break;
        default:
            break;
    }

    if (WepMenu.state == WEP_STATE_WARNING) {
        s16 warnings[10] = {0xA1, 0xA2, 0xA3, 0x19B, 0x19C, 0x19D, 0xA8, 0xAA, 0xA9, 0xAB};
        help_no = warnings[WepMenu.warning_no];
    }

    if (WepMenu.state == WEP_STATE_SCRAP_CONFIRM) {
        help_no = 0xA5;
    }

    if (WepMenu.state == WEP_STATE_SCRAP_REFUSED) {
        help_no = 0xA6;

        if (weapon->item_no == ITEM_WEAPON_SERPENT_SWORD && GetMenuHebikiriFlag() == 0) {
            help_no = 0xA7;
        }
    }

    if (CommonMenuMes2.mes_made != mes_no || CommonMenuMes2.value != value || (CommonMenuMes2.mes_made == 0x24E && (CommonMenuMes2.mes_no[0] != name_mes + 100 || CommonMenuMes2.value != value))) {
        CommonMenuMes2.value_signed = true;
        CommonMenuMes2.value_show = false;

        if (name_mes > 0) {
            CommonMenuMes2.mes_no[0] = name_mes + 100;
        }

        CommonMenuMes2.value = value;
        CommonMenuMes2.mes_made = -1;
        CommonMenuMes2.MakeMesWin(mes_no);
    }

    int changed = 0;

    if (WepMenu.mode == WEP_MENU_BUILDUP_SELECT || WepMenu.mode == WEP_MENU_BUILDUP_PREVIEW) {
        s16 previous[3];

        for (int i = 0; i < 3; i++) {
            previous[i] = CommonMenuMes1.mes_no[i];

            if (previous[i] != builds[i].weapon_no + 100 && previous[i] > 100) {
                changed = 1;
            }

            if (builds[i].enabled == 1) {
                CommonMenuMes1.mes_no[i] = builds[i].weapon_no + 100;
            } else if (builds[i].enabled == 0) {
                CommonMenuMes1.mes_no[i] = 2;
            }

            if (builds[i].weapon_no <= 0) {
                CommonMenuMes1.mes_no[i] = -1;
            }
        }
    } else if (MenuWepLevelUp.operation_kind == WEP_EFFECT_NONE) {
        for (int i = 0; i < 4; i++) {
            CommonMenuMes1.mes_no[i] = -1;

            if (i >= 0 && i < 10) {
                CommonMenuMes1.line_pos[i].x = -1;
                CommonMenuMes1.line_pos[i].y = -1;
            }
        }
    }

    if ((CommonMenuMes1.mes_made != help_no && MenuWepLevelUp.operation_kind == WEP_EFFECT_NONE) || ((WepMenu.mode == WEP_MENU_BUILDUP_SELECT || WepMenu.mode == WEP_MENU_BUILDUP_PREVIEW) && changed != 0) || (CommonMenuMes1.mes_made != help_no && WepMenu.state == WEP_STATE_WARNING) || (CommonMenuMes1.mes_made != help_no && WepMenu.state == WEP_STATE_SCRAP_CONFIRM) || (CommonMenuMes1.mes_made != help_no && WepMenu.state == WEP_STATE_SCRAP_REFUSED)) {
        switch (WepMenu.state) {
            case WEP_STATE_WARNING:
            case WEP_STATE_SCRAP_CONFIRM:
            case WEP_STATE_SCRAP_REFUSED:
                CommonMenuMes1.stay_frame = true;
                break;
            default:
                break;
        }

        CommonMenuMes1.mes_made = -1;
        CommonMenuMes1.MakeMesWin(help_no);
    }
}

namespace {

/**
 * Finds the preceding enabled customization row, wrapping from the top row.
 */
int WeaponMenuKastumSelectUp(int row, int enabled_rows) {
    if (row == 0) {
        row = 7;
    }

    int selected_row = 3;

    while (row >= 4) {
        if (enabled_rows & (1 << (row - 4))) {
            selected_row = row - 1;
            break;
        }

        row--;
    }

    return selected_row;
}

/**
 * Finds the following enabled customization row, wrapping to the top row.
 */
int WeaponMenuKastumSelectDown(int row, int enabled_rows) {
    int selected_row = 0;

    while (row <= 6) {
        if (enabled_rows & (1 << (row - 3))) {
            selected_row = row;
            break;
        }

        row++;
    }

    return selected_row;
}

/**
 * Moves the cursor across the weapon list and opens what it settles on.
 */
int WeaponSelectKey() {
    int result = 0;
    int chara_num = 6;
    int prev_chara = WepMenu.chara;

    switch (WepMenu.mode) {
        case WEP_MENU_LIST: {
            int done = 0;

            if (GamePad.Down(PAD_LEFT) && 0 < WepMenu.weapon_slot) {
                WepMenu.weapon_slot--;
            }

            if (GamePad.Down(PAD_RIGHT) && WepMenu.weapon_slot < 9) {
                WepMenu.weapon_slot++;
            }

            // Debug builds edit the selected weapon: level, every flag, maxed stats, and a cycle of element and
            // anti-monster values.
            if (DebugMode) {
                WEAPON_HAVE *weapon = GetNowSelectWeapon();

                if (GamePad.Down2(PAD_CROSS) && weapon->level < 99) {
                    weapon->level++;
                }

                if (GamePad.Down2(PAD_R2 | PAD_R1)) {
                    for (int i = 1; i <= 13; i++) {
                        weapon->flags |= 1 << i;
                    }
                }

                if (GamePad.Down2(PAD_TRIANGLE)) {
                    WEAPON_DATA *data = GetWeaponData(weapon->item_no);
                    weapon->level = 99;
                    weapon->attack = data->attack_max;
                    weapon->endurance = 99;
                    weapon->speed = 99;
                    weapon->magic = data->magic_max;
                    weapon->durability = 99;
                    weapon->durability_f = 99.0f;

                    for (char &element : weapon->elem) {
                        element = 99;
                    }

                    for (char &vs_value : weapon->vs_monster) {
                        vs_value = 99;
                    }

                    weapon->synthesis_count = 49;

                    for (int i = 1; i <= 13; i++) {
                        weapon->flags |= 1 << i;
                    }
                }

                if (GamePad.Down2(PAD_CIRCLE)) {
                    weapon->flags = 0;
                    weapon->flags = 0x36EA;
                }

                static int now = 0;

                if (GamePad.Down2(PAD_SQUARE)) {
                    now++;

                    if (now >= 4) {
                        now = 0;
                    }

                    if (now == 0) {
                        for (char &element : weapon->elem) {
                            element = 0;
                        }

                        for (char &vs_value : weapon->vs_monster) {
                            vs_value = 0;
                        }

                        printf("now 0%\n");
                    }

                    if (now == 1) {
                        for (char &element : weapon->elem) {
                            element = 20;
                        }

                        for (char &vs_value : weapon->vs_monster) {
                            vs_value = 20;
                        }

                        printf("now 20%\n");
                    }

                    if (now == 2) {
                        for (char &element : weapon->elem) {
                            element = 50;
                        }

                        for (char &vs_value : weapon->vs_monster) {
                            vs_value = 50;
                        }

                        printf("now 50%\n");
                    }

                    if (now == 3) {
                        for (char &element : weapon->elem) {
                            element = 100;
                        }

                        for (char &vs_value : weapon->vs_monster) {
                            vs_value = 100;
                        }

                        printf("now 100%\n");
                    }

                    for (int i = 0; i < now; i++) {
                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                    }
                }
            }

            if (GamePad.Down(PAD_CIRCLE)) {
                ComMenuSePlay(MENU_SOUND_REFUSE);

                switch (WepMenu.open_mode) {
                    case WEP_OPEN_VIEW_ONLY:
                        break;
                    case WEP_OPEN_FROM_BAR:
                        if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) {
                            MenuTextureReload(MenuShadowReadBlock);
                            DngActiveWeaponTextureCopy();
                        }

                        BattleMenuFlag = BTLMENU_STATE_WEAPON_CLOSE;
                        WepMenu.state = WEP_STATE_CLOSE;
                        WepMenu.counter = 0;
                        break;
                    default:
                        break;
                }

                done = 1;
            } else if (GamePad.Down(PAD_CROSS)) {
                switch (WepMenu.open_mode) {
                    case WEP_OPEN_FROM_BAR:
                        if (GetDefaultWeaponNo(WepMenu.chara) > DngWepHavePt[WepMenu.weapon_slot].item_no) {
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else {
                            WepMenu.mode = WEP_MENU_ACTION;
                            WepMenu.board.cursor = WEP_ACTION_EQUIP;
                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                        }

                        break;
                    default:
                        break;
                }

                done = 1;
            }

            if (!done) {
                if (GamePad.Down(PAD_R2 | PAD_R1)) {
                    WepMenu.chara++;

                    if (WepMenu.chara >= chara_num) {
                        WepMenu.chara = 0;
                    }
                }

                if (GamePad.Down(PAD_L2 | PAD_L1)) {
                    WepMenu.chara--;

                    if (WepMenu.chara < 0) {
                        WepMenu.chara = chara_num - 1;
                    }
                }
            }

            break;
        }
        case WEP_MENU_ACTION: {
            WEAPON_HAVE *weapon = GetNowSelectWeapon();
            int          status = NowWeaponStatusValue(weapon);

            if (GamePad.Down(PAD_UP)) {
                if (0 < WepMenu.board.cursor && WepMenu.board.cursor < 5) {
                    WepMenu.board.cursor--;
                } else {
                    WepMenu.board.cursor = WeaponMenuKastumSelectUp(WepMenu.board.cursor, status);
                }
            }

            if (GamePad.Down(PAD_DOWN)) {
                if (WepMenu.board.cursor >= 3) {
                    WepMenu.board.cursor = WeaponMenuKastumSelectDown(WepMenu.board.cursor + 1, status);
                } else {
                    WepMenu.board.cursor++;
                }
            }

            if (GamePad.Down(PAD_RIGHT) && WepMenu.board.cursor == WEP_ACTION_EQUIP) {
                WepMenu.state = WEP_STATE_SCRAP_PROMPT;
                WepMenu.confirm[0] = 1;
                ComMenuSePlay(MENU_SOUND_CURSOR);
            } else if (GamePad.Down(PAD_CROSS)) {
                s8 slot = WepMenu.weapon_slot;
                NowWeaponStatusValue(weapon);

                switch (WepMenu.board.cursor) {
                    case WEP_ACTION_EQUIP: {
                        CDngStatusData *status_data = BtlMenuStatusPt;

                        if (status_data->GetActiveCharaStatus(WepMenu.chara) & AILMENT_CURSE) {
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                            WepMenu.state = WEP_STATE_WARNING;
                            WepMenu.warning_no = 3;
                        } else if (BtlMenuStatusPt->res_limit_zone_current == RES_LIMIT_ZONE_NO_WEAPON_CHANGE) {
                            WepMenu.state = WEP_STATE_WARNING;
                            WepMenu.warning_no = 4;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else if (BtlMenuStatusPt->party_size <= WepMenu.chara) {
                            WepMenu.state = WEP_STATE_WARNING;
                            WepMenu.warning_no = 2;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else {
                            BtlMenuStatusPt->equipped_weapon_slot[WepMenu.chara] = slot;
                            WepFrameRate = 0.0f;
                            WepMenu.state = WEP_STATE_EQUIP;
                            int chara = static_cast<CUserStatus *>(BtlMenuStatusPt)->cur_chara;

                            if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && ((chara == CHARA_OSMOND && WepMenu.chara == CHARA_OSMOND) || (chara == CHARA_RUBY && WepMenu.chara == CHARA_RUBY))) {
                                StartReadBG();
                                DngWepEffectReadStart();
                            }

                            ComMenuSePlay(SE_WEAPON_EQUIP);
                        }

                        break;
                    }
                    case WEP_ACTION_ATTACH: {
                        WepMenu.mode = WEP_MENU_ATTACH_BOARD;
                        WepMenu.board.cursor = 0;
                        WepMenu.tag_page = WEP_TAG_STATUS;
                        SetOldEffectKind(weapon->best_elem);
                        int          holes = GetWeaponHoleNum(weapon->item_no);
                        CMenuCursor *cursor = SaveData->GetMenuCursor();
                        int          max = PersonalRetMax(BOARD_PAGE_ATTACH);

                        if (cursor->reset_pos == 0) {
                            WepMenu.mode = cursor->mode[2];

                            if (WepMenu.mode < WEP_MENU_ATTACH_WEAPON || WepMenu.mode > WEP_MENU_ATTACH_TAGS) {
                                WepMenu.mode = WEP_MENU_ATTACH_BOARD;
                            }

                            WepMenu.board.cursor = cursor->pos[2];

                            switch (WepMenu.mode) {
                                case WEP_MENU_ATTACH_SOCKETS:
                                    WepMenu.board.cursor = std::min(holes - 1, WepMenu.board.cursor);

                                    break;
                                case WEP_MENU_ATTACH_BOARD:
                                    WepMenu.board.top_row = WepMenu.board.cursor / 5;

                                    if (max / 5 - 4 < WepMenu.board.top_row) {
                                        WepMenu.board.top_row = max / 5 - 4;

                                        while (max < WepMenu.board.cursor) {
                                            WepMenu.board.cursor -= 5;
                                        }

                                        while (WepMenu.board.cursor < 0) {
                                            WepMenu.board.cursor += 5;
                                        }
                                    }

                                    break;
                                case WEP_MENU_ATTACH_WEAPON:
                                    WepMenu.board.cursor = 0;
                                    break;
                                case WEP_MENU_ATTACH_TAGS:
                                default:
                                    break;
                            }
                        } else {
                            WepMenu.board.top_row = WepMenu.board.cursor / 5;

                            if (max / 5 - 4 < WepMenu.board.top_row) {
                                WepMenu.board.top_row = max / 5 - 4;

                                while (max < WepMenu.board.cursor) {
                                    WepMenu.board.cursor -= 5;
                                }

                                while (WepMenu.board.cursor < 0) {
                                    WepMenu.board.cursor += 5;
                                }
                            }
                        }

                        WepMenu.sockets_enabled = holes > 0;

                        WepMenu.board.cursor = std::max(WepMenu.board.cursor, 0);

                        int pages = max / 5;
                        WepMenu.board.y = 127 - WepMenu.board.top_row * 40;
                        WepMenu.board.scroll = 140.0f + 114.0f * WepMenu.board.top_row / pages;
                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                        break;
                    }
                    case WEP_ACTION_ELEMENT:
                        WepMenu.mode = WEP_MENU_ELEMENT;
                        WepMenu.tag_page = WEP_TAG_ELEMENT;
                        WepMenu.element = weapon->best_elem;

                        if (WepMenu.element < 0 || WepMenu.element > 4) {
                            WepMenu.element = 0;
                        }

                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                        break;
                    case WEP_ACTION_REPAIR:
                        if (GetNowItemNum(ITEM_REPAIR_POWDER, MenuItemPackPt) <= 0) {
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else {
                            int   chara = WepMenu.chara;
                            auto *status_data = static_cast<CUserStatus *>(BtlMenuStatusPt);

                            if (ItemUseFunc(status_data, ITEM_REPAIR_POWDER, chara, ITEM_USE_ON_BOARD_WEAPON, weapon) == 2) {
                                DeleteItemAfterUseItem(ITEM_REPAIR_POWDER, MenuItemPackPt);
                                ComMenuSePlay(SE_WEAPON_REPAIR);

                                if (IsDefaultWeapon(weapon->item_no) >= 0) {
                                    SetMenuWeaponModelReference(slot, 1, 1);
                                }

                                MenuWepLevelUp.WepRecover(weapon, &DngWeaponFrm[slot], WepMenuEffectReadBuf, BtlMenuExReadBlock);
                                WepMenu.state = WEP_STATE_REPAIR_EFFECT;
                            } else {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            }
                        }

                        break;
                    case WEP_ACTION_LEVELUP: {
                        int item_no = weapon->item_no;

                        if (item_no == GetDefaultWeaponNo(WepMenu.chara)) {
                            WepMenu.state = WEP_STATE_WARNING;
                            WepMenu.warning_no = 7;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else if (item_no == ITEM_WEAPON_SERPENT_SWORD && GetMenuHebikiriFlag() == 0) {
                            WepMenu.state = WEP_STATE_WARNING;
                            WepMenu.warning_no = 5;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else if (weapon->level >= 99) {
                            WepMenu.state = WEP_STATE_WARNING;
                            WepMenu.warning_no = 8;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else {
                            WepMenu.mode = WEP_MENU_LEVELUP_CONFIRM;
                            WepMenu.board.cursor = 1;
                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                        }

                        break;
                    }
                    case WEP_ACTION_STATUS_BREAK: {
                        int             count = 0;
                        int             i = 0;
                        DNG_CONSUMABLE *items = BtlMenuStatusPt->consumable_items;
                        count = 0;

                        for (i = 0; i < 40; i++) {
                            if (items[i].id >= ITEM_ATTACH_START) {
                                count++;
                            }
                        }

                        if (count >= 40) {
                            WepMenu.state = WEP_STATE_WARNING;
                            WepMenu.warning_no = 1;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else {
                            WepMenu.mode = WEP_MENU_STATUS_BREAK_CONFIRM;
                            WepMenu.board.cursor = 1;
                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                        }

                        break;
                    }
                    case WEP_ACTION_BUILDUP:
                        WepMenu.mode = WEP_MENU_BUILDUP_SELECT;
                        WepMenu.board.cursor = 0;
                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                        break;
                    default:
                        break;
                }
            } else if (GamePad.Down(PAD_CIRCLE)) {
                WepMenu.mode = WEP_MENU_LIST;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        }
        case WEP_MENU_ELEMENT:
            WepMenu.tag_page = WEP_TAG_ELEMENT;

            if (GamePad.Down(PAD_L2 | PAD_R2 | PAD_L1 | PAD_R1)) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            if (GamePad.Down(PAD_UP) && 0 < WepMenu.element) {
                WepMenu.element--;
            }

            if (GamePad.Down(PAD_DOWN) && WepMenu.element < 4) {
                WepMenu.element++;
            }

            if (GamePad.Down(PAD_CROSS)) {
                WEAPON_HAVE *weapon = GetNowSelectWeapon();
                WEAPON_HAVE  total;
                WeaponAllValueSet(weapon, &total, 0);

                if (WeaponMenuCheckEnableSetElem(weapon, &total, WepMenu.element)) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else {
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }
            } else if (GamePad.Down(PAD_CIRCLE)) {
                WepMenu.mode = WEP_MENU_ACTION;

                if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && static_cast<CUserStatus *>(BtlMenuStatusPt)->cur_chara == CHARA_RUBY && WepMenu.chara == CHARA_RUBY) {
                    StartReadBG();
                    DngWepEffectReadStart();
                }

                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        case WEP_MENU_LEVELUP_CONFIRM:
            if (GamePad.Down(PAD_UP | PAD_DOWN)) {
                if (WepMenu.board.cursor != 0) {
                    WepMenu.board.cursor = 0;
                } else {
                    WepMenu.board.cursor = 1;
                }
            }

            if (WepMenu.board.cursor == 0 && GamePad.Down(PAD_CROSS)) {
                s8 slot = WepMenu.weapon_slot;

                if (DngWepHavePt[slot].experience < GetWeaponMaxExp(&DngWepHavePt[slot])) {
                    DeleteItemAfterUseItem(ITEM_POWERUP_POWDER, MenuItemPackPt);
                }

                MenuWepLevelUp.SetLevelUpValue(&DngWepHavePt[slot], &DngWeaponFrm[slot], WepMenuEffectReadBuf, BtlMenuExReadBlock);
                WepMenu.state = WEP_STATE_LEVELUP_EFFECT;
                WepMenu.mode = WEP_MENU_LIST;
                WepMenu.board.cursor = 0;
                ComMenuSePlay(MENU_SOUND_CONFIRM);
            } else if ((WepMenu.board.cursor == 1 && GamePad.Down(PAD_CROSS)) || GamePad.Down(PAD_CIRCLE)) {
                WepMenu.mode = WEP_MENU_ACTION;
                WepMenu.board.cursor = WEP_ACTION_LEVELUP;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        case WEP_MENU_STATUS_BREAK_CONFIRM:
            if (GamePad.Down(PAD_UP | PAD_DOWN)) {
                if (WepMenu.board.cursor != 0) {
                    WepMenu.board.cursor = 0;
                } else {
                    WepMenu.board.cursor = 1;
                }
            }

            if ((WepMenu.board.cursor == 1 && GamePad.Down(PAD_CROSS)) || GamePad.Down(PAD_CIRCLE)) {
                WepMenu.mode = WEP_MENU_ACTION;
                WepMenu.board.cursor = WEP_ACTION_STATUS_BREAK;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            } else if (WepMenu.board.cursor == 0 && GamePad.Down(PAD_CROSS)) {
                int   slot = WepMenu.weapon_slot;
                auto *buffer = reinterpret_cast<CWeaponLevelUp *>(MenuExCashBuffer.base + MenuExCashBuffer.used * 16);
                MenuWepLevelUp.SetStatusBreak(&DngWepHavePt[slot], &DngWeaponFrm[slot], buffer, BtlMenuExReadBlock);
                WepMenu.state = WEP_STATE_STATUS_BREAK_EFFECT;
                WepMenu.mode = WEP_MENU_LIST;
                WepMenu.board.cursor = 0;
                ComMenuSePlay(MENU_SOUND_CONFIRM);
            }

            break;
        case WEP_MENU_BUILDUP_SELECT: {
            WEAPON_HAVE     *weapon = GetNowSelectWeapon();
            WEP_BUILDUP_INFO info[7];
            int              build_up = 0;
            EnableBuildUpModel(info, weapon);
            int count = WeaponStatusBuildUp(weapon, build_up);

            if (GamePad.Down(PAD_UP)) {
                WepMenu.board.cursor--;
            }

            if (GamePad.Down(PAD_DOWN)) {
                WepMenu.board.cursor++;
            }

            if (WepMenu.board.cursor < 0) {
                WepMenu.board.cursor = count - 1;
            }

            if (count - 1 < WepMenu.board.cursor) {
                WepMenu.board.cursor = 0;
            }

            if (GamePad.Down(PAD_CIRCLE)) {
                WepMenu.board.cursor = WEP_ACTION_BUILDUP;
                WepMenu.mode = WEP_MENU_ACTION;
            } else if (GamePad.Down(PAD_CROSS)) {
                int attached = 0;

                for (int i = 0; i < GetWeaponHoleNum(weapon->item_no); i++) {
                    if (weapon->attach[i].item_no >= ITEM_ATTACH_START) {
                        attached++;
                    }
                }

                int weapon_no = info[WepMenu.board.cursor].weapon_no;
                int enabled = info[WepMenu.board.cursor].enabled;

                if (enabled == 1 && attached == 0) {
                    MenuWepLevelUp.buildup_weapon_no = weapon_no;
                    WepMenu.mode = WEP_MENU_BUILDUP_CONFIRM;
                    WepMenu.board.cursor = 1;
                } else if (enabled == 0) {
                    WepMenu.mode = WEP_MENU_BUILDUP_PREVIEW;
                    WepMenu.tag_page = WEP_TAG_STATUS;
                } else {
                    WepMenu.state = WEP_STATE_WARNING;
                    WepMenu.warning_no = 6;
                }
            }

            break;
        }
        case WEP_MENU_BUILDUP_CONFIRM:
            if (GamePad.Down(PAD_UP | PAD_DOWN)) {
                if (WepMenu.board.cursor != 0) {
                    WepMenu.board.cursor = 0;
                } else {
                    WepMenu.board.cursor = 1;
                }
            }

            if ((WepMenu.board.cursor == 1 && GamePad.Down(PAD_CROSS)) || GamePad.Down(PAD_CIRCLE)) {
                WepMenu.mode = WEP_MENU_ACTION;
                WepMenu.board.cursor = WEP_ACTION_BUILDUP;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            } else if (WepMenu.board.cursor == 0 && GamePad.Down(PAD_CROSS)) {
                MenuWepLevelUp.SetBuildUp(&DngWepHavePt[WepMenu.weapon_slot], &DngWeaponFrm[WepMenu.weapon_slot], WepMenuEffectReadBuf, BtlMenuExReadBlock);
                WepMenu.state = WEP_STATE_BUILDUP_EFFECT;
                WepMenu.mode = WEP_MENU_LIST;
                WepMenu.board.cursor = 0;
                ComMenuSePlay(MENU_SOUND_CONFIRM);
            }

            break;
        case WEP_MENU_BUILDUP_PREVIEW:
            if (GamePad.Down(PAD_L2 | PAD_L1)) {
                WepMenu.tag_page--;

                if (WepMenu.tag_page < 0) {
                    WepMenu.tag_page = WEP_TAG_VS_MONSTER;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (GamePad.Down(PAD_R2 | PAD_R1)) {
                WepMenu.tag_page++;

                if (WepMenu.tag_page > WEP_TAG_VS_MONSTER) {
                    WepMenu.tag_page = WEP_TAG_STATUS;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (GamePad.Down(PAD_CIRCLE)) {
                WepMenu.mode = WEP_MENU_BUILDUP_SELECT;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        default:
            break;
    }

    if (prev_chara != WepMenu.chara) {
        int chara = static_cast<CUserStatus *>(BtlMenuStatusPt)->cur_chara;

        if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && chara == prev_chara) {
            DngWeaponEquipModelBuild(chara, MenuExtendReadBlock, BtlMenuReadBuf);
        }

        int             selected_chara = WepMenu.chara;
        CDngStatusData *status_data = BtlMenuStatusPt;
        DngWepHavePt = status_data->chara_weapons[selected_chara];

        if (StartReadWepMDS(BtlMenuReadBuf, WepMenu.chara)) {
            MenuExTextureReadFlag = MENU_EX_TEX_UNREAD;
            WepMenu.state = WEP_STATE_CHARA_CHANGE;
            WepMenu.counter = 0;

            if (abs(prev_chara - WepMenu.chara) > 1) {
                for (auto &sys_chara : SysChara) {
                    sys_chara.x = (sys_chara.chara - WepMenu.chara) * 110 + 270;
                }
            }

            int             next_chara = WepMenu.chara;
            CDngStatusData *next_status_data = BtlMenuStatusPt;
            DngWepHavePt = next_status_data->chara_weapons[next_chara];
            WepMenu.weapon_slot = BtlMenuStatusPt->equipped_weapon_slot[WepMenu.chara];

            if (WepMenu.weapon_slot < 0 || WepMenu.weapon_slot >= 10) {
                WepMenu.weapon_slot = 0;
            }

            WeaponPos = 0xE8 - WepMenu.weapon_slot * 0xD6;
            WepPolyPos = 2.0f + 16.0f * -WepMenu.weapon_slot;
            WepMenu.state = WEP_STATE_IDLE;
            WepMenu.counter = 0;
            result = 0;
        } else {
            ComMenuSePlay(MENU_SOUND_REFUSE);
        }
    }

    return result;
}

/**
 * Stores a menu's cursor position in the saved menu cursors.
 */
void SetCursorPos(CMenuCursor *cursor, int menu, int pos) {
    cursor->pos[menu] = pos;
}

/**
 * Stores the party member the item menu shows in the saved menu cursors.
 */
void SetCursorChara(CMenuCursor *cursor, int chara) {
    cursor->chara_no = chara;
}

/**
 * Puts the attachment the weapon menu is holding back where it came from.
 */
void WepAttachHaveCancel() {
    ATTACH_LIST *slot = nullptr;
    s16          held = BtlHaveItemPt->item_no;

    if (held >= ITEM_ATTACH_START) {
        switch (BtlHaveItemPt->from_page) {
            case WEP_MENU_ATTACH_BOARD: {
                auto *items = reinterpret_cast<ATTACH_LIST *>(BtlMenuStatusPt->consumable_items);
                slot = &items[BtlHaveItemPt->from_slot];
                break;
            }
            case WEP_MENU_ATTACH_SOCKETS:
                slot = &DngWepHavePt[WepMenu.weapon_slot].attach[BtlHaveItemPt->from_slot];
                break;
            default:
                break;
        }

        if (slot != nullptr) {
            s16 item_no = slot->item_no;
            MenuDataSwap(slot, &WepMenu.board.held_attach);
            BtlHaveItemPt->item_no = item_no;
            slot->item_no = held;
        }

        if (BtlHaveItemPt->item_no < ITEM_ATTACH_START) {
            InitHaveData(BtlHaveItemPt);
        }
    } else {
        CMenuCursor *cursor = SaveData->GetMenuCursor();

        if (cursor->reset_pos == 0) {
            cursor->mode[2] = WepMenu.mode;
            SetCursorPos(cursor, 2, WepMenu.board.cursor);
        }

        WepMenu.mode = WEP_MENU_ACTION;
        WepMenu.board.cursor = WEP_ACTION_ATTACH;
        WepMenu.sockets_enabled = false;
        GetNowSelectWeapon();

        if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && BtlMenuStatusPt->cur_chara == CHARA_RUBY && WepMenu.chara == CHARA_RUBY) {
            StartReadBG();
            DngWepEffectReadStart();
        }
    }
}

} // namespace

void WeaponMenuAttachModeKey() {
    switch (WepMenu.mode) {
        case WEP_MENU_ATTACH_TAGS:
        case WEP_MENU_ATTACH_SOCKETS:
        case WEP_MENU_ATTACH_BOARD:
        case WEP_MENU_ATTACH_WEAPON: {
            int page = WepMenu.tag_page;

            if (GamePad.Down(PAD_R2 | PAD_R1) != 0) {
                WepMenu.tag_page++;

                if (WepMenu.tag_page > WEP_TAG_VS_MONSTER) {
                    WepMenu.tag_page = WEP_TAG_STATUS;
                }
            }

            if (GamePad.Down(PAD_L2 | PAD_L1) != 0) {
                WepMenu.tag_page--;

                if (WepMenu.tag_page < 0) {
                    WepMenu.tag_page = WEP_TAG_VS_MONSTER;
                }
            }

            if (page != WepMenu.tag_page) {
                int rows[3] = {5, 4, 9};

                WepMenu.tag_row = std::min<int>(rows[WepMenu.tag_page], WepMenu.tag_row);

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            break;
        }
        default:
            break;
    }

    int row = WepMenu.tag_row;
    s16 mode = WepMenu.mode;
    WeaponMenuSelectKeyLockFlag = 0;

    switch (mode) {
        case WEP_MENU_ATTACH_WEAPON:
            WeaponMenuActWepKey();
            break;
        case WEP_MENU_ATTACH_TAGS:
            WeaponMenuTagKey();
            break;
        case WEP_MENU_ATTACH_SOCKETS:
            WeaponMenuAttachWepKey();
            break;
        case WEP_MENU_ATTACH_BOARD:
            WeaponMenuAttachKey();
            break;
        default:
            break;
    }

    if ((row != WepMenu.tag_row || mode != WepMenu.mode) && GamePad.Down(PAD_CIRCLE) == 0) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
}

namespace {

/**
 * Handles input on the weapon menu's equipped weapon row.
 */
void WeaponMenuActWepKey() {
    if (GamePad.Down(PAD_RIGHT) != 0) {
        WepMenu.mode = WEP_MENU_ATTACH_BOARD;
        WepMenu.board.cursor = WepMenu.board.top_row * 5 + 10;
    } else if (GamePad.Down(PAD_DOWN) != 0) {
        WepMenu.mode = WEP_MENU_ATTACH_TAGS;
        WepMenu.tag_row = WEP_TAG_ROW_DURABILITY;
    } else if (GamePad.Down(PAD_CIRCLE) != 0) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        WepAttachHaveCancel();
    }
}

} // namespace

void WeaponMenuTagKey() {
    int moved = 0;
    s16 rows[3] = {5, 4, 10};

    WepMenu.tag_row = std::max<s8>(WepMenu.tag_row, 0);

    WepMenu.tag_row = std::min<int>(rows[WepMenu.tag_page], WepMenu.tag_row);

    GetWeaponHoleNum(GetNowSelectWeapon()->item_no);

    switch (WepMenu.tag_page) {
        case WEP_TAG_STATUS:
            if (GamePad.Down(PAD_UP) != 0) {
                moved = 1;

                if (0 < WepMenu.tag_row) {
                    WepMenu.tag_row--;
                } else if (WepMenu.sockets_enabled != 0) {
                    WepMenu.mode = WEP_MENU_ATTACH_SOCKETS;
                } else {
                    WepMenu.mode = WEP_MENU_ATTACH_WEAPON;
                }
            } else if (GamePad.Down(PAD_DOWN) != 0) {
                moved = 1;

                if (WepMenu.tag_row < rows[WepMenu.tag_page]) {
                    WepMenu.tag_row++;
                }
            } else if (GamePad.Down(PAD_CROSS) != 0) {
                moved = 1;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            // Debug builds step the status row under the cursor up or down, and max it out.
            if (DebugMode) {
                WEAPON_HAVE  total;
                WEAPON_HAVE *weapon = GetNowSelectWeapon();
                WEAPON_DATA *data = GetWeaponData(weapon->item_no);
                WeaponAllValueSet(weapon, &total, 0);

                if (GamePad.On2(PAD_RIGHT)) {
                    switch (WepMenu.tag_row) {
                        case WEP_TAG_ROW_DURABILITY:
                            if (weapon->durability_f < weapon->durability) {
                                weapon->durability_f += 1.0f;
                            }

                            break;
                        case WEP_TAG_ROW_EXP:
                            if (weapon->experience < GetWeaponMaxExp(weapon)) {
                                weapon->experience++;
                            }

                            break;
                        case WEP_TAG_ROW_ATTACK:
                            if (total.attack < data->attack_max) {
                                weapon->attack++;
                            }

                            break;
                        case WEP_TAG_ROW_ENDURANCE:
                            if (weapon->endurance < 99) {
                                weapon->endurance++;
                            }

                            break;
                        case WEP_TAG_ROW_SPEED:
                            if (weapon->speed < 99) {
                                weapon->speed++;
                            }

                            break;
                        case WEP_TAG_ROW_MAGIC:
                            if (total.magic < data->magic_max) {
                                weapon->magic++;
                            }

                            break;
                        default:
                            break;
                    }
                }

                if (GamePad.On2(PAD_LEFT)) {
                    switch (WepMenu.tag_row) {
                        case WEP_TAG_ROW_DURABILITY:
                            if (1.0f < weapon->durability_f) {
                                weapon->durability_f -= 1.0f;
                            }

                            break;
                        case WEP_TAG_ROW_EXP:
                            if (0 < weapon->experience) {
                                weapon->experience--;
                            }

                            break;
                        case WEP_TAG_ROW_ATTACK:
                            if (0 < weapon->attack) {
                                weapon->attack--;
                            }

                            break;
                        case WEP_TAG_ROW_ENDURANCE:
                            if (0 < weapon->endurance) {
                                weapon->endurance--;
                            }

                            break;
                        case WEP_TAG_ROW_SPEED:
                            if (0 < weapon->speed) {
                                weapon->speed--;
                            }

                            break;
                        case WEP_TAG_ROW_MAGIC:
                            if (0 < weapon->magic) {
                                weapon->magic--;
                            }

                            break;
                        default:
                            break;
                    }
                }

                if (GamePad.On2(PAD_CROSS)) {
                    switch (WepMenu.tag_row) {
                        case WEP_TAG_ROW_DURABILITY:
                            if (weapon->durability < 99) {
                                weapon->durability++;
                            }

                            break;
                        default:
                            break;
                    }
                }

                if (GamePad.On2(PAD_CIRCLE)) {
                    switch (WepMenu.tag_row) {
                        case WEP_TAG_ROW_DURABILITY:
                            if (weapon->durability_f < weapon->durability) {
                                weapon->durability--;
                            }

                            break;
                        default:
                            break;
                    }
                }

                if (GamePad.Down2(PAD_TRIANGLE)) {
                    switch (WepMenu.tag_row) {
                        case WEP_TAG_ROW_ATTACK:
                            weapon->attack = 99;
                            break;
                        case WEP_TAG_ROW_ENDURANCE:
                            weapon->endurance = 99;
                            break;
                        case WEP_TAG_ROW_SPEED:
                            weapon->speed = 99;
                            break;
                        case WEP_TAG_ROW_MAGIC:
                            weapon->magic = 99;
                            break;
                        default:
                            break;
                    }
                }
            }

            break;
        case WEP_TAG_ELEMENT:
            if (GamePad.Down(PAD_UP) != 0) {
                moved = 1;

                if (0 < WepMenu.tag_row) {
                    WepMenu.tag_row--;
                } else if (WepMenu.sockets_enabled != 0) {
                    WepMenu.mode = WEP_MENU_ATTACH_SOCKETS;
                } else {
                    WepMenu.mode = WEP_MENU_ATTACH_WEAPON;
                }
            } else if (GamePad.Down(PAD_DOWN) != 0) {
                moved = 1;

                if (WepMenu.tag_row < rows[WepMenu.tag_page]) {
                    WepMenu.tag_row++;
                }
            } else if (GamePad.Down(PAD_CROSS) != 0) {
                WEAPON_HAVE total;
                moved = 1;
                WEAPON_HAVE *weapon = GetNowSelectWeapon();
                WeaponAllValueSet(weapon, &total, 0);

                if (WeaponMenuCheckEnableSetElem(weapon, &total, WepMenu.tag_row) != 0) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                } else {
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                }
            }

            if (DebugMode) {
                if (GamePad.On2(PAD_RIGHT)) {
                    WEAPON_HAVE *weapon = GetNowSelectWeapon();

                    if (weapon->elem[WepMenu.tag_row] < 99) {
                        weapon->elem[WepMenu.tag_row]++;
                    }
                }

                if (GamePad.On2(PAD_LEFT)) {
                    WEAPON_HAVE *weapon = GetNowSelectWeapon();

                    if (0 < weapon->elem[WepMenu.tag_row]) {
                        weapon->elem[WepMenu.tag_row]--;
                    }
                }
            }

            break;
        case WEP_TAG_VS_MONSTER:
            if (GamePad.Down(PAD_UP) != 0) {
                moved = 1;

                if (WepMenu.tag_row % 5 != 0) {
                    WepMenu.tag_row--;
                } else if (WepMenu.sockets_enabled != 0) {
                    WepMenu.mode = WEP_MENU_ATTACH_SOCKETS;
                } else {
                    WepMenu.mode = WEP_MENU_ATTACH_WEAPON;
                }
            } else if (GamePad.Down(PAD_DOWN) != 0) {
                moved = 1;

                if (WepMenu.tag_row % 5 != 4) {
                    WepMenu.tag_row++;
                }
            } else if (GamePad.Down(PAD_LEFT) != 0) {
                moved = 1;

                if (WepMenu.tag_row >= 5) {
                    WepMenu.tag_row -= 5;
                }
            } else if (GamePad.Down(PAD_CROSS) != 0) {
                moved = 1;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            if (DebugMode) {
                if (GamePad.On2(PAD_RIGHT)) {
                    WEAPON_HAVE *weapon = GetNowSelectWeapon();

                    if (weapon->vs_monster[WepMenu.tag_row] < 99) {
                        weapon->vs_monster[WepMenu.tag_row]++;
                    }
                }

                if (GamePad.On2(PAD_LEFT)) {
                    WEAPON_HAVE *weapon = GetNowSelectWeapon();

                    if (0 < weapon->vs_monster[WepMenu.tag_row]) {
                        weapon->vs_monster[WepMenu.tag_row]--;
                    }
                }
            }

            break;
        default:
            break;
    }

    if (moved != 0) {
        return;
    }

    if (GamePad.Down(PAD_CIRCLE) != 0) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        WepAttachHaveCancel();
        return;
    }

    if (GamePad.Down(PAD_RIGHT) != 0) {
        switch (WepMenu.tag_page) {
            case WEP_TAG_VS_MONSTER:
                if (WepMenu.tag_row < 5) {
                    WepMenu.tag_row += 5;
                    return;
                }

                WepMenu.mode = WEP_MENU_ATTACH_BOARD;
                WepMenu.board.cursor = (WepMenu.board.top_row + 2) * 5;
                return;
            case WEP_TAG_STATUS:
            case WEP_TAG_ELEMENT:
                WepMenu.mode = WEP_MENU_ATTACH_BOARD;
                WepMenu.board.cursor = (WepMenu.board.top_row + 2) * 5;
                break;
            default:
                break;
        }
    }
}

namespace {

/**
 * Moves the cursor across the sockets of the selected weapon and fits or takes off the attachment held.
 */
void WeaponMenuAttachWepKey() {
    int          moved = 0;
    WEAPON_HAVE *weapon = GetNowSelectWeapon();

    if (GamePad.Down(PAD_LEFT)) {
        moved = 1;

        if (0 < WepMenu.board.cursor) {
            WepMenu.board.cursor--;
        }
    }

    int holes = GetWeaponHoleNum(weapon->item_no);

    if (WepMenu.board.cursor > holes - 1) {
        WepMenu.board.cursor = holes - 1;

        if (WepMenu.board.cursor < 0) {
            WepMenu.board.cursor = 0;
            WepMenu.mode = WEP_MENU_ATTACH_WEAPON;
            return;
        }
    }

    if (holes <= 0) {
        WepMenu.board.cursor = 0;
        WepMenu.mode = WEP_MENU_ATTACH_WEAPON;
        return;
    }

    if (moved) {
        return;
    }

    if (GamePad.Down(PAD_CROSS)) {
        if (holes <= 0) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            return;
        }

        int hole = WepMenu.board.cursor;

        if (BtlHaveItemPt->item_no < ITEM_ATTACH_START) {
            if (weapon->attach[hole].item_no <= 0) {
                ComMenuSePlay(MENU_SOUND_REFUSE);
                return;
            }

            ComMenuSePlay(SE_ATTACHMENT_REMOVE);
            s16 held = BtlHaveItemPt->item_no;
            s16 item_no = weapon->attach[hole].item_no;
            MenuDataSwap(&WepMenu.board.held_attach, &weapon->attach[hole]);
            BtlHaveItemPt->item_no = item_no;
            weapon->attach[hole].item_no = held;
            BtlHaveItemPt->from_slot = WepMenu.board.cursor;
            BtlHaveItemPt->from_page = WEP_MENU_ATTACH_SOCKETS;
            WEAPON_HAVE total;
            WeaponAllValueSet(weapon, &total, 0);
            WeaponMenuCheckElemValue(weapon, &total);
            return;
        }

        WEAPON_DATA *data = GetWeaponData(weapon->item_no);
        int          socket = data->hole[hole];

        if (BtlHaveItemPt->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE && socket != 2) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            return;
        }

        ComMenuSePlay(SE_ATTACHMENT_SET);
        s16 item_no = weapon->attach[hole].item_no;
        s16 held = BtlHaveItemPt->item_no;
        MenuDataSwap(&weapon->attach[hole], &WepMenu.board.held_attach);
        weapon->attach[hole].item_no = held;
        BtlHaveItemPt->item_no = item_no;
        BtlHaveItemPt->from_slot = WepMenu.board.cursor;
        BtlHaveItemPt->from_page = WepMenu.mode;
        WEAPON_HAVE total;
        WeaponAllValueSet(weapon, &total, 0);
        WeaponMenuCheckElemValue(weapon, &total);
        return;
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        WepAttachHaveCancel();
        return;
    }

    if (GamePad.Down(PAD_RIGHT)) {
        if (WepMenu.board.cursor < holes - 1) {
            WepMenu.board.cursor++;

            WepMenu.board.cursor = std::max(WepMenu.board.cursor, 0);
        } else {
            WepMenu.mode = WEP_MENU_ATTACH_BOARD;
            WepMenu.board.cursor = WepMenu.board.top_row * 5;
        }
    } else if (GamePad.Down(PAD_SQUARE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
    } else if (GamePad.Down(PAD_DOWN)) {
        WepMenu.mode = WEP_MENU_ATTACH_TAGS;
        WepMenu.tag_row = WEP_TAG_ROW_DURABILITY;
    }
}

} // namespace

s32 WeaponMenuSelectKeyLockFlag;

namespace {

/**
 * Handles input while an attachment is being moved on the weapon menu.
 */
void WeaponMenuAttachKey() {
    if (PersonalBoardKey()) {
        if (WepMenu.sockets_enabled) {
            WepMenu.mode = WEP_MENU_ATTACH_SOCKETS;
            WepMenu.board.cursor = GetWeaponHoleNum(GetNowSelectWeapon()->item_no) - 1;
        } else {
            WepMenu.mode = WEP_MENU_ATTACH_WEAPON;
        }
    } else if (GamePad.Down(PAD_CROSS)) {
        switch (WepMenu.board.cursor_area) {
            case PERSONAL_BOARD_AREA_TRASH:
                InitHaveData(BtlHaveItemPt);
                InitHaveAttach(&WepMenu.board.held_attach);
                ComMenuSePlay(MENU_SOUND_REFUSE);
                break;
            case PERSONAL_BOARD_AREA_CELLS: {
                DNG_CONSUMABLE *items = BtlMenuStatusPt->consumable_items;
                auto           *slot = reinterpret_cast<ATTACH_LIST *>(&items[WepMenu.board.cursor]);

                if (slot->item_no >= ITEM_ATTACH_START || BtlHaveItemPt->item_no >= ITEM_ATTACH_START) {
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                    s16 held = BtlHaveItemPt->item_no;
                    s16 item_no = slot->item_no;
                    MenuDataSwap(slot, &WepMenu.board.held_attach);
                    BtlHaveItemPt->item_no = item_no;
                    slot->item_no = held;
                    BtlHaveItemPt->from_slot = WepMenu.board.cursor;
                    BtlHaveItemPt->from_page = WEP_MENU_ATTACH_BOARD;

                    if (BtlHaveItemPt->item_no >= ITEM_ATTACH_START) {
                        WepMenu.sockets_enabled = true;
                    } else {
                        InitHaveData(BtlHaveItemPt);
                        InitHaveAttach(&WepMenu.board.held_attach);
                    }
                } else {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }

                break;
            }
            default:
                break;
        }
    } else if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        WepAttachHaveCancel();
    } else if (GamePad.Down(PAD_SQUARE)) {
        SeitonAttachBoard(reinterpret_cast<ATTACH_LIST *>(BtlMenuStatusPt->consumable_items));
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    }
}

} // namespace

void RepairAndLevelUpDraw(int x, int y, int alpha) {
    int u = 0;
    int v = 0;

    float widths[7] = {10.7f, 10.7f, 10.7f, 15.3f, 11.8f, 12.2f, 12.3f};
    MenuHelpWinDraw(x - 10, y, widths[BtlMenuNowLang], 2.2f, alpha);
    int row_y = y + 0xE;
    MenuTextureReload(BtlMenuReadBlock);
    u8 items[2] = {ITEM_REPAIR_POWDER, ITEM_POWERUP_POWDER};
    u8 shift[7] = {0, 0, 0, 58, 8, 22, 22};

    for (int item_no : items) {
        int       count = GetNowItemNum(item_no, MenuItemPackPt);
        CTexture *icon = RetCTex(static_cast<u8>(item_no), u, v);
        DrawMenu2DSprite(icon, CRect_i_(x + 0x10, row_y, 0x20, 0x20), CRect_i_(u, v, 0x20, 0x20), alpha);
        DrawMenu2DSprite(BtStatus, CRect_i_(x + 0x9A + shift[BtlMenuNowLang], row_y + 8, 0x10, 0x10), CRect_i_(0x60, 0x58, 0x10, 0x10), alpha);
        int number_x = x + 0xC0 + shift[BtlMenuNowLang];

        if (count / 10 > 0) {
            number_x += NumberSprite[1].width >> 1;
        }

        DrawMenuNumber(count, number_x, row_y + 8, StayTex, NumberSprite[1], 2, alpha);
        row_y += 0x22;
    }

    MenuTextureReload(CommonMenuMes3.tex_block);

    if (BtlMenuNowLang > LANG_JAPANESE) {
        s8  count_shift[6] = {20, 20, 0, 12, -6, -4};
        int name_x = x + 0x36;
        int count_x = 0;
        CommonMenuMes3.line_pos[0].x = name_x;
        CommonMenuMes3.line_pos[0].y = y + 0xC;
        count_x = name_x + count_shift[BtlMenuNowLang - 1];
        CommonMenuMes3.line_pos[1].x = count_x;
        CommonMenuMes3.line_pos[1].y = y + 0x1C;
        CommonMenuMes3.line_pos[2].x = name_x;
        CommonMenuMes3.line_pos[2].y = y + 0x2E;
        CommonMenuMes3.line_pos[3].x = count_x;
        CommonMenuMes3.line_pos[3].y = y + 0x3E;
    }

    CommonMenuMes3.stay_frame = false;

    if (CommonMenuMes3.mes_made != 0x1A7) {
        CommonMenuMes3.MakeMesWin(0x1A7);
    }

    CommonMenuMes3.edge_alpha = alpha;
    CommonMenuMes3.Step();
    CommonMenuMes3.DrawMesWin();
}

void DrawBuildUpWeaponSelect(int x, int y, int cursor) {
    PortDrawBuildUpWeaponSelect(x, y, cursor);
}

void WeaponMenuDraw() {
    PortWeaponMenuDraw();
}

namespace {

/**
 * Swaps the item the menu is holding with the one in the given slot of the page it is on.
 *
 * Weapons are only taken when they are not a party member's default weapon and belong to the
 * party member holding the menu's slot.
 */
int ItemTrushKey(int *thrown, int *thrown_vol, int slot) {
    int swapped = 0;
    int page = ItemMenuMode.board.page;
    int kind = WhatIsKindofItem(BtlHaveItemPt->item_no);
    s16 held = BtlHaveItemPt->item_no;
    s16 item_no = 0;

    switch (page) {
        case BOARD_PAGE_ITEM:
            if (!kind || kind == BOARD_PAGE_NONE) {
                MenuDataSwap(&MenuItemPackPt->item[MenuItemPackPt->num + slot], &ItemMenuMode.board.held_item.item_no);
                MenuDataSwap(&ItemMenuMode.board.held_item.volume, &MenuItemPackPt->item_vol[MenuItemPackPt->num + slot]);
                swapped = 1;
            }

            break;
        case BOARD_PAGE_WEAPON:
            if (kind == BOARD_PAGE_WEAPON || kind == BOARD_PAGE_NONE) {
                int          owner = -1;
                WEAPON_HAVE *weapon = nullptr;
                int          equip = WhoIsWeaponEquip(held);

                if (equip < 0) {
                    for (int i = 0; i < 6; i++) {
                        CDngStatusData *status = BtlMenuStatusPt;
                        WEAPON_HAVE    *row = status->chara_weapons[i];
                        weapon = &row[10];

                        if (weapon != nullptr && weapon->item_no >= ITEM_WEAPON_START) {
                            owner = static_cast<s8>(GetWeaponData(weapon->item_no)->owner);
                            break;
                        }
                    }
                } else {
                    CDngStatusData *status = BtlMenuStatusPt;
                    WEAPON_HAVE    *row = status->chara_weapons[equip];
                    weapon = &row[10];

                    if (weapon != nullptr) {
                        owner = equip;
                    }
                }

                int is_default = 0;

                for (int chara = 0; chara < 6; chara++) {
                    int default_no = GetDefaultWeaponNo(chara);

                    if (ItemMenuMode.board.weapon.item_no == default_no || ItemMenuMode.board.weapon.item_no == default_no + 1) {
                        is_default = 1;
                    }
                }

                int allowed = 1;

                if (ItemMenuMode.board.held_equipped_slot >= 0) {
                    allowed = 0;
                }

                if (is_default) {
                    allowed = 0;
                }

                if (equip >= 0 && owner != equip && owner >= 0) {
                    allowed = 0;
                }

                if (allowed) {
                    item_no = weapon->item_no;
                    MenuDataSwap(weapon, &ItemMenuMode.board.weapon);
                    BtlHaveItemPt->item_no = item_no;
                    weapon->item_no = held;
                    swapped = 1;
                } else {
                    swapped = 0;
                }
            }

            break;
        case BOARD_PAGE_ATTACH:
            if (kind == BOARD_PAGE_ATTACH || kind == BOARD_PAGE_NONE) {
                DNG_CONSUMABLE *items = BtlMenuStatusPt->consumable_items;
                ATTACH_LIST    *attach = reinterpret_cast<ATTACH_LIST *>(&items[slot]) + 40;
                item_no = attach->item_no;
                MenuDataSwap(attach, &ItemMenuMode.board.held_attach);
                BtlHaveItemPt->item_no = item_no;
                attach->item_no = held;
                swapped = 1;
            }

            break;
        default:
            break;
    }

    return swapped;
}

} // namespace

void DrawTrushItem() {
    int items[7];
    int values[7];

    for (int i = 0; i < 7; i++) {
        items[i] = 0;
        values[i] = 0;
    }

    if (CheckItemThrow(items, values) == 0 && ItemMenuMode.overflow == 0) {
        return;
    }

    int page = ItemMenuMode.board.page;
    int count[3] = {3, 1, 3};
    int first[3] = {0, 3, 4};
    int entry = first[page];
    int y = 0x7E;
    MenuTextureReload(ItemMenuWeaponIconReadBlock);
    static int ncnt = 0;
    int        bright = static_cast<int>(112.0f + 48.0f * sinf(PI * static_cast<float>(ncnt - 0x3C) / 60.0f));

    for (int i = 0; i < 3; i++) {
        if ((ItemMenuMode.overflow_pages & (1 << (i + 1))) && page == i) {
            for (int j = 0; j < ItemMenuMode.overflow_count[i]; j++, entry++) {
                DrawMenu2DSprite(CharaStatus, CRect_i_(0x138, y, 0x2A, 0x29), CRect_i_(0xC2, 0xD6, 0x2A, 0x2A), bright, bright, bright, 0x80);
                DrawIconParts(items[entry], 0x13C, y + 6, 0, 0x280, 0x80, values[entry]);
                y += 0x2E;
            }

            break;
        }
    }

    (void) count;
    ncnt++;

    if (ncnt > 0x3B) {
        ncnt = 0;
    }

    MenuTextureReload(AtoraNameMes.tex_block);

    if (AtoraNameMes.mes_made != 0xBB) {
        AtoraNameMes.MakeMesWin(0xBB);
    }

    AtoraNameMes.stay_frame = true;
    AtoraNameMes.edge_alpha = 0x80;
    DrawMenuClsMes(&AtoraNameMes, 0x34, 0x152);
}

namespace {

/**
 * Remembers where the item page's cursor stood and closes it.
 */
void ExitItemSelect() {
    CommonMenuMes3.rows = 3;
    CommonMenuMes3.narrow_gaiji = false;
    CommonMenuMes3.char_width = 11;
    AtoraNameMes.stay_frame = false;
    AtoraNameMes.auto_pos = MES_POS_NONE;
    CMenuCursor *cursor = SaveData->GetMenuCursor();

    if (cursor->reset_pos == 0) {
        SetCursorPos(cursor, 0, ItemMenuMode.board.cursor);
        cursor->mode[0] = ItemMenuMode.mode;
        SetCursorChara(cursor, ItemMenuMode.board.page);
    }
}

/**
 * Starts reading the item page's weapon icons in the background.
 */
void StartBGReadItemMenuWepIcon(u_long128 *buffer, int &size) {
    StartReadBG();
    size = LoadFileBGMenuData("wepchara.img", buffer);
    ItemMenuAlreadyReadWepIconTexFlag = 0;
}

} // namespace

void ReadSyncItemMenuWepIcon() {
    BG_READ_INFO     *file = GetReadBGFile(0);
    LOADTEXTURE_INFO2 textures[3] = {
        {"#frame_image#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
        {nullptr,                                    0, 0},
        {nullptr,                                    0, 0}
    };

    textures[0].block_no = ItemMenuWeaponIconReadBlock;
    textures[1].name = reinterpret_cast<char *>(file->buffer);
    textures[1].block_no = ItemMenuWeaponIconReadBlock;
    CharaStatus = TexManager.GetTexture("status", -1);
    TexManager.DeleteTextureBlock(ItemMenuWeaponIconReadBlock);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, textures);
}

/**
 * Opens the item page on one party member, restoring the mode it was left in.
 *
 * When the pack holds more than it can carry, the page opens on the first list that overflows
 * so the player throws something away.
 */
void Btl_InitItemMode(int mode, int chara) {
    CMenuCursor *cursor = nullptr;
    int          max = 0;
    int          thrown[7];

    InitPersonalBoardMode(static_cast<CUserStatus *>(BtlMenuStatusPt), &ItemMenuMode.board, PERSONAL_BOARD_ITEM, BOARD_PAGE_ITEM);
    ItemMenuMode.mode = ITEM_MENU_BOARD;
    ItemMenuMode.chara = chara;
    cursor = SaveData->GetMenuCursor();

    if (cursor->reset_pos == 0) {
        ItemMenuMode.mode = cursor->mode[0];

        if (ItemMenuMode.mode < 0 || ItemMenuMode.mode > ITEM_MENU_BOARD) {
            ItemMenuMode.mode = ITEM_MENU_BOARD;
        }

        max = PersonalRetMax(cursor->chara_no);
        ItemMenuMode.board.cursor = cursor->pos[0];
        ItemMenuMode.board.top_row = ItemMenuMode.board.cursor / 5 - 3;

        ItemMenuMode.board.top_row = std::max(ItemMenuMode.board.top_row, 0);
    } else {
        ItemMenuMode.mode = ITEM_MENU_BOARD;
        ItemMenuMode.board.cursor = 0;
        ItemMenuMode.board.top_row = 0;
        max = PersonalRetMax(BOARD_PAGE_ITEM);
    }

    memset(thrown, 0, sizeof(thrown));

    for (short &count : ItemMenuMode.overflow_count) {
        count = 0;
    }

    ItemMenuMode.overflow_pages = 0;
    ItemMenuMode.overflow = false;

    if (CheckItemThrow(thrown, nullptr)) {
        for (int i = 0; i < 3; i++) {
            if (thrown[i] >= ITEM_DUNGEON_START) {
                ItemMenuMode.overflow_count[0]++;
            }
        }

        if (thrown[3] >= ITEM_WEAPON_START) {
            ItemMenuMode.overflow_count[1]++;
        }

        for (int i = 0; i < 3; i++) {
            if (thrown[i + 4] < ITEM_ATTACH_START) {
                break;
            }

            ItemMenuMode.overflow_count[2]++;
        }

        for (int i = 0; i < 3; i++) {
            if (ItemMenuMode.overflow_count[i] > 0) {
                ItemMenuMode.overflow_pages |= 1 << (i + 1);
            }
        }

        for (int i = 2; i >= 0; i--) {
            if (ItemMenuMode.overflow_pages & (1 << (i + 1))) {
                ItemMenuMode.board.page = i;
            }
        }

        if (ItemMenuMode.board.page == BOARD_PAGE_WEAPON) {
            ItemMenuMode.board.cursor = WhoIsWeaponEquip(thrown[3]) * 10;
            ItemMenuMode.board.top_row = ItemMenuMode.board.cursor / 5;

            ItemMenuMode.board.top_row = std::max(ItemMenuMode.board.top_row, 0);

            ItemMenuMode.board.top_row = std::min(ItemMenuMode.board.top_row, 9);
        }

        ItemMenuMode.overflow = true;
        BtlMenuStatusPt->overflow_flag = true;
    }

    ItemMenuMode.board.y = 127 - ItemMenuMode.board.top_row * 40;
    int pages = max / 5;
    ItemMenuMode.board.scroll = 140.0f + 114.0f * ItemMenuMode.board.top_row / pages;
    ItemMenuMode.board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
    ItemMenuMode.state = ITEM_MENU_STATE_LOADING;
    ItemMenuMode.counter = 0;
    u_long128 *buffer = BtlMenuReadBuf;
    int        size = 0;
    StartBGReadItemMenuWepIcon(buffer, size);
    buffer += (size >> 4) + 1;
    ItemMenuCharaReadBuf = MenuCalcBufAlignment(buffer);
    BtlHaveItemPt = &ItemMenuMode.board.held_item;
    StartLoadCharaMDS(ItemMenuCharaReadBuf, chara, 1);
    MenuExTextureReadFlag = MENU_EX_TEX_UNREAD;
    memset(&IconAutoGet, -1, sizeof(IconAutoGet));
    CommonMenuMes3.Preset(MES_PRESET_NAME);
    CommonMenuMes3.mes_made = -1;
    CommonMenuMes3.rows = 1;
    CommonMenuMes3.char_width = 12;
    CommonMenuMes1.mes_made = -1;
    AtoraNameMes.value_signed = false;
    AtoraNameMes.Preset(MES_PRESET_SYSTEM);
    AtoraNameMes.mes_made = -1;
    AtoraNameMes.rows = 3;

    for (int i = 0; i < 10; i++) {
        AtoraNameMes.mes_no[i] = 0;
        AtoraNameMes.values[i] = 0;
        SetLinePos(&AtoraNameMes, i, -1, -1);
    }

    AtoraNameMes.char_width = GetMenuCommonFontW(BtlMenuNowLang, -1);
    int             member = ItemMenuMode.chara;
    CDngStatusData *status = BtlMenuStatusPt;
    int             slot = status->equipped_weapon_slot[member];
    WEAPON_HAVE    *row = status->chara_weapons[member];
    WEAPON_HAVE    *weapon = &row[slot];

    if (weapon != nullptr) {
        SetNowEquipWeaponDataForMsg(weapon->item_no, weapon->level);
    } else {
        SetNowEquipWeaponDataForMsg(0, 0);
    }
}

namespace {

/**
 * Waits for the item page's data and opens it on the throw-away mode.
 */
void InitItemTrushStart() {
    if (ReadBGSync() == 0) {
        BattleMenuTexEnter();

        if (BtlMenuMode == BATTLE_MENU_MODE_TOWN) {
            BtlMenuStatusPt->cur_chara = 0;
        }

        Btl_InitItemMode(0, BtlMenuStatusPt->cur_chara);
        BattleMenuFlag = MenuSelect[0] + BTLMENU_STATE_OPEN_OFFSET;
        BtlEffectFlag = BTLEFFECT_PAGE_OPEN;
        BtlEffectCt = 0.0f;
    }
}

} // namespace

void ExistItemMenu() {
    int blocks[3] = {0, 0, -1};

    blocks[0] = MenuExtendReadBlock;
    blocks[1] = ItemMenuWeaponIconReadBlock;
    MenuTextureDelete(blocks);
    TexManager.CleanUpTextureList();
    BtlMenuTexBlockEnter();

    for (int i = 0; i < 6; i++) {
        int slot = BtlMenuStatusPt->equipped_weapon_slot[i];

        if (0 > slot || slot >= 10) {
            EquipDefaultWeapon(i);
        }
    }

    ForBackMenu();
}

namespace {

/**
 * Moves the item page from one party member to the next.
 */
void ChangeMenuChara() {
    if (MenuWepLevelUp.operation_kind < WEP_EFFECT_CURE_HEAL_HP || MenuWepLevelUp.operation_kind > WEP_EFFECT_CURE_POCKET) {
        int previous = ItemMenuMode.chara;
        int party_size = BtlMenuStatusPt->party_size;

        if (GamePad.Down(PAD_L2 | PAD_L1) != 0) {
            ItemMenuMode.chara--;

            if (ItemMenuMode.chara < 0) {
                ItemMenuMode.chara = party_size - 1;
            }
        }

        if (GamePad.Down(PAD_R2 | PAD_R1) != 0) {
            ItemMenuMode.chara++;

            if (party_size - 1 < ItemMenuMode.chara) {
                ItemMenuMode.chara = 0;
            }
        }

        if (previous != ItemMenuMode.chara) {
            if (ItemMenuMode.chara <= BtlMenuStatusPt->party_size) {
                if (ReadBGSync() != 0) {
                    BreakReadBG();
                }

                MenuExTextureReadFlag = StartLoadCharaMDS(ItemMenuCharaReadBuf, ItemMenuMode.chara, 0);
                int             chara = ItemMenuMode.chara;
                CDngStatusData *status = BtlMenuStatusPt;
                int             slot = status->equipped_weapon_slot[chara];
                WEAPON_HAVE    *row = status->chara_weapons[chara];
                WEAPON_HAVE    *weapon = &row[slot];

                if (weapon != nullptr) {
                    SetNowEquipWeaponDataForMsg(weapon->item_no, weapon->level);
                } else {
                    SetNowEquipWeaponDataForMsg(0, 0);
                }
            } else {
                MenuExTextureReadFlag = MENU_EX_TEX_BUILDING;
            }
        }
    }
}

/**
 * Returns a party member's current life from the battle menu's status data.
 */
int DebugGetHp(int chara) {
    return BtlMenuStatusPt->hp[chara];
}

/**
 * Returns a party member's maximum life from the battle menu's status data.
 */
int DebugGetMaxHp(int chara) {
    return BtlMenuStatusPt->max_hp[chara];
}

/**
 * Sets a party member's current life in the battle menu's status data.
 */
void DebugSetHp(int chara, int hp) {
    BtlMenuStatusPt->hp[chara] = hp;
}

/**
 * Returns the water a party member holds from the battle menu's status data.
 */
float DebugGetWater(int chara) {
    return BtlMenuStatusPt->water_now[chara];
}

/**
 * Sets the water a party member holds in the battle menu's status data.
 */
void DebugSetWater(int chara, float water) {
    BtlMenuStatusPt->water_now[chara] = water;
}

/**
 * Returns a party member's defense from the battle menu's status data.
 */
int DebugGetDefense(int chara) {
    return BtlMenuStatusPt->defense[chara];
}

/**
 * Sets a party member's defense in the battle menu's status data.
 */
void DebugSetDefense(int chara, int defense) {
    BtlMenuStatusPt->defense[chara] = defense;
}

} // namespace

/**
 * Handles item-menu input, item use, equipment changes, and inventory transfers.
 */
int ItemMenuMainKey() {
    float effect_x = 0.0f;
    float effect_y = 0.0f;

    switch (MenuExTextureReadFlag) {
        case MENU_EX_TEX_UNREAD:
            if (ReadBGSync() == 0) {
                if (ItemMenuAlreadyReadWepIconTexFlag == 0) {
                    ReadSyncItemMenuWepIcon();
                    ItemMenuAlreadyReadWepIconTexFlag = 1;
                    ItemMenuMode.state = ITEM_MENU_STATE_OPEN;
                }

                MenuCharaMDSBuild2(ItemMenuMode.chara, MenuExtendReadBlock);
                MenuExTextureReadFlag = MENU_EX_TEX_BUILDING;
                BtlMDSBuildCnt = 0;
            }

            break;
        case MENU_EX_TEX_BUILDING:
            BtlMDSBuildCnt++;
            MenuCharaFrame.Step();

            if (ItemMenuMode.chara == 0) {
                MenuCharaFrame.ClothStep(0);
            }

            if (BtlMDSBuildCnt > 3) {
                MenuExTextureReadFlag = MENU_EX_TEX_READY;
            }

            break;
        default:
            break;
    }

    int old_mode = ItemMenuMode.mode;
    int old_cursor = ItemMenuMode.board.cursor;
    int old_chara = ItemMenuMode.chara;
    int old_page = ItemMenuMode.board.page;
    int thrown[18];
    int thrown_vol[18];

    for (int i = 0; i < 18; i++) {
        thrown[i] = 0;
        thrown_vol[i] = 0;
    }

    int result = CheckItemThrow(thrown, thrown_vol);
    int overflowing = 0;

    for (int i = 0; i < 3; i++) {
        int maximum = 0;
        GetNowModeMaxNum(i, &maximum);

        if (maximum != 0) {
            overflowing = 1;
        }
    }

    if (IconAutoGet.IsMoveIcon() == 0 && result == 0 && overflowing == 0 && BtlHaveItemPt->item_no < ITEM_ATTACH_START) {
        ItemMenuMode.overflow = false;
        static_cast<CUserStatus *>(BtlMenuStatusPt)->overflow_flag = false;
        DeleteMenuTrushMark();
    }

    switch (ItemMenuMode.state) {
        case ITEM_MENU_STATE_LOADING:
            ToFromSelect(0);
            break;
        case ITEM_MENU_STATE_OPEN:
            result = ToFromSelect(0);
            ItemMenuMode.counter++;

            if (ItemMenuMode.counter >= 0x11 && result != 0) {
                BtlEffectFlag = BTLEFFECT_NONE;
                ItemMenuMode.state = ITEM_MENU_STATE_IDLE;
                ItemMenuMode.counter = 0;
                BattleMenuFlag = BTLMENU_STATE_ITEM;
            }

            break;
        case ITEM_MENU_STATE_CLOSE:
            result = ToFromSelect(1);

            if (BtlMenuExReadFlag == 0) {
                BtlMenuExReadFlag = DngActItemModelBuild(0);
            }

            ItemMenuMode.counter++;

            if (ItemMenuMode.counter > 0x11 && result != 0 && BtlMenuExReadFlag != 0) {
                ExistItemMenu();
                BattleMenuFlag = BTLMENU_STATE_MAIN;
            }

            break;
        case ITEM_MENU_STATE_EFFECT: {
            int motion = MenuWepLevelUp.effect.motion_state;
            int state = MenuWepLevelUp.effect_state;

            if (state == WEP_EFFECT_STATE_CURE_STATUS_PLAY || state == WEP_EFFECT_STATE_CURE_FRUIT_OF_EDEN_PLAY || state == WEP_EFFECT_STATE_CURE_GOURD_PLAY || state == WEP_EFFECT_STATE_CURE_POCKET_PLAY || state == 0x1Fu || state == WEP_EFFECT_STATE_CURE_REVIVAL_POWDER_PLAY || (state == WEP_EFFECT_STATE_CURE_AILMENT_PLAY && MenuWepLevelUp.message_no > 0x190)) {
                if (motion == 3 && GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
                    ItemMenuMode.state = ITEM_MENU_STATE_IDLE;
                    MenuWepLevelUp.SetSnd(MenuWepLevelUp.snd_volume, MenuWepLevelUp.snd_from, 7);
                    MenuWepLevelUp.Initialize();
                }
            } else if (state == WEP_EFFECT_STATE_CURE_HEAL_HP_PLAY || state == WEP_EFFECT_STATE_CURE_DRINK_PLAY || (state == WEP_EFFECT_STATE_CURE_AILMENT_PLAY && MenuWepLevelUp.message_no < 0x190) || state == WEP_EFFECT_STATE_CURE_REPAIR_PLAY) {
                if (motion == 3) {
                    ItemMenuMode.state = ITEM_MENU_STATE_IDLE;
                    MenuWepLevelUp.Initialize();
                }
            }

            break;
        }
        case ITEM_MENU_STATE_POWERUP_CONFIRM:
            if (GamePad.Down(PAD_UP | PAD_DOWN) != 0) {
                if (ItemMenuMode.confirm != 0) {
                    ItemMenuMode.confirm = 0;
                } else {
                    ItemMenuMode.confirm = 1;
                }

                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                if (ItemMenuMode.confirm == 0) {
                    WEAPON_HAVE *weapon = ItemMenuMode.target_weapon;
                    s16          item_no = BtlHaveItemPt->item_no;
                    int          result = ItemUseFunc(BtlMenuStatusPt, item_no, ItemMenuMode.chara, ITEM_USE_ON_EQUIP_WEAPON, weapon);

                    if (static_cast<u32>(result - 1) < 2) {
                        int sounds[2] = {1, 0x18};
                        ComMenuSePlay(sounds[result - 1]);
                    }

                    if (ItemMenuMode.use_target == ITEM_USE_ON_EQUIP_WEAPON) {
                        effect_x = -16.95f;
                        effect_y = 1.0f;
                    }

                    if (ItemMenuMode.use_target == ITEM_USE_ON_BOARD_WEAPON) {
                        effect_x = 4.3f + 3.2f * static_cast<float>(ItemMenuMode.board.cursor % 5);
                        effect_y = 6.7f - 3.2f * static_cast<float>(ItemMenuMode.board.cursor / 5 - ItemMenuMode.board.top_row);
                    }

                    auto *effect = reinterpret_cast<CWeaponLevelUp *>(MenuExCashBuffer.base + MenuExCashBuffer.used * 16);
                    MenuWepLevelUp.CureEffect(static_cast<int>(effect_x), static_cast<int>(effect_y), effect, BtlMenuExReadBlock, WEP_EFFECT_CURE_REPAIR);
                    InitHaveData(BtlHaveItemPt);
                    ItemMenuMode.state = ITEM_MENU_STATE_IDLE;
                    ComMenuSePlay(SE_WEAPON_REPAIR);
                } else {
                    ItemMenuMode.state = ITEM_MENU_STATE_IDLE;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                ItemMenuMode.state = ITEM_MENU_STATE_IDLE;
                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        case ITEM_MENU_STATE_IDLE:
            switch (ItemMenuMode.mode) {
                case ITEM_MENU_QUICK_SLOTS:
                case ITEM_MENU_CHARA:
                case ITEM_MENU_WEAPON:
                case ITEM_MENU_DEFENSE:
                    ChangeMenuChara();
                case ITEM_MENU_TRUSH:
                    ItemMenuModeKey();
                    break;
                case ITEM_MENU_BOARD: {
                    int done = PersonalBoardKey();
                    int row = ItemMenuMode.board.cursor / 5 - ItemMenuMode.board.top_row;

                    if (done != 0) {
                        if (row < 2) {
                            ItemMenuMode.mode = ITEM_MENU_QUICK_SLOTS;
                            ItemMenuMode.board.cursor = 2;
                        } else {
                            ItemMenuMode.mode = ITEM_MENU_CHARA;
                        }

                        int page = ItemMenuMode.board.page;
                        int count = ItemMenuMode.overflow_count[page];

                        if ((ItemMenuMode.overflow_pages & (1 << (page + 1))) && ItemMenuMode.overflow != 0) {
                            ItemMenuMode.mode = ITEM_MENU_TRUSH;
                            ItemMenuMode.board.cursor = row;

                            ItemMenuMode.board.cursor = std::min(ItemMenuMode.board.cursor, count - 1);

                            ItemMenuMode.board.cursor = std::max(ItemMenuMode.board.cursor, 0);
                        }
                    }

                    break;
                }
                default:
                    break;
            }

            if (MenuExTextureReadFlag >= MENU_EX_TEX_READY && MenuWepLevelUp.operation_kind == WEP_EFFECT_NONE) {
                if (GamePad.Down(PAD_CROSS) != 0) {
                    int            cursor = ItemMenuMode.board.cursor;
                    COM_ITEM_INFO *info = GetCommonItemInfo(BtlHaveItemPt->item_no);

                    switch (ItemMenuMode.mode) {
                        case ITEM_MENU_QUICK_SLOTS: {
                            int held = BtlHaveItemPt->item_no;

                            if ((held >= ITEM_ATTACH_START && held < ITEM_DUNGEON_START) || held >= ITEM_WEAPON_START) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else if (IconAutoGet.IsMoveIcon() != 0) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else {
                                s16       *slot_item = &MenuItemPackPt->quick_item_slot[cursor];
                                s16       *slot_qty = &MenuItemPackPt->quick_item_qty[cursor];
                                ITEM_DATA *slot_data = nullptr;

                                if (*slot_item >= ITEM_DUNGEON_START) {
                                    slot_data = GetItemData(*slot_item);
                                }

                                int        held_kind = 0;
                                ITEM_DATA *held_data = nullptr;

                                if (BtlHaveItemPt->item_no >= ITEM_DUNGEON_START) {
                                    held_data = GetItemData(BtlHaveItemPt->item_no);
                                    held_kind = held_data->stack_kind;
                                }

                                int stack = 0;
                                int ok = 0;
                                int take = 0;

                                switch (held_kind) {
                                    case ITEM_STACK_NEVER:
                                        break;
                                    case ITEM_STACK_SINGLE:
                                        if (slot_data == nullptr) {
                                            ok = 1;
                                        } else {
                                            switch (slot_data->stack_kind) {
                                                case ITEM_STACK_SINGLE:
                                                    ok = 1;
                                                    break;
                                                case ITEM_STACK_MULTI:
                                                    ok = 1;

                                                    if (*slot_qty >= 2) {
                                                        ok = 0;
                                                    }

                                                    break;
                                                default:
                                                    break;
                                            }
                                        }

                                        break;
                                    case ITEM_STACK_MULTI:
                                        if (slot_data == nullptr) {
                                            ok = 1;
                                        } else {
                                            ok = 1;

                                            if (slot_data->stack_kind == ITEM_STACK_MULTI) {
                                                if (*slot_item == BtlHaveItemPt->item_no) {
                                                    if (*slot_qty < 9) {
                                                        stack = 1;
                                                    } else {
                                                        ok = 0;
                                                    }
                                                } else if (held_data == nullptr) {
                                                    take = 1;
                                                } else if (*slot_qty >= 2) {
                                                    ok = 0;
                                                }
                                            }
                                        }

                                        break;
                                    default:
                                        break;
                                }

                                if (ok == 0) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else {
                                    if (stack != 0) {
                                        if (*slot_qty == 0) {
                                            *slot_qty = 1;
                                        } else {
                                            (*slot_qty)++;
                                        }

                                        InitHaveData(BtlHaveItemPt);
                                    } else if (take != 0) {
                                        BtlHaveItemPt->item_no = *slot_item;
                                        BtlHaveItemPt->slot_state = ITEM_MENU_QUICK_SLOTS;
                                        BtlHaveItemPt->from_slot = cursor;
                                        (*slot_qty)--;

                                        if (*slot_qty <= 0) {
                                            *slot_item = -1;
                                            *slot_qty = 0;
                                        }
                                    } else if (ok != 0) {
                                        MenuDataSwap(slot_item, &BtlHaveItemPt->item_no);
                                        auto *status = static_cast<CUserStatus *>(BtlMenuStatusPt);
                                        int   vol = status->active_item_vol[cursor];
                                        status->active_item_vol[cursor] = BtlHaveItemPt->volume;
                                        BtlHaveItemPt->volume = vol;

                                        if (*slot_item >= ITEM_DUNGEON_START) {
                                            *slot_qty = 1;
                                        } else {
                                            *slot_qty = 0;
                                        }

                                        if (BtlHaveItemPt->item_no >= ITEM_DUNGEON_START) {
                                            BtlHaveItemPt->slot_state = ITEM_MENU_QUICK_SLOTS;
                                            BtlHaveItemPt->from_slot = cursor;
                                        }
                                    }

                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                }
                            }

                            break;
                        }
                        case ITEM_MENU_CHARA:
                            if (info == nullptr) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else if (info->kind != ITEMKIND_ITEM) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else {
                                int chara = ItemMenuMode.chara;
                                int result = ItemUseFunc(BtlMenuStatusPt, BtlHaveItemPt->item_no, chara, ITEM_USE_ON_CHARA, nullptr);
                                int sounds[4] = {2, 1, 0x18, -1};
                                int sound = 0;

                                if (result < 4) {
                                    sound = sounds[result];
                                } else {
                                    sound = 2;
                                }

                                if (sound == 2) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else {
                                    ITEM_DATA *data = GetItemData(BtlHaveItemPt->item_no);
                                    int        effect = WEP_EFFECT_NONE;

                                    if (sound > 0) {
                                        u32 use = data->use_flags;

                                        if ((use & 0x80) && (data->kind_flags & 4)) {
                                            effect = WEP_EFFECT_CURE_DRINK;
                                            effect_x = -5.0f;
                                            effect_y = -3.0f;
                                        }

                                        if ((use & 0x40) && (data->kind_flags & 4)) {
                                            effect = WEP_EFFECT_CURE_HEAL_HP;
                                            effect_x = -5.5f;
                                            effect_y = -6.0f;
                                            ComMenuSePlay(SE_EFFECT_CHIME);
                                        }

                                        if ((data->use_flags & 0x20) && (data->kind_flags & 0x20)) {
                                            effect = WEP_EFFECT_CURE_STATUS;
                                            effect_x = -5.5f;
                                            effect_y = -6.0f;
                                            int volume = GetMenuItemUseVolume();
                                            MenuWepLevelUp.message_no = ItemMenuMode.chara;
                                            MenuWepLevelUp.message_value = volume;
                                        }

                                        if ((data->kind_flags & 1) && ((data->use_flags & 0x4000) || (data->use_flags & 0x8000) || (data->use_flags & 0x10000) || (data->use_flags & 0x20000) || (data->use_flags & 0x3C000))) {
                                            int message = 0;

                                            switch (BtlHaveItemPt->item_no) {
                                                case ITEM_ANTIDOTE_DRINK:
                                                    message = 0x1A2;
                                                    break;
                                                case ITEM_HOLY_WATER:
                                                    message = 0x1A3;
                                                    break;
                                                case ITEM_SOAP:
                                                    message = 0x1A4;
                                                    break;
                                                case ITEM_MIGHTY_HEALING:
                                                    message = 0x1A5;
                                                    break;
                                                default:
                                                    message = -1;
                                                    break;
                                            }

                                            MenuWepLevelUp.message_no = message;
                                            MenuWepLevelUp.message_value = 0;
                                            effect = WEP_EFFECT_CURE_AILMENT;
                                            effect_x = -6.0f;
                                            effect_y = -2.0f;
                                            ComMenuSePlay(SE_EFFECT_CHIME);
                                        }

                                        if ((data->kind_flags & 1) && (data->use_flags & 0x1000)) {
                                            effect = WEP_EFFECT_CURE_AILMENT;

                                            if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) {
                                                ComMenuSePlay(SE_POWER_UP);
                                            }

                                            effect_x = -6.0f;
                                            effect_y = -2.0f;
                                        }

                                        if (BtlHaveItemPt->item_no == ITEM_POCKET) {
                                            effect = WEP_EFFECT_CURE_POCKET;
                                            effect_x = 19.03f;
                                            effect_y = 9.1f;
                                            int volume = GetMenuItemUseVolume();
                                            MenuWepLevelUp.message_no = ItemMenuMode.chara;
                                            MenuWepLevelUp.message_value = volume;
                                        }

                                        if (BtlHaveItemPt->item_no == ITEM_FRUIT_OF_EDEN) {
                                            effect = WEP_EFFECT_CURE_FRUIT_OF_EDEN;
                                            effect_x = -5.5f;
                                            effect_y = -6.0f;
                                            int volume = GetMenuItemUseVolume();
                                            MenuWepLevelUp.message_no = ItemMenuMode.chara;
                                            MenuWepLevelUp.message_value = volume;
                                        }

                                        if (BtlHaveItemPt->item_no == ITEM_GOURD) {
                                            effect = WEP_EFFECT_CURE_GOURD;
                                            effect_x = -5.5f;
                                            effect_y = -6.0f;
                                            int volume = GetMenuItemUseVolume();
                                            MenuWepLevelUp.message_no = ItemMenuMode.chara;
                                            MenuWepLevelUp.message_value = volume;
                                        }

                                        if (BtlHaveItemPt->item_no == ITEM_REVIVAL_POWDER) {
                                            effect = WEP_EFFECT_CURE_REVIVAL_POWDER;
                                            effect_x = -5.5f;
                                            effect_y = -2.0f;
                                            int volume = GetMenuItemUseVolume();
                                            MenuWepLevelUp.message_no = ItemMenuMode.chara;
                                            MenuWepLevelUp.message_value = volume;
                                        }

                                        if (effect > 0) {
                                            ItemMenuMode.state = ITEM_MENU_STATE_EFFECT;
                                            auto *buffer = reinterpret_cast<CWeaponLevelUp *>(MenuExCashBuffer.base + MenuExCashBuffer.used * 16);
                                            MenuWepLevelUp.CureEffect(static_cast<int>(effect_x), static_cast<int>(effect_y), buffer, BtlMenuExReadBlock, effect);
                                        }

                                        InitHaveData(BtlHaveItemPt);
                                    }
                                }
                            }

                            break;
                        case ITEM_MENU_WEAPON: {
                            int          chara = ItemMenuMode.chara;
                            int          slot = BtlMenuStatusPt->equipped_weapon_slot[chara];
                            WEAPON_HAVE *row = BtlMenuStatusPt->chara_weapons[chara];
                            ItemMenuMode.target_weapon = &row[slot];
                            int          is_default = 0;
                            WEAPON_HAVE *weapon = ItemMenuMode.target_weapon;
                            int          before = weapon->item_no;

                            if (before == GetDefaultWeaponNo(ItemMenuMode.chara)) {
                                is_default = 1;
                            }

                            int held = BtlHaveItemPt->item_no;

                            if (held == ITEM_POWERUP_POWDER && is_default == 0) {
                                WEAPON_HAVE *target = ItemMenuMode.target_weapon;

                                if (target->experience >= GetWeaponMaxExp(weapon)) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else if (target->level >= 99) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else if (target->item_no == ITEM_WEAPON_SERPENT_SWORD && GetMenuHebikiriFlag() == 0) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else {
                                    ItemMenuMode.state = ITEM_MENU_STATE_POWERUP_CONFIRM;
                                    ItemMenuMode.use_target = ITEM_USE_ON_EQUIP_WEAPON;
                                    ItemMenuMode.confirm = 1;
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                }
                            } else if (held != ITEM_POWERUP_POWDER && held != ITEM_REPAIR_POWDER) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else {
                                int result = ItemUseFunc(BtlMenuStatusPt, held, ItemMenuMode.chara, ITEM_USE_ON_BOARD_WEAPON, weapon);
                                int after = ItemMenuMode.target_weapon->item_no;

                                if (static_cast<u32>(result - 1) < 2) {
                                    s16 sounds[2] = {1, 0x18};
                                    ComMenuSePlay(sounds[result - 1]);
                                    MenuWepLevelUp.CureEffect(-0x10, 1, reinterpret_cast<CWeaponLevelUp *>(MenuExCashBuffer.base + MenuExCashBuffer.used * 16), BtlMenuExReadBlock, WEP_EFFECT_CURE_REPAIR);

                                    if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) {
                                        if (before != after) {
                                            int who = static_cast<CUserStatus *>(BtlMenuStatusPt)->cur_chara;

                                            if (ItemMenuMode.chara == who) {
                                                EquipWeaponFrame(&DefaultWeapon, ItemMenuMode.chara, CharaMainHandViewFlag);

                                                if (static_cast<CUserStatus *>(BtlMenuStatusPt)->cur_chara == CHARA_OSMOND) {
                                                    NowMainEffect = &CharaMainEffectCrash;
                                                    BtActStatus.gun_type = 0;
                                                }
                                            }
                                        }

                                        MenuWeaponSpSet(&MainWeapon, ItemMenuMode.target_weapon);
                                    }

                                    SetNowEquipWeaponDataForMsg(ItemMenuMode.target_weapon->item_no, ItemMenuMode.target_weapon->level);
                                    InitHaveData(BtlHaveItemPt);
                                } else {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                }
                            }

                            break;
                        }
                        case ITEM_MENU_DEFENSE:
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                            break;
                        case ITEM_MENU_BOARD: {
                            s16             held = BtlHaveItemPt->item_no;
                            PERSONAL_BOARD *board = &ItemMenuMode.board;

                            if (board->cursor_area == PERSONAL_BOARD_AREA_TRASH) {
                                if (held < ITEM_ATTACH_START) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else if (IsEnableTrushThrow(held) != 0) {
                                    board->trash_anim = 1;
                                    board->trash_frame = 0;
                                    InitHaveData(BtlHaveItemPt);
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                } else {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                }
                            } else {
                                int before = -1;
                                int item = held;

                                if (ItemMenuMode.board.page == BOARD_PAGE_WEAPON) {
                                    int             chara = ItemMenuMode.board.cursor / 10;
                                    int             slot = ItemMenuMode.board.cursor % 10;
                                    CDngStatusData *status = BtlMenuStatusPt;
                                    WEAPON_HAVE    *row = status->chara_weapons[chara];
                                    ItemMenuMode.target_weapon = &row[slot];
                                    before = ItemMenuMode.target_weapon->item_no;
                                }

                                int result = PersonalBoardItemGetorSwap(ItemMenuMode.board.cursor);

                                if (result == 0) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else if (result == 1) {
                                    if (BtlHaveItemPt->item_no >= ITEM_ATTACH_START) {
                                        BtlHaveItemPt->slot_state = ITEM_MENU_BOARD;
                                    } else {
                                        InitHaveData(BtlHaveItemPt);
                                        InitHaveWep(&ItemMenuMode.board.weapon);
                                        InitHaveAttach(&ItemMenuMode.board.held_attach);
                                    }

                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                } else if (result == 2) {
                                    float x = 4.3f + 3.2f * static_cast<float>(ItemMenuMode.board.cursor % 5);
                                    float y = 6.7f - 3.2f * static_cast<float>(ItemMenuMode.board.cursor / 5 - ItemMenuMode.board.top_row);

                                    if (item == ITEM_REPAIR_POWDER) {
                                        auto *effect = reinterpret_cast<CWeaponLevelUp *>(MenuExCashBuffer.base + MenuExCashBuffer.used * 16);
                                        MenuWepLevelUp.CureEffect(static_cast<int>(x), static_cast<int>(y), effect, BtlMenuExReadBlock, WEP_EFFECT_CURE_REPAIR);
                                    }

                                    if (ItemMenuMode.board.page == BOARD_PAGE_WEAPON) {
                                        if (item == ITEM_REPAIR_POWDER) {
                                            int after = ItemMenuMode.target_weapon->item_no;

                                            if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) {
                                                if (after != before) {
                                                    EquipWeaponFrame(&DefaultWeapon, ItemMenuMode.chara, CharaMainHandViewFlag);
                                                }

                                                MenuWeaponSpSet(&MainWeapon, ItemMenuMode.target_weapon);
                                            }

                                            ComMenuSePlay(SE_WEAPON_REPAIR);
                                        }

                                        if (item == ITEM_POWERUP_POWDER && before != GetDefaultWeaponNo(ItemMenuMode.board.cursor / 10)) {
                                            ItemMenuMode.state = ITEM_MENU_STATE_POWERUP_CONFIRM;
                                            ItemMenuMode.use_target = ITEM_USE_ON_BOARD_WEAPON;
                                            ItemMenuMode.confirm = 1;
                                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                                        }
                                    }
                                } else if (result == 1) {
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                } else {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                }
                            }

                            break;
                        }
                        case ITEM_MENU_TRUSH:
                            if (ItemTrushKey(thrown, thrown_vol, ItemMenuMode.board.cursor) != 0) {
                                BtlHaveItemPt->slot_state = ITEM_MENU_TRUSH;
                                BtlHaveItemPt->from_slot = ItemMenuMode.board.cursor;
                                ComMenuSePlay(MENU_SOUND_CONFIRM);
                            } else {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            }

                            break;
                        default:
                            break;
                    }
                } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);

                    if (BtlHaveItemPt->item_no < ITEM_ATTACH_START && IconAutoGet.IsMoveIcon() == 0 && ItemMenuMode.overflow == 0) {
                        BattleMenuFlag = BTLMENU_STATE_ITEM_CLOSE;
                        u_long128 *buffer = BtlMenuReadBuf;

                        switch (BtlMenuMode) {
                            case BATTLE_MENU_MODE_DUNGEON:
                                BtlMenuExReadFlag = DngActItemModelReadStart(buffer);
                                break;
                            default:
                                break;
                        }

                        ExitItemSelect();
                        ItemMenuMode.state = ITEM_MENU_STATE_CLOSE;
                        ItemMenuMode.counter = 0;
                    } else if (IconAutoGet.IsMoveIcon() != 0) {
                        ComMenuSePlay(MENU_SOUND_REFUSE);
                        break;
                    } else {
                        s16 *slot_item = nullptr;
                        s16 *slot_qty = nullptr;
                        s16  vol = 0;
                        s16  item_no = 0;
                        s16  held = BtlHaveItemPt->item_no;

                        switch (BtlHaveItemPt->slot_state) {
                            case ITEM_MENU_TRUSH:
                                switch (WhatIsKindofItem(held)) {
                                    case BOARD_PAGE_ITEM:
                                        MenuDataSwap(&BtlHaveItemPt->item_no, &MenuItemPackPt->item[MenuItemPackPt->num + BtlHaveItemPt->from_slot]);
                                        MenuDataSwap(&BtlHaveItemPt->volume, &MenuItemPackPt->item_vol[MenuItemPackPt->num + BtlHaveItemPt->from_slot]);
                                        break;
                                    case BOARD_PAGE_WEAPON: {
                                        int             who = WhoIsWeaponEquip(held);
                                        CDngStatusData *status = BtlMenuStatusPt;
                                        WEAPON_HAVE    *weapons = status->chara_weapons[who];
                                        WEAPON_HAVE    *weapon = &weapons[10];
                                        item_no = weapons[10].item_no;
                                        MenuDataSwap(weapon, &ItemMenuMode.board.weapon);
                                        BtlHaveItemPt->item_no = item_no;
                                        weapon->item_no = held;

                                        if (BtlHaveItemPt->item_no < ITEM_WEAPON_START) {
                                            InitHaveData(BtlHaveItemPt);
                                            InitHaveWep(&ItemMenuMode.board.weapon);
                                        }

                                        break;
                                    }
                                    case BOARD_PAGE_ATTACH: {
                                        ATTACH_LIST *attach = &(reinterpret_cast<ATTACH_LIST *>(BtlMenuStatusPt->consumable_items))[BtlHaveItemPt->from_slot];
                                        item_no = attach->item_no;
                                        MenuDataSwap(attach, &ItemMenuMode.board.held_attach);
                                        BtlHaveItemPt->item_no = item_no;
                                        attach->item_no = held;
                                        break;
                                    }
                                    default:
                                        break;
                                }

                                break;
                            case ITEM_MENU_BOARD:
                                PersonalBoardItemCancel();
                                break;
                            case ITEM_MENU_QUICK_SLOTS: {
                                int slot = BtlHaveItemPt->from_slot;
                                slot_item = &MenuItemPackPt->quick_item_slot[slot];
                                slot_qty = &MenuItemPackPt->quick_item_qty[slot];
                                auto *status = static_cast<CUserStatus *>(BtlMenuStatusPt);
                                vol = status->active_item_vol[slot];

                                if (*slot_qty < 2 && *slot_item != held && held >= ITEM_DUNGEON_START) {
                                    MenuDataSwap(slot_item, &BtlHaveItemPt->item_no);
                                    int held_vol = BtlHaveItemPt->volume;

                                    held_vol = std::max(held_vol, 0);

                                    status = static_cast<CUserStatus *>(BtlMenuStatusPt);
                                    status->active_item_vol[slot] = held_vol;
                                    BtlHaveItemPt->volume = vol;
                                    *slot_qty = 1;
                                } else if (*slot_item == held && held >= ITEM_DUNGEON_START && *slot_qty < 9) {
                                    switch (GetItemData(held)->stack_kind) {
                                        case ITEM_STACK_SINGLE: {
                                            MenuDataSwap(slot_item, &BtlHaveItemPt->item_no);
                                            int held_vol = BtlHaveItemPt->volume;

                                            held_vol = std::max(held_vol, 0);

                                            status = static_cast<CUserStatus *>(BtlMenuStatusPt);
                                            status->active_item_vol[slot] = held_vol;
                                            BtlHaveItemPt->volume = vol;
                                            *slot_qty = 1;
                                            break;
                                        }
                                        case ITEM_STACK_MULTI:
                                            (*slot_qty)++;
                                            InitHaveData(BtlHaveItemPt);
                                            break;
                                        default:
                                            break;
                                    }

                                    if (BtlHaveItemPt->item_no < ITEM_ATTACH_START) {
                                        InitHaveData(BtlHaveItemPt);
                                    }
                                }

                                break;
                            }
                            default:
                                break;
                        }
                    }
                } else if (GamePad.Down(PAD_SQUARE) != 0) {
                    switch (ItemMenuMode.mode) {
                        case ITEM_MENU_QUICK_SLOTS: {
                            int        cursor = ItemMenuMode.board.cursor;
                            ITEM_PACK *pack = MenuItemPackPt;
                            s16       *items = pack->item;
                            s16       *vols = pack->item_vol;
                            s16       *slot_item = &pack->quick_item_slot[cursor];
                            s16       *slot_qty = &pack->quick_item_qty[cursor];
                            int        i = 0;
                            auto      *status = static_cast<CUserStatus *>(BtlMenuStatusPt);
                            s16        vol = status->active_item_vol[cursor];

                            if (*slot_item > 0) {
                                for (i = 0; 0 < *slot_qty || i < MenuItemPackPt->num; i++) {
                                    if (items[i] < 0) {
                                        items[i] = *slot_item;
                                        vols[i] = vol;
                                        auto *holder = static_cast<CUserStatus *>(BtlMenuStatusPt);
                                        holder->active_item_vol[cursor] = 0;
                                        (*slot_qty)--;

                                        if (*slot_qty <= 0) {
                                            *slot_item = -1;
                                            break;
                                        }
                                    }
                                }
                            }

                            if (static_cast<CUserStatus *>(BtlMenuStatusPt)->overflow_flag != 0) {
                                DeleteMenuTrushMark();
                                SetMenuTrushMark(reinterpret_cast<ITEM_PACK *>(static_cast<CUserStatus *>(BtlMenuStatusPt)->active_item));
                            }

                            ComMenuSePlay(MENU_SOUND_REFUSE);
                            break;
                        }
                        case ITEM_MENU_BOARD:
                        case ITEM_MENU_DEFENSE:
                        case ITEM_MENU_CHARA:
                            switch (ItemMenuMode.board.page) {
                                case BOARD_PAGE_ITEM:
                                    SeitonItemBoard(MenuItemPackPt);

                                    if (static_cast<CUserStatus *>(BtlMenuStatusPt)->overflow_flag != 0) {
                                        SetMenuTrushMark(reinterpret_cast<ITEM_PACK *>(static_cast<CUserStatus *>(BtlMenuStatusPt)->active_item));
                                    }

                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                    break;
                                case BOARD_PAGE_ATTACH:
                                    SeitonAttachBoard(reinterpret_cast<ATTACH_LIST *>(BtlMenuStatusPt->consumable_items));
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                    break;
                                default:
                                    break;
                            }

                            break;
                        default:
                            break;
                    }
                } else if (GamePad.Down(PAD_TRIANGLE) != 0) {
                    int  cursor = ItemMenuMode.board.cursor;
                    s16 *items = MenuItemPackPt->item;

                    switch (ItemMenuMode.mode) {
                        case ITEM_MENU_QUICK_SLOTS: {
                            s16 *slot_item = &MenuItemPackPt->quick_item_slot[cursor];
                            s16 *slot_qty = &MenuItemPackPt->quick_item_qty[cursor];

                            if (*slot_item < ITEM_DUNGEON_START) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                                break;
                            }

                            ITEM_DATA *data = GetItemData(*slot_item);

                            if (data->stack_kind == ITEM_STACK_NEVER) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                                break;
                            }

                            int moving = IconAutoGet.IsSameItem(*slot_item);

                            if (moving > 0) {
                                ComMenuSePlay(MENU_SOUND_CONFIRM);
                            } else if (data->stack_kind == ITEM_STACK_SINGLE) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else {
                                int space = IconAutoGet.GetSpace();
                                int qty = *slot_qty;
                                int total = qty + moving;

                                if (total >= 9 || space < 0) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                } else {
                                    s16 held = BtlHaveItemPt->item_no;

                                    if (held >= ITEM_DUNGEON_START && held == *slot_item && total == 8) {
                                        *slot_qty = qty + 1;
                                        BtlHaveItemPt->item_no = 0;
                                        ComMenuSePlay(MENU_SOUND_CONFIRM);
                                    } else {
                                        int found = 0;

                                        for (int i = 0; i < MenuItemPackPt->num; i++) {
                                            int want = *slot_item;

                                            if ((want < ITEM_DUNGEON_START || want == items[i]) && (want >= ITEM_DUNGEON_START || held == items[i])) {
                                                int y = static_cast<int>(ItemMenuMode.board.y + static_cast<float>(i / 5 * 0x28));

                                                if (y >= 0x78 && y < 0x119) {
                                                    int x = i % 5 * 0x28 + 0x168;
                                                    int vol = MenuItemPackPt->item_vol[cursor];

                                                    if (want < 0) {
                                                        want = held;
                                                    }

                                                    IconAutoGet.IconMoveTarSet(space, cursor, want, vol, static_cast<float>(x), static_cast<float>(y));
                                                } else {
                                                    if (want <= 0) {
                                                        *slot_item = held;
                                                    }

                                                    (*slot_qty)++;
                                                }

                                                items[i] = -1;
                                                found = 1;
                                                break;
                                            }
                                        }

                                        if (found == 0) {
                                            if (BtlHaveItemPt->item_no > 0 && BtlHaveItemPt->item_no == *slot_item) {
                                                (*slot_qty)++;
                                                BtlHaveItemPt->item_no = 0;
                                                found = 1;
                                            }
                                        }

                                        if (found != 0) {
                                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                                        } else {
                                            ComMenuSePlay(MENU_SOUND_REFUSE);
                                        }
                                    }
                                }
                            }

                            break;
                        }
                        case ITEM_MENU_BOARD:
                            if (ItemMenuMode.board.cursor_area == PERSONAL_BOARD_AREA_TRASH) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else if (ItemMenuMode.board.page == BOARD_PAGE_WEAPON || ItemMenuMode.board.page == BOARD_PAGE_ATTACH) {
                                ComMenuSePlay(MENU_SOUND_REFUSE);
                            } else {
                                int space = IconAutoGet.GetSpace();

                                if (space < 0) {
                                    break;
                                }

                                if (IconAutoGet.IsMoveIcon() != 0) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                    break;
                                }

                                int pos = ItemMenuMode.board.cursor;
                                int item_no = SearchBoardNowPosItemExist(ItemMenuMode.board.page, pos);

                                if (item_no < ITEM_DUNGEON_START) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                    break;
                                }

                                ITEM_DATA *data = GetItemData(item_no);

                                if (data->stack_kind == ITEM_STACK_NEVER) {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                    break;
                                }

                                int vol = MenuItemPackPt->item_vol[ItemMenuMode.board.cursor];
                                int same = 0;
                                int slot = 0;
                                int empty = -1;

                                for (slot = 0; slot < 3; slot++) {
                                    s16 slot_item = MenuItemPackPt->quick_item_slot[slot];
                                    int moving = IconAutoGet.GetMoveIconGole(slot, slot_item);

                                    if ((data->stack_kind == ITEM_STACK_MULTI && slot_item == item_no && MenuItemPackPt->quick_item_qty[slot] + moving < 9) || (data->stack_kind == ITEM_STACK_SINGLE && slot_item < ITEM_DUNGEON_START)) {
                                        same = 1;
                                        break;
                                    }

                                    if (slot_item < ITEM_DUNGEON_START) {
                                        empty = slot;
                                        break;
                                    }
                                }

                                if (same != 0 || empty >= 0) {
                                    int target = 0;

                                    if (same != 0) {
                                        target = slot;

                                        if (GetItemData(item_no)->stack_kind == ITEM_STACK_SINGLE && MenuItemPackPt->quick_item_qty[slot] == 1) {
                                            ComMenuSePlay(MENU_SOUND_REFUSE);
                                            break;
                                        }
                                    } else if (empty >= 0) {
                                        target = empty;
                                    }

                                    int y = static_cast<int>(ItemMenuMode.board.y + static_cast<float>(pos / 5 * 0x28));

                                    if (y >= 0x78 && y < 0x119) {
                                        IconAutoGet.IconMoveTarSet(space, target, item_no, vol, static_cast<float>(pos % 5 * 0x28 + 0x176), static_cast<float>(y));
                                    } else {
                                        MenuItemPackPt->quick_item_qty[target]++;
                                    }

                                    items[pos] = -1;
                                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                                } else {
                                    ComMenuSePlay(MENU_SOUND_REFUSE);
                                }
                            }

                            break;
                        default:
                            break;
                    }
                }
            }

            // Debug builds edit the selected member's life, water and defense and the party's money, and open the
            // debug item list.
            if (DebugMode) {
                switch (ItemMenuMode.mode) {
                    case ITEM_MENU_CHARA: {
                        int hp = DebugGetHp(ItemMenuMode.chara);
                        int max_hp = DebugGetMaxHp(ItemMenuMode.chara);

                        if (GamePad.On2(PAD_CROSS) && hp < max_hp) {
                            hp++;
                        }

                        if (GamePad.On2(PAD_CIRCLE) && hp > 1) {
                            hp--;
                        }

                        if (GamePad.On2(PAD_SQUARE)) {
                            hp = 0;
                        }

                        DebugSetHp(ItemMenuMode.chara, hp);

                        if (GamePad.On2(PAD_TRIANGLE)) {
                            float water = DebugGetWater(ItemMenuMode.chara);

                            if (water > 0.0f) {
                                water -= 1.0f;
                            }

                            DebugSetWater(ItemMenuMode.chara, water);
                        }

                        break;
                    }
                    case ITEM_MENU_DEFENSE: {
                        int defense = DebugGetDefense(ItemMenuMode.chara);

                        if (GamePad.On2(PAD_CROSS) && defense < 999) {
                            defense++;
                        }

                        if (GamePad.On2(PAD_CIRCLE) && 0 < defense) {
                            defense--;
                        }

                        DebugSetDefense(ItemMenuMode.chara, defense);
                        break;
                    }
                    default:
                        break;
                }

                if (GamePad.On2(PAD_SQUARE)) {
                    switch (ItemMenuMode.mode) {
                        case ITEM_MENU_CHARA: {
                            float water = DebugGetWater(ItemMenuMode.chara) - 1.0f;

                            water = std::max(water, 0.0f);

                            DebugSetWater(ItemMenuMode.chara, water);
                            break;
                        }
                        default:
                            break;
                    }
                }

                if (GamePad.On2(PAD_TRIANGLE)) {
                    switch (ItemMenuMode.mode) {
                        case ITEM_MENU_CHARA:
                            break;
                        case ITEM_MENU_DEFENSE: {
                            CDngStatusData *status = BtlMenuStatusPt;
                            int             money = status->money + 1000;

                            if (money >= 0xFFFF) {
                                status->money = 0xFFFF;
                            } else {
                                status->money = money;
                            }

                            break;
                        }
                        default:
                            break;
                    }
                }

                if (GamePad.Down(PAD_SELECT)) {
                    ItemMenuMode.state = ITEM_MENU_STATE_DEBUG_ITEMS;
                    InitDebugItemGet();
                }
            }

            break;
        case ITEM_MENU_STATE_DEBUG_ITEMS:
            if (DebugItemGetKey() == -1) {
                ItemMenuMode.state = ITEM_MENU_STATE_IDLE;
            }

            break;
        default:
            break;
    }

    if (old_mode != ItemMenuMode.mode || old_cursor != ItemMenuMode.board.cursor || old_chara != ItemMenuMode.chara || old_page != ItemMenuMode.board.page) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if (GamePad.Down(PAD_L2 | PAD_R2 | PAD_L1 | PAD_R1) != 0 && ItemMenuMode.mode == ITEM_MENU_CHARA && old_chara == ItemMenuMode.chara) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
    }

    PERSONAL_BOARD *board = &ItemMenuMode.board;
    int             cursor = board->cursor;
    int             chara_mes = -1;
    int             value = -1;
    int             name_mes = -1;

    if (GetCommonItemInfo(BtlHaveItemPt->item_no) != nullptr) {
        chara_mes = -1;
    }

    s16 held = BtlHaveItemPt->item_no;

    if (held >= ITEM_ATTACH_START) {
        if (held == ITEM_ATTACH_SYNTHESIS_SPHERE) {
            name_mes = GetWeaponMsgNo2(ItemMenuMode.board.held_attach.sphere_weapon_no);
        }

        value = GetAttachVolumeForMsg(&ItemMenuMode.board.held_attach);
    }

    int messages[7] = {-1, 0xC1, 0xBF, 0xBD, -1, -1, -1};
    int message = messages[ItemMenuMode.mode];

    switch (ItemMenuMode.mode) {
        case ITEM_MENU_QUICK_SLOTS: {
            s16 *slot = &MenuItemPackPt->quick_item_slot[cursor];

            if (*slot >= ITEM_DUNGEON_START) {
                message = *slot + 0x1F4;
            }

            break;
        }
        case ITEM_MENU_CHARA:
            chara_mes = ItemMenuMode.chara;
            break;
        case ITEM_MENU_WEAPON:
            GetNowEquipWeaponDataForMsg(name_mes, value);
            break;
        case ITEM_MENU_DEFENSE:
            break;
        case ITEM_MENU_TRUSH:
            switch (ItemMenuMode.board.page) {
                case BOARD_PAGE_ITEM: {
                    s16 item_no = MenuItemPackPt->item[MenuItemPackPt->num + ItemMenuMode.board.cursor];

                    if (item_no >= ITEM_DUNGEON_START) {
                        message = item_no + 0x1F4;
                    }

                    break;
                }
                case BOARD_PAGE_WEAPON:
                    for (int i = 0; i < 6; i++) {
                        CDngStatusData *status = BtlMenuStatusPt;
                        WEAPON_HAVE    *weapons = status->chara_weapons[i];
                        WEAPON_HAVE    *weapon = &weapons[10];

                        if (weapon->item_no >= ITEM_WEAPON_START) {
                            message = GetWeaponMsgNo2(weapon->item_no) + 0x1F4;
                            value = weapon->level;
                            break;
                        }
                    }

                    break;
                case BOARD_PAGE_ATTACH: {
                    ATTACH_LIST *attach = reinterpret_cast<ATTACH_LIST *>(BtlMenuStatusPt->consumable_items) + cursor + 40;

                    if (attach->item_no >= ITEM_ATTACH_START) {
                        message = attach->item_no + 0x1F4;

                        if (attach->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                            name_mes = GetWeaponMsgNo2(attach->sphere_weapon_no);
                        }

                        value = GetAttachVolumeForMsg(attach);
                    }

                    break;
                }
                default:
                    break;
            }

            break;
        case ITEM_MENU_BOARD:
            switch (board->cursor_area) {
                case PERSONAL_BOARD_AREA_CELLS:
                    switch (board->page) {
                        case BOARD_PAGE_ITEM: {
                            s16 *slot = &MenuItemPackPt->item[cursor];

                            if (*slot >= ITEM_DUNGEON_START) {
                                message = *slot + 0x1F4;
                            }

                            break;
                        }
                        case BOARD_PAGE_WEAPON: {
                            int             chara = board->cursor / 10;
                            CDngStatusData *status = BtlMenuStatusPt;
                            WEAPON_HAVE    *row = status->chara_weapons[chara];
                            WEAPON_HAVE    *weapon = &row[board->cursor % 10];

                            if (weapon->item_no >= ITEM_WEAPON_START) {
                                message = GetWeaponMsgNo2(weapon->item_no) + 0x1F4;
                                value = weapon->level;
                            }

                            break;
                        }
                        case BOARD_PAGE_ATTACH: {
                            auto *attach = reinterpret_cast<ATTACH_LIST *>(&board->consumables[ItemMenuMode.board.cursor]);

                            if (attach->item_no >= ITEM_ATTACH_START) {
                                message = attach->item_no + 0x1F4;
                                value = GetAttachVolumeForMsg(attach);

                                if (attach->item_no == ITEM_ATTACH_SYNTHESIS_SPHERE) {
                                    name_mes = GetWeaponMsgNo2(attach->sphere_weapon_no);
                                }
                            }

                            break;
                        }
                        default:
                            break;
                    }

                    break;
                case PERSONAL_BOARD_AREA_TRASH:
                    if (BtlHaveItemPt->item_no < ITEM_ATTACH_START) {
                        message = 0xBE;
                    }

                    break;
                default:
                    break;
            }

            break;
        default:
            break;
    }

    message = std::max(message, 0);

    if (ItemMenuMode.state != ITEM_MENU_STATE_DEBUG_ITEMS) {
        int remake = 0;

        if (CommonMenuMes2.mes_made != message) {
            remake = 1;
        }

        int made_no = CommonMenuMes2.mes_no[0];
        int made_value = CommonMenuMes2.value;

        if ((name_mes > 0 && made_no != name_mes + 0x64) || (value >= 0 && made_value != value) || (chara_mes >= 0 && made_no != chara_mes + 0x32)) {
            remake = 1;
        }

        if (remake != 0) {
            CommonMenuMes2.value_signed = true;
            CommonMenuMes2.value_show = false;
            CommonMenuMes2.value = value;

            if (name_mes > 0) {
                CommonMenuMes2.mes_no[0] = name_mes + 0x64;
            }

            if (chara_mes >= 0) {
                CommonMenuMes2.mes_no[0] = chara_mes + 0x32;
            }

            CommonMenuMes2.mes_made = -1;
            CommonMenuMes2.MakeMesWin(message);
        }
    }

    if (ItemMenuMode.overflow == 0) {
        switch (ItemMenuMode.mode) {
            case ITEM_MENU_QUICK_SLOTS:
            case ITEM_MENU_CHARA:
            case ITEM_MENU_WEAPON:
            case ITEM_MENU_DEFENSE:
            case ITEM_MENU_BOARD:
            case ITEM_MENU_TRUSH: {
                s16 names[6] = {0xB4, 0xB6, 0xB6, 0xB6, 0xB5, 0xB6};
                int made = AtoraNameMes.mes_made;
                s16 name = names[ItemMenuMode.mode];

                if (made != name) {
                    AtoraNameMes.MakeMesWin(name);
                }

                break;
            }
            default:
                break;
        }
    }

    return 1;
}

void ItemMenuModeDraw() {
    BtlMenuTexBlockEnter();
    CharaStatus = TexManager.GetTexture("status", -1);

    switch (ItemMenuMode.mode) {
        case ITEM_MENU_QUICK_SLOTS:
        case ITEM_MENU_CHARA:
        case ITEM_MENU_WEAPON:
        case ITEM_MENU_DEFENSE:
        case ITEM_MENU_BOARD:
        case ITEM_MENU_TRUSH: {
            ActiveItemDraw(0x3A, 0x4A, 0x80);
            DrawPersonalBoard(0x154, 0x78, ItemMenuMode.board.page, 0x80, 0);
            CommonTrushDraw(0x232, 0x10C, 0x80);
            CommonMoneyBoardDraw(0x163, 0x120, static_cast<CUserStatus *>(BtlMenuStatusPt)->money, 0x80);
            DrawTrushItem();
            IconAutoGet.IconAutoMove();
            IconAutoGet.IconAutoMoveDraw();
            ItemMenuCharaStatusDraw(0x3A, 0xB4, ItemMenuMode.chara, 0x80);
            MenuTextureReload(BtlMenuReadBlock);
            int arrow_x = -1;
            int arrow_y = arrow_x;
            int arrow_w = arrow_x;

            switch (ItemMenuMode.mode) {
                case ITEM_MENU_QUICK_SLOTS:
                case ITEM_MENU_CHARA:
                case ITEM_MENU_WEAPON:
                case ITEM_MENU_DEFENSE:
                    if (BtlMenuStatusPt->party_size > 1) {
                        arrow_x = 0x32;
                        arrow_y = 0x9C;
                        arrow_w = 0xE8;
                    }

                    break;
                case ITEM_MENU_BOARD:
                case ITEM_MENU_TRUSH:
                    arrow_x = 0x146;
                    arrow_y = 0x2A;
                    arrow_w = 0xE6;
                    break;
                default:
                    break;
            }

            if (0 < arrow_x) {
                DrawBtlMenuLRCursor(arrow_x, arrow_y, arrow_w, 0x80);
            }

            break;
        }
        default:
            break;
    }

    if (ItemMenuMode.overflow == 0) {
        MenuTextureReload(AtoraNameMes.tex_block);
        float widths[7] = {11.4f, 12.7f, 12.7f, 12.9f, 12.7f, 12.7f, 12.7f};
        s8    shift[7] = {0, 0, 0, 0, 0, -16, -16};
        float heights[7] = {1.2f, 1.2f, 1.2f, 1.2f, 1.2f, 2.4f, 2.4f};
        MenuHelpWinDraw(0x36, 0x174 + shift[BtlMenuNowLang], widths[BtlMenuNowLang], heights[BtlMenuNowLang], 0x80);
        AtoraNameMes.stay_frame = false;
        AtoraNameMes.text_x = 0x4A;
        AtoraNameMes.text_y = 0x178;
        AtoraNameMes.edge_alpha = 0x80;
        AtoraNameMes.Step();
        AtoraNameMes.DrawMesWin();
    }

    MenuTextureReload(BtlMenuReadBlock);

    if (ItemMenuMode.state == ITEM_MENU_STATE_DEBUG_ITEMS) {
        DebugItemGetDraw();
    }

    s16 cursor_pos[7][2] = {
        {0,     0x68},
        {0xB2,  0xE2},
        {0x2D,  0xBF},
        {0x59,  0xBF},
        {0,     0   },
        {0x11A, 0   },
        {0,     0   }
    };
    cursor_pos[0][0] = (ItemMenuMode.board.cursor << 6) + 0x3E;
    cursor_pos[5][1] = ItemMenuMode.board.cursor * 0x2C + 0x7E;
    int             x = cursor_pos[ItemMenuMode.mode][0];
    int             y = cursor_pos[ItemMenuMode.mode][1];
    PERSONAL_BOARD *board = &ItemMenuMode.board;

    switch (ItemMenuMode.mode) {
        case ITEM_MENU_BOARD:
            switch (board->cursor_area) {
                case PERSONAL_BOARD_AREA_CELLS: {
                    int cursor = board->cursor;
                    x = cursor % 5 * 0x28 + 0x148;
                    cursor -= board->top_row * 5;
                    y = cursor / 5 * 0x28 + 0x7C;
                    break;
                }
                case PERSONAL_BOARD_AREA_TRASH:
                    x = 0x216;
                    y = 0x110;
                    break;
                default:
                    break;
            }

            break;
        default:
            break;
    }

    int frame_w = 0x2A;
    int frame_h = 0x22;
    int frame_x = x + 0x17;
    int frame_y = y - 2;

    if (ItemMenuMode.mode == ITEM_MENU_CHARA) {
        frame_h = 0x68;
        frame_w = frame_h;
        frame_x = x + 0xE;
        frame_y = y - 0x26;
    }

    int vibe = 0;

    if (board->held_item.item_no >= ITEM_ATTACH_START) {
        vibe = 2;
    } else {
        switch (ItemMenuMode.mode) {
            case ITEM_MENU_QUICK_SLOTS: {
                int  cursor = ItemMenuMode.board.cursor;
                s16 *slot = &MenuItemPackPt->quick_item_slot[cursor];

                if (0 <= cursor && *slot >= ITEM_DUNGEON_START) {
                    vibe = 1;
                }

                break;
            }
            case ITEM_MENU_TRUSH:
            case ITEM_MENU_BOARD: {
                int cursor = ItemMenuMode.board.cursor;

                if (SearchBoardNowPosItemExist(ItemMenuMode.board.page, cursor) >= ITEM_ATTACH_START) {
                    vibe = 1;
                }

                if (ItemMenuMode.board.cursor_area == PERSONAL_BOARD_AREA_TRASH && board->held_item.item_no < ITEM_ATTACH_START) {
                    vibe = 0;
                }

                break;
            }
            default:
                break;
        }
    }

    s16 vibe_offset[3][2] = {
        {0,    0   },
        {0x18, 0x18},
        {0x1C, 0x0D}
    };
    x += vibe_offset[vibe][0];
    y += vibe_offset[vibe][1];
    vibe = (vibe << 5) + 0x40;
    SysCur[0] += (static_cast<float>(x) - SysCur[0]) / 4.0f;
    SysCur[1] += (static_cast<float>(y) - SysCur[1]) / 4.0f;
    int big = 1;

    if (vibe >= 0x80) {
        big = 0;
    }

    if (board->held_item.item_no >= ITEM_ATTACH_START && ItemMenuMode.state != ITEM_MENU_STATE_POWERUP_CONFIRM) {
        ItemNaviCursor(board->held_item.item_no);
        COM_ITEM_INFO *info = GetCommonItemInfo(board->held_item.item_no);

        if (info != nullptr && (info->kind == ITEMKIND_WEAPON || info->kind == ITEMKIND_ATTACH)) {
            MenuTextureReload(ItemMenuWeaponIconReadBlock);
        }

        DrawMenuVibeItem(static_cast<int>(SysCur[0]), static_cast<int>(SysCur[1]), 2, -0xC, 0x80);
    }

    switch (ItemMenuMode.state) {
        case ITEM_MENU_STATE_LOADING:
        case ITEM_MENU_STATE_OPEN:
        case ITEM_MENU_STATE_CLOSE: {
            int alpha = 0;

            switch (ItemMenuMode.state) {
                case ITEM_MENU_STATE_LOADING:
                    alpha = 0x80;
                    break;
                case ITEM_MENU_STATE_OPEN:
                    alpha = 0x80 - ItemMenuMode.counter * 8;
                    break;
                case ITEM_MENU_STATE_CLOSE:
                    alpha = ItemMenuMode.counter * 5 + 0x40;
                    break;
                default:
                    break;
            }

            alpha = std::max(alpha, 0);

            alpha = std::min(alpha, 0x80);

            FrameImageDraw(0x40, alpha);
            break;
        }
        case ITEM_MENU_STATE_POWERUP_CONFIRM: {
            WEAPON_HAVE *weapon = ItemMenuMode.target_weapon;
            int          filled = 0;
            int          holes = GetWeaponHoleNum(weapon->item_no);

            for (int i = 0; i < holes; i++) {
                if (weapon->attach[i].item_no >= ITEM_ATTACH_START) {
                    filled++;
                }
            }

            CommonMenuMes1.value_show = true;

            if (CommonMenuMes1.mes_made != 0xBC || CommonMenuMes1.values[0] != filled || CommonMenuMes1.values[1] != holes) {
                for (int i = 0; i < 10; i++) {
                    CommonMenuMes1.mes_no[i] = -1;
                    CommonMenuMes1.values[i] = 0;
                }

                CommonMenuMes1.value_signed = false;
                CommonMenuMes1.mes_made = -1;
                CommonMenuMes1.values[0] = filled;
                CommonMenuMes1.values[1] = holes;
                CommonMenuMes1.MakeMesWin(0xBC);
            }

            switch (ItemMenuMode.use_target) {
                case ITEM_USE_ON_EQUIP_WEAPON:
                case ITEM_USE_ON_BOARD_WEAPON: {
                    CommonMenuMes1.stay_frame = true;
                    MenuTextureReload(CommonMenuMes1.tex_block);
                    CommonMenuMes1.auto_pos = MES_POS_AUTO;
                    CommonMenuMes1.text_x = 0xE2;
                    CommonMenuMes1.text_y = 0x92;
                    CommonMenuMes1.Step();
                    CommonMenuMes1.DrawMesWin();
                    setbilinear(0);
                    MenuTextureReload(BtlMenuReadBlock);
                    int hole_x = 0x13C - (holes * 0x12 + 0xA);
                    DrawWepHole(hole_x, 0x5A, weapon, 1, 0x80);
                    MenuTextureReload(ItemMenuWeaponIconReadBlock);
                    DrawWepAttach(hole_x + 0xE, 0x60, weapon, 1, 0x80);
                    MenuTextureReload(BtlMenuReadBlock);
                    DrawDngYesNoDialog(0x114, 0xE6, 0x80);
                    int yes_y = ItemMenuMode.confirm * 0x1C + 0xE0;
                    DrawMenuWaku(268.0f, static_cast<float>(yes_y - 2), 0x60, 0x18, 0, StayTex, 0x80);
                    DrawMenuObjectVibe(0xF2, yes_y, 0, 0x40);
                    break;
                }
                default:
                    break;
            }

            break;
        }
        default:
            if (ItemMenuMode.state != ITEM_MENU_STATE_DEBUG_ITEMS && ItemMenuMode.state != ITEM_MENU_STATE_EFFECT) {
                DrawMenuWaku(static_cast<float>(frame_x), static_cast<float>(frame_y), frame_w, frame_h, 0, StayTex, 0x80);
                DrawMenuObjectVibe(static_cast<int>(SysCur[0]), static_cast<int>(SysCur[1]), big, vibe);
            }

            break;
    }
}

int ItemMenuModeKey() {
    if (ItemMenuMode.mode == ITEM_MENU_TRUSH) {
        int count = TrushMoveMax[ItemMenuMode.board.page];

        if (count <= ItemMenuMode.board.cursor) {
            ItemMenuMode.board.cursor = count - 1;
        }
    }

    if (GamePad.Down(PAD_UP) != 0) {
        switch (ItemMenuMode.mode) {
            case ITEM_MENU_WEAPON:
            case ITEM_MENU_DEFENSE:
                ItemMenuMode.board.cursor = 0;
                ItemMenuMode.mode = ITEM_MENU_QUICK_SLOTS;
                break;
            case ITEM_MENU_TRUSH:
                if (0 < ItemMenuMode.board.cursor) {
                    ItemMenuMode.board.cursor--;
                }

                ItemMenuMode.board.cursor = std::max(ItemMenuMode.board.cursor, 0);

                break;
            default:
                ItemMenuMode.board.cursor = 1;
                ItemMenuMode.mode = ITEM_MENU_QUICK_SLOTS;
                break;
        }
    }

    if (GamePad.Down(PAD_DOWN) != 0) {
        switch (ItemMenuMode.mode) {
            case ITEM_MENU_TRUSH: {
                ItemMenuMode.board.cursor++;
                int count = ItemMenuMode.overflow_count[ItemMenuMode.board.page];

                if (count <= ItemMenuMode.board.cursor) {
                    ItemMenuMode.board.cursor = count - 1;
                }

                ItemMenuMode.board.cursor = std::max(ItemMenuMode.board.cursor, 0);

                break;
            }
            case ITEM_MENU_QUICK_SLOTS:
                if (ItemMenuMode.board.cursor == 0) {
                    ItemMenuMode.mode = ITEM_MENU_WEAPON;
                    break;
                }
            default:
                ItemMenuMode.mode = ITEM_MENU_CHARA;
                break;
        }
    }

    if (GamePad.Down(PAD_LEFT) != 0) {
        switch (ItemMenuMode.mode) {
            case ITEM_MENU_QUICK_SLOTS:
                if (0 < ItemMenuMode.board.cursor) {
                    ItemMenuMode.board.cursor--;
                }

                break;
            case ITEM_MENU_DEFENSE:
                ItemMenuMode.mode = ITEM_MENU_WEAPON;
                break;
            case ITEM_MENU_CHARA:
                ItemMenuMode.mode = ITEM_MENU_DEFENSE;
                break;
            case ITEM_MENU_TRUSH:
                ItemMenuMode.mode = ITEM_MENU_CHARA;
                ItemMenuMode.board.cursor = 0;
                break;
            default:
                break;
        }
    }

    if (GamePad.Down(PAD_RIGHT) != 0) {
        int old_mode = ItemMenuMode.mode;

        switch (ItemMenuMode.mode) {
            case ITEM_MENU_TRUSH:
                ItemMenuMode.mode = ITEM_MENU_BOARD;
                ItemMenuMode.board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
                ItemMenuMode.board.cursor = (ItemMenuMode.board.top_row + ItemMenuMode.board.cursor) * 5;
                break;
            case ITEM_MENU_QUICK_SLOTS:
                if (ItemMenuMode.board.cursor < 2) {
                    ItemMenuMode.board.cursor++;
                } else {
                    ItemMenuMode.mode = ITEM_MENU_BOARD;
                    ItemMenuMode.board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
                    ItemMenuMode.board.cursor = ItemMenuMode.board.top_row * 5;
                }

                break;
            case ITEM_MENU_WEAPON:
                ItemMenuMode.mode = ITEM_MENU_DEFENSE;
                break;
            case ITEM_MENU_DEFENSE:
                ItemMenuMode.mode = ITEM_MENU_CHARA;
                break;
            case ITEM_MENU_CHARA:
                ItemMenuMode.mode = ITEM_MENU_BOARD;
                ItemMenuMode.board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
                ItemMenuMode.board.cursor = ItemMenuMode.board.top_row * 5 + 5;
                break;
            default:
                break;
        }

        if (old_mode != ITEM_MENU_TRUSH && ItemMenuMode.mode == ITEM_MENU_BOARD) {
            int rows[4] = {0, 2, 0, 0};

            if ((ItemMenuMode.overflow_pages & (1 << (ItemMenuMode.board.page + 1))) && ItemMenuMode.overflow != 0) {
                ItemMenuMode.board.cursor = rows[old_mode];

                ItemMenuMode.board.cursor = std::min(ItemMenuMode.board.cursor, ItemMenuMode.overflow_count[ItemMenuMode.board.page] - 1);

                ItemMenuMode.mode = ITEM_MENU_TRUSH;
            }
        }
    }

    return 0;
}

void ActiveItemDraw(int x, int y, int alpha) {
    RECT digits = {0x90, 0xDC, 0xC, 0xC};

    MenuTextureReload(ItemMenuWeaponIconReadBlock);
    int slot_x = x + 0xE;
    int slot_y = 0;
    DrawMenu2DSprite(CharaStatus, CRect_i_(slot_x, y + 1, 0xE0, 0x4F), CRect_i_(0, 0, 0xE0, 0x50), alpha);
    s16 *items = MenuItemPackPt->quick_item_slot;
    s16 *counts = MenuItemPackPt->quick_item_qty;

    for (int i = 0; i < 3; i++) {
        s16 item_no = items[i];

        if (item_no >= ITEM_DUNGEON_START) {
            if (item_no >= ITEM_DUNGEON_START) {
                slot_x = x + 0x22 + (i << 6);
            }

            slot_y = y + 0x1F;
            DrawMenu2DSprite(CharaStatus, CRect_i_(slot_x, slot_y, 0x2C, 0x2C), CRect_i_(0x128, 0, 0x2C, 0x2C), alpha);
            DrawIconParts(item_no, slot_x + 6, slot_y + 6, 0, 0x280, alpha, 0);
            s16 count = counts[i];

            if (count >= 2) {
                DrawMenuNumber(count, slot_x + 0x26, slot_y + 0x1E, PerBoardTex, digits, 0, alpha);
            }
        }
    }

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON:
            DngActiveItemTextureCopy();
            break;
        default:
            break;
    }
}

namespace {

/**
 * Steps and draws the party member's model on the item page.
 */
void MenuCharaPolyDraw() {
    SetNowCharaMotionNo(ItemMenuMode.chara);
    SetItemMenuColor(ItemMenuMode.chara);
    MenuCharaFrame.Step();

    if (ItemMenuMode.chara == 0) {
        MenuCharaFrame.ClothStep(0);
    }

    MenuCharaFrame.Draw();
    SetItemMenuOldAmbient();
}

} // namespace

void ItemMenuCharaStatusDraw(int x, int y, int chara, int alpha) {
    int draw_x = 0;
    int draw_y = 0;

    if (CharaStatus != nullptr) {
        MenuTextureReload(ItemMenuWeaponIconReadBlock);
        DrawMenu2DSprite(CharaStatus, CRect_i_(x, y + 1, 0xFC, 0x85), CRect_i_(0, 0x50, 0xFC, 0x86), alpha);
        int  level = BtlMenuStatusPt->defense[chara];
        RECT number_rect = {0x100, 0x60, 0xC, 0xE};
        DrawMenuNumber(level, x + 0x64, y + 0x24, CharaStatus, number_rect, 1, alpha);

        if (MenuExTextureReadFlag == MENU_EX_TEX_READY && chara < BtlMenuStatusPt->party_size) {
            MenuTextureReload(MenuExtendReadBlock);
            MGSetWindowRect(CRect_i_(x + 0x82, 0x5D, 0x76, 0x3F));
            MenuPolygonDraw(alpha, MenuCharaPolyDraw);
            MGSetWindowRect();
            spRGBA top;
            spRGBA bottom;
            top.r = top.g = top.b = bottom.r = bottom.g = bottom.b = 0x80;
            top.a = 0;
            bottom.a = alpha;
            MenuTextureReload(ItemMenuWeaponIconReadBlock);
            draw_x = x + 0x7E;
            draw_y = y + 0x6E;
            set2DSprite(GetVif1Packet(), CharaStatus, CRect_i_(draw_x, draw_y + 1, 0x7E, 0x17), CRect_i_(0x7E, 0xBE, 0x7E, 0x18), &top, &top, &bottom, &bottom, 1);
        }

        MenuTextureReload(BtlMenuReadBlock);
        draw_x = x + 0x6E;
        draw_y = y + 0x14;
        CharaStatusMsgDraw(draw_x, draw_y, chara, 0, alpha);
        CDngStatusData *status = BtlMenuStatusPt;
        int             hp = status->hp[chara];
        int             max_hp = status->max_hp[chara];

        if (max_hp <= 0) {
            max_hp = 1;
        }

        WEAPON_HAVE *weapon = nullptr;
        int          slot = status->equipped_weapon_slot[chara];

        if (slot < 0) {
            weapon = &ItemMenuMode.board.weapon;
        } else {
            WEAPON_HAVE *row = status->chara_weapons[chara];
            weapon = &row[slot];
        }

        float durability = 0.0f;
        float durability_max = durability = 0.0f;

        if (weapon != nullptr) {
            durability = weapon->durability_f;
            durability_max = weapon->durability;
        }

        if (durability_max < 0.0f) {
            durability_max = 1.0f;
        }

        MenuTextureReload(ItemMenuWeaponIconReadBlock);
        int      hp_width = max_hp * 153 / 200;
        int      durability_width = 153.0f * durability_max / 99.0f;
        CRect_i_ bar_src(0x154, 0, 4, 0x10);
        draw_y = y + 0x41;
        DrawMenu2DSprite(CharaStatus, CRect_i_(x + 0x22, draw_y, hp_width, 0x10), bar_src, alpha);
        DrawMenu2DSprite(CharaStatus, CRect_i_(x + 0x1A + hp_width, draw_y, 0xA, 0x10), CRect_i_(0x154, 0, 0xA, 0x10), alpha);
        draw_y = y + 0x51;
        DrawMenu2DSprite(CharaStatus, CRect_i_(x + 0x22, draw_y, durability_width, 0x10), bar_src, alpha);
        DrawMenu2DSprite(CharaStatus, CRect_i_(x + 0x1A + durability_width, draw_y, 0xA, 0x10), CRect_i_(0x158, 0, 0xA, 0x10), alpha);
        float thirst_value = BtlMenuStatusPt->water_max[chara];
        int   thirst = thirst_value / 10.0f;
        draw_x = x + 0x1E;
        draw_y = y + 0x62;

        for (int i = 0; i < thirst - 1; i++) {
            DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x, draw_y + 2, 0x12, 0x15), CRect_i_(0x154, 0x10, 0x12, 0x14), alpha);
            draw_x += 0x12;
        }

        DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x, draw_y + 2, 0x18, 0x15), CRect_i_(0x166, 0x10, 0x18, 0x14), alpha);
        draw_x = x + 0x1F;
        draw_y = y + 0x47;
        float hp_fill = static_cast<float>(hp_width * hp) / static_cast<float>(max_hp);
        int   color = 0;
        float low_hp = 0.3f * static_cast<float>(max_hp);

        if (static_cast<float>(hp) < low_hp) {
            color++;
        }

        if (static_cast<float>(hp) < 0.15f * static_cast<float>(max_hp)) {
            color++;
        }

        GRADATION_COLOR_INFO2 *info = GetGradationColorInfo2(color + 0x16);
        GRADATION_COLOR_INFO2  gradation{};
        memcpy(&gradation, info, sizeof(gradation));
        gradation.colors[3].a = alpha;
        gradation.colors[2].a = alpha;
        gradation.colors[1].a = alpha;
        gradation.colors[0].a = alpha;
        DrawMenuColorGradation(Ps2Lvalue(CRect_i_(draw_x, draw_y - 1, static_cast<int>(hp_fill), 6)), &info->colors[0], &info->colors[1], &info->colors[2], &info->colors[3]);
        int values[2] = {hp, max_hp};
        draw_x = x + 0x3C + hp_width;
        draw_y = y + 0x47;
        DrawStatusNumberNowAndMax(values, draw_x, draw_y, 1, alpha);
        int motion = 0;
        int now_motion = MenuCharaFrame.motion_no;

        if (hp_fill < low_hp) {
            motion = 1;
        } else {
            motion = 0;
        }

        if (motion != now_motion) {
            MenuCharaFrame.SetMotion(motion, 0);
        }

        draw_x = x + 0x1F;
        draw_y = y + 0x56;
        hp_fill = static_cast<float>(durability_width) * durability / durability_max;
        int durability_color = 0;

        if (durability < 0.3f * durability_max) {
            durability_color = 0x1A;
        } else {
            durability_color = 0x19;
        }

        info = GetGradationColorInfo2(0x19);
        memcpy(&gradation, info, sizeof(gradation));
        gradation.colors[3].a = alpha;
        gradation.colors[2].a = alpha;
        gradation.colors[1].a = alpha;
        gradation.colors[0].a = alpha;
        DrawMenuColorGradation(Ps2Lvalue(CRect_i_(draw_x, draw_y, static_cast<int>(hp_fill), 6)), &gradation.colors[0], &gradation.colors[1], &gradation.colors[2], &gradation.colors[3]);
        draw_x = x + 0x3C + durability_width;
        draw_y = y + 0x57;
        values[0] = GetDispVolumeForFloat(durability);
        values[1] = static_cast<int>(durability_max);
        DrawStatusNumberNowAndMax(values, draw_x, draw_y, 1, alpha);
        draw_x = x + 0x20;
        draw_y = y + 0x60;
        int drops = static_cast<int>(BtlMenuStatusPt->water_now[chara]) / 10;

        for (int i = 0; i < drops; i++) {
            DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x, draw_y, 0x10, 0x14), CRect_i_(0x128, 0x2C, 0x10, 0x14), alpha);
            draw_x += 0x12;
        }

        drops = static_cast<int>(BtlMenuStatusPt->water_now[chara]) % 10;

        if (drops != 0) {
            DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x, draw_y, 0x10, 0x14), CRect_i_(0x128 + (3 - static_cast<int>(drops / 2.5f)) * 0x10, 0x2C, 0x10, 0x14), alpha);
        }

        draw_y = y - 0x14;
        int party_size = BtlMenuStatusPt->party_size;
        int tab = 0;
        int i = 0;
        int after = 0;
        int tex_x = 0;
        int tex_y = 0;

        for (; i <= party_size; i++) {
            if (tab == chara) {
                if (after != 0) {
                    draw_x = x + 0x10 + chara * 0xC;
                    DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x, draw_y - 1, 0x20, 0x1B), CRect_i_(0, 0xD8, 0x20, 0x1C), alpha);
                    DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x + 0x20, draw_y - 1, 0x5E, 0x1B), CRect_i_(0x20, 0xD8, 0x4A, 0x1C), alpha);
                    DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x + 0x7E, draw_y - 1, 0x20, 0x1B), CRect_i_(0x6A, 0xD8, 0x20, 0x1C), alpha);
                } else {
                    after = 1;
                    tab = party_size - 1;
                }
            } else {
                draw_x = tab * 0xC + (x + 0x10 + after * 0x69);
                draw_y = y - 0x14;
                tex_y = 0xD8;

                if (after == 0) {
                    tex_x = 0xA6;
                    tab++;
                } else {
                    tex_x = 0x8A;
                    tab--;

                    if (GetMenuLangFlag() > LANG_JAPANESE) {
                        draw_x += 0x14;
                    }
                }

                DrawMenu2DSprite(CharaStatus, CRect_i_(draw_x, draw_y + 2, 0x1C, 0x13), CRect_i_(tex_x, tex_y, 0x1C, 0x14), alpha);
            }
        }

        int item_no = weapon->item_no;
        draw_x = x + 0x16;
        draw_y = y + 0x10;
        RetCTex(item_no, tex_x, tex_y);
        CTexture *icon = TexManager.GetTexture("wepicon", -1);

        if (icon != nullptr && item_no >= ITEM_WEAPON_START) {
            MenuTextureReload(ItemMenuWeaponIconReadBlock);
            WEAPON_HAVE total;
            WeaponAllValueSet(weapon, &total, 0);
            DrawMenu2DSprite(icon, CRect_i_(draw_x, draw_y, 0x20, 0x20), CRect_i_(tex_x, tex_y, 0x20, 0x20), alpha);
            DrawMenuNumber(total.attack, draw_x + 0x20, draw_y + 0x14, CharaStatus, number_rect, 1, alpha);
        }

        MenuTextureReload(CommonMenuMes3.tex_block);
        int mes_no = chara + 0xC2;

        if (CommonMenuMes3.mes_made != mes_no) {
            CommonMenuMes3.MakeMesWin(mes_no);
        }

        int name_length = GetMsgLengthCharaName(chara);
        CommonMenuMes3.edge_alpha = alpha;
        CommonMenuMes3.stay_frame = false;
        s8  char_width[7] = {8, 6, 6, 6, 6, 6, 6};
        s8  name_offset[7] = {0, 6, 6, 6, 6, 6, 6};
        int lang = BtlMenuNowLang;
        DrawMenuClsMes(&CommonMenuMes3, x + 0x5A + chara * 0xC - name_length * char_width[lang] + name_offset[lang], y - 0x12);
    }
}

void ItemNaviCursor(int item_no) {
    s8         shown[3] = {0, 0, 0};
    ITEM_DATA *data = GetItemData(item_no);

    if (data == nullptr) {
        return;
    }

    switch (data->sort_key) {
        case ITEM_SORT_FOOD:
            shown[0] = 1;

            if (item_no >= ITEM_REGULAR_WATER && item_no <= ITEM_CHEESE) {
                shown[1] = 1;
            }

            if (item_no == ITEM_MELLOW_BANANA) {
                shown[1] = 1;
            }

            break;
        case ITEM_SORT_POWDER:
            if (static_cast<u32>(item_no - ITEM_REPAIR_POWDER) < 2) {
                shown[2] = 1;
            }

            if (item_no == ITEM_REVIVAL_POWDER) {
                shown[1] = 1;
                int             chara = ItemMenuMode.chara;
                CDngStatusData *status = BtlMenuStatusPt;

                if (static_cast<float>(status->hp[chara]) <= 0.0f) {
                    shown[0] = 1;
                }
            }

            break;
        case ITEM_SORT_THROW:
            shown[1] = 1;

            if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON && item_no >= ITEM_THROBBING_CHERRY && item_no < ITEM_MEDUSA_POWDER && item_no != ITEM_BOMB_NUTS) {
                shown[0] = 1;
            }

            break;
        default:
            if (item_no == ITEM_DRAN_S_FEATHER) {
                shown[1] = 1;
            }

            break;
    }

    static int ct = 0;
    s16        pos[3][2] = {
        {0x104, 0xAC},
        {0x10C, 0x46},
        {0x64,  0xAA}
    };
    ct++;

    for (int i = 0; i < 3; i++) {
        if (shown[i] != 0 && ct % 60 < 30) {
            int      px = pos[i][0];
            int      py = pos[i][1];
            CRect_i_ texel(0xD8, 0, 0x28, 0x14);

            for (int j = 0; j < 2; j++, py -= 0x14, texel.x -= 0x28) {
                DrawMenu2DSprite(BtStatus, CRect_i_(px, py, texel.width, texel.height), texel, 0x80);
            }
        }
    }

    if (ct >= 0x3C) {
        ct = 0;
    }
}

void CharaStatusMsgDraw(int x, int y, int chara, int per_member, int alpha) {
    static float statusCnt = -PI;

    int bob_y = static_cast<int>(static_cast<float>(y) + 4.0f * sinf(statusCnt));
    int condition = 0;
    int icon = -1;
    condition = BtlMenuStatusPt->GetActiveCharaStatus(chara);

    if (condition & AILMENT_GOO) {
        icon = 5;
    }

    if (condition & AILMENT_CURSE) {
        icon = 2;
    }

    if (condition & AILMENT_POISON) {
        icon = 4;
    }

    if (condition & AILMENT_STAMINA) {
        icon = 1;
    }

    if (condition & AILMENT_FREEZE) {
        icon = 3;
    }

    if (BtlMenuStatusPt->hp[chara] <= 0 || (condition & AILMENT_KNOCKED_OUT)) {
        icon = 0;
    }

    if (icon >= 0) {
        int u = icon / 3 * 0x44 + 0x78;
        int v = icon % 3 * 0x24 + 0x14;
        DrawMenu2DSprite(BtStatus, CRect_i_(x, bob_y, 0x44, 0x23), CRect_i_(u, v, 0x44, 0x24), alpha);
    }

    float period = 0.0f;

    if (per_member != 0) {
        float count = BtlMenuStatusPt->GetPartySize();
        period = 50.0f * count;
    } else {
        period = 50.0f;
    }

    statusCnt += PI / period;

    if (!(statusCnt < PI)) {
        statusCnt = -PI;
    }
}

namespace {

/**
 * Runs the Atla page and returns to the menu bar when it closes.
 */
int BattleMenuAtoraKey() {
    switch (BtlEffectFlag) {
        case BTLEFFECT_PAGE_OPEN:
            if (ToFromSelect(0) != 0) {
                BtlEffectFlag = BTLEFFECT_NONE;
                BtlEffectCt = 0.0f;
            }

            break;
        case BTLEFFECT_PAGE_CLOSE:
            if (ToFromSelect(1) != 0) {
                MenuSelect[0] = BTLMENU_PAGE_ATLA;
                BattleMenuFlag = BTLMENU_STATE_ATLA_CLOSE;
                ForBackMenu();
                BattleMenuFlag = BTLMENU_STATE_MAIN;
            }

            break;
        default:
            switch (MenuAtoraSelectKey()) {
                case ATORA_SELECT_CLOSE:
                    BtlEffectFlag = BTLEFFECT_PAGE_CLOSE;
                    BtlEffectCt = 0.0f;
                    break;
                case ATORA_SELECT_NONE:
                default:
                    break;
            }

            break;
    }

    if (BtlEffectFlag != BTLEFFECT_NONE) {
        BtlEffectCt += 1.0f;
    } else {
        BtlEffectCt = 0.0f;
    }

    return 1;
}

int IsLoadMapNo();

int MapNoTransFunc(int map_no);

} // namespace

s16           MenuGrobalMapNoTbl[16] = {0, 200, 1, 201, 14, 2, 202, 3, 42, 203, 23, 4, 204, 40, 205, 60};
WORLD_MAP_POS TownOrDngPos[16] = {
    {"e01", 0, 0, 0},
    {"d01", 0, 0, 0},
    {"e02", 0, 0, 0},
    {"d02", 0, 0, 0},
    {"s04", 0, 0, 0},
    {"e03", 0, 0, 0},
    {"d03", 0, 0, 0},
    {"e04", 0, 0, 0},
    {"s32", 0, 0, 0},
    {"d04", 0, 0, 0},
    {"s13", 0, 0, 0},
    {"e05", 0, 0, 0},
    {"d05", 0, 0, 0},
    {"s30", 0, 0, 0},
    {"d06", 0, 0, 0},
    {"xx",  0, 0, 0},
};
float mapo[4] = {0.0f, 0.0f, 0.0f, 1.0f};
float mapmovev[4] = {0.0f, 0.0f, 0.0f, 1.0f};

char     *NextWorldPos;
CFrame   *MapMoveCursor;
CTexture *MenuMoveTex;

namespace {

/**
 * Starts reading one region's world map in the background.
 */
void StartLoadWorldMap(int region, u_long128 *buffer);

} // namespace

void InitMenuMove(int mode, int texture_block, u_long128 *buffer) {
    BtlMenuSaveDataPt = SaveData;
    MenuMove.mode = mode;

    switch (mode) {
        case MENU_MOVE_DUNGEON_ESCAPE:
        case MENU_MOVE_INTERIOR_OUT:
            MenuMapJumpMode = 0;
            MenuMove.cursor = 1;
            MenuMove.ready = 1;
            CommonMenuMes3.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes3.rows = 3;
            CommonMenuMes3.auto_pos = MES_POS_NONE;
            SysCur[1] = MenuMove.cursor * 26 + 174;
            MapMoveCursor = nullptr;
            MenuMoveTex = nullptr;
            break;
        case MENU_MOVE_WORLD_MAP_DIRECT:
            BtlMenuStatusPt = BtlMenuSaveDataPt->GetDngStatus();
            StayTex = TexManager.GetTexture("stayframe", -1);
            GamePad.MenuModeOn(0x78);
            GamePad.SetAutoRepeat(PAD_DPAD, 30, 5);
        case MENU_MOVE_WORLD_MAP:
            if ((static_cast<s32 *>(SaveData->GetConfigData()))[14] != 0 && SaveData->VisitMap(MenuGrobalMapNoTbl[WORLD_PLACE_FIELD_S30], 0) > 0) {
                printf("clear!!!!\n");

                if (SaveData->VisitMap(MenuGrobalMapNoTbl[WORLD_PLACE_DEMON_SHAFT] <= 0, 0) != 0) {
                    SaveData->VisitMap(MenuGrobalMapNoTbl[WORLD_PLACE_DEMON_SHAFT], 1);
                    printf("demon shaft On!!!!!\n");
                }
            } else {
                if ((static_cast<s32 *>(SaveData->GetConfigData()))[14] == 0) {
                    printf("not clear \n");
                }

                if (SaveData->QuestDungeon(5, 0) <= 0) {
                    printf("not toki no kairou\n");
                }
            }

            printf("MapNo = %d\n", MapNo);
            MenuMapJumpMode = -1;
            MenuMove.cursor = MapNoTransFunc(MapNo);
            MenuMove.start_place = MenuMove.cursor;
            MenuMove.load_map = IsLoadMapNo();
            printf("Now select map = %d\n", MenuMove.cursor);
            printf("Now Load MapNo = %d\n", MenuMove.load_map);
            NextWorldPos = TownOrDngPos[MenuMove.cursor].frame;
            MenuMove.tex_block = texture_block;
            buffer = MenuCalcBufAlignment(buffer);
            MenuMove.last_place = WORLD_PLACE_DEMON_SHAFT;
            StartLoadWorldMap(MenuMove.load_map, buffer);

            if (mode == MENU_MOVE_WORLD_MAP_DIRECT) {
                do {
                } while (ReadBGSync() != 0);
            }

            MenuMove.ready = 0;
            CommonMenuMes3.Preset(MES_PRESET_NAME);
            CommonMenuMes3.rows = 1;
            MapMoveCursor = nullptr;
            break;
        case MENU_MOVE_FIRST_DUNGEON:
            MenuMove.cursor = 1;
            MenuMapJumpMode = -1;
            CommonMenuMes3.Preset(MES_PRESET_SYSTEM);
            CommonMenuMes3.rows = 3;
            SysCur[1] = MenuMove.cursor * 26 + 174;
            break;
        default:
            break;
    }

    MenuMove.state = MENU_MOVE_STATE_OPEN;
    MenuMove.counter = 1;
    CommonMenuMes2.mes_made = -1;
    CommonMenuMes3.mes_made = -1;
}

void GetTownOrDngPos() {
    MenuWorldTrans(&MenuCamera);
    float scale = 2.2f;
    MenuCharaFrame.SetScale(scale, scale, scale);
    MenuCharaFrame.SetPosition(mapo);
    float ref[4] = {0.0f, 0.0f, -45.0f, 1.0f};
    float pos[4] = {0.0f, 0.0f, 60.0f, 1.0f};
    MenuCamera.SetRef(ref);
    MenuCamera.SetPos(pos);

    for (int i = 0; i < 16; i++) {
        WORLD_MAP_POS *place = &TownOrDngPos[i];

        if (place != nullptr) {
            if (GetVisitInfo(i, MenuMove.mode) == 0) {
                place->visited = -1;
            } else {
                place->visited = 1;
            }

            CFrame *frame = MenuCharaFrame.frame->SearchFrame(place->frame);

            if (frame == nullptr) {
                printf("%d is NULL nothing\n", i);
            } else {
                int screen[2];
                Get3DPosTo2DPos(frame, screen);
                place->x = screen[0] - 4;
                place->y = screen[1] - 0x2A;
            }
        }
    }
}

int MenuMoveKey() {
    int stay = 1;
    int decided = 0;
    int message = CommonMenuMes3.mes_made;
    int help = 0;
    int done = 0;

    switch (MenuMove.state) {
        case MENU_MOVE_STATE_OPEN: {
            done = ToFromSelect(0);
            int ready = 1;

            switch (MenuMove.mode) {
                case MENU_MOVE_DUNGEON_ESCAPE:
                case MENU_MOVE_FIRST_DUNGEON:
                case MENU_MOVE_INTERIOR_OUT:
                    MenuMove.ready = 1;
                    break;
                case MENU_MOVE_WORLD_MAP:
                case MENU_MOVE_WORLD_MAP_DIRECT:
                    if (MenuMove.ready == 0) {
                        MenuMove.ready = LoadWorldMap();

                        if (MenuMove.ready != 0) {
                            ComMenuSePlay(SE_WORLD_MAP_OPEN);
                        }

                        ready = 0;
                    } else {
                        ready = 0;

                        if (MenuCharaFrame.motion_state == 3) {
                            ready = 1;
                            SysCur[0] = 328.0f;
                            SysCur[1] = 164.0f;
                            GetTownOrDngPos();
                        }
                    }

                    break;
                default:
                    break;
            }

            if (done != 0 && MenuMove.counter > 0x14 && MenuMove.ready != 0 && ready != 0) {
                switch (MenuMove.mode) {
                    case MENU_MOVE_WORLD_MAP_DIRECT:
                    default:
                        BattleMenuFlag = BTLMENU_STATE_MOVE;
                        break;
                }

                MenuMove.state = MENU_MOVE_STATE_SELECT;

                switch (MenuMove.mode) {
                    case MENU_MOVE_DUNGEON_ESCAPE:
                    case MENU_MOVE_FIRST_DUNGEON:
                    case MENU_MOVE_INTERIOR_OUT:
                        break;
                    case MENU_MOVE_WORLD_MAP_DIRECT:
                    case MENU_MOVE_WORLD_MAP:
                        SysCur[0] = 328.0f;
                        SysCur[1] = 164.0f;
                        break;
                    default:
                        break;
                }
            }

            break;
        }
        case MENU_MOVE_STATE_CLOSE: {
            done = ToFromSelect(1);

            if (MenuMove.mode == MENU_MOVE_WORLD_MAP_DIRECT) {
                done = 1;
            }

            if (done != 0 && MenuMove.counter > 0x14) {
                MenuMove.ready = 0;
                ForBackMenu();

                switch (MenuMove.mode) {
                    case MENU_MOVE_WORLD_MAP_DIRECT:
                        GamePad.AutoRepeatOff();
                        GamePad.MenuModeOff();
                        TexManager.DeleteTextureBlock(MenuMove.tex_block);
                        break;
                    default:
                        BattleMenuFlag = BTLMENU_STATE_MAIN;
                        break;
                }

                for (int &message : CommonMenuMes3.mes_no) {
                    message = -1;
                }

                for (int i = 0; i < 4; i++) {
                    CommonMenuMes1.mes_no[i] = -1;

                    if (i >= 0 && i < 10) {
                        CommonMenuMes1.line_pos[i].x = -1;
                        CommonMenuMes1.line_pos[i].y = -1;
                    }
                }
            }

            break;
        }
        case MENU_MOVE_STATE_ZOOM_IN:
            mapo[0] += mapmovev[0];
            mapo[1] += mapmovev[1];
            mapo[2] += mapmovev[2];

            if (MenuMove.counter > 0xF) {
                MenuMove.state = MENU_MOVE_STATE_CONFIRM;
            }

            break;
        case MENU_MOVE_STATE_ZOOM_OUT:
            mapo[0] += mapmovev[0];
            mapo[1] += mapmovev[1];
            mapo[2] += mapmovev[2];

            if (MenuMove.counter > 0xF) {
                mapo[0] = 0.0f;
                mapo[1] = 0.0f;
                mapo[2] = 0.0f;
                MenuMove.state = MENU_MOVE_STATE_SELECT;
            }

            break;
        case MENU_MOVE_STATE_CONFIRM:
            switch (MenuMove.cursor) {
                case WORLD_PLACE_WISE_OWL_FOREST:
                case WORLD_PLACE_FIELD_S04:
                case WORLD_PLACE_MUSKA_LACKA:
                case WORLD_PLACE_FIELD_S32:
                case WORLD_PLACE_SUN_MOON_TEMPLE:
                case WORLD_PLACE_FIELD_S13:
                case WORLD_PLACE_YELLOW_DROPS:
                case WORLD_PLACE_FIELD_S30:
                    help = 0x13A;
                    break;
                default:
                    help = 0x139;
                    break;
            }

            if (GamePad.Down(PAD_CROSS) != 0) {
                MenuMapJumpMode = MenuGrobalMapNoTbl[MenuMove.cursor];

                if (MapNo != MenuMapJumpMode) {
                    stay = 0;
                    ComMenuSePlay(MENU_SOUND_CONFIRM);
                } else if (MenuMove.mode == MENU_MOVE_WORLD_MAP_DIRECT) {
                    stay = 0;
                    MenuMapJumpMode = MapNo;
                } else {
                    stay = 1;
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                }
            } else if (GamePad.Down(PAD_CIRCLE) != 0) {
                MenuMove.state = MENU_MOVE_STATE_ZOOM_OUT;
                MenuMove.counter = 0;
                mapmovev[0] = -mapmovev[0];
                mapmovev[1] = -mapmovev[1];
                mapmovev[2] = -mapmovev[2];
            }

            break;
        case MENU_MOVE_STATE_SELECT: {
            int old_cursor = MenuMove.cursor;

            switch (MenuMove.mode) {
                case MENU_MOVE_DUNGEON_ESCAPE:
                case MENU_MOVE_FIRST_DUNGEON:
                case MENU_MOVE_INTERIOR_OUT:
                    if (GamePad.Down(PAD_UP | PAD_DOWN) != 0) {
                        if (MenuMove.cursor != 0) {
                            MenuMove.cursor = 0;
                        } else {
                            MenuMove.cursor = 1;
                        }
                    }

                    if (GamePad.Down(PAD_CROSS) != 0) {
                        decided = 1;

                        if (MenuMove.cursor == 0) {
                            stay = 0;

                            switch (MenuMove.mode) {
                                case MENU_MOVE_DUNGEON_ESCAPE:
                                    NowMonstorUnit->GetMonstorNum();

                                    switch (EscapeDungeonMode()) {
                                        case ESCAPE_DNG_POWDER:
                                            BtlMenuStatusPt->LostItem(ITEM_ESCAPE_POWDER);
                                            break;
                                        case ESCAPE_DNG_PAY: {
                                            u16   money = 0;
                                            int   half = 0;
                                            auto *status = static_cast<CUserStatus *>(BtlMenuStatusPt);
                                            money = status->money;
                                            half = money >> 1;

                                            if (money - half >= 0xFFFF) {
                                                status->money = 0xFFFF;
                                            } else {
                                                status->money += -half;
                                            }

                                            break;
                                        }
                                        default:
                                            break;
                                    }

                                    MenuMapJumpMode = 1;
                                    break;
                                case MENU_MOVE_FIRST_DUNGEON:
                                    MenuMapJumpMode = 0xC8;
                                    break;
                                case MENU_MOVE_INTERIOR_OUT:
                                    SetInteriorOutFlag(1);
                                    BtlEffectCt = 0.0f;
                                    MenuMove.state = MENU_MOVE_STATE_INTERIOR_FADE;
                                    MenuMove.counter = 0;
                                    stay = 1;
                                    break;
                                default:
                                    break;
                            }

                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                        } else {
                            MenuMove.state = MENU_MOVE_STATE_CLOSE;
                            BattleMenuFlag = BTLMENU_STATE_MOVE_CLOSE;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        }
                    }

                    switch (MenuMove.mode) {
                        case MENU_MOVE_DUNGEON_ESCAPE: {
                            int escape = EscapeDungeonMode();
                            s16 messages[3] = {0x131, 0x130, 0x12F};
                            message = messages[escape];
                            break;
                        }
                        case MENU_MOVE_FIRST_DUNGEON:
                            message = 0x136;
                            break;
                        case MENU_MOVE_INTERIOR_OUT:
                            message = 0x137;
                            break;
                        default:
                            break;
                    }

                    break;
                case MENU_MOVE_WORLD_MAP:
                case MENU_MOVE_WORLD_MAP_DIRECT:
                    WorldMapMoveKey();

                    // Debug builds mark the first place not yet visited as visited, or its dungeon as entered.
                    if (DebugMode && GamePad.Down2(PAD_CROSS)) {
                        for (int i = 0; i <= 15; i++) {
                            if (GetVisitInfo(i, MenuMove.mode) == 0) {
                                int map = MenuGrobalMapNoTbl[i];
                                printf("nowset mapID = %d\n", map);

                                if (map >= 200) {
                                    map -= 200;
                                }

                                switch (i) {
                                    case WORLD_PLACE_NORUNE:
                                    case WORLD_PLACE_MATATAKI:
                                    case WORLD_PLACE_FIELD_S04:
                                    case WORLD_PLACE_QUEENS:
                                    case WORLD_PLACE_MUSKA_LACKA:
                                    case WORLD_PLACE_FIELD_S32:
                                    case WORLD_PLACE_FIELD_S13:
                                    case WORLD_PLACE_YELLOW_DROPS:
                                    case WORLD_PLACE_FIELD_S30:
                                    case WORLD_PLACE_DEMON_SHAFT:
                                        BtlMenuSaveDataPt->VisitMap(map, 1);

                                        if (BtlMenuSaveDataPt->VisitMap(map, 0) == 0) {
                                            printf(" mapID = %d, visitflag=%d\n", map, BtlMenuSaveDataPt->VisitMap(map, 0));
                                        } else {
                                            printf("mapID = %d ,visitflag=%d\n", map, BtlMenuSaveDataPt->VisitMap(map, 0));
                                        }

                                        break;
                                    default:
                                        BtlMenuSaveDataPt->QuestDungeon(map, 1);
                                        break;
                                }

                                break;
                            }
                        }
                    }

                    if (MenuMove.cursor < 0) {
                        MenuMove.cursor = WORLD_PLACE_FIELD_S04;
                    }

                    if (GamePad.Down(PAD_CROSS) != 0) {
                        decided = 1;
                        MenuMapJumpMode = MenuGrobalMapNoTbl[MenuMove.cursor];

                        if (MenuMove.mode != MENU_MOVE_WORLD_MAP_DIRECT && MapNo == MenuMapJumpMode) {
                            stay = 1;
                            ComMenuSePlay(MENU_SOUND_REFUSE);
                        } else {
                            ComMenuSePlay(MENU_SOUND_CONFIRM);
                            MenuMove.state = MENU_MOVE_STATE_ZOOM_IN;
                            float world[4];
                            float local[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                            MapMoveCursor->GetWorldPosition(world, local);
                            mapmovev[0] = -world[0] / 15.0f;
                            mapmovev[1] = -world[1] / 15.0f;
                            mapmovev[2] = 2.4f;
                        }
                    }

                    message = 0x138;
                    break;
                default:
                    break;
            }

            if (old_cursor != MenuMove.cursor) {
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }

            if (decided == 0 && GamePad.Down(PAD_CIRCLE) != 0) {
                switch (MenuMove.mode) {
                    case MENU_MOVE_DUNGEON_ESCAPE:
                        MenuMapJumpMode = 0;
                        break;
                    case MENU_MOVE_WORLD_MAP_DIRECT:
                        break;
                    case MENU_MOVE_WORLD_MAP:
                    case MENU_MOVE_FIRST_DUNGEON:
                        MenuMapJumpMode = -1;
                        break;
                    case MENU_MOVE_INTERIOR_OUT:
                        SetInteriorOutFlag(0);
                        break;
                    default:
                        break;
                }

                if (MenuMove.mode != MENU_MOVE_WORLD_MAP_DIRECT) {
                    MenuMove.state = MENU_MOVE_STATE_CLOSE;

                    if (MenuMove.mode != MENU_MOVE_WORLD_MAP_DIRECT) {
                        BattleMenuFlag = BTLMENU_STATE_MOVE_CLOSE;
                    }
                }

                ComMenuSePlay(MENU_SOUND_REFUSE);
            }

            break;
        }
        case MENU_MOVE_STATE_INTERIOR_FADE:
            if (!(BtlEffectCt <= 44.0f)) {
                ExitBattleMenu(1);
                stay = 0;
            }

            break;
        default:
            break;
    }

    int names[16] = {0, 1, 2, 3, 4, 5, 6, 7, 13, 8, 9, 10, 11, 14, 12, 15};
    int name = names[MenuMove.cursor] + 0xA;

    if (message == 0x138 && CommonMenuMes3.mes_no[0] != name) {
        CommonMenuMes3.mes_no[0] = name;
        CommonMenuMes3.mes_made = -1;
    }

    if (static_cast<u32>(help - 0x139) < 2 && CommonMenuMes2.mes_no[0] != name) {
        CommonMenuMes2.mes_no[0] = name;
        GetMsgLengthMenu(&CommonMenuMes2, name);
        help = 0x13A;
        CommonMenuMes2.mes_no[0] = name;
        CommonMenuMes2.mes_made = -1;
    }

    if (CommonMenuMes3.mes_made != message) {
        CommonMenuMes3.MakeMesWin(message);
    }

    if (CommonMenuMes2.mes_made != help) {
        CommonMenuMes2.MakeMesWin(help);
    }

    return stay;
}

void DrawMenuMove() {
    PortDrawMenuMove();
}

void DrawEscapeItem(int x, int y, int alpha) {
    float widths[7] = {8.6f, 9.0f, 9.0f, 11.0f, 9.0f, 9.0f, 9.0f};
    int   u = 0;
    int   v = 0;

    float height = 1.0f;
    MenuHelpWinDraw(x, y, widths[BtlMenuNowLang], height, alpha);
    int row_y = y + 0x10;
    MenuTextureReload(BtlMenuReadBlock);

    if (PerBoardTex == nullptr) {
        PerBoardTex = TexManager.GetTexture("perbrd", -1);
    }

    int money = static_cast<CUserStatus *>(BtlMenuStatusPt)->money;
    CommonMoneyBoardDraw(x, y - 0x1E, money, alpha);
    s8        shift[7] = {0, 0, 0, 34, 0, 0, 0};
    int       count = GetNowItemNum(ITEM_ESCAPE_POWDER, MenuItemPackPt);
    CTexture *icon = RetCTex(0xAF, u, v);
    DrawMenu2DSprite(icon, CRect_i_(x + 0x14, row_y, 0x20, 0x20), CRect_i_(u, v, 0x20, 0x20), alpha);
    DrawMenu2DSprite(BtStatus, CRect_i_(x + 0x86 + shift[BtlMenuNowLang], row_y + 8, 0x10, 0x10), CRect_i_(0x60, 0x58, 0x10, 0x10), alpha);
    int number_x = x + 0xA2 + shift[BtlMenuNowLang];
    number_x += GetNumberKeta(count) * 6;
    DrawMenuNumber(count, number_x, row_y + 8, StayTex, NumberSprite[1], 2, alpha);
    s16 messages[7] = {0x138, 0x1A8, 0x1A8, 0x1A8, 0x1A8, 0x1A8, 0x1A8};

    if (CommonMenuMes1.mes_made != messages[BtlMenuNowLang]) {
        CommonMenuMes1.mes_no[0] = 0x113;
        CommonMenuMes1.MakeMesWin(messages[BtlMenuNowLang]);
    }

    if (BtlMenuNowLang > LANG_JAPANESE) {
        row_y += 0x10;
        CommonMenuMes1.line_pos[0].x = x + 0x38;
        CommonMenuMes1.line_pos[0].y = row_y - 0x14;
        CommonMenuMes1.line_pos[1].x = x + 0x3C;
        CommonMenuMes1.line_pos[1].y = row_y;
    }

    MenuTextureReload(CommonMenuMes1.tex_block);
    CommonMenuMes1.edge_alpha = alpha;
    CommonMenuMes1.stay_frame = false;
    DrawMenuClsMes(&CommonMenuMes1, x + 0x38, row_y - 0x10);
}

namespace {

/**
 * Appends the world map archive name of a region to a path.
 */
void AddWorldMapFileName(std::string &path, int region) {
    constexpr std::array<std::string_view, 6> files = {"matatagi.pac", "queen.pac", "musuka.pac", "moon.pac", "dark.pac", "lastdng.pac"};

    path += files[region];
}

void StartLoadWorldMap(int region, u_long128 *buffer) {
    std::array<char, 64> lang_path{};
    int                  size = 0;

    if (region < 0 || region > 5) {
        return;
    }

    GetPathReadDifferntLang(lang_path.data());
    std::string path = std::string(lang_path.data()) + "map/";
    AddWorldMapFileName(path, region);
    StartReadBG();
    LoadFileBG(path.data(), buffer, &size);
}

} // namespace

int LoadWorldMap() {
    if (ReadBGSync() == 0) {
        BG_READ_INFO     *file = GetReadBGFile(0);
        std::string       img = std::format("w_map_1{}.img", MenuMove.load_map + 1);
        std::string       cfg = std::format("w_map1{}.cfg", MenuMove.load_map + 1);
        LOADTEXTURE_INFO2 textures[5] = {
            {"#frame_imagemove#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
            {nullptr,                                        0, 0},
            {nullptr,                                        0, 0},
            {nullptr,                                        0, 0},
            {nullptr,                                        0, 0}
        };
        textures[0].block_no = MenuMove.tex_block;
        textures[1].block_no = MenuMove.tex_block;
        textures[2].block_no = MenuMove.tex_block;
        textures[3].block_no = MenuMove.tex_block;
        textures[1].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "w_map_02.img", nullptr));
        textures[2].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), img.data(), nullptr));
        textures[3].name = reinterpret_cast<char *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "menumove.img", nullptr));
        TexManager.DeleteTextureBlock(MenuMove.tex_block);
        int blocks[2] = {0, -1};
        blocks[0] = MenuMove.tex_block;
        MenuTextureDelete(blocks);
        TexManager.CleanUpTextureList();
        BtlMenuTexBlockEnter();
        TexManager.LoadTextureBlockEX(-1, textures);
        MenuMoveTex = TexManager.GetTexture("menumove", -1);

        if (MenuMove.mode == MENU_MOVE_WORLD_MAP_DIRECT) {
            InitMenuMesSet(MENU_MES_SET_WORLD_MAP, reinterpret_cast<short *>(GetPackFile(reinterpret_cast<u_int *>(file->buffer), "allmenu.mes", nullptr)));
        }

        u_long128 *model = file->buffer + ((((file->size >> 6) + 1) << 6) >> 4);
        MenuCharaFrame.Initialize();
        MenuExCashBuffer.base = reinterpret_cast<u_char *>(model);
        MenuExCashBuffer.limit = 0xCF80;
        MenuExCashBuffer.used = 0;
        MenuCharaFrame.LoadPackData(reinterpret_cast<u_int *>(file->buffer), cfg.data(), &MenuExCashBuffer, &MenuExCashBuffer, nullptr);
        mapo[2] = 0.0f;
        mapo[1] = 0.0f;
        mapo[0] = 0.0f;
        mapo[3] = 1.0f;
        MenuCharaFrame.SetPosition(mapo);
        MenuCharaFrame.motion_no = 0;
        MenuCharaFrame.motion_flags = 2;
        MenuCharaFrame.motion_speed = -1.0f;
        MapMoveCursor = MenuCharaFrame.frame->SearchFrame(TownOrDngPos[MenuMove.cursor].frame);
        return 1;
    }

    return 0;
}

namespace {

/**
 * Steps and draws the party's marker over the world map.
 */
void LocalDrawWorldMap() {
    MenuCharaFrame.SetScale(2.2f, 2.2f, 2.2f);
    MenuCharaFrame.SetPosition(mapo);
    MenuCharaFrame.Step();
    MenuCharaFrame.Draw();
}

/**
 * Draws the world map with the party's marker over it.
 */
void DrawWorldMap(int alpha) {
    MenuTextureReload(MenuMove.tex_block);
    MenuPolygonDraw(alpha, LocalDrawWorldMap);
}

} // namespace

void MenuDataSwap(MAP_JUMP_COMPARE *first, MAP_JUMP_COMPARE *second) {
    MAP_JUMP_COMPARE saved{};

    memcpy(&saved, first, sizeof(MAP_JUMP_COMPARE));
    memcpy(first, second, sizeof(MAP_JUMP_COMPARE));
    memcpy(second, &saved, sizeof(MAP_JUMP_COMPARE));
}

int GetNearWorldPos(int direction, int *position) {
    if (direction == 0) {
        return -1;
    }

    int            up = 0;
    int            down = 0;
    int            right = 0;
    int            left = 0;
    WORLD_MAP_POS *from = &TownOrDngPos[MenuMove.cursor];
    float          dx = 0.0f;
    float          dy = dx;
    up = direction & 1;

    if (up) {
        dy -= 1.0f;
    }

    down = direction & 2;

    if (down) {
        dy += 1.0f;
    }

    right = direction & 8;

    if (right) {
        dx += 1.0f;
    }

    left = direction & 4;

    if (left) {
        dx -= 1.0f;
    }

    float length = sqrt(dx * dx + dy * dy);

    if (length != 0.0f) {
        dx /= length;
    }

    if (length != 0.0f) {
        dy /= length;
    }

    MAP_JUMP_COMPARE places[16];

    for (int i = 0; i < 16; i++) {
        MAP_JUMP_COMPARE *place = &places[i];
        place->index = i;
        WORLD_MAP_POS *pos = &TownOrDngPos[i];

        if (pos->visited <= 0 || i == MenuMove.cursor) {
            place->reachable = 0;
        } else {
            place->reachable = 1;
        }

        place->dx = pos->x - from->x;
        place->dy = pos->y - from->y;
        place->distance_sq = place->dx * place->dx + place->dy * place->dy;
        float distance = sqrt(place->distance_sq);
        float nx = place->dx;
        float ny = place->dy;

        if (distance != 0.0f) {
            nx /= distance;
            ny /= distance;
        }

        place->alignment = nx * dx + ny * dy;
    }

    for (int i = 0; i < 16;) {
        int               swapped = 0;
        MAP_JUMP_COMPARE *place = &places[i];

        for (int j = i + 1; j < 16; j++) {
            MAP_JUMP_COMPARE *other = &places[j];

            if (place->alignment < other->alignment) {
                MenuDataSwap(place, other);
                swapped = 1;
                break;
            }
        }

        if (!swapped) {
            i++;
        }
    }

    int nearest = -1;
    int nearest_sq = 600000;

    for (const MAP_JUMP_COMPARE &place : places) {
        if (place.reachable > 0 && (!up || place.dy < 0) && (!down || place.dy > 0) && (!right || place.dx > 0) && (!left || place.dx < 0) && place.distance_sq < nearest_sq) {
            nearest = place.index;
            nearest_sq = place.distance_sq;
        }
    }

    return nearest;
}

namespace {

/**
 * Moves the world-map cursor to the place nearest the direction pressed.
 */
int WorldMapMoveKey() {
    int position[2];

    position[0] = SysCur[0];
    position[1] = SysCur[1];
    int direction = 0;

    if (GamePad.Down(PAD_UP) != 0) {
        direction |= 1;
    }

    if (GamePad.Down(PAD_DOWN) != 0) {
        direction |= 2;
    }

    if (GamePad.Down(PAD_RIGHT) != 0) {
        direction |= 8;
    }

    if (GamePad.Down(PAD_LEFT) != 0) {
        direction |= 4;
    }

    if (direction > 0) {
        direction = GetNearWorldPos(direction, position);

        if (direction < 16 && direction >= 0) {
            NextWorldPos = TownOrDngPos[direction].frame;
            MenuMove.cursor = direction;
            MapMoveCursor = MenuCharaFrame.frame->SearchFrame(TownOrDngPos[MenuMove.cursor].frame);
        }
    }

    return 1;
}

} // namespace

/**
 * Marks every world-map place the party has already reached.
 */
void DrawMapCheck(int alpha) {
    CRect_i_ dst(0, 0, 0x10, 0xF);
    CRect_i_ src(0, 0x68, 0x15, 0x15);

    for (int i = 0; i < 16; i++) {
        WORLD_MAP_POS *place = &TownOrDngPos[i];

        if (place->visited > 0) {
            CFrame *frame = MenuCharaFrame.frame->SearchFrame(place->frame);

            if (frame != nullptr) {
                int pos[2];

                Get3DPosTo2DPos(frame, pos);
                dst.x = pos[0];
                dst.y = pos[1];
                src.x = 0;

                if (MenuMove.start_place == i) {
                    src.x += 0x14;
                }

                DrawMenu2DSprite(MenuMoveTex, dst, src, 0x80);
            }
        }
    }
}

namespace {

/**
 * Gives how often a world-map place has been visited, or how far its dungeon has been cleared.
 */
int GetVisitInfo(int place, int menu_mode) {
    int visits = 0;
    int dungeon = 0;
    int map_no = MenuGrobalMapNoTbl[place];

    if (map_no >= 200) {
        map_no -= 200;
    }

    switch (place) {
        case WORLD_PLACE_NORUNE:
        case WORLD_PLACE_MATATAKI:
        case WORLD_PLACE_FIELD_S04:
        case WORLD_PLACE_QUEENS:
        case WORLD_PLACE_MUSKA_LACKA:
        case WORLD_PLACE_FIELD_S32:
        case WORLD_PLACE_FIELD_S13:
        case WORLD_PLACE_YELLOW_DROPS:
        case WORLD_PLACE_FIELD_S30:
        case WORLD_PLACE_DEMON_SHAFT:
            dungeon = 0;
            break;
        default:
            dungeon = 1;
            break;
    }

    if (!dungeon) {
        visits = BtlMenuSaveDataPt->VisitMap(map_no, 0);

        switch (NowGetGameFlagForBtlMenu(menu_mode)) {
            case MENU_MOVE_WORLD_MAP:
            case MENU_MOVE_WORLD_MAP_DIRECT:
                if (map_no == 1) {
                    visits = 1;
                }

                break;
            default:
                break;
        }
    } else {
        visits = BtlMenuSaveDataPt->QuestDungeon(map_no, 0);
    }

    return visits;
}

/**
 * Gives the region whose world map should be read for the party's position.
 */
int IsLoadMapNo() {
    int region = -1;
    s8  map_region[80] = {
        0,
        0,
        1,
        2,
        3,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        1,
        1,
        3,
        2,
        3,
        0,
        0,
        0,
        2,
        0,
        3,
        3,
        3,
        3,
        0,
        2,
        1,
        2,
        3,
        0,
        1,
        4,
        3,
        2,
        1,
        1,
        1,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        5,
        5,
    };

    for (int map_no = 0; map_no < 80; map_no++) {
        if (BtlMenuSaveDataPt->VisitMap(map_no, 0) && map_region[map_no] > region) {
            region = map_region[map_no];
        }
    }

    s8 dungeon_region[6] = {0, 1, 2, 3, 4, 5};

    for (int dungeon = 1; dungeon <= 5; dungeon++) {
        if (BtlMenuSaveDataPt->QuestDungeon(dungeon, 0) && dungeon_region[dungeon - 1] > region) {
            region = dungeon_region[dungeon - 1];
        }
    }

    return region;
}

/**
 * Turns a map number into the world-map place that stands for it.
 */
int MapNoTransFunc(int map_no) {
    int place = 0;

    if (map_no < 10) {
        s16 towns[5] = {WORLD_PLACE_NORUNE, WORLD_PLACE_MATATAKI, WORLD_PLACE_QUEENS, WORLD_PLACE_MUSKA_LACKA, WORLD_PLACE_YELLOW_DROPS};
        place = towns[map_no];
    }

    if (map_no >= 11 && map_no < 200) {
        char fields[69] = {
            WORLD_PLACE_MATATAKI,
            WORLD_PLACE_MATATAKI,
            WORLD_PLACE_MATATAKI,
            WORLD_PLACE_FIELD_S04,
            WORLD_PLACE_FIELD_S04,
            WORLD_PLACE_FIELD_S04,
            WORLD_PLACE_FIELD_S04,
            WORLD_PLACE_FIELD_S04,
            WORLD_PLACE_QUEENS,
            WORLD_PLACE_QUEENS,
            WORLD_PLACE_FIELD_S13,
            WORLD_PLACE_FIELD_S32,
            WORLD_PLACE_FIELD_S13,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_GALLERY_OF_TIME,
            WORLD_PLACE_FIELD_S32,
            WORLD_PLACE_FIELD_S04,
            WORLD_PLACE_FIELD_S13,
            WORLD_PLACE_FIELD_S13,
            WORLD_PLACE_FIELD_S13,
            WORLD_PLACE_FIELD_S13,
            WORLD_PLACE_MATATAKI,
            WORLD_PLACE_SUN_MOON_TEMPLE,
            WORLD_PLACE_SHIPWRECK,
            WORLD_PLACE_FIELD_S32,
            WORLD_PLACE_MOON_SEA,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_SHIPWRECK,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_FIELD_S32,
            WORLD_PLACE_QUEENS,
            WORLD_PLACE_QUEENS,
            WORLD_PLACE_QUEENS,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_FIELD_S30,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_NORUNE,
            WORLD_PLACE_DEMON_SHAFT,
            WORLD_PLACE_DEMON_SHAFT,
        };
        place = fields[map_no - 11];
    }

    if (map_no >= 200) {
        s8 dungeons[6] = {WORLD_PLACE_DIVINE_BEAST_CAVE, WORLD_PLACE_WISE_OWL_FOREST, WORLD_PLACE_SHIPWRECK, WORLD_PLACE_SUN_MOON_TEMPLE, WORLD_PLACE_MOON_SEA, WORLD_PLACE_GALLERY_OF_TIME};
        map_no -= 200;
        place = dungeons[map_no];
    }

    return place;
}

/**
 * Runs the options page and returns to the menu bar when it closes.
 */
void BattleMenuOptionKey() {
    int transition_done = 0;

    switch (BtlEffectFlag) {
        case BTLEFFECT_PAGE_OPEN:
            transition_done = ToFromSelect(0);

            if (transition_done != 0) {
                BtlEffectFlag = BTLEFFECT_NONE;
                BtlEffectCt = 0.0f;
            }

            break;
        case BTLEFFECT_PAGE_CLOSE:
            transition_done = ToFromSelect(1);
            break;
        default:
            break;
    }

    if (BtlEffectFlag != BTLEFFECT_NONE) {
        BtlEffectCt += 1.0f;
    } else {
        BtlEffectCt = 0.0f;
    }

    int closed = MenuOptionKey();

    if (OptionMenuFadeOutStart() != 0) {
        BtlEffectFlag = BTLEFFECT_PAGE_CLOSE;

        if (transition_done != 0 && closed != 0) {
            MenuSelect[0] = BTLMENU_PAGE_OPTION;
            BattleMenuFlag = BTLMENU_STATE_OPTION_CLOSE;
            ForBackMenu();
            BattleMenuFlag = BTLMENU_STATE_MAIN;
        }
    }
}

/**
 * Runs the save page and returns to the menu bar when it closes.
 */
void BattleMenuSaveKey() {
    int transition_done = 0;

    switch (BtlEffectFlag) {
        case BTLEFFECT_PAGE_OPEN:
            transition_done = ToFromSelect(0);

            if (transition_done != 0) {
                BtlEffectFlag = BTLEFFECT_NONE;
                BtlEffectCt = 0.0f;
            }

            break;
        case BTLEFFECT_PAGE_CLOSE:
            transition_done = ToFromSelect(1);
            break;
        default:
            break;
    }

    if (BtlEffectFlag != BTLEFFECT_NONE) {
        BtlEffectCt += 1.0f;
    } else {
        BtlEffectCt = 0.0f;
    }

    MenuSaveKey();

    if (SaveMenuEffectFadeOut() != 0) {
        BtlEffectFlag = BTLEFFECT_PAGE_CLOSE;

        if (transition_done != 0) {
            MenuSelect[0] = BTLMENU_PAGE_SAVE;
            // The previous page's close state, so ForBackMenu leaves MenuSelect[0] one page low.
            BattleMenuFlag = BTLMENU_STATE_OPTION_CLOSE;
            ForBackMenu();
            BattleMenuFlag = BTLMENU_STATE_MAIN;
        }
    }
}

} // namespace

void Btl_BattleManualInit(int *result, u_long128 *load_buffer) {
    InitMenuManual(result, load_buffer);
}

int BattleManualKey() {
    int transition_done = 0;

    switch (BtlEffectFlag) {
        case BTLEFFECT_PAGE_OPEN:
            transition_done = ToFromSelect(0);

            if (transition_done != 0) {
                BtlEffectFlag = BTLEFFECT_NONE;
                BtlEffectCt = 0.0f;
            }

            break;
        case BTLEFFECT_PAGE_CLOSE:
            transition_done = ToFromSelect(1);
            break;
        default:
            break;
    }

    if (BtlEffectFlag != BTLEFFECT_NONE) {
        BtlEffectCt += 1.0f;
    } else {
        BtlEffectCt = 0.0f;
    }

    MenuManualKey();

    if (GetNowManualMenuMode() == MANUAL_MODE_CLOSING) {
        BtlEffectFlag = BTLEFFECT_PAGE_CLOSE;

        if (transition_done != 0) {
            MenuSelect[0] = BTLMENU_PAGE_MANUAL;
            // The previous page's close state, so ForBackMenu leaves MenuSelect[0] one page low.
            BattleMenuFlag = BTLMENU_STATE_SAVE_CLOSE;
            ForBackMenu();
            BattleMenuFlag = BTLMENU_STATE_MAIN;
        }
    }

    return 0;
}

namespace {

void BattleManualDraw() {
    MenuManualDraw();
}

/**
 * Draws a current-over-maximum pair, reddening it as the value runs low.
 */
void DrawStatusNumberNowAndMax(int *values, int x, int y, int color, int alpha) {
    RECT digits = NumberSprite[0];

    if (values[0] < 0.3f * values[1]) {
        digits.y += 12;
    }

    int end = DrawMenuNumber(values[1], x, y, StayTex, digits, color, alpha);
    DrawMenu2DSprite(StayTex, CRect_i_(end - 11, y, 12, 12), CRect_i_(0x78, digits.y, 12, 12), alpha);
    DrawMenuNumber(values[0], end - 11, y, StayTex, digits, color, alpha);
}

} // namespace

void DrawWepHole(int x, int y, WEAPON_HAVE *weapon, int selected, int alpha) {
    if (x < 10 || x > 620) {
        return;
    }

    if (weapon == nullptr) {
        return;
    }

    WEAPON_DATA *data = GetWeaponData(weapon->item_no);
    u8           colors[3][3] = {
        {0x80, 0x80, 0x80},
        {0x28, 0x28, 0xA0},
        {0xFF, 0xE8, 0x02},
    };
    x += 10.0f;

    for (int i = 0; i < 6; i++) {
        if (data->hole[i] - 1 < 0) {
            return;
        }

        if (weapon->attach_kind[i] == 3) {
            float width = 26.0f;
            float height = 12.0f;
            DrawMenu2DSprite(WepStatus, CRect_i_(x + 8, y + 16, width, height), CRect_i_(0xD4, 0x134, 0x1A, 0xD), alpha);
        }

        int src_y = 0xE0;
        int filled = 0;

        if (weapon->attach[i].item_no > 0) {
            src_y += 0x2A;
            filled = 1;
        }

        CRect_i_ src(0xD4, src_y, 0x2A, 0x2A);
        DrawMenu2DSprite(WepStatus, CRect_i_(x + 1, y + 2, 0x2A, 0x29), src, 0, 0, 0, alpha * 80 >> 7);
        u8 *color = nullptr;

        if (data->hole[i] == 2) {
            color = colors[1];

            if (filled) {
                color = colors[2];
            }
        } else {
            color = colors[0];
        }

        DrawMenu2DSprite(WepStatus, CRect_i_(x, y + 1, 0x2A, 0x29), src, color[0], color[1], color[2], alpha);
        x += 0x2A;
    }
}

void MenuClsMes::InitMes() {
    message = &EastKingMsgCls;
}

void MenuClsMes::InitData() {
    draw_x = 366;
    draw_y = 166;
    mode = MENU_CLSMES_OFF;
    option_flags = 0;
    option_count = 0;
    option_start = 0;
}

void MenuClsMes::SetBuffInfo(short *buffer) {
    ClsMes *mes = message;

    mes->text_columns = MES_WIN_COLUMNS;
    mes->text_rows = 10;
    mes->text_len = 0;
    mes->text_width = 0;
    mes->text_height = 0;
    mes->fade = 0.0f;
    mes->fade_in = true;
    mes->text_rate = mes->text_rate_set;
    mes->waiting = false;
    mes->text_at = 0.0f;
    mes->text_no = 0;
    mes->text_from = 0;
    mes->page_from = 0;
    mes->InitMesWinTbl();
    mes->clut_now = mes->clut_default;
    mes->wait = 0;
    mes->blink = 0;
    mes->auto_page_wait = 0;
    mes->mes_made = -1;
    mes->edge_alpha = 0x80;

    for (int &message : mes->mes_no) {
        message = -1;
    }

    for (int &value : mes->values) {
        value = 0;
    }

    mes->value = 0;
    mes->value_signed = false;
    mes->value_show = true;
    mes->value_narrow = false;
    mes->space_width = -1;
    mes->space_area = -1;
    mes->cursor_row = -1;
    mes->cursor_y = 0;
    mes->cursor_lit = false;

    for (auto &position : mes->line_pos) {
        position.x = -1;
        position.y = -1;
    }

    message->tex_buff = work_area;
    message->SetBuff_system(SystemMes);
    message->SetBuff(buffer);
    message->tex_block = 0x1A;
    ClsMes *window = message;
    window->auto_pos = MES_POS_NONE;
    message->Preset(MES_PRESET_SYSTEM);
    message->narrow_gaiji_set = 2;
    message->style = MES_EDGE_BLACK;
    message->rows = 9;
    message->stay_frame = false;
    message->char_width = GetMenuCommonFontW(BtlMenuNowLang, -1);
    message->value_show = false;
    message->unk_02C = 0x100;
    message->init_030 = 0x100;
    InitData();
}

void MenuClsMes::NowWeaponStatus(WEAPON_HAVE *selected_weapon) {
    weapon = selected_weapon;
    option_flags = 1;
    option_count = 0;

    if (selected_weapon != nullptr) {
        option_flags = selected_weapon->flags;

        for (auto &slot : weapon->attach) {
            ATTACH_LIST *attachment = &slot;
            s16          attachment_flags = attachment->sphere_flags;

            if (attachment_flags != 0 && attachment_flags != 1) {
                option_flags |= attachment_flags;
            }
        }

        option_flags = CheckWeaponOptionStatus(option_flags);
        option_count = 0;
        int clear_slot = 0;
        int changed = 0;

        for (int bit = 1; bit <= 13; bit++) {
            if (option_flags & (1 << bit)) {
                int     message_no = bit + 69;
                ClsMes *window = message;
                int     index = option_count;

                if (message_no != window->mes_no[index]) {
                    changed = 1;
                }

                window->mes_no[index] = message_no;
                option_count++;
            }
        }

        for (clear_slot = option_count; clear_slot < 10; clear_slot++) {
            ClsMes *window = message;
            window->mes_no[clear_slot] = 0;
        }

        if (changed != 0) {
            message->mes_made = -1;
            message->MakeMesWin(422);
        }
    }
}

void MenuClsMes::Step() {
    if (message != nullptr) {
        switch (mode) {
            case MENU_CLSMES_OFF:
                break;
            case MENU_CLSMES_OPTIONS:
            case MENU_CLSMES_UNK_2:
                NowWeaponStatus(GetNowSelectWeapon());
                message->Step();
                break;
            default:
                break;
        }
    }
}

namespace {

/**
 * Shifts a message line's x left by five pixels for each of its characters.
 */
int MesLineX(int x, int len) {
    return x - len * 5;
}

} // namespace

void MenuClsMes::Draw1(int x, int y, int draw_alpha) {
    if (message != nullptr && weapon != nullptr) {
        int row_y = y;
        int option = 0;

        switch (mode) {
            case MENU_CLSMES_OFF:
                break;
            case MENU_CLSMES_OPTIONS:
            case MENU_CLSMES_UNK_2:
                if (option_flags == 0 || option_flags == 1) {
                    return;
                }

                row_y -= option_count * 10;
                DrawMenu2DSprite(PerBoardTex, CRect_i_(x + 0x1E, row_y - 0x17, 0x52, 0x17), CRect_i_(0x80, 0x68, 0x52, 0x18), alpha);
                DrawMenu2DSprite(PerBoardTex, CRect_i_(x - 0x24, y - 10, 0x20, 0x20), CRect_i_(0x80, 0x80, 0x20, 0x20), alpha);
                CRect_i_  left(0x20, 0x80, 0x20, 0x18);
                CRect_i_  middle(0x40, 0x80, 0x14, 0x18);
                CRect_i_  right(0x78, 0x80, 0x8, 0x18);
                CRect_i_  icon_src(0xEC, 0x50, 0x14, 0x14);
                CTexture *face = TexManager.GetTexture("charaface", -1);

                for (option = 1; option <= 13; option++) {
                    if (option == 8) {
                        icon_src.x -= icon_src.width;
                    }

                    if (option_flags & (1 << option)) {
                        if (IsWeaponOptionGoodOrBad(option)) {
                            left.y = middle.y = right.y = 0x80;
                        } else {
                            left.y = middle.y = right.y = 0x68;
                        }

                        int icon = option;

                        if (option > 7) {
                            icon = option - 7;
                        }

                        if (option == 8 && icon == 1) {
                            icon++;
                        }

                        if (option == 9 && icon == 2) {
                            icon--;
                        }

                        icon_src.y = icon_src.height * (icon - 1) + 0x50;
                        DrawMenu2DSprite(PerBoardTex, CRect_i_(x, row_y, left.width, left.height), left, alpha);
                        DrawMenu2DSprite(PerBoardTex, CRect_i_(x + left.width, row_y, 100, middle.height), middle, alpha);
                        DrawMenu2DSprite(PerBoardTex, CRect_i_(x + left.width + 100, row_y, right.width, right.height), right, alpha);
                        DrawMenu2DSprite(face, CRect_i_(x + 2, row_y + 2, 0x14, 0x15), icon_src, alpha);
                        row_y += left.height;
                    }
                }

                MenuTextureReload(message->tex_block);
                x += 0xB;
                row_y = y - option_count * 10;

                for (option = 0; option < option_count; option++, row_y += left.height) {
                    SetLinePos(message, option, MesLineX(x + 0x11, message->GetMesLen_system(message->mes_no[option])), row_y);
                }

                message->edge_alpha = alpha;
                message->DrawMesWin();
                break;
        }
    }
}

} // namespace dc::game
