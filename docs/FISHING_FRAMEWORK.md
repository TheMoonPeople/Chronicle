# Fishing Framework

Dark Cloud's fishing tables are compiled into the game. The port turns them into JSON documents that
datapacks can replace, as the item tables are (docs/ITEM_FRAMEWORK.md), and keeps the species and
places in registries (`dc::fishing`, `port/src/fishing/`) that retail's fishing code asks. A fishing
session is the framework's too: retail's steps, moved out of `EdMoveChara`, asking the place's stages
what happens at each.

With no datapacks fishing plays as retail: the same fish at the same places with the same odds,
points, names and prizes, except where the port makes the bite readable and fixes a size roll:

- A fish that takes the hook holds it for half a second to a second (the `retail` bite's
  `hook_ticks`); retail's third of a second to five sixths left too little time to press Cross.
- The moment it takes the hook is marked by a splash, rings at the float and a strong rumble
  (`BiteCue`, `fishing/angler.cpp`), where retail's tug and rumble were much like a nibble's.
- Sizes near a kind's largest are ramped into it rather than piling onto it (`SmoothFishSize`).

## Documents

Each table is one document under `gameplay/fishing/`, read like the item documents: from the
highest-priority pack down, then the data's own, then the table as compiled. They are tracked in
`port/gameplay/fishing/`, sealed into the gameplay seed with the item documents, and written by
`darkcloud --export-gameplay <directory>`.

| Path | `kind` | Rows (key) | Table |
|---|---|---|---|
| `fish.json` | `fish` | `fish` (`fish_kind`) | `fish_info`, with each kind's model (`GetFishFileName`) and name message (`GetFishMsgNo`) |
| `places.json` | `fishing_places` | `places` (`spot`) | `FishingLoadFish`'s spots, fish counts and spawn odds |
| `prizes.json` | `fish_prizes` | `prizes` (in order) | `exitemlst`, the fishing exchange |

A document must hold every row of its table and every field of each row; one that cannot be used
prints `config: <path>: <reason>` and the next is tried.

**`fish`**, one row per retail kind, 0 to 17:

```json
{ "fish_kind": 5, "id": "vanilla:fish_5", "model": "chara/f06a.chr", "name_msg": 45,
  "model_size": 21.0, "min_size": 10.0, "max_size": 16.0, "min_fp": 200, "max_fp": 400,
  "food_affinity": [ { "bait": "vanilla:poisonous_apple", "affinity": 1.0 } ],
  "mardan_garayan": true }
```

- `min_size` is the kind's usual size, the centre of `CFish::SetScale`'s roll; points are linear in
  the size from `min_fp` to `max_fp`.
- `food_affinity` names baits by item ID (an item with the `Bait` trait); a bait left out is not
  liked. A fish notices a bait at its liking × 0.3 and takes it at × 0.5, so 2 is the most that
  means anything.
- `mardan_garayan` counts a catch toward the Mardan Garayan (retail: kinds 5 and 17).
- The kinds and their IDs are fixed: the save's records name a fish by kind.

**`fishing_places`**, one row per spot the town scripts open (0 Norune, 1 and 2 Matataki, 3 Queens,
4 Muska Lacka):

```json
{ "spot": 2, "id": "vanilla:matataki_2", "fish_count": 5, "draw_under_water": true,
  "spawn": { "stage": "retail",
             "rare": { "interval": { "day": 50, "evening": 20, "night": 35, "morning": 25 },
                       "fish": [ { "fish": "vanilla:fish_17", "weight": 1 }, { "fish": "vanilla:fish_5", "weight": 4 } ] },
             "fish": [ { "fish": "vanilla:fish_2", "weight": 1 }, { "fish": "vanilla:fish_4", "weight": 1 },
                       { "fish": "vanilla:fish_6", "weight": 1 } ] },
  "bite": { "stage": "retail", "hook_ticks": [30, 60], "lose_bait_early": 0.2, "lose_bait_escaped": 0.3 },
  "fight": { "stage": "retail", "ticks": 120 },
  "outcome": { "stage": "retail" } }
```

