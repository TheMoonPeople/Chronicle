#pragma once

#include "common.h"

/**
 * @file
 * Declares the vector that the game passes by value.
 */

/**
 * Names a point or a direction in the world by three parts. A fourth part
 * follows them, so that the vector copies as one quadword.
 *
 * `CVector3_f_` is the disassembler's filesystem-safe spelling of the retail
 * name; it is what every mangled symbol in this project carries.
 */
class CVector3_f_ {
public:
    float x; /**< Part of the vector along the first axis. */
    float y; /**< Part of the vector along the second axis. */
    float z; /**< Part of the vector along the third axis. */
    float w;

    CVector3_f_() {}

    CVector3_f_(float x, float y, float z) : x(x), y(y), z(z) {}
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(CVector3_f_) == 0x10);

/**
 * Names an integer grid coordinate in a quadword-sized vector.
 */
class CVector3_i_ {
public:
    int x; /**< Gives the grid coordinate along the first axis. */
    int y; /**< Gives the grid elevation. */
    int z; /**< Gives the grid coordinate along the third axis. */
    int unk_0c;
} __attribute__((aligned(16)));

STATIC_ASSERT(sizeof(CVector3_i_) == 0x10);
STATIC_ASSERT(__alignof__(CVector3_i_) == 0x10);
