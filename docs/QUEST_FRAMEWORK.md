# Quest Framework (`dc::quest` Namespace)

Quests the port adds to the game: each a state machine of named stages, moved on by rules and by
what the player picks in conversations, kept beside each save. Quests add choices to villagers'
conversations through the dialog framework (`docs/DIALOG_FRAMEWORK.md`).

---

## 1. Layers

Dependencies flow downward only.

1. **`quest::` runtime** (`port/src/quest/framework.hpp`, `port/src/quest/framework.cpp`). The
   registry, stages, rules, conditions, conversation choices and saving. No PS2 headers: a quest
   reaches the game only through `quest::IGame`.
2. **Host** (`port/src/quest/host.cpp`). `IGame` over the save data, the dialog option provider, the
   save files, and the shop screen.
3. **Quests.** Documents (`gameplay/quests/`, section 3), read from the data and every active datapack,
   so a pack adds a quest without the game being rebuilt; and in code (`port/src/quest/`, listed in
   `RegisterAllQuests`, `quests.cpp`) only a quest that takes over something retail does itself, which a
   document cannot (the Mayor's supplies, section 7).

---

## 2. A quest

```cpp
quest::Definition Errand() {
    return quest::Builder("errand")
        .Stages({"start", "middle", "end"})   // starts at the first
        .Advance("start", quest::HasItem("vanilla:bread"), "middle")
        .Option({.map_no = 0, .npc_id = 7}, quest::AtStage("middle"), {"quest.errand.ask", "The errand"}, Errand)
        .Build();
}
```

Every quest, in code or a document, comes down to this. `port/src/tests/quest_framework_test.cpp` has
more, among them a quest that ends in a shop (`FetchQuest`).

- **Stages** are names; a quest is at exactly one. `Build` rejects a quest without stages, a stage
  named twice, a rule between names that are not stages, and a choice that does nothing.
- **Rules** (`Advance`) move a quest from one stage to another once a condition holds. `Evaluate`
  applies them until none does; it runs whenever an NPC's menu is put together, and again after
  each quest choice.
- **Conditions** are plain functions of the quest's `Context`: `AtStage`, `AtStages`, `HasItem` (by
  number, or by item ID: `HasItem("vanilla:bread", 2)`, docs/ITEM_FRAMEWORK.md), `HasItemWith<T>`
  (items with an item trait between them: `HasItemWith<item::Fish>(3)`), `HasMoney`, `FloorReached`, `DungeonStarted`, `PartyMember`, `VarAtLeast`, the game's flags
  (`GameFlagSet`, `GameNumberAtLeast`, `MapFlagSet`), its georama (`PartPlaced`, `PartComplete`,
  `EventDone`, `RequestMet`, `VillagerHome`), and `All`, `Any`, `Not` to combine them. A lambda works
  too.
- **Choices** (`Option`) are added to the menu of the NPCs an `NpcMatch` names (map, villager, message
  file; `quest::kAny` matches any) while their condition holds. A choice runs as a `Scene`.
- **Numbers**: `Var` and `SetVar` keep a quest's own counters and flags, saved with its stage.
  `quest::ChangeVar` changes one from outside the quest, for what it counts as it happens: fishing's
  helpers count catches that way (docs/FISHING_FRAMEWORK.md, "Events and quests"). `IGame::SystemText`
  reads a system message, such as a town's name, as the player's data and language have it.
- **Retail takeovers** (`TakesOver(npc, quest::Retail::...)`): something retail does by itself, which
  the quest does its own way instead. While the quest is registered, the host holds retail's version
  back for the NPCs it names. `Retail::MayorSupplies` is the only one so far (section 7).

### Scenes

What a choice does, in the conversation it was picked in:

| Call | Does |
|---|---|
| `Say(text, args)` | A page of text (`{page}` splits it, `%1`... are `args`), then back to the page under it. |
| `Ask(prompt, choices)` | A question; `Choice(label, fn)` makes its choices, each running as a `Scene` of the quest. |
| `Back()` | Back a page, or out of the conversation from the NPC's menu. |
| `Close()` | Ends the conversation. |
| `OpenShop(spec)` | Ends the conversation and opens a shop screen (section 5). |
| `SayThenClose(text)` | A page of text that ends the conversation once read: for what plays after it, such as an item received. |
| `SetStage`, `SetVar`, `Game()` | The quest's progress and the game, as in a rule. |

After a choice runs, the NPC's menu is put together again, so a choice that finishes a step shows
the next one's choices straight away.

### Text

