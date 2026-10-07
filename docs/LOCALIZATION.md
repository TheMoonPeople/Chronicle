# Localization

Game text can come from JSON files instead of the disc's `.mes` message files. A JSON file holds only the
messages it changes; every message it leaves out, and every file it does not mention, is read from the
disc as before. Nothing from the disc is shipped: a translator starts from an export of their own
extracted data (below), and the files the port distributes hold only text written for the port.

The files are laid out as Minecraft's language files are: one flat JSON file per language, named
`language_COUNTRY.json`, each an object of `"dotted.key": "text"`, with the same keys in every language.

## Where the files go

One file per language, named by the language the game is set to:

| Language | `LanguageCode` | File |
|---|---|---|
| Japanese | 0 | `ja_jp.json` |
| English (US) | 1 | `en_us.json` |
| English (UK) | 2 | `en_gb.json` |
| Francais | 3 | `fr_fr.json` |
| Deutsch | 4 | `de_de.json` |
| Italiano | 5 | `it_it.json` |
| Espanol | 6 | `es_es.json` |

The port looks in two folders, in this order, and a later file's messages replace an earlier one's:

1. `lang/` beside the executable,
2. `lang/` in the save folder (`--save`, or `DC_SAVE`).

The language is looked up whenever the game asks for a file, so a change of language takes effect on the
next file the game loads; the JSON itself is read once per language, so restart the game after editing a
file.

## Format

```json
{
    "dun.script.d01.d01.0": "Bienvenue.",
    "dun.script.d01.d01.12": "Il y a {-1021}trois{-1024} coffres ici.\nPrenez-les !",
    "commenu.dungeon.dunmenu5.pak.allmenu.11": "\"Objet\"\nUtiliser un objet ou\nregler les objets."
}
```

- A key is **the message file's key, a dot, and the message id**. The id is the number the game gives the
  message, an s16 (`-32768` to `32767`); an id the disc's file does not have is added.
- The message file's key is the same in every language, so `fr_fr.json` and `de_de.json` have the same keys
  for the same messages and can be set side by side. It is the path the game loads with the language taken
  out, the `.mes` taken off and the slashes made dots:

  | The game loads | Key |
  |---|---|
  | `dun/script/d01/d01_3.mes` (and `d01_2.mes`, `d01_4.mes`...) | `dun.script.d01.d01` |
  | `meswin/system14_2.mes` | `meswin.system14` |
  | `gedit/e01/e01talk_3.mes` | `gedit.e01.e01talk` |
  | `commenu/a_fre/dungeon/dunmenu5.pak/allmenu.mes` (`a_eng/`, `a_ger/`...) | `commenu.dungeon.dunmenu5.pak.allmenu` |
  | `noda_w/dun00_eng.mes` (no language in its name) | `noda_w.dun00_eng` |

  A message file inside a pack (`.pak`, `.pac`) is the pack's path and the entry's name, as in the last
  rows. The "language" is the last number of a file's name (`_0` to `_6`) or a menu folder named `a_xxx/`.
- A key that does not end in a message id is one of **the port's own strings**, the ones it draws itself
  (the Options screen). They are in the same files, under `options.`; see "The port's own text" below.
- A text is UTF-8. `\n` is a line break. `{N}` writes the code N as it is, which is how colours, pauses,
  icons and the game's name and number fields appear (in the disc's English menu text a coloured word is
  wrapped in `{-1021}` and `{-1024}`); `{{` is a `{`. The export writes every message in this form, so
  copy those codes across unchanged.

## The port's own text

The Options screen is the port's, not the disc's, so its words are not in the disc's message files. They are
in the language files too, under keys the code derives from each setting, so a new row gets its keys without
any other change:

| Key | Is |
|---|---|
| `options.page.game` (`display`, `audio`, `controls`) | a page's name |
| `options.<setting>.label` | the row's name; `<setting>` is the name in `config.json`, `game.map`, `video.aspect`... |
| `options.<setting>.help` | the help window's text for a setting the port describes itself |
| `options.<setting>.choice.<n>` | the n-th choice of the setting, counting from 0 (`On`, `Off`...) |
| `options.<setting>.value.<English>` | a value the row works out itself, by its English (`Desktop`, `Unlimited`) |
| `options.shortcuts`, `options.help.*`, `options.display.*` | the hints at the bottom and the warning texts |

A text may hold `%1`, `%2`... where the program puts a number or a name, and the pad's button glyphs as
their `{N}` codes (`{-759}` square, `{-761}` triangle, `{-762}` circle, `{-760}` cross, `{-766}` L1,
`{-765}` R1); keep both as the English has them. Help windows hold four lines of about 22 characters, and a
label about 20; a longer one runs past its window.

The settings the game describes itself (the help for Message Speed, Clock...) are the disc's messages and
come from the disc's language files as before.

