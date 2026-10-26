#include "stubs/mglib.hpp"
#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 250

#include "mglib.hpp"
#include "types.h"

#include <eekernel.h>
#include <libdma.h>
#include <libpkt.h>
#include <sifdev.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataset.hpp"
#include "frame.hpp"
#include "mathutil.hpp"
#include "rect.hpp"
#include "texture.hpp"
#include "vutext.hpp"

int            DBuffID;
int            mgWaitVSync;
sceDmaChan    *DmaCH1;
sceDmaChan    *DmaCH2;
sceDmaChan    *DmaCH8;
sceVif1Packet *Vif1Packet;
int            VSyncField__2;
int            mgClearBackFlag;
/** Horizontal offset of the display from its default position. */
int mgAdjustX;
/** Vertical offset of the display from its default position. */
int mgAdjustY;
sceGsTex1  mgTEX1Env;
sceGsTest  mgPixelTest;
sceGsZbuf  mgZBuffer;
sceGsAlpha mgAlpha;
sceGsTexa  mgTexa;
/** First word of video memory past the frame and depth buffers. */
int mgTopVRAM;
/** Word address of the depth buffer in video memory. */
int mgZBufferAdr;

MG_PICKZ      mgPickZBuff[16];
sceGifTag     GiftagAD;
sceGsDBuff    mgDBuff;
sceVu0FVECTOR mgBackColor;

/* The window rectangle. Its four words are written back to front at static-initialisation time,
   which CRect_i_'s own constructor does not do -- every rectangle handed to a drawing call has
   them written front to back -- so the storage is declared through a type that writes them in
   that order, and postprocess_object.py gives it the name the rest of the game reaches it by. */
class CWindowRect {
public:
    s32 x;      /**< Left edge of the window in pixels. */
    s32 y;      /**< Top edge of the window in pixels. */
    s32 width;  /**< Width of the window in pixels. */
    s32 height; /**< Height of the window in pixels. */

    CWindowRect() {
        height = 0;
        width = 0;
        y = 0;
        x = 0;
    }
} __attribute__((aligned(16)));

CWindowRect mgWindowRectStore;
RenderInfo  mgRenderInfo;
/* The one texture manager the game has. It is built here rather than beside the textures because
   this is the unit that brings up the graphics. */
CTextureManager TexManager;

sceVu0FVECTOR        mgZeroVector;
sceVu0FVECTOR        mgUnitVector;
sceVu0FVECTOR        mgUnitVector2;
sceVu0FVECTOR        mgZeroVector2;
sceVu0FMATRIX        mgUnitMatrix;
sceVu0FMATRIX        mgZeroMatrix;
static sceVif1Packet vifpacket[2];
sceGsStoreImage      gs_simage;
static int           packetbuf[2];

static int vcount;
static int over_vsync;
static int (*VSyncCallBack2)(int);
static int call_back_active;
int MGGetVSyncCount();

void MGInit();

/* Waiting out a handler that is already running is what keeps the pointer from changing under it. */
void MGInitVSyncCallBack(int (*callback)(int));

/* Meant to lift each buffer to a quadword boundary and unable to: the remainder is taken over four
   bytes rather than sixteen, so it is zero for every address the caller can pass and the step never
   runs. */
void MGInitVif1Packet(u_long128 *buffer0, u_long128 *buffer1);

/* The frame the capture is part-way through: it decides which of the two read-back buffers a field
   lands in, and whether this call is the one that opens and closes the file. */
static int movie_count;

/* One screenshot, written where the host filesystem can take it: the two fields are read back out
   of the frame buffer separately, packed from 32-bit colour down to the 24 a TGA holds, and
   written row by row from the bottom up, which is the order that format stores. */
