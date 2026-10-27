#include "dialog/npc.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "clsmes.hpp"
#include "dialog/framework.hpp"
#include "editloop.hpp"
#include "gamepad.hpp"
#include "localize/localize.hpp"
#include "menu_draw.hpp"
#include "platform/input.hpp"
#include "text/gametext.hpp"
#include "ui/menu_pointer.hpp"

namespace dc::dialog {

using namespace dc::game;
using namespace dc::localize;
using namespace dc::text;
using namespace dc::ui;

namespace {

/**
 * The framework's pages are drawn by the conversation's own window, EditMes1, as retail draws a
 * villager's choice prompt: the speech bubble, the hand, and the rows the hand is not on dimmed.
 * The page's text sits above its choices and keeps its colour (ClsMesSetChoiceRows).
 */
struct Window {
    short       *retail_buff = nullptr; /**< EditMes1's talk messages, put back when the session ends. */
    std::string  shown;                 /**< The text EditMes1 was last made with. */
    GameTextFile file;
    int          first_row = 0;   /**< The row the choices start on. */
    int          choice_rows = 0; /**< How many choices are in view; 0 on a page of text. */
    MenuPointer  pointer;         /**< The mouse, while the window is up. */
};

Window g_window;

// LocalizeMessageKey of the file EditMes1's villager messages came from.
std::string g_message_file_key;

// The talk window grows to its widest line and never wraps one, so the framework wraps at about
// the width retail's own villager lines run to, and keeps the rows within the window's ten.
void Setup() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    LayoutConfig cfg = GetLayoutConfig();
    cfg.max_inner_width = 380.0f;
    cfg.max_visible_choices = 6;
    SetLayoutConfig(cfg);

