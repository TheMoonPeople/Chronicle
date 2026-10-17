#include "battlemenu/internal.hpp"
#include "save_slots.hpp"
#include "savedata.hpp"
#include "ui_framework/lang.hpp"

namespace {

/**
 * Choices available in the town Save/Load confirmation modal.
 */
enum class SaveLoadChoice : int {
    Save = 0,
    Load = 1,
};

/**
 * Choices available in the Quit Game confirmation modal.
 */
enum class QuitChoice : int {
    Yes = 0,
    No  = 1,
};

/**
 * Number of icons on the dungeon Battle Menu ring.
 */
constexpr int kDungeonRingIconCount = 8;

/**
 * Number of icons on the town Battle Menu ring.
 */
constexpr int kTownRingIconCount = 9;

/**
 * Width of the right-hand help box in logical pixels.
 */
constexpr int kHelpBoxWidth = 276;

/**
 * Width ratio converting a ring caption measurement to selection bracket width.
 */
constexpr float kCaptionToMessageRatio = 1.32f;

/**
 * Whether the Quit Game confirmation modal is currently open.
 */
bool quit_confirm_active = false;

/**
 * Selected option index in the Quit Game confirmation modal.
 */
int quit_confirm_choice = static_cast<int>(QuitChoice::No);

/**
 * Animated hand cursor for the Quit Game confirmation modal.
 */
UI::Cursor quit_cursor;

/**
 * Whether the Save/Load selection modal is currently open.
 */
bool saveload_modal_active = false;

/**
 * Selected option index in the Save/Load selection modal.
 */
int saveload_modal_choice = static_cast<int>(SaveLoadChoice::Save);

/**
 * Animated hand cursor for the Save/Load selection modal.
 */
UI::Cursor saveload_cursor;

/**
 * Whether the save screen was opened in load mode from the town ring.
 */
bool ingame_load_active = false;

/**
 * Whether an in-game save load has completed and awaits a town reload.
 */
bool ingame_load_pending = false;

/**
 * Name of the captured background frame texture.
 */
char frame_image_name[] = "frame_image";

/**
 * Returns the number of icons shown on the Battle Menu ring for the active mode.
 */
int GetMenuModeMax() {
    return (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) ? kDungeonRingIconCount : kTownRingIconCount;
}

/**
 * Populates the array of sprite icon indices for the current Battle Menu mode.
 */
void BtlMenuMekeIconInfo(int *icons, int /*menu_mode*/) {
    constexpr s16 kCustomDrawTbl[2][kTownRingIconCount] = {
        {0, 1, 2, 3, 7, 4, 6, 12,  0},
        {0, 1, 2, 3, 8, 5, 4,  6, 12}
    };

    const int icon_count = GetMenuModeMax();
    for (int i = 0; i < icon_count; i++) {
        icons[i] = kCustomDrawTbl[BtlMenuMode][i];
    }

    if (BtlMenuMode != BATTLE_MENU_MODE_DUNGEON) {
        switch (NowGetGameFlagForBtlMenu(BtlMenuMode)) {
            case MENU_MOVE_LOCKED:
            case MENU_MOVE_FIRST_DUNGEON:
                icons[4] = 9;
                break;
            case MENU_MOVE_WORLD_MAP:
                icons[4] = 8;
                break;
            case MENU_MOVE_INTERIOR_OUT:
                icons[4] = 12;
                break;
            default:
                icons[4] = 0;
                break;
        }
    }
}

/**
 * Computes the resting screen coordinates of a ring icon by its slot index.
 */
void GetMainRingIconTarget(int index, float &out_x, float &out_y) {
    const int next     = index + 1;
    int       target_x = next * 16 + 80;
    const int target_y = index * 40 + 54;
    if (next > 4) {
        target_x -= ((index - 4) << 5) + 16;
    }

    out_x = static_cast<float>(target_x);
    out_y = static_cast<float>(target_y);
}

/**
 * Computes the animated screen coordinates of a ring icon, including the 9th slot.
 */
void GetSafeIconPos(int index, float &out_x, float &out_y) {
    const int  menu_flag    = BattleMenuFlag;
    const bool in_main_ring = (menu_flag == BTLMENU_STATE_MAIN ||
                               menu_flag == BTLMENU_STATE_APPEAR ||
                               menu_flag == BTLMENU_STATE_EXIT);

    if (!in_main_ring) {
        if (index >= 0 && index < kDungeonRingIconCount) {
            out_x = NorMenuIcon[index].x;
            out_y = NorMenuIcon[index].y;
        } else {
            out_x = -200.0f;
            out_y = -200.0f;
        }
        return;
    }

    float base_x = 0.0f;
    float base_y = 0.0f;
    GetMainRingIconTarget(index, base_x, base_y);
    const float slide_dx = NorMenuIcon[0].x - 96.0f;
    out_x                = base_x + slide_dx;
    out_y                = base_y;
}

/**
 * Draws the Battle Menu icon ring and its runtime captions.
 */
void DrawBtlMenuBar() {
    int alpha = 128;
    if (BtlEffectFlag == BTLEFFECT_PAGE_CLOSE || BattleMenuFlag == BTLMENU_STATE_APPEAR) {
        alpha = std::min(128, static_cast<int>(10.0f * BtlEffectCt));
    } else if (BtlEffectFlag == BTLEFFECT_PAGE_OPEN || BattleMenuFlag == BTLMENU_STATE_EXIT) {
        alpha = std::max(0, static_cast<int>(128.0f - 10.0f * BtlEffectCt));
    }

    NowGetGameFlagForBtlMenu(BtlMenuMode);
    const int icon_count                = GetMenuModeMax();
    int       icons[kTownRingIconCount] = {};
    BtlMenuMekeIconInfo(icons, BtlMenuMode);

    setbilinear(0);
    MenuTextureReload(BtlMenuReadBlock);

    const int  menu_flag    = BattleMenuFlag;
    const bool in_main_ring = (menu_flag == BTLMENU_STATE_MAIN ||
                               menu_flag == BTLMENU_STATE_APPEAR ||
                               menu_flag == BTLMENU_STATE_EXIT);

    if (!in_main_ring &&
        (menu_flag == BTLMENU_STATE_SAVE || menu_flag == BTLMENU_STATE_SAVE_OPEN || menu_flag == BTLMENU_STATE_SAVE_CLOSE) &&
        (SaveMenu.mode == SAVE_MENU_MODE_LOAD || ingame_load_active)) {
        return;
    }

    for (int i = 0; i < icon_count; i++) {
        int       icon_alpha = alpha;
        const int brightness = (MenuWarningMsgFlag != 0) ? 64 : 128;

        if (!in_main_ring && i != MenuSelect[1]) {
            continue;
        }

        float icon_x = 0.0f;
        float icon_y = 0.0f;
        GetSafeIconPos(i, icon_x, icon_y);

        if (icon_x < -120.0f) {
            continue;
        }

        int selected = 0;
        int x        = static_cast<int>(icon_x);
        int y        = static_cast<int>(icon_y + 3.0f);

        if (i == MenuSelect[1]) {
            selected   = 1;
            x          = static_cast<int>(icon_x - 10.0f);
            y          = static_cast<int>(icon_y - 1.0f);
            icon_alpha = 128;
        }

        if (icons[i] >= 0) {
            setbilinear(0);
            MenuTextureReload(BtlMenuReadBlock);
            caption::SetNextCaptionText(UI::GetBattleMenuCaption(i, BtlMenuMode, ingame_load_active));
            DrawMainMenuIcon(x, y, icons[i], selected, brightness, icon_alpha);
        }
    }
}

/**
 * Draws the selection brackets and hand cursor on the active Battle Menu ring item.
 */
void DrawBattleMainOverlay() {
    if (BattleMenuFlag != BTLMENU_STATE_MAIN || quit_confirm_active || saveload_modal_active) {
        return;
    }

    int icons[kTownRingIconCount] = {};
    BtlMenuMekeIconInfo(icons, BtlMenuMode);
    const int       sel_icon = icons[MenuSelect[1]];
    MENU_ICON_INFO *info     = GetMenuIconInfo(sel_icon);

    UI::Label cap_lbl(UI::GetBattleMenuCaption(MenuSelect[1], BtlMenuMode, ingame_load_active),
                      UI::ColorWhite, UI::Alignment::Left, true, kCaptionToMessageRatio);
    int width = cap_lbl.GetWidth() + 0x52;
    if (info != nullptr && width < (info->label_width + 0x4A)) {
        width = info->label_width + 0x4A;
    }
    if (BtlMenuMode != BATTLE_MENU_MODE_DUNGEON && MenuSelect[1] == 5) {
        if (MENU_ICON_INFO *quit_info = GetMenuIconInfo(12)) {
            width = std::max(width, static_cast<int>(quit_info->label_width) + 0x4A);
        }
    }

    float icon_x = 0.0f;
    float icon_y = 0.0f;
    GetSafeIconPos(MenuSelect[1], icon_x, icon_y);

    setbilinear(0);
    MenuTextureReload(BtlMenuReadBlock);
    DrawMenuWaku(icon_x - 18.0f, icon_y - 8.0f, width, 0x22, 0, StayTex, 0x80);
    DrawMenuObjectVibe(static_cast<int>(SysCur[0]), static_cast<int>(SysCur[1]), 1, 0x40);
}

/**
 * Masks the Battle Menu overlay draw flag during Georama events or character transitions.
 */
int BtlMenuDrawSpecialFlag(int flag) {
    if ((BattleMenuFlag == BTLMENU_STATE_ATLA_OPEN || BattleMenuFlag == BTLMENU_STATE_ATLA ||
         BattleMenuFlag == BTLMENU_STATE_ATLA_CLOSE) && GetMenuAtraEventFlag() != 0) {
        flag = 0;
    }
    if (MenuChara.state == MENU_CHARA_CHANGE_LOAD || MenuChara.state == MENU_CHARA_CHANGE_EXIT) {
        flag = 0;
    }
    return flag;
}

/**
 * Draws the Quit Game confirmation modal.
 */
void DrawQuitConfirmModal(int alpha) {
    UI::ConfirmModal::Draw(
        LocalizeText("battlemenu.quit.confirm_title", "\"Quit Game\""),
        LocalizeText("battlemenu.quit.confirm_desc", "Return to desktop?"),
        LocalizeText("battlemenu.quit.yes", "YES"),
        LocalizeText("battlemenu.quit.no", "NO"),
        quit_confirm_choice,
        quit_cursor,
        alpha
    );
}

/**
 * Draws the Save/Load action selection modal.
 */
void DrawSaveLoadChoiceModal(int alpha) {
    std::string title;
    std::string desc;
    std::string save_btn;
    std::string load_btn;
    std::string modal_prompt;
    UI::GetLocalizedSaveLoadStrings(title, desc, save_btn, load_btn, modal_prompt);

    UI::ConfirmModal::Draw(
        title,
        modal_prompt,
        save_btn,
        load_btn,
        saveload_modal_choice,
        saveload_cursor,
        alpha
    );
}

/**
 * Monitors an in-game load operation and schedules a town reload when complete.
 */
void StepInGameLoadMonitor() {
    if (!ingame_load_active) {
        return;
    }

    // Check SaveMenu.loaded before BTLMENU_STATE_MAIN because BattleMenuSaveKey transitions
    // directly to BTLMENU_STATE_MAIN once the fade-out slide finishes.
    if (SaveMenu.loaded != 0 || SaveMenu.result == MENU_SAVE_LOADED) {
        if (BattleMenuFlag == BTLMENU_STATE_MAIN || SaveMenu.step_time > 16 || SaveMenu.result == MENU_SAVE_LOADED) {
            ingame_load_active  = false;
            SaveMenu.loaded     = false;
            ingame_load_pending = true;
            ExitBattleMenu(1);
            BattleMenuFlag = 0;
            BtlEffectFlag  = 0;
            BtlEffectCt    = 0.0f;
        }
        return;
    }

    if (BattleMenuFlag == BTLMENU_STATE_MAIN || BattleMenuFlag == BTLMENU_STATE_EXIT) {
        ingame_load_active = false;
        if (MenuMes.message != nullptr && MenuMes.message->buff != nullptr) {
            InitMenuMesSet(MENU_MES_SET_ALLMENU, MenuMes.message->buff);
            CommonMenuMes2.stay_frame = false;
        }
    }
}

} // namespace

