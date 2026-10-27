# Item Framework

Dark Cloud's item data is compiled into the game. The port turns its item tables into JSON
documents, lets datapacks replace them, and registers every item with the traits that say what it
can do, so other code asks the registry rather than keeping its own item tables.

With no datapacks the game behaves exactly as retail, and saves are untouched.

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
| `Bait` | how far fish notice it | `esa_info` |
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

`darkcloud --export-gameplay <directory>` writes the documents as compiled, so changing one item
means copying its table's document into a pack and editing a line.

What a change reaches:

- Item properties, attachment stats, icons, names, models, prices and bait radii are read from the
  tables whenever they are needed, so a change takes effect at once.
- A weapon's stats are copied into the weapon when it is obtained, so a change reaches weapons
  obtained after it, not those a save already holds.
- An item's `kind` and `index` and a weapon's `owner` and `hole_num` decide where a save's copies
  sit; a document that changes them is refused. A bait's item is fixed too.

## Datapacks

A datapack is a folder in `datapacks/` beside the data folder, holding files at the paths they
replace. `datapacks/datapacks.json` lists the active packs, later ones winning:

```json
{ "schema_version": 1, "load_order": ["lures", "bakery"] }
```

A pack holding `gameplay/items/prices.json` replaces the whole price table; two packs changing
different items of one table do not combine. Packs are read at start-up, so changing them takes a
restart.

A document is tried from the highest-priority pack down. The first whose `schema_version` is 2,
whose `kind` matches and whose values fit retail's fields is used; one that fails prints
`config: <path>: <reason>` and the next is tried, down to the compiled table. A field the port does
not read is reported and ignored. A bad `datapacks.json` disables every pack; a listed pack that is
missing, or is a ZIP, is skipped.

## Quests

Quests can name items by ID and ask about traits (docs/QUEST_FRAMEWORK.md):
`quest::HasItem("vanilla:bread", 2)` looks the ID up as it is checked and never holds for an ID no
item has, and `quest::HasItemWith<item::Fish>(3)` holds while the player carries three items with the
`Fish` trait between them.