- `fish_count` is 1 to 6; `draw_under_water` is false only at the sea, whose fish aren't drawn.
- `spawn` names the stage that chooses the fish, with its settings beside the name. The `retail`
  stage draws each fish from `fish` by weight, or, one time in the time of day's `interval`, from
  `rare`. Retail rolled each spot its own way; the odds are the same (`FishingDocuments.EachPlaceSpawnsWithRetailsOdds`),
  the draws for a given seed are not.
- `bite`, `fight` and `outcome` name the place's other stages (below).
- Where the water is stays in the town scripts, which pass its bounds and level; a document changes
  the places they open, not adds new water. Debug mode's fill of every spot stays retail's.

**`fish_prizes`**, the exchange's 35 rows in its order, `{ "item": "vanilla:antidote_drink", "price": 5 }`:
any item by ID, and its price in fish points. The exchange screen draws a fixed list, so a document
changes prizes and prices, not how many there are.

## Custom species

A pack adds a species with a document of its own, `gameplay/fishing/species/<namespace>.<name>.json`
(the data may hold some too, read under the packs), as custom items are added:

```json
{
  "schema_version": 2,
  "kind": "fish_species",
  "id": "chronicle:moon_trout",
  "model": "chara/f06a.chr",
  "model_size": 21.0, "min_size": 12.0, "max_size": 24.0,
  "min_fp": 50, "max_fp": 90,
  "food_affinity": [ { "bait": "vanilla:carrot", "affinity": 1.0 } ],
  "mardan_garayan": false,
  "text": { "en_gb": { "name": "Moon Trout" }, "fr_fr": { "name": "Truite de lune" } }
}
```

The fields are a `fish.json` row's (with `item` optional, as there), the name by language file (falling back to English, then
the first given) in place of `name_msg`, and `mardan_garayan` false if left out. The file's name must
be the ID with `.` for `:`, and `vanilla:` is retail's. Custom species get kinds from 256
(`fishing::kCustomKindFirst`) in ID order, and a place's spawn names them like any species. The catch
message gives the name by laying it over the evaluation window (`ClsMesSetMessageOverlay`), since the
towns' message files have none.

**Records.** `save.dat`'s `fish_data` names retail kinds, so a custom species' catches go instead into
`chronicle.dat`'s `FISH` chunk (docs/PC.md, "Saves"): for each species, by ID, how many have been
caught, the largest and the baits it has been caught with (`fishing::RecordOf`), with the rod chosen
from the fishing menu. Records of a species
whose document is gone are kept and written back until it returns. Its points still go to the save's
fish points, which are only a number.

## Baits

