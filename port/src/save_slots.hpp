#pragma once

#include <filesystem>
#include <string_view>
#include <vector>

#include "memorycardaccess.hpp"

// The port keeps no memory card. Its saves are in <save root>/saves: a folder for each save, named
// after the number its board shows, N + 1 for the game's file N, holding save.dat, the image retail
// wrote to the card as darkcloudN. Shared state.json sits beside the numbered folders.

std::filesystem::path SaveSlotsRoot();

// The autosave's file: its folder is saves/autosave, which no numbered save can take.
constexpr int kSaveSlotAutosave = -2;

std::filesystem::path SaveSlotDirectory(int file_no);

std::filesystem::path SaveSlotPath(int file_no);

std::filesystem::path SaveStatePath();

// The file a save folder's name stands for, or -1 for any other name.
int SaveSlotFileNo(std::string_view name);

// The file of every save folder's name, ascending, whatever each holds: a folder without a whole
// save, or a file under such a name, keeps its number from a new save too.
std::vector<int> SaveSlotFiles();

// The lowest N that ascending files does not hold.
int SaveSlotFirstFree(const std::vector<int> &files);

// What the save and load screens list.
struct SaveSlotList {
    std::vector<int>           files; // Every save file, readable or not.
    std::vector<SAVEDATA_INFO> saves; // The readable ones, ascending by file.
};

extern SaveSlotList SaveSlots;

// Set while the save screen saves to a new file, which then never replaces one.
extern bool SaveSlotNew;

// The screens' boards: one per readable save and, on the save screen, a last one for a new save.
int SaveSlotRows(const SaveSlotList &list, bool new_save);

// The board showing file_no; a file with no board of its own gives the new save's board, or the
// first one.
int SaveSlotRow(const SaveSlotList &list, int file_no, bool new_save);

// The file a board stands for; the new save's board stands for the first free file.
int SaveSlotFileAt(const SaveSlotList &list, int row);

// Writes the game as it stands over the autosave, as the save screen would write it; false if the
// write failed, the autosave then keeping what it held.
bool SaveSlotWriteAutosave();
