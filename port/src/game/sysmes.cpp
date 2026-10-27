#include "sysmes.hpp"

#include <array>
#include <format>
#include <memory>
#include <print>
#include <string>

#include "clsmes.hpp"
#include "dataread.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "mglib.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

int SkipSpace__FR9input_str__3(input_str &input) {
    const char *text = input.data;
    int         position = input.pos;

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

int CheckChar__Fc__3(char value) {
    const bool whitespace = value == ' ' || value == '\t' || value == '\n' || value == '\r';
    return !whitespace;
}

void PreProcess__FR9input_str__2(input_str &input) {
    char *text = input.data;
    int   position = 0;

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

namespace {

/* One line of at most 26 characters with no speech bubble, typing animation or page marker; it
   appears whole and stays for as many frames as its caller asked for. */
ClsMes SystemMessage;

// Read straight off the disc, which transfers by DMA into whole cache lines.
alignas(64) std::array<short, 24000> mes_data;

std::unique_ptr<char[]> mes_buff;

int SystemMesNo = -1;
int SystemMesPosition = MES_POS_BOTTOM;
int SystemMesCount;
int SystemMesWait;
int SystemMesInputKey;

} // namespace

/* The language is spelled into the file name, and a failed read falls back to one fixed file
   rather than to no text at all. */
void InitSystemMes() {
    short *data = mes_data.data();
    int    size = 0;

    std::string name = "meswin/system_a";

    if (LanguageCode > LANG_JAPANESE) {
        name = std::format("meswin/system_a_{}", LanguageCode);
    }

    name += ".mes";

    if (!LoadFile2(name.data(), data, &size, 0)) {
        LoadFile("meswin/system_ae.bin", data, &size);
    }

    if (mes_buff == nullptr) {
        mes_buff = std::make_unique<char[]>(65536);
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
    SystemMessage.tex_buff = mes_buff.get();
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

/* A message is cleared at one rather than at zero, so the frame that would have shown it empty is
   the frame it goes away on. A message that asked for a button holds at two until it is pressed. */
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

    if ((SystemMesInputKey == 0 || SystemMesCount != 2 || GamePad.Down(PAD_CROSS)) && SystemMesCount > 0) {
        SystemMesCount--;
    }
}

/* The window's textures are reloaded every frame because anything drawn in between may have taken
   their video memory. The line is centred by the width the text laid out to. */
void SystemMesDraw() {
    if (SystemMesWait > 0) {
        return;
    }

    if (SystemMesNo <= 0) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, SystemMessage.tex_block);
    const int columns = SystemMessage.text_columns;
    const int x = 320 - columns * 14 / 2;
    setbilinear(0);
    SystemMessage.text_x = x;
    SystemMessage.text_y = 345;
    SystemMessage.auto_pos = SystemMesPosition;
    SystemMessage.DrawMesWin();
}

/* The message is chosen by the band the item number falls in; common items name their message in
   their own record. */
void ItemGetMes(int item_no, int value, int frames, int input_key) {
    std::array<int, 4> mes_args = {-1, -1, -1, -1};
    std::array<int, 4> numbers = {0, 0, 0, 0};

    numbers[0] = value;
    SystemMessage.value_show = false;
    SystemMessage.value_signed = true;

    if (value < 0) {
        numbers[0] = 0;
        SystemMessage.value_signed = false;
    }

    mes_args[0] = item_no + 100;
    int message_no = 10;

    if (item_no >= ITEM_ATTACH_START) {
        message_no += 10;
    }

    if (item_no >= ITEM_REGULAR_WATER) {
        message_no += 10;
    }

    if (item_no >= ITEM_WEAPON_START) {
        message_no = 10;
        const COM_ITEM_INFO *info = GetCommonItemInfo(item_no);

        if (info == nullptr) {
            std::println("item_no: {}  is NULL", item_no);
        } else {
            mes_args[0] = info->msg + 100;
        }
    }

    SetSystemMes(message_no, frames, MES_POS_BOTTOM, input_key, mes_args.data(), numbers.data());
}

void AtraGetMes(int map_no, int element, int frames) {
    std::array<int, 4> mes_args = {GetAtraMsgNo(map_no, element), -1, -1, -1};
    SetSystemMes(40, frames, MES_POS_BOTTOM, 1, mes_args.data(), nullptr);
}

void TecGetMes(int technique, int frames) {
    std::array<int, 4> mes_args = {technique + 500, -1, -1, -1};
    SetSystemMes(50, frames, MES_POS_BOTTOM, 1, mes_args.data(), nullptr);
}

void MaxUpMes(int value, int frames) {
    std::array<int, 4> numbers = {value, -1, -1, -1};
    SetSystemMes(51, frames, MES_POS_BOTTOM, 1, nullptr, numbers.data());
}

// An out-of-range member names the first one; NotGetAtraMes clamps to the second instead.
void DeadMes(int member, int frames) {
    if (member < 0 || member > 5) {
        member = 0;
    }

    std::array<int, 4> mes_args = {member + 50, -1, -1, -1};
    SetSystemMes(60, frames, MES_POS_BOTTOM, 1, mes_args.data(), nullptr);
}

void AllDeadMes(int frames) {
    SetSystemMes(61, frames, MES_POS_BOTTOM, 1, nullptr, nullptr);
}

void NotGetAtraMes(int member, int frames) {
    if (member < 0 || member > 5) {
        member = 1;
    }

    std::array<int, 4> mes_args = {member + 50, -1, -1, -1};
    SetSystemMes(70, frames, MES_POS_BOTTOM, 0, mes_args.data(), nullptr);
}

void DontGetItemMes(int kind) {
    const int message_no = kind == 2 ? 81 : 72;
    SetSystemMes(message_no, 40, MES_POS_BOTTOM, 1, nullptr, nullptr);
}

// A negative entry in either list leaves that slot empty (-1) rather than storing the negative.
void SetSystemMes(int message_no, int frames, int position, int input_key, int *args, int *numbers) {
    SystemMesInputKey = input_key;

    if (args != nullptr) {
        for (int i = 0; i < 4; i++) {
            SystemMessage.mes_no[i] = -1;

            if (args[i] >= 0) {
                SystemMessage.mes_no[i] = args[i];
            }
        }
    }

    if (numbers != nullptr) {
        for (int i = 0; i < 4; i++) {
            SystemMessage.values[i] = -1;

            if (numbers[i] >= 0) {
                SystemMessage.values[i] = numbers[i];
            }
        }
    }

    SystemMessage.MakeMesWin(message_no);
    SystemMesNo = message_no;
    SystemMesCount = frames;
    SystemMesPosition = position;
}

} // namespace dc::game
