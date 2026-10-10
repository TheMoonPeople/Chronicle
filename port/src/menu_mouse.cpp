#include "menu_pointer.hpp"

#include <algorithm>
#include <cmath>

#include "battlemenu.hpp"
#include "itemdata.hpp"
#include "dngstatusdata.hpp"
#include "editpartsinfo.hpp"
#include "memcard.hpp"
#include "gamepad.hpp"
#include "gametext.hpp"
#include "menu_draw.hpp"
#include "menu_inventory.hpp"
#include "menu_manual.hpp"
#include "weapon_buildup.hpp"
#include "menu_misc.hpp"
#include "menu_option.hpp"
#include "menu_save.hpp"
#include "save_slots.hpp"
#include "menu_mouse.hpp"
#include "name_mouse.hpp"
#include "platform/input.hpp"

// The mouse in the game's menus. Each tick, ahead of the game's read of the pad, the screen that is
// open (the Register Name screen, or a page of the pause menu) takes the mouse as a pointer: what
// the pointer is over becomes the game's own cursor, a click a pad press the game reads, so the
// game's code does the rest. A screen with no handler leaves the mouse to the pad as before.

namespace {

struct Rect {
    int left;
    int top;
    int right;
    int bottom;

    bool Contains(float x, float y) const { return x >= left && x < right && y >= top && y < bottom; }
};

// Which pause-menu screen is on, drawn since the last tick.
bool g_battle_drawn = false;
MenuPointer g_save;
bool g_save_open = false;

MenuPointer g_battle;

// How many icons the ring shows, as the game's GetMenuModeMax counts them.
int RingIconCount() {
    int count = BtlMenuMode == BATTLE_MENU_MODE_DUNGEON ? 7 : 8;
    if (GetGameFlagForManualMenu() == 0) {
        count--;
    }
    return count;
}

// The row of one ring icon: its icon and the label beside it, where the brackets stand.
Rect RingRow(int icon) {
    int left = static_cast<int>(NorMenuIcon[icon].x) - 18;
    int top = static_cast<int>(NorMenuIcon[icon].y) - 8;
    return Rect{left, top, left + 250, top + 34};
}

// The bar of icons that opens the pause menu.
void RingMouse(const MenuPointerTake &take) {
    if (MenuWarningMsgFlag != 0) {
        // A warning is up: any click dismisses it, as Cross and Circle do.
        if (take.clicked != 0) {
            MenuPointerPress(kInputCross);
        }
        return;
    }
    if (!take.pointing) {
        return;
    }
    int hit = -1;
    for (int icon = 0; icon < RingIconCount(); ++icon) {
        if (RingRow(icon).Contains(g_battle.x, g_battle.y)) {
            hit = icon;
        }
    }
    if (hit >= 0 && hit != MenuSelect[1] && (take.moved || (take.clicked & 1) != 0)) {
        MenuSelect[1] = hit;
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if ((take.clicked & 1) != 0) {
        if (hit >= 0) {
            MenuPointerPress(kInputCross);
        }
    } else if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
    }
}

// A two-line yes-or-no plate drawn at (x, y) by DrawDngYesNoDialog: Yes is the first line.
// Returns 0 over Yes, 1 over No, -1 elsewhere.
int YesNoHit(int x, int y, float px, float py) {
    if (px < x || px >= x + 0x60) {
        return -1;
    }
    if (py >= y && py < y + 0x1C) {
        return 0;
    }
    if (py >= y + 0x1C && py < y + 0x3C) {
        return 1;
    }
    return -1;
}

// Hover and click on a yes-or-no plate whose answer the game keeps in *answer (0 is Yes).
template <typename T>
void YesNoMouse(const MenuPointerTake &take, int x, int y, T *answer) {
    int hit = take.pointing ? YesNoHit(x, y, g_battle.x, g_battle.y) : -1;
    if (hit >= 0 && hit != *answer && take.moved) {
        *answer = static_cast<T>(hit);
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if ((take.clicked & 1) != 0 && hit >= 0) {
        *answer = static_cast<T>(hit);
        MenuPointerPress(kInputCross);
    } else if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
    }
}


// The scroll bar of the personal board (the item board, the attachments board, the Georama board):
// arrows at its ends step a row, a press on the track takes hold of the thumb, which follows the
// pointer while the button is held.
const Rect kScrollUp = {566, 124, 588, 142};
const Rect kScrollDown = {566, 254, 588, 272};
const Rect kScrollTrack = {566, 142, 588, 254};
bool       g_scroll_drag = false;

// Scrolls a board to top_row, and brings the cursor into the rows shown if it sat on the cells.
void ScrollBoardTo(PERSONAL_BOARD &board, int max, int top_row, bool follow) {
    int last_top = std::max(max / 5 - 4, 0);
    top_row = std::clamp(top_row, 0, last_top);
    if (top_row == board.top_row) {
        return;
    }
    board.top_row = top_row;
    if (follow && board.cursor_area == PERSONAL_BOARD_AREA_CELLS) {
        int row = board.cursor / 5;
        int column = board.cursor % 5;
        if (row < top_row) {
            board.cursor = top_row * 5 + column;
        } else if (row > top_row + 3) {
            board.cursor = (top_row + 3) * 5 + column;
        }
    }
}

// Returns true when the pointer is working the scroll bar.
bool BoardScrollBar(const MenuPointerTake &take, PERSONAL_BOARD &board, int max, bool follow) {
    if ((take.held & 1) == 0) {
        g_scroll_drag = false;
    }
    if (!take.pointing) {
        return false;
    }
    float x = g_battle.x;
    float y = g_battle.y;
    if ((take.clicked & 1) != 0) {
        if (kScrollUp.Contains(x, y)) {
            ScrollBoardTo(board, max, board.top_row - 1, follow);
            return true;
        }
        if (kScrollDown.Contains(x, y)) {
            ScrollBoardTo(board, max, board.top_row + 1, follow);
            return true;
        }
        if (kScrollTrack.Contains(x, y)) {
            g_scroll_drag = true;
        }
    }
    if (g_scroll_drag) {
        int   last_top = std::max(max / 5 - 4, 0);
        float fraction = (y - kScrollTrack.top - 8.0f) / static_cast<float>(kScrollTrack.bottom - kScrollTrack.top - 16);
        ScrollBoardTo(board, max, static_cast<int>(std::lround(std::clamp(fraction, 0.0f, 1.0f) * last_top)), follow);
        return true;
    }
    return false;
}

// A wheel notch over a board scrolls it a row.
void BoardWheel(const MenuPointerTake &take, PERSONAL_BOARD &board, int max, bool follow) {
    ScrollBoardTo(board, max, board.top_row + (take.wheel < 0.0f ? 1 : -1), follow);
}

// The question asked before a held item is thrown away: dropped on empty ground, an item has nowhere
// to go, and the trash can is the game's own way to lose one.
struct Discard {
    bool active = false;
    int  answer = 1;
};
Discard g_discard;
bool    g_item_dragging = false;
int     g_item_drag_mode = -1;
int     g_item_drag_cursor = -1;
int     g_item_drag_area = -1;

constexpr int kDiscardPlateX = 0x118;
constexpr int kDiscardPlateY = 0xE8;

bool HoldingItem() { return BtlHaveItemPt != nullptr && BtlHaveItemPt->item_no >= ITEM_ATTACH_START; }

void AskToDiscard() {
    if (IsEnableTrushThrow(BtlHaveItemPt->item_no) == 0) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return;
    }
    g_discard.active = true;
    g_discard.answer = 1;
    ComMenuSePlay(MENU_SOUND_CONFIRM);
}

