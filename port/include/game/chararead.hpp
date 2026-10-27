#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

namespace dc::game {

/**
 * @file
 * Declares the reader that fills a character out of a pack file.
 */

class CCharacter;

/**
 * Reads one character out of a pack file: its model, its motions, its textures
 * and everything that goes with them.
 */
void ReadInfo(CCharacter *chara, unsigned int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *motion_alloc, CDataAlloc2<1> *extra_alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int visual_type, int keep_textures);

/**
 * Pulls two cloth vertices towards the rest length given by the spring.
 */
void StretchBind2(float *point_a, float *point_b, float *spring);

/**
 * Gives the length of a three-component vector, on the vector unit.
 */
float vuabs(float *vector);

/**
 * Assigns an image whose script-provided slot is also its image number.
 */
void CommandIMG(void **argv);

} // namespace dc::game
