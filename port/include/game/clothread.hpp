#pragma once

#include "common.h"

#include <libpkt.h>

#include "rect.hpp"

namespace dc::game {

class CCloth;
class CFrameVu1;
struct input_str;
struct MOTION_INFO;
template <int Kind>
class CDataAlloc2;

/**
 * Cloth instance currently receiving configuration commands.
 */
extern CCloth *pCloth;

/**
 * Reads one model's cloth description from a character script and attaches the
 * simulation to the frame it names. Returns nullptr when there is no model frame.
 */
CCloth *InitCloth(CFrameVu1 *frame, input_str &input, CDataAlloc2<1> *alloc);

/**
 * Converts analog-stick displacement into the walk speed of motion[2] and
 * returns the movement state: 0 untilted, 1 for a strong tilt, 2 for a slight one.
 */
int keyCtrl(float stick_x, float stick_y, MOTION_INFO *motion);

/**
 * Turns a model towards one heading, a step at a time, and gives back the
 * heading it stands on now.
 */
float unitRotation(CFrameVu1 *frame, float heading);

/**
 * Queues a GS local-to-local image move between two texture buffers.
 *
 * The direction only orders overlapping transfers within one buffer.
 */
void MoveImageTest(sceVif1Packet *packet, int src_base, int src_width, int src_format, const CRect_i_ &rect, int dst_base, int dst_width, int dst_format, int dst_x, int dst_y, int direction);

} // namespace dc::game
