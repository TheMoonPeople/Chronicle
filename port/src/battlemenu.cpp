#include "battlemenu.hpp"

#include "clsmes.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_manual.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "weaponlevelup.hpp"

// BattleMenuDraw is replaced only to drop retail's VU1 program call; the renderer has no programs.
// The static helpers it calls cannot be reached from here, so they are carried over as retail has
// them, with the locals MWCC left unset at zero as the port's build of ps2/src has them.
namespace {

int GetMenuModeMax() {
    int icon_count = 0;

    switch (BtlMenuMode) {
        case BATTLE_MENU_MODE_DUNGEON:
            icon_count = 7;
            break;
        case BATTLE_MENU_MODE_TOWN:
            icon_count = 8;
            break;
    }

    if (GetGameFlagForManualMenu() == 0) {
        icon_count--;
    }

    return icon_count;
}

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
        }

        icons[4] = special_icon;
    }
}

void DrawBtlMenuBar() {
    int alpha;
    int icon_alpha;
    int i;
    int x;
    int menu_flag;
    int brightness;
    int selected;
    int y;

    if (BtlEffectFlag == BTLEFFECT_PAGE_CLOSE || BattleMenuFlag == BTLMENU_STATE_APPEAR) {
        alpha = 10.0f * BtlEffectCt;

        if (alpha >= 128) {
            alpha = 128;
        }
    }

    if (BtlEffectFlag == BTLEFFECT_PAGE_OPEN || BattleMenuFlag == BTLMENU_STATE_EXIT) {
        alpha = 128.0f - 10.0f * BtlEffectCt;

        if (alpha < 0) {
            alpha = 0;
        }
    } else {
        alpha = 128;
    }

    NowGetGameFlagForBtlMenu(BtlMenuMode);
    int icon_count = GetMenuModeMax();
    int icons[8] = {};
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
            if ((menu_flag == BTLMENU_STATE_APPEAR || menu_flag == BTLMENU_STATE_EXIT) &&
                menu_flag == BTLMENU_STATE_MOVE_OPEN) {
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

int BtlMenuDrawSpecialFlag(int flag) {
    if ((BattleMenuFlag == BTLMENU_STATE_ATLA_OPEN || BattleMenuFlag == BTLMENU_STATE_ATLA ||
         BattleMenuFlag == BTLMENU_STATE_ATLA_CLOSE) &&
        GetMenuAtraEventFlag() != 0) {
        flag = 0;
    }

    if (MenuChara.state == MENU_CHARA_CHANGE_LOAD || MenuChara.state == MENU_CHARA_CHANGE_EXIT) {
        flag = 0;
    }

    return flag;
}

char g_frame_image_name[] = "frame_image";

} // namespace

PC_OVERRIDE void BattleMenuDraw() {
    int text_x = 0;
    int text_y = 0;

    setbilinear(0);
    MenuWorldTrans(&MenuCamera);
    MenuTextureReload(BtlMenuReadBlock);
    CTexture  frame = *TexManager.GetTexture(g_frame_image_name, -1);
    CTexture *texture = &frame;
    int       tcc = 0;

    if (texture != NULL) {
        ((sceGsTex0 *) &frame.tex0)->bits.tcc = tcc;
    } else {
        return;
    }

    int bright = 0x40;

    switch (BattleMenuFlag) {
        case BTLMENU_STATE_APPEAR:
            bright = (int) (128.0f - 4.0f * BtlEffectCt);

            if (bright < 0x40) {
                bright = 0x40;
            }

            break;
        case BTLMENU_STATE_EXIT:
            bright += 4.0f * BtlEffectCt;

            if (bright > 0x80) {
                bright = 0x80;
            }

            break;
    }

    DrawMenu2DSprite(&frame, MenuDispRc, MenuDispRc, bright, bright, bright, 0x80);

    switch (BattleMenuFlag) {
        case BTLMENU_STATE_MAIN:
            DrawBattleMain();
            break;
        case BTLMENU_STATE_CHARA:
        case BTLMENU_STATE_CHARA_OPEN:
        case BTLMENU_STATE_CHARA_CLOSE:
        case BTLMENU_STATE_UNK_1A:
            DrawCharaSelect();
            break;
        case BTLMENU_STATE_WEAPON:
        case BTLMENU_STATE_WEAPON_OPEN:
        case BTLMENU_STATE_WEAPON_CLOSE:
            WeaponMenuDraw();
            break;
        case BTLMENU_STATE_ITEM:
        case BTLMENU_STATE_ITEM_OPEN:
        case BTLMENU_STATE_ITEM_CLOSE:
            ItemMenuModeDraw();
            break;
        case BTLMENU_STATE_ATLA:
        case BTLMENU_STATE_ATLA_OPEN:
        case BTLMENU_STATE_ATLA_CLOSE:
            DrawMenuAtoraSelect();
            break;
        case BTLMENU_STATE_MOVE:
        case BTLMENU_STATE_MOVE_OPEN:
        case BTLMENU_STATE_MOVE_CLOSE:
            DrawMenuMove();
            break;
        case BTLMENU_STATE_OPTION:
        case BTLMENU_STATE_OPTION_OPEN:
        case BTLMENU_STATE_OPTION_CLOSE:
            DrawMenuOption();
            setbilinear(0);
            break;
        case BTLMENU_STATE_SAVE:
        case BTLMENU_STATE_SAVE_OPEN:
        case BTLMENU_STATE_SAVE_CLOSE:
            DrawMenuSave(g_frame_image_name);
            setbilinear(0);
            break;
        case BTLMENU_STATE_MANUAL:
        case BTLMENU_STATE_MANUAL_OPEN:
        case BTLMENU_STATE_MANUAL_CLOSE:
            MenuManualDraw();
            break;
    }

    CursorVibeCnt++;

    if (CursorVibeCnt >= 0x405F7E00) {
        CursorVibeCnt = 0;
    }

    int draw_help = BtlMenuDrawSpecialFlag(BtlWakuMake2);

    if (BtlMenuReadEndFlag != 0) {
        switch (BattleMenuFlag) {
            case BTLMENU_STATE_EXIT:
                BtlHelpWinAlpha -= 8;

                if (BtlHelpWinAlpha < 0) {
                    BtlHelpWinAlpha = 0;
                }

                break;
            default:
                BtlHelpWinAlpha += 9;

                if (BtlHelpWinAlpha > 0x80) {
                    BtlHelpWinAlpha = 0x80;
                }

                break;
        }

        if (draw_help != 0) {
            MenuHelpWinDraw((int) HelpWinHead[0], (int) HelpWinHead[1], BtlHelpWinW, BtlHelpWinH, BtlHelpWinAlpha);
        }
    }

    MenuTextureReload(CommonMenuMes2.tex_block);
    GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
    CommonMenuMes2.text_x = (int) (HelpWinHead[0] + text_x);
    CommonMenuMes2.text_y = (int) (HelpWinHead[1] + text_y);

    if (BtlMenuReadEndFlag != 0 && draw_help != 0) {
        CommonMenuMes2.stay_frame = false;
        CommonMenuMes2.edge_alpha = BtlHelpWinAlpha;
        CommonMenuMes2.Step();
        CommonMenuMes2.DrawMesWin();
    }

    setbilinear(0);
    draw_help = BtlMenuDrawSpecialFlag(1);

    if (draw_help != 0) {
        DrawBtlMenuBar();
    }

    if (WepMenu.mode != WEP_MENU_BUILDUP_SELECT) {
        MenuWepLevelUp.Step();
        MenuWepLevelUp.Draw();
    }

    if (GetInteriorOutFlag() != 0) {
        BtlEffectCt += 1.0f;
        int fade = (int) (3.0f * BtlEffectCt);

        if (fade > 0x80) {
            fade = 0x80;
        }

        AllFadeForMenu(fade);
    }

    setbilinear(1);
}
