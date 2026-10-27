# UI Framework (`dc::ui` Namespace)

**Status: in use, with a proposal.** The framework exists and draws a number of the port's screens
(section 3). This document describes it as it is and proposes where it goes next: what it is for,
the rules every screen built on it follows, a cleanup of what has piled up in it, and the order in
which retail's screens move onto it. Sections marked **Proposal** are not built yet.

---

## 1. Goals

- **Native look.** A screen on the framework looks like Dark Cloud: retail's own pictures where
  they exist, and where they don't, lettering and surfaces made to match them (section 7).
- **Parity.** A retail screen moved onto the framework keeps retail's layout, controls, sounds,
  timings and results, in every language, and is compared with retail before the old one is retired
  (section 4).
- **What retail can't do.** Mouse and pointer input, layouts that hold at any window size and
  aspect, text in any language without repainting pictures, and content retail never had (custom
  items, quests, new options).
- **Extendable.** A new screen or a new row costs a declaration, not a copy of retail's drawing
  code. The framework holds no screen's content (section 5), so adding a screen never changes it.
  The screens a remaster would add are built on it (section 11).
- **Off the GS, looking the same.** The framework draws with the port's own renderer, not by
  imitating the GS: retail's look is worked out from what the GS did (section 7) and then drawn the
  modern way, so a screen looks identical without carrying the PS2's drawing model along.

---

## 2. Should every retail screen move onto it?

The direction is that the framework eventually draws all of the game's menus, replacing retail's
drawing and input code for them. That is a sound direction for a modernisation, with three
conditions.

**Screens move when they need to, not in a sweep.** Each retail screen is thousands of lines that
work (`battlemenu.cpp`, which holds the item menu, has 61 item-range comparisons and 98 per-item
`case` labels). Rewriting
one costs a lot and risks small losses of parity, so a screen moves when something needs it: mouse
support, a layout that breaks at some aspect, text retail painted, or content retail can't show
(the item framework's custom items are the largest, its section 11). A screen nothing needs keeps
retail's code, which is byte-matched and correct by construction.

**The framework owns behaviour; retail's art stays where it exists.** The port has already learned
this once: #210 retired the port's redrawn weapon status numbers and powder panel and went back to
the game's own sprites, because they are the most faithful copy of themselves. So a screen on the
framework draws retail's pictures (`TextureButton`, the menus' frames and icons) wherever retail has
one, and generates lettering or surfaces only for what retail has no picture of: a new word, another
language, a custom item.

**Some of the game's UI is not a menu.** The message windows and speech bubbles are the game's text
renderer, which Chronicle Sharp already draws (docs/LOCALIZATION.md); cutscene text and anything
drawn in the 3D world stay with the game. The framework is for menus, boards and HUD panels.

---

## 3. Today

### Screens on the framework

| Screen | Unit | Uses |
|---|---|---|
| Battle menu ring, captions, Save/Load and Quit modals | `port/src/battlemenu/main_ring.cpp` | `Label`, `HelpBox`, `ConfirmModal`, `Cursor` |
| Weapon menu, its buttons and help | `port/src/battlemenu/weapon_menu.cpp` | `BevelButton`, `TrackerWindow`, `HelpBox`, `Label`, `WrapText` |
| World map and leave-dungeon prompt | `port/src/battlemenu/move_menu.cpp` | `ConfirmModal`, `Cursor` |
| Journal: manuals and chest tracker | `port/src/game/menu_manual.cpp`, `port/src/ui/journal_chest.cpp` | `TrackerWindow`, `ProgressBar`, `ItemIcon`, `DrawCounter`, `Brackets` |
| Options | `port/src/options/` | `TabPlate`, `TextureButton`, `ScrollBar`, `Cursor`, `Label` |
| Villager conversation hub | `port/src/dialog/framework.cpp` | `TrackerWindow`, `Divider`, `ScrollBar`, `Brackets`, `Cursor` |
| Title menu and its Exit confirmation | `port/src/title/title_menu.cpp` | `Label` (`FontWeight::Title`), `ConfirmModal` |

Still retail's: the save and load boards (`DrawMenuSave`, port-drawn but copied from retail), the
item menu, shops, storage, the item picker, the Georama menus, the fishing exchange, the floor
select and the dungeon HUD.

