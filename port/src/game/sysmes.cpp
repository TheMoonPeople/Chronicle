#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 361

#include "sysmes.hpp"

#include <cstdio>
#include <cstring>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "mglib.hpp"
#include "snd.hpp"
#include "texture.hpp"

extern "C" int SkipSpace__FR9input_str__3(input_str &input) {
    char *text;
    int   position;

    text = input.data;
    position = input.pos;

    while (position < input.size) {
        if (CheckChar__Fc__3(text[position])) {
            break;
        }

        position++;
    }

    input.pos = position;

    if (position >= input.size) {
        return 0;
    }

    return 1;
}

extern "C" int CheckChar__Fc__3(char value) {
    int whitespace = 0;

    if (value == ' ') {
        whitespace = 1;
    }

    if (value == '\t') {
        whitespace = 1;
    }

    if (value == '\n') {
        whitespace = 1;
    }

    if (value == '\r') {
        whitespace = 1;
    }

    return !whitespace;
}

extern "C" void PreProcess__FR9input_str__2(input_str &input) {
    u8 *text = (u8 *) input.data;
    int position = 0;

    while (position < input.size) {
        if (text[position] == '/' && text[position + 1] == '/') {
            while (position < input.size) {
                if (text[position] == '\n' || text[position] == '\r') {
                    break;
                }

                text[position] = ' ';
                position++;
            }
        }

        if (text[position] == '/' && text[position + 1] == '*') {
            while (position < input.size) {
                if (text[position] == '*' && text[position + 1] == '/') {
                    text[position] = ' ';
                    text[position + 1] = ' ';
                    break;
                }

                text[position] = ' ';
                position++;
            }

            continue;
        }

        position++;
    }
}

/* The town's own message window, which everything with something to say to the player goes through
   and which the map editor's gate calls into before doing the same thing to a window of its own.
   It is set up once and asks for none of what the class can do — no speech bubble, no typing
   animation, no page marker — so a system message is one line of at most 26 characters that
   appears whole, sits under the picture for as many frames as its caller asked for, and goes. */
ClsMes SystemMessage;

/* The message file is read straight into this one off the disc, and the drive transfers by DMA
   into whole cache lines. */
short mes_data[24000] __attribute__((aligned(64)));

char mes_buff[65536];

int SystemMesNo = -1;

int SystemMesPosition = MES_POS_BOTTOM;

int SystemMesCount;

int SystemMesWait;

int SystemMesInputKey;

/* The language is spelled into the file name rather than chosen between names, and the read that
   fails falls back to one fixed file rather than to no text at all. */
void InitSystemMes() {
    short *data = mes_data;
    int    size;

    char name[64] = "meswin/system_a";

    if (LanguageCode > LANG_JAPANESE) {
        sprintf(name, "meswin/system_a_%d", LanguageCode);
    }

    strcat(name, ".mes");

    if (!LoadFile2(name, data, &size, 0)) {
        LoadFile("meswin/system_ae.bin", data, &size);
    }

    SystemMessage.Preset(MES_PRESET_SYSTEM);
    SystemMessage.fukidashi = false;
    SystemMessage.stay_frame = true;
    SystemMessage.text_rate = 0;
    SystemMessage.text_rate_set = 0;
    SystemMessage.style = MES_EDGE_NONE;
    SystemMessage.page_arrow = false;
    SystemMessage.fade_speed = 1.0f;
    SystemMessage.columns = 26;
    SystemMessage.rows = 1;
    SystemMessage.centre_rows = false;
    SystemMessage.style = MES_EDGE_BLACK;
    SystemMessage.tex_buff = mes_buff;
    SystemMessage.tex_block = 26;
    SystemMessage.SetBuff(data);
    SystemMessage.SetBuff_system(SystemMes);
}

void ClearSystemMes() {
    SystemMessage.MakeMesWin(-1);
    SystemMesNo = -1;
    SystemMesCount = 0;
    SystemMesInputKey = 0;
    SystemMesWait = 0;
}

int SystemMesCheck() {
    return SystemMesCount > 0;
}

/* A message counts down to nothing and is cleared at one rather than at zero, so the frame that
   would have shown it empty is the frame it goes away on. A message that asked for a button holds
   at two until the button is pressed. */
void SystemMesStep() {
    if (SystemMesWait > 0) {
        SystemMesWait--;
        return;
    }

    if (SystemMesCount == 1) {
        ClearSystemMes();
    }

    if (SystemMesNo > 0) {
        SystemMessage.Step();
    }

    if (SystemMesInputKey == 0 || SystemMesCount != 2 || GamePad.Down(PAD_CROSS)) {
        if (SystemMesCount > 0) {
            SystemMesCount--;
        }
    }
}

