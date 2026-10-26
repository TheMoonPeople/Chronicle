#pragma once

#include "common.h"

#include <libdma.h>
#include <libgraph.h>
#include <libpkt.h>

class CRect_i_;
#include "renderinfo.hpp"

extern sceGifTag GiftagAD;

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CFrame;
class CTexture;

/**
 * Stores one deferred depth-buffer sample request and its result.
 */
struct MG_PICKZ {
    s32 enable; /**< Whether this request should be sampled at frame end. */
    s32 x;      /**< Horizontal screen coordinate to sample. */
    s32 y;      /**< Vertical screen coordinate to sample. */
    s32 z;      /**< Nearest sampled depth, or -1 when outside the valid area. */
};

/**
 * What the renderer draws the current frame with.
 *
 * @address 0x1C75520
 * @size 0x350
 */
extern "C" RenderInfo mgRenderInfo;

/**
 * @file
 * Declares the graphics library that the game draws every model through.
 */

/**
 * Sets the colour the frame is cleared to, from four floats in memory.
 *
 * @mangled MGSetBGColor__FPf
 * @address 0x12E210
 * @size 0x30
 * @unknownret
 */
void MGSetBGColor(float *color);

/**
 * Starts drawing the shadows, which go through a texture of their own.
 *
 * @mangled MGBeginDrawShadow__F9sceGsTex0
 * @address 0x1308A0
 * @size 0x288
 * @unknownret
 */
void MGBeginDrawShadow(sceGsTex0 tex0);

/**
 * Finishes drawing the shadows and lays them over the frame.
 *
 * @mangled MGEndDrawShadow__FUc
 * @address 0x130B30
 * @size 0x3B8
 * @unknownret
 */
void MGEndDrawShadow(unsigned char alpha);

/**
 * Sets the colour the frame is cleared to.
 *
 * @mangled MGSetBGColor__Fffff
 * @address 0x12E090
 * @size 0x174
 * @unknownret
 */
void MGSetBGColor(float red, float green, float blue, float alpha);

/**
 * Index of the display buffer, and of each visual's built block, in use this frame.
 */
extern int            DBuffID;
extern sceDmaChan    *DmaCH1;
extern sceDmaChan    *DmaCH2;
extern sceDmaChan    *DmaCH8;
extern sceVif1Packet *Vif1Packet;
/* Retail has two distinct `VSyncField` symbols (0x2A23C8 GLOBAL -- this one
 * -- and 0x2A25DC LOCAL, in a still-raw TU); the `__2` suffix is the
 * disassembly dump's disambiguation, kept here to avoid a link collision. */
extern s32           VSyncField__2;
extern sceGsTex1     mgTEX1Env;
extern s32           mgClearBackFlag;
extern MG_PICKZ      mgPickZBuff[16];
extern sceGsDBuff    mgDBuff;
extern sceVu0FVECTOR mgBackColor;

/**
 * All-zero matrix; passed as both point-light matrices it turns the lights off.
 */
extern sceVu0FMATRIX mgZeroMatrix;
extern CRect_i_      mgWindowRect;
extern sceGsTexa     mgTexa;
/** First word of video memory past the frame and depth buffers. */
extern int mgTopVRAM;
/** Word address of the depth buffer in video memory. */
extern int mgZBufferAdr;

/**
 * Returns the number of vertical syncs observed since initialization.
 */
int            MGGetVSyncCount();
void           MGClearScreen(u_char r, u_char g, u_char b, u_char a);
void           MGClearZBuffer(int mode);
sceVif1Packet *GetVif1Packet();

/**
 * Register state used when configuring the depth buffer.
 */
extern sceGsZbuf mgZBuffer;

/**
 * Register state used when configuring primitive blending.
 */
extern sceGsAlpha mgAlpha;

/**
 * Register state used when configuring pixel tests.
 */
extern sceGsTest mgPixelTest;

/**
 * Identity matrix shared by rendering helpers.
 */
extern float mgUnitMatrix[4][4];

/**
 * Reads the colour that lights every face of a model.
 *
 * @mangled MGGetAmbient__FPf
 * @address 0x12DD30
 * @size 0x28
 */
void MGGetAmbient(float *ambient);

/**
 * Sets the colour that lights every face of a model.
 *
 * @mangled MGSetAmbient__FPf
 * @address 0x12DD00
 * @size 0x2C
 */
void MGSetAmbient(float *ambient);

/**
 * Sets the direction and the colour of each of the four point lights.
 *
 * @mangled MGSetPLight__FPA4_fPA4_f
 * @address 0x12DC20
 * @size 0x90
 * @unknownret
 */
void MGSetPLight(float (*direction)[4], float (*color)[4]);

/**
 * Reads the lighting direction and colour matrices.
 *
 * @mangled MGGetPLight__FPA4_fPA4_f
 * @address 0x12DCB0
 * @size 0x48
 */
void MGGetPLight(float (*direction)[4], float (*color)[4]);

/**
 * Turns one world position into a screen position.
 *
 * @mangled MGRotTransPers__FPiPfi
 * @address 0x12E290
 * @size 0x250
 */
