#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "memorycardaccess.hpp"

namespace dc::save {

// The port keeps no memory card. Its saves are in <save root>/saves: a folder for each save, named
// after the number its board shows, N + 1 for the game's file N, holding save.dat, the image retail
// wrote to the card as darkcloudN. Shared state.json sits beside the numbered folders.

/**
 * Returns the folder holding every save folder and state.json.
 */
std::filesystem::path SaveSlotsRoot();

/**
 * The autosave's file number; its folder is saves/autosave, which no numbered save can take.
 */
constexpr int kSaveSlotAutosave = -2;

/**
 * Returns the folder holding file_no's save.
 */
std::filesystem::path SaveSlotDirectory(int file_no);

/**
 * Returns the path of file_no's save.dat.
 */
std::filesystem::path SaveSlotPath(int file_no);

/**
 * Returns the path of the shared state.json.
 */
std::filesystem::path SaveStatePath();

/**
 * Returns the file a save folder's name stands for, or -1 for any other name.
 */
int SaveSlotFileNo(std::string_view name);

/**
 * Returns the file of every save folder's name, ascending, whatever each holds: a folder without a
 * whole save, or a file under such a name, keeps its number from a new save too.
 */
std::vector<int> SaveSlotFiles();

/**
 * Returns the lowest file number that the ascending files do not hold.
 */
int SaveSlotFirstFree(const std::vector<int> &files);

/**
 * What the save and load screens list.
 */
struct SaveSlotList {
    std::vector<int>                 files; /**< Every save file, readable or not. */
    std::vector<game::SAVEDATA_INFO> saves; /**< The readable ones, ascending by file. */
};

extern SaveSlotList SaveSlots;

/**
 * Set while the save screen saves to a new file, which then never replaces one.
 */
extern bool SaveSlotNew;

/**
 * Returns the screens' board count: one per readable save and, on the save screen, a last one for a
 * new save.
 */
int SaveSlotRows(const SaveSlotList &list, bool new_save);

/**
 * Returns the board showing file_no; a file with no board of its own gives the new save's board, or
 * the first one.
 */
int SaveSlotRow(const SaveSlotList &list, int file_no, bool new_save);

/**
 * Returns the file a board stands for; the new save's board stands for the first free file.
 */
int SaveSlotFileAt(const SaveSlotList &list, int row);

} // namespace dc::save
