#pragma once

/**
 * @file
 * The overlay address table and the literal pool, the tail of main's read-only data.
 */

#include "common.h"

/**
 * Load addresses of the main executable and its overlays.
 */
extern void *_overlay_group_addresses[];

/**
 * Retail's literal pool: every float and double constant a unit loads through $gp.
 */
extern const u_int LiteralPool[438];