void StoreImage() {
    int            fd;
    int            i;
    int            j;
    int            k;
    char          *mark;
    sceVif1Packet *packet;
    u_char        *field0;
    u_char        *field1;
    static int     image_num = 0;
    static u_char  tga_head[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 2, 192, 1, 24, 0};
    char           name[128];
    char           directory[128];

    movie_count = 1;

    packet = &vifpacket[DBuffID];

    printf("%d %d %d\n", image_num, DBuffID, VSyncField__2);

    if (movie_count % 2) {
        strcpy(directory, "host0:y:/ps2/capture/");
        sprintf(name, "%si%5d.tga", directory, image_num++);

        /* The width the number is padded to is what makes the names sort, and a space is not a
           character a file name can carry. */
        for (mark = name; *mark; mark++) {
            if (*mark == ' ') {
                *mark = '0';
            }
        }

        fd = sceOpen(name, SCE_WRONLY | SCE_CREAT | SCE_TRUNC);
    }

    sceGsSetHalfOffset(&mgDBuff.draw0, 2048, 2048, 0);
    sceGsSetHalfOffset(&mgDBuff.draw1, 2048, 2048, 0);
    FlushCache(0);
    sceGsSwapDBuff(&mgDBuff, DBuffID);
    FlushCache(0);
    sceGsSyncPath(0, 0);
    DmaCH1->chcr.TTE = 1;
    sceDmaSend(DmaCH1, packet->pBase);
    sceGsSyncPath(0, 0);

    sceGsSetDefStoreImage(&gs_simage, !DBuffID * 2240, 10, 0, 0, 0, 640, SCREEN_HALF_HEIGHT);
    FlushCache(0);

    if (movie_count % 2) {
        sceGsExecStoreImage(&gs_simage, (u_long128 *) 0x02100000);
    } else {
        sceGsExecStoreImage(&gs_simage, (u_long128 *) 0x02300000);
    }

    sceGsSyncPath(0, 0);

    sceGsSetHalfOffset(&mgDBuff.draw0, 2048, 2048, 1);
    sceGsSetHalfOffset(&mgDBuff.draw1, 2048, 2048, 1);
    FlushCache(0);
    sceGsSyncPath(0, 0);
    sceGsSwapDBuff(&mgDBuff, DBuffID);
    FlushCache(0);
    sceGsSyncPath(0, 0);
    DmaCH1->chcr.TTE = 1;
    sceDmaSend(DmaCH1, packet->pBase);
    sceGsSyncPath(0, 0);

    sceGsSetDefStoreImage(&gs_simage, !DBuffID * 2240, 10, 0, 0, 0, 640, SCREEN_HALF_HEIGHT);
    FlushCache(0);

    if (movie_count % 2) {
        sceGsExecStoreImage(&gs_simage, (u_long128 *) 0x02200000);
    } else {
        sceGsExecStoreImage(&gs_simage, (u_long128 *) 0x02400000);
    }

    sceGsSyncPath(0, 0);

    if (movie_count % 2) {
        sceWrite(fd, tga_head, 18);

        for (i = 0; i < SCREEN_HALF_HEIGHT; i++) {
            field0 = (u_char *) 0x02100000 + ((((SCREEN_HALF_HEIGHT - 1) - i) * 640 >> 2) << 4);
            field1 = (u_char *) 0x02200000 + ((((SCREEN_HALF_HEIGHT - 1) - i) * 640 >> 2) << 4);

            /* Packed down in place and over the row it came from: three bytes out for every four
               in, and the two colour ends swapped, which is the order a TGA stores. */
            for (j = 0, k = 0; k < 2560; j += 3, k += 4) {
                u_char red = field0[k];
                field0[k] = field0[k + 2];
                field0[k + 2] = red;
                red = field1[k];
                field1[k] = field1[k + 2];
                field1[k + 2] = red;

                field0[j] = field0[k];
                field0[j + 1] = field0[k + 1];
                field0[j + 2] = field0[k + 2];
                field1[j] = field1[k];
                field1[j + 1] = field1[k + 1];
                field1[j + 2] = field1[k + 2];
            }

            sceWrite(fd, field1, 1920);
            sceWrite(fd, field0, 1920);
        }
    }

    if (movie_count % 2) {
        sceClose(fd);
    }

    movie_count++;
}

static int h_count;

void MGBeginFrame();

static void WaitVSync(int count) {
    int i, j;

    while (1) {
        int now = MGGetVSyncCount();

        /* The gap between polls has to cost something and do nothing, which is what a body that
           negates its own counter twice is: the count is the whole of the effect. */
        for (i = 0; i < 10; i++) {
            i = -i;
            i = -i;
        }

        if (count != now) {
            return;
        }

        for (j = 0; j < 10; j++) {
            j = -j;
            j = -j;
        }
    }
}

