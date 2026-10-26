#pragma once

/**
 * @file
 * The overlay address table, the tail of main's read-only data.
 */

#include "common.h"

/**
 * Load addresses of the main executable and its overlays.
 */
extern void *_overlay_group_addresses[];
