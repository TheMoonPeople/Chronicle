# The Seda mod

Seda, who appears only in the story's cutscenes, in Toan's place in the dungeon. This first part is a
test: `--seda` puts him in from the start, so he can be tried before anything unlocks him.

## Trying it

`dcdata extract` builds his pack into the extracted data (and `dcdata seda <data-directory>` builds it
into data extracted before). On Windows, from the checkout:

```bat
build.bat windows-x64-release
run.bat windows-x64-release --jump dungeon:0 --seda --fast-load
```

`run.bat` extracts the data from `rom\pal\Dark Cloud (PAL).iso` again whenever `dcdata.exe` has
changed, so the pack is there. `--jump dungeon:0` starts at the Divine Beast Cave's floor select;
`--seda` (or `DC_SEDA=1`) puts Seda in Toan's place. Without `--seda` Toan is as he was. The game
prints `seda: no dun/mainchara/c01seda.chr in the data` when the pack is missing, and then keeps Toan.

## When it crashes

The port prints a crash report to the console: the exception, the address it read or wrote, and the
call stack as `module+offset (address)`. In the Debug build (`windows-x64`) the addresses in brackets
name source lines with the llvm-mingw toolchain the build downloaded:

```powershell
.\build.bat windows-x64
.\run.bat windows-x64 --jump dungeon:0 --seda --fast-load
& ".\.kazarov\windows\deps\llvm\llvm-mingw-20260922-ucrt-x86_64\bin\llvm-symbolizer.exe" --obj=port\build\windows-x64\darkcloud.exe 0x140123456 0x140234567
```

with the bracketed numbers of the report in place of the two examples.

## What it is

Seda's character is `gedit/s16/chara/e502p68b.chr`, the cutscene in which he fights the assassin; its
configuration opens with `//シーダ王`, King Seda. The game moves a model's bones by their index in its
scene (`AnimeDataInit`, `ps2/src/gameutil.cpp`), and Seda's skeleton is not Toan's (3 of Toan's 77 bones
are at the same index), so Toan's motions cannot move him. He keeps his own:

- `MakeSedaPack` (`tools/dcdata/dcdata.hpp`) writes `normalized/dun/mainchara/c01seda.chr`: Seda's pack
  with his configuration rewritten as `base.cfg`, the name the dungeon loads a player character by. Its
  41 `KEY`s are Toan's, in Toan's order, each a stretch of Seda's motion (`kSedaKeys`): his walk for
  Toan's walk, his run with the sword ready for the run, cutting down and running the assassin through
  for the attacks, holding the sword for the guard, and so on. His model, textures and footstep events
  are his own.
- In his scene the sword (`seiken__m`, in his left hand) becomes `weapon`, where the game hangs the
  equipped weapon, and is no longer drawn; his right hand (`cube40_3_2`) becomes `item`. So he fights
  with the weapon Toan has equipped, its damage and element.
- The port (`port/src/dataread.cpp`) gives his pack for `dun/mainchara/c01d.chr` while `SedaActive()`.
  That is every load of Toan's dungeon character: the dungeon's start, changing character and the menu.

## What to look for, and what is not done

- Whether he loads at all, and stands, walks and runs.
- Whether the equipped weapon sits in his hand. Its place is his sword's, but a weapon model may lie along
  another axis than his sword did.
- Whether attacks land. The game times them from each key's start and end, so they should; a few of
  Toan's timings use fixed frames of Toan's motion (getting up after heavy damage) and will be off.
- How each action looks. The table in `kSedaKeys` is a first guess and is easily changed.
- Not done yet: unlocking him after the Sun and Moon Temple and choosing him in the game, the PS2 build,
  the treasure box and atla presentations (which load their own Toan, `c01d_ex00.chr`) and the towns,
  where Toan is still Toan.