void DiscardMouse(const MenuPointerTake &take) {
    if (!take.pointing || !HoldingItem()) {
        g_discard.active = false;
        return;
    }
    int hit = YesNoHit(kDiscardPlateX, kDiscardPlateY, g_battle.x, g_battle.y);
    if (hit >= 0 && hit != g_discard.answer && take.moved) {
        g_discard.answer = hit;
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if ((take.clicked & 1) != 0 && hit >= 0) {
        g_discard.active = false;
        if (hit == 0) {
            // Through the trash can, as the pad throws an item away.
            ItemMenuMode.mode = ITEM_MENU_BOARD;
            ItemMenuMode.board.cursor_area = PERSONAL_BOARD_AREA_TRASH;
            MenuPointerPress(kInputCross);
        } else {
            ComMenuSePlay(MENU_SOUND_REFUSE);
        }
    } else if ((take.clicked & 2) != 0) {
        g_discard.active = false;
        ComMenuSePlay(MENU_SOUND_REFUSE);
    }
}

// Leave Dungeon and the other two-answer pages of the travel menu.
void MoveMouse(const MenuPointerTake &take) {
    if (MenuMove.state != MENU_MOVE_STATE_SELECT) {
        return;
    }
    switch (MenuMove.mode) {
        case MENU_MOVE_DUNGEON_ESCAPE:
        case MENU_MOVE_FIRST_DUNGEON:
        case MENU_MOVE_INTERIOR_OUT: {
            int y = MenuMove.mode == MENU_MOVE_DUNGEON_ESCAPE ? 0xDC : 0xD8;
            YesNoMouse(take, 0x118, y, &MenuMove.cursor);
            break;
        }
        default:
            // The world map is a pad affair for now.
            if ((take.clicked & 2) != 0) {
                MenuPointerPress(kInputCircle);
            }
            break;
    }
}

void SaveMouse() {
    bool open = SaveMenu.texture_ready != 0 && SaveMenu.key_no != SAVE_KEY_FADE_OUT &&
                SaveMenu.key_no != SAVE_KEY_FADE_IN;
    if (!open) {
        if (g_save_open) {
            g_save.Close();
            g_save_open = false;
        }
        return;
    }
    if (!g_save_open) {
        g_save.Open();
        g_save_open = true;
    }
    MenuPointerTake take = g_save.Take(320.0f, 240.0f);
    MenuPointerShow(&g_save);
    bool slots = SaveMenu.key_no == SAVE_KEY_FILE_SELECT;
    if (slots) {
        int count = SaveSlotRows(SaveSlots, SaveMenu.access_kind == SAVE_ACCESS_SAVE);
        int selected = SaveSlotRow(SaveSlots, SaveMenu.file_no, SaveMenu.access_kind == SAVE_ACCESS_SAVE);
        if (take.pointing) {
            for (int row = 0; row < count; ++row) {
                float top = SaveMenu.board_y + (row - selected) * 150.0f;
                if (g_save.x >= 140.0f && g_save.x < 500.0f && g_save.y >= top && g_save.y < top + 126.0f) {
                    if (row != selected && take.moved) {
                        SaveMenu.file_no = SaveSlotFileAt(SaveSlots, row);
                        ComMenuSePlay(MENU_SOUND_CURSOR);
                    }
                    if ((take.clicked & 1) != 0) MenuPointerPress(kInputCross);
                    break;
                }
            }
        }
    } else {
        bool confirm = SaveMenu.key_no == SAVE_KEY_LOAD_DECIDE || SaveMenu.key_no == SAVE_KEY_SAVE_DECIDE ||
                       SaveMenu.key_no == SAVE_KEY_AFTER_ENDING;
        int x = SaveMenu.key_no == SAVE_KEY_AFTER_ENDING ? 196 : 246;
        int y = SaveMenu.key_no == SAVE_KEY_AFTER_ENDING ? 140 : 156;
        int hit = take.pointing ? YesNoHit(x, y, g_save.x, g_save.y) : -1;
        if (confirm && hit >= 0) {
            if ((take.clicked & 1) != 0) MenuPointerPress(hit == 0 ? kInputCross : kInputCircle);
            if ((take.clicked & 2) != 0) MenuPointerPress(kInputCircle);
        }
    }
}

// The turntable of party members. The one at the front is the one the page acts on; a click on any
// other turns it to the front, a wheel notch turns one place.
int g_allies_turns = 0;

Rect AlliesFront() {
    // DrawCharaSelect's frame round the front member.
    int left = static_cast<int>(394.0f - chara_r_long - 184.0f);
    int top = static_cast<int>(120.0f - 26.0f);
    return Rect{left, top, left + 0x102, top + 0x82};
}

Rect AlliesFace(int place) {
    float angle = 3.14159265f + (PosAngle * static_cast<float>(place));
    int   x = static_cast<int>(394.0f + chara_r_long * cosf(angle));
    int   y = static_cast<int>(120.0f + chara_r_long * sinf(angle));
    return Rect{x, y, x + 90, y + 90};
}

void AlliesMouse(const MenuPointerTake &take) {
    if (MenuChara.state != MENU_CHARA_SELECT) {
        if (MenuChara.state != MENU_CHARA_TURN) {
            g_allies_turns = 0;
        }
        return;
    }
    // One place a turn, as the d-pad turns it; Up and Right bring the one behind round, Down and Left the next.
    if (g_allies_turns != 0) {
        MenuPointerPress(g_allies_turns > 0 ? kInputUp : kInputDown);
        g_allies_turns += g_allies_turns > 0 ? -1 : 1;
        return;
    }
    if (!take.pointing) {
        return;
    }
    if (take.wheel != 0.0f) {
        g_allies_turns = take.wheel > 0.0f ? 1 : -1;
        return;
    }
    if ((take.clicked & 1) != 0) {
        if (AlliesFront().Contains(g_battle.x, g_battle.y)) {
            MenuPointerPress(kInputCross);
            return;
        }
        for (int i = 0; i < 6; ++i) {
            int place = SysChara[i].place;
            if (place != 0 && AlliesFace(place).Contains(g_battle.x, g_battle.y)) {
                // Up and Right move every place up one, so a member at place p needs 6 - p of them, or p Downs.
                g_allies_turns = place > 3 ? 6 - place : -place;
                return;
            }
        }
    } else if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
    }
}

