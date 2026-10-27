#include "editmenu.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <print>
#include <span>

#include "battlemenu.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dun/gameloop.hpp"
#include "edit.hpp"
#include "editloop.hpp"
#include "editpartsinfo.hpp"
#include "frame.hpp"
#include "gamepad.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_manual.hpp"
#include "menu_misc.hpp"
#include "menu_save.hpp"
#include "menuitemstep.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

namespace {

/**
 * What the town edit menu is doing, as EditSwitch holds it.
 */
enum class EditMenuState : int {
    Start = 1,        /**< Starting. */
    Select = 2,       /**< Choosing a page. */
    Atora = 3,        /**< Atla page. */
    AtoraMove = 4,    /**< Moving placed parts. */
    Analyze = 5,      /**< Analysis page. */
    Save = 6,         /**< Save page. */
    Option = 7,       /**< Option page. */
    Manual = 8,       /**< Manual page. */
    AtoraIn = 9,      /**< Entering the Atla page. */
    AtoraMoveIn = 10, /**< Entering part moving. */
    AnalyzeIn = 11,   /**< Entering the analysis page. */
    SaveIn = 12,      /**< Entering the save page. */
    OptionIn = 13,    /**< Entering the option page. */
    ManualIn = 14,    /**< Entering the manual page. */
    Exit = 15,        /**< Leaving the menu. */
    AtoraOut = 16,    /**< Leaving the Atla page. */
    AnalyzeOut = 18,  /**< Leaving the analysis page. */
    SaveOut = 19,     /**< Leaving the save page; never set. */
    OptionOut = 20,   /**< Leaving the option page; never set. */
    ManualOut = 21,   /**< Leaving the manual page; never set. */
    OutEnd = 22,      /**< End of the leaving states. */
};

/**
 * How the edit menu's icons are moving, as EdMenuEffectFlag holds it.
 */
enum class EdMenuIconMove : s16 {
    Still = 0,    /**< Still. */
    MoveFrom = 1, /**< Moving away from the menu. */
    MoveTo = 2,   /**< Moving back to the menu. */
};

/**
 * The edit menu's eased cursor position and current icon selection.
 */
struct EDIT_MENU_CURSOR {
    float x;         /**< Cursor's current screen x, eased toward its target. */
    float y;         /**< Cursor's current screen y, eased toward its target. */
    s8    selection; /**< Index of the selected icon. */
};

constexpr int ICON_MAX = 6;

float            MenuIconPos[ICON_MAX][2];
EDIT_MENU_CURSOR EdCur;
float            AnalyzeFill[3];

std::array<s8, ICON_MAX> EditMenuIconID = {13, 10, 11, 5, 4, 6};

CTexture      *Analyze;
CTexture      *AnaBar;
int            EdMenuTextureReadEndFlag;
int            EdMenuTextureBlock;
u_long128     *EdMenuWorkBuf;
int            EdMenuExTextureBlock;
int            EdMenuExTextureBlock1;
int            EdMenuExTextureBlock2;
EditMenuState  EditSwitch;
s16            EdMenuRGB;
int            EdEffectCt;
s16            EdMenuHelpWinAlpha;
s16            AnalyzeSelect;
int            EdMenuMesNo2;
int            EdMenuMesMake2;
s16            ButtonAdd;
s16            AnalyzeBackBlockCnt;
EdMenuIconMove EdMenuEffectFlag;
float          EdMenuEffectCt;
float          WindowPos[2];
float          EditMenuWinW;
float          EditMenuWinH;
s16            MakeWin2Flag;

int  EditMenuStart();
void EditMenuSelectDraw();
int  EditMenuSelect();
void EditMenuToExitDraw();
int  EditMenuToExit();
void AtoraSelectDraw();
int  AtoraSelect();
void AtoraMoveDraw();
int  AtoraMove();
void ToAnalyzeEditDraw();
void ToAnalyzeEdit();
void AnalyzeEditDraw();
int  AnalyzeEdit();
void FromAnalyzeEditDraw();
void FromAnalyzeEdit();
void EditSaveDraw();
void EditSaveKey();
void OptionDraw();
void EdOptionSelect();
int  EdMenuManualKey();
void EdMenuManualDraw();

} // namespace

int GetNumHowManyItemsHave(int item) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item);

    if (info == nullptr) {
        return 0;
    }

    CDngStatusData *dungeon_status = SaveData->GetDngStatus();

    if (dungeon_status == nullptr) {
        return 0;
    }

    if (info->kind != ITEMKIND_WEAPON) {
        return 0;
    }

    WEAPON_DATA *weapon = GetWeaponData(item);

    if (weapon == nullptr) {
        return 0;
    }

    const std::span weapons = std::span(dungeon_status->chara_weapons[weapon->owner]).first(10);

    return static_cast<int>(std::ranges::count_if(weapons, [item](const WEAPON_HAVE &held) { return item == held.item_no; }));
}

