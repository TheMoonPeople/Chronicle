#pragma once

#include <optional>

#include "save/extension.hpp"

/**
 * What the player has heard, for the journal's screens: each villager's Georama request line once a
 * villager has said it. Kept per save, in chronicle.dat's JRNL chunk (save/extension.hpp); a new game
 * and a save without the chunk start with nothing heard.
 */
namespace dc::journal {

/**
 * The talk message holding a villager's request: the third of the twenty a villager has in
 * <town>talk_<lang>.mes, as the town scripts pick it when the request is not met.
 */
constexpr int RequestMessage(int villager) {
    return (villager * 20) + 22;
}

/**
 * Notes that a villager of a town has said a talk message; their request is heard once it is the one.
 */
void NoteTalkLine(int town, int villager, int message);

/**
 * Whether the player has heard a villager's request.
 */
bool RequestHeard(int town, int villager);

/**
 * Registers the JRNL chunk with the save.
 */
void RegisterSaveChunk();

/**
 * The chunk a save would keep, nothing when nothing is heard; and taking one back, null for none. The
 * registered chunk's writer and reader, here for tests.
 */
std::optional<save::ExtensionChunk> SaveChunk();
void                                LoadChunk(const save::ExtensionChunk *chunk);

} // namespace dc::journal