void MGEndFrame();

void MGFlipWaitVSync(int wait);

void MGAdjustScreen(int x, int y);

/* Everything a frame is drawn against, recomputed from the projection scale and the two clip planes.
   The depth it sets up runs from 16700000 at the near plane down to 1 at the far one, which is what
   both z solves below are for. */
void MGSetRenderInfo(float scale, float near_z, float far_z);

void MGSetProjection(float scale);

float MGGetProjection();

/* The scissor the whole frame is drawn inside, sent down the frame's own packet rather than kept
   in the draw environment: the rectangle is an origin and an extent, so the far edges are the near
   ones plus the width and the height. */
void MGSetWindowRect();

void MGSetWindowRect(CRect_i_ rect);

void MGSetPLight(sceVu0FMATRIX direction, sceVu0FMATRIX color);

void MGGetPLight(sceVu0FMATRIX direction, sceVu0FMATRIX color);

void MGSetAmbient(float *ambient);

void MGGetAmbient(float *ambient);

/* The two overloads below are the only callers and both pass the same two scales, so the vertical
   squeeze is the engine's rather than any caller's. */
static void MGSetViewMatrix_sub(sceVu0FMATRIX view, float x_scale, float y_scale) {
    sceVu0FMATRIX scale;
    sceVu0FMATRIX screen;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR flat;

    sceVu0CopyMatrix(mgRenderInfo.view, view);
    mgRenderInfo.position[0] = view[3][0];
    mgRenderInfo.position[1] = view[3][1];
    mgRenderInfo.position[2] = view[3][2];
    mgRenderInfo.position[3] = 0.0f;

    sceVu0UnitMatrix(scale);
    scale[0][0] = x_scale;
    /* The taller PAL frame takes a fixed vertical squeeze in place of the caller's. */
    scale[1][1] = 0.5f;
    MulMatrix(mgRenderInfo.view_scaled, scale, view);

    sceVu0UnitMatrix(screen);
    screen[1][1] = mgRenderInfo.scale[0];
    screen[0][0] = mgRenderInfo.scale[0];
    screen[2][2] = mgRenderInfo.offset[2];
    screen[3][3] = 0.0f;
    screen[2][1] = 2048.0f;
    screen[2][0] = 2048.0f;
    screen[2][3] = 1.0f;
    screen[3][2] = mgRenderInfo.scale[2];
    MulMatrix(mgRenderInfo.view_screen, screen, mgRenderInfo.view_scaled);
    sceVu0CopyMatrix(mgRenderInfo.screen, screen);

    direction[0] = -mgRenderInfo.view[0][2];
    direction[1] = -mgRenderInfo.view[1][2];
    direction[2] = -mgRenderInfo.view[2][2];
    direction[3] = 0.0f;
    flat[0] = direction[0];
    flat[1] = 0.0f;
    flat[2] = direction[2];
    flat[3] = 0.0f;
    sceVu0Normalize(flat, flat);
    mgRenderInfo.yaw = atan2f(flat[0], flat[2]);
    mgRenderInfo.pitch = -atan2f(direction[1], sqrtf(direction[0] * direction[0] + direction[2] * direction[2]));
    /* Both angles are already stored by here, so the direction is normalised into a local that
       goes out of scope with the call and nothing ever reads. */
    direction[3] = 0.0f;
    sceVu0Normalize(direction, direction);
}

void MGSetViewMatrix(sceVu0FMATRIX view);

void MGSetViewMatrix(sceVu0FMATRIX view, float *position);

void MGSetFogParm(float near_z, float far_z, u_char r, u_char g, u_char b, float far_fog, float near_fog);

/* A colour every component of which is negative is the request not to clear at all; anything else
   clears, and each component is clamped into the eight bits the clear is written through. */
void MGSetBGColor(float red, float green, float blue, float alpha);

void MGSetBGColor(float *color);

void MGGetBGColor(float *color);

void MGScisioringForce(int force);

sceVif1Packet *GetVif1Packet();

/* A point through the camera and onto the screen, in floats rather than on the Vector Unit: the
   view is applied, the perspective divide is done by hand, and the screen scale and offset the
   render info carries turn the result into the 12.4 fixed point the GS takes. The answer says
   whether the point is on screen, and a point behind the near plane is answered no and clamped to
   the plane rather than divided by. */
