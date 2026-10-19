#pragma once

#include "common.h"

class CTexture;

// Item icons drawn from the item's 3D model instead of the picture on the icon sheets.
//
// The menus draw an item from a 32x32 cell of the `itemicon` / `wepicon` sheets, all through RetCTex. With this on,
// RetCTex hands out, for an item that has a model (the one the chest shows), a small texture the model was drawn into
// the first time it was asked for, and the cell is its whole 32x32. Items without a model keep the sheet's picture.
// Off by default: DC_ITEM_ICONS_3D=1 turns it on. See docs/ITEM_ICONS_3D.md.

/// Whether item icons are drawn from models.
bool ItemIcons3DEnabled();

/// The icon texture for an item, drawing it on first use; null where the item has no model or this is off.
CTexture *ItemIcon3DTexture(int item_no);

/// Debug: with DC_ITEM_ICONS_3D_SHEET=<page> set, draws every item's icon in a grid over the screen (page 0 or 1).
void ItemIcons3DDrawSheet();

void ItemIcons3DDumpDebug();
