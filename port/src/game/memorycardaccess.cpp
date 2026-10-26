#include "memorycardaccess.hpp"

#include <algorithm>
#include <print>
#include <span>
#include <string_view>

#include "menu_draw.hpp"
#include "savedata.hpp"

MC_DIR_ENTRY  SaveFileInfo[MC_DIR_ENTRY_MAX];
SV_CONFIG_SYS sys_config;

namespace {

// Writes text and its terminator to the front of destination, leaving the bytes after it as they were.
void CopyString(std::span<char> destination, std::string_view text) {
    const auto end = std::ranges::copy(text, destination.begin()).out;
    *end = '\0';
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

void CMemoryCardAccess::SetIconData(const MC_ICON_DATA *icon) {
    this->icon.view = icon->view;
    this->icon.copy = icon->copy;
    this->icon.del = icon->del;
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

char AtoraVibeTextureName[] __attribute__((section(".rodata"))) = "stayframe";