`port/lang/` ships these strings in French, German, Italian and Spanish; the build copies it to `lang/`
beside the executable, where the game reads it, so they need no export. English is in the code, and is what
shows where a language has no text for a key. The export lists every one of them with the text the language
would show now, so a translator starts from the shipped text. A translation written in `<save>/lang/` takes
the place of the shipped one, key by key. The translations are first drafts: a native speaker should look
at them.

## Choosing the language

The language screen at the start of the game sets the language, as retail does. To skip it, and to change
the language at any time, set it in `config.json` or on the Options screen (Game page, Language row):

```json
{"game": {"language": "francais"}}
```

`ask` (the default) shows the language screen; `english`, `francais`, `deutsch`, `italiano` and `espanol`
start in that language without asking, going straight to the attract movie where the language screen would
have led. Changing the row on the Options screen switches the game to that language once the Options screen
closes: the town or title it is in is loaded again, so every message, menu and image is the new language.
A dungeon floor is not restarted for it; the language takes hold when the next area loads. `ask` leaves the
current language alone.

## Exporting the disc's text

The port does this for you: the first start with data in place (right after the first-run extraction, or
any `dcdata extract`) writes the export to `<save>/lang-export/`, unless `en_gb.json` is already there.
Delete that folder to have it made again. The same by hand:

```
darkcloud --data <extracted data> --export-text <folder>
```

writes one flat file per language into `<folder>`, from every message file under the data folder (loose
`.mes` files and the `.mes` entries of `.pak` and `.pac` packs; 855 files on the PAL disc):

| File | Holds |
|---|---|
| `en_gb.json`, `fr_fr.json`, `de_de.json`, `it_it.json`, `es_es.json`, `en_us.json`, `ja_jp.json` | the messages of that language's files, all keyed as above |
| `hashes/<language>.json` | the same keys, each with the hash of the message's text, for patches |

A file that names no language and is not Japanese (`noda_w/dun00_eng.mes`) is in every language's file.
The menu packs left in `commenu/` that are Japanese go to `ja_jp.json`. Where a file's path names its
language it wins over one that does not. Two files of one language that come to the same key are told on
stderr (there are none on the PAL disc).

The export checks as it goes that every message encodes back to the disc's own codes, and prints how many
files do not (none, on the PAL disc). Messages that have no end code in the disc's file (one unused id in
`allmenu.mes`) are left out.

The export is the disc's own text. It is the user's data and must not be committed or shared:
`/lang-export/` is in `.gitignore`, and anything you put in a `lang/` folder next to a build you
distribute should be your own text, not an export.

## Translating

1. Export, then copy the language's file to `<save>/lang/` (`fr_fr.json`...).
2. Delete every message you are not changing; what stays is the part you have written. A file with all of
   the disc's messages in it works, but it hides what you changed.
3. Edit the texts. Keep every `{N}` code and line break the disc's text has unless you know what it does.
4. Start the game in that language and look at each screen. Check stderr for `localize:` lines (overlong
   texts, characters the font lacks).

## Comparing the languages

```
python scripts/port/compare_text.py <save>/lang-export [<out>]
```

writes one CSV per language (fr_fr, de_de, it_it, es_es) with `en_gb.json` as the reference, one row per
message that differs in a way worth looking at: `missing` (English has it, this language does not), `extra`,
`empty`, `same` (the English text, often untranslated, and every message of a file that names no
language), and `long` (more than twice or under half English's length, where a window may not fit it), and
a `summary.txt` of the counts. The CSVs hold the disc's own text: keep them out of git.

## Patches

A patch changes a message the disc has without carrying the disc's text. Patch files are `patches/*.json`
in either `lang/` folder, read in name order for every language. They are applied to the disc's text each
time the file loads, so updating a patch needs no re-export:

```json
{"patches": [
  {"file": "dun/script/d01/d01_3.mes", "id": 12, "replace": [["teh", "the"], ["recieve", "receive"]]},
  {"file": "commenu.emenu.pak.allmenu", "id": 11, "original": "0a1b2c3d4e5f6a7b",
   "text": "\"Item\"\nUse or equip items."}
]}
```

- `file` is the path the game loads or the message file's key.
- `replace` is a list of `[find, with]` pairs; every occurrence of `find` becomes `with`. A pair whose `find`
  is not in the message is skipped and reported; if none match, the patch is.
- `text` replaces the message whole. Give exactly one of `replace` and `text`.
- `original` (optional) is the hash of the disc's whole message (a 64-bit FNV-1a of the UTF-8 text, as 16 hex
  digits; the export lists them in `hashes/`). The patch applies only when it matches, so a different disc,
  or a message already changed, is left alone.
- A message that the language's own file also has is not patched: the explicit text wins.
- Patches in one file chain in order, so a later `find` sees an earlier patch's result.
- The shipped patches hold only the words that change, so the corrected text exists only on the user's
  machine, built from their own disc.

## File encoding

