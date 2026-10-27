#include "ui/name_mouse.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "battle_globals.hpp"
#include "gamepad.hpp"
#include "menu_draw.hpp"
#include "platform/input.hpp"
#include "rect.hpp"
#include "ui/menu_pointer.hpp"
#include "ui/name_mouse_layout.hpp"

namespace dc::ui {

/**
 * The Register Name screen's state (NAME_SELECT), laid out as the game's global NameSelect.
 */
struct NameSelectView {
    short chara_no;
    short area;
    short name_pos;
    short side_row;
    short pushed_tab;
    short input_mode;
    int   cursor;
    short state;
    int   state_count;
    float cursor_x;
    float cursor_y;
    int   frame;
    short language;
    short texture_block;
    short loaded;
};

static_assert(sizeof(NameSelectView) == 0x2C);
static_assert(offsetof(NameSelectView, cursor) == 12 && offsetof(NameSelectView, state) == 16);
static_assert(offsetof(NameSelectView, cursor_x) == 24 && offsetof(NameSelectView, language) == 36);

} // namespace dc::ui

namespace dc::game {
extern ui::NameSelectView NameSelect;
} // namespace dc::game

namespace dc::ui {

using namespace dc::game;

namespace {

using namespace name_mouse;

// NameSelect.state values.
constexpr int kStateIdle = 0;
constexpr int kStateExit = 3;
constexpr int kStateTabFlash = 6;
constexpr int kStateHelp = 8;

// menu_euro_codetbl and euro_code_linelimmit are indexed from British, which has no accented keys.
constexpr int kLanguageBritish = 2;

constexpr std::uint32_t kMouseLeft = 1;
constexpr std::uint32_t kMouseRight = 2;

struct State {
    MenuPointer pointer;
    Hit         hovered; /**< What the pointer was last over, with the screen it was over it on. */
    int         hovered_mode = -1;
    int         hovered_language = -1;
};

State g_state;

Screen CurrentScreen() {
    Screen screen;
    screen.language = NameSelect.language;
    screen.input_mode = NameSelect.input_mode;
    screen.symbol_cells = menu_kigoutbl;
    int euro_index = NameSelect.language - kLanguageBritish;
    if (euro_index > 0 && euro_index < std::ssize(euro_code_linelimmit)) {
        screen.euro_codes = &menu_euro_codetbl[euro_index][0][0];
        screen.euro_rows = euro_code_linelimmit[euro_index];
    }
    return screen;
}

// Puts the game's cursor on a key or tab, and tells whether that moved it.
bool PutCursorOn(const Hit &hit) {
    int area = hit.kind == Kind::Tab ? kAreaTabs : kAreaKeyboard;
    if (NameSelect.area == area && NameSelect.cursor == hit.index && NameSelect.side_row == 0) {
        return false;
    }
    NameSelect.area = static_cast<short>(area);
    NameSelect.cursor = hit.index;
    NameSelect.side_row = 0;
    return true;
}

} // namespace

bool NameMouseOpen() { return NameSelect.loaded != 0 && NameSelect.state != kStateExit; }

void NameMouseRelease() { g_state.pointer.Close(); }

void NameMouseUpdate() {
    MenuPointer &pointer = g_state.pointer;

    if (!pointer.IsOpen()) {
        // What is held as the screen opens (the click that ended the book) acts only once let go.
        pointer.Open();
    }
    MenuPointerShow(&pointer);

    MenuPointerTake take = pointer.Take(NameSelect.cursor_x, NameSelect.cursor_y);

    // Only while the screen takes input: not fading in or out.
    bool taking = (NameSelect.state == kStateIdle || NameSelect.state == kStateTabFlash || NameSelect.state == kStateHelp);
    if (!taking) {
        return;
    }
    if (!take.pointing) {
        g_state.hovered = Hit{};
        g_state.hovered_mode = -1;
        return;
    }

    float x = pointer.x;
    float y = pointer.y;
    int   area = NameSelect.area;
    if (area >= kAreaHelp) {
        // A message is up. The confirm dialog's Yes and No are its Cross and Circle; the others close
        // on a click anywhere in their window.
        bool inside = DialogRect(area).Contains(x, y);
        int  pressed = 0;
        if (area == kAreaConfirm) {
            int choice = ConfirmChoice(x, y);
            if ((take.clicked & kMouseLeft) != 0 && choice != 0) {
                pressed = choice > 0 ? kInputCross : kInputCircle;
            }
        } else if ((take.clicked & kMouseLeft) != 0 && inside) {
            pressed = kInputCross;
        }
        if (pressed == 0 && (take.clicked & kMouseRight) != 0) {
            pressed = kInputCircle;
        }
        MenuPointerPress(pressed);
        return;
    }

    Screen screen = CurrentScreen();
    Hit    hit = HitTest(screen, x, y);

    if (take.wheel != 0.0f) {
        // Over the name the wheel moves the text cursor, elsewhere it turns the keyboard tabs. Away
        // from the user goes left or back, as the pad's L1 and L2 do.
        bool forward = take.wheel < 0.0f;
        if (hit.kind == Kind::Slot) {
            MenuPointerPress(forward ? kInputR1 : kInputL1);
        } else {
            MenuPointerPress(forward ? kInputR2 : kInputL2);
        }
        return;
    }

    bool changed = hit != g_state.hovered || screen.input_mode != g_state.hovered_mode ||
                   screen.language != g_state.hovered_language;
    g_state.hovered = hit;
    g_state.hovered_mode = screen.input_mode;
    g_state.hovered_language = screen.language;

    if ((hit.kind == Kind::Key || hit.kind == Kind::Tab) && (changed || (take.clicked & kMouseLeft) != 0) &&
        PutCursorOn(hit)) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    if ((take.clicked & kMouseLeft) != 0) {
        if (hit.kind == Kind::Key || hit.kind == Kind::Tab) {
            MenuPointerPress(kInputCross);
        } else if (hit.kind == Kind::Slot && NameSelect.name_pos != hit.index) {
            NameSelect.name_pos = static_cast<short>(hit.index);
            ComMenuSePlay(MENU_SOUND_CONFIRM);
        }
    } else if ((take.clicked & kMouseRight) != 0) {
        MenuPointerPress(kInputCircle);
    }
}

bool NameMouseHand(CTexture *texture, CRect_i_ &screen, const CRect_i_ &texel, bool shadow) {
    float x = 0.0f;
    float y = 0.0f;
    // DrawNameTemplete's hand: 32 by 32 at (0x1C0, 0x128) of nametemp.
    if (!g_state.pointer.IsOpen() || texture != NameTemp || NameTemp == nullptr || texel.x != 0x1C0 ||
        texel.y != 0x128 || texel.width != 32 || texel.height != 32 || !MenuPointerHand(x, y)) {
        return false;
    }
    screen.x = static_cast<int>(x) + (shadow ? 2 : 0);
    screen.y = static_cast<int>(y) + (shadow ? 2 : 0);
    return true;
}

} // namespace dc::ui
