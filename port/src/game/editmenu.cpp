#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "editmenu.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "battlemenu.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "edit.hpp"
#include "editpartsinfo.hpp"
#include "frame.hpp"
#include "gamepad.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_manual.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

float            MenuIconPos[6][2];
EDIT_MENU_CURSOR EdCur;
float            AnalyzeFill[3];

s8 EditMenuIconID[6] = {13, 10, 11, 5, 4, 6};

CTexture  *Analyze;
CTexture  *AnaBar;
int        EdMenuTextureReadEndFlag;
int        EdMenuTextureBlock;
u_long128 *EdMenuWorkBuf;
int        EdMenuExTextureBlock;
int        EdMenuExTextureBlock1;
int        EdMenuExTextureBlock2;
int        EditSwitch;
s16        EdMenuRGB;
int        EdEffectCt;
s16        EdMenuHelpWinAlpha;
s16        AnalyzeSelect;
int        EdMenuMesNo2;
int        EdMenuMesMake2;
s16        ButtonAdd;
s16        AnalyzeBackBlockCnt;
s16        EdMenuEffectFlag;
float      EdMenuEffectCt;
float      WindowPos[2];
float      EditMenuWinW;
float      EditMenuWinH;
s16        MakeWin2Flag;

#include "editloop.hpp"
#include "mainselect.hpp"
#include "menuitemstep.hpp"

/**
 * Draws the edit menu's opening and enters its page textures once they have been read.
 *
 * @mangled EditMenuStart__Fv
 * @address 0x2112C0
 * @size 0x4BC
 */
static int EditMenuStart();

/**
 * Draws the edit menu's icon selection.
 *
 * @mangled EditMenuSelectDraw__Fv
 * @address 0x211780
 * @size 0x22C
 */
static void EditMenuSelectDraw();

/**
 * Handles pad input in the edit menu's icon selection and returns the result.
 *
 * @mangled EditMenuSelect__Fv
 * @address 0x2119B0
 * @size 0x38C
 */
static int EditMenuSelect();

/**
 * Draws the edit menu while it closes.
 *
 * @mangled EditMenuToExitDraw__Fv
 * @address 0x211D40
 * @size 0x20
 */
static void EditMenuToExitDraw();

/**
 * Runs the edit menu's closing and reports when it is finished.
 *
 * @mangled EditMenuToExit__Fv
 * @address 0x211D60
 * @size 0x144
 */
static int EditMenuToExit();

/**
 * Draws the Atla selection page.
 *
 * @mangled AtoraSelectDraw__Fv
 * @address 0x211EB0
 * @size 0x90
 */
static void AtoraSelectDraw();

/**
 * Runs the Atla selection page and returns the result.
 *
 * @mangled AtoraSelect__Fv
 * @address 0x211F40
 * @size 0x188
 */
static int AtoraSelect();

/**
 * Draws the Atla move state, which has nothing to draw.
 *
 * @mangled AtoraMoveDraw__Fv
 * @address 0x2120D0
 * @size 0x8
 */
static void AtoraMoveDraw();

/**
 * Leaves the edit menu for the Atla move and returns the result.
 *
 * @mangled AtoraMove__Fv
 * @address 0x2120E0
 * @size 0x30
 */
static int AtoraMove();

/**
 * Draws the transition into the analysis page.
 *
 * @mangled ToAnalyzeEditDraw__Fv
 * @address 0x2129C0
 * @size 0x348
 */
static void ToAnalyzeEditDraw();

/**
 * Speeds up the transition into the analysis page while any input is held.
 *
 * @mangled ToAnalyzeEdit__Fv
 * @address 0x212D10
 * @size 0x44
 */
static void ToAnalyzeEdit();

/**
 * Draws the analysis page.
 *
 * @mangled AnalyzeEditDraw__Fv
 * @address 0x212D60
 * @size 0x1C0
 */
static void AnalyzeEditDraw();

/**
 * Leaves the analysis page when a 0x60 pad button is pressed, and returns 0.
 *
 * @mangled AnalyzeEdit__Fv
 * @address 0x212F20
 * @size 0x54
 */
static int AnalyzeEdit();

/**
 * Draws the transition out of the analysis page.
 *
 * @mangled FromAnalyzeEditDraw__Fv
 * @address 0x212F80
 * @size 0x50
 */
static void FromAnalyzeEditDraw();

/**
 * Advances the transition out of the analysis page.
 *
 * @mangled FromAnalyzeEdit__Fv
 * @address 0x212FD0
 * @size 0xA8
 */
static void FromAnalyzeEdit();

/**
 * Draws the edit menu's save page.
 *
 * @mangled EditSaveDraw__Fv
 * @address 0x213080
 * @size 0x74
 */
static void EditSaveDraw();

/**
 * Handles pad input on the edit menu's save page.
 *
 * @mangled EditSaveKey__Fv
 * @address 0x213100
 * @size 0xDC
 */
static void EditSaveKey();

/**
 * Draws the edit menu's option page.
 *
 * @mangled OptionDraw__Fv
 * @address 0x2131E0
 * @size 0x6C
 */
static void OptionDraw();

/**
 * Handles pad input on the edit menu's option page.
 *
 * @mangled EdOptionSelect__Fv
 * @address 0x213250
 * @size 0xF4
 */
static void EdOptionSelect();

/**
 * Handles pad input on the edit menu's manual page.
 *
 * @mangled EdMenuManualKey__Fv
 * @address 0x213350
 * @size 0xF8
 */
static int EdMenuManualKey();

/**
 * Draws the edit menu's manual page.
 *
 * @mangled EdMenuManualDraw__Fv
 * @address 0x213450
 * @size 0x6C
 */
static void EdMenuManualDraw();