// The item page: the quick-use slots, the party member's panel, the personal board with its tabs,
// arrows and trash can, and the confirmation of a power-up. Each place the pointer can be over is
// where DrawMenuWaku puts the game's brackets for it (ItemMenuModeDraw).
const Rect kItemQuickSlot[3] = {
    {85,  102, 127, 136},
    {149, 102, 191, 136},
    {213, 102, 255, 136},
};
const Rect kItemWeapon = {68, 189, 110, 223};
const Rect kItemDefense = {112, 189, 154, 223};
const Rect kItemChara = {192, 188, 296, 292};
const Rect kItemBoard = {355, 124, 555, 284};
const Rect kItemTrash = {557, 270, 601, 304};
// The party arrows round the panel and the page arrows over the board.
const Rect kItemPartyLeft = {46, 150, 80, 184};
const Rect kItemPartyRight = {278, 150, 312, 184};
const Rect kItemPageLeft = {322, 38, 356, 72};
const Rect kItemPageRight = {552, 38, 586, 72};

Rect ItemPageTab(int page) {
    int left = 0x154 + 0xE + page * 0x44;
    return Rect{left, 72, left + 0x44, 120};
}

bool ItemPanelMode(int mode) {
    return mode == ITEM_MENU_QUICK_SLOTS || mode == ITEM_MENU_CHARA || mode == ITEM_MENU_WEAPON ||
           mode == ITEM_MENU_DEFENSE;
}

