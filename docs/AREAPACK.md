# Area data packs

`tools/areapack` turns the disc into one folder per place, so a single dungeon or town can be rebuilt, redrawn or
upscaled on its own, with everything that place needs listed in one file. It is for people making replacement
art, new data or reference material; the game itself does not use it.

    python tools/areapack/areapack.py extract "Dark Cloud (USA).iso" packs --area d01
    python tools/areapack/areapack.py list "Dark Cloud (USA).iso"

The source is the ISO (unpacked with `dcdata`, see `docs/PC.md`) or a folder `dcdata extract` already wrote. Nothing is
written into the repository; the packs hold the player's own game data and are never committed. A full run of every
area takes several minutes and needs several GB free (shared files are copied into each area that uses them).

## What an area is

| id | what | how its files are found |
|---|---|---|
| `d01` .. `d07` | The seven dungeons, in the game's own order (Divine Beast Cave first, Demon Shaft last). | `dun/dNN/`, `dun/dNN*.cfg` (the floor definitions), `dun/mpd_pack/dNN*`, `dun/script/dNN/`, `dun/ex/dNN*`, and the monster models and scripts in `dun/monstor/` that the dungeon's floors use. |
| `e01` .. `e05`, `s01` .. `s99`, `interior` | Towns and event maps (`gedit/<id>/`). | The whole `gedit/<id>/` folder. The name is the Japanese label at the top of the map's `mapinfo.cfg`; the four towns and a few more have an English name too. `related_dungeons` links a map to a dungeon when its label names one (a boss room, a stair landing). |
| `_characters`, `_items`, `_effects`, `_audio`, `_ui`, `_cutscenes`, `_text_scripts`, `_town_shared`, `_misc` | What no area owns: the playable characters and weapons, item models, effects, sound, menus and fonts, cutscenes, messages and scripts, files every town shares, everything else. | By folder. |

Every disc file lands in at least one pack (a file no area claims goes to `_misc`); `pack.json` lists any that somehow did not.
A file several areas use is copied into each and marked `also_in` in `area.json`.

## What a pack holds

    d01/
      area.json     files (path, size, SHA-1, also_in), pictures, table names
      README.md
      raw/          the disc files, at their disc paths
      textures/     one PNG per TIM2 picture found in those files (RGBA; --indexed also writes the paletted originals)
      tables/       JSON, see below
      replace/      empty; for edited files laid out like raw/

Each entry of `area.json`'s `textures` names the picture's file, byte offset, name, size, TIM2 types and an MD5 of its
pixels, which is what is needed to match a redrawn picture back to where the game reads it.

### Tables

The numbers come from the decomp's own C source (`ps2/src/monstorunit.cpp`, `ps2/src/dungeonparts.cpp`,
`ps2/include/monstorunit_floor_data.inc`), read at run time with the field names its structs give them, so they stay
true to the decomp as it is corrected.

| file | dungeons | holds |
|---|---|---|
| `monsters.json` | yes | Every monster any floor can hold: name, model and script files, HP, family, defence, hardness, EXP, money, drops, steal and rare items, damage taken from each attacker, shot effects, knockback. |
| `floors.json` | yes | Per floor, front side and back side: the monster count, and each monster with its percentage chance. |
| `treasure_boxes.json` | yes | The item lists a box can draw from, by floor, with item names. |
| `piero_offers.json` | yes | The two item lists the clown offers from, by floor. |
| `item_reject_rates.json` | yes | For every item, the chance out of 100 that it is turned down when drawn in this dungeon. |
| `item_free_areas.json` | yes | The map parts where items may be put down, and the rectangles within them. |
| `configs.json` | all | Every `.cfg` of the area read command by command: lighting, fog, background, and each map part's models, collision, camera and torches. Comments are decoded from Shift-JIS. |

`packs/_tables/monsters.json` has all 167 monsters and `enums.json` the item, monster-family, character and
treasure-box names with their numbers.

## What it does not do

- Models (`.mds`, `.chr`), motions, collision, the map pack (`.mpd`), messages and scripts (`.mes`, `.stb`, `.scn`) are
  copied as they are. Their formats are not decoded here, so a dungeon's room layouts and event scripts are not in `tables/`.
- Floors are built at random in the game from the map parts; there is no per-floor map to extract.
- A few table fields are unnamed in the decomp (`unk_6C`, the meaning of a floor's `monster_count`); they are kept under
  the plain name with a note, not guessed at.
- The TIM2 reader unswizzles 8-bit pictures the way `texture_port.cpp` does. A picture that still looks scrambled is a
  format case the reader does not know yet, not a damaged disc.
- Putting a redrawn picture back is not part of this tool.

## Checked against

The USA disc and a folder unpacked from it: all 5,425 files assigned, `d01` (82 files, 61 pictures, 14 monsters, 15
floors a side) and `d07` (Demon Shaft, 100 floors), a town (`e01`), two event maps and `_characters` written whole.