namespace {

/**
 * Returns the number of edit menu icons, one fewer until the manual is available.
 */
int GetEditMenuMax() {
    int icon_max = ICON_MAX;

    if (GetGameFlagForManualMenu() == 0) {
        icon_max--;
    }

    return icon_max;
}

/**
 * Draws the selected edit menu icon at its resting position.
 */
void DrawMenuIcon(int slot) {
    MENU_ICON_INFO *info = GetMenuIconInfo(EditMenuIconID[slot]);

    DrawMainMenuIcon(static_cast<int>(MenuIconPos[slot][0] - 6.0f), static_cast<int>(MenuIconPos[slot][1] - 4.0f), info->id, 1, 0x80, 0x80);
}

/**
 * Returns each edit menu icon's resting screen x for the given icon count.
 */
std::array<u8, ICON_MAX> GetEditMenuIconRestX(int icon_max) {
    std::array<u8, ICON_MAX> rest_x = {0x50, 0x60, 0x70, 0x80, 0x80, 0x70};

    if (icon_max == 5) {
        constexpr std::array<u8, 5> five_rest_x = {0x50, 0x60, 0x70, 0x80, 0x70};
        std::ranges::copy(five_rest_x, rest_x.begin());
    }

    return rest_x;
}

/**
 * Returns the resting screen position of one edit menu icon as an x/y pair.
 */
std::array<int, 2> GetEditMenuIconPos(int slot) { return {GetEditMenuIconRestX(GetEditMenuMax())[slot], slot * 40 + 0x4A}; }

/**
 * Draws each edit menu icon at its current position.
 */
void DrawMoveMenuIcon() {
    // Only assigned for the states below; any other state keeps the previous icon's value.
    int brightness;
    int icon_max = GetEditMenuMax();

    for (int slot = 0; slot < icon_max; slot++) {
        if (EditSwitch == EditMenuState::Select) {
            brightness = 0x80;
        }

        if (EditSwitch >= EditMenuState::AtoraIn && EditSwitch < EditMenuState::AtoraOut) {
            brightness = 0x80 - EdEffectCt * 8;
        }

        if (EditSwitch >= EditMenuState::AtoraOut && EditSwitch < EditMenuState::OutEnd) {
            brightness = EdEffectCt * 8;
        }

        if (brightness < 0) {
            brightness = 0;
        }

        if (brightness > 0x80) {
            brightness = 0x80;
        }

        int icon = EditMenuIconID[slot];
        GetMenuIconInfo(icon);

        int y_offset = 0;
        int x_offset = 0;
        int selected = 0;

        if (slot == EdCur.selection) {
            x_offset = 6;
            y_offset = 2;
            brightness = 0x80;
            selected = 1;
        }

        DrawMainMenuIcon(static_cast<int>(MenuIconPos[slot][0] - x_offset), static_cast<int>(MenuIconPos[slot][1] - y_offset), icon, selected, 0x80, brightness);
    }
}

/**
 * Moves the edit menu icons one step away from their resting positions.
 *
 * Returns whether every icon has arrived.
 */
bool CalMoveFromMenuIcon() {
    constexpr std::array<std::array<s16, 2>, ICON_MAX> selected_target = {
        {{0x50, 0x34}, {0x50, 0x34}, {0x50, 0x20}, {0x5E, 0x1E}, {0x50, 0x34}, {0x50, 0x34}}
    };
    constexpr int offscreen_x = -262;

    int arrived_count = 0;
    int icon_max = GetEditMenuMax();

    for (int slot = 0; slot < icon_max; slot++) {
        int  arrived_axes = 0;
        bool selected = slot == static_cast<int>(EditSwitch) - static_cast<int>(EditMenuState::AtoraIn);
        int  x = selected ? selected_target[slot][0] : offscreen_x;
        int  y = selected ? selected_target[slot][1] : static_cast<int>(MenuIconPos[slot][1]);

        float delta_x = static_cast<float>(x) - MenuIconPos[slot][0];
        MenuIconPos[slot][0] += delta_x / 4.0f;

        if (static_cast<float>(std::abs(static_cast<int>(delta_x))) < 4.0f) {
            MenuIconPos[slot][0] = static_cast<float>(x);
            arrived_axes++;
        }

        float delta_y = static_cast<float>(y) - MenuIconPos[slot][1];
        MenuIconPos[slot][1] += delta_y / 4.0f;

        if (static_cast<float>(std::abs(static_cast<int>(delta_y))) < 4.0f) {
            MenuIconPos[slot][1] = static_cast<float>(y);
            arrived_axes++;
        }

        if (arrived_axes >= 2) {
            arrived_count++;
        }
    }

    return arrived_count >= icon_max;
}

/**
 * Moves the edit menu icons one step back toward their resting positions.
 *
 * Returns whether every icon has arrived.
 */
bool CalMoveToMenuIcon() {
    int arrived_count = 0;
    int icon_max = GetEditMenuMax();

    for (int slot = 0; slot < icon_max; slot++) {
        int        arrived_axes = 0;
        const auto target = GetEditMenuIconPos(slot);

        float delta_x = static_cast<float>(target[0]) - MenuIconPos[slot][0];
        MenuIconPos[slot][0] += delta_x / 4.0f;

        if (static_cast<float>(std::abs(static_cast<int>(delta_x))) < 2.0f) {
            MenuIconPos[slot][0] = static_cast<float>(target[0]);
            arrived_axes++;
        }

        float delta_y = static_cast<float>(target[1]) - MenuIconPos[slot][1];
        MenuIconPos[slot][1] += delta_y / 4.0f;

        if (static_cast<float>(std::abs(static_cast<int>(delta_y))) < 2.0f) {
            MenuIconPos[slot][1] = static_cast<float>(target[1]);
            arrived_axes++;
        }

        if (arrived_axes >= 2) {
            arrived_count++;
        }
    }

    return arrived_count >= icon_max;
}

/**
 * Deletes the edit menu's texture blocks and releases their textures.
 */
void DeleteEdMenuTextures() {
    std::array<int, 5> blocks = {EdMenuTextureBlock, EdMenuExTextureBlock, EdMenuExTextureBlock1, EdMenuExTextureBlock2, -1};

    MenuTextureDelete(blocks.data());
    TexManager.CleanUpTextureList();
}

/**
 * Releases the edit menu's textures and returns the pad to normal mode.
 */
void EditMenuExit() {
    DeleteEdMenuTextures();
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

/**
 * Returns whether to draw the help window given the requested flag.
 *
 * The window is hidden on the Atla page while its event flag is set.
 */
bool GetDrawHelpWindow(int draw) {
    if ((EditSwitch == EditMenuState::AtoraIn || EditSwitch == EditMenuState::Atora || EditSwitch == EditMenuState::AtoraOut) && GetMenuAtraEventFlag()) {
        return false;
    }

    return draw != 0;
}

} // namespace

void EditMenuInit(int *texture_blocks, int start_at_atora) {
    StartReadBG();
    auto *buffer = reinterpret_cast<u_long128 *>(EdMenuBuffer.base + (static_cast<std::ptrdiff_t>(EdMenuBuffer.used) * 16));
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
    EdMenuEffectFlag = EdMenuIconMove::Still;
    EdMenuEffectCt = 0;
    ItemVolumeStep.CheckItemVolume();

    if (start_at_atora != 0) {
        EdMenuTextureReadEndFlag = 1;
        EdMenuRGB = 0x40;
        MenuIconPos[0][0] = 80.0f;
        MenuIconPos[0][1] = 52.0f;
        std::array<int, 2> blocks = {EdMenuTextureBlock, EdMenuExTextureBlock};
        InitMenuAtora1(ATORA_OPEN_PLACE, NowEditMap, blocks.data(), EdMenuWorkBuf);
        InitMenuAtoraSelect(NowEditMap);
        EditSwitch = EditMenuState::Atora;
        CommonMenuMes2.edge_alpha = 0x80;
        CommonMenuMes3.edge_alpha = 0x80;
        return;
    }

    CommonMenuMes2.edge_alpha = 0;
    CommonMenuMes3.edge_alpha = 0;
    EdCur.selection = 0;
    EditSwitch = EditMenuState::Start;
    EdMenuRGB = 0x80;
    WindowPos[1] = 325.0f;
    GetMainMenuRightHelpWinLangOffset(WindowPos[0], WindowPos[1], EditMenuWinW, EditMenuWinH);
    EdCur.x = 48.0f;
    EdCur.y = 76.0f;
    constexpr std::array<s16, ICON_MAX> start_x = {-0xC0, -0xB0, -0xA0, -0x90, -0x80, -0x80};

    for (int slot = 0; slot < GetEditMenuMax(); slot++) {
        MenuIconPos[slot][0] = start_x[slot];
        MenuIconPos[slot][1] = static_cast<float>((slot * 40) + 0x4A);
    }
}

bool EditMenuTexturesLoading() {
    return EditSwitch == EditMenuState::Start && EdMenuTextureReadEndFlag == 0;
}

int EditMenuLoop() {
    ReadBG();
    int result = 0;

    switch (EditSwitch) {
        case EditMenuState::Select:
            result = EditMenuSelect();
            break;
        case EditMenuState::AnalyzeIn:
            ToAnalyzeEdit();
            break;
        case EditMenuState::AtoraMove:
            result = AtoraMove();
            break;
        case EditMenuState::Analyze:
            result = AnalyzeEdit();
            break;
        case EditMenuState::Atora:
        case EditMenuState::AtoraIn:
        case EditMenuState::AtoraOut:
            result = AtoraSelect();
            break;
        case EditMenuState::AnalyzeOut:
            FromAnalyzeEdit();
            break;
        case EditMenuState::Save:
        case EditMenuState::SaveIn:
        case EditMenuState::SaveOut:
            EditSaveKey();
            break;
        case EditMenuState::Option:
        case EditMenuState::OptionIn:
        case EditMenuState::OptionOut:
            EdOptionSelect();
            break;
        case EditMenuState::Manual:
        case EditMenuState::ManualIn:
        case EditMenuState::ManualOut:
            EdMenuManualKey();
            break;
        case EditMenuState::Exit:
            result = EditMenuToExit();
            break;
        default:
            break;
    }

    EditMenuDraw();
    return result;
}

void EditMenuDraw() {
    int text_x = 0;
    int text_y = 0;

    MenuTextureReload(EdMenuTextureBlock);
    setbilinear(0);
    FrameImageDraw(EdMenuRGB, 0x80);

    switch (EditSwitch) {
        case EditMenuState::Start:
            EditMenuStart();
            break;
        case EditMenuState::Select:
            EditMenuSelectDraw();
            break;
        case EditMenuState::AnalyzeIn:
            ToAnalyzeEditDraw();
            break;
        case EditMenuState::AtoraMove:
            AtoraMoveDraw();
            break;
        case EditMenuState::Analyze:
            AnalyzeEditDraw();
            break;
        case EditMenuState::AnalyzeOut:
            FromAnalyzeEditDraw();
            break;
        case EditMenuState::Atora:
        case EditMenuState::AtoraIn:
        case EditMenuState::AtoraOut:
            AtoraSelectDraw();
            break;
        case EditMenuState::Save:
        case EditMenuState::SaveIn:
        case EditMenuState::SaveOut:
            EditSaveDraw();
            break;
        case EditMenuState::Option:
        case EditMenuState::OptionIn:
        case EditMenuState::OptionOut:
            OptionDraw();
            break;
        case EditMenuState::Manual:
        case EditMenuState::ManualIn:
        case EditMenuState::ManualOut:
            EdMenuManualDraw();
            break;
        case EditMenuState::Exit:
            EditMenuToExitDraw();
            break;
        default:
            break;
    }

    CursorVibeCnt++;

    if (CursorVibeCnt >= 1080000000) {
        CursorVibeCnt = 0;
    }

    MenuTextureReload(CommonMenuMes2.tex_block);
    GetMainMenuRightHelpMsgLangOffset(text_x, text_y);
    GetMainMenuRightHelpWinLangOffset(WindowPos[0], WindowPos[1], EditMenuWinW, EditMenuWinH);
    CommonMenuMes2.text_x = static_cast<int>(WindowPos[0] + static_cast<float>(text_x));
    CommonMenuMes2.text_y = static_cast<int>(WindowPos[1] + static_cast<float>(text_y));

    if (EdMenuTextureReadEndFlag != 0) {
        if (EditSwitch == EditMenuState::Exit) {
            EdMenuHelpWinAlpha -= 8;

            if (EdMenuHelpWinAlpha < 0) {
                EdMenuHelpWinAlpha = 0;
            }

            CommonMenuMes2.edge_alpha = EdMenuHelpWinAlpha;
        } else {
            EdMenuHelpWinAlpha += 8;

            if (EdMenuHelpWinAlpha > 0x80) {
                EdMenuHelpWinAlpha = 0x80;
            }

            CommonMenuMes2.edge_alpha += 8;

            if (CommonMenuMes2.edge_alpha > 0x80) {
                CommonMenuMes2.edge_alpha = 0x80;
            }
        }

        if (GetDrawHelpWindow(MakeWin2Flag)) {
            MenuHelpWinDraw(static_cast<int>(WindowPos[0]), static_cast<int>(WindowPos[1]), EditMenuWinW, EditMenuWinH, EdMenuHelpWinAlpha);

            if (EdMenuMesMake2 != 0) {
                EdMenuMesMake2 = CommonMenuMes2.MakeMesWin(EdMenuMesNo2);
            }

            CommonMenuMes2.Step();
            CommonMenuMes2.DrawMesWin();
        }
    }

    setbilinear(1);
}

namespace {

/**
 * Draws the edit menu's opening and enters its page textures once they have been read.
 */
int EditMenuStart() {
    int        icon_max = GetEditMenuMax();
    const auto rest_x = GetEditMenuIconRestX(icon_max);

    for (int slot = 0; slot < icon_max; slot++) {
        MenuIconPos[slot][0] += (static_cast<float>(rest_x[slot]) - MenuIconPos[slot][0]) / 4.0f;
    }

    if (EdMenuRGB > 0x40) {
        EdMenuRGB -= 4;
    }

    int brightness = EdEffectCt * 7;

    if (brightness > 0x80) {
        brightness = 0x80;
    }

    for (int slot = 0; slot < GetEditMenuMax(); slot++) {
        int y_offset = 0;
        int x_offset = 0;
        int icon = EditMenuIconID[slot];
        GetMenuIconInfo(icon);
        int selected = 0;

        if (slot == EdCur.selection) {
            x_offset = 8;
            y_offset = 2;
            brightness = 0x80;
            selected = 1;
        }

        DrawMainMenuIcon(static_cast<int>(MenuIconPos[slot][0] - x_offset), static_cast<int>(MenuIconPos[slot][1] - y_offset), icon, selected, 0x80, brightness);
    }

    if (ReadBGSync() == 0 && EdMenuTextureReadEndFlag == 0) {
        std::array<LOADTEXTURE_INFO2, 3> textures = {LOADTEXTURE_INFO2{.name = "#frame_menuemenu#640#" SCREEN_HEIGHT_STR "#4"}};
        textures[1].block_no = textures[0].block_no = EdMenuTextureBlock;
        BG_READ_INFO *file = GetReadBGFile(0);
        auto         *pack = reinterpret_cast<u_int *>(file->buffer);
        textures[1].name = reinterpret_cast<char *>(GetPackFile(pack, "editmenu.img", nullptr));
        DeleteEdMenuTextures();
        TexManager.LoadTextureBlockEX(-1, textures.data());
        Analyze = TexManager.GetTexture("analyzeB", EdMenuTextureBlock);
        AnaBar = TexManager.GetTexture("anabar", EdMenuTextureBlock);
        PerBoardTex = TexManager.GetTexture("perbrd", EdMenuTextureBlock);
        VillageName = TexManager.GetTexture("vilname", -1);
        VillageBar = TexManager.GetTexture("viltag", -1);
        GetAtraMsgReadBuf = reinterpret_cast<s16 *>(GetPackFile(pack, "atrames.bin", nullptr));
        InitMenuMesSet(MENU_MES_SET_ALLMENU, reinterpret_cast<s16 *>(GetPackFile(pack, "allmenu.mes", nullptr)));
        EdMenuTextureReadEndFlag = 1;
    }

    EdEffectCt++;

    if (EdEffectCt >= 17 && EdMenuTextureReadEndFlag != 0) {
        EditSwitch = EditMenuState::Select;
        EdCur.selection = 0;
        EdEffectCt = 0;
    }

    return 0;
}

/**
 * Draws the edit menu page and its selected icon.
 */
void EditMenuSelectDraw() {
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
    DrawMenuObjectVibe(static_cast<int>(EdCur.x), static_cast<int>(EdCur.y), 1, 0x40);

    CommonMenuMes2.stay_frame = false;

    if (CommonMenuMes2.mes_made != info->help_mes_no) {
        CommonMenuMes2.MakeMesWin(info->help_mes_no);
    }
}

/**
 * Handles pad input in the edit menu's icon selection.
 */
int EditMenuSelect() {
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
        EditSwitch = EditMenuState::Exit;
        EdEffectCt = 0;
    } else if (GamePad.Down(PAD_CROSS) != 0) {
        constexpr std::array<EditMenuState, ICON_MAX> next_state = {EditMenuState::AtoraIn, EditMenuState::AtoraMoveIn, EditMenuState::AnalyzeIn, EditMenuState::SaveIn, EditMenuState::OptionIn, EditMenuState::ManualIn};
        EditSwitch = next_state[EdCur.selection];
        ComMenuSePlay(MENU_SOUND_CONFIRM);
        EdEffectCt = 0;
        CommonMenuMes2.mes_made = -1;
        u_long128 *work = EdMenuWorkBuf;

        switch (EditSwitch) {
            case EditMenuState::AtoraIn: {
                std::array<int, 2> blocks = {EdMenuTextureBlock, EdMenuExTextureBlock};
                int                area = GetNowMapTransAtraMap(MapNo);

                if (area < 0 || area > 4) {
                    area = 0;
                }

                std::println("now atra load area = {}", area);
                InitMenuAtora1(ATORA_OPEN_PLACE, NowEditMap, blocks.data(), work);
                InitMenuAtoraSelect(NowEditMap);
                EdMenuEffectFlag = EdMenuIconMove::MoveFrom;
                EdMenuEffectCt = 0;
                EdMenuMesMake2 = 1;
                break;
            }
            case EditMenuState::AtoraMoveIn:
                EditSwitch = EditMenuState::AtoraMove;
                break;
            case EditMenuState::AnalyzeIn:
                CommonMenuAtoraInfo = &EditPartsInfo;
                EdEffectCt = 1;

                std::ranges::fill(AnalyzeFill, 0.0f);
                ButtonAdd = 0;
                AnalyzeBackBlockCnt = 0;
                AnalyzeSelect = 0;
                MakeWin2Flag = 0;
                break;
            case EditMenuState::SaveIn:
                MakeWin2Flag = 0;
                EdMenuEffectFlag = EdMenuIconMove::MoveFrom;
                EdMenuMesMake2 = 1;
                InitMenuSave(SAVE_MENU_MODE_SAVE, EdMenuExTextureBlock, work);
                break;
            case EditMenuState::OptionIn:
                EdMenuMesMake2 = 1;
                EdMenuEffectFlag = EdMenuIconMove::MoveFrom;
                InitMenuOption(OPTION_OPEN_GAME_MENU, EdMenuExTextureBlock, work);
                break;
            case EditMenuState::ManualIn: {
                MakeWin2Flag = 0;
                EdMenuEffectFlag = EdMenuIconMove::MoveFrom;
                std::array<int, 3> blocks = {EdMenuExTextureBlock1, EdMenuExTextureBlock, EdMenuExTextureBlock2};
                InitMenuManual(blocks.data(), work);
                break;
            }
            default:
                break;
        }
    }

