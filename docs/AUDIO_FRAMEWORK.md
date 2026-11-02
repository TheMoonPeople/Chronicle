# Sound data

Dark Cloud's sound ships in Sony's formats: HD and BD banks, SQ sequences, inside `.snd` packs and,
for the title and the opening, inside the `.pak` packs that hold their pictures and models too.
`dcdata extract` converts it into ChronicleTwo's open formats (its `docs/PORT_AUDIO_FORMAT.md`)
beside the originals, so that the sound can be read and replaced with ordinary tools;
`dcdata publish-sound <data>` does it alone on an extracted data folder, and a rerun rewrites the
same files.

## How the game plays it

The game plays the published sound in place of the packs' own: as it loads a pack's load list, it
finds the pack's document beside it, builds each bank from its `bank.json` and WAV samples and each
sequence from its JSON through Kazarov's loaders, and takes the list's `.cfg` settings and the two
volume tables from their documents (`port/src/sound/published.cpp`). A pack without a document, a
list or member the document lacks, or a file that cannot be read plays the pack's own, as data the
first-run window extracted does, since it publishes nothing.

Every published file is looked for in the active datapacks first, the highest priority's first, then
in the data (docs/ITEM_FRAMEWORK.md, "Datapacks"), so a pack can replace a pack's document, a bank,
a sequence or a single sample by its path. The custom soundtrack (docs/SOUNDTRACK.md) still plays a
recording in place of a sequence by its name, whichever source the sequence came from.

The original packs are still extracted, and the game still reads each into memory before it walks
its lists; a follow-up removes them once the game no longer reads them.

## What a pack holds

Each pack (`sound/bgm`, `sound/set`, `sound/voice`, `sound/special`, and the opening's
`opdat/dungeon/o1bbgm.snd`) carries one or more load lists, its `.txt` members. A list names, one
per line, the banks (`.hd`, with the `.bd` of the same name) and sequences (`.sq`) the game loads,
in order. The sixth letter of a member's name picks its driver slot: `e01a_c.hd` loads into slot
`c`. A `.cfg` with the list's name sets the reverb (`REVERBE <mode>, <depth>`) and the effect table
(`TABLE <table>, <type>`). Members that are missing or empty, such as the dummy banks some packs
carry, are skipped, as the game skips them.

## What is published

| Disc | Published |
|---|---|
| a pack, `sound/set/snd0.snd` | `sound/set/snd0.sound.json` |
| a pack that also holds sound, `titledat/title.pak` | `titledat/title.sound.json`, its sequences under `titledat/title.sound/` |
| its banks (`.hd` and `.bd`) | `sound/banks/<member>-<hash>.sound/bank.json` and its `samples/N.wav` |
| its sequences (`.sq`) | `sound/set/snd0.sound/sequences/<member>.json` |
| `sound/tbl/sqtbl.txt` | `sound/tbl/sqtbl.json` |
| `sound/tbl/setbl.txt` | `sound/tbl/setbl.json` |

The same bank sits in many packs, so each distinct bank is published once, named for the first
member that holds it and a hash of its HD and BD, and every pack that loads it names that document.
Samples are 16-bit WAV.

A pack document (`kind: "sound_pack"`) lists its load lists in the pack's order:

```json
{
  "schema_version": 1,
  "kind": "sound_pack",
  "lists": [
    {
      "name": "snd0.txt",
      "config": {"reverb_mode": 4, "reverb_depth": 30, "se_table": 1, "se_table_type": 0},
      "load": [
        {"member": "e01a_c.hd", "slot": "c", "bank": "sound/banks/e01a_c-0123456789abcdef.sound/bank.json"},
        {"member": "e01a_e.sq", "slot": "e", "sequence": "sound/set/snd0.sound/sequences/e01a_e.json"}
      ]
    }
  ]
}
```

`config` is `null` for a list without a `.cfg`; a command the `.cfg` lacks reads as zero. Paths are
relative to the data folder.

`sqtbl.json` (`kind: "sequence_volumes"`) holds `rows` of `name` and `volume`, the default volume
of each sequence (at most 400, names cut to nine characters, as the game keeps them).
`setbl.json` (`kind: "effect_volumes"`) holds `rows` of `bank`, `program` and `volume` for each
sound effect (at most 3,000).

## Checking

`DataRead.APacksPublishedSoundPlaysInPlaceOfItsOwn` and `DataRead.APackWithoutPublishedSoundPlaysItsOwn`
load a synthetic pack as the game does, with and without its document.

With `DC_DATA` set to a data folder that has been published, `SoundPublishRealData` in
`darkcloud_tests` loads every published bank and sequence through Kazarov's JSON loaders and the
original through its raw reader, and checks that every program, note and velocity resolves to the
same samples, PCM and loops and every sequence to the same events. It compares each `.cfg` with the
game's own `LoadSoundInfo` and the effect table with `CSound::LoadSeInf`.
