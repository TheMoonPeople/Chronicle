#pragma once

#include "common.h"

class ClsMes;

/**
 * Message window that shows the East King event's text.
 */
extern ClsMes EastKingMsgCls;

/**
 * Stores the background-music volume saved before the East King event.
 */
extern s16 PrevEastKingSndVol;

/**
 * Finalizes the East King event's asynchronously loaded texture and sound data.
 *
 * @mangled EastKingTextureEnter__Fv
 * @address 0x00232830
 * @size 0x430
 */
void EastKingTextureEnter();

/**
 * Restores the background-music volume saved before the East King event.
 *
 * @mangled GetPrevEastKingSndVol__Fv
 * @address 0x00232E00
 * @size 0x24
 */
void GetPrevEastKingSndVol();

/**
 * Initializes one East King event and begins loading its resources.
 *
 * @mangled InitEastKingEvent__FiPiP1
 * @address 0x00232E30
 * @size 0x1A8
 */
int InitEastKingEvent(int event_no, int *texture_block, u_long128 *load_buffer);

/**
 * Processes input and state transitions for the East King event.
 *
 * @mangled EastKingEventKey__Fv
 * @address 0x00232FE0
 * @size 0x354
 */
int EastKingEventKey();

/**
 * Draws the East King event image, message, and fade overlay.
 *
 * @mangled EastKingEventDraw__Fv
 * @address 0x00233340
 * @size 0x1E0
 */
void EastKingEventDraw();

/**
 * Frame image of the manual window.
 */
extern char manual_frame_image[];
