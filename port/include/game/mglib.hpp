#pragma once

/**
 * @file
 * Declares the graphics library that the game draws every model through.
 */

#include "common.h"

#include <libdma.h>
#include <libgraph.h>
#include <libpkt.h>

#include "renderinfo.hpp"

namespace dc::game {

class CBoxVu0;
class CFrame;
class CRect_i_;
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
 */
extern RenderInfo mgRenderInfo;

/**
 * GIF tag that opens an A+D register list of one register per quadword.
 */
extern sceGifTag GiftagAD;

/**
 * Index of the display buffer, and of each visual's built block, in use this frame.
 */
extern int DBuffID;

/**
 * Packet builder the current frame's VIF1 chain is written into.
 */
extern sceVif1Packet *Vif1Packet;

/**
 * Field of the interlaced display being drawn; the suffix keeps it apart from a second,
 * file-local VSyncField in retail.
 */
extern s32 VSyncField__2;

/** Number of vertical syncs a frame waits for before it is shown. */
extern int mgWaitVSync;

/** Horizontal offset of the display from its default position. */
extern int mgAdjustX;

/** Vertical offset of the display from its default position. */
extern int mgAdjustY;

/** TEX1 register state used for textured primitives. */
extern sceGsTex1 mgTEX1Env;

/** Whether the frame is cleared to the background colour before drawing. */
extern s32 mgClearBackFlag;

/** Depth-buffer sample requests resolved at the end of each frame. */
extern MG_PICKZ mgPickZBuff[16];

/** Colour the frame is cleared to. */
extern sceVu0FVECTOR mgBackColor;

/** All-zero vector. */
extern sceVu0FVECTOR mgZeroVector;

/** Vector with every component one. */
extern sceVu0FVECTOR mgUnitVector;

/** Second all-zero vector, initialised from mgZeroVector. */
extern sceVu0FVECTOR mgZeroVector2;

/** Second unit vector, initialised from mgUnitVector. */
extern sceVu0FVECTOR mgUnitVector2;

/**
 * All-zero matrix; passed as both point-light matrices it turns the lights off.
 */
extern sceVu0FMATRIX mgZeroMatrix;

/**
 * Identity matrix shared by rendering helpers.
 */
extern float mgUnitMatrix[4][4];

/** Scissor rectangle the whole frame is drawn inside. */
extern CRect_i_ mgWindowRect;

/** TEXA register state used when expanding texture alpha. */
extern sceGsTexa mgTexa;

/** First word of video memory past the frame and depth buffers. */
extern int mgTopVRAM;

/** Word address of the depth buffer in video memory. */
extern int mgZBufferAdr;

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
 * Sets the colour the frame is cleared to, from four floats in memory.
 */
void MGSetBGColor(float *color);

/**
 * Starts drawing the shadows, which go through a texture of their own.
 */
void MGBeginDrawShadow(sceGsTex0 tex0);

/**
 * Finishes drawing the shadows and lays them over the frame.
 */
void MGEndDrawShadow(unsigned char alpha);

/**
 * Sets the colour the frame is cleared to. A colour whose every component is negative turns
 * clearing off; anything else is clamped into eight bits per component.
 */
void MGSetBGColor(float red, float green, float blue, float alpha);

/**
 * Returns the number of vertical syncs observed since initialization.
 */
int MGGetVSyncCount();

/**
 * Clears the frame to one colour and the depth buffer to zero.
 */
void MGClearScreen(u_char r, u_char g, u_char b, u_char a);

/**
 * Clears the depth buffer while leaving colour unchanged.
 */
void MGClearZBuffer(int mode);

/**
 * Returns the packet builder for the current frame.
 */
sceVif1Packet *GetVif1Packet();

/**
 * Reads the colour that lights every face of a model.
 */
void MGGetAmbient(float *ambient);

/**
 * Sets the colour that lights every face of a model.
 */
void MGSetAmbient(float *ambient);

/**
 * Sets the direction and the colour of each of the four point lights.
 */
void MGSetPLight(float (*direction)[4], float (*color)[4]);

/**
 * Reads the lighting direction and colour matrices.
 */
void MGGetPLight(float (*direction)[4], float (*color)[4]);

/**
 * Turns one world position into a 12.4 fixed-point screen position and returns
 * whether it is on screen.
 */
int MGRotTransPers(int *screen, float *position, int fog);

/**
 * Turns one world position into the two screen corners of a sprite that faces
 * the eye, and gives back 1 while the sprite is on the screen.
 */
int MGRotTransPers3DSprite(int *top_left, int *bottom_right, float *position, float width, float height, int fog);

/**
 * Draws one model.
 */
void MGDraw(CFrame *frame);

/**
 * Drops the shadow of one model onto the ground with the fast shadow
 * microprogram, using fewer faces than `MGDrawShadow`.
 */
void MGDrawShadowFast(CFrame *frame, float *position, float *normal);

/**
 * Drops the shadow of one model onto the ground with the second fast shadow
 * microprogram, using fewer faces than `MGDrawShadow`.
 */
void MGDrawShadowFast2(CFrame *frame, float *position, float *normal);

/**
 * Drops the shadow of one model onto the ground, preserving the perspective
 * and viewport matrices.
 */
void MGDrawShadow(CFrame *frame, float *position, float *normal);

/**
 * Draws the shade that lies on one model.
 */
void MGDrawShade(CFrame *frame);

/**
 * Names the texture that stands for the frame being drawn.
 */
void MGGetFBuffTex(sceGsTex0 *tex0);

/**
 * Sets the register that names where the depth buffer lives; null sends
 * `mgZBuffer`.
 */
void MGSetGsZBUF(sceGsZbuf *zbuf);

/**
 * Sets the register that names which pixels a primitive may write; null sends
 * `mgPixelTest`.
 */
void MGSetGsTEST(sceGsTest *test);

/**
 * Fills a rectangle of the frame with one colour.
 */
void MGFillBox(const CRect_i_ &rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

/**
 * Sets how far the fog reaches and what colour it is.
 */
void MGSetFogParm(float near_z, float far_z, unsigned char r, unsigned char g, unsigned char b, float far_fog, float near_fog);

/**
 * Sets the register that names how a primitive blends with the frame; null
 * sends `mgAlpha`.
 */
void MGSetGsALPHA(sceGsAlpha *alpha);

/**
 * Copies a rectangle of one texture into another.
 */
void MGMoveImage(sceGsTex0 *src, const CRect_i_ &rect, sceGsTex0 *dst, int dst_x, int dst_y, int direction);

/**
 * Copies the frame being drawn into a texture.
 */
void MGMoveFrameBuffImage(sceGsTex0 *dst, int x, int y, int direction);

/**
 * Sets the matrix that puts the world in front of the eye, and where the eye
 * stands.
 */
void MGSetViewMatrix(float (*view)[4], float *position);

/**
 * Sets the matrix that puts the world in front of the eye, leaving where the eye stands.
 */
void MGSetViewMatrix(sceVu0FMATRIX view);

/**
 * Initializes the renderer, its GS state, and its shared vectors and matrices.
 */
void MGInit();

/**
 * Replaces the callback invoked on every vertical sync.
 */
void MGInitVSyncCallBack(int (*callback)(int));

/**
 * Initializes the two alternating VIF1 packet builders over caller buffers.
 */
void MGInitVif1Packet(u_long128 *buffer0, u_long128 *buffer1);

/**
 * Opens packet construction for a new rendered frame.
 */
void MGBeginFrame();

/**
 * Submits the current packet, completes frame processing, and swaps buffers.
 */
void MGEndFrame();

/**
 * Selects how many vertical syncs frame completion waits for.
 */
void MGFlipWaitVSync(int wait);

/**
 * Moves the display by an offset of at most 32 pixels each way, rounded to an even count.
 */
void MGAdjustScreen(int x, int y);

/**
 * Rebuilds the projection and clip-volume values used for rendering. Depth runs
 * from 16700000 at the near plane down to 1 at the far one.
 */
void MGSetRenderInfo(float scale, float near_z, float far_z);

/**
 * Updates the projection scale and its dependent rendering values.
 */
void MGSetProjection(float scale);

/**
 * Returns the current projection scale.
 */
float MGGetProjection();

/**
 * Restores the default full-frame scissor rectangle.
 */
void MGSetWindowRect();

/**
 * Sets the scissor rectangle used for subsequent drawing, as an origin and an
 * extent.
 */
void MGSetWindowRect(CRect_i_ rect);

/**
 * Returns the background clear colour.
 */
void MGGetBGColor(float *color);

/**
 * Forces or releases render-info scissoring.
 */
void MGScisioringForce(int force);

/**
 * Projects a world position into whole-pixel screen coordinates relative to the
 * screen centre, with the view's vertical squeeze undone.
 */
int MGRotTransPers2D(int *screen, float *position, int fog);

/**
 * Draws the screen-space flare produced by a visible world-space light source.
 */
void LensFlare(CTexture *texture, float *position, unsigned char red, unsigned char green, unsigned char blue);

/**
 * Calculates a lit vertex colour from a surface normal.
 */
void MGCalcColor(float *color, float *normal);

/**
 * Returns six clip bits (near, far and one per side) for a world-space vertex,
 * measured against the visible extent. A vertex at or behind the eye has its side
 * tests reversed so a running AND across corners stays meaningful.
 */
int MGClipVertex(float *position);

/**
 * Rejects a box when all of its corners lie beyond one clip plane; zero means
 * the box is kept.
 */
int MGClipBox(CBoxVu0 *box);

/**
 * Sets the register that controls texture alpha expansion; null sends
 * `mgTexa`.
 */
void MGSetGsTEXA(sceGsTexa *texa);

/**
 * Names the frame buffer not currently being drawn as a texture.
 */
void MGGetFBuffBackTex(sceGsTex0 *tex0);

/**
 * Draws a source texture rectangle into a differently sized destination rectangle.
 */
void MGStretchMoveImage(sceGsTex0 *src, const CRect_i_ &src_rect, sceGsTex0 *dst, const CRect_i_ &dst_rect);

} // namespace dc::game
