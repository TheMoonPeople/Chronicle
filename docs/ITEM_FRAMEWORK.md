# Item Framework

Dark Cloud's item data is compiled into the game. The port turns its item tables into JSON
documents, lets datapacks replace them, and registers every item with the traits that say what it
can do, so other code asks the registry rather than keeping its own item tables.

With no datapacks the game behaves exactly as retail, and saves are retail's.

## Items and IDs

An item has a string ID and the number retail's code and saves know it by. IDs are
`namespace:name`, each part lower-case letters, digits and underscores. Retail's items are
`vanilla:` and their enumerator in `port/include/game/itemdata.hpp` without `ITEM_`, in lower case:
`ITEM_BREAD` is `vanilla:bread`, `ITEM_WEAPON_DAGGER` is `vanilla:weapon_dagger`, `ITEM_ATTACH_FIRE`
is `vanilla:attach_fire`. A number with no enumerator is `vanilla:item_<number>`.

`scripts/port/item_ids.py` writes `port/src/item/item_ids.inc` from the header; run it after
changing `enum Item`. CI runs it with `--check`.

## Traits

`item::Def` (`port/src/item/framework.hpp`) is an item's ID, number and traits. Code asks whether an
item has the trait it cares about (`def->Has<item::Bait>()`, `def->Get<item::Sellable>()`), and
`item::With<T>()` lists every item with a trait.

| Trait | Says | Vanilla items get it from |
|---|---|---|
| `Consumable` | heals HP, restores water, grants a status; its volume | `ITEM_DATA::use_flags`, `kind_flags`, `vol` |
| `Throwable` | can be thrown | `ITEMKINDF_THROWABLE` |
| `QuickSlot` | may go in a quick-use slot, stacking or not | `ITEM_DATA::stack_kind` |
| `Attachment` | stats, elements, monster effectiveness | `AttachList` |
| `Weapon` | owner, base stats, durability, holes | `WeaponList` |
| `Bait` | how far fish notice it, whether it is a lure, what species that don't name it think of it | `esa_info` (eaten, liked by none but those that name it) |
| `Rod` | a fishing rod's five stats, which Enhanced fishing reads (docs/FISHING_FRAMEWORK.md, "Rods") | the Fishing Rod, with the stats that change nothing |
| `Fish` | a fish kept as an item | `ITEM_SORT_FISH` |
| `Sellable` | buy and sell price | `PriceList` |
| `KeyItem`, `QuestItem` | a story item, a dungeon's quest item | `ITEM_SORT_KEY_ITEM`, `ITEM_SORT_QUEST` |

The registry refuses an invalid ID, an ID or number registered twice, and a `Weapon` that is also
`Consumable` or an `Attachment`.

## Documents

Each table is one document under `gameplay/items/`:

| Path | `kind` | Rows (key) | Table |
|---|---|---|---|
| `item_common.json` | `item_common` | `records` | `ComItemInfo`, with each item's `model` from `ITEM_NAME_TBL_NEW` |
| `items.json` | `items` | `items` | `ITEM_LIST` |
| `attachments.json` | `attachments` | `attachments` | `AttachList` |
| `weapons.json` | `weapons` | `weapons` | `WeaponList` |
| `bait.json` | `bait` | `bait` | `esa_info` |
| `prices.json` | `prices` | `prices` | `PriceList` |

```json
{
  "schema_version": 2,
  "kind": "items",
  "items": [
    { "item_no": 199, "sort_key": 6, "unk_02": 0, "use_flags": 0, "kind_flags": 0, "vol": 1,
      "vol_range": 0, "unk_0E": 0, "stack_kind": 2 }
  ]
}
```

Rows are keyed by `item_no` and hold their table's fields under retail's names. A document must
hold every row of its table and every field of each row. `model` is the name of the item's model
files (lower-case letters, digits and underscores), or `null` for none.

The vanilla documents are tracked in `port/gameplay/items/`, as Dark Cloud 2's gameplay documents
are. The build checks them and seals them into Kazarov's gameplay seed (`assets/data.zip` beside
`dcdata`, with each document's digest), and `dcdata extract`, or `dcdata publish-gameplay <data>` on
data extracted before, writes them into the data under `gameplay/`. They hold the tables as compiled
(`ItemDocuments.TheTrackedDocumentsHoldTheCompiledTables`).

`darkcloud --export-gameplay <directory>` writes the documents as compiled (the fishing ones too,
docs/FISHING_FRAMEWORK.md), so changing one item means copying its table's document into a pack and
editing a line.

What a change reaches:

- Item properties, attachment stats, icons, names, models, prices and bait radii are read from the
  tables whenever they are needed, so a change takes effect at once.
- A weapon's stats are copied into the weapon when it is obtained, so a change reaches weapons
  obtained after it, not those a save already holds.
- An item's `kind` and `index` and a weapon's `owner` and `hole_num` decide where a save's copies
  sit; a document that changes them is refused. A bait's item is fixed too.

## Datapacks

