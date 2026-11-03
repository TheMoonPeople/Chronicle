#pragma once

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <utility>
#include <vector>

/**
 * Quests as documents (docs/QUEST_FRAMEWORK.md, "Quest documents"): gameplay/quests/<id>.json, kind quest,
 * one quest a file, from every active datapack and the data, so a pack adds a quest without the game
 * being rebuilt. Each becomes a quest::Definition through quest::Builder, its conditions the library's,
 * its choices' steps quest::Scene's calls.
 */
namespace dc::quest {

/**
 * Reads gameplay/quests/: each file name once, the highest-priority pack's, then the data's. A document
 * that cannot be used is reported and left out, as is one naming an item no item document has, so the
 * items load first.
 */
void LoadDocuments();

/**
 * Registers the quests LoadDocuments read, in ID order, each one's counters following the game from then on.
 */
void RegisterDocuments();

/**
 * Registers one quest document and starts its counters; throws std::runtime_error, registering nothing,
 * for one that cannot be used.
 */
void RegisterDocument(const nlohmann::json &document);

/**
 * A quest document's text: each key as the localization files name it (quest.<id>.<key>) and its English.
 */
std::vector<std::pair<std::string, std::string>> DocumentStrings(const nlohmann::json &document);

/**
 * The text of every quest LoadDocuments read.
 */
std::vector<std::pair<std::string, std::string>> DocumentStrings();

} // namespace dc::quest
