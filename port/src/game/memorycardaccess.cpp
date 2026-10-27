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
#include "menu_draw.hpp"
#include "menu_save.hpp"
#include "options/menu_option.hpp"
#include "platform/files.hpp"
#include "quest/host.hpp"
#include "save/save_slots.hpp"
#include "save/save_state.hpp"
#include "savedata.hpp"

namespace dc::game {

using namespace dc::options;
using namespace dc::platform;
using namespace dc::quest;
using namespace dc::save;

MC_DIR_ENTRY  SaveFileInfo[MC_DIR_ENTRY_MAX];
SV_CONFIG_SYS sys_config;

// There is no memory card: the operations read and write the files save_slots.hpp names. Each
// finishes in the step that starts it and returns 1, or -1 where a card would have failed, so the
// save screen's own flow and alerts still apply. The card the screens check is always there,
// formatted and roomy, and its save directory always exists.

namespace fs = std::filesystem;

namespace {

// Writes text and its terminator to the front of destination, leaving the bytes after it as they were.
void CopyString(std::span<char> destination, std::string_view text) {
    const auto end = std::ranges::copy(text, destination.begin()).out;
    *end = '\0';
}

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

void InitSaveFileInfoTbl() {
    std::ranges::fill(SaveFileInfo, MC_DIR_ENTRY{});
}

void CMemoryCardAccess::Initialize() {
    CopyString(this->dir_name, "BESCES-50295dkcloud");
    CopyString(this->file_name, "darkcloud");
    std::ranges::fill(this->current_dir, '\0');

    this->port = 0;
    this->file_no = 0;
    this->fd = -1;
    this->error = {};
    this->SetVersion("darkcloudVer1.9");
    this->func_no = MC_OPERATION_IDLE;
    this->idle_code = 0x3D;
    this->step = 0;
    this->save_buffer = nullptr;
    this->load_buffer = nullptr;
    this->read_buffer = nullptr;
    this->dir_table = SaveFileInfo;
    this->transferred = 0;
    this->transfer_size = 0;
    std::ranges::fill(this->card, MC_CARD_INFO{});
    std::ranges::fill(this->file_info, SAVEDATA_INFO{});
    this->icon = {};
    this->card[0].present = 1;
    this->card[1].present = 1;
    std::println("SaveData size = {}", sizeof(CSaveData));
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

void CMemoryCardAccess::SetIconData(const MC_ICON_DATA *icon) {
    this->icon.view = icon->view;
    this->icon.copy = icon->copy;
    this->icon.del = icon->del;
}

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

void CMemoryCardAccess::SetFuncNo(int func_no) {
    this->func_no = func_no;
    this->step = 0;

    if (func_no == MC_OPERATION_IDLE) {
        this->idle_code = 0x3D;
    }
}

int CMemoryCardAccess::GetFuncNo() {
    return this->func_no;
}

int CMemoryCardAccess::Step() {
    int result = 0;

    switch (func_no) {
        case MC_OPERATION_SEARCH_TYPE:
            result = SearchMcType();
            break;
        case MC_OPERATION_GET_DIR:
            result = GetDir();
            break;
        case MC_OPERATION_MAKE_DIR:
            result = MakeDir();
            break;
        case MC_OPERATION_GET_ALL_SAVE_FILE_INFO:
            result = GetAllSaveFileInfo();
            break;
        case MC_OPERATION_SAVE:
            result = SaveToMc(file_no);
            break;
        case MC_OPERATION_LOAD:
            result = LoadFromMc(file_no);
            break;
        case MC_OPERATION_FORMAT:
            result = FormatForMc();
            break;
        case MC_OPERATION_UNFORMAT:
            result = McUnFormatForDebug();
            break;
        case MC_OPERATION_DELETE:
            result = DeleteFile(error.code == MC_ERROR_NONE ? file_no : error.file_no);
            break;
        case MC_OPERATION_LOAD_CONFIG:
            result = LoadSysConfig();
            break;
        case MC_OPERATION_SAVE_CONFIG:
            result = SaveSysConfig();
            break;
        case MC_OPERATION_WRITE_TEST:
            result = Write();
            break;
        case MC_OPERATION_CONVERT:
            result = Convert();
            break;
        default:
            break;
    }

    if (result == 1) {
        step = 0;
        SetFuncNo(MC_OPERATION_IDLE);
    } else {
        McError(result);
    }

    return result;
}

void CMemoryCardAccess::SetVersion(const char *version) {
    CopyString(this->version, version);
}

char *CMemoryCardAccess::GetVersion() {
    return this->version;
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
    info.map_no = save->GetMapNo();
    info.play_time = save->GetPlayTime();
    info.party_size = save->GetDngStatus()->GetPartySize();

    // DrawSaveBoard names the map from a table of 62; a dungeon's autosave shows its town.
    if (IsDungeonMap(info.map_no)) {
        info.map_no -= DUNGEON_MAP_BASE;
    }
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

int CMemoryCardAccess::McError(int result) {
    MC_ERROR_INFO &info = this->error;

    switch (result) {
        case sceMcResNoFormat:
        case sceMcResFailDetect:
            std::println("mc is unformat");
            info.code = MC_ERROR_UNFORMATTED;
            break;
        case sceMcResFullDevice:
            std::println("memory is over or noting, break!");
            info.code = MC_ERROR_FULL;
            break;
        case sceMcResNoEntry:
            std::println("file not open or not exist");
            break;
        case sceMcResDeniedPermit:
            std::println("not open by write mode ");
            break;
        case sceMcResFailReplace:
            std::println("write failed");
            break;
        default:
            break;
    }

    if (result < -10) {
        info.code = MC_ERROR_NOT_CARD;
        std::println("not memory card or (read write)error = {}", result);
    }

    if (result < 0) {
        std::println("result = {}", result);
        info.func_no = this->GetFuncNo();
        info.step = this->step;
        std::println("func = {}", info.func_no);
        std::println("phase = {}", info.step);
        info.file_no = this->file_no;
    }

    return 0;
}

void CMemoryCardAccess::DmySync() {}

int CMemoryCardAccess::McUnFormatForDebug() {
    return 1;
}

char AtoraVibeTextureName[] __attribute__((section(".rodata"))) = "stayframe";

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

} // namespace dc::game
