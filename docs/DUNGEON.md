# Dungeons

What a dungeon's floors give, as documents a datapack can change, what its floor select tells the
player, and changes to how its events play.

## Loot

`gameplay/dungeon/loot.json` (kind `dungeon_loot`) holds what retail compiled into `dungeonparts.cpp`, by
item ID so a pack can put its own items in:

- **`chests`** and **`back_floor_chests`**: the lists a treasure box draws from, each for one floor
  (counted from one) or for either half of the dungeon (`"lower"`, before the floor retail's `floorNum`
  divides it at; `"upper"`, from it on). A floor's own list wins over its half's. A small box draws what
  is not a weapon, a large box a weapon.
- **`clown`** and **`back_floor_clown`**: the clown's two offers, each from its own list, for each half.
- **`turned_down`**: each item's chance out of a hundred of being drawn and put back, the dungeon's for
  its boxes and its clown alike; a weapon the party already holds is turned down less (10 for one copy,
  20 for more), as retail has it. An item left out is never turned down.

It is read as the item documents are (docs/ITEM_FRAMEWORK.md, "Datapacks"), every dungeon once, with a
copy as compiled tracked in `port/gameplay/dungeon/loot.json`. Each pack's `loot.patch.json` (kind
`dungeon_loot_patch`) then applies over it, the lowest priority's first: a row names a dungeon and the
fields it replaces, and its `turned_down` adds to or changes the dungeon's chances. A patch that cannot be
used is reported and left out whole; an item ID no loaded item has is reported and left out of its list.
Where items may lie on each map part, and how many floors a dungeon has, stay compiled.

The draws are retail's, random number for random number (`DungeonLoot.WithoutPacksBoxesAndTheClownDrawAsRetailDid`),
except that a clown list a pack makes all but impossible stops after as many tries as a box gets.

## Floor select

The floor select is drawn on the UI framework (docs/UI_FRAMEWORK.md) as retail drew it, and takes the
mouse: a hover puts the cursor on a floor, a click goes down to it, the wheel moves a floor. In English its
floor words (B, Territory, Layer...), the floors' numbers and the bosses' names are drawn in the captions' font,
each fitted to the box its painted letters fill (`DrawFloorLabels`, `port/src/game/menu_draw.cpp`); other
languages keep the painted pictures.

The dungeon's name plate is a blank plate with the name written on it, in every language: the plate
(`lang/textures/plates/dungeon_name.png`, CC0, the disc's size) is laid over the names painted in the select's
sheet as it loads, and the select writes the dungeon's name from the game's own system messages (the world
map's), in capitals, in the painted names' colours and height (`port/src/ui/dungeon_plates.cpp`). Without the
plate the painted names stay.

It also warns of the floors that limit the party, always, since the warning changes nothing in play.
A limited floor's row has a `!` after its kill count, and with the cursor on it the window below says
what the limit is in the words the dungeon says it with on arrival (`dunmsd00`, messages 11, 13 and
14), so every language has them. The limits (`port/src/dungeon/limits.cpp`) are of two kinds:

- **Fixed:** floors whose script always limits them, read out of retail's floor scripts
  (`dun/script/dNN/event.stb`, program 100): Divine Beast Cave B7 (weapons can't be changed) and B11
  (only Xiao), Wise Owl Forest 4 (only Xiao) and 12 (only Goro), Sun and Moon Temple 5 (only Goro)
  and 13 (only Ungaga).
- **Rolled:** three or four floors of each dungeon after the first, rolled when the game begins
  (weapons can't be changed, or the party gets thirsty five times as fast) and kept in the save. A
  roll that lands on a floor the script builds from a fixed map, or on the boss's floor, never
  applies there, and is not warned of. A fixed limit wins over a rolled one, as the script sets it
  last.

## Events

Changes to how a dungeon's events play, each off unless `gameplay/dungeon/rules.json` turns it on, so
without one the game plays as retail. A datapack turns them on (the Quality of Life pack does).

### The changes

| Change | What it does | Where |
|---|---|---|
| Key item filter (`key_item_filter`) | An event's item window lists only the items the event takes: a floor's exit its item, a back floor's entrance its item, a locked door its key, a quest event its item. Retail listed the whole bag. A window that names no items still lists them all. | `BtMiniItemSelect_Loop` (`port/src/game/btitem.cpp`); the board's `EVENT_ITEM_MODE_USABLE` (`port/src/game/menu_save.cpp`) |
| Auto-open locked doors (`auto_open_locked_doors`) | Cross at a locked room door opens it when the bag holds a key for the dungeon's locked doors, using the key up as choosing it would. Without one, Cross does as in retail. | `AutoOpenLockedDoor`, `TakeAutoKey` (`port/src/dungeon/doors.cpp`), at the event press in `port/src/game/dun/movechara.cpp` |

The items an event takes are the ones its script names (`_ITEM_USE_WINDOW`). A door is told by the map
part that placed its event (`CDungeonEvent::parts_id`, one of the `MAP_PARTS_ROOM_GATE_*`): Cross there
runs the door's script as Square does, asking for an item, and its window is given the key without
opening. Which key that is comes from the items' `DungeonKey` trait (docs/ITEM_FRAMEWORK.md, "Traits"),
so a pack that gives the doors another key changes it here too.

### Rules document

`gameplay/dungeon/rules.json`, read as the item documents are (docs/ITEM_FRAMEWORK.md, "Datapacks"):
the highest-priority usable document, then the data's own, or every change off. A switch left out is
off. The document as compiled is tracked in `port/gameplay/dungeon/rules.json`, every switch off.

```json
{
  "schema_version": 2,
  "kind": "dungeon_rules",
  "key_item_filter": true,
  "auto_open_locked_doors": true
}
```

`darkcloud --export-gameplay <directory>` writes it with every switch off.
