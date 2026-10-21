#pragma once

#include <filesystem>

// The quest framework in the game (docs/QUEST_FRAMEWORK.md): the save data as the Quest::IGame the
// quests act on, their choices in villagers' conversations, their progress kept beside each save,
// and the shops they open.

// Registers the quests, the game and the conversation choices, the first time it is called.
void QuestHostSetup();

// A new game: every quest starts again.
void QuestHostNewGame();

// A conversation with villager npc_id of map map_no starts (EdTalkModeInit), before the talk script
// runs: retail behaviour a quest takes over is held back for it (Quest::Retail) until the
// conversation is up (QuestHostTalkOpened, its first tick in EdTalkMode).
void QuestHostTalkStarting(int map_no, int npc_id);
void QuestHostTalkOpened();
int  QuestHostPlayerAllItemCheck(int item);

// The quests' progress, written into or read from a save's folder (quests.json) as the save is.
// A save without the file (made before there were quests) starts every quest again.
bool QuestHostSave(const std::filesystem::path &save_directory);
void QuestHostLoad(const std::filesystem::path &save_directory);
// The save is being deleted: its quests.json goes first, so its folder is left empty.
void QuestHostDelete(const std::filesystem::path &save_directory);

// An item a quest showed the player receiving in the conversation that just ended: gives the event
// that shows it (runscript_port.hpp), for EdTalkMode to run as a talk's own event is run. False when
// none was.
bool QuestHostTakeItemShown(int *event_no);

// A shop a quest opened in the conversation that just ended, made the custom shop the shop screen
// opens next (custom_shop.hpp): gives the shop number to open it with. speaker_model is the
// character file of the NPC the conversation was with, for a quest whose shopkeeper is the speaker;
// null for none. False when no quest opened a shop.
bool QuestHostTakeShop(const char *speaker_model, int *shop_no);
