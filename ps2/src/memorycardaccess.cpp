#include "memorycardaccess.hpp"

#include <cstdio>
#include <cstring>

#include "dsgeorama.hpp"
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
#ifdef PAL
    strcpy(this->dir_name, "BESCES-50295dkcloud");
#else
    switch (GetMenuLangFlag()) {
        case LANG_JAPANESE:
            strcpy(this->dir_name, "BISCPS-15004dkcloud");
            break;
        case LANG_ENGLISH_US:
        default:
            strcpy(this->dir_name, "BASCUS-97111dkcloud");
            break;
    }

#endif
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

int CMemoryCardAccess::InitForMC() {
    int status = sceMcInit();
    this->Initialize();
    InitSaveFileInfoTbl();

    int result;

    switch (status) {
        case sceMcIniSucceed:
            printf("Memory Card Initialized Successed!!\n\n\n\n");
            result = 0;
            break;
        case sceMcIniErrKernel:
            result = 1;
            printf("Initialized Failed!!\n");
            break;
        case sceMcIniOldMcserv:
            result = 1;
            printf("mcserv.irx is old file\n");
            break;
        case sceMcIniOldMcman:
            result = 1;
            printf("mcman.irx is old file \n");
            break;
    }

    return result;
}

void CMemoryCardAccess::SetBuff(char *buffer) {
    char *data;
    char *sum;
    u32   i;
    char  total;

    buffer = (char *) ((((int) buffer >> 6) + 1) << 6);
    this->save_buffer = (CSaveData *) buffer;
    memcpy(this->save_buffer, SaveData, 0x131C0);
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
    // The Demon Shaft georama's record follows retail's image.
    DsgWriteSave((char *) this->save_buffer + 0x136A7);
    sum = (char *) this->save_buffer + 0x136A7 + DSG_SAVE_SIZE;
#endif
    this->read_buffer = (char *) ((((int) sum >> 6) + 1) << 6);
    this->load_buffer = this->read_buffer;
}

void CMemoryCardAccess::SetIconData(MC_ICON_DATA *icon) {
    memcpy(&this->icon.view, &icon->view, sizeof(MC_ICON_FILE));
    memcpy(&this->icon.copy, &icon->copy, sizeof(MC_ICON_FILE));
    memcpy(&this->icon.del, &icon->del, sizeof(MC_ICON_FILE));
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

    memset(&this->icon_sys, 0, sizeof(sceMcIconSys));
    strcpy(this->icon_sys.head, "PS2D");
    // "Dark Cloud Data" in full-width Shift-JIS.
    strcpy((char *) this->icon_sys.title_name, "\x82\x63\x82\x81\x82\x92\x82\x8B\x81\x40\x82\x62\x82\x8C\x82\x8F"
                                               "\x82\x95\x82\x84\x81\x40\x82\x63\x82\x81\x82\x94\x82\x81");
    this->icon_sys.nl_offset = 20;
    this->icon_sys.trans_rate = 0x60;
    // Only the first entry of each table is copied; the rest of the image stays zero.
    memcpy(this->icon_sys.bg_color, bg_color, 16);
    memcpy(this->icon_sys.light_dir, light_dir, 16);
    memcpy(this->icon_sys.light_color, light_color, 16);
    memcpy(this->icon_sys.ambient, ambient, 16);
    strcpy(this->icon_sys.fname_view, this->icon.view.name);
    strcpy(this->icon_sys.fname_copy, this->icon.copy.name);
    strcpy(this->icon_sys.fname_del, this->icon.del.name);
}