A datapack is a folder in `datapacks/` beside the data folder, holding files at the paths they
replace. The game also ships packs of its own, in `assets/datapacks/` beside the executable; a listed
pack the player's `datapacks/` lacks is taken from there, and a player's copy wins over the game's.
`datapacks/datapacks.json` lists the active packs, later ones winning:

```json
{ "schema_version": 1, "load_order": ["lures", "bakery"] }
```

A pack holding `gameplay/items/prices.json` replaces the whole price table; two packs changing
different items of one table do not combine. Packs are read at start-up, so changing them takes a
restart.

A document is tried from the highest-priority pack down, then the data's own. The first whose
`schema_version` is 2, whose `kind` matches and whose values fit retail's fields is used; one that
fails prints `config: <path>: <reason>` and the next is tried, down to the compiled table, which
data extracted without the documents (by the first-run window, which converts nothing, or before
them) uses. The items load once the game has indexed its data. A field the port does
not read is reported and ignored. A bad `datapacks.json` disables every pack; a listed pack that is
missing, or is a ZIP, is skipped.

## Custom items

A pack adds a bag item with a document of its own, `gameplay/items/custom/<namespace>.<name>.json`
(the data may hold some too, read under the packs):

```json
{
  "schema_version": 2,
  "kind": "item",
  "id": "chronicle:test_rod",
  "sort_key": 6,
  "stack_kind": 2,
  "buy_price": 900,
  "sell_price": 450,
  "icon": 56,
  "model": "turisao",
  "text": {
    "en_gb": { "name": "Test Rod", "description": "A rod from a test pack." },
    "fr_fr": { "name": "Canne d'essai", "description": "Une canne d'un pack d'essai." }
  }
}
```

The file's name must be the ID with `.` for `:`, and `vanilla:` is retail's. The fields are
`ITEM_LIST`'s and `PriceList`'s under retail's names, 0 where left out; `icon`, a retail icon number;
`model`, a model in `dun/item/main_data`; `text`, the name and description by language file, falling
back to English and then to the first given; and an optional `bait` with its `radius`, `lure` (true for
one that comes back to the bag after a catch rather than being eaten, and is never lost; false if left
out) and `default_affinity` (the liking of a species that doesn't name it, 0 if left out;
docs/FISHING_FRAMEWORK.md); and an optional `rod` with any of its stats, `heft`, `control`, `spring`,
`balance` and `lure` (0 to 100, retail's rod's where left out; docs/FISHING_FRAMEWORK.md, "Rods"). A
document
that cannot be read is left out with a `config:` line saying why, and a field one holds that this
version does not read is named in another.

Custom items get numbers from `0x800` in ID order, past the system messages' ids, since the game
looks an item's name up at its number + 100 and its description at its number + 500. Retail's
lookups answer them: `GetCommonItemInfo` and `GetItemData` return the document's rows, prices,
models and icons come from the document, and a name or description the message files lack is the
document's text in the current language (`item::CustomMessage`); the disc's own messages always win.
Retail tells kinds apart by number ranges and takes anything from 257 up for a weapon, so the places
that would take a custom item for one ask `item::KindOf` instead (None, Attachment, Item or Weapon),
and custom items are bag items wherever they go: the bag, quick slots, storage and shops.
`--give` takes custom IDs (`--give chronicle:test_rod`).

## Copies

An item whose document declares `state`, names to the numbers a new copy starts with, keeps state of
its own in each copy, with `state_version` (the version of its keys) beside it:

```json
"state": { "size": 31.5 },
"state_version": 1
```

Such a copy travels through retail's code under a handle, a number of its own from `0x1000` up to
`0x7E00` (a description is looked up at the number + 500, an `s16`), standing wherever retail keeps
item numbers. Everything the host answers resolves a handle to its item first (`item::Definition`),
and the copy (`item::FindCopy`) holds its item's ID, its instance, a number that stays its own for
good, and its state. `item::NewCopy` makes one; the item's own number in the bag, as a shop sells it,
stands for a copy with the starting state. Handles no longer in the bag, its quick slots or storage
are freed between areas and when saving.

## Saves

`save.dat` stays retail's image: as a save is written, its custom items and copies are taken out of
the bag, the quick slots and storage, leaving the empty slots retail would hold, and kept by ID in
`chronicle.dat` beside it (docs/PC.md, "Saves"), with what is left of each and a copy's instance and
state. A save without custom items is byte for byte retail's and has no `chronicle.dat`. Loading puts
them back in their slots. The same handle found in two places becomes two copies, each with an
instance of its own.

A saved item whose definition is missing, its pack turned off or removed, loads as an orphan: an
"Unknown item" (`item.orphan.name`) that keeps its ID, instance and state, cannot be used or sold,
and is saved again as it was. Once its definition returns it is itself again. The save and load
screens show a "!" after the number of a save holding one.

## Quests

Quests can name items by ID and ask about traits (docs/QUEST_FRAMEWORK.md):
`quest::HasItem("vanilla:bread", 2)` looks the ID up as it is checked and never holds for an ID no
item has, and `quest::HasItemWith<item::Fish>(3)` holds while the player carries three items with the
`Fish` trait between them.
