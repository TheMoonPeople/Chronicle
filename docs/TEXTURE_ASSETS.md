# Texture assets

The port can draw a texture from a PNG instead of the one on the disc. Pictures are looked up **by the texture's
name** (the name the game gives it, the same one `DC_TEXTURE_DUMP` writes), per area, so one dungeon or town can be
redone without touching the rest. The layout matches what `tools/areapack` writes (`textures/` in each area folder).

## Layout

```
<root>/d02/textures/<name>.png     only while the game is in dungeon 2
<root>/e03/textures/<name>.png     only in that town or event map
<root>/a_fre/textures/<name>.png   only with that language set (a_jpn a_usa a_eng a_fre a_ger a_ita a_spa)
<root>/textures/<name>.png         everywhere (shared, title, menus, loading screens)
```

- Roots, in order: the save folder's `assets`, then `assets` next to the executable. The save folder wins.
- At each step `<name>_<w>x<h>.png` is tried before `<name>.png`, for two textures of different sizes sharing a name.
- Order: the area's folder, then the language's, then `textures/`.
- Areas: `d01`..`d07` are the dungeons (the disc tags), a town or event map is its map name (`e01`, `s04`, ...).
  The title, menus and loading screens are in no area and use `textures/` only.
- A language's own picture (`lang/textures/...`, see LOCALIZATION.md) is tried first and wins.

## Making the pictures

```
dcdata extract "Dark Cloud.iso" data
dcdata textures data assets          # add --overwrite to replace pictures that are already there
```

`dcdata textures` finds every named TIM2 picture in the extracted files, decodes it (palettes, 4/8-bit, the PS2's
swizzled order and alpha range) and writes it in the layout above. A picture the same in every area is written once in
`textures/`; one that differs in an area goes in that area's folder; a name used at two sizes gets `_<w>x<h>`. A
picture already in the assets folder is kept, so running it again never undoes an artist's work. Unnamed pictures and
TIM2 files holding several pictures are counted and skipped. `tools/areapack` gives the same pictures with the rest of
an area's data and is the reference for the decoding.

## Size

A picture must have the texture's shape (sides in the same ratio, 2% tolerance); otherwise it is ignored and the
reason is printed once. Otherwise:

- **At least as many pixels as the texture**: all of them are kept (up to 8192 a side; larger is scaled down). The game
  still addresses the texture at its original size, so drawing and animation need no change. This needs the
  `logical_width/height` field of `gfx::TextureDesc` (Kazarov), so the Chronicle change needs that change merged and
  `tools/kazarov` bumped first.
- **Fewer pixels**: scaled up to the texture's original size.

## Limits

- A replaced texture is RGBA, so a palette the game changes while running (colour cycling, fades by palette) no longer
  has an effect on it.
- Render targets and textures the game writes on the CPU every frame (`textureanime.cpp`) are not replaced.
- Copies between a high-resolution replaced texture and another texture (`PortMoveImage`) use the original size and
  may not line up.
- Nothing was compiled or run in the environment this was written in; check with `DC_TEXTURE_DUMP` names first.