int MGRotTransPers(int *screen, float *position, int fog);

/**
 * Turns one world position into the two screen corners of a sprite that faces
 * the eye, and gives back 1 while the sprite is on the screen.
 *
 * @mangled MGRotTransPers3DSprite__FPiPiPfffi
 * @address 0x12E740
 * @size 0x250
 */
int MGRotTransPers3DSprite(int *top_left, int *bottom_right, float *position, float width, float height, int fog);

/**
 * Draws one model.
 *
 * @mangled MGDraw__FP6CFrame
 * @address 0x12ED80
 * @size 0x6C
 * @unknownret
 */
void MGDraw(CFrame *frame);

/**
 * Drops the shadow of one model onto the ground, with fewer faces than
 * `MGDrawShadow` uses.
 *
 * @mangled MGDrawShadowFast__FP6CFramePfPf
 * @address 0x1303B0
 * @size 0x120
 * @unknownret
 */
void MGDrawShadowFast(CFrame *frame, float *position, float *normal);

/**
 * Drops the shadow of one model onto the ground, with fewer faces than
 * `MGDrawShadow` uses.
 *
 * @mangled MGDrawShadowFast2__FP6CFramePfPf
 * @address 0x1304D0
 * @size 0x120
 * @unknownret
 */
void MGDrawShadowFast2(CFrame *frame, float *position, float *normal);

/**
 * Drops the shadow of one model onto the ground.
 *
 * @mangled MGDrawShadow__FP6CFramePfPf
 * @address 0x1305F0
 * @size 0x170
 * @unknownret
 */
void MGDrawShadow(CFrame *frame, float *position, float *normal);

/**
 * Draws the shade that lies on one model.
 *
 * @mangled MGDrawShade__FP6CFrame
 * @address 0x130760
 * @size 0x138
 * @unknownret
 */
void MGDrawShade(CFrame *frame);

/**
 * Names the texture that stands for the frame being drawn.
 *
 * @mangled MGGetFBuffTex__FP9sceGsTex0
 * @address 0x12F130
 * @size 0x70
 */
void MGGetFBuffTex(sceGsTex0 *tex0);

/**
 * Sets the register that names where the depth buffer lives.
 *
 * @mangled MGSetGsZBUF__FP9sceGsZbuf
 * @address 0x12EEC0
 * @size 0xC4
 * @unknownret
 */
void MGSetGsZBUF(sceGsZbuf *zbuf);

/**
 * Sets the register that names which pixels a primitive may write.
 *
 * @mangled MGSetGsTEST__FP9sceGsTest
 * @address 0x12EDF0
 * @size 0xC4
 * @unknownret
 */
void MGSetGsTEST(sceGsTest *test);

/**
 * Fills a rectangle of the frame with one colour.
 *
 * @mangled MGFillBox__FRC8CRect_i_UcUcUcUc
 * @address 0x12FA70
 * @size 0x2C0
 */