    // DC_DIALOG_TEST=1: every villager offers one more choice, to see the framework in the game
    // before anything registers a provider of its own.
    if (std::getenv("DC_DIALOG_TEST") == nullptr) {
        return;
    }
    RegisterOptionProvider([](const NpcContext &, std::vector<ChoiceItem> &out) {
        ChoiceItem item;
        item.id = "test.who";
        item.fallback_label = "Who are you?";
        item.color = ChoiceColor::Cyan;
        item.on_select = [](ISessionController &session) {
            const NpcContext &ctx = session.GetContext();
            session.PushRawMessage("Map %1, villager %2, message %3\nof %4.{page}Press {cross} to go back.",
                                   {std::to_string(ctx.map_no), std::to_string(ctx.npc_id),
                                    std::to_string(ctx.mes_id), ctx.mes_file_key});
        };
        out.push_back(std::move(item));
    });
}

// The colour tag a choice's label is wrapped in, or none for the window's own colour.
std::string_view ColourTag(const ChoiceItem &item) {
    if (!item.enabled) {
        return "grey";
    }
    switch (item.color) {
        case ChoiceColor::White:
            return {};
        case ChoiceColor::Yellow:
            return "yellow";
        case ChoiceColor::Cyan:
            return "cyan";
        case ChoiceColor::Green:
            return "green";
        case ChoiceColor::Gold:
            return "gold";
        case ChoiceColor::Grey:
            return "grey";
        case ChoiceColor::Highlight:
            return "highlight";
    }
    return {};
}

// The view as one message: the page's lines, then the choices in view. first_row is the row the
// choices start on, cursor_row the one the hand stands on (-1 for a page without choices).
std::string WindowText(const ActiveView &view, int &first_row, int &cursor_row) {
    std::string text;
    int         rows = 0;
    auto        add = [&](std::string_view line) {
        if (rows++ > 0) {
            text += '\n';
        }
        text += line;
    };

    const std::vector<std::string> &lines = view.current_slice.wrapped_markup_lines;
    const bool                      blank = std::ranges::all_of(lines, [](const std::string &line) { return line.empty(); });
    // A page of only choices is drawn as retail's are, with no row above them.
    if (!blank || !view.show_choices_on_current_slice) {
        for (const std::string &line : lines) {
            add(line);
        }
    }

    first_row = rows;
    cursor_row = -1;
    if (!view.show_choices_on_current_slice) {
        return text;
    }
    const int total = static_cast<int>(view.choices.size());
    const int end = std::min(total, view.choice_scroll_offset + view.layout.visible_choice_count);
    for (int i = view.choice_scroll_offset; i < end; ++i) {
        const std::string_view tag = ColourTag(view.choices[static_cast<size_t>(i)]);
        const std::string     &label = view.formatted_choice_labels[static_cast<size_t>(i)];
        add(tag.empty() ? label : "{" + std::string(tag) + "}" + label + "{/color}");
    }
    cursor_row = first_row + view.selected_choice - view.choice_scroll_offset;
    return text;
}

// Makes EditMes1 show the open session, rebuilding its window only when the text changes.
void ShowWindow() {
    const ActiveView *view = GetActiveView();
    if (view == nullptr) {
        return;
    }
    int               first_row = 0;
    int               cursor_row = -1;
    const std::string text = WindowText(*view, first_row, cursor_row);

    if (text != g_window.shown) {
        g_window.shown = text;
        g_window.file.Set(0, text);
        EditMes1.SetBuff(g_window.file.Data());
        // Choices show whole, as retail's prompt does; a page of text types out.
        EditMes1.text_rate = cursor_row >= 0 ? 0.0f : EditMes1.text_rate_set;
        EditMes1.mes_made = -1;
        EditMes1.MakeMesWin(0);
    }
    EditMes1.cursor_row = cursor_row;
    g_window.first_row = first_row;
    const int choice_count = static_cast<int>(view->choices.size());
    const int visible_start = std::clamp(view->choice_scroll_offset, 0, choice_count);
    g_window.choice_rows = cursor_row >= 0
                               ? std::min(choice_count - visible_start, std::max(0, view->layout.visible_choice_count))
                               : 0;
    ClsMesSetChoiceRows(cursor_row >= 0 ? &EditMes1 : nullptr, first_row);
    ClsMesSetChoiceScroll(cursor_row >= 0 && view->choice_scroll_offset > 0,
                          cursor_row >= 0 && view->choice_scroll_offset + g_window.choice_rows < choice_count);
}

// Gives EditMes1 back the villager's messages, for retail's conversation or the next one.
void HideWindow() {
    if (g_window.retail_buff != nullptr) {
        EditMes1.SetBuff(g_window.retail_buff);
        g_window.retail_buff = nullptr;
    }
    g_window.shown.clear();
    g_window.choice_rows = 0;
    EditMes1.cursor_row = -1;
    ClsMesSetChoiceRows(nullptr, 0);
}

// Where DrawMesWin puts the text's left edge: a narrow bubble centres it.
int TextLeft() {
    int left = EditMes1.text_x;
    if (EditMes1.fukidashi != 0 && EditMes1.text_columns < 4) {
        left += (4 - EditMes1.text_columns) * EditMes1.char_width >> 1;
    }
    return left;
}

// The choice in view the point (x, y) is on, counting from the first in view, or -1.
int ChoiceAt(float x, float y) {
    if (g_window.choice_rows <= 0 || EditMes1.char_height <= 0) {
        return -1;
    }
    // From the hand's column to a character past the widest line; a row's characters sit 3 above it.
    const auto left = static_cast<float>(TextLeft() - 0xC - EditMes1.char_width * 2);
    const auto right = static_cast<float>(TextLeft() + EditMes1.text_width + EditMes1.char_width);
    const auto top = static_cast<float>(EditMes1.text_y - 3);
    if (x < left || x >= right || y < top) {
        return -1;
    }
    const int choice = static_cast<int>((y - top) / static_cast<float>(EditMes1.char_height)) - g_window.first_row;
    return choice >= 0 && choice < g_window.choice_rows ? choice : -1;
}

} // namespace

void DialogNpcSetMessageFile(const char *path) {
    g_message_file_key = path != nullptr && path[0] != '\0' ? LocalizeMessageKey(path) : std::string();
}

