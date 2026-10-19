# Item icons from the 3D models

The menus draw an item from a 32x32 cell of the `itemicon` / `wepicon` sheets. The chest and pick-up scenes draw the
item's model. Both are looked up from the item id (`ComItemInfo` for the icon cell, `ITEM_NAME_TBL_NEW` /
`makeWeaponName` for the model), so the model for an icon is known: `tools/areapack/itemicons.py` lists them.

With `DC_ITEM_ICONS_3D=1` the port draws each item's model into a small texture the first time the menus ask for its
icon (`RetCTex`), and uses that in place of the cell. An item with no model of its own keeps the sheet's picture.

* The model is drawn with the menu's camera and lights into an offscreen target, then cropped around where its origin
  lands and scaled to the cell. The texture block 0x47 holds the model's pictures only while it is drawn.
* `DC_ITEM_ICONS_3D_SHEET=<0|1>` draws every item's icon in a grid over the menu, for checking them.
* `DC_ITEM_ICONS_3D_DEBUG=1` prints each item drawn and where its origin landed.
