# Dialog Framework (`Dialog::` Namespace)

The dialog framework lets port code add choices to a villager's conversation. A villager that
something adds a choice to opens on a menu instead of on their own words:

```text
  Talk            <- the villager's own conversation, exactly as retail has it
  (added choices) <- whatever the providers added
  Never mind      <- ends the conversation
```

Villagers nothing adds a choice to are left alone: their conversation never touches the framework.

---

## 1. Layers

Dependencies flow downward only.

1. **`UI::` presentation** (`port/include/ui_framework/`). Window plates, labels, cursor, brackets,
   scrollbar. No conversation state.
2. **`Dialog::` runtime** (`port/include/dialog_framework.hpp`, `port/src/dialog_framework.cpp`). The
   page stack, word wrap and layout, token substitution, option providers, input and the default
   renderer. Its public header includes no PS2 headers, and it knows nothing of the systems that
   register providers.
3. **Host glue** (`port/src/dialog_npc.cpp`, `port/src/editloop3.cpp`). Offers each villager
   conversation to the framework, reads the pad for it, and hands "Talk" back to retail.
4. **Providers** (quests, special shops, mods). Register an `OptionProviderFn` and look at the
   `Dialog::NpcContext` to decide what to add.

---

## 2. How a conversation reaches it

The port overrides retail's `EdTalkMode` group in `port/src/editloop3.cpp` (`EdTalkModeInit`,
`EdTalkMode`, `EdNowTalkChara`, `EdNowTalkCharaInfoID`: every function that reads the conversation's
file statics). The copies are retail's, with one addition in `EdTalkMode`:

1. On the conversation's first tick, after the villager's talk script has filled `EdEventInfo`,
   `DialogNpcOpen` builds an `NpcContext` (`map_no` = `MapNo`, `npc_id` = the villager's character
   info id, `mes_id` = the message retail is about to show, `mes_file_key` = `LocalizeMessageKey`
   of the town's villager message file, as the language files name it) and calls
   `Dialog::TryInterceptOpen`.
2. If no provider adds a choice, the framework declines and the conversation runs as retail.
3. Otherwise the framework has the conversation. Each tick `DialogNpcStep` reads the pad (Up and
   Down move, Cross takes, or shows typing text whole; Circle backs out a page or leaves) and the
   camera and villager turn as in retail.
4. The page is drawn by the conversation's own window, `EditMes1`: the paper speech bubble, the hand,
   and the rows the hand is not on dimmed. It is placed as retail places them (`DialogNpcSpeaking`):
   a page with the villager's words over the villager, with the tail and their talking motion, as
   retail's messages are; a page of choices alone over the player, as retail's choice prompts are. The
   page's text is one message (a `GameTextFile`) of its lines with the choices in view below them;
   `ClsMesSetChoiceRows` (`clsmes_port.hpp`) marks the window: the text rows above the choices keep
   their colour, and a coloured choice the hand is not on fades (half opacity) where retail's dimming,
   which doubles the colour, would brighten it. A page with no text is choices only, as retail's
   prompts are. The villager's own message file
   goes back into `EditMes1` when the session ends. `Dialog::DrawSession`'s own renderer is not
   used here; it remains for sessions opened elsewhere (`OpenCustomSession`).
5. "Talk" ends the session with `SessionEnd::PlayOriginal` (the context has `host_plays_original`).
   The next tick, retail's conversation starts from the beginning in its own window, with its own
   choice prompts, shops and events.
6. "Never mind", or Circle on the first page, ends the conversation as retail's Circle does.

The mouse works the window as the menus' does (`DialogNpcMouseUpdate`, from `MenuMouseUpdate` in
`menu_mouse.cpp`): the hand follows the pointer and a choice it hovers is selected, the left button
takes it (or turns a page of text), the right button backs out and the wheel moves the hand. Each is
the pad's press, so `DialogNpcStep` does the rest.

A list of choices longer than the window (six) scrolls with the cursor. The window's own page arrow
(`syst04`, the blinking arrow that says more text follows) blinks below the last choice where more
are below, and drawn turned over above the first where more are above (`ClsMesSetChoiceScroll`).

