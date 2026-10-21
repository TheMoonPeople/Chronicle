#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "quest_framework.hpp"

// The port's quests (docs/QUEST_FRAMEWORK.md), one file each in this folder. A new quest is a
// function here and a line in RegisterAllQuests (quests.cpp): the list is spelled out rather than
// filled in by globals, which a linker is free to leave out of a static library.

// The Mayor of Norune: map 0, villager 13, as the dialog framework numbers them (DC_DIALOG_TEST's
// "Who are you?" shows a villager's).
const Quest::NpcMatch &NoruneMayor();
// His lines in the town's villager message file start at 20 + 13 * 20, as every villager's do at
// 20 + villager * 20 (the talk script, gedit/e01/event.stb).
inline constexpr int kNoruneMayorLines = 280;

// The Mayor of Norune's supplies, a choice of his menu rather than handed out unasked: Tasty Water,
// Bread, Repair Powder, Escape Powder and Antidote Drink, as his talk script hands them out.
extern const int  kNoruneMayorSupplies[5];
Quest::Definition MayorSupplies();
/**
 * Lists the Mayor quest localization text.
 */
std::span<const Quest::Text> MayorSuppliesTexts();

// Carl's Slingshot: Gina borrowed it and won't give it back; the Bone Slingshot for Xiao in the end.
// The example quest of docs/QUEST_FRAMEWORK.md.
Quest::Definition CarlsSlingshot();
/**
 * Lists the slingshot quest localization text.
 */
std::span<const Quest::Text> CarlsSlingshotTexts();

// Registers every quest above.
void RegisterAllQuests();

/**
 * Lists all quest localization keys and their English text.
 */
std::vector<std::pair<std::string, std::string>> QuestStrings();
