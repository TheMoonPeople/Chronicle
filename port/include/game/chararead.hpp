#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

/**
 * @file
 * Declares the reader that fills a character out of a pack file.
 */

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CCharacter;

/**
 * Reads one character out of a pack file: its model, its motions, its textures
 * and everything that goes with them.
 *
 * @mangled ReadInfo__FP10CCharacterPUiPcP14CDataAlloc2_1_P14CDataAlloc2_1_P14CDataAlloc2_1_iP14CDataAlloc2_1_ii
 * @address 0x139D10
 * @size 0x258
 * @unknownret
 */
void ReadInfo(CCharacter *chara, unsigned int *pack, char *name, CDataAlloc2<1> *model_alloc, CDataAlloc2<1> *motion_alloc, CDataAlloc2<1> *extra_alloc, int texture_block_no, CDataAlloc2<1> *image_alloc, int visual_type, int keep_textures);

/**
 * Gives the length of a three-component vector, on the vector unit.
 *
 * @mangled vuabs__FPf
 * @address 0x13B450
 * @size 0x30
 */
float vuabs(float *vector);