### What the code holds

```text
port/src/ui/framework.hpp       umbrella header
port/src/ui/framework/
    types.hpp      palette constants, metrics, Alignment, FontWeight, Anchor, Rect, ZLayer, LayerQueue
    text.hpp       TextStyle, Label, WrapText, DrawTextBox, DrawCounter, DrawStatus*
    surfaces.hpp   WindowPreset, TrackerWindow, PaperWindow, HelpBox, Window, TabPlate, Divider, SubWindow
    buttons.hpp    ButtonPreset, ButtonStyle (Thick, Thin), BevelButton, DrawPaintedPlate, TextureButton
    widgets.hpp    ItemIcon, Cursor, Brackets, ScrollBar, ProgressBar, MenuHeader
    dialogs.hpp    DialogBox, ConfirmModal, ListView
    *.cpp          their implementations, and internal.hpp (ui::detail)
```

About a third of the public API has no caller: `DialogBox`, `ListView`, `LayerQueue` and `ZLayer`,
`SubWindow`, `MenuHeader`, `PaperWindow` and `Window`, `DrawTextBox`, the `DrawStatus*` family, five
of `Label`'s draw variants, and the `NATIVE_COLOR_*` and `*_MENU` aliases. Section 14 says what
happens to each.

---

## 4. Moving a retail screen onto the framework

Each move is its own pull request and follows the same steps.

1. **Record retail.** Captures of every state of the screen on a real GPU (not lavapipe, section 7),
   in each language, with the textures it draws (`DC_TEXTURE_DUMP`) and the draw calls
   (`DC_SPRITE_TRACE`). They stay on the developer's machine: they are disc content.
2. **List what it does.** Controls, sounds (which effect, on which press), timings and animations,
   what each action changes, what the mouse should do. This list is the parity check.
3. **Build it on the framework,** drawing retail's pictures where they exist (section 2).
4. **Compare.** Side by side with the captures, state by state and language by language, and every
   item of the list from step 2.
5. **Switch and retire.** The retail function is replaced in its game unit (`port/src/game`), or its
   callers are for a `static` one, only once the comparison holds.

---

## 5. Layers and what belongs here

- **The framework draws with the game's services**: its textures and sprites, the menus' frame
  (`DrawMenuWaku`), colour gradients, menu sounds, pad input and the text renderer. It includes the
  game headers for those.
- **It holds no screen's content or state**: what a screen says, when, and in which order belongs to
  the screen. The battle menu's ring text is `port/src/battlemenu/strings.cpp`; each screen's
  translation keys live with that screen.
- **Other frameworks use its public headers only.** Everything is in one codebase, so this isn't
  about a boundary between projects: `port/src/ui/framework/internal.hpp` is the framework's working
  detail (game headers, texture blocks, helpers) that it changes freely, and a unit reaching into it
  breaks without warning when it does. **Proposal:** `dialog/framework.cpp` reads
  `ui::detail::kMessageTextureBlock` there today; the framework gains one public call that readies the
  message texture, and the dialog framework uses that.
- `ui::DrawPaintedPlate` recognises retail's painted plates by texture name inside the framework,
  which this rule would put in a module above it. It is left where it is by decision (it is
  Tororoi's, works, and moving it changes nothing anyone sees).

---

## 6. Text

### Fonts

`port/src/text/captionfont.{hpp,cpp}` and `port/src/text/caption_text.{hpp,cpp}` keep four TrueType font
slots, switched with `capfont::UseFontSlot`:

| Slot | Font | `ui::FontWeight` | Draws |
|---|---|---|---|
| 0 | `ChronicleSharp-Bold.ttf` (`lang/font.ttf`) | `Normal`, `Bold` | the text the port draws from a font, menu captions, the name alphabet |
| 1 | `ChronicleSharp-Regular.ttf` (`lang/font_thin.ttf`) | `Thin` | the weapon menu's buttons, area cards, the floor select's boss names |
| 2 | `SourceSansProSemibold.ttf` (embedded, or `lang/`) | `Status` | the weapon status abbreviations and numbers the port draws |
| 3 | `ChronicleTitle-Bold.ttf` (embedded) | `Title` | the title menu's rows, in Chronicle Title Bold (Regular, to come, is PUSH START BUTTON's): retail's painted capitals fitted into the font, the letters the painting lacks built by its rules, spaced by the usual method for capitals (`tools/font/README.md`) |