The talk window grows to its widest line and does not wrap, so `DialogNpcOpen` sets the layout's
`max_inner_width` to 380 (about as wide as retail's villager lines run) and `max_visible_choices`
to 6, to stay within the window's ten rows. Lines are measured in the menu font, which is close to
but not exactly the talk window's.

---

## 3. Writing a provider

```cpp
#include "dialog_framework.hpp"

Dialog::RegisterOptionProvider([](const Dialog::NpcContext &ctx, std::vector<Dialog::ChoiceItem> &out) {
    if (ctx.map_no != 0 || ctx.npc_id != 3) {
        return;
    }
    Dialog::ChoiceItem item;
    item.id = "quest.lost_cat";
    item.label_key = "quest.lost_cat.ask";         // LocalizeText key
    item.fallback_label = "About the lost cat...";  // the English
    item.on_select = [](Dialog::ISessionController &session) {
        session.PushChoicePage("quest.lost_cat.offer", {/* Accept, Decline... */});
    };
    out.push_back(std::move(item));
});
```

An `on_select` can push pages (`PushMessageKey`, `PushRawMessage`, `PushChoicePage`,
`PushRawChoicePage`, and `PushClosingMessage`, a page that ends the conversation once read), go back
(`PopPage`), hand over to retail (`PlayOriginalMessage`), ask the
providers for the NPC's menu again after changing what they offer (`RefreshHub`), or end the
conversation (`Close`). A page with no choices goes back to the page under it when its last slice is
confirmed.

Quests (`docs/QUEST_FRAMEWORK.md`) are a provider: their choices come through `Quest::CollectOptions`.

---

## 4. Text

Text is the game's markup, as `GameTextDecode` writes it and `docs/LOCALIZATION.md` lists it:
`{page}` splits slices, `{cyan}...{/color}` colours, `{cross}` is a pad icon. Layout measures each
line in the message font through `UI::Label`, so icons and names take the room they draw in.

- `{name1}`...`{name6}`, `{insert1}`...`{insert10}` and `{value}`/`{value1}`...`{value8}` are left
  for the game's renderer to fill in, as it does for retail text (the player can rename Toan). A
  `TokenContext` entry that is not empty replaces its tag instead.
- `%1`...`%9` are the format arguments a page was pushed with.
- Labels lay their text out with `KeepGameRandom` on, so an open dialog does not move the game's
  random numbers.

### Localization

The framework's strings follow the port's convention for its own text: English in the code
(`kEnglish` in `dialog_framework.cpp`, `fallback_label` for a provider's), translations in
`port/lang/<language>.json` under `dialog.`:

| Key | English |
|---|---|
| `dialog.common.talk` | Talk |
| `dialog.common.leave` | Never mind |
| `dialog.common.back`, `.yes`, `.no`, `.accept`, `.decline` | for providers' pages |
| `dialog.common.next_page` | {cross} Next |
| `dialog.hub.default_prompt` | Is there something you need? (not shown by default: an NPC's menu is its choices alone, unless `NpcContext::hub_greeting_key` names a greeting such as this one) |
| `dialog.hub.anything_else` | Was there anything else? |

`Dialog::Strings()` lists these for `OptionStrings()`, so the text export carries them and the
language-file test checks each language has them.

`Dialog::SetExternalLocalizeLookup` lets a mod answer keys first.

---

## 5. Trying it

`DC_DIALOG_TEST=1` registers a test provider: every villager offers "Who are you?", which shows the
map, villager, message and message file a provider would match on.

`port/src/tests/dialog_framework_test.cpp` covers page splitting, tokens, wrapping, layout, and the
session: the hub, "Talk"'s hand-back, leaving, pushed pages, disabled choices and scrolling. It sets
a text measurer, so it needs no font.

---

## 6. Not done yet

- Wrapping measures in the menu font, close to but not exactly the talk window's.
- The default renderer (for `OpenCustomSession`, not villagers) lays out a speech-bubble tail
  (`ComputedLayout::has_tail`) but does not draw one.
