#pragma once

#include "common.h"

#include <libgraph.h>
#include <libpkt.h>

#include "texture.hpp"

/**
 * @file
 * Declares the calls that drive the loading screen while the disc is read.
 */

/**
 * Reports whether the loading-screen operation has ended.
 *
 * @mangled check_now_loading__Fv
 * @address 0x153F40
 * @size 0xC
 */
int check_now_loading();

/**
 * Clears the flag set after the loading screen completes a field.
 *
 * @mangled clear_now_loading_vsync_end__Fv
 * @address 0x153F50
 * @size 0xC
 */
void clear_now_loading_vsync_end();

/**
 * Reports whether the loading screen has completed a field.
 *
 * @mangled check_now_loading_vsync_end__Fv
 * @address 0x153F60
 * @size 0xC
 */
int check_now_loading_vsync_end();

/**
 * Waits for the loading screen to reach the end of a field.
 *
 * @mangled wait_now_loading_vsync__Fv
 * @address 0x153F70
 * @size 0x3C
 */
void wait_now_loading_vsync();

/**
 * Hides the active loading-screen overlay.
 *
 * @mangled now_loading_off__Fv
 * @address 0x153FB0
 * @size 0x10
 */
void now_loading_off();

/**
 * Loads the loading screen for one map, arms its vertical-sync callback, and
 * starts its fade.
 *
 * @mangled init_now_loading__Fi
 * @address 0x153FC0
 * @size 0x354
 */
void init_now_loading(int title_number);

/**
 * Draws the loading screen and advances its fade once per vertical sync.
 *
 * @mangled VSyncCallBack_Load__Fi
 * @address 0x154320
 * @size 0x450
 */
int VSyncCallBack_Load(int field);

/**
 * Uploads a named image and its palette to video memory and records where
 * they went.
 *
 * @mangled LoadTexture__FPcPUcP8CTextureii
 * @address 0x154770
 * @size 0x1D4
 */
void LoadTexture(char *name, unsigned char *archive, CTexture *texture, int image_address, int palette_address);

/**
 * Uploads one already-located image and its palette to video memory and
 * records where they went.
 *
 * @mangled LoadTexture__FP8TM2_headP8CTextureii
 * @address 0x154950
 * @size 0x1D4
 */
void LoadTexture(TM2_head *image, CTexture *texture, int image_address, int palette_address);

/**
 * Whether the loading screen has finished, set on arming and again when its fade ends.
 */
extern int end_flag;
/**
 * VIF1 packet buffer the loading screen builds each frame's sprites in.
 */
extern u_int now_load[2000];
/**
 * Double-buffered GS environment the loading screen draws with.
 */
extern sceGsDBuff nowloadDB;
/**
 * Packet builder writing into now_load.
 */
extern sceVif1Packet nlPacket;
/**
 * Loading-screen image, or the first logo on the boot logo screen.
 */
extern CTexture nl_tex;
/**
 * Second logo on the boot logo screen.
 */
extern CTexture nl_tex2;
/**
 * Whether the loading screen is being shown.
 */
extern int now_loding_flag;
/**
 * Vertical syncs left before the loading screen starts drawing.
 */
extern int nl_start_cnt;
/**
 * Current brightness of the loading screen's fade, 0 to 128.
 */
extern float col_cnt;
/**
 * Brightness step of the loading screen's fade, negated when it turns.
 */
extern float col_add;
/**
 * Index of the boot logo being shown.
 */
extern int logo_count;
/**
 * Map title number the loading screen was armed with.
 */
extern int map_title_no;
/**
 * Requests that the loading display be disabled.
 */
extern int now_loding_off;
/**
 * Whether the loading display has completed a field.
 */
extern int now_loading_vsync_end;
