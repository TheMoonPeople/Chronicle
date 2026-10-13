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

The disc has two Sedas. Seda himself (`c07a`, his configuration opens with `//シーダ`) and King Seda
of the story's last flashback (`e502p68b`, `//シーダ王`). `dcdata seda <data-directory> [seda|king]`
builds the pack from either; `extract` builds Seda's.

| Variant | Character | Motions | Sword |
| --- | --- | --- | --- |
| `seda` (default) | `gedit/s99/chara/c07a.chr`, the dungeon's cutscenes | 24: stand, walk, hold the sword, sweep and leap, point the sword, magic pose, throw, laugh, suffer | `obj10` in his right hand stays drawn; the equipped weapon hangs from his hand (`r_hand4`) |
| `king` | `gedit/s16/chara/e502p68b.chr`, the assassin's cutscene | 52: walk, run with the sword, cut down, run through, push off | his own (`seiken__m`, left hand) is hidden and the equipped weapon takes its place |

The game moves a model's bones by their index in its scene (`AnimeDataInit`, `ps2/src/gameutil.cpp`),
and neither skeleton is Toan's, so Toan's motions cannot move them. Each keeps its own:

- `MakeSedaPack` (`tools/dcdata/dcdata.hpp`) writes `normalized/dun/mainchara/c01seda.chr`: the
  character's pack with its configuration rewritten as `base.cfg`, the name the dungeon loads a player
  character by, and no NUL after its text (the game's reader takes a NUL for a letter). Its 41 `KEY`s
  are Toan's, in Toan's order, each a stretch of the character's own motion (`kSedaC07Keys`,
  `kKingSedaKeys`). The model, textures and footstep events are the character's.
- In the scene one object becomes `weapon`, where the game hangs the equipped weapon, and one `item`,
  which holds items. So he fights with the weapon Toan has equipped, its damage and element.
- The port (`port/src/dataread.cpp`) gives that pack for `dun/mainchara/c01d.chr` while `SedaActive()`.
  That is every load of Toan's dungeon character: the dungeon's start, changing character and the menu.

Toan's attack code (`ToanKey_Play`, `ps2/src/charakey.cpp`) times its hits, its charged attack's
leap, trails and sounds by frames of Toan's own motion. With the mod those times go through
`TOAN_TIME` (`ps2/include/sedamod.hpp`): `SedaToanTime` (`port/src/seda.cpp`) turns the time in the
playing motion into the same share of the way through Toan's motion of that number, so a hit lands as
far into Seda's swing as it did into Toan's. Without `MOD_SEDA` the macro is the time itself and the
PS2 build is unchanged.

A cutscene loads its characters' shared images from a pack of its own (as `gedit/s99/chara/e29img.pak`),
so a character's pack can lack images its configuration loads; drawn without them those parts are black.
`MissingImages` finds them in the packs beside the character's and puts them in its pack.
The model can also draw with a texture that only an image its configuration never loads holds (the
game loads only the images named by `IMG` lines); `MissingTextureImages` takes that image, from his own
pack first and else from the packs beside it, into the slot left (`IMG 3`). `dcdata seda` says for each
texture which image holds it.

Toan's character also reads other motion sets (an event's, the treasure box's and atla's
presentations), made for Toan's bones. On Seda's skeleton they would read past his bones, so while his
model is Toan's (no object named `c01...`) `ReadInfo` leaves those sets out (`SedaRefusesMotionSet`); in
those scenes he stands still.

Seda shows his own sword; the equipped weapon still deals the blows (its damage, element and reach)
but is not drawn. The pack says so with a member `seda_hide_equipped`, which the port reads when it
gives the pack (`SedaEquippedDrawn`, used by `Draw_MainUnit`). King Seda's sword gives way to the
equipped weapon, which is drawn.

`dcdata seda <data-directory> [seda|king] --weapon OBJECT --hide-weapon|--show-weapon --item OBJECT
--show-equipped|--hide-equipped` chooses anew, to try another place for the weapon without building
anything.

Seda has no run of his own (only `dun/mainchara/c07a.chr`, another model with another skeleton,
has one); his walk, played quicker, stands in for it.

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
