#pragma once

#include "common.h"

#include <libpkt.h>

#include "rect.hpp"

class CFrameVu1;

/**
 * Cloth instance currently receiving configuration commands.
 */
extern CCloth *pCloth;

/**
 * Turns a model towards one heading, a step at a time, and gives back the
 * heading it stands on now.
 *
 * @mangled unitRotation__FP9CFrameVu1f
 * @address 0x140810
 * @size 0x28C
 */
float unitRotation(CFrameVu1 *frame, float heading);

/**
 * Queues a GS local-to-local image move between two texture buffers.
 *
 * @mangled MoveImageTest__FP13sceVif1PacketiiiRC8CRect_i_iiiiii
 * @address 0x140660
 * @size 0x1a4
 */
void MoveImageTest(sceVif1Packet *packet, int src_base, int src_width, int src_format, const CRect_i_ &rect, int dst_base, int dst_width, int dst_format, int dst_x, int dst_y, int direction);
