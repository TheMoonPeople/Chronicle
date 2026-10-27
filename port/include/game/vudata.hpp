#pragma once

/**
 * @file
 * The DMA tag chain and drawing environment the frame starts with.
 */

#include "common.h"

namespace dc::game {

/**
 * DMA tag chain that begins every frame's VU1 display list.
 */
extern u_int My_dma_start0[];
/**
 * GS drawing environment the chain above sets up.
 */
extern u_int My_DrawEnv[];

} // namespace dc::game
