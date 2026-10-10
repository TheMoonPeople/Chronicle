# Button symbols

The game draws pad symbols in its text (the font's icons) and in a few textures. With `input.glyphs` `"new"`
(the default) the port draws redrawn symbols for the device in use instead; `"original"` keeps the PS2's art.
Options > Controls has both settings.

| config.json | values | meaning |
|---|---|---|
| `input.glyphs` | `new`, `original` | the redrawn symbols, or the game's own |
| `input.glyph_device` | `auto`, `ps3`, `ps4`, `ps5`, `xbox`, `switch`, `steamdeck`, `steamcontroller`, `keyboard` | whose symbols: `auto` follows the device touched last (keyboard or mouse, otherwise the first gamepad's make, Xbox for a pad SDL does not know; PS4 until something is touched); the others force one |

The keyboard style shows the key or mouse button the player bound to the action (`input.bindings`), the PS4 symbol
where the art has none for that key. Face buttons follow their position on the pad (the game's Cross is the bottom
button: A on Xbox, B on Switch).

## Where they are drawn

- Font icons (`ClsMes::DrawGaijiFont`, `port/src/clsmes.cpp`): the pad codes -0x300 to -0x2F4 (`{select}`, `{start}`,
  `{L1}` to `{R2}`, `{circle}`, `{triangle}`, `{cross}`, `{square}`, `{dpad}`...). Every message and menu that names a
  button uses them.
- Symbols painted into textures (`port/src/glyph_draw.cpp`, table `kBaked`): the name entry's L1/R1 hints (`nametemp`), the
  character board's tab hints (`perbrd`) and the cutscene skip prompt (`pause_e`, `skip_bord`). Draws of these rectangles
  (`RectSprite`, `port/src/snd.cpp`) are replaced by, or overlaid with, the symbol.
- Symbols on other textures the port has not found yet are still the game's: `DC_SPRITE_TRACE=1` prints each texture
  rectangle a sprite draws (stderr) and `DC_TEXTURE_DUMP=<dir>` writes every texture the game loads as a PNG, which is
  how the table was made. Add the box to `kBaked`.

## Files

`glyphs/` beside the executable (or in the save folder, which wins): `glyphs.json` and one atlas PNG per style. The
json gives each symbol's rectangle in its atlas; `reference` is the atlas size of a round face button. A symbol is
drawn scaled so a round button fills the height of its slot. `tools/glyphs/build_glyphs.py --src <art folder>` builds
them from the source art (P4Gamepad/Retro, P5Gamepad/Retro, XGamepad/Alt 2, SGamepad/Retro, Keyboard_Mouse/Dark);
Xbox's Menu button and the keyboard style's cursor-key clusters are composed by the tool. The atlases are read with
stb_image (a pinned download, as stb_truetype is).

The PS3, Steam Deck and Steam Controller styles (and PS5, below) are drawn from flat packs, one 64 px PNG per button;
the Steam Controller style is used for both of Valve's pads, the 2015 one and the 2026 one. A Valve pad is told from
its USB vendor (0x28DE), the Deck by its product (0x1205).

The PS5 styles are drawn from flat packs of 64 px buttons (`ps5_a_butt.png`, `ps5_lstick_all.png`...):
`tools/glyphs/build_glyphs.py --ps5-pack <folder>` rebuilds only `ps5.png` and its entry in `glyphs.json`, leaving the
other styles; `--ps5-color-pack <folder>` builds the separate `ps5color` style from the same pack format. Options >
Text > Symbols Shown keeps the monochrome PS5 style and adds PS5 Colored. The supplied pack is in
`port/glyphs/ps5color-pack/`. Its Cross, Circle, Square and Triangle use the source
`P5Gamepad/Alt/T_P5_{Cross,Circle,Square,Triangle}_Color_Alt.png` images (normalized to 64 px); the remaining
glyphs use the flat pack's A, B, X and Y as those four face-button positions, plus its shoulder, menu, d-pad and stick
images. The source `T_P5_Alt_Sprite.svg` / `.png` shows the full layout; the individual colored images are used so the
other glyphs can keep their existing pack art. No license or attribution file was present with the supplied assets.
`--ps3-pack` (`ps3_*.png`), `--steamdeck-pack` (`sd_*.png`) and `--steamcontroller-pack` (`sc_*.png`)
do the same for theirs. The pack's A, B, X and Y are Cross, Circle, Square and Triangle; L3 and R3 are its plain stick icons.
