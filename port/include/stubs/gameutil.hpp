#pragma once

/**
 * Holds a query box for the vu_box_missed calls that follow.
 */
void vu_hold_box(float *max, float *min);

/**
 * Returns non-zero when a box misses or only touches the held query box.
 */
int vu_box_missed(float *max, float *min);
