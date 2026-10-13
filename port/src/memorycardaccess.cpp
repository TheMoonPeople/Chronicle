#include "memorycardaccess.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <vector>

#include "dngstatusdata.hpp"
#include "dsgeorama.hpp"
#include "menu_option.hpp"
#include "platform/files.hpp"
#include "platform/save_state.hpp"
#include "save_slots.hpp"
#include "savedata.hpp"

// The port has no memory card: the operations read and write the files save_slots.hpp names.
// Each finishes in the step that starts it and returns 1, or -1 where retail's card would have
// failed, so the save screen's own flow and alerts still apply. The card the screens check is
// always there, formatted and roomy, and its save directory always exists.

namespace fs = std::filesystem;

namespace {

// Retail's save image: the save data, its version and its checksums.
constexpr int kRetailImageSize = 0x136A7;
#ifdef MOD_DEMON_SHAFT_GEORAMA
// The Demon Shaft georama mod's record follows retail's image; a save without it still loads.
constexpr int kImageSize = kRetailImageSize + DSG_SAVE_SIZE;
#else
constexpr int kImageSize = kRetailImageSize;
#endif
constexpr int kVersionSize = 0x20;

// The whole save, read before any of it reaches SaveData.
alignas(64) char g_image[kImageSize];
// How much of g_image the last ReadImage filled.
int g_image_read;

// Retail's ((int)p >> 6) + 1) << 6 on the whole pointer: the next 64-byte boundary strictly after p.
char *PastNext64(char *pointer) {
    return pointer + (64 - reinterpret_cast<std::uintptr_t>(pointer) % 64);
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

// Reads file_no's whole save into g_image: this version's, and every checksum right.
bool ReadImage(int file_no, const char *version) {
    std::memset(g_image, 0, sizeof(g_image));
    g_image_read = static_cast<int>(FilesRead(SaveSlotPath(file_no), g_image, kImageSize));

    if (g_image_read < kRetailImageSize ||
        std::strncmp(g_image + sizeof(CSaveData), version, kVersionSize) != 0) {
        return false;
    }

    const char *data = g_image;
    const char *sum = g_image + sizeof(CSaveData) + kVersionSize;
    char        total = 0;

    for (u32 i = 0; i < sizeof(CSaveData); i++) {
        total += *data++;

        if (i % 64 == 63) {
            if (total != *sum++) {
                return false;
            }

            total = 0;
        }
    }

    return true;
}

} // namespace

PORT_OVERRIDE int CMemoryCardAccess::InitForMC() {
    this->Initialize();
    InitSaveFileInfoTbl();
    return 0;
}

PORT_OVERRIDE void CMemoryCardAccess::SetBuff(char *buffer) {
    char *data;
    char *sum;
    u32   i;
    char  total;

    buffer = PastNext64(buffer);
    this->save_buffer = (CSaveData *) buffer;
    memcpy(this->save_buffer, SaveData, 0x131C0);
    GameOptionsClear(*this->save_buffer);
    this->save_buffer->ConvertConfig(&sys_config);
    char *version = (char *) this->save_buffer + 0x131C0;
    strcpy(version, this->version);
    this->check_sum = version + 0x20;
    data = (char *) this->save_buffer;
    sum = this->check_sum;
    memset(sum, 0, 0x4C7);
    total = 0;

    for (i = 0; i < 0x131C0; i++) {
        total += *data++;

        if ((int) i % 64 == 63) {
            *sum++ = total;
            total = 0;
        }
    }

#ifdef MOD_DEMON_SHAFT_GEORAMA
    DsgWriteSave((char *) this->save_buffer + kRetailImageSize);
    sum = (char *) this->save_buffer + kImageSize;
#endif
    this->read_buffer = PastNext64(sum);
    this->load_buffer = this->read_buffer;
}

PORT_OVERRIDE void CMemoryCardAccess::SetFuncNo(int func_no) {
    this->func_no = func_no;
    this->step = 0;

    if (func_no == MC_OPERATION_IDLE) {
        this->idle_code = 0x3D;
    }
}

PORT_OVERRIDE int CMemoryCardAccess::SearchMcType() {
    MC_CARD_INFO *card = &this->card[this->port];

    card->present = 1;
    card->type = sceMcTypePS2;
    card->formatted = 1;
    card->format_change = 0;
    card->free_size = 8000;
    card->result = sceMcResSucceed;
    return 1;
}

PORT_OVERRIDE int CMemoryCardAccess::GetDir() {
    MC_CARD_INFO *card = &this->card[this->port];

    InitSaveFileInfoTbl();
    card->dir_exists = 1;
    card->dir_entries = static_cast<s32>(SaveSlotFiles().size());
    return 1;
}

// Retail took the card's whole configuration; state.json gives the last save and the game clear
// flag, the options being config.json's. Without the file the game keeps its own, as it did
// without a save directory.
PORT_OVERRIDE int CMemoryCardAccess::LoadSysConfig() {
    SaveState state = CurrentState();

    if (SaveStateRead(SaveStatePath(), state)) {
        ConfigWord(kConfigLastFile) = state.last_save - 1;
        ConfigWord(kConfigGameClear) = state.game_clear;
    }

    return 1;
}

PORT_OVERRIDE int CMemoryCardAccess::SaveSysConfig() {
    return SaveStateWrite(SaveStatePath(), CurrentState()) ? 1 : -1;
}

// The card's write test and the copy out of the NTSC 1.0 directory have nothing to work on.
PORT_OVERRIDE int CMemoryCardAccess::Write() {
    return 1;
}

PORT_OVERRIDE int CMemoryCardAccess::Convert() {
    return 1;
}

PORT_OVERRIDE int CMemoryCardAccess::MakeDir() {
    this->card[this->port].dir_exists = 1;
    return 1;
}

// Adds the file to SaveSlots when it is a whole save of this version. Any other file is left as
// it is, unlisted: retail deleted a save of another version once its message was dismissed.
PORT_OVERRIDE int CMemoryCardAccess::GetSaveFileInfoFromMc(int file_no) {
    if (!ReadImage(file_no, this->GetVersion())) {
        return 1;
    }

    CSaveData    *save = reinterpret_cast<CSaveData *>(g_image);
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

    if (info.quest_total > 9999) {
        info.quest_total = 9999;
    }

    SaveSlots.saves.push_back(info);
    return 1;
}

PORT_OVERRIDE int CMemoryCardAccess::GetAllSaveFileInfo() {
    memset(this->file_info, 0, sizeof(this->file_info));
    SaveSlots.files = SaveSlotFiles();
    SaveSlots.saves.clear();

    for (int file_no : SaveSlots.files) {
        this->GetSaveFileInfoFromMc(file_no);
    }

    return 1;
}

// A new save has nothing to confirm: it never replaces one.
PORT_OVERRIDE int CMemoryCardAccess::CheckFileNo(int file_no) {
    std::error_code error;
    return !SaveSlotNew && file_no >= 0 && fs::exists(SaveSlotDirectory(file_no), error);
}

// A new save takes its folder by creating it, which fails when anything has that name: one taken by
// another copy of the game or by hand since the list was read sends the save to the next free
// folder instead, and file_no is then the file written.
PORT_OVERRIDE int CMemoryCardAccess::SaveToMc(int file_no) {
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
            if (!std::binary_search(files.begin(), files.end(), file_no)) {
                files.insert(std::upper_bound(files.begin(), files.end(), file_no), file_no);
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
    return SaveStateWrite(SaveStatePath(), CurrentState()) ? 1 : -1;
}

PORT_OVERRIDE int CMemoryCardAccess::LoadFromMc(int file_no) {
    if (!ReadImage(file_no, this->GetVersion())) {
        return -1;
    }

    // Remember only the cursor, retaining title state before the older payload replaces it.
    // Missing or malformed state falls back to the current session's values.
    SaveState state = CurrentState();
    SaveStateRead(SaveStatePath(), state);
    memcpy(SaveData, g_image, sizeof(CSaveData));
#ifdef MOD_DEMON_SHAFT_GEORAMA
    DsgReadSave(g_image + kRetailImageSize, g_image_read - kRetailImageSize);
#endif
    ConfigWord(kConfigLastFile) = file_no;
    state.last_save = file_no + 1;
    // Cursor memory is useful across restarts, but its failure must not reject a valid save.
    if (!SaveStateWrite(SaveStatePath(), state)) {
        std::fprintf(stderr, "save state: could not remember loaded save %d\n", file_no + 1);
    }
    return 1;
}

PORT_OVERRIDE int CMemoryCardAccess::FormatForMc() {
    this->card[this->port].formatted = 1;
    return 1;
}

// A save already gone counts as deleted; its folder goes with it once nothing else is in it.
PORT_OVERRIDE int CMemoryCardAccess::DeleteFile(int file_no) {
    std::error_code error;
    fs::remove(SaveSlotPath(file_no), error);

    if (error) {
        return -1;
    }

    fs::remove(SaveSlotDirectory(file_no), error);
    return 1;
}

PORT_OVERRIDE void CMemoryCardAccess::DmySync() {}

PORT_OVERRIDE int CMemoryCardAccess::McUnFormatForDebug() {
    return 1;
}
