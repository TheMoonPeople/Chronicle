# DarkCloudCompendium.ttf

The Dark Cloud Compendium Community Font ("Dark Cloud Speech", version 001.000), by Dayuppy and Moonbunny,
copyright (c) 2020 Dayuppy & Moonbunny. It was released to the community to use as they wish, and Dayuppy
confirmed on 2026-10-06 that it may be used in this port.

The digit 5 was redrawn (the original had no tail on its bottom stroke, so it read like an S); every other glyph is Dayuppy and Moonbunny's.
It draws the FPS counter (`gen_overlay_font.py`) and the game's message text (docs/LOCALIZATION.md): the build
copies it to `lang/font.ttf` beside the executable. Its accented letters, the cedilla, `¡`, `¿`, `œ` and `Œ`
are blank in the file; the port builds them from the font's own letters and marks when it draws.

To use another font for message text, put it at `lang/font.ttf` in the save folder (that wins over this
one) or start the game with `--font <file>`.

## DarkCloudCompendiumThin.ttf

The THIN variant of the same font, unchanged ("THIN Dark Cloud Compendium Community Font", by Dayuppy and Moonbunny; Jeff, who contributes the port's
text work, confirmed on 2026-10-10 that he has Dayuppy's permission to use it). The build copies it to `lang/font_thin.ttf`. The port
draws the area name cards from it when it is there (the cards' painted lettering has a lighter stroke than the regular font's); everything else
uses `font.ttf`. Its digit 5 is the original, not the redrawn one, and no card draws a digit.

## The Sharp text font

When the disc is extracted (`dcdata extract`, the first-run extraction, or `dcdata sharp <data>` for data extracted
before), `tools/dcdata/sharpfont.hpp` reshapes these two fonts letter by letter until, drawn at 14x20, each matches the
game's own letter in `meswin/gaiji.img`: the regular font to the game's bold dialogue letters, the thin one to its
narrower set. It writes `sharp/` in the data folder: the two fonts (`font.ttf`, `font_regular.ttf`) and, per language,
the glyph sheet redrawn from them at six times its size (the narrower set with the soft shadow the game's own letters
carry in their texels). With Text Font set to Sharp the game draws its message text from that sheet exactly where and as
it draws its own letters, and text the port draws from a font uses `sharp/font.ttf`. It is made from the disc, so it is
never part of the repository or a release; until it is made, Sharp draws as Community does. Text Font: Community draws
with these fonts as they are.

## Cinzel-Regular.ttf

Cinzel (Copyright 2020 The Cinzel Project Authors, https://github.com/NDISCOVER/Cinzel), SIL Open Font License 1.1. It is
used only by `tools/textpack/build.py`, to letter the bosses' names (a Trajan-style serif) on the pictures that tool draws.

## SourceSansProSemibold.ttf
Copyright 2010, 2012 Adobe Systems Incorporated﻿
﻿license: SIL Open Font License (OFL)
link: https://www.fontspace.com/source-sans-pro-font-f15035