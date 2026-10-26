#include "common.h"

#include <eekernel.h>
#include <libdma.h>
#include <libgraph.h>
#include <libpkt.h>

#include <cstdio>

#include "dataread.hpp"
#include "mainselect.hpp"
#include "mglib.hpp"
#include "nowload.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "tim2.hpp"

int end_flag = 1;

CTexture nl_tex;
CTexture nl_tex2;

int   now_loding_flag;
int   nl_start_cnt;
float col_cnt;
float col_add;
int   logo_count;
int        map_title_no;
int        now_loding_off;
int        now_loading_vsync_end;

int check_now_loading();

void clear_now_loading_vsync_end() {
    now_loading_vsync_end = 0;
}

int check_now_loading_vsync_end() {
    return now_loading_vsync_end;
}

void wait_now_loading_vsync();

void now_loading_off();

/**
 * Loads the loading screen for one map, arms its vertical-sync callback, and starts its fade.
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
 * Uploads a named image and its palette to video memory and records where they went.
 *
 * @mangled LoadTexture__FPcPUcP8CTextureii
 * @address 0x154770
 * @size 0x1D4
 */
void LoadTexture(char *name, u_char *archive, CTexture *texture, int image_address, int palette_address);

/**
 * Uploads one already-located image and its palette to video memory and records where they went.
 *
 * @mangled LoadTexture__FP8TM2_headP8CTextureii
 * @address 0x154950
 * @size 0x1D4
 */
void LoadTexture(TM2_head *image, CTexture *texture, int image_address, int palette_address);

