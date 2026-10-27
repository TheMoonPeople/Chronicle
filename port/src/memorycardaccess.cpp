#include "memorycardaccess.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <print>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#include "dngstatusdata.hpp"
#include "menu_option.hpp"
#include "menu_save.hpp"
#include "platform/files.hpp"
#include "platform/save_state.hpp"
#include "quest_host.hpp"
#include "save_slots.hpp"
#include "savedata.hpp"

// There is no memory card: the operations read and write the files save_slots.hpp names. Each
// finishes in the step that starts it and returns 1, or -1 where a card would have failed, so the
// save screen's own flow and alerts still apply. The card the screens check is always there,
// formatted and roomy, and its save directory always exists.

namespace fs = std::filesystem;

namespace {

constexpr int kImageSize = 0x136A7;
constexpr int kVersionSize = 0x20;

// The whole save, read before any of it reaches SaveData.
struct alignas(64) SaveImage {
    std::array<char, kImageSize> bytes{};
};

std::unique_ptr<SaveImage> g_image_storage = std::make_unique<SaveImage>();
char *const                g_image = g_image_storage->bytes.data();

constexpr char kVersion[] = "darkcloudVer1.9";

// The next 64-byte boundary strictly after pointer.
char *PastNext64(char *pointer) {
    return pointer + (64 - reinterpret_cast<std::uintptr_t>(pointer) % 64);
}

// Copies text into a zeroed field without its terminator, which the field already holds.
template <typename Field>
void CopyText(Field &field, std::string_view text) {
    std::ranges::copy(text, std::begin(field));
}

// Config words of the save data that state.json keeps: the last save's file and the game clear
// flag, which InitMenuSave sets for the save after the ending.
constexpr int kConfigGameClear = 14;
constexpr int kConfigLastFile = 17;

s32 &ConfigWord(int word) {
    return static_cast<s32 *>(SaveData->GetConfigData())[word];
}

SaveState CurrentState() {
    SaveState state;
    int       file_no = ConfigWord(kConfigLastFile);
    state.last_save = file_no >= 0 && file_no < 999999999 ? file_no + 1 : 0;
    state.game_clear = ConfigWord(kConfigGameClear) != 0;
    return state;
}

void BuildImage(char *image, const char *version) {
    auto *save = reinterpret_cast<CSaveData *>(image);
    std::memcpy(save, SaveData, sizeof(CSaveData));
    GameOptionsClear(*save);
    save->ConvertConfig(&sys_config);
    std::span<char, kImageSize> bytes(image, kImageSize);
    std::ranges::fill(bytes.subspan(sizeof(CSaveData)), '\0');
    std::ranges::copy(std::string_view(version), bytes.begin() + sizeof(CSaveData));
    std::span<char> sums = bytes.subspan(sizeof(CSaveData) + kVersionSize);
    char            total = 0;

    for (u32 i = 0; i < sizeof(CSaveData); i++) {
        total += bytes[i];

        if (i % 64 == 63) {
            sums[i / 64] = total;
            total = 0;
        }
    }
}

// Reads file_no's whole save into g_image: this version's, and every checksum right.
bool ReadImage(int file_no, const char *version) {
    if (FilesRead(SaveSlotPath(file_no), g_image, kImageSize) < kImageSize ||
        std::strncmp(g_image + sizeof(CSaveData), version, kVersionSize) != 0) {
        return false;
    }

    std::span<const char, kImageSize> bytes(g_image, kImageSize);
    std::span<const char>             sums = bytes.subspan(sizeof(CSaveData) + kVersionSize);
    char                              total = 0;

    for (u32 i = 0; i < sizeof(CSaveData); i++) {
        total += bytes[i];

        if (i % 64 == 63) {
            if (total != sums[i / 64]) {
                return false;
            }

            total = 0;
        }
    }

    return true;
}

} // namespace