Every string is a `quest::Text`: a key and the English. The key looks the text up in the language
files (LocalizeText, docs/LOCALIZATION.md), so a quest is translated as the port's other text is,
under `quest.<quest id>.`. `Text` holds views: in code it is made of string literals, and a document's
quest keeps the document's text for as long as it lives.

`QuestStrings()` (`quest/quests.cpp`) gathers the quests' text for `OptionStrings()`: the Mayor's
(`MayorSuppliesTexts`) and every document's. The text export lists them for translators. The test
`LocalizeOptions.ShippedTranslationsHaveExactlyTheCodesKeys` checks that every language file in
`port/lang/` has exactly the code's keys and those of the game's own quest documents
(`port/gameplay/quests/`), with the same `{codes}` and `%1`... as the English, and
`LocalizeOptions.ShippedPacksTranslateTheirQuests` checks each shipped pack's `lang/` against its own
quests the same way. Xiao and the other party members are `{name2}`...`{name6}` in the text rather than
their names, as retail's text has them.

---

## 3. Quest documents

`gameplay/quests/<id>.json` (`port/src/quest/documents.cpp`), kind `quest`, one quest a file, its name
the quest's ID with `:` as `.` (`lures:lost_ring` is `lures.lost_ring.json`). They are read at start-up,
after the items and fishing, from every active datapack and the data, each file name once, the
highest-priority pack's first (as custom items are, docs/ITEM_FRAMEWORK.md), and registered after the
quests in code, in ID order: the order their choices appear in an NPC's menu. The game's own are tracked
in `port/gameplay/quests/` and sealed into its data. A document that cannot be used is reported
(`config: <path>: <where>: <why>`) and left out, as is one naming an item no item document has; a save
keeps such a quest's progress (section 6). A pack's quest uses an ID of its own namespace; the game's
own keep the bare IDs their saves already have.

```json
{
  "schema_version": 2, "kind": "quest", "id": "lures:lost_ring",
  "stages": ["hidden", "offered", "asked", "done"],
  "rules": [{ "from": "hidden", "when": { "floor_reached": { "dungeon": 0, "floor": 3 } }, "to": "offered" }],
  "options": [{
    "npc": { "town": "norune", "villager": 8 }, "when": { "at_stage": "offered" }, "label": "ask",
    "steps": [{ "say": "offer" }, { "stage": "asked" }]
  }],
  "text": { "ask": "Lost ring", "offer": "I lost my ring in the cave...{page}Will you find it?" }
}
```

| Field | Holds |
|---|---|
| `stages` | The stages, the first the quest starts at. |
| `rules` | `from`, `when` (a condition) and `to`, as `Advance`. |
| `options` | Choices: `npc` (`town`, a name or 0 to 4, and `villager`, as the dialog framework numbers them; `message_file`; each left out matches any), `when` (left out: always), `label` (a text), `color` (`white`, `yellow`, `cyan`, `green`, `gold`, `grey`, `highlight`) and `steps`. |
| `counters` | What the quest counts as it happens (below). |
| `text` | Each text's name and its English; its key is `quest.<id>.<name>`. |

Towns are named `norune`, `matataki`, `queens`, `muska_lacka`, `yellow_drops`, and party members
`toan`, `xiao`, `goro`, `ruby`, `ungaga`, `osmond`; a number works for either. Items are IDs.

**Conditions** are objects with one member, the library's under their own names:

