# The Demon Shaft georama mod

`MOD_DEMON_SHAFT_GEORAMA` adds a seventh georama, the Demon Shaft's own. Its atla lie on the Demon
Shaft's floors, and the town it builds is its own: its parts, placement, chips and atla are kept apart
from every other town's. It has no models of its own. It is built on the ground, parts, part names
and pictures of a town of the game, the donor (Muska Lacka, `DSG_DONOR_TOWN` in
`ps2/include/dsgeorama.hpp`).

Retail has none of this. The PC port builds it by default; `-DMOD_DEMON_SHAFT_GEORAMA=OFF` leaves it
out. The PS2 build leaves it out unless asked, because with it the executable no longer matches retail:

```sh
DSG=1 ./build.sh ps2-pal      # or ps2-ntsc; DSG=0 (or nothing) builds retail again
```

## Playing it

- **Atla.** About one Demon Shaft floor in three has an atla, and a few have two. Only Toan can pick
  one up, as elsewhere. Together they hand out every part and chip of the donor's dungeon (Sun and
  Moon Temple) as many times as that dungeon does, but no villagers. Once all of them have been picked
  up, no more appear. Floors are drawn anew on every visit, as the Demon Shaft's always are, so an atla
  that was not picked up is offered again later.
- **The town.** On the world map, put the cursor on the Demon Shaft and press **Triangle** instead of
  Cross, then Cross (or Triangle) to go. You arrive in the donor town's ground with the Demon Shaft's
  georama on it, and build it with the georama editor as in any town. Leave it by the world map as
  usual. Cross on the Demon Shaft still goes to the Demon Shaft.
- **Saving.** A save made in the Demon Shaft's georama loads back into it. A save made before the mod
  still loads, with the Demon Shaft's georama empty.

## How it works

The georama is number 6 (`DSG_GEORAMA_NO`), one past retail's last. All of its data lives in
`DsgSave` (`SV_DSG_SAVE`), outside `CSaveData`, so retail's save layout and every offset in it stay
the same.

- **Save data.** `CSaveData`'s georama accessors (`GetGrdData`, `GetEditPartsInfo`, `GetParts`,
  `GetGrdNPCData`, `GetElemData`) give `DsgSave`'s records for georama 6. While the player stands in
  the Demon Shaft's georama they also give them for the donor town's number, so the editor, the
  georama menu and the town's scripts, which all ask by the town's map number, work on it unchanged.
  `DsgTownActive` marks that, and `MapNo` must still be the donor's, so the donor's dungeon never reads
  the wrong records.
- **Tables.** `GetEditAtraData`, `GetEditAtraPartsData` and `GetAtraMsgNo` read the donor's tables
  for georama 6.
- **Entering.** The world map sets `DsgTownRequest` and jumps to the donor's map number. `EditInit`
  calls `DsgEnterTown`, which turns `DsgTownActive` on only for that jump.
- **Atla.** `BtAtraFloorCyoice` asks `DsgAtlaFloorChoose` for the Demon Shaft's floors. The records
  are filled from the donor's `AtraAppearData` the first time they are wanted, leaving out ids from
  80 on (villagers). `BtAtraGetShort_Loop` reads a record's id with `DsgAtlaId`, and
  `getAtraToSaveData` gives the part to georama 6 and counts the record down. The georama
  presentation and the part names are the donor's.
- **The save file.** `SV_DSG_SAVE` is written after retail's image (save data, version, checksums),
  with its own magic and checksum. A file that ends with retail's image, or whose record is broken,
  loads with an empty georama.
- **PS2 memory.** The main image ends where the overlays are loaded, at `0x1DC4000`. The mod's code
  and `DsgSave` take room there, so the PS2 build gives up 16 KB of `GlobalDataBuffer`
  (`GLOBAL_DATA_QUADS`). The busiest modes (the opening and the georama editor) still leave about 270 KB of it unused.

## What is not done

- No new models, textures or text. The town looks like the donor and its parts carry the donor's
  names. Nothing on screen names the Demon Shaft's georama: the world map still says "Demon Shaft"
  when Triangle is pressed.
- The donor's villagers never come, so its villager requests and events do not happen in the Demon
  Shaft's georama. The donor's own scripts still run there, on the Demon Shaft's georama.
- The georama status screens that list all six towns do not list the seventh.
- The town's own exits open the world map through the town's event scripts, which are data on the
  disc. Whether they make the jump between the donor town and the Demon Shaft's georama (the same map
  number) is untested. The world map from the main menu jumps directly.
- It has not been run on a PS2 or in an emulator. The PS2 build has not been compiled with the mod
  on. The PC port compiles with it, and the unit tests in `port/src/tests/dsgeorama_test.cpp` and
  `save_slots_test.cpp` cover the atla, the save redirection and the save file.
