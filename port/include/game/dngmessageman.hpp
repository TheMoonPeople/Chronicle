#pragma once

#include "common.h"

namespace dc::game {

/**
 * State of the dungeon message window: which message is shown, for how
 * long, and the names and numbers inserted into its text.
 */
class CDngMessageMan {
public:
    s32 enabled;        /**< Whether the message window may be shown; cleared while an event runs. */
    s32 timer;          /**< Frames the message has left on screen. */
    s32 hide_timer;     /**< Frames the window stays hidden while a system message is shown. */
    s32 insert_mes_1;   /**< Message number of the first name inserted into the text. */
    s32 insert_mes_2;   /**< Message number of the second name inserted into the text. */
    s32 insert_value_1; /**< First number inserted into the text. */
    s32 insert_value_2; /**< Second number inserted into the text. */
    s32 steev_window;   /**< Whether the message is drawn in Steev's window instead of the standard one. */
    s32 steev_index;    /**< Which of the ten rotating Steev lines is shown next. */
    s32 message;        /**< Message the window shows, or -1 for none. */

    /**
     * Selects the warning for the current dungeon restriction zone and
     * resets its display timer.
     */
    void LimmitZone();

    /**
     * Starts or clears the low-water and no-water warnings from a
     * character's maximum, previous and current water.
     */
    void SetStatus_Dry(float water_max, float water_before, float water_now);

    /**
     * Queues the next of ten rotating messages, numbered from first, in
     * Steev's window when no message is on screen.
     */
    void SetSteevMes(int first);
};

/**
 * Dungeon message state shared by battle and the dungeon loop.
 */
extern "C" CDngMessageMan DngMessMan;

/**
 * The Japanese and American image path prefixes. NameExchg reads it as rows
 * of two indexed by language and takes the second of the row, so language 0
 * gives the American prefix; retail sizes the table for the one row.
 */
extern char *LanguageStr[7];

} // namespace dc::game
