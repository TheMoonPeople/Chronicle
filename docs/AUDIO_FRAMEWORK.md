# Sound data

Dark Cloud's sound ships in Sony's formats: HD and BD banks, SQ sequences, inside `.snd` packs.
`dcdata publish-sound <data>` converts it into ChronicleTwo's open formats (its
`docs/PORT_AUDIO_FORMAT.md`) beside the originals, so that the sound can be read and replaced with
ordinary tools. Run it on an extracted data folder; a rerun rewrites the same files.

The game still plays the original packs. The published files are the format the game will load
from once Kazarov's loaders take over, and the reference for anyone editing sound until then.

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

With `DC_DATA` set to a data folder that has been published, `SoundPublishRealData` in
`darkcloud_tests` loads every published bank and sequence through Kazarov's JSON loaders and the
original through its raw reader, and checks that every program, note and velocity resolves to the
same samples, PCM and loops and every sequence to the same events. It compares each `.cfg` with the
game's own `LoadSoundInfo` and the effect table with `CSound::LoadSeInf`.