void ItemMouse(const MenuPointerTake &take) {
    if (ItemMenuMode.state == ITEM_MENU_STATE_POWERUP_CONFIRM) {
        YesNoMouse(take, 0x114, 0xE6, &ItemMenuMode.confirm);
        return;
    }
    if (ItemMenuMode.state != ITEM_MENU_STATE_IDLE) {
        if ((take.clicked & 2) != 0) {
            MenuPointerPress(kInputCircle);
        }
        return;
    }
    if (g_discard.active) {
        DiscardMouse(take);
        return;
    }
    if (!take.pointing) {
        return;
    }
    float x = g_battle.x;
    float y = g_battle.y;
    PERSONAL_BOARD &board = ItemMenuMode.board;
    bool            clicked = (take.clicked & 1) != 0;
    bool            act = take.moved || clicked;

    if (BoardScrollBar(take, board, PersonalRetMax(board.page), ItemMenuMode.mode == ITEM_MENU_BOARD)) {
        return;
    }

    // The place the pointer is over, as the mode and cursor the game would be in with its hand there.
    int mode = -1;
    int cursor = board.cursor;
    int area = board.cursor_area;
    int tab = -1;
    int arrow = 0;
    for (int i = 0; i < 3; ++i) {
        if (kItemQuickSlot[i].Contains(x, y)) {
            mode = ITEM_MENU_QUICK_SLOTS;
            cursor = i;
        }
    }
    if (kItemWeapon.Contains(x, y)) {
        mode = ITEM_MENU_WEAPON;
    } else if (kItemDefense.Contains(x, y)) {
        mode = ITEM_MENU_DEFENSE;
    } else if (kItemChara.Contains(x, y)) {
        mode = ITEM_MENU_CHARA;
    } else if (kItemBoard.Contains(x, y)) {
        int column = static_cast<int>((x - kItemBoard.left) / 40);
        int row = static_cast<int>((y - kItemBoard.top) / 40);
        int index = (board.top_row + row) * 5 + column;
        if (index < PersonalRetMax(board.page)) {
            mode = ITEM_MENU_BOARD;
            area = PERSONAL_BOARD_AREA_CELLS;
            cursor = index;
        }
    } else if (kItemTrash.Contains(x, y)) {
        mode = ITEM_MENU_BOARD;
        area = PERSONAL_BOARD_AREA_TRASH;
    }
    for (int i = 0; i < 3; ++i) {
        if (ItemPageTab(i).Contains(x, y)) {
            tab = i;
        }
    }
    if (BtlMenuStatusPt->party_size > 1 && ItemPanelMode(ItemMenuMode.mode)) {
        if (kItemPartyLeft.Contains(x, y)) {
            arrow = -1;
        } else if (kItemPartyRight.Contains(x, y)) {
            arrow = 1;
        }
    }
    if (!ItemPanelMode(ItemMenuMode.mode)) {
        if (kItemPageLeft.Contains(x, y)) {
            arrow = -1;
        } else if (kItemPageRight.Contains(x, y)) {
            arrow = 1;
        }
    }

    if (take.wheel != 0.0f) {
        // Over the board it scrolls; elsewhere it turns the page or the party member.
        bool forward = take.wheel < 0.0f;
        if (kItemBoard.Contains(x, y)) {
            BoardWheel(take, board, PersonalRetMax(board.page), ItemMenuMode.mode == ITEM_MENU_BOARD);
        } else {
            MenuPointerPress(forward ? kInputR1 : kInputL1);
        }
        return;
    }

    if (mode >= 0 && act) {
        bool moved = mode != ItemMenuMode.mode || cursor != board.cursor ||
                     (mode == ITEM_MENU_BOARD && area != board.cursor_area);
        if (moved) {
            ItemMenuMode.mode = static_cast<short>(mode);
            board.cursor = cursor;
            board.cursor_area = area;
            ComMenuSePlay(MENU_SOUND_CURSOR);
        }
    }

    // An item picked up with the button held rides the pointer: let go over another place and it goes there,
    // over empty ground and the question about throwing it away comes up, over the same place and it stays held.
    if ((take.released & 1) != 0 && g_item_dragging) {
        g_item_dragging = false;
        if (HoldingItem()) {
            bool same = mode == g_item_drag_mode && cursor == g_item_drag_cursor && area == g_item_drag_area;
            if (mode >= 0 && !same) {
                MenuPointerPress(kInputCross);
            } else if (mode < 0 && tab < 0 && arrow == 0 && !kScrollTrack.Contains(x, y)) {
                AskToDiscard();
            }
        }
        return;
    }

    if (clicked) {
        if (mode >= 0) {
            if (!HoldingItem() && mode == ITEM_MENU_BOARD) {
                g_item_dragging = true;
                g_item_drag_mode = mode;
                g_item_drag_cursor = cursor;
                g_item_drag_area = area;
            }
            MenuPointerPress(kInputCross);
        } else if (HoldingItem() && tab < 0 && arrow == 0) {
            // Set down on empty ground.
            AskToDiscard();
        } else if (tab >= 0 || (arrow != 0 && !ItemPanelMode(ItemMenuMode.mode))) {
            // The page tabs and arrows work on the board's pages, as L1 and R1 do from the board.
            if (ItemPanelMode(ItemMenuMode.mode)) {
                ItemMenuMode.mode = ITEM_MENU_BOARD;
                board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
                board.cursor = board.top_row * 5;
            }
            int steps = arrow != 0 ? arrow : (tab - board.page + 3) % 3 == 2 ? -1 : (tab - board.page + 3) % 3;
            if (steps > 0) {
                MenuPointerPress(kInputR1);
            } else if (steps < 0) {
                MenuPointerPress(kInputL1);
            }
        } else if (arrow != 0) {
            MenuPointerPress(arrow > 0 ? kInputR1 : kInputL1);
        }
    } else if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
    }
}

