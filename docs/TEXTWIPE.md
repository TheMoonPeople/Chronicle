# Text wipe: old picture text against runtime text

> **Off by default.** The tool is there only when `config.json` has `"game": {"text_wipe": true}`. With it off the
> keys below do nothing, nothing is drawn but the runtime text, and the tuning file is not read: the text keeps the
> locked-in fits (see the end of this page). Turn it on to look at or adjust the fits.

A developer tool for the text the port draws at runtime (from `font.ttf` and the language's JSON) instead of
as the game's pictures of words. It puts a line down the screen: the **old look** (the game's bitmaps and
painted words) on its left, the **new look** (runtime text) on its right.

| Key | Does |
|---|---|
| **F9** | Turns the wipe on and off. Turning it on starts a slide: everything old, then the line eases left across the screen revealing the new text, rests, and slides back. |
| **F10** | Steps the mode while on: slide, hold all old, hold all new, **overlay**. |

It works on any scene that draws one of the places below, and needs `font.ttf` (the build ships it in `lang/`);
without it both looks are the old one. The bottom of the screen labels the two sides. It is
a view only: nothing is saved, and with it off the game draws exactly what it did.

## Places that draw both looks

| Place | Old look | New look | Code |
|---|---|---|---|
| Message text: dialogue, menus, help windows, item and shop text, every message the game lays out | the 14x20 bitmap characters of `gaiji` | `font.ttf` characters at the screen's resolution | `ClsMes::Draw` in `clsmes.cpp` (`text_pass`) |
| Menu captions beside the main menu's icons (ITEM, WEAPON, GO TO DUNGEON...), town and dungeon | the words painted into the unchanged `stayframe` | outlined `font.ttf` text fitted to the painted bounds | `DrawMainMenuIcon` in `menu_draw.cpp`, `caption_text.cpp` |
| Area name cards on the loading screen (Norune, Matataki...) | the disc's card with the name painted in | the text-free plate with the name drawn over it | `nowload.cpp`, `area_card.cpp` |

Not wiped, because their old look is not drawn alongside: the FPS badge (`gameloop.cpp`, `overlay.cpp`; its
old look is the 5x7 bitmap font, recorded into a separate overlay list), the Options screen's rows (they
are message text, so they are covered by the first row), and per-language pictures swapped for others
(`localize_texture.cpp`, `LocalizeTexture`), which replace a picture rather than draw text.

## How a place joins

```cpp
textwipe::Both([&](bool runtime) {
    if (runtime) { /* draw the text */ } else { /* draw what the game drew */ }
});
```

Off, `Both` calls the function once with `runtime = true`. On, it calls it with `false` clipped to the line's
left (`gfx::SetDrawClip`) and with `true` clipped to its right, so the draws must go out inside the call (not
into a packet flushed later). Keep the old look's data when the new one replaces it; menu captions retain
the original `stayframe` texture.

## Overlay and tuning

**Overlay** (the fourth F10 mode) draws both looks in full, over each other: the old text in red, the new in
cyan. Where they agree the colours blend; where they do not, a red or cyan fringe shows which way to move.

While the wipe is on, the new text can be dialled onto the old. A panel under the FPS badge lists the values.

| Key | Does |
|---|---|
| **F11** | Next set of text: messages, menu captions, area cards |
| **F12** (Shift: back) | Next value |
| **-** / **=** | Change the value (hold to repeat; Shift ten times the step, Ctrl a tenth) |
| **F8** | Reset the set |

Values: **size** (everything), **width** and **height** (side-to-side and up-and-down size), **letter spacing** and
**word spacing** (logical pixels), **move right / move down** (logical pixels) and, for messages, **fit to cells**
(on: each word is fitted to the cells the game laid out, as the bitmaps are; off: the letters keep the spacing
you dial, starting at the word's first cell, which is the one to use when matching the old text by hand).

They are saved at once to `<save folder>/lang/textwipe_tuning.json` and read at every start, and they apply to the
game as played, wipe or no wipe (while `game.text_wipe` is on), so a good fit stays. The defaults change nothing. When a fit looks right, send
the file: the numbers can be made the code's own.

## Locked-in fits

The fits dialled in with the overlay are the shipped defaults (`textwipe::Locked` in `textwipe.cpp`): dialogue and
other windows, the Options screen (its own set, taken for as long as `options::Draw` runs) and the area name cards.
`textwipe_tuning.json` now holds only what differs from them; F8 returns a set to the locked fit. The menu captions
have not been tuned and are as the code lays them out.
