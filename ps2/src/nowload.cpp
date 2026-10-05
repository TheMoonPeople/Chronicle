#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
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

u_int         now_load[2000] __attribute__((aligned(64)));
sceGsDBuff    nowloadDB;
sceVif1Packet nlPacket;
CTexture      nl_tex;
CTexture      nl_tex2;

int   now_loding_flag;
int   nl_start_cnt;
float col_cnt;
float col_add;
int   logo_count;
/**
 * Frames the fade holds at full or zero brightness before it moves again.
 */
static int count;
int        map_title_no;
int        now_loding_off;
int        now_loading_vsync_end;
/**
 * Field parity the loading screen's vertical-sync callback last read from the GS.
 */
static int VSyncField;

int check_now_loading() {
    return end_flag;
}

void clear_now_loading_vsync_end() {
    now_loading_vsync_end = 0;
}

int check_now_loading_vsync_end() {
    return now_loading_vsync_end;
}

void wait_now_loading_vsync() {
    if (end_flag == 0) {
        clear_now_loading_vsync_end();

        do {

        } while (check_now_loading_vsync_end() == 0);
    }
}

void now_loading_off() {
    now_loding_off = 1;
}

/**
 * Loads the loading screen for one map, arms its vertical-sync callback, and starts its fade.
 *
 * @mangled init_now_loading__Fi
 * @address 0x153FC0
 * @size 0x354
 */
void init_now_loading(int title_number) {
    end_flag = 1;

    if (now_loding_off != 0) {
        now_loding_off = 0;
        return;
    }

#ifdef PAL
    u_char raw_archive[320000];
#else
    u_char raw_archive[64000];
#endif
    u_char *archive = raw_archive;
    int     archive_size;
    int     misalignment = (int) archive % 64;

    if (misalignment != 0) {
        archive += 64 - misalignment;
    }

    now_loding_flag = 0;

    char image_directory[64] = "img";

    if (LanguageCode > LANG_JAPANESE) {
        sprintf(image_directory, "img_%d", LanguageCode);
    }

    char path[64] = "";

    if (title_number < 5) {
        sprintf(path, "%s/mt0%d.tm2", image_directory, title_number + 1);
    } else if (title_number < 100) {
        sprintf(path, "%s/mt%d.tm2", image_directory, title_number);
    } else {
        sprintf(path, "%s/mt%d.tm2", image_directory, title_number - 99);
    }

    if (title_number == 0x321) {
        sprintf(path, "%s/title.img", image_directory);

        if (LoadFile2(path, archive, &archive_size, 0) == 0) {
            return;
        }

#ifdef PAL
        LoadTexture("SCElogo", archive, &nl_tex, 8000, 10000);
        LoadTexture("L5logo", archive, &nl_tex2, 9500, 10100);
#else
        LoadTexture("SCElogo", archive, &nl_tex, 0x1A40, 10000);
        LoadTexture("L5logo", archive, &nl_tex2, 8000, 0x2774);
#endif
    } else {
        if (path[0] == '\0') {
            return;
        }

        if (LoadFile2(path, archive, &archive_size, 0) == 0) {
            return;
        }

#ifdef PAL
        LoadTexture((TM2_head *) archive, &nl_tex, 8000, 9000);
#else
        LoadTexture((TM2_head *) archive, &nl_tex, 0x1A40, 8000);
#endif
    }

    map_title_no = title_number;
    nl_start_cnt = 20;
    sceGsSetDefDBuff(&nowloadDB, 0, 640, SCREEN_HALF_HEIGHT, 2, 0x31, 1);
    nowloadDB.clear0.rgbaq.R = 0;
    nowloadDB.clear0.rgbaq.G = 0;
    nowloadDB.clear0.rgbaq.B = 0;
    nowloadDB.clear0.rgbaq.A = 0x80;
    nowloadDB.clear1.rgbaq.R = 0;
    nowloadDB.clear1.rgbaq.G = 0;
    nowloadDB.clear1.rgbaq.B = 0;
    nowloadDB.clear1.rgbaq.A = 0x80;
    sceVif1PkInit(&nlPacket, now_load);
    col_cnt = 0.0f;
    col_add = 1.0f;
    count = 0;
    logo_count = 0;
    end_flag = 0;
    now_loading_vsync_end = 1;
    MGInitVSyncCallBack(VSyncCallBack_Load);
}

/**
 * Draws the loading screen and advances its fade once per vertical sync.
 *
 * @mangled VSyncCallBack_Load__Fi
 * @address 0x154320
 * @size 0x450
 */
