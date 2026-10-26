#include "common.h"

#include "crt0.hpp"
#include "literals.hpp"

/* The tail of retail's read-only data: the overlay address table. Retail's
   literal pool, which MWLD gathered from every unit's float and double
   constants, is not needed on PC: each unit loads its own constants. */
