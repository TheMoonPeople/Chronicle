#pragma once

#include "common.h"

class ClsMes;

/**
 *          Carries a mutable text buffer and the parser's current position.
 */
struct input_str {
    char *data; /**< Text being parsed. */
    int   size; /**< Number of bytes available in the text. */
    int   pos;  /**< Current byte position in the text. */

    input_str() : size(0), pos(0) {}

    input_str(int unused) {}

    int get(int *value) {
        *value = (u_char) data[pos];
        pos++;

        if (size < pos) {
            return 0;
        }

        return 1;
    }
};

STATIC_ASSERT(sizeof(input_str) == 0xC);

/**
 * Message window used for transient system notifications.
 */
extern ClsMes SystemMessage;
/**
 * Aligned storage containing the system-message file.
 */
extern short mes_data[24000];
/**
 * Work buffer used while laying out a system message.
 */
extern char mes_buff[65536];
/**
 * Message number currently displayed, or -1 when none is active.
 */
extern int SystemMesNo;
/**
 * Screen-position preset used by the active message.
 */
extern int SystemMesPosition;
/**
 * Display frames remaining for the active message.
 */
extern int SystemMesCount;
/**
 * Frames before the active message begins stepping.
 */
extern int SystemMesWait;
/**
 * Whether the active message waits for confirm input.
 */
extern int SystemMesInputKey;

/**
 * Initializes the system-message window and loads its message file.
 */
void InitSystemMes();
/**
 * Clears the active system message.
 */
void ClearSystemMes();
/**
 * Reports whether a system message still has display time remaining.
 */
int SystemMesCheck();
/**
 * Advances the active system message and its input wait.
 */
void SystemMesStep();
/**
 * Draws the active system message.
 */
void SystemMesDraw();
/**
 * Shows the message for an acquired item.
 */
void ItemGetMes(int item_no, int value, int frames, int input_key);
/**
 * Shows the message for an acquired Atlamillia component.
 */
void AtraGetMes(int map_no, int element, int frames);
/**
 * Shows the message for an acquired technique.
 */
void TecGetMes(int technique, int frames);
/**
 * Shows a maximum-stat increase message.
 */
void MaxUpMes(int value, int frames);
/**
 * Shows the message for one defeated party member.
 */
void DeadMes(int member, int frames);
/**
 * Shows the message for the defeated party.
 */
void AllDeadMes(int frames);
/**
 * Shows why a party member cannot collect an Atlamillia component.
 */
void NotGetAtraMes(int member, int frames);
/**
 * Shows why an item cannot be collected.
 */
void DontGetItemMes(int kind);
/**
 * Configures and opens one system message.
 */
void SetSystemMes(int message_no, int frames, int position, int input_key, int *args, int *numbers);

/**
 * Skips the blanks in front of the next token; the system message copy of the script reader's SkipSpace.
 *
 * @mangled SkipSpace__FR9input_str__3
 * @address 0x15FD30
 * @size 0x94
 */
extern "C" int SkipSpace__FR9input_str__3(input_str &input);

/**
 * Whether a character can start a token; the system message copy of the script reader's CheckChar.
 *
 * @mangled CheckChar__Fc__3
 * @address 0x15FDD0
 * @size 0x60
 */
extern "C" int CheckChar__Fc__3(char value);

/**
 * Strips comments and line ends from a script; the system message copy of the script reader's PreProcess.
 *
 * @mangled PreProcess__FR9input_str__2
 * @address 0x15FE30
 * @size 0x11c
 */
extern "C" void PreProcess__FR9input_str__2(input_str &input);