// The Georama Parts page: the list of parts on the left, with a cell for the part and one for each
// of its six chips, and the board of chips the player holds on the right.
EDITPARTS_INFO *AtlaPartsInfo(int index) {
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

// Where the game's brackets stand for the part (cursor 0) and each chip (1 to 6).
Rect AtlaPartCell(int cursor) {
    if (cursor == 0) {
        return Rect{79, 147, 165, 233};
    }
    int x = ((cursor + 2) % 3) * 0x2C + 0x92 + 24;
    int y = (cursor < 4 ? 167 : 210) - 19;
    return Rect{x - 4, y - 4, x + 40, y + 40};
}

void AtlaMouse(const MenuPointerTake &take) {
    if (MenuAtoraSel.step != ATORA_STEP_RUN) {
        return;
    }
    if (!take.pointing) {
        return;
    }
    float x = g_battle.x;
    float y = g_battle.y;
    bool  clicked = (take.clicked & 1) != 0;
    MENU_ATORA_SEL &sel = MenuAtoraSel;
    int             mode = -1;
    int             cursor = -1;

    if (kItemBoard.Contains(x, y)) {
        int column = static_cast<int>((x - kItemBoard.left) / 40);
        int row = static_cast<int>((y - kItemBoard.top) / 40);
        mode = ATORA_SIDE_CHIP_LIST;
        cursor = (sel.board.top_row + row) * 5 + column;
        if (cursor >= PersonalRetMax(sel.board.page)) {
            mode = -1;
        }
    } else {
        EDITPARTS_INFO *info = AtlaPartsInfo(sel.board_pos);
        if (info != nullptr) {
            for (int c = 0; c <= 6; ++c) {
                // A chip's cell answers only where the part has a chip for it.
                if (c > 0 && info->elements[c - 1].id < 0) {
                    continue;
                }
                if (AtlaPartCell(c).Contains(x, y)) {
                    mode = ATORA_SIDE_BOARD;
                    cursor = c;
                }
            }
        }
    }

    if (BoardScrollBar(take, sel.board, PersonalRetMax(sel.board.page), sel.mode == ATORA_SIDE_CHIP_LIST)) {
        return;
    }
    if (take.wheel != 0.0f) {
        bool forward = take.wheel < 0.0f;
        if (kItemBoard.Contains(x, y)) {
            BoardWheel(take, sel.board, PersonalRetMax(sel.board.page), sel.mode == ATORA_SIDE_CHIP_LIST);
        } else {
            if (sel.mode == ATORA_SIDE_BOARD) {
                sel.board.cursor = 0;
            }
            MenuPointerPress(forward ? kInputDown : kInputUp);
        }
        return;
    }

    if (mode >= 0 && (take.moved || clicked)) {
        if (sel.mode != mode || sel.board.cursor != cursor) {
            sel.mode = mode;
            sel.board.cursor = cursor;
            sel.board.cursor_area = PERSONAL_BOARD_AREA_CELLS;
            ComMenuSePlay(MENU_SOUND_CURSOR);
        }
    }
    if (clicked) {
        if (mode >= 0) {
            MenuPointerPress(kInputCross);
        } else if (GetAtoraMaxVillage() - 3 > 0) {
            // The arrows over the board turn to the next town's parts.
            Rect left = {0x146 - 4, 62, 0x146 + 30, 94};
            Rect right = {0x20C - 4, 62, 0x20C + 30, 94};
            if (left.Contains(x, y)) {
                MenuPointerPress(kInputL1);
            } else if (right.Contains(x, y)) {
                MenuPointerPress(kInputR1);
            }
        }
    } else if ((take.clicked & 2) != 0) {
        MenuPointerPress(kInputCircle);
    }
}

// Where the game's brackets stand for a row of the tag board under the weapon (WeaponMenuDraw), or an
// empty rectangle where the page has no such row.
Rect WepTagRow(int page, int row) {
    switch (page) {
        case WEP_TAG_STATUS:
            if (row < 2) {
                return Rect{64, 270 + 20 * row, 266, 290 + 20 * row};
            }
            if (row < 6) {
                return Rect{70, 322 + 16 * (row - 2), 266, 338 + 16 * (row - 2)};
            }
            break;
        case WEP_TAG_ELEMENT:
            if (row < 5) {
                return Rect{70, 268 + 24 * row, 266, 292 + 24 * row};
            }
            break;
        case WEP_TAG_VS_MONSTER:
            if (row < 10) {
                int left = 68 + 104 * (row / 5);
                return Rect{left, 268 + 24 * (row % 5), left + 102, 292 + 24 * (row % 5)};
            }
            break;
    }
    return Rect{0, 0, 0, 0};
}

// The weapon page. WeaponMouse returns false in the modes the mouse does not handle, which leave it
// to the pad.
int g_weapon_chara_target = -1;

// The rows of the action dialog the cursor can be on, top to bottom: Equip, Attach, Element and Repair,
// then Level up, Break down and Build up where the weapon allows them. Gives how many.
int WeaponActionRows(int *cursors) {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        cursors[count++] = i;
    }
    // The game's NowWeaponStatusValue: bit 1 is Level up, bit 2 Break down, bit 3 Build up.
    WEAPON_HAVE *weapon = &DngWepHavePt[WepMenu.weapon_slot];
    int          options = 0;
    if (weapon->experience >= GetWeaponMaxExp(weapon) || GetNowItemNum(ITEM_POWERUP_POWDER, MenuItemPackPt) > 0) {
        options |= 2;
    }
    if (WeaponStatusBreakEnable(weapon) != 0) {
        options |= 4;
    }
    options |= 8;
    int build_up = 0;
    if (WeaponStatusBuildUp(weapon, build_up) <= 0 || GetNowWeaponAttachNum(weapon) != 0 || build_up <= 0) {
        // Build up is on offer only for a weapon with something to build up to.
        options &= ~0x10;
    }
    if (IsNotBuildUpWeapon(weapon->item_no) != 0) {
        options &= ~8;
    }
    for (int i = 1; i <= 3; ++i) {
        if ((options & (1 << i)) != 0) {
            cursors[count++] = i + 3;
        }
    }
    return count;
}

