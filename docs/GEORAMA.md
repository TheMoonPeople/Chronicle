# Georama

The Georama's tables are documents a datapack can change (docs/ITEM_FRAMEWORK.md, "Datapacks"):
what each town's parts are, and which Atla each dungeon hands out. The game reads them at start-up
into the tables retail compiled in (`port/src/game/editpartsdata.cpp`); without a pack, or for a town
or dungeon a document leaves out, the compiled tables stay. `darkcloud --export-gameplay <directory>`
writes both documents as compiled, a starting point for a pack.

| Path | `kind` | Holds | Table |
|---|---|---|---|
| `gameplay/georama/parts.json` | `georama_parts` | each town's parts and elements | `EditPartsData`, `EditElementData` |
| `gameplay/georama/atla.json` | `georama_atla` | each dungeon's Atla | `AtraAppearData` |

Towns are `norune`, `matataki`, `queens`, `muska_lacka`, `yellow_drops` and `dark_heaven_castle`;
their dungeons, in the same order, `divine_beast_cave`, `wise_owl_forest`, `shipwreck`,
`sun_and_moon_temple`, `moon_sea` and `gallery_of_time`.

## Parts

```json
{
  "schema_version": 2,
  "kind": "georama_parts",
  "towns": [
    {
      "town": "norune",
      "parts": [
        { "part": 14, "max": 25, "stackable": 10, "picture": 13, "name_message": 14, "kind": 0, "elements": [] },
        { "part": 1, "max": 1, "stackable": 0, "picture": 1, "name_message": 1, "kind": 1, "elements": [
          { "id": 0, "requires": -1, "models": ["saku"], "villager": 2 }
        ] }
      ],
      "elements": [
        { "element": 40, "picture": 0, "name_message": 0, "unk_08": 5 }
      ]
    }
  ]
}
```

A town's `parts` are its 24 parts by number (`part`, 0 to 23); a part left out is empty. `max` is the
most copies the player can hold. A part whose `stackable` is above zero gains five copies with each
Atla, up to `max`; otherwise one Atla gives the part. `picture` and `name_message` pick its picture and
its name; `kind` is retail's classification. Its `elements` (at most six) are the pieces the player
adds to it: `id`, the element slot that must be in place first (`requires`, -1 for none), the model
names it shows (`models`, at most four) and the villager it belongs to (`villager`, -1 for none).

A town's `elements` are its element records by number (0 to 99): each one's `picture` and
`name_message`, and retail's `unk_08`, whose use is unknown. One left out is empty.

## Atla

```json
{
  "schema_version": 2,
  "kind": "georama_atla",
  "dungeons": [
    {
      "dungeon": "divine_beast_cave",
      "atla": [
        { "part": 14, "floor": "first_half", "count": 2 },
        { "element": 0, "floor": 1, "count": 1 }
      ]
    }
  ]
}
```

Each Atla names a `part` (0 to 39) or an `element` (0 to 59) of the dungeon's town, and its `floor`:
a floor counted from one, or any floor of the dungeon's `first_half` or `second_half` as the game
divides it. `count` (1 if left out) is how many floors a half's Atla lies on.

A dungeon deals out its Atla once, the first time the player enters it, and the save keeps them, so a
changed list reaches a dungeon not yet entered. Parts' limits are read whenever an Atla is picked up,
so a change to them takes effect at once.

## What is refused

A document the game could not use is reported (`config: <path>: <reason>`) and the compiled tables
stay. That is one that names a town, dungeon, part, element or floor that does not exist, gives one
twice, or gives a dungeon more Atla than the save holds (100), or more on a half than that half's
floors can take (8 a floor, on the floors the game puts Atla on, less the ones fixed there), which
would have the game look for room forever.
