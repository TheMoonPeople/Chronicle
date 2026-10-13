# textpack

Draws the localization's pictures of text (area name cards, dungeon floor labels, boss names) from the words in
`strings/*.json`. See "Pictures of text" in `docs/LOCALIZATION.md` for what the port does with them.

    python tools/textpack/build.py --originals <extracted textures> --out <save folder>/lang/textures [--scale 1|4]

- `build.py` the command. `textpack.py` draws text with the Compendium font (it builds the accented letters the font
  leaves blank). `plates.py` rebuilds a card's backdrop without its lettering by combining the languages' cards.
  `slots.py` finds the word boxes of an existing sheet. `shadow_preview.py` shows the shadow the port casts.
- `build.py --only plates` writes the cards' text-free backdrops and layouts for the port's runtime lettering (`area_card.cpp`).
- `strings/titles.json`, `title_styles.json` (per-area look), `floors.json`, `bosses.json` are the words. Nothing in
  this folder comes from the disc; the pictures are made from the player's own textures and are not committed.
- Needs Python 3 with numpy, Pillow and opencv-python.