| Condition | Holds when |
|---|---|
| `{"at_stage": "asked"}`, `{"at_stages": ["asked", "excuse"]}` | The quest is at the stage, or one of them. |
| `{"has_item": "vanilla:bread"}`, `{"has_item": {"item": "vanilla:bread", "count": 2}}` | The player carries it. |
| `{"has_item_with": "fish"}`, `{"has_item_with": {"trait": "fish", "count": 3}}` | Items with a trait between them: `fish`, `bait`, `rod`, `key_item`, `quest_item`, `dungeon_key`. |
| `{"has_money": 100}` | |
| `{"floor_reached": {"dungeon": 0, "floor": 3}}`, `{"dungeon_started": 0}` | |
| `{"game_flag": 200}`, `{"game_number_at_least": {"slot": 3, "value": 100}}`, `{"map_flag": {"map": 0, "flag": 12}}` | |
| `{"part_placed": {"town": "norune", "part": 7}}`, `part_complete`, `event_done` | |
| `{"request_met": {"town": "matataki", "request": 2}}`, `{"villager_home": {"town": "queens", "villager": 1}}` | |
| `{"party_member": "xiao"}` | |
| `{"var_at_least": {"var": "paid", "value": 1}}` | |
| `{"caught_a_fish": true}` | The player has caught a fish in this save (fishing's `HasCaughtAFish`). |
| `{"all": [...]}`, `{"any": [...]}`, `{"not": {...}}` | |

**Steps** run in order when the choice is picked. Pages are listed as they are read: the scene's pages
are a stack, the last pushed read first, so a choice's pages wait until its steps have run and are then
pushed last first. A step that can fail runs its `then` steps when it does what it says, its `else`
steps when not, and the steps after it either way.

| Step | Does |
|---|---|
| `{"say": "name", "args": [...]}` | A page of text. |
| `{"say_then_close": "name", "args": [...]}` | A page that ends the conversation once read. |
| `{"ask": "name", "args": [...], "choices": [{"label": "name", "color": "grey", "steps": [...]}]}` | A question; each choice runs its steps as a choice of the quest. |
| `"back"`, `"close"` | Back a page, or end the conversation; the pages said before it are pushed first, and the steps after it are not run. `say_then_close` ends after a page. |
| `{"stage": "asked"}` | Moves the quest on. |
| `{"set_var": "paid", "value": 1}`, `{"add_var": "times", "value": 1}` | |
| `{"give_item": "id", "then": [...], "else": [...]}` | `else` where there is no room. |
| `{"take_item": "id", "then": [...], "else": [...]}`, `{"take_money": 100, ...}` | `else` where the player has none, or too little. |
| `{"give_money": 100}` | |
| `{"show_item_received": "id", "then": [...], "else": [...]}` | Section 9; `else` where the town cannot show it, and says so in a page. |
| `{"if": {condition}, "then": [...], "else": [...]}` | |
| `{"open_shop": {"look_of_shop": 0, "keeper": "speaker", "goods": [{"item": "id", "price": 30}], "lines": {"greeting": "name"}}}` | Section 5: ends the conversation and opens the shop. `keeper` is `speaker` (the default) or `retail`; `lines` are `greeting`, `browsing`, `selling`, `not_enough_money`, `no_room`, `confirm`, `thanks`, `goodbye`. |

A text's `%1`, `%2`... are its `args`, each filled in as its step is reached: `{"var": "times"}`;
`{"var_left": "inland", "of": 7}` (7 less the number); `{"system_text": 10, "or": "Norune Village"}`
(a system message, such as a town's name, as the player's data and language have it); `{"item_name":
"id"}`; `{"item_count": "id"}`.

**Counters** follow fishing's catches into the quest's numbers (docs/FISHING_FRAMEWORK.md, "Events and
quests"), from when the quest is registered. `while_at` (stages) limits one to those stages.

| Counter | Counts |
|---|---|
| `{"count": "fish_caught", "var": "mardan", "species": "vanilla:fish_5", "place": ..., "bait": ..., "min_size": ..., "while_at": [...]}` | 1 for each catch the filter (each field left out matches any) matches. |
| `{"count": "fish_species", "var": "inland", "species": [...], "while_at": [...]}` | Each species once. |
| `{"count": "largest_fish", "var": "record", ...filter}` | Keeps the largest catch's size. |
| `{"count": "fish_points", "var": "points"}` | Each catch's points. |

Anything a document cannot say (a retail takeover, a step the table lacks) is a quest in code, or a
new step here. A document's own top-level fields this version does not read are reported and left
alone; within rules, choices and steps, an unknown name refuses the document, so a typo is told
rather than skipped.

### Translating a pack's quest

The document holds the English. A pack's `lang/<language>.json` (`fr_fr`, `de_de`, `it_it`, `es_es`)
holds its translations under the same keys, `quest.<id>.<name>`, and wins over the game's own files
(docs/LOCALIZATION.md); the Enhanced fishing pack's `lang/` translates Ruty's rods.

---

## 4. The game

`IGame` is all a quest sees of the game; `quest/host.cpp` implements it over `SaveData`:

| Call | In the game |
|---|---|
| `ItemCount(item)` | Copies in the pack (for pack items, the ones `TakeItem` takes); other kinds as the game counts them. |
| `GiveItem(item)` | One item, as a pickup is (`CDngStatusData::GetItem`): a dungeon item into the pack, a weapon to the character who wields it. False when there is no room (a full pack; ten weapons). |
| `TakeItem(item)` | One out of the pack (`LostItem`); false when there is none. |
| `Money`, `SpendMoney`, `EarnMoney` | The party's Gilda, at most 65535. |
| `HasItemAnywhere(item)` | In the pack or stored, as retail's `PlayerAllItemCheck` looks. |
| `DeepestFloor(dungeon)` | `floor_reached`: -1 for a dungeon never entered; 0 is the Divine Beast Cave. |
| `DungeonCount(dungeon)` | `CSaveData::QuestDungeon(dungeon, 0)`, as the town scripts read it: 0 until the player has been in. |
| `PartySize()` | How many have joined: Toan 1, Xiao 2, Goro 3... `quest::PartyMember(chara)` holds once character `chara` (0 Toan, 1 Xiao...) has. |
| `GameFlag`, `SetGameFlag` | The game's flags, 0 to 8191, as the town scripts read and set them (`_SGET_GAME_FLAG`, `_SSET_GAME_FLAG`). Setting one changes what retail does, so a quest sets only flags it knows the meaning of. |
| `GameNumber`, `SetGameNumber` | The game's 32 numbers (`_SGET_GAME_INT_FLAG`, `_SSET_GAME_INT_FLAG`). |
| `MapFlag`, `SetMapFlag` | A map's flags as the scripts read them (`_SGET_MAP_FLAG`): set where the save holds 0. The map the player is on goes through `EdGetMapFlag`, which shares Treant's chest flags between its two maps. |
| `PartObtained`, `PartPlaced`, `PartComplete`, `PartEventDone` | A georama part, by town (0 Norune, 1 Matataki, 2 Queens, 3 Muska Lacka, 4 Yellow Drops) and part (0 to 23): obtained, copies standing, every element in place (`CheckComplete`), its completion event run (`GetCompEvent`; Norune's part 7 is Gaffer's Buggy). |
| `RequestMet(town, request)` | A villager's georama request is met (`_SCHECK_REQUEST`). |
| `VillagerHome(town, villager)` | A villager, by the number the dialog framework gives them, is back: their house stands and they are in it (`_CHECK_VILLAGER`). Known for the town the player is in only; false for any other. |
| `TalkLine(message)` | A line of the town's villager message file, the NPCs' own words in the player's language. `Scene::SayTalkLine` says one, with a fallback where the file has none. |
| `OpenShop(spec)` | A shop screen once the conversation has ended. |
| `ShowItemReceived(item)` | Once the conversation has ended, the scene the towns' events show an item received with: the camera, the glow, Toan holding it up, the jingle and the message naming it. Shows only (`GiveItem` gives). False where the town's script has no such scene, and the quest says so itself. |

A test runs quests against a fake `IGame` (`port/src/tests/quest_framework_test.cpp`).

---

## 5. Shops

A quest's shop is a **custom shop** (`port/src/quest/custom_shop.cpp`): retail's item shop screen with goods,
prices, a shopkeeper and lines of its own. Retail's shops keep theirs:

| What | How the custom shop gives it |
|---|---|
| Goods | `GetItemShopList` (the port's, in `port/src/game/shop.cpp`, with retail's lists) gives the custom shop's while it is open. |
| Shopkeeper | `ItemShopGetPacFileName` and `ItemShopGetImgFileName` (the port's, in `shop.cpp`) give the keeper's character file. A villager's town model (`gedit/e01/chara/p13a.chr`) loads as a shopkeeper's does: the same pack, model and `<model>01.img`, and the shop plays only motions 0 and 3, which every villager has. |
| Lines | A message file of the shop's own lines is laid over the shopkeeper's window (`CommonMenuMes3`, `ClsMesSetMessageOverlay`): item shop *n*'s lines are messages 4000 + *n* &times; 100 and an offset for the moment. |
| Prices | The screen reads them from retail's `PriceList` through a function only `shop.cpp` can call, so the custom shop's buying prices are laid over that table for the visit and put back when the screen closes (`CommonShopLoop`). |

A quest's `ShopSpec` gives:

- `look_of_shop`: the retail item shop the screen is laid out as: where the shopkeeper stands, and the
  lines the quest does not give.
- `goods`: items and buying prices.
- `keeper`: `Shopkeeper::Speaker` (the default) puts the villager the conversation was with behind the
  counter; `Shopkeeper::Retail` keeps `look_of_shop`'s. A model the data does not have, or one too large
  for the screen's buffer (1 MB; retail's largest town model is 641 KB), falls back to
  `look_of_shop`'s shopkeeper. The screen needs one: without a shopkeeper it never shows its lines.
- `lines`: `ShopLine::Greeting`, `Browsing`, `Selling`, `NotEnoughMoney`, `NoRoom`, `Confirm`,
  `Thanks`, `Goodbye`, as `quest::Text`.

The shopkeeper stands where `look_of_shop`'s does, scaled as theirs is: a villager much taller or
shorter than that shopkeeper may need a `look_of_shop` whose shopkeeper is more their size.

---

## 6. Saving

Each save's folder gets a `quests.json` beside it, written with the save (and the autosave) and read
when the save is loaded:

```json
{"version": 1, "quests": {"mayor_supplies": {"stage": "open", "vars": {}}}}
```

- A save without the file (made before quests) starts every quest at its first stage; so does a new
  game.
- A quest the file has that is not registered (a mod taken out) is kept and written back as it was.
- A stage the quest no longer has starts that quest again.
- Deleting a save deletes its `quests.json`.

---

## 7. The Mayor's supplies

`port/src/quest/mayor_supplies.cpp`. Retail's talk script (`gedit/e01/event.stb`, event 0x100) hands
the player one each of Tasty Water, Bread, Repair Powder, Escape Powder and Antidote Drink they have
none of, in the pack or stored (`_SITEM_CHECK_ALL`, `_SITEM_GET`), whenever they talk to the Mayor,
once they have been into the Divine Beast Cave (`QuestDungeon(0, 0)`) and until Gaffer's Buggy is
built (`_SGET_CMP_EVENT(7)`). He then says line 287 of the town's villager file (his lines start at
20 + 13 &times; 20); once the buggy is built he says line 289 about it instead.

Here it is "Do you have any supplies?", the first choice below "Talk" once the cave has been entered:
the same five items on the same terms, with line 287 or 289, and lines of its own where nothing is
missing or the pack is full (retail would put the items in the pack's overflow; this gives only what
fits).

The script's own handing out is held back (`Retail::MayorSupplies`): `EdTalkModeInit` calls
`QuestHostTalkStarting` before the script runs, and while the talk with the Mayor starts,
`PlayerAllItemCheck` (the port's, in `port/src/game/menu_save.cpp`) answers that the player has those five
items, so the script gives nothing and says his usual lines. The hold ends on the conversation's first
tick in `EdTalkMode` (`QuestHostTalkOpened`).

---

## 8. Carl's Slingshot

`port/gameplay/quests/carls_slingshot.json`, the example quest document. Carl (villager 7 of Norune) and Gina
(villager 9) are two of the village's children, and come back with two different georama pieces.

| Stage | Carl | Gina |
|---|---|---|
| `hidden` | | |
| `offered` (Xiao has joined) | "You look upset.": Gina borrowed his slingshot and won't give it back. &rarr; `asked` | |
| `asked` | "Your slingshot": go and ask her | "Carl's slingshot": a big bird took it! &rarr; `excuse` |
| `excuse` | A bird? She's fibbing; ask her again. &rarr; `doubted` | The bird took it! Honest! |
| `doubted` | Ask her again, no birds this time | She broke it, and made him a new one out of old bones. &rarr; `confessed` |
| `confessed` | It's junk; keep it, give it to Xiao: the **Bone Slingshot**. &rarr; `done` | Did you give it to him? Is he mad? |
| `done` | | |

The Bone Slingshot is Xiao's weapon, so the quest opens once she has joined; if she has no room for
another weapon, Carl says so and the quest waits. Carl's last words end the conversation
(`SayThenClose`), and Toan holds the slingshot up as the town's own events show an item received
(`show_item_received`; where the town cannot, he says so in a page). Each child's choice is one option
whose steps look at the stage (`if`), so a stage that is not theirs to move on gives a reminder instead.

---

## 9. An item received

`ShowItemReceived` runs the town script's own item-get routine, the one its events call for every
gift (the Manual, the World Map): `ItemGet(5, item, fade, camera)` loads the glow (`/chara/iget.chr`)
into character slot 5, plays Toan's motion, puts the item's model in his hands, plays sound 296 and
shows the system message naming the item. The scripts carry no function names, and the routine sits
at a different offset in each script, so the port finds it by what it does (`runscript.cpp`): of the
functions the programs reach, the one of four arguments that pushes `"/chara/iget.chr"`.

When the conversation ends, `EdTalkMode` returns the port's event number `kEventItemGet`
(`runscript.hpp`) as a talk's own event, and `CRunScript::run` starts the routine with the item,
no fade and its own camera. Norune, Matataki and Queens have the routine, as do some interiors;
Muska Lacka and Yellow Drops do not, and there `ShowItemReceived` is false.

---

## 10. Georama, live and saved

The town the player is in keeps its georama in `EditPartsInfo`, written back to the save when they
leave it (`EditSave`); every other town's is in the save (`SV_GEORAMA_DATA`: each part's state and each
request's). The georama queries read `EditPartsInfo` for the town the player is in
(`NowEditMap`), and for any other load a copy of that town's from its save, its parts' catalogue
coming from the game's own tables (`CEditPartsInfo::Load`), so a quest can ask about Matataki from
Norune. A villager's being home depends on where their house stands in the town's layout, which only
the town the player is in has loaded.
