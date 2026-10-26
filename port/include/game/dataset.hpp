#pragma once

#include "common.h"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CFrame;

#include "dataalloc_fwd.hpp"

/**
 * @file
 * Declares the calls that build a model out of the data that was read.
 */

/**
 * Clears the global data arena and prepares its persistent scene banks.
 *
 * @mangled InitializeDataBuffer__Fv
 * @address 0x125990
 * @size 0xF4
 */
void InitializeDataBuffer();

/**
 * Reserves the read, packet, and scratch buffers from the global data arena.
 *
 * @mangled SetPacketReadBuffer__Fii
 * @address 0x125AE0
 * @size 0xD8
 */
void SetPacketReadBuffer(int packet_quads, int read_quads);

/**
 * Gives every data buffer back, so the next area starts from an empty one.
 *
 * @mangled BufferAllClear__Fv
 * @address 0x125BC0
 * @size 0x254
 */
void BufferAllClear();

/**
 * Gives a frame the attributes that it draws with.
 *
 * @mangled SetFrameAttr__FP6CFramei
 * @address 0x125EF0
 * @size 0x3BC
 * @unknownret
 */
void SetFrameAttr(CFrame *frame, int recurse);

/**
 * Receives the files that synchronous game-data reads load.
 */
extern u_int *read_buffer;

/**
 * Points at the shared scratch allocator used while loading and transforming data.
 */
extern CDataAlloc2<1> *WorkBuffer;

/**
 * Points at whichever per-frame VU data arena the current frame builds into.
 */
extern CDataAlloc2<1> *ActiveData;