int GetNumHowManyItemsHave(int item) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item);

    if (info == NULL) {
        return 0;
    }

    CDngStatusData *dungeon_status = SaveData->GetDngStatus();

    if (dungeon_status == NULL) {
        return 0;
    }

    int count = 0;

    switch (info->kind) {
        case ITEMKIND_WEAPON: {
            WEAPON_DATA *weapon = GetWeaponData(item);

            if (weapon != NULL) {
                int          i;
                s8           owner = weapon->owner;
                WEAPON_HAVE *weapons = dungeon_status->chara_weapons[owner];

                for (i = 0; i < 10; i++) {
                    if (item == weapons[i].item_no) {
                        count++;
                    }
                }
            }

            break;
        }
    }

    return count;
}

/**
 * Returns the number of edit menu icons, one fewer until the manual is available.
 *
 * @mangled GetEditMenuMax__Fv
 * @address 0x2102E0
 * @size 0x3C
 */
static int GetEditMenuMax() {
    int icon_max = 6;

    if (GetGameFlagForManualMenu() == 0) {
        icon_max--;
    }

    return icon_max;
}

/**
 * Draws the selected edit menu icon at its resting position.
 */
static void DrawMenuIcon(int slot) {
    MENU_ICON_INFO *info = GetMenuIconInfo(EditMenuIconID[slot]);

    DrawMainMenuIcon((int) (MenuIconPos[slot][0] - 6.0f), (int) (MenuIconPos[slot][1] - 4.0f), info->id, 1, 0x80, 0x80);
}

/**
 * Writes the screen position of one edit menu icon.
 *
 * @mangled GetEditMenuIconPos__FiPi
 * @address 0x2103F0
 * @size 0xFC
 */
static void GetEditMenuIconPos(int slot, int *position) {
    u8  six[6] = {0x50, 0x60, 0x70, 0x80, 0x80, 0x70};
    u8  five[5] = {0x50, 0x60, 0x70, 0x80, 0x70};
    u8  x[6] = {0x50, 0x60, 0x70, 0x80, 0x80, 0x70};
    int icon_max = GetEditMenuMax();

    if (icon_max == 5) {
        for (int i = 0; i < icon_max; i++) {
            x[i] = five[i];
        }
    }

    if (icon_max == 6) {
        for (int i = 0; i < icon_max; i++) {
            x[i] = six[i];
        }
    }

    position[0] = x[slot];
    position[1] = slot * 40 + 0x4A;
}

/**
 * Draws each edit menu icon at its current position.
 */
static void DrawMoveMenuIcon() {
    int y_offset;
    int brightness;
    int icon_max;
    int i;
    int icon;
    int x_offset;

    icon_max = GetEditMenuMax();

    for (i = 0; i < icon_max; i++) {
        if (EditSwitch == EDIT_MENU_SELECT) {
            brightness = 0x80;
        }

        if (EditSwitch >= EDIT_MENU_ATORA_IN && EditSwitch < EDIT_MENU_ATORA_OUT) {
            brightness = 0x80 - EdEffectCt * 8;
        }

        if (EditSwitch >= EDIT_MENU_ATORA_OUT && EditSwitch < EDIT_MENU_OUT_END) {
            brightness = EdEffectCt * 8;
        }

        if (brightness < 0) {
            brightness = 0;
        }

        if (brightness > 0x80) {
            brightness = 0x80;
        }

        icon = EditMenuIconID[i];
        GetMenuIconInfo(icon);

        int selected;

        y_offset = 0;
        x_offset = 0;
        selected = 0;

        if (i == EdCur.selection) {
            x_offset = 6;
            y_offset = 2;
            brightness = 0x80;
            selected = 1;
        }

        DrawMainMenuIcon((int) (MenuIconPos[i][0] - x_offset), (int) (MenuIconPos[i][1] - y_offset), icon, selected, 0x80, brightness);
    }
}

/**
 * Moves the edit menu icons one step away from their resting positions and reports when they have arrived.
 *
 * @mangled CalMoveFromMenuIcon__Fv
 * @address 0x2106B0
 * @size 0x240
 */
static int CalMoveFromMenuIcon() {
    int done = 0;
    int arrived_count = 0;
    s16 target[13] = {0x50, 0x34, 0x50, 0x34, 0x50, 0x20, 0x5E, 0x1E, 0x50, 0x34, 0x50, 0x34, -262};
    int icon_max = GetEditMenuMax();

    int x;
    int y;

    for (int i = 0; i < icon_max; i++) {
        int arrived_axes = 0;

        if (i != EditSwitch - EDIT_MENU_ATORA_IN) {
            x = target[12];
            y = (int) MenuIconPos[i][1];
        } else {
            x = target[i * 2];
            y = target[i * 2 + 1];
        }

        float delta_x = x - MenuIconPos[i][0];
        MenuIconPos[i][0] += delta_x / 4.0f;

        if (abs((int) delta_x) < 4.0f) {
            MenuIconPos[i][0] = x;
            arrived_axes++;
        }

        float delta_y = y - MenuIconPos[i][1];
        MenuIconPos[i][1] += delta_y / 4.0f;

        if (abs((int) delta_y) < 4.0f) {
            MenuIconPos[i][1] = y;
            arrived_axes++;
        }

        if (arrived_axes >= 2) {
            arrived_count++;
        }
    }

    if (arrived_count >= icon_max) {
        done = 1;
    }

    return done;
}

/**
 * Moves the edit menu icons back to their resting positions.
 */
static int CalMoveToMenuIcon() {
    int done = 0;
    int arrived_count = 0;
    int icon_max = GetEditMenuMax();

    for (int i = 0; i < icon_max; i++) {
        int arrived_axes = 0;
        int target[2];

        GetEditMenuIconPos(i, target);

        float delta_x = (float) target[0] - MenuIconPos[i][0];
        MenuIconPos[i][0] += delta_x / 4.0f;

        if (abs((int) delta_x) < 2.0f) {
            MenuIconPos[i][0] = target[0];
            arrived_axes++;
        }

        float delta_y = (float) target[1] - MenuIconPos[i][1];
        MenuIconPos[i][1] += delta_y / 4.0f;

        if (abs((int) delta_y) < 2.0f) {
            MenuIconPos[i][1] = target[1];
            arrived_axes++;
        }

        if (arrived_axes >= 2) {
            arrived_count++;
        }
    }

    if (arrived_count >= icon_max) {
        done = 1;
    }

    return done;
}

