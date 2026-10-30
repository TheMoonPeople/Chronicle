#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dc::game {
class ClsMes;
}

// A town script's question, asked by the dialog framework (docs/DIALOG_FRAMEWORK.md, "A town script's
// question"). The script makes its message in one of its windows and reads the pad itself for the
// answer; a provider that takes the question has the framework ask it instead, in the same window with
// the same text, rows of its own added if it likes, and the script is handed the row it knows.
namespace dc::dialog {

/**
 * A row a provider adds to a question. Chosen, its window closes and then runs while the script
 * still waits, so a menu then opens (EdEventMenu) is up before the script goes on; once it closes, the
 * script is told answer, one of its own rows.
 */
struct QuestionRow {
    std::string           label;       /**< As the window shows it. */
    int                   before = -1; /**< The script's row it goes above; -1 for after the last. */
    int                   answer = 0;  /**< The script's row it answers with. */
    std::function<void()> then{};      /**< Runs as the row is chosen, before the script has its answer. */
};

/**
 * The question about to be asked.
 */
struct QuestionContext {
    std::string_view town;     /**< The folder the town's scripts load from: "e01", "s09". */
    bool             interior; /**< Asked inside a house, whose scripts and messages are its own. */
    int              window;   /**< The script's window. */
    int              message;  /**< Its message. */
};

/**
 * Takes the question (true), adding rows if it likes, or leaves it to the script (false).
 */
using QuestionProvider = std::function<bool(const QuestionContext &question, std::vector<QuestionRow> &rows)>;

/**
 * Adds a provider; the first that takes a question has it.
 */
void RegisterQuestionProvider(QuestionProvider provider);

/**
 * Forgets every provider.
 */
void ClearQuestionProviders();

// The town's script commands' side (editloop3.cpp).

/**
 * _MES_MAKE: offers message of window (mes) to the providers. True when one took it and the window is
 * made with its rows; false leaves the script to make it.
 */
bool ScriptQuestionMake(game::ClsMes &mes, int window, int message);

/**
 * _GET_PADDOWN: while a question is taken, a tick of it from the pad as the script reads it (read), and
 * what the script reads in its place: nothing while the player chooses, then Down to the script's row
 * and confirm. False otherwise.
 */
bool ScriptQuestionPad(int read, int &pressed);

/**
 * Whether a question is taken: its window's cursor is the framework's, and the script reads the stick
 * centred.
 */
bool ScriptQuestionActive();

/**
 * Whether mes is a taken question's window, whose cursor the script's _SET_MES_CURSOR leaves alone but
 * for hiding it.
 */
bool ScriptQuestionHolds(const game::ClsMes *mes);

/**
 * _MES_CLOSE: mes closed; a taken question in it ends.
 */
void ScriptQuestionClosed(const game::ClsMes *mes);

/**
 * An event began or ended, or a town started: a question left taken is forgotten, so that no script's
 * pad or stick stays the framework's.
 */
void ScriptQuestionReset();

/**
 * The mouse over a taken question, ahead of the game's read of the pad: a hover puts the cursor on a
 * row, the left button takes it, the wheel moves the cursor. True while a question has the mouse.
 */
bool ScriptQuestionPointerUpdate();

} // namespace dc::dialog
