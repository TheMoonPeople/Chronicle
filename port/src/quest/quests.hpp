#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "quest/framework.hpp"

// The port's quests in code (docs/QUEST_FRAMEWORK.md): only those that take over something retail does
// itself; every other quest is a document (quest/documents.hpp). The list is spelled out rather than
// filled in by globals, which a linker is free to leave out of a static library.

namespace dc::quest {

/**
 * Matches the Mayor of Norune: map 0, villager 13, as the dialog framework numbers them.
 */
const NpcMatch &NoruneMayor();

/**
 * First of the Mayor's lines in the town's villager message file (20 + villager * 20).
 */
inline constexpr int kNoruneMayorLines = 280;

/**
 * Items the Mayor hands out from his menu, in his talk script's order.
 */
extern const int kNoruneMayorSupplies[5];

/**
 * Builds the Mayor of Norune's supplies quest, a choice of his menu.
 */
Definition MayorSupplies();

/**
 * Lists the Mayor quest localization text.
 */
std::span<const Text> MayorSuppliesTexts();

/**
 * Registers every quest: the Mayor's, then the quest documents (quest/documents.hpp) in ID order, the
 * order their choices appear in an NPC's menu.
 */
void RegisterAllQuests();

/**
 * Lists the quests' localization keys and their English: the Mayor's, and the documents' read.
 */
std::vector<std::pair<std::string, std::string>> QuestStrings();

} // namespace dc::quest