void EditMenuInit(int *texture_blocks, int start_at_atora) {
    StartReadBG();
    u_long128 *buffer = (u_long128 *) (EdMenuBuffer.base + EdMenuBuffer.used * 16);
    EdMenuWorkBuf = buffer + (LoadFileBGMenuData("emenu.pak", buffer) >> 4) + 1;
    EdMenuWorkBuf = MenuCalcBufAlignment(EdMenuWorkBuf);
    EdMenuTextureReadEndFlag = 0;
    EdMenuTextureBlock = texture_blocks[0];
    EdMenuExTextureBlock = texture_blocks[1];
    EdMenuExTextureBlock1 = texture_blocks[2];
    EdMenuExTextureBlock2 = texture_blocks[3];
    StayTex = TexManager.GetTexture("stayframe", -1);
    GamePad.MenuModeOn(0x78);
    GamePad.SetAutoRepeat(PAD_DPAD, 0x1E, 9);
    CommonMenuMes2.mes_made = -1;
    CommonMenuMes3.mes_made = -1;
    MakeWin2Flag = 1;
    EdMenuMesMake2 = 1;
    EdMenuMesNo2 = 0x15;
    EdMenuHelpWinAlpha = 0;
    EdEffectCt = 0;
    CursorVibeCnt = 0;
    EdMenuEffectFlag = ED_MENU_ICON_STILL;
    EdMenuEffectCt = 0;
    ItemVolumeStep.CheckItemVolume();

    if (start_at_atora != 0) {
        EdMenuTextureReadEndFlag = 1;
        EdMenuRGB = 0x40;
        MenuIconPos[0][0] = 80.0f;
        MenuIconPos[0][1] = 52.0f;
        int blocks[2] = {0, 0};
        blocks[0] = EdMenuTextureBlock;
        blocks[1] = EdMenuExTextureBlock;
        InitMenuAtora1(ATORA_OPEN_PLACE, NowEditMap, blocks, EdMenuWorkBuf);
        InitMenuAtoraSelect(NowEditMap);
        EditSwitch = EDIT_MENU_ATORA;
        CommonMenuMes2.edge_alpha = 0x80;
        CommonMenuMes3.edge_alpha = 0x80;
        return;
    }

    CommonMenuMes2.edge_alpha = 0;
    CommonMenuMes3.edge_alpha = 0;
    EdCur.selection = 0;
    EditSwitch = EDIT_MENU_START;
    EdMenuRGB = 0x80;
    WindowPos[1] = 325.0f;
    GetMainMenuRightHelpWinLangOffset(WindowPos[0], WindowPos[1], EditMenuWinW, EditMenuWinH);
    EdCur.x = 48.0f;
    EdCur.y = 76.0f;
    s16 start_x[6] = {-0xC0, -0xB0, -0xA0, -0x90, -0x80, -0x80};

    for (int i = 0; i < GetEditMenuMax(); i++) {
        MenuIconPos[i][0] = start_x[i];
        MenuIconPos[i][1] = i * 40 + 0x4A;
    }
}

/**
 * Releases the edit menu's textures and returns the pad to normal mode.
 *
 * @mangled EditMenuExit__Fv
 * @address 0x210DA0
 * @size 0x8C
 */