Any item with the `Bait` trait is a bait: retail's thirteen, and custom items whose document has a
`bait` (docs/ITEM_FRAMEWORK.md, "Custom items"). The bait picker (retail's event item board) offers
every one the player carries and nothing else (`EventItemListed`; the board's first row of slots is
drawn where retail drew it a pixel low, which left a line of the town showing above it), and the town's script attaches the one chosen with its own code, the port putting a bait its list
lacks on it (`ListBaitForScript`, which fixed PAL's five baits that were eaten and never attached).
`esa_type` and a fish's `food_kind` index the framework's list of baits (`fishing::BaitAt`): retail's
in `esa_info`'s order, then the custom ones.

A fish that takes the hook eats the bait, as retail. A lure (`"lure": true`) comes back to the bag
instead and is never lost to a nibble or a fish that lets go; it has to come off the hook as the fish
is landed, because the landed fish's model is put where the town's script built the bait's (the
event buffer starts at the villagers' buffer's free end), so it is put on again from the picker. A species that doesn't name a bait in its `food_affinity` likes it at the bait's
`default_affinity`. Leaving fishing or changing the bait gives back the one on the hook, as retail.

## Stages

What happens at a place is decided by four stages, each chosen by name in its row with its settings
beside the name (`port/src/fishing/framework.hpp`; a new one is a class and a line in
`RegisterBuiltinStages`, `stages.cpp`). The port ships `retail` of each:

| Stage | Decides | `retail` |
|---|---|---|
| spawn | the fish in the water when fishing starts | each fish drawn by weight, the rare table one time in `interval` |
| bite | what each fish does about the bait, asked by `CFish::Step` when its last action has run out, and when the bait is lost (never a lure) | notices at its liking x 0.3, takes the bait and the hook at x 0.5, loses interest at 0.1, every 21 to 50 ticks; holds the hook `hook_ticks`; loses the bait at `lose_bait_early` for Cross on a nibble and `lose_bait_escaped` for a fish that lets go |
| fight | from the hook set to the fish landed or the line snapped, with the fish's state across the fight (`fishing::Fight`) | none: the controller rumbles for `ticks` while the fish's model loads |
| outcome | what a landed fish gives | the Mardan Garayan count, the points, the records and the evaluation message |

The port also ships a `brisk` bite, Dark Cloud 2's (`GetUkiWaitTime`): once the float is out, one
roll chooses a fish in the water by its liking for the bait (not at all, below 0.35, below 0.75, or
more weigh 0, 0.5, 1 and 1.5) against `nothing`'s weight, and rolls again `retry` ticks after nothing
comes; a bait no fish there likes never brings one. The chosen fish swims to the bait in at least
`wait` ticks (a range), faster than retail's crawl where it has far to come (up to 0.6 a tick,
`fishing::ApproachSpeed`), nibbles there for its liking's `nibble` range (`low`, `normal`, `high`),
then holds the hook for its `window` range; missed, or kept from the bait for twice its wait, it swims
off and the next roll comes. It nibbles only at the bait, because Dark Cloud 1's fish can be seen. The
other fish leave the bait alone, and turning or reeling in starts the cast over, sending the fish at
the bait away at once. Its losses are `lose_bait_early` and an optional `lose_bait_escaped`:

```json
"bite": { "stage": "brisk", "wait": [240, 480], "retry": 600, "nothing": 1.0,
          "nibble": { "low": [100, 160], "normal": [50, 100], "high": [30, 60] }, "window": [40, 75],
          "lose_bait_early": 0.2 }
```

And a `tension` fight, Dark Cloud 2's (`BattleLoop`, `LineTensionStep`): the fish pulls and the
angler winds it in without letting the line's tension reach the top, where it snaps. Dark Cloud 2 runs
at 30 ticks a second, so its rates are halved and its durations doubled to keep its pace; the numbers
below are its own, in its ticks, where not said otherwise.

