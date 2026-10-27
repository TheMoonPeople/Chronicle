#pragma once

#include <filesystem>

// The quest framework in the game (docs/QUEST_FRAMEWORK.md): the save data as the quest::IGame the
// quests act on, their choices in villagers' conversations, their progress kept beside each save,
// and the shops they open.

namespace dc::quest {

/**
 * Registers the quests, the game and the conversation choices, the first time it is called.
 * Later calls do nothing.
 */
void QuestHostSetup();

/**
 * Starts a new game: every quest starts again.
 * Any shop a quest left pending is dropped.
 */
void QuestHostNewGame();

/**
 * A conversation with villager npc_id of map map_no starts (EdTalkModeInit), before the talk script
 * runs: retail behaviour a quest takes over is held back for it (quest::Retail) until the
 * conversation is up (QuestHostTalkOpened).
 */
void QuestHostTalkStarting(int map_no, int npc_id);

/**
 * The conversation is up (its first tick in EdTalkMode): retail behaviour held back by
 * QuestHostTalkStarting is released.
 */
void QuestHostTalkOpened();

/**
 * Whether the player has item in the pack or stored, as the talk script asks (_SITEM_CHECK_ALL).
 * While a quest holds the Mayor's supplies, his own count as had. Returns 1 or 0.
 */
int QuestHostPlayerAllItemCheck(int item);

/**
 * Writes the quests' progress into a save's folder (quests.json) as the save is written.
 * Returns false when the file could not be written.
 */
bool QuestHostSave(const std::filesystem::path &save_directory);

/**
 * Reads the quests' progress from a save's folder as the save is loaded.
 * A save without the file (made before there were quests) starts every quest again.
 */
void QuestHostLoad(const std::filesystem::path &save_directory);

/**
 * The save is being deleted: removes its quests.json first,
 * so its folder is left empty.
 */
void QuestHostDelete(const std::filesystem::path &save_directory);

/**
 * An item a quest showed the player receiving in the conversation that just ended: gives the event
 * that shows it (runscript.hpp), for EdTalkMode to run as a talk's own event is run. False when
 * none was.
 */
bool QuestHostTakeItemShown(int *event_no);

/**
 * A shop a quest opened in the conversation that just ended, made the custom shop the shop screen
 * opens next (custom_shop.hpp): gives the shop number to open it with. speaker_model is the
 * character file of the NPC the conversation was with, for a quest whose shopkeeper is the speaker;
 * null for none. False when no quest opened a shop.
 */
bool QuestHostTakeShop(const char *speaker_model, int *shop_no);

} // namespace dc::quest