bool WeaponMouse(const MenuPointerTake &take) {
    bool clicked = (take.clicked & 1) != 0;
    switch (WepMenu.state) {
        case WEP_STATE_WARNING:
        case WEP_STATE_SCRAP_REFUSED:
        case WEP_STATE_LEVELUP_EFFECT:
        case WEP_STATE_STATUS_BREAK_EFFECT:
        case WEP_STATE_BUILDUP_EFFECT:
        case WEP_STATE_REPAIR_EFFECT:
            // A message or an effect that waits for a button.
            if (take.clicked != 0) {
                MenuPointerPress((take.clicked & 2) != 0 ? kInputCircle : kInputCross);
            }
            return true;
        case WEP_STATE_OPEN:
        case WEP_STATE_CLOSE:
        case WEP_STATE_CHARA_CHANGE:
        case WEP_STATE_EQUIP:
            return true;
        case WEP_STATE_IDLE:
            break;
        default:
            return false;
    }
    if (WepMenu.mode != WEP_MENU_LIST) {
        g_weapon_chara_target = -1;
    }
    float x = g_battle.x;
    float y = g_battle.y;

    switch (WepMenu.mode) {
        case WEP_MENU_LIST: {
            // Walking to the party member clicked on: one L1 or R1 a tick.
            if (g_weapon_chara_target >= 0) {
                if (WepMenu.chara == g_weapon_chara_target) {
                    g_weapon_chara_target = -1;
                } else {
                    int forward = (g_weapon_chara_target - WepMenu.chara + 6) % 6;
                    MenuPointerPress(forward <= 3 ? kInputR1 : kInputL1);
                    return true;
                }
            }
            if (!take.pointing) {
                return true;
            }
            if (take.wheel != 0.0f) {
                MenuPointerPress(take.wheel < 0.0f ? kInputRight : kInputLeft);
                return true;
            }
            if (clicked) {
                if (y < 120.0f && BtlMenuStatusPt->party_size > 1) {
                    // The row of faces: the one clicked comes to the front.
                    for (int i = 0; i < 6; ++i) {
                        int left = static_cast<int>(SysChara[i].x);
                        if (SysChara[i].chara != WepMenu.chara && x >= left && x < left + 100 && y >= 14.0f) {
                            g_weapon_chara_target = SysChara[i].chara;
                        }
                    }
                } else if (y >= 130.0f && y < 280.0f) {
                    if (x >= 228.0f && x < 408.0f) {
                        MenuPointerPress(kInputCross);
                    } else if (x < 228.0f) {
                        MenuPointerPress(kInputLeft);
                    } else {
                        MenuPointerPress(kInputRight);
                    }
                }
            } else if ((take.clicked & 2) != 0) {
                MenuPointerPress(kInputCircle);
            }
            return true;
        }
        case WEP_MENU_ACTION: {
            if (!take.pointing) {
                return true;
            }
            int cursors[7];
            int count = WeaponActionRows(cursors);
            int hit = -1;
            for (int k = 0; k < count; ++k) {
                Rect row = {114, 115 + 26 * k, 210, 115 + 26 * k + 26};
                if (row.Contains(x, y)) {
                    hit = cursors[k];
                }
            }
            if (hit >= 0 && hit != WepMenu.board.cursor && (take.moved || clicked)) {
                WepMenu.board.cursor = hit;
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }
            if (take.wheel != 0.0f) {
                MenuPointerPress(take.wheel < 0.0f ? kInputDown : kInputUp);
            } else if (clicked) {
                if (hit >= 0) {
                    MenuPointerPress(kInputCross);
                }
            } else if ((take.clicked & 2) != 0) {
                MenuPointerPress(kInputCircle);
            }
            return true;
        }
        case WEP_MENU_ELEMENT: {
            if (!take.pointing) {
                return true;
            }
            int hit = -1;
            for (int e = 0; e < 5; ++e) {
                Rect row = {392, 128 + 24 * e, 588, 152 + 24 * e};
                if (row.Contains(x, y)) {
                    hit = e;
                }
            }
            if (hit >= 0 && hit != WepMenu.element && (take.moved || clicked)) {
                WepMenu.element = static_cast<char>(hit);
                ComMenuSePlay(MENU_SOUND_CURSOR);
            }
            if (clicked) {
                if (hit >= 0) {
                    MenuPointerPress(kInputCross);
                }
            } else if ((take.clicked & 2) != 0) {
                MenuPointerPress(kInputCircle);
            }
            return true;
        }
        case WEP_MENU_LEVELUP_CONFIRM:
        case WEP_MENU_BUILDUP_CONFIRM:
            YesNoMouse(take, 0x88, 0xDD, &WepMenu.board.cursor);
            return true;
        case WEP_MENU_STATUS_BREAK_CONFIRM:
            YesNoMouse(take, 0x88, 0xF3, &WepMenu.board.cursor);
            return true;
        case WEP_MENU_ATTACH_WEAPON:
        case WEP_MENU_ATTACH_SOCKETS:
        case WEP_MENU_ATTACH_TAGS:
        case WEP_MENU_ATTACH_BOARD: {
            // The attachment screen: the weapon's sockets on the left, the board of attachments on the right.
            if (!take.pointing) {
                return true;
            }
            PERSONAL_BOARD &board = WepMenu.board;
            int             holes = GetWeaponHoleNum(DngWepHavePt[WepMenu.weapon_slot].item_no);
            int             mode = -1;
            int             cursor = board.cursor;
            int             area = board.cursor_area;
            if (kItemBoard.Contains(x, y)) {
                int column = static_cast<int>((x - kItemBoard.left) / 40);
                int row = static_cast<int>((y - kItemBoard.top) / 40);
                int index = (board.top_row + row) * 5 + column;
                if (index < PersonalRetMax(BOARD_PAGE_ATTACH)) {
                    mode = WEP_MENU_ATTACH_BOARD;
                    area = PERSONAL_BOARD_AREA_CELLS;
                    cursor = index;
                }
            } else if (kItemTrash.Contains(x, y)) {
                mode = WEP_MENU_ATTACH_BOARD;
                area = PERSONAL_BOARD_AREA_TRASH;
            } else if (WepMenu.sockets_enabled != 0) {
                for (int i = 0; i < holes; ++i) {
                    int left = 0x90 - holes * 0x12 + i * 0x2A + 0xC;
                    if (Rect{left + 6, 184, left + 46, 226}.Contains(x, y)) {
                        mode = WEP_MENU_ATTACH_SOCKETS;
                        cursor = i;
                    }
                }
            }
            if (BoardScrollBar(take, board, PersonalRetMax(BOARD_PAGE_ATTACH), WepMenu.mode == WEP_MENU_ATTACH_BOARD)) {
                return true;
            }
            if (take.wheel != 0.0f) {
                if (kItemBoard.Contains(x, y)) {
                    BoardWheel(take, board, PersonalRetMax(BOARD_PAGE_ATTACH), WepMenu.mode == WEP_MENU_ATTACH_BOARD);
                }
                return true;
            }
            // The tag board under the weapon: its three tabs, and the rows of the one showing.
            int tag_tab = -1;
            for (int t = 0; t < 3; ++t) {
                if (Rect{80 + 56 * t, 240, 80 + 56 * t + 52, 272}.Contains(x, y)) {
                    tag_tab = t;
                }
            }
            int tag_row = -1;
            for (int r = 0; r < 10; ++r) {
                if (WepTagRow(WepMenu.tag_page, r).Contains(x, y)) {
                    tag_row = r;
                }
            }
            if (tag_row >= 0 && (take.moved || clicked)) {
                if (WepMenu.mode != WEP_MENU_ATTACH_TAGS || WepMenu.tag_row != tag_row) {
                    WepMenu.mode = WEP_MENU_ATTACH_TAGS;
                    WepMenu.tag_row = static_cast<char>(tag_row);
                    ComMenuSePlay(MENU_SOUND_CURSOR);
                }
            }
            if (clicked && tag_tab >= 0) {
                int steps = (tag_tab - WepMenu.tag_page + 3) % 3;
                if (steps == 1) {
                    MenuPointerPress(kInputR1);
                } else if (steps == 2) {
                    MenuPointerPress(kInputL1);
                }
                return true;
            }
            if (mode >= 0 && (take.moved || clicked)) {
                bool moved = mode != WepMenu.mode || cursor != board.cursor ||
                             (mode == WEP_MENU_ATTACH_BOARD && area != board.cursor_area);
                if (moved) {
                    WepMenu.mode = static_cast<short>(mode);
                    board.cursor = cursor;
                    board.cursor_area = area;
                    ComMenuSePlay(MENU_SOUND_CURSOR);
                }
            }
            if (clicked) {
                if (mode >= 0) {
                    MenuPointerPress(kInputCross);
                }
            } else if ((take.clicked & 2) != 0) {
                MenuPointerPress(kInputCircle);
            }
            return true;
        }
        default:
            return false;
    }
}

