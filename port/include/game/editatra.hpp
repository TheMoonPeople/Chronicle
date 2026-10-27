#pragma once

#include "common.h"

#include "editpartsdata.hpp"

namespace dc::game {

/**
 * Returns an editor element attribute after validating both indices.
 */
EDIT_ELEMENT_ATRA *GetEditAtraData(int ground, int number);

/**
 * Returns an editor part attribute after validating both indices.
 */
EDIT_PARTS_ATRA *GetEditAtraPartsData(int ground, int number);

/**
 * Returns an editor chip attribute from the element table's chip range.
 */
EDIT_ELEMENT_ATRA *GetEditAtraChipData(int ground, int number);

} // namespace dc::game
