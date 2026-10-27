#pragma once

#include "runscript.hpp"

/**
 * The script stack slot ITEM_USE_WINDOW names for the chosen item (btsysscript.cpp), which
 * BtMiniItemSelect_Loop (btitem.cpp) writes back. BtEventInfo.item_select_result, an s32, says
 * whether one is pending; this holds the slot itself.
 */
extern RS_STACKDATA *PortItemSelectResult;
