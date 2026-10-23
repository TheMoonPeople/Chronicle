#include "memcard.hpp"

#include <cmath>
#include <string>

#include "battlemenu/strings.hpp"
#include "caption_text.hpp"
#include "clsmes.hpp"
#include "gamepad.hpp"
#include "memorycardaccess.hpp"
#include "menu_draw.hpp"
#include "menu_pointer.hpp"
#include "menu_save.hpp"
#include "menuetc.hpp"
#include "mglib.hpp"
#include "options/screen.hpp"
#include "rect.hpp"
#include "save_slots.hpp"
#include "snd.hpp"
#include "texture.hpp"

PORT_OVERRIDE int InitMenuOption(int mode, int block_no, u_long128 *buffer) {
    return options::Open(mode, block_no, buffer);
}

PORT_OVERRIDE int MenuOptionKey() {
    return options::Run();
}

PORT_OVERRIDE void DrawMenuOption() {
    options::Draw();
}

PORT_OVERRIDE int OptionMenuFadeOutStart() {
    return options::g_screen.step == OPTION_STEP_FADE_OUT;
}

// The save screens without a memory card: no card is chosen, and the boards are every save of
// SaveSlots in file order, then on the save screen one "New file" board for a new save. The
// screens keep retail's steps otherwise; SaveMenu.file_no still names the file a step works on,
// so the confirmations, the save and the load are retail's.

void ExitSaveSelect();

int SaveMenuKeyFadeIn();
int SaveMenuKeyFadeOut();
int SaveMenuKeyModeSelect();
int SaveMenuKeyCheckMcType();
int SaveMenuKeyCheckMc();
int SaveMenuKeyLoadConfig();
int SaveMenuKeySaveCheck();
int SaveMenuKeySaveDecide();
int SaveMenuKeySave();
int SaveMenuKeyEndSave();
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
int SaveMenuKeySaveDecideEnding();

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
    }

    SaveMenu.step_time = 0;
    ComMenuSePlay(MENU_SOUND_REFUSE);
}

bool Listed(int file_no) {
    for (const SAVEDATA_INFO &info : SaveSlots.saves) {
        if (info.file_no == file_no + 1) {
            return true;
        }
    }

    return false;
}

bool NewSaveBoard() {
    return SaveMenu.access_kind == SAVE_ACCESS_SAVE;
}

// In place of the card choice: the one card is taken at once, as retail's Cross on slot 1 did.
int SaveMenuKeyOpen() {
    if (!SaveMenu.texture_ready) {
        return 1;
    }

    // The town and dungeon menus that open the save screen repeat the d-pad already; the title
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
                SaveSlotNew = row == static_cast<int>(SaveSlots.saves.size());
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
        }
    }

    return 1;
}

// Retail's, with the cursor on the file written, which a new save may have had to move on from.
int SaveMenuKeySaved() {
    SaveMenu.file_no = McAccess.file_no;
    return SaveMenuKeyEndSave();
}

// Retail's, with the save looked up in SaveSlots rather than McAccess.file_info.
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

// Alerts after the ending: Cross asks again and Circle closes the screen. Retail's alert went back
// to the card choice, which would now save again at once.
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

// The save after the ending writes the configuration alone. Retail started the write and waited on
// it, and nothing answered its failure, so the screen waited for ever; the write finishes here.
int SaveMenuKeySaveEnding() {
    if (McAccess.SaveSysConfig() == 1) {
        SaveMenu.key_no = SAVE_KEY_END_SAVE_ENDING;
    } else {
        SaveMenu.key_no = SAVE_KEY_ALERT;
        SaveMenu.alert_no = SAVE_ALERT_SAVE_FAILED;
    }

    return 1;
}

// Retail went back to the card choice, which would now save again; the screen closes instead.
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

PORT_OVERRIDE int (*SaveMenuFunc[26])() = {
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

// Retail's, with the boards of SaveSlots: the chosen one sits where retail's did and the rest
// scroll past above and below it, only those on screen drawn. The boards stay up while the
// list is read again after a save, which no longer takes long enough to show.
PORT_OVERRIDE void DrawMenuSave(char *frame_name) {
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
    }

    int alpha = 0x80;

    switch (SaveMenu.key_no) {
        case SAVE_KEY_FADE_IN:
            alpha = SaveMenu.step_time * 6;

            if (alpha > 0x80) {
                alpha = 0x80;
            }

            break;
        case SAVE_KEY_FADE_OUT:
            alpha = 0x80 - SaveMenu.step_time * 4;

            if (alpha < 0) {
                alpha = 0;
            }

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
    }

    if (SaveMenu.key_no == SAVE_KEY_FADE_OUT && SaveMenu.mode == SAVE_MENU_MODE_LOAD && SaveMenu.loaded != 0) {
        show = 1;
    }

    if (show != 0) {
        for (int row = 0; row < rows; row++, y += kBoardStep) {
            if (y <= -kBoardStep || y >= SCREEN_HEIGHT) {
                continue;
            }

            if (row < static_cast<int>(SaveSlots.saves.size())) {
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
                    text_x = 196.0f;
                    text_y = 140.0f;
                    CommonMenuMes2.auto_pos = MES_POS_CENTRE;
                    break;
                case SAVE_KEY_ALERT:
                case SAVE_KEY_AFTER_ENDING:
                case SAVE_KEY_END_SAVE_ENDING:
                case SAVE_KEY_END_SAVE:
                    text_x = 196.0f;
                    text_y = 140.0f;
                    CommonMenuMes2.auto_pos = MES_POS_CENTRE;
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
    CommonMenuMes2.edge_alpha = alpha;

    if (CommonMenuMes2.edge_alpha > 0x80) {
        CommonMenuMes2.edge_alpha = 0x80;
    }

    if (CommonMenuMes2.edge_alpha < 0) {
        CommonMenuMes2.edge_alpha = 0;
    }

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
    }

    if (0 < hand_x && 0 < hand_y) {
        static int ct = 0;
        RECT       hand = {0x160, 0xD6, 0x20, 0x20};
        float      draw_x = static_cast<float>(hand_x) + 7.0f * cosf(0.0805536583f * ct);
        float      draw_y = static_cast<float>(hand_y) + 5.0f * sinf(0.116355285f * ct);
        CRect_i_   source(0x160, 0xD6, 0x20, 0x20);
        DrawMenu2DSprite(SaveBoard, CRect_i_(static_cast<int>(5.0f + draw_x), static_cast<int>(3.0f + draw_y), hand.width, hand.height), source, 0, 0, 0, alpha);
        DrawMenu2DSprite(SaveBoard, CRect_i_(static_cast<int>(draw_x), static_cast<int>(draw_y), hand.width, hand.height), source, alpha);
        ct++;

        if (!(static_cast<float>(ct) < 105299.0f)) {
            ct = 0;
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
            // LOAD in the menu captions' lettering, over the painted one: its letters from 6 to 72 of the picture, their
            // capitals from row 5 to 24. The painted LOAD without the font.
            std::string title, desc, save_word, load_word, prompt;
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
    }
}

// The menus' hand follows the mouse while a screen has taken it as a pointer (port/src/menu_mouse.cpp).
PORT_OVERRIDE void DrawMenuObjectVibe(int x, int y, int shadow, int icon_u) {
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
