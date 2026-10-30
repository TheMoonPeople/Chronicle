#include "dialog/script_question.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <utility>

#include "clsmes.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "gamepad.hpp"
#include "platform/input.hpp"
#include "text/gametext.hpp"
#include "ui/menu_pointer.hpp"

namespace dc::dialog {

using namespace dc::game;

namespace {

// The message the window shows in place of the script's where rows are added; the towns' files stop
// far below it (the catch's species name is 0x7F00).
constexpr int kQuestionMessage = 0x7F01;

// The script's pad, as exch_ok_cancel hands it over: up, down, and the confirm button.
constexpr int kScriptUp = 0x1000;
constexpr int kScriptDown = 0x4000;

struct Row {
    int                   answer;
    std::function<void()> then;
};

struct Question {
    ClsMes          *mes = nullptr;
    std::vector<Row> rows;
    int              cursor = 0;        // the row the window's hand is on
    int              script_row = 0;    // the row the script's own cursor is on
    int              stick = 0;         // the stick's last way, as the script's helper reads it
    bool             confirm = false;   // confirm pressed, for the script once its cursor is there
    bool             handing = false;   // a row's then has run; the script is told at its next read
    bool             answered = false;
};

std::vector<QuestionProvider> g_providers;
std::optional<Question>       g_question;
ui::MenuPointer               g_pointer;
text::GameTextFile            g_text;

// The town's folder name, from EditDataDir ("gedit/e01/").
std::string_view Town() {
    std::string_view dir = EditDataDir;
    while (!dir.empty() && dir.back() == '/') {
        dir.remove_suffix(1);
    }
    return dir.substr(dir.rfind('/') + 1);
}

// The message's rows, as GameTextDecode writes them.
std::vector<std::string> Lines(ClsMes &mes, int message) {
    std::vector<std::string> lines;
    const short             *codes = mes.buff != nullptr ? mes.GetTextLineDataTop(message) : nullptr;
    if (codes == nullptr) {
        return lines;
    }
    const std::string text = text::GameTextDecode(codes);
    for (std::size_t start = 0; start <= text.size();) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

// The row under the window's text at (x, y), as the window draws its rows: a char_height apart from
// text_y, reaching left to where the hand stands.
int RowAt(const ClsMes &mes, int rows, float x, float y) {
    const float left = static_cast<float>(mes.text_x - 0xC - mes.char_width * 2);
    const float right = static_cast<float>(mes.text_x + std::max(mes.text_width, mes.char_width * 8));
    if (x < left || x >= right) {
        return -1;
    }
    for (int row = 0; row < rows; row++) {
        const float top = static_cast<float>(mes.text_y + row * mes.char_height - 4);
        if (y >= top && y < top + static_cast<float>(mes.char_height)) {
            return row;
        }
    }
    return -1;
}

void Finish() {
    ClsMesSetChoiceRows(nullptr, 0);
    ClsMesSetMessageOverlay(nullptr, nullptr);
    g_question.reset();
}

} // namespace

void RegisterQuestionProvider(QuestionProvider provider) {
    g_providers.push_back(std::move(provider));
}

void ClearQuestionProviders() {
    g_providers.clear();
}

bool ScriptQuestionMake(ClsMes &mes, int window, int message) {
    if (g_question) {
        Finish();
    }
    const QuestionContext    context{.town = Town(), .interior = EdInteriorFlag != 0, .window = window, .message = message};
    std::vector<QuestionRow> added;
    const auto               taker = std::ranges::find_if(g_providers, [&](const QuestionProvider &provider) {
        return provider(context, added);
    });
    if (taker == g_providers.end()) {
        return false;
    }

    // A message with no rows the framework can read is left to the script, which can answer it.
    std::vector<std::string> lines = Lines(mes, message);
    if (lines.empty()) {
        return false;
    }

    Question question{.mes = &mes};
    for (int row = 0; row < static_cast<int>(lines.size()); row++) {
        question.rows.push_back({.answer = row, .then = {}});
    }
    if (added.empty()) {
        // Retail's own message, as the script would have made it.
        mes.MakeMesWin(message);
    } else {
        // Retail's rows with the providers' among them, laid over the window's own messages.
        for (QuestionRow &row : added) {
            const int at = row.before < 0 ? static_cast<int>(lines.size()) : std::min(row.before, static_cast<int>(lines.size()));
            lines.insert(lines.begin() + at, row.label);
            question.rows.insert(question.rows.begin() + at, {.answer = row.answer, .then = std::move(row.then)});
        }
        std::string text;
        for (const std::string &line : lines) {
            text += text.empty() ? line : "\n" + line;
        }
        if (g_text.Set(kQuestionMessage, text) < 0) {
            return false;
        }
        ClsMesSetMessageOverlay(&mes, g_text.Data());
        mes.MakeMesWin(kQuestionMessage);
    }
    ClsMesSetChoiceRows(&mes, 0);
    mes.cursor_row = 0;
    g_question = std::move(question);
    return true;
}

bool ScriptQuestionPad(int read, int &pressed) {
    if (!g_question) {
        return false;
    }
    Question &question = *g_question;
    pressed = 0;
    if (question.answered) {
        return true;
    }
    // The script reads again once a menu the chosen row opened has closed.
    if (question.handing) {
        question.answered = true;
        pressed = ED_PAD_CONFIRM;
        return true;
    }

    // The player chooses as the script's helper would have them: the stick turned from the middle, then
    // the pad's Up and Down, a row at a time and not past the ends; confirm takes the row.
    const float stick_y = GamePad.GetLYf();
    const int   stick = stick_y < -0.5f ? -1 : (stick_y > 0.5f ? 1 : 0);
    int         step = question.stick == 0 ? stick : 0;
    question.stick = stick;
    if ((read & kScriptUp) != 0) {
        step = -1;
    }
    if ((read & kScriptDown) != 0) {
        step = 1;
    }
    // Once confirm is pressed, the row stays the one confirmed while the script's cursor catches up.
    if (question.confirm) {
        step = 0;
    }
    const int last = static_cast<int>(question.rows.size()) - 1;
    question.cursor = std::clamp(question.cursor + step, 0, std::max(last, 0));
    question.mes->cursor_row = question.cursor;
    question.confirm = question.confirm || (read & ED_PAD_CONFIRM) != 0;

    // The script's cursor follows on the same tick, to the row the hand's answers with: retail's rows
    // move it as retail does, a row added beside the one it answers as leaves it.
    const int answer = last >= 0 ? question.rows[static_cast<std::size_t>(question.cursor)].answer : 0;
    if (answer > question.script_row) {
        question.script_row++;
        pressed = kScriptDown;
    } else if (answer < question.script_row) {
        question.script_row--;
        pressed = kScriptUp;
    } else if (question.confirm && last >= 0) {
        std::function<void()> then = std::move(question.rows[static_cast<std::size_t>(question.cursor)].then);
        if (then) {
            // The window goes as the script's _MES_CLOSE takes it, and the row's own business runs
            // while the script waits.
            ClsMes &mes = *question.mes;
            mes.cursor_row = -1;
            mes.text_rate = mes.text_rate_set;
            mes.mes_made = -1;
            mes.fade_in = false;
            question.handing = true;
            then();
        } else {
            question.answered = true;
            pressed = ED_PAD_CONFIRM;
        }
    }
    return true;
}

bool ScriptQuestionActive() {
    return g_question.has_value();
}

bool ScriptQuestionHolds(const ClsMes *mes) {
    return g_question && g_question->mes == mes;
}

void ScriptQuestionClosed(const ClsMes *mes) {
    if (g_question && g_question->mes == mes) {
        Finish();
    }
}

void ScriptQuestionReset() {
    if (g_question) {
        Finish();
    }
}

bool ScriptQuestionPointerUpdate() {
    if (!g_question || g_question->answered || g_question->handing) {
        return !g_pointer.Close();
    }
    Question &question = *g_question;
    if (!g_pointer.IsOpen()) {
        g_pointer.Open();
    }
    const ClsMes             &mes = *question.mes;
    const float               hand_x = static_cast<float>(mes.text_x - 0xC - mes.char_width * 2);
    const float               hand_y = static_cast<float>(mes.text_y + mes.cursor_y - 4);
    const ui::MenuPointerTake take = g_pointer.Take(hand_x, hand_y);
    ui::MenuPointerShow(&g_pointer);
    if (!take.pointing) {
        return true;
    }
    const int row = RowAt(mes, static_cast<int>(question.rows.size()), g_pointer.x, g_pointer.y);
    if (row >= 0 && take.moved && !question.confirm) {
        question.cursor = row;
        question.mes->cursor_row = row;
    }
    int pressed = 0;
    if ((take.clicked & 1) != 0 && row >= 0) {
        pressed |= kInputCross;
    }
    if (take.wheel > 0.0f) {
        pressed |= kInputUp;
    } else if (take.wheel < 0.0f) {
        pressed |= kInputDown;
    }
    ui::MenuPointerPress(pressed);
    return true;
}

} // namespace dc::dialog