void BattleMouse() {
    if (!g_battle.IsOpen()) {
        g_battle.Open();
    }
    MenuPointerShow(&g_battle);
    MenuPointerTake take = g_battle.Take(SysCur[0], SysCur[1]);

    switch (BattleMenuFlag) {
        case BTLMENU_STATE_MAIN:
            RingMouse(take);
            break;
        case BTLMENU_STATE_WEAPON:
            if (!WeaponMouse(take)) {
                MenuPointerShow(nullptr);
                g_battle.Close();
            }
            break;
        case BTLMENU_STATE_ITEM:
            ItemMouse(take);
            break;
        case BTLMENU_STATE_ATLA:
            AtlaMouse(take);
            break;
        case BTLMENU_STATE_ATLA_OPEN:
            // The Georama page never leaves its opening state; it runs once the page has slid in.
            if (BtlEffectFlag == BTLEFFECT_NONE) {
                AtlaMouse(take);
            }
            break;
        case BTLMENU_STATE_CHARA:
            AlliesMouse(take);
            break;
        case BTLMENU_STATE_MOVE:
            MoveMouse(take);
            break;
        default:
            if (BattleMenuFlag >= BTLMENU_STATE_ITEM_OPEN && BattleMenuFlag <= BTLMENU_STATE_MANUAL_CLOSE &&
                BattleMenuFlag != BTLMENU_STATE_ATLA_OPEN) {
                // A page sliding in or out: the pointer stays where it is, and clicks wait for the page.
                break;
            }
            // A page without a handler: the mouse goes back to the pad.
            MenuPointerShow(nullptr);
            g_battle.Close();
            break;
    }
}

} // namespace