int MGRotTransPers(int *screen, float *position, int fog);

/* The same transform for something drawn flat against the screen: the vertical squeeze
   MGSetViewMatrix_sub bakes into the view is undone, the result is left in whole pixels rather than
   scaled into 12.4, and the pair is moved off the screen centre the offset put it at. */
int MGRotTransPers2D(int *screen, float *position, int fog);

/* Where one point falls outside the screen, as six bits rather than a yes or no: near, far, and one
   per side. The half-widths are the guard band's and not the window's, so a point off the edge is
   still inside until it is off by half a screen. A point at or behind the eye keeps its bits and
   has every side test reversed, which is what makes the caller's running AND mean the same thing
   for a corner in front and a corner behind. */
int MGClipVertex(float *position);

/* A bound rejected only when every one of its eight corners is outside the same side, which is what
   the running AND of the six bits says. Answering zero is answering "keep it": a corner that is
   inside everything has no bits at all and ends the walk on the spot. */
int MGClipBox(CBoxVu0 *box);

void MGDraw(CFrame *frame);

/* Each of the four takes the register it names down the frame's packet, and a null argument means
   the module's own copy of that register rather than nothing to send. */
void MGSetGsTEST(sceGsTest *test);

void MGSetGsZBUF(sceGsZbuf *zbuf);

void MGSetGsALPHA(sceGsAlpha *alpha);

void MGSetGsTEXA(sceGsTexa *texa);

/* The frame buffer read back as a texture, which is what every effect that samples the picture it
   is drawing over needs: a frame buffer page is 32 texture blocks, and the size is the largest the
   register can name rather than the buffer's own. */
void MGGetFBuffTex(sceGsTex0 *tex0);

void MGGetFBuffBackTex(sceGsTex0 *tex0);

/* A rectangle moved from one place in GS local memory to another, which is the transfer the GS
   does entirely on its own side: four registers down the frame's packet and no pixels through the
   bus. The second packet is a TEXFLUSH on its own, because the destination may be a texture the
   cache still holds the old contents of. */
void MGMoveImage(sceGsTex0 *src, const CRect_i_ &rect, sceGsTex0 *dst, int dst_x, int dst_y, int direction);

/* The same move as MGMoveImage where the two rectangles are not the same size, which the GS has no
   transfer for: this draws instead. One textured sprite carries the source rectangle onto the
   destination, so the scale is the sampler's, and every register it needs is set and put back
   inside the one packet. The destination is reached by pointing FRAME_1 at it, which is why the
   frame buffer's own texture is read before anything is written and FRAME_1 restored from it at
   the end. The two offsets stay statics rather than becoming literals because they were meant to
   be adjusted, and dyy carries the interlace correction on top of its own value. */
void MGStretchMoveImage(sceGsTex0 *src, const CRect_i_ &src_rect, sceGsTex0 *dst, const CRect_i_ &dst_rect);

/* Copies both interlaced fields into the destination, one 640-by-1 line at a time. */
void MGMoveFrameBuffImage(sceGsTex0 *dst, int x, int y, int direction);

void MGFillBox(const CRect_i_ &rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

/* Writes depth over the 640-by-224 field while leaving colour unchanged. */
void MGClearZBuffer(int mode);

/* Writes colour and zero depth over the 640-by-224 field in 16-pixel-wide strips. */
void MGClearScreen(u_char r, u_char g, u_char b, u_char a);

/* Draws a model's shadow with the fast shadow microprogram. */
void MGDrawShadowFast(CFrame *frame, float *position, float *normal);

/* Draws a model's shadow with the second fast shadow microprogram. */
void MGDrawShadowFast2(CFrame *frame, float *position, float *normal);

/* Draws a model's shadow while preserving the perspective and viewport matrices. */
void MGDrawShadow(CFrame *frame, float *position, float *normal);

/* Draws the shade pass for one model. */
void MGDrawShade(CFrame *frame);

static sceGsTex0 Shadow_SaveFrameBuff;
static sceGsTex0 Shadow_WorkTex;

void MGBeginDrawShadow(sceGsTex0 tex0);

void MGEndDrawShadow(u_char alpha);

