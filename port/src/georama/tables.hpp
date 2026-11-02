#pragma once

#include <filesystem>

/**
 * The Georama's tables as documents (docs/GEORAMA.md): each town's parts and elements
 * (gameplay/georama/parts.json) and the Atla each dungeon hands out (gameplay/georama/atla.json), read as
 * the item documents are, the compiled tables (editpartsdata.cpp) for a town or dungeon a document leaves
 * out.
 */
namespace dc::georama {

/**
 * Reads the documents into the tables the game reads, EditPartsData, EditElementData and AtraAppearData;
 * a document that cannot be used is reported and the compiled tables stay.
 */
void Load();

/**
 * Writes both documents as compiled under directory/gameplay/georama/. False when they cannot be written.
 */
bool Export(const std::filesystem::path &directory);

} // namespace dc::georama