void MenuMouseNoteBattleMenu() { g_battle_drawn = true; }

void MenuMouseUpdate() {
    MenuPointerClearPresses();
    MenuPointerShow(nullptr);
    bool battle = g_battle_drawn;
    g_battle_drawn = false;

    bool save = SaveMenu.texture_ready != 0 && SaveMenu.key_no != SAVE_KEY_FADE_OUT;
    if (save) {
        g_battle.Abandon();
        if (MenuOptionOpen() || NameMouseOpen()) {
            g_save.Abandon();
            g_save_open = false;
        }
        SaveMouse();
        return;
    }
    if (g_save_open) {
        g_save.Close();
        g_save_open = false;
    }

    if (MenuOptionOpen()) {
        // The Options screen takes the mouse for itself (options/screen.cpp).
        g_battle.Abandon();
        return;
    }
    if (NameMouseOpen()) {
        g_battle.Close();
        NameMouseUpdate();
        return;
    }
    NameMouseRelease();

    if (battle && BattleMenuFlag != BTLMENU_STATE_EXIT && BattleMenuFlag != BTLMENU_STATE_APPEAR) {
        BattleMouse();
    } else {
        g_battle.Close();
    }
}

void MenuMouseDrawOverlay() {
    if (!g_discard.active) {
        return;
    }
    static GameText question;
    static bool     laid_out = false;
    if (!laid_out) {
        question.Set("Throw this item away?");
        laid_out = true;
    }
    MenuTextureReload(BtlMenuReadBlock);
    MenuHelpWinDraw(0xA0, 0x94, 11.0f, 0.8f, 0x80);
    MenuTextureReload(CommonMenuMes2.tex_block);
    question.Draw(0xBE, 0xB0, 0x80);
    MenuTextureReload(BtlMenuReadBlock);
    CRect_i_ dst(kDiscardPlateX, kDiscardPlateY, 0x60, 0x20);
    CRect_i_ src(0, 0, 0x60, 0x20);
    for (int row = 0; row < 2; ++row) {
        DrawMenu2DSprite(BtStatus, dst, src, 0x80);
        dst.y += 0x1C;
        src.y += 0x20;
    }
    DrawMenuWaku(static_cast<float>(kDiscardPlateX - 4), static_cast<float>(kDiscardPlateY - 2 + g_discard.answer * 0x1C), 0x68,
                 0x26, 0, StayTex, 0x80);
    DrawMenuObjectVibe(static_cast<int>(SysCur[0]), static_cast<int>(SysCur[1]), 1, 0x40);
}
