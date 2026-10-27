#pragma once

#include "common.h"

namespace dc::game {

/**
 * Carries a mutable text buffer and the parser's current position.
 */
struct input_str {
    char *data; /**< Text being parsed. */
    int   size; /**< Number of bytes available in the text. */
    int   pos;  /**< Current byte position in the text. */

    input_str() : size(0), pos(0) {}

    /**
     * Reads the byte at the current position and advances past it; returns 0 once that passes the end.
     */
    int get(int *value) {
        *value = static_cast<u_char>(data[pos]);
        pos++;

        if (size < pos) {
            return 0;
        }

        return 1;
    }
};

STATIC_ASSERT(sizeof(input_str) == 0xC);

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
 * Configures and opens one system message; negative entries in args or numbers leave that slot empty.
 */
void SetSystemMes(int message_no, int frames, int position, int input_key, int *args, int *numbers);

/**
 * Skips the blanks in front of the next token; returns 0 when the text runs out first.
 */
int SkipSpace__FR9input_str__3(input_str &input);

/**
 * Whether a character can start a token, i.e. is not a space, tab or line end.
 */
int CheckChar__Fc__3(char value);

/**
 * Blanks out // and block comments in a script in place.
 */
void PreProcess__FR9input_str__2(input_str &input);

} // namespace dc::game
