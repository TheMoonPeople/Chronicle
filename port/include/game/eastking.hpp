#pragma once

#include "common.h"

namespace dc::game {

class ClsMes;

/**
 * Message window that shows the East King event's text.
 */
extern ClsMes EastKingMsgCls;

/**
 * Background-music volume saved before the East King event.
 */
extern s16 PrevEastKingSndVol;

/**
 * Finalizes the East King event's asynchronously loaded texture and sound data.
 */
void EastKingTextureEnter();

/**
 * Saves the current background-music volume so it can be restored after the East King event.
 */
void GetPrevEastKingSndVol();

/**
 * Initializes one East King event and begins loading its resources into load_buffer.
 */
int InitEastKingEvent(int event_no, int *texture_block, u_long128 *load_buffer);

/**
 * Processes input and state transitions for the East King event; returns 1 once it has finished.
 */
int EastKingEventKey();

/**
 * Draws the East King event image, message, and fade overlay.
 */
void EastKingEventDraw();

} // namespace dc::game
