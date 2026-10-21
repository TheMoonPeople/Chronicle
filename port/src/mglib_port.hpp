#pragma once

#include <libgraph.h>

#include <string_view>

#include "gfx/gfx.hpp"

// What the 2D and 3D replacement units share with mglib.cpp: the draw state the game's current
// register shadows describe, and the conversions from GS units into the renderer's.

// The DrawState the last MGSetGsTEST/MGSetGsZBUF/MGSetGsALPHA/MGSetGsTEXA/MGSetWindowRect calls
// describe, with fog from mgRenderInfo.
gfx::DrawState MGPortDrawState();

// Decoders for units that assemble the registers themselves.
void MGPortApplyTest(gfx::DrawState &state, const sceGsTest &test);
void MGPortApplyZbuf(gfx::DrawState &state, const sceGsZbuf &zbuf);
void MGPortApplyAlpha(gfx::DrawState &state, const sceGsAlpha &alpha);
void MGPortApplyTexa(gfx::DrawState &state, const sceGsTexa &texa);

// GS 12.4 primitive coordinates, including the game's window offsets (X 1728, Y GS_Y_OFFSET on a
// field-height buffer), into the renderer's 640x480 logical space.
float MGPortLogicalX(int gs_x);
float MGPortLogicalY(int gs_y);

// A 24-bit GS depth (near 16,700,000 .. far 1) into the renderer's reverse-Z [0, 1].
float MGPortDepth(unsigned gs_z);

// The frame buffer and the previous frame, as the TEX0 values MGGetFBuffTex and
// MGGetFBuffBackTex hand out. Texture lookups by TEX0 recognise these two TBP0 values.
inline constexpr unsigned kMGPortFrameTbp0 = 0;
inline constexpr unsigned kMGPortPreviousFrameTbp0 = 0xFFF;

// ---- Additions of the 3D phase ---------------------------------------------------------------

// Logical rows per GS row of a texture the game addresses by TEX0. The frame buffer and its
// previous copy are 640x480 logical but the game's rects and UVs on them count field rows (2);
// every other texture counts its own texel rows (1).
int MGPortFrameRowScale(gfx::TextureHandle texture);

// How the 640x480 logical frame maps onto a target the game draws 3D into as if it were the frame:
// 1 on the main target and on frame-height targets, 0.5 on the field-height copies (shadow_buf,
// water, blender: 224 to 256 rows) whose rows are the frame's field rows.
float MGPortTargetRowScale(gfx::TextureHandle target);

// World space (what mgRenderInfo.view_scaled transforms) to Vulkan clip space of the current
// render target, column-major [column][row] as sceVu0FMATRIX: the logical frame lands where the 2D
// units put it, depth is MGPortDepth's of the GS Z MGRotTransPers gives. 3D sprites and long-tail
// geometry multiply their model matrix onto it.
void MGPortWorldToClip(float clip[4][4]);

// Whether a texture name the game registers is one of its frame grabs ("frame_image",
// "frame_buff" and their kin, and the water's "water" and "water_buff"), which gfx keeps at the
// window's aspect (gfx::CreateRenderTarget).
bool MGPortFrameTarget(std::string_view name);

// MGMoveFrameBuffImage of the last finished frame, for a grab made before the tick has drawn:
// there the main target holds only MGBeginFrame's clear.
void MGPortMovePreviousFrameImage(sceGsTex0 *dst);

// ---- Display lists ---------------------------------------------------------------------------

// The next tick MGBeginFrame records is not interpolated from the one before (a mode's first).
void MGPortCutInterpolation();