void CMemoryCardAccess::MakeMcIconSysInfo() {
    int bg_color[4][4] = {
        {0x80, 0,    0x40, 0},
        {0,    0x80, 0,    0},
        {0,    0,    0x80, 0},
        {0x80, 0x80, 0x80, 0},
    };
    float light_dir[3][4] = {
        {0.5f,  0.5f,  0.5f,  0.0f},
        {0.0f,  -0.4f, -0.1f, 0.0f},
        {-0.5f, -0.5f, 0.5f,  0.0f},
    };
    float light_color[3][4] = {
        {0.48f, 0.48f, 0.03f, 0.0f},
        {0.5f,  0.33f, 0.2f,  0.0f},
        {0.14f, 0.14f, 0.38f, 0.0f},
    };
    float ambient[4] = {0.5f, 0.5f, 0.5f, 0.0f};

    this->icon_sys = {};
    // The header is a four-byte identifier, not a null-terminated string.
    CopyText(this->icon_sys.head, "PS2D");
    // "Dark Cloud Data" in Shift-JIS full-width characters.
    CopyText(this->icon_sys.title_name, "\x82\x63\x82\x81\x82\x92\x82\x8B\x81\x40\x82\x62\x82\x8C\x82\x8F"
                                        "\x82\x95\x82\x84\x81\x40\x82\x63\x82\x81\x82\x94\x82\x81");
    this->icon_sys.nl_offset = 20;
    this->icon_sys.trans_rate = 0x60;
    // Only the first entry of each lighting table reaches the icon.
    std::memcpy(this->icon_sys.bg_color, bg_color, 16);
    std::memcpy(this->icon_sys.light_dir, light_dir, 16);
    std::memcpy(this->icon_sys.light_color, light_color, 16);
    std::memcpy(this->icon_sys.ambient, ambient, 16);
    CopyText(this->icon_sys.fname_view, this->icon.view.name);
    CopyText(this->icon_sys.fname_copy, this->icon.copy.name);
    CopyText(this->icon_sys.fname_del, this->icon.del.name);
}

int CMemoryCardAccess::InitForMC() {
    this->Initialize();
    InitSaveFileInfoTbl();
    return 0;
}

void CMemoryCardAccess::SetBuff(char *buffer) {
    buffer = PastNext64(buffer);
    this->save_buffer = reinterpret_cast<CSaveData *>(buffer);
    BuildImage(buffer, this->version);
    this->check_sum = buffer + sizeof(CSaveData) + kVersionSize;
    this->read_buffer = PastNext64(buffer + kImageSize);
    this->load_buffer = this->read_buffer;
}

void CMemoryCardAccess::SetFuncNo(int func_no) {
    this->func_no = func_no;
    this->step = 0;

    if (func_no == MC_OPERATION_IDLE) {
        this->idle_code = 0x3D;
    }
}

int CMemoryCardAccess::SearchMcType() {
    MC_CARD_INFO *card = &this->card[this->port];

    card->present = 1;
    card->type = sceMcTypePS2;
    card->formatted = 1;
    card->format_change = 0;
    card->free_size = 8000;
    card->result = sceMcResSucceed;
    return 1;
}

int CMemoryCardAccess::GetDir() {
    MC_CARD_INFO *card = &this->card[this->port];

    InitSaveFileInfoTbl();
    card->dir_exists = 1;
    card->dir_entries = static_cast<s32>(SaveSlotFiles().size());
    return 1;
}

// state.json gives the last save and the game clear flag; the options are config.json's. Without
// the file the game keeps its own.
int CMemoryCardAccess::LoadSysConfig() {
    SaveState state = CurrentState();

    if (SaveStateRead(SaveStatePath(), state)) {
        ConfigWord(kConfigLastFile) = state.last_save - 1;
        ConfigWord(kConfigGameClear) = state.game_clear;
    }

    return 1;
}

int CMemoryCardAccess::SaveSysConfig() {
    return SaveStateWrite(SaveStatePath(), CurrentState()) ? 1 : -1;
}

// The card's write test and the copy out of the NTSC 1.0 directory have nothing to work on.
int CMemoryCardAccess::Write() {
    return 1;
}

int CMemoryCardAccess::Convert() {
    return 1;
}

int CMemoryCardAccess::MakeDir() {
    this->card[this->port].dir_exists = 1;
    return 1;
}

// Adds the file to SaveSlots when it is a whole save of this version. Any other file is left as
// it is, unlisted.
int CMemoryCardAccess::GetSaveFileInfoFromMc(int file_no) {
    if (!ReadImage(file_no, this->GetVersion())) {
        return 1;
    }

    auto         *save = reinterpret_cast<CSaveData *>(g_image);
    SAVEDATA_INFO info = {};
    info.state = 1;
    info.file_no = file_no + 1;
    std::memcpy(info.name, save->GetCharaName(0), sizeof(info.name));
    info.map_no = save->map_no;
    info.play_time = save->GetPlayTime();
    info.party_size = save->GetDngStatus()->GetPartySize();

    // DrawSaveBoard names the map from a table of 62.
    if (info.map_no < 0 || info.map_no >= 62) {
        info.map_no = 0;
    }

    for (int dungeon = 0; dungeon < 7; dungeon++) {
        info.quest_total += std::clamp(save->QuestDungeon(dungeon, 0), 0, 9999);
    }

    info.quest_total = std::min(info.quest_total, 9999);

    SaveSlots.saves.push_back(info);
    return 1;
}

