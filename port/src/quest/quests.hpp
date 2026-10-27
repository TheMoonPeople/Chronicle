#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "quest/framework.hpp"

// The port's quests (docs/QUEST_FRAMEWORK.md), one file each in this folder. A new quest is a
// function here and a line in RegisterAllQuests: the list is spelled out rather than filled in by
// globals, which a linker is free to leave out of a static library.

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
 * Builds Carl's Slingshot, the example quest of docs/QUEST_FRAMEWORK.md.
 */
Definition CarlsSlingshot();

/**
 * Lists the slingshot quest localization text.
 */
std::span<const Text> CarlsSlingshotTexts();

/**
 * Registers every quest above, in the order their choices appear in an NPC's menu.
 */
void RegisterAllQuests();

/**
 * Lists all quest localization keys and their English text.
 */
std::vector<std::pair<std::string, std::string>> QuestStrings();

} // namespace dc::quest
