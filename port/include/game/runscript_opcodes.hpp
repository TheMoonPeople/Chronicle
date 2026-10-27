#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

namespace dc::game {

/**
 * @file
 * Declares how a monster's battle script is loaded and given the opcodes it
 * can call.
 */

class CRunScript;

/**
 * Loads a monster script, giving it its argument stack, call stack and opcode table.
 */
int BtSetEventScript(CRunScript *script, char *program, CDataAlloc2<1> *arena);

/**
 * Builds the opcode table out of the extension list, refusing a list that names one twice.
 */
void BtSetEventExtendTable();

} // namespace dc::game