Every file the port writes is UTF-8 without a byte-order mark, and letters are written as themselves, never
as `\uXXXX` escapes, so `Crème` and `Jäger-Ohrring` read as they are in any editor. The port reads UTF-8
files (a BOM is not expected).

**Japanese is the exception, for now.** The Japanese messages are not text but numbers: each is the index
(0 to 784) of a glyph in the game's own Japanese font, which is not Unicode, so `ja_jp.json` has them as
`{237}{9}{2}...`. Writing them as real Japanese needs a table from those indices to characters, which the
port does not have yet. Until then they stay as the codes, which still round-trip exactly, and the other
languages are unaffected.

## What the game font can draw

The PAL font has the 88 letters, digits and marks of its `gaiji.img` and the accented letters
`ÀÁÂÄÈÉÊËÌÍÎÏÑÒÓÔÖÙÚÛÜàáâäèéêëìíîïñòóôöùúûü`, `œ Œ ß Ç ç ¡ ¿`, and the curly quotes and dashes become the
plain ones. Anything else (`;`, `_`, `~`, a Cyrillic or Greek letter) is drawn as `?`, and the port tells you
once per file how many characters did that. The full table is in [PC.md](PC.md#game-text).

## Crisp text from a TrueType font

The game draws its text from 14x20 bitmaps, which blur when the picture is scaled up. Put a TrueType file
called `font.ttf` in a `lang/` folder (the save folder's, then the executable's) and the port draws every
character the font has from it instead, rasterized at the screen's own resolution, in the cell the game laid
the bitmap character out in (size, colour, outline and the cursor row's flashing are the game's own). A
character the font lacks, and the icons and pad buttons, still draw as bitmaps; a font that has blank
placeholders for the accented letters, the cedilla, `¡`, `¿`, `œ` and `Œ` gets them built from its own
letters and marks. The port logs `font: message text from <file>` when it loads one.

- It works for every message the game draws through its message windows, whatever its source: the disc's
  text, the JSON's, item and monster names.
- It does not touch text that is a picture: the large menu words (ITEM, WEAPON...), the title screen's and
  the like are textures, and stay as they are.
- A word keeps the cells the game gave it: its letters are spaced by the font inside the word and the word
  is centred on its cells, so rows, indents and the icons between words stay where the game put them.
- The font is not shipped with the port: each user adds a file they have the right to use.

## Limits, and what happens when they are passed

A text that does not fit is left as the disc has it, and the port says so once on stderr (`localize: ...`):

- a window shows at most 720 characters (`{N}` fields count as what they expand to);
- a message file's offsets are s16, so a file is at most about 32,000 characters in all;
- a message file the game reads from a **loose file** (`dun/script/...`, `gedit/...`, `meswin/...`) is read
  into a buffer sized for the disc's file, rounded up to whole 2048-byte sectors. A translated file that
  comes to more than that keeps the disc's text for the whole file. Translate longer by shortening
  elsewhere in the file, or put the text in a message file the game reads out of a pack, which has no such
  limit.

Bad JSON (not JSON, a key that is not a file key and an id, a value that is not a string) is reported once
and the file is ignored; the game runs on disc text.

## Memory

The JSON for the current language (the language's file, from both folders) is held parsed in memory: about
100 bytes of bookkeeping per message plus the text. A translation that changes some messages is a few
hundred KB; a full export of the disc's English is about 3.5 MB. A message file taken from a pack is rebuilt
once and kept (about 80 KB each, about 2 MB for every pack). Loose message files are rebuilt each time the
game loads them and not kept. Reading a file costs a transient 3 to 4 times its size. With `font.ttf`, the
glyph atlas is one 2048 x 2048 texture (16 MB of video memory).

Set `DC_LOCALIZE_TRACE=1` to log every message file the game loads and whether the JSON has an entry for it.

## Not done

- Japanese as characters (above).
- Other text the port may draw itself later: a screen that adds rows only has to follow the key scheme above.
- Reloading a JSON file without restarting the game.

## In the code

`port/src/localize.{hpp,cpp}`: `LocalizeText` and `LocalizeFormat` give the port's own strings,
`LocalizeMessageKey` makes a file's key, `LocalizeParse` reads a language's
file, `LocalizeMessages` lays texts over a disc message file (through `GameTextFile`), `LocalizeFile` and
`LocalizePack` are the hooks, and `LocalizeExport` is `--export-text`. The Options screen asks
`LocalizeText` for each of its words (`port/src/menu_option.cpp`; `OptionStrings()` lists them for the export),
and lays its text out again when it is opened in another language. The hooks sit in
`port/src/dataread.cpp`: `LoadFile2` and `LoadFileBG` pass each file they read to `LocalizeLoaded` and
`LocalizeFile`, and the port's `GetPackFile` gives a message entry through `LocalizePack`. Tests:
`port/src/tests/localize_test.cpp`.