static void EditMenuExit() {
    int blocks[5] = {0, 0, 0, 0, -1};

    blocks[0] = EdMenuTextureBlock;
    blocks[1] = EdMenuExTextureBlock;
    blocks[2] = EdMenuExTextureBlock1;
    blocks[3] = EdMenuExTextureBlock2;
    MenuTextureDelete(blocks);
    TexManager.CleanUpTextureList();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

/**
 * Returns the help-window draw flag it is given, or 0 while the edit menu is in
 * state 3, 9 or 0x10 and the Atra event flag is set.
 *
 * @mangled GetDrawHelpWindow__Fi
 * @address 0x210E30
 * @size 0x64
 */
static int GetDrawHelpWindow(int draw) {
    if ((EditSwitch == EDIT_MENU_ATORA_IN || EditSwitch == EDIT_MENU_ATORA || EditSwitch == EDIT_MENU_ATORA_OUT) && GetMenuAtraEventFlag()) {
        draw = false;
    }

    return draw;
}

int EditMenuLoop() {
    int result;

    ReadBG();
    result = 0;

    switch (EditSwitch) {
        case EDIT_MENU_SELECT:
            result = EditMenuSelect();
            break;
        case EDIT_MENU_ANALYZE_IN:
            ToAnalyzeEdit();
            break;
        case EDIT_MENU_ATORA_MOVE:
            result = AtoraMove();
            break;
        case EDIT_MENU_ANALYZE:
            result = AnalyzeEdit();
            break;
        case EDIT_MENU_ATORA:
        case EDIT_MENU_ATORA_IN:
        case EDIT_MENU_ATORA_OUT:
            result = AtoraSelect();
            break;
        case EDIT_MENU_ANALYZE_OUT:
            FromAnalyzeEdit();
            break;
        case EDIT_MENU_SAVE:
        case EDIT_MENU_SAVE_IN:
        case EDIT_MENU_SAVE_OUT:
            EditSaveKey();
            break;
        case EDIT_MENU_OPTION:
        case EDIT_MENU_OPTION_IN:
        case EDIT_MENU_OPTION_OUT:
            EdOptionSelect();
            break;
        case EDIT_MENU_MANUAL:
        case EDIT_MENU_MANUAL_IN:
        case EDIT_MENU_MANUAL_OUT:
            EdMenuManualKey();
            break;
        case EDIT_MENU_EXIT:
            result = EditMenuToExit();
            break;
    }

    EditMenuDraw();
    return result;
}

void EditMenuDraw() {
    int text_x;
    int text_y;

    MenuTextureReload(EdMenuTextureBlock);
    setbilinear(0);
    FrameImageDraw(EdMenuRGB, 0x80);

    switch (EditSwitch) {
        case EDIT_MENU_START:
            EditMenuStart();
            break;
        case EDIT_MENU_SELECT:
            EditMenuSelectDraw();
            break;
        case EDIT_MENU_ANALYZE_IN:
            ToAnalyzeEditDraw();
            break;
        case EDIT_MENU_ATORA_MOVE:
            AtoraMoveDraw();
            break;
        case EDIT_MENU_ANALYZE:
            AnalyzeEditDraw();
            break;
        case EDIT_MENU_ANALYZE_OUT:
            FromAnalyzeEditDraw();
            break;
        case EDIT_MENU_ATORA:
        case EDIT_MENU_ATORA_IN:
        case EDIT_MENU_ATORA_OUT:
            AtoraSelectDraw();
            break;
        case EDIT_MENU_SAVE:
        case EDIT_MENU_SAVE_IN:
        case EDIT_MENU_SAVE_OUT:
            EditSaveDraw();
            break;
        case EDIT_MENU_OPTION:
        case EDIT_MENU_OPTION_IN:
        case EDIT_MENU_OPTION_OUT:
            OptionDraw();
            break;
        case EDIT_MENU_MANUAL:
        case EDIT_MENU_MANUAL_IN:
        case EDIT_MENU_MANUAL_OUT:
            EdMenuManualDraw();
            break;
        case EDIT_MENU_EXIT:
            EditMenuToExitDraw();
            break;
    }

    CursorVibeCnt++;

    if (CursorVibeCnt >= 1080000000) {
        CursorVibeCnt = 0;
    }

    MenuTextureReload(CommonMenuMes2.tex_block);
    GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
    GetMainMenuRightHelpWinLangOffset(WindowPos[0], WindowPos[1], EditMenuWinW, EditMenuWinH);
    CommonMenuMes2.text_x = (int) (WindowPos[0] + text_x);
    CommonMenuMes2.text_y = (int) (WindowPos[1] + text_y);

    if (EdMenuTextureReadEndFlag != 0) {
        switch (EditSwitch) {
            case EDIT_MENU_EXIT:
                EdMenuHelpWinAlpha -= 8;

                if (EdMenuHelpWinAlpha < 0) {
                    EdMenuHelpWinAlpha = 0;
                }

                CommonMenuMes2.edge_alpha = EdMenuHelpWinAlpha;
                break;
            default:
                EdMenuHelpWinAlpha += 8;

                if (EdMenuHelpWinAlpha > 0x80) {
                    EdMenuHelpWinAlpha = 0x80;
                }

                CommonMenuMes2.edge_alpha += 8;

                if (CommonMenuMes2.edge_alpha > 0x80) {
                    CommonMenuMes2.edge_alpha = 0x80;
                }

                break;
        }

        if (GetDrawHelpWindow(MakeWin2Flag) != 0) {
            MenuHelpWinDraw((int) WindowPos[0], (int) WindowPos[1], EditMenuWinW, EditMenuWinH, EdMenuHelpWinAlpha);

            if (EdMenuMesMake2 != 0) {
                EdMenuMesMake2 = CommonMenuMes2.MakeMesWin(EdMenuMesNo2);
            }

            CommonMenuMes2.Step();
            CommonMenuMes2.DrawMesWin();
        }
    }

    setbilinear(1);
}

static int EditMenuStart() {
    u8  six[6] = {0x50, 0x60, 0x70, 0x80, 0x80, 0x70};
    u8  five[5] = {0x50, 0x60, 0x70, 0x80, 0x70};
    int icon_max = GetEditMenuMax();
    u8  x[6] = {0x50, 0x60, 0x70, 0x80, 0x80, 0x70};
    int brightness;
    int x_offset;
    int y_offset;
    int i;
    int icon;

    if (icon_max == 5) {
        for (int i = 0; i < icon_max; i++) {
            x[i] = five[i];
        }
    }

    if (icon_max == 6) {
        for (int i = 0; i < icon_max; i++) {
            x[i] = six[i];
        }
    }

    for (int i = 0; i < icon_max; i++) {
        MenuIconPos[i][0] += ((float) x[i] - MenuIconPos[i][0]) / 4.0f;
    }

    if (EdMenuRGB > 0x40) {
        EdMenuRGB -= 4;
    }

    brightness = EdEffectCt * 7;

    if (brightness > 0x80) {
        brightness = 0x80;
    }

    for (i = 0; i < GetEditMenuMax(); i++) {
        y_offset = 0;
        x_offset = 0;
        icon = EditMenuIconID[i];
        GetMenuIconInfo(icon);
        int selected = 0;

        if (i == EdCur.selection) {
            x_offset = 8;
            y_offset = 2;
            brightness = 0x80;
            selected = 1;
        }

        DrawMainMenuIcon((int) (MenuIconPos[i][0] - x_offset), (int) (MenuIconPos[i][1] - y_offset), icon, selected, 0x80, brightness);
    }

    if (ReadBGSync() == 0 && EdMenuTextureReadEndFlag == 0) {
        LOADTEXTURE_INFO2 textures[3] = {{"#frame_menuemenu#640#" SCREEN_HEIGHT_STR "#4"}};
        textures[1].block_no = textures[0].block_no = EdMenuTextureBlock;
        BG_READ_INFO *file = GetReadBGFile(0);
        textures[1].name = (char *) GetPackFile((u_int *) file->buffer, "editmenu.img", NULL);
        int blocks[5] = {0, 0, 0, 0, -1};
        blocks[0] = EdMenuTextureBlock;
        blocks[1] = EdMenuExTextureBlock;
        blocks[2] = EdMenuExTextureBlock1;
        blocks[3] = EdMenuExTextureBlock2;
        MenuTextureDelete(blocks);
        TexManager.CleanUpTextureList();
        TexManager.LoadTextureBlockEX(-1, textures);
        Analyze = TexManager.GetTexture("analyzeB", EdMenuTextureBlock);
        AnaBar = TexManager.GetTexture("anabar", EdMenuTextureBlock);
        PerBoardTex = TexManager.GetTexture("perbrd", EdMenuTextureBlock);
        VillageName = TexManager.GetTexture("vilname", -1);
        VillageBar = TexManager.GetTexture("viltag", -1);
        GetAtraMsgReadBuf = (s16 *) GetPackFile((u_int *) file->buffer, "atrames.bin", NULL);
        InitMenuMesSet(MENU_MES_SET_ALLMENU, (s16 *) GetPackFile((u_int *) file->buffer, "allmenu.mes", NULL));
        EdMenuTextureReadEndFlag = 1;
    }

    EdEffectCt++;

    if (EdEffectCt >= 17 && EdMenuTextureReadEndFlag != 0) {
        EditSwitch = EDIT_MENU_SELECT;
        EdCur.selection = 0;
        EdEffectCt = 0;
    }

    return 0;
}

/**
 * Draws the edit menu page and its selected icon.
 */
static void EditMenuSelectDraw() {
    DrawMoveMenuIcon();

    int   icon_max = GetEditMenuMax();
    s8    selection = EdCur.selection;
    float slot = selection;
    float x = 48.0f + 16.0f * slot;

    if (icon_max == 6) {
        if (selection == icon_max - 2) {
            x -= 16.0f;
        }

        if (selection == icon_max - 1) {
            x -= 48.0f;
        }
    }

    if (icon_max == 5 && selection == icon_max - 1) {
        x -= 32.0f;
    }

    float y = 76.0f + 40.0f * selection;

    EdCur.x += (x - EdCur.x) / 4.0f;
    EdCur.y += (y - EdCur.y) / 3.0f;

    MENU_ICON_INFO *info = GetMenuIconInfo(EditMenuIconID[EdCur.selection]);
    int             width = info->label_width + 0x4A;
    DrawMenuWaku(x + 18.0f, y - 15.0f, width, 0x28, 0, StayTex, 0x80);
    DrawMenuObjectVibe((int) EdCur.x, (int) EdCur.y, 1, 0x40);

    CommonMenuMes2.stay_frame = false;

    if (CommonMenuMes2.mes_made != info->help_mes_no) {
        CommonMenuMes2.MakeMesWin(info->help_mes_no);
    }
}

static int EditMenuSelect() {
    int icon_max = GetEditMenuMax();
    int previous = EdCur.selection;

    if (GamePad.Down(PAD_UP | PAD_LEFT) != 0) {
        EdCur.selection--;

        if (EdCur.selection < 0) {
            EdCur.selection = icon_max - 1;
        }
    }

    if (GamePad.Down(PAD_RIGHT | PAD_DOWN) != 0) {
        EdCur.selection++;

        if (icon_max - 1 <= EdCur.selection - 1) {
            EdCur.selection = 0;
        }
    }

    if (previous != EdCur.selection) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if (GamePad.Down(PAD_CIRCLE) != 0) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        EditSwitch = EDIT_MENU_EXIT;
        EdEffectCt = 0;
    } else if (GamePad.Down(PAD_CROSS) != 0) {
        int next[6] = {EDIT_MENU_ATORA_IN, EDIT_MENU_ATORA_MOVE_IN, EDIT_MENU_ANALYZE_IN, EDIT_MENU_SAVE_IN, EDIT_MENU_OPTION_IN, EDIT_MENU_MANUAL_IN};
        EditSwitch = next[EdCur.selection];
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        EdEffectCt = 0;
        CommonMenuMes2.mes_made = -1;
        u_long128 *work = EdMenuWorkBuf;

        switch (EditSwitch) {
            case EDIT_MENU_ATORA_IN: {
                int blocks[2] = {0, 0};
                blocks[0] = EdMenuTextureBlock;
                blocks[1] = EdMenuExTextureBlock;
                int area = GetNowMapTransAtraMap(MapNo);

                if (area < 0 || area > 4) {
                    area = 0;
                }

                printf("now atra load area = %d\n", area);
                InitMenuAtora1(ATORA_OPEN_PLACE, NowEditMap, blocks, work);
                InitMenuAtoraSelect(NowEditMap);
                EdMenuEffectFlag = ED_MENU_ICON_MOVE_FROM;
                EdMenuEffectCt = 0;
                EdMenuMesMake2 = 1;
                break;
            }
            case EDIT_MENU_ATORA_MOVE_IN:
                EditSwitch = EDIT_MENU_ATORA_MOVE;
                break;
            case EDIT_MENU_ANALYZE_IN:
                CommonMenuAtoraInfo = &EditPartsInfo;

                if (CommonMenuAtoraInfo == NULL) {
                    CommonMenuAtoraInfo->Load(0, SaveData, 1);
                }

                EdEffectCt = 1;

                for (int i = 0; i < 3; i++) {
                    AnalyzeFill[i] = 0.0f;
                }

                ButtonAdd = 0;
                AnalyzeBackBlockCnt = 0;
                AnalyzeSelect = 0;
                MakeWin2Flag = 0;
                break;
            case EDIT_MENU_SAVE_IN:
                MakeWin2Flag = 0;
                EdMenuEffectFlag = ED_MENU_ICON_MOVE_FROM;
                EdMenuMesMake2 = 1;
                InitMenuSave(SAVE_MENU_MODE_SAVE, EdMenuExTextureBlock, work);
                break;
            case EDIT_MENU_OPTION_IN:
                EdMenuMesMake2 = 1;
                EdMenuEffectFlag = ED_MENU_ICON_MOVE_FROM;
                InitMenuOption(OPTION_OPEN_GAME_MENU, EdMenuExTextureBlock, work);
                break;
            case EDIT_MENU_MANUAL_IN: {
                MakeWin2Flag = 0;
                EdMenuEffectFlag = ED_MENU_ICON_MOVE_FROM;
                int blocks[3] = {0, 0, 0};
                blocks[0] = EdMenuExTextureBlock1;
                blocks[1] = EdMenuExTextureBlock;
                blocks[2] = EdMenuExTextureBlock2;
                InitMenuManual(blocks, work);
                break;
            }
        }
    }

    return 0;
}