PORT_OVERRIDE void DrawBattleMain() {
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    GetMainRingIconTarget(MenuSelect[1], pos_x, pos_y);

    SysCur[0] += ((pos_x - 44.0f) - SysCur[0]) / 4.0f;
    SysCur[1] += (pos_y - SysCur[1]) / 4.0f;

    BtlWakuMake2              = 1;
    CommonMenuMes2.mes_made   = -1;
    CommonMenuMes2.stay_frame = false;

    if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON) {
        const int limit = GetLimmitMsg();
        if (CommonMenuMes1.mes_made != limit) {
            CommonMenuMes1.MakeMesWin(limit);
        }
        if (limit >= 0) {
            MenuTextureReload(CommonMenuMes1.tex_block);
            CommonMenuMes1.text_x     = 0x50;
            CommonMenuMes1.text_y     = 0x14A;
            CommonMenuMes1.stay_frame = true;
            CommonMenuMes1.Step();
            CommonMenuMes1.DrawMesWin();
        }
    }
}

PORT_OVERRIDE void BattleMenuDraw() {
    StepInGameLoadMonitor();
    if (BattleMenuFlag == 0 && ingame_load_pending) {
        return;
    }

    MenuMouseNoteBattleMenu();

    setbilinear(0);
    MenuWorldTrans(&MenuCamera);
    MenuTextureReload(BtlMenuReadBlock);

    CTexture *raw_frame = TexManager.GetTexture(frame_image_name, -1);
    if (raw_frame == nullptr) {
        return;
    }
    CTexture frame = *raw_frame;
    reinterpret_cast<sceGsTex0 *>(&frame.tex0)->bits.tcc = 0;

    int bright = 0x40;
    if (BattleMenuFlag == BTLMENU_STATE_APPEAR) {
        bright = std::max(0x40, static_cast<int>(128.0f - 4.0f * BtlEffectCt));
    } else if (BattleMenuFlag == BTLMENU_STATE_EXIT) {
        bright = std::min(0x80, static_cast<int>(bright + 4.0f * BtlEffectCt));
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
            setbilinear(0);
            DrawCharaSelect();
            setbilinear(0);
            break;
        case BTLMENU_STATE_WEAPON:
        case BTLMENU_STATE_WEAPON_OPEN:
        case BTLMENU_STATE_WEAPON_CLOSE:
            setbilinear(0);
            WeaponMenuDraw();
            setbilinear(0);
            break;
        case BTLMENU_STATE_ITEM:
        case BTLMENU_STATE_ITEM_OPEN:
        case BTLMENU_STATE_ITEM_CLOSE:
            setbilinear(0);
            ItemMenuModeDraw();
            setbilinear(0);
            break;
        case BTLMENU_STATE_ATLA:
        case BTLMENU_STATE_ATLA_OPEN:
        case BTLMENU_STATE_ATLA_CLOSE:
            setbilinear(0);
            DrawMenuAtoraSelect();
            setbilinear(0);
            break;
        case BTLMENU_STATE_MOVE:
        case BTLMENU_STATE_MOVE_OPEN:
        case BTLMENU_STATE_MOVE_CLOSE:
            setbilinear(0);
            DrawMenuMove();
            setbilinear(0);
            break;
        case BTLMENU_STATE_OPTION:
        case BTLMENU_STATE_OPTION_OPEN:
        case BTLMENU_STATE_OPTION_CLOSE:
            setbilinear(0);
            DrawMenuOption();
            setbilinear(0);
            break;
        case BTLMENU_STATE_SAVE:
        case BTLMENU_STATE_SAVE_OPEN:
        case BTLMENU_STATE_SAVE_CLOSE:
            setbilinear(0);
            DrawMenuSave(frame_image_name);
            setbilinear(0);
            break;
        case BTLMENU_STATE_MANUAL:
        case BTLMENU_STATE_MANUAL_OPEN:
        case BTLMENU_STATE_MANUAL_CLOSE:
            setbilinear(0);
            MenuManualDraw();
            setbilinear(0);
            break;
    }

    CursorVibeCnt = (CursorVibeCnt + 1 >= 0x405F7E00) ? 0 : (CursorVibeCnt + 1);

    const int draw_help = BtlMenuDrawSpecialFlag(BtlWakuMake2);
    if (BtlMenuReadEndFlag != 0) {
        if (BattleMenuFlag == BTLMENU_STATE_MAIN || BattleMenuFlag == BTLMENU_STATE_APPEAR) {
            BtlHelpWinAlpha = std::min(0x80, BtlHelpWinAlpha + 9);
        } else {
            BtlHelpWinAlpha = std::max(0, BtlHelpWinAlpha - 8);
        }
    }

    int text_x = 0;
    int text_y = 0;
    GetMainMenuRightHelpMsgLangOffset(text_x, text_y);

    if (BtlMenuReadEndFlag != 0 && draw_help != 0 &&
        BtlHelpWinAlpha > 0 && !quit_confirm_active && !saveload_modal_active) {
        std::string title;
        std::string desc;
        UI::GetBattleMenuHelpData(MenuSelect[1], BtlMenuMode, title, desc);

        const int box_x = (HelpWinHead[0] > 0) ? static_cast<int>(HelpWinHead[0]) : (640 - kHelpBoxWidth - 20);
        UI::HelpBox::DrawTitled(box_x, 452, kHelpBoxWidth, text_x, title, desc, BtlHelpWinAlpha);
    }

    setbilinear(0);
    if (BtlMenuDrawSpecialFlag(1) != 0) {
        DrawBtlMenuBar();
        DrawBattleMainOverlay();
    }

    if (WepMenu.mode != WEP_MENU_BUILDUP_SELECT) {
        MenuWepLevelUp.Step();
        MenuWepLevelUp.Draw();
    }

    if (saveload_modal_active && BtlHelpWinAlpha > 0) {
        DrawSaveLoadChoiceModal(BtlHelpWinAlpha);
    }

    if (quit_confirm_active && BtlHelpWinAlpha > 0) {
        DrawQuitConfirmModal(BtlHelpWinAlpha);
    }

    MenuMouseDrawOverlay();

    if (GetInteriorOutFlag() != 0) {
        BtlEffectCt += 1.0f;
        AllFadeForMenu(std::min(0x80, static_cast<int>(3.0f * BtlEffectCt)));
    }

    setbilinear(0);
}

