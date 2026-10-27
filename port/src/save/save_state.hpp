#pragma once

#include <filesystem>

namespace dc::save {

/**
 * What the title needs to know before a save is loaded, kept as JSON beside the saves.
 * On the PS2 the memory card's configuration file held it.
 */
struct SaveState {
    int  last_save = 0;      /**< Folder of the last save loaded or written; 0 for none. */
    bool game_clear = false; /**< Game clear flag of the last save written. */
};

/**
 * Reads the state file at path into state. Returns false when there is no such file or it is not
 * JSON; a key that is missing or of another type keeps state's existing value.
 */
bool SaveStateRead(const std::filesystem::path &path, SaveState &state);

/**
 * Writes state durably to path (see FilesWrite), creating its directory first if need be.
 * Returns false when the directory or the file cannot be written.
 */
bool SaveStateWrite(const std::filesystem::path &path, const SaveState &state);

} // namespace dc::save
