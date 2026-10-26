#pragma once

#include "common.h"

#include "editpartsdata.hpp"

/**
 * Returns an editor element attribute after validating both indices.
 *
 * @mangled GetEditAtraData__Fii
 * @address 0x158D80
 * @size 0x78
 */
EDIT_ELEMENT_ATRA *GetEditAtraData(int ground, int number);

/**
 * Returns an editor part attribute after validating both indices.
 *
 * @mangled GetEditAtraPartsData__Fii
 * @address 0x158E00
 * @size 0x74
 */
EDIT_PARTS_ATRA *GetEditAtraPartsData(int ground, int number);

/**
 * Returns an editor chip attribute from the element table's chip range.
 *
 * @mangled GetEditAtraChipData__Fii
 * @address 0x158E80
 * @size 0x24
 */
EDIT_ELEMENT_ATRA *GetEditAtraChipData(int ground, int number);