int VSyncCallBack_Load(int field) {
    (void) field;

    if (end_flag) {
        now_loading_vsync_end = 1;
        return 0;
    }

    VSyncField = !((*(volatile u_long *) 0x12001000 >> 13) & 1);

    if (nl_start_cnt == 0) {
        sceDmaSync(DmaCH1, 0, 0);
        sceVif1PkReset(&nlPacket);

        if (map_title_no == 0x321) {
            if (logo_count == 0) {
#ifdef PAL
                // Languages past the first two show a full-screen logo image instead.
                if (LanguageCode >= LANG_ENGLISH_UK) {
                    set2DSprite(&nlPacket, &nl_tex, CRect_i_(0, 0x10, 0x280, 0x1C0), CRect_i_(0, 0, 0x280, 0x1C0), (u_char) (int) col_cnt);
                } else {
                    set2DSprite(&nlPacket, &nl_tex, CRect_i_(0x60, 0xC0, 0x1C0, 0x40), CRect_i_(0, 0, 0x1C0, 0x40), (u_char) (int) col_cnt);
                }
#else
                set2DSprite(&nlPacket, &nl_tex, CRect_i_(0x60, 0xC0, 0x1C0, 0x40), CRect_i_(0, 0, 0x1C0, 0x40), (u_char) (int) col_cnt);
#endif
            }

            if (logo_count == 1) {
                set2DSprite(&nlPacket, &nl_tex2, CRect_i_(0x100, 0xA0, 0x80, 0x80), CRect_i_(0, 0, 0x80, 0x80), (u_char) (int) col_cnt);
            }

            if (count == 0) {
                col_cnt += col_add * 2.0f;
            }

            if (col_cnt > 128.0f) {
#ifdef PAL_TIMING
                // The logo holds keep their NTSC duration at 50 fields a second.
                count = 183;
#else
                count = 220;
#endif
                col_cnt = 128.0f;
                col_add *= -1.0f;
            }

            if (col_cnt < 0.0f) {
#ifdef PAL_TIMING
                count = 83;
#else
                count = 100;
#endif
                col_cnt = 0.0f;
                col_add *= -1.0f;

                if (logo_count == 1) {
                    end_flag = 1;
                }

                logo_count++;
            }

            count--;

            if (count < 0) {
                count = 0;
            }
        } else {
            set2DSprite(&nlPacket, &nl_tex, CRect_i_(0x80, 0xA0, 0x180, 0x80), CRect_i_(0, 0, 0x180, 0x80), (u_char) (int) col_cnt);

            if (count == 0) {
                col_cnt += col_add;
            }

            if (col_cnt > 128.0f) {
                count = 120;
                col_cnt = 128.0f;
                col_add *= -1.0f;
            }

            if (col_cnt < 0.0f) {
                end_flag = 1;
                col_cnt = 0.0f;
            }

            count--;

            if (count < 0) {
                count = 0;
            }
        }

        sceVif1PkEnd(&nlPacket, 0);
        sceVif1PkTerminate(&nlPacket);
        iFlushCache(0);
        sceDmaSend(DmaCH1, nlPacket.pBase);
    }

    nl_start_cnt--;

    if (nl_start_cnt < 0) {
        nl_start_cnt = 0;
    }

    sceGsDrawEnv1 *draw = DBuffID != 0 ? &nowloadDB.draw1 : &nowloadDB.draw0;
    sceGsSetHalfOffset(draw, 0x800, 0x800, VSyncField);
    iSyncDCache(&nowloadDB, (u_char *) &nowloadDB + sizeof(nowloadDB));
    sceGsSwapDBuff(&nowloadDB, DBuffID);
    DBuffID = !DBuffID;
    sceDmaSync(DmaCH2, 0, 0);
    now_loading_vsync_end = 1;
    return 0;
}

/**
 * Uploads a named image and its palette to video memory and records where they went.
 *
 * @mangled LoadTexture__FPcPUcP8CTextureii
 * @address 0x154770
 * @size 0x1D4
 */
void LoadTexture(char *name, u_char *archive, CTexture *texture, int image_address, int palette_address) {
    SetTextureInfo(texture, name, archive);
    sceGsTex0     *tex0 = (sceGsTex0 *) &texture->tex0;
    sceGsLoadImage load;
    sceGsSetDefLoadImage(&load, (short) image_address, tex0->TBW, tex0->PSM, 0, 0, texture->width, texture->height);
    FlushCache(0);
    sceGsExecLoadImage(&load, (u_long128 *) texture->image[0]);

    if (texture->bpp == 0) {
        sceGsSetDefLoadImage(&load, (short) palette_address, 1, 0, 0, 0, 8, 2);
    }

    if (texture->bpp == 1) {
        sceGsSetDefLoadImage(&load, (short) palette_address, 1, 0, 0, 0, 16, 16);
    }

    FlushCache(0);

    if (texture->bpp < 2) {
        sceGsExecLoadImage(&load, (u_long128 *) texture->clut);
    }

    tex0->TBP0 = image_address;
    tex0->bits.tcc = 1;
    tex0->CBP = palette_address;
}

/**
 * Uploads one already-located image and its palette to video memory and records where they went.
 *
 * @mangled LoadTexture__FP8TM2_headP8CTextureii
 * @address 0x154950
 * @size 0x1D4
 */
void LoadTexture(TM2_head *image, CTexture *texture, int image_address, int palette_address) {
    SetTextureInfo(texture, "maptitle", image);
    sceGsTex0     *tex0 = (sceGsTex0 *) &texture->tex0;
    sceGsLoadImage load;
    sceGsSetDefLoadImage(&load, (short) image_address, tex0->TBW, tex0->PSM, 0, 0, texture->width, texture->height);
    FlushCache(0);
    sceGsExecLoadImage(&load, (u_long128 *) texture->image[0]);

    if (texture->bpp == 0) {
        sceGsSetDefLoadImage(&load, (short) palette_address, 1, 0, 0, 0, 8, 2);
    }

    if (texture->bpp == 1) {
        sceGsSetDefLoadImage(&load, (short) palette_address, 1, 0, 0, 0, 16, 16);
    }

    FlushCache(0);

    if (texture->bpp < 2) {
        sceGsExecLoadImage(&load, (u_long128 *) texture->clut);
    }

    tex0->TBP0 = image_address;
    tex0->bits.tcc = 1;
    tex0->CBP = palette_address;
}
