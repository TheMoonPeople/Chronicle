#include "memorycardaccess.hpp"

#include <cstdio>
#include <cstring>

#include "menu_draw.hpp"
#include "savedata.hpp"

MC_DIR_ENTRY  SaveFileInfo[MC_DIR_ENTRY_MAX];
char          mcdmybuf[0x40];
SV_CONFIG_SYS sys_config;

void InitSaveFileInfoTbl() {
    for (int i = 0; i < MC_DIR_ENTRY_MAX; i++) {
        memset(&SaveFileInfo[i], 0, sizeof(SaveFileInfo[i]));
        SaveFileInfo[i].name[0] = 0;
    }
}

int GetOpenAttribute(char *name) {
    for (int i = 0; i < MC_DIR_ENTRY_MAX; i++) {
        char *entry = SaveFileInfo[i].name;

        if (strcmp(name, entry) == 0) {
            printf("same file existed!!!!!!\n");
            return 1;
        }
    }

    return 0;
}

void CMemoryCardAccess::Initialize() {
    strcpy(this->dir_name, "BESCES-50295dkcloud");
    strcpy(this->file_name, "darkcloud");

    for (int i = 0; i < 0x40; i++) {
        this->current_dir[i] = 0;
    }

    this->port = 0;
    this->file_no = 0;
    this->fd = -1;
    memset(&this->error, 0, sizeof(this->error));
    this->SetVersion("darkcloudVer1.9");
    this->func_no = MC_OPERATION_IDLE;
    this->idle_code = 0x3D;
    this->step = 0;
    this->save_buffer = NULL;
    this->load_buffer = NULL;
    this->read_buffer = NULL;
    this->dir_table = SaveFileInfo;
    this->transferred = 0;
    this->transfer_size = 0;
    memset(this->card, 0, sizeof(this->card));
    memset(this->file_info, 0, sizeof(this->file_info));
    memset(&this->icon, 0, sizeof(this->icon));
    this->card[0].present = 1;
    this->card[1].present = 1;
    printf("SaveData size = %d\n", sizeof(CSaveData));
}

void CMemoryCardAccess::SetIconData(MC_ICON_DATA *icon) {
    memcpy(&this->icon.view, &icon->view, sizeof(MC_ICON_FILE));
    memcpy(&this->icon.copy, &icon->copy, sizeof(MC_ICON_FILE));
    memcpy(&this->icon.del, &icon->del, sizeof(MC_ICON_FILE));
}

int CMemoryCardAccess::GetFuncNo() {
    return this->func_no;
}

int CMemoryCardAccess::Step() {
    int result;
    int target_file;

    result = 0;

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
            if (error.code == MC_ERROR_NONE) {
                target_file = file_no;
            } else {
                target_file = error.file_no;
            }

            result = DeleteFile(target_file);
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
    }

    if (result == 1) {
        step = 0;
        SetFuncNo(MC_OPERATION_IDLE);
    } else {
        McError(result);
    }

    return result;
}

void CMemoryCardAccess::SetVersion(char *version) {
    strcpy(this->version, version);
}

char *CMemoryCardAccess::GetVersion() {
    return this->version;
}

int CMemoryCardAccess::GetMsgNo(int msg_no) {
    int result = 0;

    switch (this->GetFuncNo()) {
        case MC_OPERATION_GET_DIR:
            result = msg_no + 5;
            break;
        case MC_OPERATION_MAKE_DIR:
            result = msg_no + 11;
            break;
        case MC_OPERATION_GET_ALL_SAVE_FILE_INFO:
            result = msg_no + 5;
            break;
        case MC_OPERATION_SAVE:
        case MC_OPERATION_SAVE_CONFIG:
            result = msg_no + 11;
            break;
        case MC_OPERATION_LOAD:
        case MC_OPERATION_LOAD_CONFIG:
            result = msg_no + 21;
            break;
        case MC_OPERATION_FORMAT:
            result = msg_no + 35;
            break;
        case MC_OPERATION_UNFORMAT:
            result = msg_no + 39;
            break;
        case MC_OPERATION_DELETE:
            result = msg_no + 41;
            break;
        case MC_OPERATION_UNK_8:
            result = msg_no + 43;
            break;
    }

    return result;
}

int CMemoryCardAccess::McError(int result) {
    MC_ERROR_INFO *info = &this->error;

    switch (result) {
        case sceMcResNoFormat:
        case sceMcResFailDetect:
            printf("mc is unformat\n");
            info->code = MC_ERROR_UNFORMATTED;
            break;
        case sceMcResFullDevice:
            printf("memory is over or noting, break!\n");
            info->code = MC_ERROR_FULL;
            break;
        case sceMcResNoEntry:
            printf("file not open or not exist\n");
            break;
        case sceMcResDeniedPermit:
            printf("not open by write mode \n");
            break;
        case sceMcResFailReplace:
            printf("write failed\n");
            break;
    }

    if (result < -10) {
        info->code = MC_ERROR_NOT_CARD;
        printf("not memory card or (read write)error = %d\n", result);
    }

    if (result < 0) {
        printf("result = %d\n", result);
        info->func_no = this->GetFuncNo();
        info->step = this->step;
        printf("func = %d\n", info->func_no);
        printf("phase = %d\n", info->step);
        info->file_no = this->file_no;
    }

    return 0;
}

char AtoraVibeTextureName[] __attribute__((section(".rodata"))) = "stayframe";