static void EditMenuToExitDraw() {
    DrawMoveMenuIcon();
}

/**
 * Moves the edit menu offscreen while closing its message windows.
 */
static int EditMenuToExit() {
    EdMenuRGB += 3;

    if (EdMenuRGB > 0x80) {
        EdMenuRGB = 0x80;
    }

    CommonMenuMes2.edge_alpha -= 9;

    if (CommonMenuMes2.edge_alpha < 0) {
        CommonMenuMes2.edge_alpha = 0;
    }

    CommonMenuMes3.edge_alpha -= 9;

    if (CommonMenuMes3.edge_alpha < 0) {
        CommonMenuMes3.edge_alpha = 0;
    }

    for (int i = 0; i < GetEditMenuMax(); i++) {
        MenuIconPos[i][0] += (-220.0f - MenuIconPos[i][0]) / 4.0f;
    }

    EdEffectCt++;

    if (EdEffectCt > 0x15) {
        EdEffectCt = 0;
        EditMenuStatus.mode = EDIT_MENU_MODE_CLOSED;
        EditMenuStatus.parts = -1;
        EditMenuExit();
        return 1;
    }

    return 0;
}

/**
 * Draws the Atora selection page and its moving icons.
 */
static void AtoraSelectDraw() {
    if (EdMenuTextureReadEndFlag == 0 && ReadBGSync() == 0) {
        EdMenuTextureReadEndFlag = 1;
    }

    DrawMenuAtoraSelect();

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
        case ED_MENU_ICON_MOVE_TO:
            DrawMoveMenuIcon();
            break;
        default:
            if (GetMenuAtraEventFlag() == 0) {
                DrawMoveMenuIcon();
            }

            break;
    }
}

