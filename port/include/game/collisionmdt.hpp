#pragma once

#include "collision.hpp"
#include "mdt.hpp"

namespace dc::game {

/**
 * Writes the component-wise maximum and minimum of three vectors.
 */
void vu_maxmin3(float *max, float *min, float *a, float *b, float *c);

/**
 * Writes the unnormalised normal of the triangle a, b, c as (b - a) x (c - a).
 */
void vu_normal(float *normal, float *a, float *b, float *c);

/**
 * Holds a query box for the vu_box_missed calls that follow.
 */
void vu_hold_box(float *max, float *min);

/**
 * Returns non-zero when a box misses or only touches the held query box.
 */
int vu_box_missed(float *max, float *min);

} // namespace dc::game