// The load screen lists the autosave first; the save screen cannot write it.
int CMemoryCardAccess::GetAllSaveFileInfo() {
    std::memset(this->file_info, 0, sizeof(this->file_info));
    SaveSlots.files = SaveSlotFiles();
    SaveSlots.saves.clear();

    if (SaveMenu.mode == SAVE_MENU_MODE_LOAD) {
        this->GetSaveFileInfoFromMc(kSaveSlotAutosave);
    }

    for (int file_no : SaveSlots.files) {
        this->GetSaveFileInfoFromMc(file_no);
    }

    return 1;
}

// A new save has nothing to confirm: it never replaces one.
int CMemoryCardAccess::CheckFileNo(int file_no) {
    std::error_code error;
    return !SaveSlotNew && file_no >= 0 && fs::exists(SaveSlotDirectory(file_no), error);
}

// A new save takes its folder by creating it, which fails when anything has that name: one taken by
// another copy of the game or by hand since the list was read sends the save to the next free
// folder instead, and file_no is then the file written.
int CMemoryCardAccess::SaveToMc(int file_no) {
    if (file_no < 0 || file_no >= 999999999) {
        return -1;
    }
    FilesResult result = FilesResult::kWritten;

    if (SaveSlotNew) {
        if (FilesCreateDirectory(SaveSlotsRoot()) == FilesResult::kFailed) {
            return -1;
        }

        result = FilesCreateDirectory(SaveSlotDirectory(file_no));

        for (int attempt = 0; result == FilesResult::kExists && attempt < 100; attempt++) {
            std::vector<int> files = SaveSlotFiles();
            if (!std::ranges::binary_search(files, file_no)) {
                files.insert(std::ranges::upper_bound(files, file_no), file_no);
            }
            file_no = SaveSlotFirstFree(files);
            result = FilesCreateDirectory(SaveSlotDirectory(file_no));
        }
    }

    if (result == FilesResult::kWritten) {
        result = FilesWrite(SaveSlotPath(file_no), this->save_buffer, kImageSize, !SaveSlotNew);
    }

    if (result != FilesResult::kWritten) {
        // Leave failed claims reserved: the pathname may now belong to another actor.
        return -1;
    }

    this->file_no = file_no;
    ConfigWord(kConfigLastFile) = file_no;
    this->card[this->port].dir_exists = 1;
    // The quests' progress goes beside the save; a failure is reported, and the save stands.
    QuestHostSave(SaveSlotDirectory(file_no));
    return SaveStateWrite(SaveStatePath(), CurrentState()) ? 1 : -1;
}

int CMemoryCardAccess::LoadFromMc(int file_no) {
    if (!ReadImage(file_no, this->GetVersion())) {
        return -1;
    }

    // Remember only the cursor, retaining title state before the older payload replaces it.
    // Missing or malformed state falls back to the current session's values.
    SaveState state = CurrentState();
    SaveStateRead(SaveStatePath(), state);
    std::memcpy(SaveData, g_image, sizeof(CSaveData));
    QuestHostLoad(SaveSlotDirectory(file_no));

    // The save screen keeps starting at the last numbered save.
    if (file_no == kSaveSlotAutosave) {
        ConfigWord(kConfigLastFile) = state.last_save - 1;
        return 1;
    }

    ConfigWord(kConfigLastFile) = file_no;
    state.last_save = file_no + 1;
    // Cursor memory is useful across restarts, but its failure must not reject a valid save.
    if (!SaveStateWrite(SaveStatePath(), state)) {
        std::println(stderr, "save state: could not remember loaded save {}", file_no + 1);
    }
    return 1;
}

int CMemoryCardAccess::FormatForMc() {
    this->card[this->port].formatted = 1;
    return 1;
}

// A save already gone counts as deleted; its folder goes with it once nothing else is in it.
int CMemoryCardAccess::DeleteFile(int file_no) {
    std::error_code error;
    fs::remove(SaveSlotPath(file_no), error);

    if (error) {
        return -1;
    }

    QuestHostDelete(SaveSlotDirectory(file_no));
    fs::remove(SaveSlotDirectory(file_no), error);
    return 1;
}

void CMemoryCardAccess::DmySync() {}

int CMemoryCardAccess::McUnFormatForDebug() {
    return 1;
}

bool SaveSlotWriteAutosave() {
    if (FilesCreateDirectory(SaveSlotsRoot()) == FilesResult::kFailed ||
        FilesCreateDirectory(SaveSlotDirectory(kSaveSlotAutosave)) == FilesResult::kFailed) {
        return false;
    }
    BuildImage(g_image, kVersion);
    if (FilesWrite(SaveSlotPath(kSaveSlotAutosave), g_image, kImageSize, true) != FilesResult::kWritten) {
        return false;
    }
    QuestHostSave(SaveSlotDirectory(kSaveSlotAutosave));
    return true;
}