/**
 * Processes input and transitions on the Atora selection page.
 */
static int AtoraSelect() {
    int result = 0;

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
            if (CalMoveFromMenuIcon()) {
                EditSwitch = EDIT_MENU_ATORA;
                EdMenuEffectFlag = ED_MENU_ICON_STILL;
                EdMenuEffectCt = 0.0f;
            }

            break;
        case ED_MENU_ICON_MOVE_TO:
            if (CalMoveToMenuIcon()) {
                EditSwitch = EDIT_MENU_SELECT;
                EdCur.selection = 0;
                EdCur.x = EdCur.selection * 16 + 0x30;
                EdCur.y = EdCur.selection * 40 + 0x4C;
            }

            break;
        default:
            switch (MenuAtoraSelectKey()) {
                case ATORA_SELECT_PLACE_EXIT:
                case ATORA_SELECT_PLACE:
                    result = 1;
                    break;
                case ATORA_SELECT_CLOSE:
                    EditSwitch = EDIT_MENU_ATORA_OUT;
                    EdMenuEffectFlag = ED_MENU_ICON_MOVE_TO;
                    EdMenuEffectCt = 0.0f;
                    break;
                case ATORA_SELECT_NONE:
                    break;
            }

            break;
    }

    if (EdMenuEffectFlag != ED_MENU_ICON_STILL) {
        EdMenuEffectCt += 1.0f;
        EdEffectCt++;
    } else {
        EdMenuEffectCt = 0.0f;
        EdEffectCt = 0;
    }

    return result;
}

static void AtoraMoveDraw() {}

/**
 * Closes the edit menu and selects the Atora page.
 */
static int AtoraMove() {
    EditMenuExit();
    EditMenuStatus.mode = EDIT_MENU_MODE_MOVE;
    return 1;
}

/**
 * Draws the scrolling background panels on the analysis page.
 */
static void AnalyzeBackDraw(int alpha, int brightness) {
    AnalyzeBackBlockCnt++;

    if (AnalyzeBackBlockCnt >= 0x10E) {
        AnalyzeBackBlockCnt = 0;
    }

    int x = AnalyzeBackBlockCnt / 5 - 0x36;
    int y = 0x3C;

    for (int column = 0; column < 0xD; column++, x += 0x36, y = 0x3C) {
        for (int row = 0; row < 5; row++, y += 0x36) {
            int bottom = 0x36;
            int top = 0;

            if (y < 0x50) {
                top = 0x50 - y;
            }

            if (y + 0x36 > 0x130) {
                bottom = 0x130 - y;
            }

            DrawMenu2DSprite(AnaBar, CRect_i_(x, y + top, 0x36, bottom - top), CRect_i_(0x36, top, 0x36, bottom - top), brightness, brightness, brightness, alpha);
        }
    }
}

/**
 * Returns the percentage of requested parts that have been collected.
 */
static float AnalyzeRequestPer() {
    CEditPartsInfo *info = CommonMenuAtoraInfo;
    float           total = (float) info->parts_max;

    if (total < 1.0f) {
        return 100.0f;
    }

    float done = 0.0f;

    for (int i = 0; i < 24; i++) {
        if (info->request[i] != 0) {
            done += 1.0f;
        }
    }

    float percent = 100.0f * done / total;

    if (percent >= 100.0f) {
        percent = 100.0f;
    }

    return percent;
}

/**
 * Draws the analysis page's bars and reports whether the fill has reached its target.
 *
 * @mangled AnalyzeBarDraw__Fv
 * @address 0x212320
 * @size 0x69C
 */