`tools/font/README.md` has each font's origin and licence.

### Two renderers, chosen explicitly (**Proposal**)

`Label` draws with one of two renderers: the game's own text renderer (retail's glyph sheet, or
Chronicle Sharp's sheet in its place) or the TrueType renderer (`caption::DrawLine`). Today the
choice is implicit: a label at scale 1, `FontWeight::Normal`, no custom colour and no outline gets
the game's renderer, and changing any of those switches it to TrueType, so the same label changes
look when its size changes.

**The goal is one renderer:** Tororoi's lettering (`caption::DrawLine` with Chronicle Sharp and the
other slots), which is the port's standard for text quality. The game's renderer is only still needed
for labels whose text carries the game's control codes (`{cyan}`, `{page}`, button glyphs), which the
TrueType path doesn't read yet. So:

1. **Now:** `TextStyle` gains a `renderer` field (`GameText` or `Font`) and the silent switch goes.
   Every existing call site is set to the renderer it draws with today, found by the same rule, so
   nothing anyone sees changes.
2. **Then:** the TrueType path learns the control codes the framework's labels use, the call sites
   move to `Font`, and `GameText` is left to retail's own windows (which Chronicle Sharp already
   draws), outside the framework.

**Two things the lettering does that a screen must know.** It sets letters ink to ink, each letter's
drawn width plus a gap, which is how it fits retail's painted slots. A letter's drawn picture carries a soft edge a
window pixel wide, whose share of a logical pixel shrinks as the window grows, so this spacing tightens on larger
windows; `ink_spacing` sets letters by their outlines instead, the same at any size; a line drawn as typeset text
(the title rows) is placed by its first letter's side bearing, so a flat E stands a little right of a
round C or O as a typesetter sets them. It kerns when a style asks (`kern`, the strength of the font's
own kerning, from its kerning table or GPOS pair kerning), measuring and drawing alike, so a kerned
label still aligns. **Proposal:** the unified renderer gains full typeset spacing (the font's advances
and bearings) as an option for typeset looks. And its alpha darkens the letters by
default, so its passes (shadow, rim, face) don't show through each other; `see_through` makes the
alpha transparency instead, as retail's sprites fade (section 7).

### Effects

`TextStyle` and `DrawScaledText` (`port/src/ui/framework/text.cpp`) support:

- **Recessed** (`recessed`): the label stamped into a button's surface, a catch-light below right and
  a cast shadow above left, coloured per button preset. Used by `ButtonStyle::Thick`.
- **Raised bevel** (`raised_bevel`): an extruded body under the face with a dark outer stroke, the
  stock weapon status digits' look.
