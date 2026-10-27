#pragma once

#include <string>

// Villager conversations through the dialog framework (docs/DIALOG_FRAMEWORK.md). EdTalkMode offers
// each conversation to the framework on its first tick; one the framework takes runs here until it
// closes, or until "Talk" hands it back to the retail message window.

namespace dc::dialog {

/**
 * Where a framework conversation stands after a tick of DialogNpcStep.
 */
enum class DialogNpcState {
    Running,      /**< The framework's window is up. */
    Closed,       /**< The player left: the conversation is over. */
    PlayOriginal, /**< "Talk": retail shows the villager's own message from here. */
};

/**
 * Records that the town's villager messages were read from path (an empty one: from no message
 * file of its own), for NpcContext::mes_file_key: LocalizeMessageKey's key, the same in every language.
 */
void DialogNpcSetMessageFile(const char *path);

/**
 * Offers the conversation with villager character_info_id of map map_no, opening on message mes_no:
 * true when the framework took it, which it does only where a provider adds a choice.
 */
bool DialogNpcOpen(int map_no, int character_info_id, int mes_no);

/**
 * One tick of a conversation DialogNpcOpen took: the pad, then where it stands. While it runs,
 * EdTalkMode places the window (DialogNpcSpeaking) and then makes it (DialogNpcShowWindow).
 */
DialogNpcState DialogNpcStep();

/**
 * Whether the page up is the NPC's words (text, with or without choices below it) rather than only
 * the player's choices: the window goes over the NPC's head for the one and the player's for the
 * other, as retail places a villager's message and its own choice prompts.
 */
bool DialogNpcSpeaking();

/**
 *
 * Makes EditMes1 show the page up, where EdTalkMode placed it.
 *
 */
void DialogNpcShowWindow();

/**
 * Line message of the town's villager message file (the one EditMes1 reads the villagers' own
 * conversations from, even while the framework's window has it), as GameTextDecode writes it;
 * empty where it has none.
 */
std::string DialogNpcTalkLine(int message);

/**
 * Whether the mouse drives the framework's window, for MenuMouseUpdate (menu_mouse.cpp) ahead of
 * the game's read of the pad.
 */
bool DialogNpcMouseOpen();

/**
 * Hovering a choice puts the hand on it; a click presses the pad button DialogNpcStep reads
 * (left: Cross, right: Circle, the wheel: Up and Down).
 */
void DialogNpcMouseUpdate();

/**
 *
 * The window is not up: the mouse goes back to the pad's buttons once none of it is held.
 *
 */
void DialogNpcMouseRelease();

} // namespace dc::dialog
