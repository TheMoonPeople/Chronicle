#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

/**
 * @file
 * Declares the calls that build a model out of the data that was read.
 */

namespace dc::game {

class CFrame;

/**
 * Clears the global data arena and prepares its persistent scene banks.
 */
void InitializeDataBuffer();

/**
 * Reserves the read, packet, and scratch buffers from the global data arena.
 */
void SetPacketReadBuffer(int packet_quads, int read_quads);

/**
 * Gives every data buffer back, so the next area starts from an empty one.
 */
void BufferAllClear();

/**
 * Applies the attribute codes after "__" in a frame's name to its draw
 * attributes, and to its children's as well when recurse is set.
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

} // namespace dc::game