/* The window's own textures are put back every frame it is drawn, because whatever the game drew
   in between is free to have taken the video memory they were in. The line is centred by the width
   the text actually laid out to, which is not known until the window has been made. */
void SystemMesDraw() {
    int x;
    int columns;

    if (SystemMesWait > 0) {
        return;
    }

    if (SystemMesNo <= 0) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, SystemMessage.tex_block);
    columns = SystemMessage.text_columns;
    x = 320;
    x -= columns * 14 / 2;
    setbilinear(0);
    SystemMessage.text_x = x;
    SystemMessage.text_y = 345;
    SystemMessage.auto_pos = SystemMesPosition;
    SystemMessage.DrawMesWin();
}

/* Which message an item announces is decided by the band its number falls in, and the fourth band
   is the common items, whose message the item's own record names. */
void ItemGetMes(int item_no, int value, int frames, int input_key) {
    int           *number_ptr;
    int            message_no;
    COM_ITEM_INFO *info;

    int mes_args[4] = {-1, -1, -1, -1};
    int numbers[4] = {0, 0, 0, 0};

    number_ptr = numbers;
    numbers[0] = value;
    SystemMessage.value_show = false;
    SystemMessage.value_signed = true;

    if (value < 0) {
        numbers[0] = 0;
        SystemMessage.value_signed = false;
    }

    mes_args[0] = item_no + 100;
    message_no = 10;

    if (item_no >= ITEM_ATTACH_START) {
        message_no += 10;
    }

    if (item_no >= ITEM_REGULAR_WATER) {
        message_no += 10;
    }

    if (item_no >= ITEM_WEAPON_START) {
        message_no = 10;
        info = GetCommonItemInfo(item_no);

        if (!info) {
            printf("item_no: %d  is NULL\n", item_no);
        } else {
            mes_args[0] = info->msg + 100;
        }
    }

    SetSystemMes(message_no, frames, MES_POS_BOTTOM, input_key, mes_args, number_ptr);
}

void AtraGetMes(int map_no, int element, int frames) {
    int mes_args[4] = {-1, -1, -1, -1};

    mes_args[0] = GetAtraMsgNo(map_no, element);
    SetSystemMes(40, frames, MES_POS_BOTTOM, 1, mes_args, 0);
}

void TecGetMes(int technique, int frames) {
    int mes_args[4] = {-1, -1, -1, -1};

    mes_args[0] = technique + 500;
    SetSystemMes(50, frames, MES_POS_BOTTOM, 1, mes_args, 0);
}

void MaxUpMes(int value, int frames) {
    int numbers[4] = {0, -1, -1, -1};

    numbers[0] = value;
    SetSystemMes(51, frames, MES_POS_BOTTOM, 1, 0, numbers);
}

/* A party member out of range names the first one rather than saying nothing, which is the clamp
   the message further down makes one member along. */
void DeadMes(int member, int frames) {
    if (member < 0 || member > 5) {
        member = 0;
    }

    int mes_args[4] = {0, -1, -1, -1};

    mes_args[0] = member + 50;
    SetSystemMes(60, frames, MES_POS_BOTTOM, 1, mes_args, 0);
}

void AllDeadMes(int frames) {
    SetSystemMes(61, frames, MES_POS_BOTTOM, 1, 0, 0);
}

void NotGetAtraMes(int member, int frames) {
    if (member < 0 || member > 5) {
        member = 1;
    }

    int mes_args[4] = {0, -1, -1, -1};

    mes_args[0] = member + 50;
    SetSystemMes(70, frames, MES_POS_BOTTOM, 0, mes_args, 0);
}

void DontGetItemMes(int kind) {
    int message_no;

    message_no = 72;

    if (kind == 2) {
        message_no = 81;
    }

    SetSystemMes(message_no, 40, MES_POS_BOTTOM, 1, 0, 0);
}

/* A negative entry in either list means *leave the slot empty* rather than substitute a negative
   number, so every slot is cleared before it is written and only four of each are ever filled. */
void SetSystemMes(int message_no, int frames, int position, int input_key, int *args, int *numbers) {
    int i;

    SystemMesInputKey = input_key;

    if (args) {
        for (i = 0; i < 4; i++) {
            SystemMessage.mes_no[i] = -1;

            if (*args >= 0) {
                SystemMessage.mes_no[i] = *args;
            }

            args++;
        }
    }

    if (numbers) {
        for (i = 0; i < 4; i++) {
            SystemMessage.values[i] = -1;

            if (*numbers >= 0) {
                SystemMessage.values[i] = *numbers;
            }

            numbers++;
        }
    }

    SystemMessage.MakeMesWin(message_no);
    SystemMesNo = message_no;
    SystemMesCount = frames;
    SystemMesPosition = position;
}