static int AnalyzeBarDraw() {
    float parts_total;
    float parts_complete;
    float elements_total;
    int   elements_done;
    int   parts_done;
    int   bars_started;
    int   x;
    int   i;
    int   top;
    int   digit_x;
    int   digits;
    int   number;
    float goal;
    float edge;
    float fill;
    float percent[3];
    float target[3];

    elements_total = parts_complete = parts_total = 0.0f;
    elements_done = 0;
    parts_done = 0;

    for (i = 0; i < 24; i++) {
        EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(i);

        if (info != NULL && info->stock > 0) {
            parts_total += 1.0f;

            if (info->obtained != 0) {
                parts_done++;

                if (CommonMenuAtoraInfo->CheckComplete(i) != 0) {
                    parts_complete += 1.0f;
                }
            }

            for (int j = 0; j < 6; j++) {
                if (info->elements[j].id < 0) {
                    break;
                }

                elements_total += 1.0f;

                if (info->elements[j].enabled != 0) {
                    elements_done++;
                }
            }
        }
    }

    s16 *elements = (s16 *) SaveData->GetElemData(NowEditMap);

    for (i = 0; i < 128; i++) {
        if (elements[i] > -1) {
            elements_done++;
        }
    }

    int total = (int) (parts_total + elements_total);

    if (total <= 0) {
        percent[0] = 0.0f;
    } else {
        percent[0] = 100.0f * (float) (parts_done + elements_done) / (float) total;
    }

    if ((int) parts_total <= 0) {
        percent[1] = 0.0f;
    } else {
        percent[1] = 100.0f * parts_complete / parts_total;
    }

    percent[2] = AnalyzeRequestPer();

    for (i = 0; i < 3; i++) {
        target[i] = 144.0f * percent[i] / 100.0f;

        if (!(target[i] <= 144.0f)) {
            target[i] = 144.0f;
        }
    }

    bars_started = 0;
    x = -0x3D;

    for (i = 0; i < 3; i++) {
        x += 0xB5;

        if (i != bars_started) {
            break;
        }

        AnalyzeFill[i] += 2.0f + 2.0f * ButtonAdd;
        goal = target[i];

        if (goal < AnalyzeFill[i]) {
            AnalyzeFill[i] = goal;
        }

        fill = AnalyzeFill[i];
        edge = 0.08f * fill;

        if (8.0f < edge) {
            edge = 8.0f;
        }

        top = (int) (306.0f - fill);
        CRect_i_ bar_destination;
        CRect_i_ bar_source;
        bar_source.x = 0;
        bar_source.y = i * 16 + 8;
        bar_source.width = 0x36;
        bar_source.height = 7;
        int height = (int) fill;
        bar_destination.x = x;
        bar_destination.y = top;
        bar_destination.width = 0x36;
        bar_destination.height = height;
        DrawMenu2DSprite(AnaBar, bar_destination, bar_source, 0x80);
        DrawMenu2DSprite(AnaBar, CRect_i_(x, top, 0x36, (int) edge), CRect_i_(0, i * 16, 0x36, 8), 0x80);
        edge = 0.1f * AnalyzeFill[i];

        if (!(AnalyzeFill[i] <= 0.0f)) {
            DrawMenu2DSprite(AnaBar, CRect_i_(x + 0x36, (int) (top + edge), 10, (int) ((float) (0x130 - top) - edge)), CRect_i_(0x56, 0x36, 10, 9), 0x80);
        }

        number = (int) (100.0f * AnalyzeFill[i] / 144.0f);

        if (percent[i] < (float) number) {
            number = (int) percent[i];
        }

        digit_x = x + 0x1B;

        for (digits = GetNumberKeta(number); digits != 0; digits--, digit_x -= 13) {
            int digit = number % 10;
            int u = digit * 13;
            int v = 0x6F;

            if (digit > 8) {
                u = 0;
                v = 0x5E;
            }

            DrawMenu2DSprite(AnaBar, CRect_i_(digit_x, top - 0x1E, 0x10, 0x18), CRect_i_(u, v, 13, 0x11), 0x8C);
            number /= 10;
        }

        DrawMenu2DSprite(AnaBar, CRect_i_(x + 0x28, top - 0x1E, 0xF, 0x13), CRect_i_(13, 0x5E, 13, 0x11), 0x80);

        if (0.4f * goal <= AnalyzeFill[i]) {
            bars_started++;
        }
    }

    if (AnalyzeFill[2] == target[2]) {
        return 1;
    }

    return 0;
}

static void ToAnalyzeEditDraw() {
    MenuTextureReload(EdMenuTextureBlock);

    for (int i = 0; i < 3; i++) {
        DrawMenu2DSprite(Analyze, CRect_i_(i * 0xB5 + 0x56, 0x14a, 0x7E, 0x50), CRect_i_(0x80, i * 0x50, 0x7E, 0x50), 0x80);
    }

    float left = (float) (0x280 - EdEffectCt * 16);
    int   width = (int) (80.0f + left);

    if (left < 0.0f) {
        left = 0.0f;
    }

    width = (int) ((float) width - left);

    if (width < 0) {
        width = 0;
    }

    FadeTexX((int) left, width, 0x280, 0, "frame_image", 0);
    DrawMoveMenuIcon();
    int alpha = EdEffectCt * 4;

    if (alpha > 0x80) {
        alpha = 0x80;
    }

    int brightness = EdEffectCt + 16;

    if (brightness > 0x40) {
        brightness = 0x40;
    }

    AnalyzeBackDraw(alpha, brightness);
    int   done = 0;
    float bar = (float) (EdEffectCt * 20);

    if (640.0f < bar) {
        bar = 640.0f;
    }

    if (560.0f < bar) {
        done = AnalyzeBarDraw();
    }

    if (640.0f < bar) {
        bar = 640.0f;
    }

    DrawMenu2DSprite(AnaBar, CRect_i_(0, 0x130, (int) bar, 0x18), CRect_i_(0, 0x36, 0x20, 0x18), 0x80);

    for (int x = 0x75; (float) x < bar; x += 0xB5) {
        int segment_width = (int) (bar - (float) x);

        if (segment_width > 0x3C) {
            segment_width = 0x3C;
        }

        DrawMenu2DSprite(AnaBar, CRect_i_(x, 0x130, segment_width, 0xF), CRect_i_(0x20, 0x36, 0x36, 0xF), 0x80);
    }

    CalMoveFromMenuIcon();
    EdEffectCt++;

    if (done == 1) {
        EdEffectCt = 0;
        AnalyzeSelect = 0;
        EditSwitch = EDIT_MENU_ANALYZE;
    }
}

static void ToAnalyzeEdit() {
    if (GamePad.AllOn()) {
        ButtonAdd = 2;
        EdEffectCt += 4;
    }
}

/**
 * Draws the panels and progress bars on the analysis page.
 */