void MGFillBox(const CRect_i_ &rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

/**
 * Sets how far the fog reaches and what colour it is.
 *
 * @mangled MGSetFogParm__FffUcUcUcff
 * @address 0x12E010
 * @size 0x7C
 * @unknownret
 */
void MGSetFogParm(float near_z, float far_z, unsigned char r, unsigned char g, unsigned char b, float far_fog, float near_fog);

/**
 * Sets the register that names how a primitive blends with the frame.
 *
 * @mangled MGSetGsALPHA__FP10sceGsAlpha
 * @address 0x12EF90
 * @size 0xC4
 * @unknownret
 */
void MGSetGsALPHA(sceGsAlpha *alpha);

/**
 * Copies a rectangle of one texture into another.
 *
 * @mangled MGMoveImage__FP9sceGsTex0RC8CRect_i_P9sceGsTex0iii
 * @address 0x12F210
 * @size 0x21C
 * @unknownret
 */
void MGMoveImage(sceGsTex0 *src, const CRect_i_ &rect, sceGsTex0 *dst, int dst_x, int dst_y, int direction);

/**
 * Copies the frame being drawn into a texture.
 *
 * @mangled MGMoveFrameBuffImage__FP9sceGsTex0iii
 * @address 0x12F970
 * @size 0x100
 * @unknownret
 */
void MGMoveFrameBuffImage(sceGsTex0 *dst, int x, int y, int direction);

/**
 * Sets the matrix that puts the world in front of the eye, and where the eye
 * stands.
 *
 * @mangled MGSetViewMatrix__FPA4_fPf
 * @address 0x12DFA0
 * @size 0x6C
 * @unknownret
 */
void MGSetViewMatrix(float (*view)[4], float *position);

/**
 * Initializes the renderer, its GS state, and its shared vectors and matrices.
 *
 * @mangled MGInit__Fv
 * @address 0x12C220
 * @size 0x4E0
 */
void MGInit();

/**
 * Replaces the callback invoked from the vertical-sync interrupt handler.
 *
 * @mangled MGInitVSyncCallBack__FPFi_i
 * @address 0x12C700
 * @size 0x18
 */
void MGInitVSyncCallBack(int (*callback)(int));

/**
 * Initializes the two alternating VIF1 packet builders over caller buffers.
 *
 * @mangled MGInitVif1Packet__FP1P1
 * @address 0x12C720
 * @size 0xE0
 */
void MGInitVif1Packet(u_long128 *buffer0, u_long128 *buffer1);

/**
 * Opens packet construction for a new rendered frame.
 *
 * @mangled MGBeginFrame__Fv
 * @address 0x12CD90
 * @size 0x2E0
 */
void MGBeginFrame();

/**
 * Submits the current packet, completes frame processing, and swaps buffers.
 *
 * @mangled MGEndFrame__Fv
 * @address 0x12D100
 * @size 0x5CC
 */
void MGEndFrame();

/**
 * Selects how many vertical syncs frame completion waits for.
 *
 * @mangled MGFlipWaitVSync__Fi
 * @address 0x12D6D0
 * @size 0xC
 */
void MGFlipWaitVSync(int wait);

/**
 * Moves the display by an offset of at most 32 pixels each way, rounded to an even count.
 *
 * @mangled MGAdjustScreen__Fii
 * @address 0x12D960
 * @size 0x60
 */
void MGAdjustScreen(int x, int y);

/**
 * Rebuilds the projection and clip-volume values used for rendering.
 *
 * @mangled MGSetRenderInfo__Ffff
 * @address 0x12D6E0
 * @size 0x27C
 */
void MGSetRenderInfo(float scale, float near_z, float far_z);

/**
 * Updates the projection scale and its dependent rendering values.
 *
 * @mangled MGSetProjection__Ff
 * @address 0x12D960
 * @size 0x30
 */
void MGSetProjection(float scale);

/**
 * Returns the current projection scale.
 *
 * @mangled MGGetProjection__Fv
 * @address 0x12D990
 * @size 0x10
 */
float MGGetProjection();

/**
 * Restores the default full-frame scissor rectangle.
 *
 * @mangled MGSetWindowRect__Fv
 * @address 0x12D9A0
 * @size 0x13C
 */
void MGSetWindowRect();

/**
 * Sets the scissor rectangle used for subsequent drawing.
 *
 * @mangled MGSetWindowRect__F8CRect_i_
 * @address 0x12DAE0
 * @size 0x13C
 */
void MGSetWindowRect(CRect_i_ rect);

/**
 * Returns the background clear colour.
 *
 * @mangled MGGetBGColor__FPf
 * @address 0x12E240
 * @size 0x28
 */
void MGGetBGColor(float *color);

/**
 * Forces or releases render-info scissoring.
 *
 * @mangled MGScisioringForce__Fi
 * @address 0x12E270
 * @size 0x10
 */
void MGScisioringForce(int force);

/**
 * Projects a world position into unscaled two-dimensional screen coordinates.
 *
 * @mangled MGRotTransPers2D__FPiPfi
 * @address 0x12E4E0
 * @size 0x260
 */
int MGRotTransPers2D(int *screen, float *position, int fog);

/**
 * Draws the screen-space flare produced by a visible world-space light source.
 *
 * @mangled LensFlare__FP8CTexturePfUcUcUc
 * @address 0x158EB0
 * @size 0x2E4
 */
void LensFlare(CTexture *texture, float *position, unsigned char red, unsigned char green, unsigned char blue);

/**
 * Calculates a lit vertex colour from a surface normal.
 *
 * @mangled MGCalcColor__FPfPf
 * @address 0x12E990
 * @size 0xA8
 */
void MGCalcColor(float *color, float *normal);

/**
 * Returns the clip-plane mask for a world-space vertex.
 *
 * @mangled MGClipVertex__FPf
 * @address 0x12EA40
 * @size 0x23C
 */
int MGClipVertex(float *position);

/**
 * Rejects a box when all of its corners lie beyond one clip plane.
 *
 * @mangled MGClipBox__FP7CBoxVu0
 * @address 0x12EC80
 * @size 0xF4
 */
int MGClipBox(class CBoxVu0 *box);

/**
 * Sets the register that controls texture alpha expansion.
 *
 * @mangled MGSetGsTEXA__FP9sceGsTexa
 * @address 0x12F060
 * @size 0xC4
 */
void MGSetGsTEXA(sceGsTexa *texa);

/**
 * Names the frame buffer not currently being drawn as a texture.
 *
 * @mangled MGGetFBuffBackTex__FP9sceGsTex0
 * @address 0x12F1A0
 * @size 0x70
 */
void MGGetFBuffBackTex(sceGsTex0 *tex0);

/**
 * Draws a source texture rectangle into a differently sized destination rectangle.
 *
 * @mangled MGStretchMoveImage__FP9sceGsTex0RC8CRect_i_P9sceGsTex0RC8CRect_i_
 * @address 0x12F430
 * @size 0x538
 */
void MGStretchMoveImage(sceGsTex0 *src, const CRect_i_ &src_rect, sceGsTex0 *dst, const CRect_i_ &dst_rect);