void CMemoryCardAccess::SetFuncNo(int func_no) {
    this->func_no = func_no;
    this->step = 0;

    if (func_no == MC_OPERATION_IDLE) {
        this->idle_code = 0x3D;
    }

    sceMcSync(MC_NOWAIT, NULL, NULL);
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

int CMemoryCardAccess::SearchMcType() {
    static int    old_format;
    MC_CARD_INFO *card;
    int           cmd;
    int           status;

    card = &this->card[this->port];

    if (this->step == 0) {
        old_format = card->formatted;
    }

    switch (this->step % 2) {
        case 0:
            memset(&card->present, 0, sizeof(card->present));

            if (sceMcGetInfo(this->port, 1, &card->type, &card->free_size, &card->formatted) == 0) {
                this->step++;
            } else {
                this->step += 2;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 1) {
                sceMcSync(MC_NOWAIT, NULL, NULL);
                break;
            }

            card->present = 1;
            card->result = status;

            switch (status) {
                case sceMcResSucceed:
                    break;
                case sceMcResChangedCard:
                    card->formatted = 1;
                    break;
                case sceMcResNoFormat:
                    card->formatted = 0;
                    break;
                default:
                    if (status < -10) {
                        card->present = 0;
                    }

                    break;
            }

            this->step++;

            if (this->step >= 2 && card->present && card->formatted) {
                if (old_format && card->formatted) {
                    card->format_change = 0;
                }

                if (!old_format && card->formatted) {
                    card->format_change = 1;
                }

                return 1;
            }

            if (this->step >= 16) {
                if (old_format && card->formatted) {
                    card->format_change = 0;
                }

                if (!old_format && !card->formatted) {
                    card->format_change = 0;
                }

                if (!old_format && card->formatted) {
                    card->format_change = 1;
                }

                if (old_format && !card->formatted) {
                    card->format_change = -1;
                }

                return 1;
            }

            break;
    }

    return 0;
}

int CMemoryCardAccess::GetDir() {
    MC_CARD_INFO *card;
    char          name[0x40];
    char          path[0x40];
    char          dir[0x80];
    char          pattern[0x48];
    int           cmd;
    int           result;

    card = &this->card[this->port];

    switch (this->step) {
        case 0:
            strcpy(name, this->dir_name);
            strcpy(path, "/");
            strcat(path, this->dir_name);
            InitSaveFileInfoTbl();
            card->dir_exists = 0;
            this->step++;
            break;
        case 1:
            strcpy(dir, "/");
            strcat(dir, this->dir_name);

            if (sceMcChdir(this->port, 1, dir, this->current_dir) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 2:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 0xC) {
                return -1;
            }

            if (result < 0) {
                if (result == sceMcResNoEntry || result == sceMcResNoFormat) {
                    return 1;
                }

                if (result < -9) {
                    return -1;
                }
            }

            card->dir_exists = 1;
            result = 0;
            strcpy(pattern, this->file_name);
            strcat(pattern, "*");

            if (sceMcGetDir(this->port, 1, pattern, 0, 15, SaveFileInfo) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 0xD || (cmd == 0xD && result < 0)) {
                return -1;
            }

            card->dir_entries = result;
            return 1;
    }

    return 0;
}

int CMemoryCardAccess::LoadSysConfig() {
    int port_no;
    int cmd;
    int result;
    int i;

    port_no = this->port;

    switch (this->step) {
        case 0:
            if (sceMcOpen(port_no, 1, this->dir_name, 1) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 2 || (cmd == 2 && result < 0)) {
                return -1;
            }

            this->fd = result;
            this->transfer_size = 0x40;
            memset(&sys_config, 0, this->transfer_size + 0x40);
            sys_config.values[17] = 1;
            this->transferred = 0;

            if (sceMcRead(this->fd, &sys_config, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 2:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 5 || (cmd == 5 && result < 0)) {
                return -1;
            }

            this->transferred += result;

            if (this->transferred < this->transfer_size) {
                break;
            }

            if (sceMcClose(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && result < 0)) {
                return -1;
            }

            if (SaveData->InvertConfig(&sys_config) != 0) {
                for (i = 0; i < 12; i++) {
                }
            }

            return 1;
    }

    return 0;
}

int CMemoryCardAccess::SaveSysConfig() {
    int port_no;
    int i;
    int cmd;
    int result;

    port_no = this->port;

    switch (this->step) {
        case 0:
            if (sceMcOpen(port_no, 1, this->dir_name, 2) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 2 || (cmd == 2 && result < 0)) {
                return -1;
            }

            this->fd = result;
            this->fd = result;
            this->transferred = 0;
            this->transfer_size = 0x40;

            for (i = 0; i < 18; i++) {
                printf("config%d = %d\n", i, sys_config.values[i]);
            }

            if (sceMcWrite(this->fd, &sys_config, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 2:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 6 || (cmd == 6 && result < 0)) {
                return -1;
            }

            this->transferred += result;

            if (this->transferred < this->transfer_size) {
                break;
            }

            if (sceMcFlush(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 0xA || (cmd == 0xA && result < 0)) {
                return -1;
            }

            if (sceMcClose(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 4:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && result < 0)) {
                return -1;
            }

            return 1;
    }

    return 0;
}

int CMemoryCardAccess::Write() {
    char buffer[0x198];
    int  result;
    int  cmd;

    result = 0;
    sceMcChdir(this->port, 1, "/", NULL);
    sceMcSync(MC_WAIT, &cmd, &result);
    sceMcOpen(this->port, 1, "test", 0x202);
    sceMcSync(MC_WAIT, &cmd, &result);
    this->fd = result;
    sceMcWrite(this->fd, buffer, 0x76F800);
    sceMcSync(MC_WAIT, &cmd, &result);
    sceMcFlush(this->fd);
    sceMcSync(MC_WAIT, &cmd, &result);
    sceMcClose(this->fd);
    sceMcSync(MC_WAIT, &cmd, &result);
    return 1;
}

int CMemoryCardAccess::Convert() {
    int  result;
    int  cmd;
    char src_name[0x80];
    char dst_name[0x80];
    char buffer[0x19000];
    int  i;
    int  fd;

    sceMcChdir(this->port, 1, "/", "");
    sceMcSync(MC_WAIT, &cmd, &result);
    printf("move to dir\n");

    while (this->MakeDir() == 0) {
    }

    printf("create new dir\n");

    for (i = 0; i < MC_SAVE_FILE_MAX; i++) {
        strcpy(src_name, "BASCUS-97112dkcloud/");
        strcat(src_name, this->file_name);
        strcat(src_name, "%d");
        sprintf(src_name, src_name, i);
        strcpy(dst_name, "BASCUS-97111dkcloud/");
        strcat(dst_name, this->file_name);
        strcat(dst_name, "%d");
        sprintf(dst_name, dst_name, i);

        if (sceMcOpen(this->port, 1, src_name, 1) != 0) {
            continue;
        }

        sceMcSync(MC_WAIT, &cmd, &result);

        if (cmd != 2 || (cmd == 2 && result < 0)) {
            continue;
        }

        fd = result;
        printf("fname = %s\n", src_name);
        memset(this->load_buffer, 0, 0x136E7);
        sceMcRead(fd, this->load_buffer, 0x136A7);
        sceMcSync(MC_WAIT, &cmd, &result);
        sceMcClose(fd);
        sceMcSync(MC_WAIT, &cmd, &result);

        if (sceMcOpen(this->port, 1, dst_name, 0x202) != 0) {
            continue;
        }

        sceMcSync(MC_WAIT, &cmd, &result);

        if (cmd != 2 || (cmd == 2 && result < 0)) {
            continue;
        }

        fd = result;
        printf("fname = %s\n", dst_name);
        memset(buffer, 0, 0x136E7);
        sceMcWrite(fd, this->load_buffer, 0x136A7);
        sceMcSync(MC_WAIT, &cmd, &result);
        sceMcFlush(fd);
        sceMcSync(MC_WAIT, &cmd, &result);
        printf("save %d\n", i);
        sceMcClose(fd);
        sceMcSync(MC_WAIT, &cmd, &result);
    }

    return 1;
}

int CMemoryCardAccess::MakeDir() {
    int        status = 0;
    static int iconNo = -1;
    char       path[128];
    int        cmd;

    switch (this->step) {
        case 0:
            if (sceMcMkdir(this->port, 1, this->dir_name) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 0xB) {
                break;
            }

            if (cmd == 0xB && status < 0) {
                return -1;
            }

            strcpy(path, this->dir_name);
            strcat(path, "/");

            if (sceMcChdir(this->port, 1, path, this->current_dir) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 2:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 0xC || (cmd == 0xC && status < 0)) {
                return -1;
            }

            cmd = sceMcOpen(this->port, 1, this->dir_name, 0x202);

            if (cmd == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 2 || (cmd == 2 && status < 0)) {
                return -1;
            }

            this->fd = status;
            this->transferred = 0;
            this->transfer_size = sizeof(mcdmybuf);
            memset(mcdmybuf, 0, sizeof(mcdmybuf));
            mcdmybuf[0x11] = 1;

            if (sceMcWrite(this->fd, mcdmybuf, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 4:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 6 || (cmd == 6 && status < 0)) {
                return -1;
            }

            this->transferred += status;
            printf("read size = %d\n", this->transfer_size);

            if (this->transferred < this->transfer_size) {
                break;
            }

            if (sceMcClose(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 5:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && status < 0)) {
                return -1;
            }

            if (sceMcOpen(this->port, 1, "icon.sys", 0x202) == 0) {
                iconNo = -1;
                this->step++;
            } else {
                return -1;
            }

            break;
        case 6:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 2 || (cmd == 2 && status < 0)) {
                printf("open error = %d\n", status);
                return -1;
            }

            this->fd = status;
            this->transferred = 0;
            this->transfer_size = sizeof(sceMcIconSys);

            if (sceMcWrite(this->fd, &this->icon_sys, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 7:
        case 11:
        case 15:
        case 19:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 6 || (cmd == 6 && status < 0)) {
                return -1;
            }

            this->transferred += status;

            if (this->transferred < this->transfer_size) {
                break;
            }

            if (sceMcFlush(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 8:
        case 12:
        case 16:
        case 20:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 0xA || (cmd == 0xA && status < 0)) {
                return -1;
            }

            cmd = sceMcClose(this->fd);

            if (cmd == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 9:
        case 13:
        case 17:
        case 21:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && status < 0)) {
                return -1;
            }

            iconNo++;

            if (iconNo < 3) {
                cmd = sceMcOpen(this->port, 1, (&this->icon.view)[iconNo].name, 0x203);

                if (cmd == 0) {
                    this->step++;
                } else {
                    return -1;
                }
            } else {
                return 1;
            }

            break;
        case 10:
        case 14:
        case 18:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 2 || (cmd == 2 && status < 0)) {
                return -1;
            }

            this->fd = status;
            this->transferred = 0;
            this->transfer_size = (&this->icon.view)[iconNo].size;
            cmd = sceMcWrite(this->fd, (&this->icon.view)[iconNo].data, this->transfer_size);

            if (cmd == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
    }

    return 0;
}

int CMemoryCardAccess::GetSaveFileInfoFromMc(int file_no) {
    char           name[0x28];
    int            cmd;
    int            result;
    char          *data;
    SAVEDATA_INFO *info;
    int            i;
    MC_ERROR_INFO *error;
    int            dungeon;

    info = &this->file_info[file_no];

    switch ((this->step - 1) % 4) {
        case 0:
            strcpy(name, this->file_name);
            strcat(name, "%d");
            sprintf(name, name, file_no);

            if (sceMcOpen(this->port, 1, name, 1) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 2) {
                return -1;
            }

            if (result < 0) {
                if (result == sceMcResNoEntry || result == sceMcResNoFormat) {
                    printf("not found\n");
                    info->state = 0;
                    this->step += 3;
                    return 1;
                }

                return -1;
            }

            this->fd = result;
            this->transferred = 0;
            this->transfer_size = 0x136A7;
            this->error.retry_count = 0;
            memset((void *) this->read_buffer, 0, this->transfer_size);

            if (sceMcRead(this->fd, this->read_buffer, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 2:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 5 || (cmd == 5 && result < 0)) {
                return -1;
            }

            if (this->transferred < this->transfer_size) {
                this->error.retry_count++;

                if (this->error.retry_count > 100) {
                    printf("getinfo read error \n");

                    if (sceMcClose(this->fd) == 0) {
                        this->step++;
                    } else {
                        return -1;
                    }

                    break;
                }
            }

            this->transferred += result;

            if (this->transferred < this->transfer_size) {
                break;
            }

            if (sceMcClose(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && result < 0)) {
                return -1;
            }

            i = 0;
            data = this->read_buffer;

            while (i < this->transfer_size) {
                if (memcmp(data, "darkcloud", 9) == 0) {
                    i = this->transfer_size;
                    break;
                }

                data++;
                i++;
            }

            error = &this->error;

            if (*data == 'd' && strcmp(this->GetVersion(), data) != 0) {
                this->step++;
                error->step = this->step;
                error->file_no = file_no;
                error->code = MC_ERROR_VERSION;
                return 1;
            }

            if (this->transferred < this->transfer_size) {
                error->code = MC_ERROR_SHORT_READ;
                error->file_no = this->file_no;
                this->step++;
                printf("not enough size\n");
                return 1;
            }

            info->state = 1;
            info->file_no = file_no + 1;
            memcpy(info->name, ((CSaveData *) this->read_buffer)->GetCharaName(0), sizeof(info->name));
            info->map_no = ((CSaveData *) this->read_buffer)->map_no;
            info->play_time = ((CSaveData *) this->read_buffer)->GetPlayTime();
            info->party_size = ((CSaveData *) this->read_buffer)->GetDngStatus()->GetPartySize();
            info->quest_total = 0;

            for (dungeon = 0; dungeon < 6; dungeon++) {
                info->quest_total += ((CSaveData *) this->read_buffer)->QuestDungeon(dungeon, 0);
            }

            info->quest_total += ((CSaveData *) this->read_buffer)->QuestDungeon(6, 0);

            if (info->quest_total > 9999) {
                info->quest_total = 9999;
            }

            this->step++;
            return 1;
    }

    return 0;
}

int CMemoryCardAccess::GetAllSaveFileInfo() {
    int result;
    int file_no;
    int status;

    switch (this->step) {
        case 0:
            if (sceMcSync(MC_NOWAIT, NULL, &result) == 0) {
                break;
            }

            this->step++;
            memset(this->file_info, 0, sizeof(this->file_info));
        default:
            file_no = (this->step - 1) >> 2;
            status = this->GetSaveFileInfoFromMc(file_no);

            if (status == 1 && file_no + 1 >= MC_SAVE_FILE_MAX) {
                return 1;
            }

            if (status < 0) {
                return -1;
            }

            break;
    }

    return 0;
}

int CMemoryCardAccess::CheckFileNo(int file_no) {
    char name[0x20];

    if (file_no < 0) {
        return 0;
    }

    strcpy(name, this->file_name);
    strcat(name, "%d");
    sprintf(name, name, file_no);
    return GetOpenAttribute(name);
}

int CMemoryCardAccess::SaveToMc(int file_no) {
    char          name[0x20];
    char          pattern[0x48];
    int           result;
    int           cmd;
    int           status;
    int           i;
    MC_CARD_INFO *card;

    strcpy(name, this->file_name);
    strcat(name, "%d");
    sprintf(name, name, file_no);

    switch (this->step) {
        case 0:
            sys_config.values[17] = file_no;
            sys_config.values_copy1[17] = file_no;
            sys_config.values_copy2[17] = file_no;
            cmd = sceMcOpen(this->port, 1, name, GetOpenAttribute(name) ? 2 : 0x202);

            if (cmd == 0) {
                this->step++;
            } else {
                printf("cmd = %d\n", cmd);
                return -1;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 2) {
                printf("fd = %d\n", result);
                break;
            }

            if (result < 0) {
                return -1;
            }

            this->fd = result;
            this->transferred = 0;
#ifdef MOD_DEMON_SHAFT_GEORAMA
            this->transfer_size = 0x136A7 + DSG_SAVE_SIZE;
#else
            this->transfer_size = 0x136A7;
#endif

            if (sceMcWrite(this->fd, this->save_buffer, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 2:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 6 || (cmd == 6 && result < 0)) {
                return -1;
            }

            this->transferred += result;

            if (this->transferred < this->transfer_size) {
                break;
            }

            if (sceMcFlush(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 0xA || (cmd == 0xA && result < 0)) {
                return -1;
            }

            if (sceMcClose(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 4:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && result < 0)) {
                return -1;
            }

            strcpy(pattern, this->file_name);
            strcat(pattern, "*");

            if (sceMcGetDir(this->port, 1, pattern, 0, 15, SaveFileInfo) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 5:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 0xD || (cmd == 0xD && result < 0)) {
                return -1;
            }

            card = &this->card[this->port];
            card->dir_exists = 1;
            this->step++;
            break;
        case 6:
            cmd = sceMcOpen(this->port, 1, this->dir_name, 2);

            if (cmd == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 7:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 2) {
                return -1;
            }

            if (result < 0) {
                return -1;
            }

            this->fd = result;
            this->transferred = 0;
            this->transfer_size = 0x40;

            for (i = 0; i < 12; i++) {
            }

            if (sceMcWrite(this->fd, &sys_config, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 8:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 6 || (cmd == 6 && result < 0)) {
                return -1;
            }

            this->transferred += result;

            if (this->transferred < this->transfer_size) {
                break;
            }

            if (sceMcFlush(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 9:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 0xA || (cmd == 0xA && result < 0)) {
                return -1;
            }

            if (sceMcClose(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 10:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && result < 0)) {
                return -1;
            }

            return 1;
    }

    return status;
}

int CMemoryCardAccess::LoadFromMc(int file_no) {
    char  name[0x80];
    char  version[0x20];
    char  saved_version[0x20];
    int   result;
    int   cmd;
    int   ok;
    char *data;
    char *sum;
    char  total;
    int   i;

    switch (this->step) {
        case 0:
            strcpy(name, this->file_name);
            strcat(name, "%d");
            sprintf(name, name, file_no);

            if (sceMcOpen(this->port, 1, name, 1) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 2 || (cmd == 2 && result < 0)) {
                return -1;
            }

            this->fd = result;
#ifdef MOD_DEMON_SHAFT_GEORAMA
            this->transfer_size = 0x136A7 + DSG_SAVE_SIZE;
#else
            this->transfer_size = 0x136A7;
#endif
            memset(this->load_buffer, 0, this->transfer_size + 0x40);
            this->transferred = 0;

            if (sceMcRead(this->fd, this->load_buffer, this->transfer_size) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 2:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 5 || (cmd == 5 && result < 0)) {
                return -1;
            }

            this->transferred += result;

#ifdef MOD_DEMON_SHAFT_GEORAMA
            // A save from before the mod ends with retail's image.
            if (this->transferred < 0x136A7) {
                break;
            }
#else
            if (this->transferred < this->transfer_size) {
                break;
            }
#endif

            if (sceMcClose(this->fd) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &result) == 0) {
                break;
            }

            if (cmd != 3 || (cmd == 3 && result < 0)) {
                return -1;
            }

            ok = 1;
            data = this->load_buffer + sizeof(CSaveData);
            strcpy(version, this->GetVersion());
            memcpy(saved_version, data, sizeof(saved_version));

            if (saved_version[0] == version[0]) {
                if (strcmp(version, saved_version) == 0) {
                    sum = data + 0x20;
                    data = this->load_buffer;
                    total = 0;

                    for (i = 0; i < sizeof(CSaveData); i++) {
                        total += *data++;

                        if (i % 64 == 63) {
                            if (total != *sum) {
                                printf("save data break!!!\n");
                                ok = 0;
                                break;
                            }

                            sum++;
                            total = 0;
                        }
                    }
                }
            } else {
                ok = 0;
                printf("not load\n");
            }

            if (ok) {
                memcpy(SaveData, this->load_buffer, sizeof(CSaveData));
#ifdef MOD_DEMON_SHAFT_GEORAMA
                DsgReadSave(this->load_buffer + 0x136A7, this->transferred - 0x136A7);
#endif
                ((s32 *) SaveData->GetConfigData())[17] = file_no;
            } else {
                return -1;
            }

            return 1;
    }

    return 0;
}

int CMemoryCardAccess::FormatForMc() {
    int           result;
    MC_CARD_INFO *card;
    int           cmd;
    int           status;

    result = 0;
    card = &this->card[this->port];

    switch (this->step) {
        case 0:
            if (sceMcFormat(this->port, 1) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 0x10) {
                break;
            }

            if (status < 0) {
                printf("format failed\n");
                return -1;
            }

            printf("format finished\n");
            this->step++;
            break;
        case 2:
            if (sceMcGetInfo(this->port, 1, &card->type, &card->free_size, &card->formatted) == 0) {
                this->step++;
            } else {
                return -1;
            }

            break;
        case 3:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 1) {
                return -1;
            }

            card->present = 1;

            if (status < -9) {
                card->present = 0;
            }

            card->result = status;
            result = 1;
            break;
    }

    return result;
}

int CMemoryCardAccess::DeleteFile(int file_no) {
    int status;
    int cmd;

    switch (this->step) {
        case 0: {
            char name[64] = "darkcloud%d";

            sprintf(name, name, file_no);
            printf("delete:%s\n", name);

            if (sceMcDelete(this->port, 1, name) == 0) {
                this->step++;
            } else {
                sceMcSync(MC_NOWAIT, NULL, NULL);
            }

            break;
        }
        case 1:
            if (sceMcSync(MC_NOWAIT, &cmd, &status) == 0) {
                break;
            }

            if (cmd != 0xF) {
                break;
            }

            if (status < 0) {
                this->error.retry_count++;

                if (this->error.retry_count > 120) {
                    printf("delete error \n");
                    return 1;
                }

                break;
            }

            return 1;
    }

    return 0;
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

void CMemoryCardAccess::DmySync() {
    int cmd;
    int result;
    int running;

    running = 0;

    while (running == 0) {
        running = sceMcSync(MC_WAIT, &cmd, &result);
    }
}

int CMemoryCardAccess::McUnFormatForDebug() {
    int result;
    int cmd;
    int status;

    switch (this->step) {
        case 0:
            result = sceMcUnformat(this->port, 1);

            if (result == 0) {
                this->step++;
            } else {
                sceMcSync(MC_NOWAIT, &result, &status);
                printf("dmy sync\n");
            }

            break;
        case 1:
            result = sceMcSync(MC_NOWAIT, &cmd, &status);

            if (result == 0) {
                break;
            }

            if (cmd != 0x11) {
                break;
            }

            if (cmd == 0x11 && status < 0) {
                break;
            }

            this->card[this->port].formatted = 0;
            return 1;
    }

    return 0;
}

char AtoraVibeTextureName[] __attribute__((section(".rodata"))) = "stayframe";
