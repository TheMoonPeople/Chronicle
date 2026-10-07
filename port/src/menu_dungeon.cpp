#include "menu_dungeon.hpp"

/* The debug item preview (EnterItemPolygonView) copies a whole vector into each of these, one float
   more than retail declares. MWCC gave every one of them a 16-byte slot, so on the PS2 the fourth
   float lands in padding; the port gives them the fourth float, as zero, the padding's value. */
PC_OVERRIDE float menudebugrot[4] = {0.0f, 0.0f, 0.0f, 0.0f};

PC_OVERRIDE float menudebugrscale[4] = {3.0f, 3.0f, 3.0f, 0.0f};
