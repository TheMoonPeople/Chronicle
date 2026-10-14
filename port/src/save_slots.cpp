#include "save_slots.hpp"

#include <algorithm>
#include <string>
#include <system_error>

#include "platform/paths.hpp"

namespace fs = std::filesystem;

namespace {

// Nine digits always fit an int.
constexpr std::size_t kMaxDigits = 9;

} // namespace

SaveSlotList SaveSlots;

bool SaveSlotNew;

fs::path SaveSlotsRoot() {
    return PathsSaveRoot() / "saves";
}

fs::path SaveSlotDirectory(int file_no) {
    if (file_no == kSaveSlotAutosave) {
        return SaveSlotsRoot() / "autosave";
    }
    return SaveSlotsRoot() / std::to_string(file_no + 1);
}

fs::path SaveSlotPath(int file_no) {
    return SaveSlotDirectory(file_no) / "save.dat";
}

fs::path SaveStatePath() {
    return SaveSlotsRoot() / "state.json";
}

// A number as the board writes it: from 1, with no leading zero.
int SaveSlotFileNo(std::string_view name) {
    if (name.empty() || name.size() > kMaxDigits || name[0] == '0') {
        return -1;
    }
    int number = 0;
    for (char c : name) {
        if (c < '0' || c > '9') {
            return -1;
        }
        number = number * 10 + (c - '0');
    }
    return number - 1;
}

std::vector<int> SaveSlotFiles() {
    std::vector<int> files;
    std::error_code  error;
    for (const fs::directory_entry &entry : fs::directory_iterator(SaveSlotsRoot(), error)) {
        const std::u8string name = entry.path().filename().u8string();
        int                 file_no = SaveSlotFileNo(std::string_view(reinterpret_cast<const char *>(name.data()), name.size()));
        if (file_no >= 0) {
            files.push_back(file_no);
        }
    }
    std::ranges::sort(files);
    return files;
}

int SaveSlotFirstFree(const std::vector<int> &files) {
    int file_no = 0;
    for (int used : files) {
        if (used != file_no) {
            break;
        }
        ++file_no;
    }
    return file_no;
}

int SaveSlotRows(const SaveSlotList &list, bool new_save) {
    return static_cast<int>(list.saves.size()) + (new_save ? 1 : 0);
}

int SaveSlotRow(const SaveSlotList &list, int file_no, bool new_save) {
    for (std::size_t row = 0; row < list.saves.size(); ++row) {
        if (list.saves[row].file_no == file_no + 1) {
            return static_cast<int>(row);
        }
    }
    return new_save ? static_cast<int>(list.saves.size()) : 0;
}

int SaveSlotFileAt(const SaveSlotList &list, int row) {
    if (row >= 0 && row < static_cast<int>(list.saves.size())) {
        return list.saves[row].file_no - 1;
    }
    return SaveSlotFirstFree(list.files);
}