bool DialogNpcOpen(int map_no, int character_info_id, int mes_no) {
    Setup();
    NpcContext ctx;
    ctx.map_no = map_no;
    ctx.npc_id = character_info_id;
    ctx.mes_id = mes_no;
    ctx.mes_file_key = g_message_file_key;
    ctx.bubble = BubbleKind::SpeechTail;
    ctx.host_plays_original = true;
    if (!TryInterceptOpen(ctx, {})) {
        return false;
    }
    g_window.retail_buff = EditMes1.buff;
    g_window.shown.clear();
    return true;
}

DialogNpcState DialogNpcStep() {
    StepSession();

    // As EdTalkMode's choice prompt reads the pad: Up and Down move, Cross takes (or, while the
    // text is still typing out, shows it whole), Circle backs out.
    if (GamePad.Down(PAD_UP) != 0 && InputMoveUp()) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if (GamePad.Down(PAD_DOWN) != 0 && InputMoveDown()) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if (GamePad.Down(PAD_CROSS) != 0) {
        if (EditMes1.State() != CLSMES_SHOWN) {
            EditMes1.text_rate = 0.0f;
        } else if (InputConfirm()) {
            ComMenuSePlay(MENU_SOUND_CONFIRM);
        }
    } else if (GamePad.Down(PAD_CIRCLE) != 0) {
        InputCancel();
    }

    if (IsSessionActive()) {
        return DialogNpcState::Running;
    }
    HideWindow();
    return TakeSessionEnd() == SessionEnd::PlayOriginal ? DialogNpcState::PlayOriginal : DialogNpcState::Closed;
}

bool DialogNpcSpeaking() {
    const ActiveView *view = GetActiveView();
    if (view == nullptr) {
        return false;
    }
    return !std::ranges::all_of(view->current_slice.wrapped_markup_lines, [](const std::string &line) { return line.empty(); });
}

void DialogNpcShowWindow() {
    ShowWindow();
}

std::string DialogNpcTalkLine(int message) {
    const short *file = g_window.retail_buff != nullptr ? g_window.retail_buff : EditMes1.buff;
    if (file == nullptr) {
        return {};
    }
    // As ClsMes::GetTextLineDataTop reads a message file: the count, each message's number and
    // where its text starts, then the texts.
    const int    count = file[0];
    const short *body = &file[1];
    for (int i = 0; i < count; i++) {
        if (body[i * 2 + 1] == message) {
            return GameTextDecode(&body[count + body[i * 2 + 2]]);
        }
    }
    return {};
}

bool DialogNpcMouseOpen() {
    return g_window.retail_buff != nullptr && IsSessionActive();
}

void DialogNpcMouseRelease() {
    g_window.pointer.Close();
}

void DialogNpcMouseUpdate() {
    MenuPointer &pointer = g_window.pointer;
    if (!pointer.IsOpen()) {
        // What is held as the window opens (the click that started the talk) acts only once let go.
        pointer.Open();
    }
    MenuPointerShow(&pointer);

    // The pointer takes over from the pad where the window's hand is.
    const auto            hand_x = static_cast<float>(TextLeft() - 0xC - EditMes1.char_width * 2);
    const auto            hand_y = static_cast<float>(EditMes1.text_y + EditMes1.cursor_y - 4);
    const MenuPointerTake take = pointer.Take(hand_x, hand_y);
    if (!take.pointing) {
        return;
    }

    const int choice = ChoiceAt(pointer.x, pointer.y);
    if (choice >= 0 && take.moved && InputHoverChoice(choice)) {
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }

    // A click is the pad's press, so DialogNpcStep does the rest: the left button takes the choice
    // it is on (or, on a page of text, turns it, wherever it is), the right one backs out, and the
    // wheel moves the hand.
    int pressed = 0;
    if ((take.clicked & 1) != 0 && (choice >= 0 || g_window.choice_rows == 0)) {
        pressed |= kInputCross;
    } else if ((take.clicked & 2) != 0) {
        pressed |= kInputCircle;
    }
    if (take.wheel > 0.0f) {
        pressed |= kInputUp;
    } else if (take.wheel < 0.0f) {
        pressed |= kInputDown;
    }
    MenuPointerPress(pressed);
}

} // namespace dc::dialog
