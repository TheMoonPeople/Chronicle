# areapack

Builds one folder per dungeon, town or event map from the player's own disc: the disc files the area uses, its
pictures as PNG, and the game's numbers for it (monsters, floors, treasure boxes ...) as JSON. Someone can then work
on one area at a time, and has the list of every file and table that area is made of.

    python tools/areapack/areapack.py list    "Dark Cloud (USA).iso"
    python tools/areapack/areapack.py extract "Dark Cloud (USA).iso" packs --area d01 e01
    python tools/areapack/areapack.py extract <folder dcdata extract wrote> packs          # every area

An ISO is unpacked first with `dcdata` (found in `build/`, on `PATH`, or via `--dcdata`) into `packs/_disc`; a folder
that `dcdata extract` already wrote is used as it is. Needs Python 3.9+ with numpy and Pillow. Tests:
`python -m unittest discover -s tools/areapack`. The full guide is `docs/AREAPACK.md`.

- `areapack.py` the command, the packs and their output. `catalog.py` which files belong to which area.
- `cdata.py` reads the decomp's C tables (`ps2/src`, `ps2/include`) as data. `cfg.py` reads the disc's `.cfg` files.
- `tim2.py` finds and decodes the TIM2 pictures inside the disc files.
- Nothing in this folder comes from the disc; the packs are made from the player's own copy and are not committed.