    return 0;
}

void EditMenuToExitDraw() { DrawMoveMenuIcon(); }

/**
 * Moves the edit menu offscreen while closing its message windows.
 *
 * Returns 1 once the menu has closed.
 */
int EditMenuToExit() {
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
void AtoraSelectDraw() {
    if (EdMenuTextureReadEndFlag == 0 && ReadBGSync() == 0) {
        EdMenuTextureReadEndFlag = 1;
    }

    DrawMenuAtoraSelect();

    switch (EdMenuEffectFlag) {
        case EdMenuIconMove::MoveFrom:
        case EdMenuIconMove::MoveTo:
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
int AtoraSelect() {
    int result = 0;

    switch (EdMenuEffectFlag) {
        case EdMenuIconMove::MoveFrom:
            if (CalMoveFromMenuIcon()) {
                EditSwitch = EditMenuState::Atora;
                EdMenuEffectFlag = EdMenuIconMove::Still;
                EdMenuEffectCt = 0.0f;
            }

            break;
        case EdMenuIconMove::MoveTo:
            if (CalMoveToMenuIcon()) {
                EditSwitch = EditMenuState::Select;
                EdCur.selection = 0;
                EdCur.x = static_cast<float>((EdCur.selection * 16) + 0x30);
                EdCur.y = static_cast<float>((EdCur.selection * 40) + 0x4C);
            }

            break;
        default:
            switch (MenuAtoraSelectKey()) {
                case ATORA_SELECT_PLACE_EXIT:
                case ATORA_SELECT_PLACE:
                    result = 1;
                    break;
                case ATORA_SELECT_CLOSE:
                    EditSwitch = EditMenuState::AtoraOut;
                    EdMenuEffectFlag = EdMenuIconMove::MoveTo;
                    EdMenuEffectCt = 0.0f;
                    break;
                default:
                    break;
            }

            break;
    }

    if (EdMenuEffectFlag != EdMenuIconMove::Still) {
        EdMenuEffectCt += 1.0f;
        EdEffectCt++;
    } else {
        EdMenuEffectCt = 0.0f;
        EdEffectCt = 0;
    }

    return result;
}

void AtoraMoveDraw() {}

/**
 * Closes the edit menu and hands the town the part-moving mode.
 */
int AtoraMove() {
    EditMenuExit();
    EditMenuStatus.mode = EDIT_MENU_MODE_MOVE;
    return 1;
}

/**
 * Draws the scrolling background panels on the analysis page.
 */
void AnalyzeBackDraw(int alpha, int brightness) {
    AnalyzeBackBlockCnt++;

    if (AnalyzeBackBlockCnt >= 0x10E) {
        AnalyzeBackBlockCnt = 0;
    }

    int x = (AnalyzeBackBlockCnt / 5) - 0x36;
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
float AnalyzeRequestPer() {
    CEditPartsInfo *info = CommonMenuAtoraInfo;
    auto            total = static_cast<float>(info->parts_max);

    if (total < 1.0f) {
        return 100.0f;
    }

    float done = 0.0f;

    for (s32 request : info->request) {
        if (request != 0) {
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
 * Draws the analysis page's bars.
 *
 * Returns 1 once the last bar's fill has reached its target.
 */
int AnalyzeBarDraw() {
    float parts_total = 0.0f;
    float parts_complete = 0.0f;
    float elements_total = 0.0f;
    int   elements_done = 0;
    int   parts_done = 0;

    for (int parts = 0; parts < 24; parts++) {
        EDITPARTS_INFO *info = CommonMenuAtoraInfo->GetPartsInfo(parts);

        if (info != nullptr && info->stock > 0) {
            parts_total += 1.0f;

            if (info->obtained != 0) {
                parts_done++;

                if (CommonMenuAtoraInfo->CheckComplete(parts) != 0) {
                    parts_complete += 1.0f;
                }
            }

            for (const auto &element : info->elements) {
                if (element.id < 0) {
                    break;
                }

                elements_total += 1.0f;

                if (element.enabled != 0) {
                    elements_done++;
                }
            }
        }
    }

    for (s16 element : std::span(SaveData->GetElemData(NowEditMap), 128)) {
        if (element > -1) {
            elements_done++;
        }
    }

    auto                 total = static_cast<int>(parts_total + elements_total);
    std::array<float, 3> percent{};
    std::array<float, 3> target{};

    if (total <= 0) {
        percent[0] = 0.0f;
    } else {
        percent[0] = 100.0f * static_cast<float>(parts_done + elements_done) / static_cast<float>(total);
    }

    if (static_cast<int>(parts_total) <= 0) {
        percent[1] = 0.0f;
    } else {
        percent[1] = 100.0f * parts_complete / parts_total;
    }

    percent[2] = AnalyzeRequestPer();

    for (int bar = 0; bar < 3; bar++) {
        target[bar] = 144.0f * percent[bar] / 100.0f;

        if (!(target[bar] <= 144.0f)) {
            target[bar] = 144.0f;
        }
    }

    int bars_started = 0;
    int x = -0x3D;

    for (int bar = 0; bar < 3; bar++) {
        x += 0xB5;

        if (bar != bars_started) {
            break;
        }

        AnalyzeFill[bar] += 2.0f + (2.0f * static_cast<float>(ButtonAdd));
        float goal = target[bar];

        if (goal < AnalyzeFill[bar]) {
            AnalyzeFill[bar] = goal;
        }

        float fill = AnalyzeFill[bar];
        float edge = 0.08f * fill;

        if (8.0f < edge) {
            edge = 8.0f;
        }

        auto     top = static_cast<int>(306.0f - fill);
        CRect_i_ bar_destination;
        CRect_i_ bar_source;
        bar_source.x = 0;
        bar_source.y = (bar * 16) + 8;
        bar_source.width = 0x36;
        bar_source.height = 7;
        auto height = static_cast<int>(fill);
        bar_destination.x = x;
        bar_destination.y = top;
        bar_destination.width = 0x36;
        bar_destination.height = height;
        DrawMenu2DSprite(AnaBar, bar_destination, bar_source, 0x80);
        DrawMenu2DSprite(AnaBar, CRect_i_(x, top, 0x36, static_cast<int>(edge)), CRect_i_(0, bar * 16, 0x36, 8), 0x80);
        edge = 0.1f * AnalyzeFill[bar];

        if (!(AnalyzeFill[bar] <= 0.0f)) {
            DrawMenu2DSprite(AnaBar, CRect_i_(x + 0x36, static_cast<int>(static_cast<float>(top) + edge), 10, static_cast<int>(static_cast<float>(0x130 - top) - edge)), CRect_i_(0x56, 0x36, 10, 9), 0x80);
        }

        auto number = static_cast<int>(100.0f * AnalyzeFill[bar] / 144.0f);

        if (percent[bar] < static_cast<float>(number)) {
            number = static_cast<int>(percent[bar]);
        }

        int digit_x = x + 0x1B;

        for (int digits = GetNumberKeta(number); digits != 0; digits--, digit_x -= 13) {
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

        if (0.4f * goal <= AnalyzeFill[bar]) {
            bars_started++;
        }
    }

    if (AnalyzeFill[2] == target[2]) {
        return 1;
    }

    return 0;
}

/**
 * Draws the analysis page's three bar headings.
 */
void DrawAnalyzeHeadings() {
    for (int bar = 0; bar < 3; bar++) {
        DrawMenu2DSprite(Analyze, CRect_i_((bar * 0xB5) + 0x56, 0x14a, 0x7E, 0x50), CRect_i_(0x80, bar * 0x50, 0x7E, 0x50), 0x80);
    }
}

/**
 * Draws the transition into the analysis page.
 */
void ToAnalyzeEditDraw() {
    MenuTextureReload(EdMenuTextureBlock);
    DrawAnalyzeHeadings();

    auto left = static_cast<float>(0x280 - (EdEffectCt * 16));
    auto width = static_cast<int>(80.0f + left);

    if (left < 0.0f) {
        left = 0.0f;
    }

    width = static_cast<int>(static_cast<float>(width) - left);

    if (width < 0) {
        width = 0;
    }

    FadeTexX(static_cast<int>(left), width, 0x280, 0, "frame_image", 0);
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
    int  done = 0;
    auto bar = static_cast<float>(EdEffectCt * 20);

    if (640.0f < bar) {
        bar = 640.0f;
    }

    if (560.0f < bar) {
        done = AnalyzeBarDraw();
    }

    DrawMenu2DSprite(AnaBar, CRect_i_(0, 0x130, static_cast<int>(bar), 0x18), CRect_i_(0, 0x36, 0x20, 0x18), 0x80);

    for (int x = 0x75; static_cast<float>(x) < bar; x += 0xB5) {
        auto segment_width = static_cast<int>(bar - static_cast<float>(x));

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
        EditSwitch = EditMenuState::Analyze;
    }
}

/**
 * Speeds up the transition into the analysis page while any input is held.
 */
void ToAnalyzeEdit() {
    if (GamePad.AllOn()) {
        ButtonAdd = 2;
        EdEffectCt += 4;
    }
}

/**
 * Draws the panels and progress bars on the analysis page.
 */
void AnalyzeEditDraw() {
    DrawMenuIcon(2);
    AnalyzeBackDraw(0x80, 0x40);
    DrawAnalyzeHeadings();
    AnalyzeBarDraw();
    DrawMenu2DSprite(AnaBar, CRect_i_(0, 0x130, 0x280, 0x18), CRect_i_(0, 0x36, 0x20, 0x18), 0x80);

    for (int bar = 0, x = 0x75; x < 0x280; bar++, x += 0xB5) {
        int width = 0x280 - x;

        if (width > 0x3C) {
            width = 0x3C;
        }

        if (AnalyzeFill[bar] > 0.0f) {
            DrawMenu2DSprite(AnaBar, CRect_i_(x, 0x130, width, 0xF), CRect_i_(0x20, 0x36, 0x36, 0xF), 0x80);
        }
    }
}

/**
 * Leaves the analysis page when circle or cross is pressed.
 */
int AnalyzeEdit() {
    if (GamePad.Down(PAD_CIRCLE | PAD_CROSS)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        EditSwitch = EditMenuState::AnalyzeOut;
        AnalyzeSelect = 2;
    }

    return 0;
}

/**
 * Draws the transition out of the analysis page.
 */
void FromAnalyzeEditDraw() {
    DrawMoveMenuIcon();
    int alpha = 0x80 - (EdEffectCt * 10);

    if (alpha < 0) {
        alpha = 0;
    }

    AnalyzeBackDraw(alpha, 0x40);
}

/**
 * Returns the cursor to the edit menu after analysis.
 */
void FromAnalyzeEdit() {
    CalMoveToMenuIcon();
    EdEffectCt++;

    if (EdEffectCt >= 0x10) {
        EdEffectCt = 0;
        MakeWin2Flag = 1;
        EdCur.selection = 2;
        EdCur.x = static_cast<float>((EdCur.selection * 16) + 0x30);
        EdCur.y = static_cast<float>((EdCur.selection * 40) + 0x4C);
        EditSwitch = EditMenuState::Select;
    }
}

/**
 * Draws the save page and its selected icon.
 */
void EditSaveDraw() {
    DrawMenuSave("frame_image");
    setbilinear(0);

    switch (EdMenuEffectFlag) {
        case EdMenuIconMove::MoveFrom:
        case EdMenuIconMove::MoveTo:
            DrawMoveMenuIcon();
            break;
        default:
            DrawMenuIcon(3);
            break;
    }
}

/**
 * Handles pad input on the edit menu's save page.
 */
void EditSaveKey() {
    // Left unset while the icons are still; the fade-out check below then reads it unset.
    int arrived;

    switch (EdMenuEffectFlag) {
        case EdMenuIconMove::MoveFrom:
            arrived = static_cast<int>(CalMoveFromMenuIcon());

            if (arrived) {
                EdMenuEffectFlag = EdMenuIconMove::Still;
                EdMenuEffectCt = 0.0f;
            }

            break;
        case EdMenuIconMove::MoveTo:
            arrived = static_cast<int>(CalMoveToMenuIcon());
            break;
        default:
            break;
    }

    if (EdMenuEffectFlag != EdMenuIconMove::Still) {
        EdMenuEffectCt += 1.0f;
    } else {
        EdMenuEffectCt = 0.0f;
    }

    MenuSaveKey();

    if (SaveMenuEffectFadeOut()) {
        EdMenuEffectFlag = EdMenuIconMove::MoveTo;

        if (arrived) {
            EdEffectCt = 0;
            EditSwitch = EditMenuState::Select;
            MakeWin2Flag = 1;
        }
    }
}

/**
 * Draws the options page and its selected icon.
 */
void OptionDraw() {
    DrawMenuOption();
    setbilinear(0);

    switch (EdMenuEffectFlag) {
        case EdMenuIconMove::MoveFrom:
        case EdMenuIconMove::MoveTo:
            DrawMoveMenuIcon();
            break;
        default:
            DrawMenuIcon(4);
            break;
    }
}

/**
 * Moves the edit menu icons one step for the current icon movement.
 *
 * Returns whether the icons arrived this step.
 */
bool StepMenuIconMove() {
    bool arrived = false;

    switch (EdMenuEffectFlag) {
        case EdMenuIconMove::MoveFrom:
            arrived = CalMoveFromMenuIcon();

            if (arrived) {
                EdMenuEffectFlag = EdMenuIconMove::Still;
                EdMenuEffectCt = 0.0f;
            }

            break;
        case EdMenuIconMove::MoveTo:
            arrived = CalMoveToMenuIcon();
            break;
        default:
            break;
    }

    if (EdMenuEffectFlag != EdMenuIconMove::Still) {
        EdMenuEffectCt += 1.0f;
    } else {
        EdMenuEffectCt = 0.0f;
    }

    return arrived;
}

/**
 * Handles pad input on the edit menu's option page.
 */
void EdOptionSelect() {
    bool arrived = StepMenuIconMove();
    int  key = MenuOptionKey();

    if (OptionMenuFadeOutStart()) {
        EdMenuEffectFlag = EdMenuIconMove::MoveTo;

        if (arrived && key) {
            EdEffectCt = 0;
            EditSwitch = EditMenuState::Select;
            MakeWin2Flag = 1;
        }
    }
}

/**
 * Handles pad input on the edit menu's manual page.
 */
int EdMenuManualKey() {
    bool arrived = StepMenuIconMove();
    int  key = MenuManualKey();

    if (GetNowManualMenuMode() == MANUAL_MODE_CLOSING) {
        EdMenuEffectFlag = EdMenuIconMove::MoveTo;

        if (arrived && key) {
            EdEffectCt = 0;
            EditSwitch = EditMenuState::Select;
            MakeWin2Flag = 1;
        }
    }

    return 0;
}

/**
 * Draws the manual page and its selected icon.
 */
void EdMenuManualDraw() {
    MenuManualDraw();
    setbilinear(0);

    switch (EdMenuEffectFlag) {
        case EdMenuIconMove::MoveFrom:
        case EdMenuIconMove::MoveTo:
            DrawMoveMenuIcon();
            break;
        default:
            DrawMenuIcon(5);
            break;
    }
}

} // namespace

} // namespace dc::game
