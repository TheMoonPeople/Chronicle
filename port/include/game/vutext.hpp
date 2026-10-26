#pragma once

/**
 * @file
 * The VU1 microprograms the renderers upload, which ps2/src/vutext.cpp holds.
 */

#include "common.h"

/**
 * VU1 microprogram of the model renderer.
 */
extern u_int Vu_prog0[];
/**
 * VU1 microprogram of the flat-shaded model renderer.
 */
extern u_int Vu_prog0f[];
/**
 * VU1 microprogram of the second model renderer.
 */
extern u_int Vu_prog1[];
/**
 * VU1 microprogram of the ground renderer.
 */
extern u_int Vu_progg[];
/**
 * VU1 microprogram that draws the drop shadows.
 */
extern u_int Vu_shadow[];
/**
 * VU1 microprogram that draws the drop shadows, second variant.
 */
extern u_int Vu_shadow2[];
/**
 * VU1 microprogram that draws the drop shadows, third variant.
 */
extern u_int Vu_shadow3[];
/**
 * VU1 microprogram of the main scene renderer.
 */
extern u_int Vu_progmain[];
