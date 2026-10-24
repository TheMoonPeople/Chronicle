# ChronicleSharp-Bold.ttf and ChronicleSharp-Regular.ttf

Chronicle Sharp, the port's text font, copyright (c) 2026 Tororoi: Dark Cloud's letters as a TrueType font.
Bold is the game's bold dialogue set, Regular its narrower set.

It is derived from the Dark Cloud Compendium Community Font ("Dark Cloud Speech", by Dayuppy and Moonbunny, copyright
(c) 2020), which was released to the community to use as they wish; Dayuppy confirmed on 2026-10-06 that it may be used
in this port. Bold started from its regular weight and Regular from its THIN variant (Jeff, who contributes the port's
text work, confirmed on 2026-10-10 that he has Dayuppy's permission to use it). Each of the game's 88 letters was then
reshaped until, drawn at the game's 14x20, it matches the game's letter in `meswin/gaiji.img`; `%` was split so its
rings stand clear of the slash, `~` was redrawn, and the Japanese marks the name alphabet carries were added
(`「 」 『 』`, one stroke weight down and across, and `・ 。`). The font's other glyphs are the Compendium font's.

Each letter sits on the game's glyph sheet cell: 50 units a cell pixel, the baseline at the cell's row 15. Its accented
letters, the cedilla, `¡`, `¿`, `œ` and `Œ` are blank; the port builds them from the font's own letters and marks
when it draws.

The build copies Bold to `lang/font.ttf` and Regular to `lang/font_thin.ttf` beside the executable. The message windows'
88 letters are drawn from both at the screen's own resolution (Bold for the game's bold set, Regular for its narrower
one), each in the cell the game cuts its letter from, so the game lays them out and edges them as it always has. Bold
also draws the text the port draws from a font, the name alphabet (the name entry keyboard, the names on it, the save
slots and the ally card), the menu captions and the dungeon entry's floor labels; Regular draws the area cards, the
floor select's boss names and the weapon menu's buttons.

To use another font for the text the port draws from a font, put it at `lang/font.ttf` in the save folder (that wins
over this one) or start the game with `--font <file>`.

## Cinzel-Regular.ttf

Cinzel (Copyright 2020 The Cinzel Project Authors, https://github.com/NDISCOVER/Cinzel), SIL Open Font License 1.1. It is
used only by `tools/textpack/build.py`, to letter the bosses' names (a Trajan-style serif) on the pictures that tool draws.

## SourceSansProSemibold.ttf
Copyright 2010, 2012 Adobe Systems Incorporated﻿
﻿license: SIL Open Font License (OFL)
link: https://www.fontspace.com/source-sans-pro-font-f15035