static void AnalyzeEditDraw() {
    DrawMenuIcon(2);
    AnalyzeBackDraw(0x80, 0x40);

    for (int i = 0; i < 3; i++) {
        DrawMenu2DSprite(Analyze, CRect_i_(i * 0xB5 + 0x56, 0x14a, 0x7E, 0x50), CRect_i_(0x80, i * 0x50, 0x7E, 0x50), 0x80);
    }

    AnalyzeBarDraw();
    DrawMenu2DSprite(AnaBar, CRect_i_(0, 0x130, 0x280, 0x18), CRect_i_(0, 0x36, 0x20, 0x18), 0x80);

    int x = 0x75;
    int i = 0;

    while (x < 0x280) {
        int width = 0x280 - x;

        if (width > 0x3C) {
            width = 0x3C;
        }

        if (AnalyzeFill[i] > 0.0f) {
            DrawMenu2DSprite(AnaBar, CRect_i_(x, 0x130, width, 0xF), CRect_i_(0x20, 0x36, 0x36, 0xF), 0x80);
        }

        i++;
        x += 0xB5;
    }
}

static int AnalyzeEdit() {
    if (GamePad.Down(PAD_CIRCLE | PAD_CROSS)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        EditSwitch = EDIT_MENU_ANALYZE_OUT;
        AnalyzeSelect = 2;
    }

    return 0;
}

static void FromAnalyzeEditDraw() {
    int alpha;

    DrawMoveMenuIcon();
    alpha = 0x80 - EdEffectCt * 10;

    if (alpha < 0) {
        alpha = 0;
    }

    AnalyzeBackDraw(alpha, 0x40);
}

/**
 * Returns the cursor to the edit menu after analysis.
 */
static void FromAnalyzeEdit() {
    CalMoveToMenuIcon();
    EdEffectCt++;

    if (EdEffectCt >= 0x10) {
        EdEffectCt = 0;
        MakeWin2Flag = 1;
        EdCur.selection = 2;
        EdCur.x = EdCur.selection * 16 + 0x30;
        EdCur.y = EdCur.selection * 40 + 0x4C;
        EditSwitch = EDIT_MENU_SELECT;
    }
}

/**
 * Draws the save page and its selected icon.
 */
static void EditSaveDraw() {
    DrawMenuSave("frame_image");
    setbilinear(0);

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
        case ED_MENU_ICON_MOVE_TO:
            DrawMoveMenuIcon();
            break;
        default:
            DrawMenuIcon(3);
            break;
    }
}

static void EditSaveKey() {
    int arrived;

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
            arrived = CalMoveFromMenuIcon();

            if (arrived) {
                EdMenuEffectFlag = ED_MENU_ICON_STILL;
                EdMenuEffectCt = 0.0f;
            }

            break;
        case ED_MENU_ICON_MOVE_TO:
            arrived = CalMoveToMenuIcon();
            break;
    }

    if (EdMenuEffectFlag != ED_MENU_ICON_STILL) {
        EdMenuEffectCt += 1.0f;
    } else {
        EdMenuEffectCt = 0.0f;
    }

    MenuSaveKey();

    if (SaveMenuEffectFadeOut()) {
        EdMenuEffectFlag = ED_MENU_ICON_MOVE_TO;

        if (arrived) {
            EdEffectCt = 0;
            EditSwitch = EDIT_MENU_SELECT;
            MakeWin2Flag = 1;
        }
    }
}

/**
 * Draws the options page and its selected icon.
 */
static void OptionDraw() {
    DrawMenuOption();
    setbilinear(0);

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
        case ED_MENU_ICON_MOVE_TO:
            DrawMoveMenuIcon();
            break;
        default:
            DrawMenuIcon(4);
            break;
    }
}

static void EdOptionSelect() {
    int arrived = false;
    int key;

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
            arrived = CalMoveFromMenuIcon();

            if (arrived) {
                EdMenuEffectFlag = ED_MENU_ICON_STILL;
                EdMenuEffectCt = 0.0f;
            }

            break;
        case ED_MENU_ICON_MOVE_TO:
            arrived = CalMoveToMenuIcon();
            break;
    }

    if (EdMenuEffectFlag != ED_MENU_ICON_STILL) {
        EdMenuEffectCt += 1.0f;
    } else {
        EdMenuEffectCt = 0.0f;
    }

    key = MenuOptionKey();

    if (OptionMenuFadeOutStart()) {
        EdMenuEffectFlag = ED_MENU_ICON_MOVE_TO;

        if (arrived && key) {
            EdEffectCt = 0;
            EditSwitch = EDIT_MENU_SELECT;
            MakeWin2Flag = 1;
        }
    }
}

static int EdMenuManualKey() {
    int arrived = false;
    int key;

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
            arrived = CalMoveFromMenuIcon();

            if (arrived) {
                EdMenuEffectFlag = ED_MENU_ICON_STILL;
                EdMenuEffectCt = 0.0f;
            }

            break;
        case ED_MENU_ICON_MOVE_TO:
            arrived = CalMoveToMenuIcon();
            break;
    }

    if (EdMenuEffectFlag != ED_MENU_ICON_STILL) {
        EdMenuEffectCt += 1.0f;
    } else {
        EdMenuEffectCt = 0.0f;
    }

    key = MenuManualKey();

    if (GetNowManualMenuMode() == MANUAL_MODE_CLOSING) {
        EdMenuEffectFlag = ED_MENU_ICON_MOVE_TO;

        if (arrived && key) {
            EdEffectCt = 0;
            EditSwitch = EDIT_MENU_SELECT;
            MakeWin2Flag = 1;
        }
    }

    return 0;
}

/**
 * Draws the manual page and its selected icon.
 */
static void EdMenuManualDraw() {
    MenuManualDraw();
    setbilinear(0);

    switch (EdMenuEffectFlag) {
        case ED_MENU_ICON_MOVE_FROM:
        case ED_MENU_ICON_MOVE_TO:
            DrawMoveMenuIcon();
            break;
        default:
            DrawMenuIcon(5);
            break;
    }
}
