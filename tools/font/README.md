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

## ChronicleTitle-Bold.ttf

Chronicle Title Bold, the title menu's font (`port/src/title/title_menu.cpp`), embedded in the executable as the status
font is. Chronicle Title is the family of the title screen's painted lettering: Bold is the menu rows', and Regular is to
be the thinner PUSH START BUTTON's.

The title screen's menu rows are painted pictures (`start.img` and its French, German, Italian and Spanish copies), their
capitals 16.7 pixels tall in English. As Chronicle Sharp's letters were reshaped until they match the game's, Chronicle
Title's capitals are the painting's: each painted capital is an outline of TrueType's own curves, fitted until, drawn
into the painting's texels at the painting's size, it covers each texel as much as the painted letter does, kept smooth
where the painting's pixels step. The English rows' letters are used, since English is the one compared against;
M, W, Z, the brackets and the acute, which English doesn't paint, come from the other languages' rows at the English
size.

The capitals the painting lacks are built from the painted ones by the painting's rules: stems are its I, horizontal
strokes its E's arms, bowls its P's, diagonals as thick as its A's legs and Z's diagonal and cut level at their ends,
corners rounded as its are (the ends of built diagonals nearly through, as the A's feet are), and the valleys where diagonals meet left sharp (B and D from the E's stem and the P's bowl
and O; F, L and H from the E and I; G from the C; J from the U; K, X and Y from the diagonals; V from the A; Q from the O
and the R's leg). The accents are drawn with the painting's one accent, the acute over Spanish OPCIÓN: the grave is its
mirror, the circumflex and caron the two together, the dots, ring, tilde, macron, breve and double acute drawn with its
stroke and standing as high above the capitals; the cedilla, ogonek and comma below are the earlier Albert Sans marks,
joined to the new letters (an ogonek hangs from the letter's last stroke on the baseline). Every accented capital, and
Æ, Œ, Ø, Ð, Ł, Ŀ, Þ and the German capital sharp s ẞ (a stem, the E's top arm, a diagonal to the waist and the B's lower
bowl), is built from these. Only Ŋ, Ə, Δ and Ω, which none of the game's languages use, are still Albert Sans's.

The capitals are spaced by the usual method for capitals (Walter Tracy's): each side of a letter gets a share of the
space a straight stem gets, by its shape (straight sides all of it, round ones 0.6, open sides and arms less, diagonals
almost none), the stem's space set so the painting's S and T stand as far apart as it has them. The painting's own
spacing is uneven (its I and N sit closer than its U and E), so it isn't copied. Its T and A, where the A tucks under
the T's arm, is made a rule: any pair whose letters can come as close as those two did comes that close, which gives
AT, AV, AY, LT, LV, LY, TA, TJ, VA, YA and YJ. The renderer sets letters by their ink, so the spacing is all kerning:
every pair of capitals kerns by its gap beyond 1.3 pixels, through GPOS class kerning: accented capitals as their
letter, and a letter made of two as the one on that side (Æ as an A on its left and an E on its right).

The lower case, digits and punctuation are still Albert Sans 1.025, copyright 2021 The Albert Sans Project Authors
(https://github.com/usted/Albert-Sans), under the SIL Open Font License 1.1 (no reserved font name), at weight 650,
refitted to the painting's widths and rounded; the painting has none to fit them to. Its licence fields carry the OFL.
The font was made with fontTools, skia-pathops, NumPy, SciPy and scikit-image by scripts kept outside the repository; the
font is the only output that ships.