PORT_OVERRIDE int BattleMenuSelect() {
    if (MenuWarningMsgFlag != 0) {
        if (GamePad.Down(PAD_CIRCLE | PAD_CROSS) != 0) {
            MenuWarningMsgFlag = 0;
        }
        return 1;
    }

    if (saveload_modal_active) {
        if (GamePad.Down(PAD_UP | PAD_DOWN)) {
            saveload_modal_choice = 1 - saveload_modal_choice;
            ComMenuSePlay(MENU_SOUND_CURSOR);
        } else if (GamePad.Down(PAD_CROSS)) {
            saveload_modal_active = false;
            BtlWakuMake2          = 0;
            MenuSelect[0]         = BTLMENU_PAGE_SAVE;
            u_long128 *buffer     = BtlMenuReadBuf;

            if (saveload_modal_choice == static_cast<int>(SaveLoadChoice::Save)) {
                ingame_load_active = false;
                InitMenuSave(SAVE_MENU_MODE_SAVE, MenuExtendReadBlock, buffer);
            } else {
                ingame_load_active = true;
                InitMenuSave(SAVE_MENU_MODE_LOAD, MenuExtendReadBlock, buffer);
            }

            BattleMenuFlag = MenuSelect[0] + BTLMENU_STATE_OPEN_OFFSET;
            BtlEffectFlag  = BTLEFFECT_PAGE_OPEN;
            BtlEffectCt    = 0;
            ComMenuSePlay(MENU_SOUND_CONFIRM);
        } else if (GamePad.Down(PAD_CIRCLE)) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            saveload_modal_active = false;
        }
        return 1;
    }

    if (quit_confirm_active) {
        if (GamePad.Down(PAD_UP | PAD_DOWN)) {
            quit_confirm_choice = 1 - quit_confirm_choice;
            ComMenuSePlay(MENU_SOUND_CURSOR);
        } else if (GamePad.Down(PAD_CROSS)) {
            if (quit_confirm_choice == static_cast<int>(QuitChoice::Yes)) {
                ComMenuSePlay(MENU_SOUND_CONFIRM);
                SDL_Event event{};
                event.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&event);
            } else {
                ComMenuSePlay(MENU_SOUND_REFUSE);
                quit_confirm_active = false;
            }
        } else if (GamePad.Down(PAD_CIRCLE)) {
            ComMenuSePlay(MENU_SOUND_REFUSE);
            quit_confirm_active = false;
        }
        return 1;
    }

    const int old = MenuSelect[1];
    const int max = GetMenuModeMax();

    if (GamePad.Down(PAD_UP | PAD_LEFT) != 0) {
        if (--MenuSelect[1] < 0) {
            MenuSelect[1] = max - 1;
        }
    }
    if (GamePad.Down(PAD_RIGHT | PAD_DOWN) != 0) {
        if (++MenuSelect[1] > max - 1) {
            MenuSelect[1] = 0;
        }
    }
    if (old != MenuSelect[1]) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if (GamePad.Down(PAD_CROSS)) {
        constexpr int kPages[2][kTownRingIconCount] = {
            {BTLMENU_PAGE_ITEM, BTLMENU_PAGE_WEAPON, BTLMENU_PAGE_CHARA, BTLMENU_PAGE_ATLA, BTLMENU_PAGE_MOVE,
             BTLMENU_PAGE_OPTION, BTLMENU_PAGE_MANUAL, BTLMENU_PAGE_QUIT, BTLMENU_PAGE_NONE},
            {BTLMENU_PAGE_ITEM, BTLMENU_PAGE_WEAPON, BTLMENU_PAGE_CHARA, BTLMENU_PAGE_ATLA, BTLMENU_PAGE_MOVE,
             BTLMENU_PAGE_SAVE, BTLMENU_PAGE_OPTION, BTLMENU_PAGE_MANUAL, BTLMENU_PAGE_QUIT}
        };
        MenuSelect[0]             = kPages[BtlMenuMode][MenuSelect[1]];
        CommonMenuMes1.stay_frame = false;
        CUserStatus *status       = reinterpret_cast<CUserStatus *>(BtlMenuStatusPt);
        const int    chara        = status->cur_chara;
        u_long128   *buffer       = BtlMenuReadBuf;

        switch (MenuSelect[0]) {
            case BTLMENU_PAGE_CHARA:
                if (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON &&
                    status->res_limit_zone_current >= 0 && status->res_limit_zone_current < 6) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    return 1;
                }
                BtlWakuMake2 = 0;
                Btl_InitMenuChara(buffer);
                break;
            case BTLMENU_PAGE_WEAPON:
                BtlWakuMake2 = 1;
                Btl_InitWeaponSelect(WEP_OPEN_FROM_BAR, chara);
                break;
            case BTLMENU_PAGE_ITEM:
                BtlWakuMake2 = 1;
                Btl_InitItemMode(0, chara);
                break;
            case BTLMENU_PAGE_ATLA: {
                BtlWakuMake2  = 1;
                int blocks[2] = {BtlMenuReadBlock, MenuExtendReadBlock};
                int map       = (BtlMenuMode == BATTLE_MENU_MODE_DUNGEON)
                                    ? status->cur_georama
                                    : GetNowMapTransAtraMap(MapNo);
                if (map < 0 || map >= 6) {
                    map = 0;
                }
                InitMenuAtora1(BtlMenuMode, map, blocks, buffer);
                InitMenuAtoraSelect(map);
                break;
            }
            case BTLMENU_PAGE_MOVE: {
                const int flag = NowGetGameFlagForBtlMenu(BtlMenuMode);
                if (flag == MENU_MOVE_LOCKED) {
                    ComMenuSePlay(MENU_SOUND_REFUSE);
                    return 1;
                }
                InitMenuMove(flag, MenuExtendReadBlock, buffer);
                BtlWakuMake2 = 0;
                break;
            }
            case BTLMENU_PAGE_OPTION:
                BtlWakuMake2 = 0;
                InitMenuOption(OPTION_OPEN_GAME_MENU, MenuExtendReadBlock, buffer);
                break;
            case BTLMENU_PAGE_SAVE:
                saveload_modal_active = true;
                saveload_modal_choice = static_cast<int>(SaveLoadChoice::Save);
                saveload_cursor.SnapTo((640 - 100) / 2.0f - 34.0f, (164 + 58 + 10) + 3.0f);
                ComMenuSePlay(MENU_SOUND_CONFIRM);
                return 1;
            case BTLMENU_PAGE_MANUAL: {
                BtlWakuMake2  = 0;
                int blocks[3] = {MenuExtendReadBlock, BtlMenuExReadBlock, BtlMenuManualReadBlock};
                Btl_BattleManualInit(blocks, buffer);
                break;
            }
            case BTLMENU_PAGE_QUIT:
                quit_confirm_active = true;
                quit_confirm_choice = static_cast<int>(QuitChoice::No);
                quit_cursor.SnapTo((640 - 100) / 2.0f - 34.0f, (164 + 58 + 10 + 26 + 6) + 3.0f);
                ComMenuSePlay(MENU_SOUND_CONFIRM);
                return 1;
        }

        BattleMenuFlag = MenuSelect[0] + BTLMENU_STATE_OPEN_OFFSET;
        BtlEffectFlag  = BTLEFFECT_PAGE_OPEN;
        BtlEffectCt    = 0;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    } else if (GamePad.Down(PAD_CIRCLE) != 0) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        BattleMenuFlag = BTLMENU_STATE_EXIT;
    }

    return 1;
}

bool BattleMenuQuitConfirmActive() {
    return quit_confirm_active || saveload_modal_active;
}

int *BattleMenuQuitConfirmChoicePtr() {
    return saveload_modal_active ? &saveload_modal_choice : &quit_confirm_choice;
}

bool TakeBattleMenuLoadRequest() {
    if (!ingame_load_pending) {
        return false;
    }
    ingame_load_pending = false;
    return true;
}