- **Top-lit gradient**: every `Status` label is lit from the top (a cached 2x16 fill field from a
  pale highlight into the label's colour).
- **Painted** (`painted`): the weapon buttons' lettering, cream with no outline over a soft brown
  halo. With `ButtonStyle::Thin`, a word the game paints is drawn letter for letter as painted
  (`caption::DrawButtonWord`) and any other word gets the same lettering fitted to the plate: one
  look, whatever the word or language.
- **Kerning** (`kern`): the font's kerning, at the given strength (0, the default, for none).
- **Ink spacing** (`ink_spacing`): letters set by their outlines rather than their pictures, measuring and drawing
  alike, so the spacing is the same at any window size. Off by default; the title rows use it, their spacing being
  exact kerning.
- **Halo** (`halo`, `halo_strength`): the soft black stroke retail's painted lettering sits in, darkest
  against the letters and fading as it reaches out; four dark outlines, each wider and fainter, under
  the letters. Strength 1 is all but black next to the letters.
- **Weight and spacing:** `Bold` thickens the main font (weight 1.1) by stacking offset copies, which
  also makes it more opaque than its alpha; `Title` is drawn as its font is, its weight made in the font.
  `letter` adds to the gap between letters.

---

## 7. Matching retail's 2D look

When the framework draws something retail drew as a picture (a painted word, a plate, a menu row),
its colour and opacity are worked out from what the GS did with that picture, not judged by eye.
Two rules of the GS decide most of it:

- **0x80 is 1.0, and texels can go to 0xFF.** Vertex colour and alpha, and texel alpha, are all in
  GS units, where 0x80 is full. The GS multiplies texel by vertex and divides by 0x80. Many of
  retail's painted pictures carry texel alpha 0xFF, twice full, so the alpha the game passes is
  doubled: a sprite drawn at "32" over 0xFF texels is half opaque, not a quarter.
- **Past full, the port shows the picture's own colour.** On the GS a result above 0x80 can overshoot
  (`Cd + (Cs - Cd) * As / 0x80` is `2 * Cs - Cd` at twice full), but in the port retail's doubled
  pictures drawn at full show their texels' own colour: the title's selected row measures exactly its
  painting's cream on a real GPU. Match that, not the arithmetic.

**Finding the numbers.** `DC_TEXTURE_DUMP=<dir>` writes every texture as the renderer holds it, alpha
in GS units (255 in the PNG is 0xFF, not 0x80). Read the picture's texel colour and alpha there,
take the alpha the game draws it at (its `set2DSprite` or `DrawMenu2DSprite` call), and compute the
result. Then check it against a capture of retail on a real GPU (the retail drawing is in git
history, or on a PS2 or in PCSX2). Don't calibrate colour from a headless lavapipe screenshot alone:
one of retail's title rows came out there at about half the brightness a real GPU shows, for a
reason not yet known.

**The port's own textures don't.** The glyph atlas and the framework's generated surfaces are ordinary
textures, full coverage opaque. One trap when judging them: anything drawn as several overlapping
copies (the renderer's `Bold` thickening stacks five) is more opaque than its alpha says.

**Worked example: the title menu.** Retail's rows (`start3`) are cream letters, (226, 253, 210), at
texel alpha 0xFF, drawn at 128 when selected and 32 when not. The selected row shows the cream itself;
the others are the cream at half opacity, see-through, with no shadow. The font's rows do the same, in Chronicle Title, whose capitals are the painting's letters: every row in a
3.75-pixel halo at strength 0.55, the selected one the cream at full strength, the rest cream and
see-through, each row placed by its first letter's side bearing, capitals 16.7 tall as the English rows' are. The unselected rows are drawn at three quarters of the half
the arithmetic gives: the painting's strokes are softer than the font's, so less of each letter is at
full strength, and on a real GPU half reads too bright. That last figure is judged, not computed.

---

## 8. Looks (**Proposal**)

Today every screen sets its own `TextStyle` and `ButtonStyle` numbers, so nothing says in one place
"this is the menu caption" or "this is the title row", and nothing records which retail picture each
was matched to. The title rows were first guessed for that reason.

A **look** is a named, documented style: what it draws, the renderer and font, its colours, size and
effects, and the retail picture it matches (texture, texel rectangle, the alpha the game draws it at,
the measured values and how they were found). The looks live in one unit
(`port/src/ui/framework/looks.cpp`), and a screen asks for one by name:

```cpp
ui::Label(LocalizeText("title.menu.start", "START"), ui::Look::TitleRowSelected).Draw(x, y, alpha);
```

The first looks are the ones the port already draws: menu caption, title row (selected and not),
weapon button word, modal text, help text, the dialog hub's choices, the journal's progress label.
`TextStyle` stays underneath for one-offs, and a one-off used twice becomes a look.

Looks are what makes "native" checkable: a look either cites the retail picture it reproduces, or
says it is the port's own (a widget retail never had) and which retail look it follows.

**Themes.** Looks are looked up through a registry, so a theme is a set of looks under the same
names, and a screen never names a theme. The retail theme is the default and the reference every
other theme is measured from; it is the only one this proposal builds. A theme that leaves a look out
falls back to the retail theme's, so a theme can change a few looks without restating the rest, and a
theme from a newer version of the port still loads.

---

## 9. Widgets and layout

- **Coordinates** are the game's 640x480 logical space; the 2D mapping (docs/PC.md, "Window aspect")
  places it on the window. Retail laid many screens out for NTSC's 448 rows and lowered them by 16
  on PAL; a screen moved from retail keeps retail's positions and that lowering where it applies.
- **`Rect`** carries layout: anchoring, insets, splits and rows.
- **Surfaces:** `TrackerWindow` draws the procedural `BlueStone` and `Paper` windows (periodic noise
  with bevelled rounded-box edges, cached by size and preset; PNG overrides at
  `textures/plates/ui_stone.png` and `ui_paper.png`) and the dark translucent box; `HelpBox`,
  `TabPlate`, `Divider` and the stone banner (`DrawStoneBanner`, from `wepstatus`).
- **Buttons:** `BevelButton` with the procedural plates (`Terracotta`, `AmberGold`, `StoneGreen`,
  `SlateBlue`), `ButtonStyle::Thick` (recessed lettering) and `Thin` (painted lettering, section 6),
  pressed or not; `TextureButton` for a button that is a retail picture. The plates' size: 96 by 24
  logical pixels visible (96 by 32 with the shadow margin), 26 between rows.
- **Widgets:** `Cursor` (retail's hand, with its swing), `Brackets` (the pulsing selection
  brackets), `ScrollBar`, `ProgressBar`, `ItemIcon`, `DrawCounter`.
- **Dialogs:** `ConfirmModal` (a box and the game's yes/no plates).

### Hit-testing

**Every widget that draws something clickable says where it is, from the layout it draws with.**
`ConfirmModal::Hit` and `ConfirmModal::ButtonRect` are the first: the box grows with its text, so its
buttons move, and the mouse and the hand's first position read them rather than copies of the
coordinates. **Proposal:** the rest follow as their screens move (`ListView::HitTest` exists), and
`port/src/ui/menu_mouse.cpp` (1,200 lines, most of them screens' coordinates kept a second time) shrinks
as each screen's widgets answer for themselves.

---

## 10. Input and sounds

Screens read the pad (`GamePad`) and the pointer (`MenuPointer`: a hover moves the game's own cursor,
a left click presses Cross, a right click Circle). Retail plays different effects in different
places: the menus' `ComMenuSePlay`, the title's own `TiPlayVolSE` set. **Proposal:** a screen hands
its widgets its sound set, so a `ListView` on the title plays the title's cursor sound and in the
pause menu the menu's, without either knowing where it is.

---

## 11. New screens (**Proposal**)

Once retail's menus are on the framework, it carries the screens a remaster adds. The ones planned or
likely so far: a bestiary, an achievement tracker, a fish tracker (with the fishing framework), custom
tutorials, and more manual and journal entries; quests' screens and the item framework's custom items
already lean on it.

Most of them are one of three shapes, so the framework gains them as composites built from its
widgets, each with retail's look:

- **Catalogue:** a list of entries (`ListView`), a detail panel for the selected one, and a picture or
  model of it; locked entries shown as retail shows unknown things. Bestiary, fish tracker, item
  catalogue.
- **Tracker:** groups of goals with progress (`ProgressBar`, `DrawCounter`), as the journal's chest
  tracker already is. Achievements, collections.
- **Pages:** paged text and pictures turned as the manuals are. Tutorials, journal entries.

The data comes from the framework that owns it (the bestiary's from an enemy framework, the fish
tracker's from fishing) through callbacks registered with the screen, as dialog providers register
with `dialog::`, so the UI framework never knows what a fish or an enemy is.

## 12. PS2 texture state

- Before drawing paletted PS2 menu sprites (`StayTex`, `DrawMenuWaku`, `WepStatus`, the character
  faces), call `setbilinear(0)` and `MenuTextureReload(BtlMenuReadBlock)` (or the PAL extension block
  for `wepstatus`), so bilinear filtering doesn't bleed the colour-key texels.
- Call `MenuTextureReload(0x1A)` before drawing `GameText` or a `Label` on the game's renderer.
- `DrawMenu2DSprite` (`port/src/game/menu_draw.cpp`) clamps a 1:1 sprite whose texel rectangle is 1 or 2
  rows taller than its screen rectangle, as retail's often are, so it doesn't sample the next sprite
  in the atlas at scaled resolutions; the completed Georama plate is trimmed to 119 rows for its
  gauge cap.

---

## 13. Notes on the screens built on it

- **Battle menu captions** keep retail's sizing (`kEm = 25`, outline 1.6, least shrink 0.62). The
  town's `SAVE/LOAD` caption is wider than the painted `SAVE` label it replaces, so its box takes
  `QUIT GAME`'s width (icon 12) and `DrawBattleMainOverlay` widens the selection frame to match.
- **In-game Save/Load:** the town ring's Save entry opens a `ConfirmModal` (Save, Load). Loading
  opens `InitMenuSave(SAVE_MENU_MODE_LOAD, ...)`; once the save is read, the menu closes
  (`StepInGameLoadMonitor`) and the main loop takes the request (`TakeBattleMenuLoadRequest`) and
  reloads the saved area as a load from the title would.
- **Quit** asks, then goes back to the title through `TakeBattleMenuTitleRequest` (docs/PC.md, "The
  title menu, Exit and Quit").

---

## 14. Cleanup (**Proposal**)

Measured against the goals: what a parity replacement of retail's screens needs stays and is
finished; what nothing needs goes.

| Item | Decision | Why |
|---|---|---|
| `ListView` | keep, finish with its first user | every list retail has (items, shops, storage, save boards) is one; untested so far |
| `DialogBox` | keep, finish; `ConfirmModal` becomes a two-option `DialogBox` | one dialog implementation, not two |
| `MenuHeader` | keep until the pages it heads move | a page header with the ring's icons; checked against retail when first used |
| `DrawStatus*` | remove | #210 went back to retail's sprites for the status numbers, which is the rule (section 2) |
| `LayerQueue`, `ZLayer`, `DeferDraw` | remove | no screen draws out of order; retail draws in order |
| `SubWindow`, `PaperWindow`, `Window` | remove | wrappers of `TrackerWindow` and `Rect` no screen uses |
| `DrawTextBox`, `Label::DrawInRect`, `DrawCenteredInRect`, `DrawAnchored`, `DrawWithStroke`, `DrawWrapped` | remove | unused; `Rect` anchoring and `WrapText` cover them |
| `NATIVE_COLOR_*`, `BLUE_STONE_MENU`, `PAPER_MENU`, `DARK_TRANSLUCENT_MENU` | remove | aliases; the one use of `BLUE_STONE_MENU` takes `WindowPreset::BlueStone` |
| `metrics` | keep what is used, the rest goes | constants nothing reads |
| `ui::detail` reached from `dialog/framework.cpp` | public replacement (section 5) | the private header stays private |
| Implicit renderer choice | explicit (section 6) | |
| Screens' own `TextStyle` numbers | looks (section 8) | |

Nothing removed changes what any screen draws.

---

## 15. Phases (**Proposal**)

Each is its own pull request.

1. **Cleanup:** the removals of section 14, the explicit renderer, and the private header kept
   private. Nothing visible changes.
2. **Looks:** the registry and the looks the port already draws; screens switched to them.
3. **Save and load boards** on the framework, with `ListView` finished on them (roadmap: the Main
   Menu's load screen; the item framework's orphan "!" mark draws on these boards).
4. **Item screens** with the item framework's phase 3 (its section 11): the item menu, then storage,
   the shops and the item picker, each following section 4.
5. **The rest,** screen by screen, as each needs it.
6. **New screens** (section 11), after retail's menus, the item framework and the fishing framework:
   the composites first, then the screens on them (the bestiary is planned).

---

## 16. Tests (**Proposal**)

The framework has no tests today: laying text out needs the fonts. Layout is pure arithmetic once text
widths are known, so the layout code takes a text measurer, and tests give it fixed widths: where
`ConfirmModal`'s buttons land for one, two and four lines, `ListView`'s scrolling and wrapping,
`Rect`'s anchoring, and that every widget's hit-test agrees with where it draws. Appearance is
checked by the captures of section 4, which can't be committed.

---

## 17. Open questions

- Which retail screens are first for the composites of section 11 to be proven on (the journal's
  chest tracker is already a tracker)?