- **Reeling:** holding Cross winds in `reel_speed` of line a port tick and raises tension by 0.03 x the
  fish's pull, 26.5% more than Dark Cloud 2 (the port's). The fish is held at the line's end, so the line
  out is how far it is; once that is within `land_distance`, after `min_ticks` port ticks, it is landed
  as in Original. Left alone, tension eases `ease` a port tick (0.005, two and a half times Dark Cloud
  2's; its own made a fight mostly waiting for the line to slacken), and the fish's vigour recovers.
- **Mashing:** more than 5 presses of Cross and pushes of the stick in quick succession make tension
  climb, more than 20 make it jump.
- **The fish veering** (the port's answer, in place of Dark Cloud 2's leaning on the rod): every 60 to
  139 ticks it swims to one side for `veer_ticks` port ticks (60, Dark Cloud 2's second), shown on
  screen. The left stick held that way takes three quarters of what reeling adds, winds the line in 5%
  faster and tires the fish, for as long as the arrow shows and 10 port ticks after, for a person's
  reaction. Held the other way,
  or at all once the veer is over, the rod is out of position: tension climbs by half again what
  reeling adds, reeling or not.
- **The pull** is the species' `fight.pull` (`fish.json` row or species document; 0.5 if left out, the
  least at which holding Cross through the fight snaps the line) at its usual size, in proportion to its size, and grows up to half again with its vigour. Tension can't
  fall below a floor rising 0.0001 a tick, so a fight ends in about three minutes at most.
- **Snapped:** the fish swims free, and the bait goes with it at `lose_bait_snapped`. The bait stays on
  through the fight and is taken as the fish is landed.

```json
"fight": { "stage": "tension", "land_distance": 8.0, "min_ticks": 200, "reel_speed": 0.15, "lose_bait_snapped": 0.3 }
```

The gauges are the port's, since Dark Cloud 1 has none (`fishing::DrawFight`, drawn after the town's
messages): a paper plate at the foot of the screen with the tension, shading toward red, and the line
still to reel in; "HIT!" as the fight starts; and the side the fish veers to. A warning beep sounds
above 0.7 (retail's `SE_WARNING`), and the line breaks with `SE_WEAPON_BREAK`. The rumble follows Dark
Cloud 2's.

The port also ships a `keep` outcome: the fish goes into the bag as its species' `item` (below), and
gives its points and goes in the records (with the Mardan Garayan count) only where the place's
settings say `"points": true` and `"records": true`. The evaluation message still shows, with no points
unless they were given. A full bag overflows as an event's gift does, the player choosing what to
throw away. A species without an item isn't kept.

## Rods

A rod is an item with the `Rod` trait, carrying Dark Cloud 2's five rod numbers under names for what
each does. Only the stages that read rods use them: `brisk` (Lure) and `tension` (the rest); in
Original, and to the town scripts, which check for the Fishing Rod before offering to fish, a rod is
only a rod. The Fishing Rod has the stats that change nothing, so it fights as the stages' numbers say;
better rods are custom items with a `rod` (docs/ITEM_FRAMEWORK.md). The rod in use is the one chosen
from the fishing menu while it is in the bag, else the Fishing Rod (`fishing::RodInUse`); the choice is
kept in `chronicle.dat`'s `FISH` chunk.

**The fishing menus.** Fishing's screens are the UI framework's in every game, Enhanced fishing or
not, and look and sound as retail's (docs/UI_FRAMEWORK.md, section 3): the bait board, the fish
exchange and the fishing records. Its two questions are the towns' scripts' own, which every fishing
town asks in its window 1 (Norune's, Matataki's, Muska Lacka's and the sea's): the sign's "Fish /
Exchange FP / Fishing log / Quit fishing" (message 20) and Circle's "Continue fishing / Quit fishing"
(message 22, event 0x85, Circle while standing at the water). The dialog framework asks both
(`fishing::RegisterQuestions`, docs/DIALOG_FRAMEWORK.md, "A town script's question"), the same as
retail's and with the mouse. Where the place's fight reads rods (Enhanced), Change rod goes between
Circle's two: the item board with only the rods in the bag (`ED_MENU_CHANGE_ROD`,
`EVENT_ITEM_MODE_ROD`), opened from Circle's event as Square opens the bait's; the rod picked is the
rod in use, and fishing goes on as Continue fishing goes on.

| Stat | Retail's rod | Does (`fishing::EffectOf`) |
|---|---|---|
| Heft | 50 | the hooked fish's size, 0.8 to 1.2 times, as the hook is set (more points, a stronger fight) |
| Control | 33 | the stick held the fish's way tires it 3 x control / 100 times as much |
| Spring | 10 | the fish's pull is divided by 1 + 2 x (spring - 10) / 90 above 10 |
| Balance | 100 | heft, control and spring count 0.8 + 0.2 x balance / 100 of themselves |
| Lure | 0 | up to half fewer rolls of nothing biting, and half the bait lost to a fish that gets away or snaps the line |

## Enhanced fishing

Enhanced fishing is a datapack, `port/datapacks/enhanced_fishing/`, which the game ships off: it
installs in `assets/datapacks/` beside the executable, and naming it in `datapacks/datapacks.json`
beside the data folder turns it on (docs/ITEM_FRAMEWORK.md, "Datapacks"; `port/datapacks/README.md`
has the steps). Without it fishing is the port's base. It holds:

- `places.json`: every place with the `brisk` bite and the `tension` fight, its spawns and outcomes
  retail's.
- `fish.json`: each species' `fight.pull` with retail's rod, from 0.5 for most small fish to 2 for the
  Baron Garayan.
- Ruty's three rods, custom items with the `Rod` trait (retail's model and icon): the Sea Rod (Spring
  18, the Baron at 1.7: barely catchable), the Garayan Rod (33, at 1.3: catchable) and the Baron Rod
  (80, at 0.8, with Control 100 to tire it).
- `modpack.json`: its name, description and version.

`FishingDocuments.TheIncludedEnhancedFishingPackLoadsWhole` loads it whole.

**Ruty's rods** (`port/src/quest/rutys_rods.cpp`) is the pack's quest, registered only where its rods
are. Ruty keeps Queens' fish store and studies fish: she has seen the sea fish that come through the
port but none from inland. Once her store is open and Toan has caught a fish, she asks him to catch
every kind in Norune and Matataki (the Sea Rod), then a Mardan Garayan (the Garayan Rod), then the
Baron Garayan from her oldest fish book (the Baron Rod). The catches count only while she waits for
them (`fishing::CountSpecies`, `CountCatches` with the stages). She names the two towns from the
system messages (10 and 12), so each region and language has its own names (Nolun on PAL, Norune on
NTSC). Her lines are `quest.rutys_rods.*`, in the four languages as first drafts.

## Kept fish

A species row or document may name an `item` (an item ID, or null): what one of its fish becomes at a
`keep` place. Where the item's document declares `state` with a `size`, each kept fish is a copy of its
own with its size there (docs/ITEM_FRAMEWORK.md, "Copies"); an item with `sort_key` 7 has the `Fish`
trait, so a quest's `HasItemWith<item::Fish>` counts it. Retail's places don't keep: turning that on is
a pack's `places.json`.

## The session

`fishing::Session` (`session.cpp`) is retail's `EdMoveChara` fishing block, one tick at a time:
standing (Cross casts, Circle and Square open the town's menus, events 0x85 and 0x86), casting,
waiting with its idle motions, the nibble, the bite, reeling in, the fight, the result and putting the
fish away. Cross on the bite's own tick sets the hook (retail counted it as early from a nibble, or as
reeling in from the wait). It has no game headers: everything it does in the game goes through `fishing::Angler`,
which `angler.cpp` implements with the angler, the line, the fish, the camera, the sounds and the
messages, and tests implement with a record of what was asked (`FishingSession.*`). `EdMoveChara`
calls `fishing::StepSession`, and `chara_fishing` still holds the step for the game's code that reads
it.

## Events and quests

Every landed fish is published once, after its outcome, to listeners added with `fishing::OnCatch`:
its species, size, points, place, bait, time of day and whether it was kept. A fish that got away is
published to `fishing::OnLost` listeners with why (Cross on a nibble, or letting go of the hook) and
whether the bait went with it. A listener can't change either; what a catch gives is the outcome
stage's.

Quests count catches in their own numbers, which `quest::VarAtLeast` reads and the save keeps with
their progress (`port/src/fishing/quests.hpp`, through `quest::ChangeVar`), from when the helper is
called, usually as the quest is registered:

| Helper | Keeps in the quest's var |
|---|---|
| `CountCatches(quest, var, filter)` | how many catches match |
| `CountFishPoints(quest, var)` | the points from fishing |
| `KeepLargest(quest, var, filter)` | the size of the largest catch that matches |
| `CountSpecies(quest, var, species)` | how many of the listed species have been caught, each once |

A `CatchFilter` matches by species, place, bait and a least size, each left empty to match any. A kept
fish is an item too, so "bring me a fish" is the quest framework's `HasItemWith<item::Fish>`.
