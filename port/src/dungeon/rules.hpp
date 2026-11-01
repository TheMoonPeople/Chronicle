#pragma once

#include <filesystem>

/**
 * Changes to how a dungeon's events play, each off unless gameplay/dungeon/rules.json turns it on, so
 * without one the game plays as retail (docs/DUNGEON.md). The document is read as the item documents are.
 */
namespace dc::dungeon {

/**
 * Which changes are on.
 */
struct Rules {
    bool key_item_filter = false;        /**< An event's item window lists only the items it takes. */
    bool auto_open_locked_doors = false; /**< Cross at a locked room door opens it with its key. */
};

/**
 * Reads the highest-priority usable document, then the data's own, or turns every change off.
 */
void LoadRules();

/**
 * The changes that are on.
 */
const Rules &GetRules();

/**
 * Writes the document as compiled, every change off, under directory/gameplay/dungeon/. False when it
 * cannot be written.
 */
bool ExportRules(const std::filesystem::path &directory);

} // namespace dc::dungeon
