#pragma once

#include "common.h"

/**
 * @file
 * Declares the rectangle that the sprite calls take.
 */

/**
 * Names a rectangle by the corner it starts at and how far it reaches.
 *
 * `CRect_i_` is the disassembler's filesystem-safe spelling of the retail
 * name; it is what every mangled symbol in this project carries.
 */
class CRect_i_ {
public:
    s32 x;      /**< Distance of the left edge from the left of the texture. */
    s32 y;      /**< Distance of the top edge from the top of the texture. */
    s32 width;  /**< Distance from the left edge to the right edge. */
    s32 height; /**< Distance from the top edge to the bottom edge. */

    CRect_i_() {}

    CRect_i_(s32 x, s32 y, s32 width, s32 height) : x(x), y(y), width(width), height(height) {}
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(CRect_i_) == 0x10);

/**
 * Names a rectangle the same way, for the calls that take the plain C name.
 */
struct RECT {
    s32 x;      /**< Distance of the left edge from the left of the screen. */
    s32 y;      /**< Distance of the top edge from the top of the screen. */
    s32 width;  /**< Distance from the left edge to the right edge. */
    s32 height; /**< Distance from the top edge to the bottom edge. */
};

STATIC_ASSERT(sizeof(RECT) == 0x10);

/**
 * Names the colour a sprite corner draws with.
 */
struct spRGBA {
    u8 r; /**< How much red the corner has. */
    u8 g; /**< How much green the corner has. */
    u8 b; /**< How much blue the corner has. */
    u8 a; /**< How opaque the corner is; 0x80 is fully opaque. */
};

STATIC_ASSERT(sizeof(spRGBA) == 0x4);
