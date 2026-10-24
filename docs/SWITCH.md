# Nintendo Switch

The port (`docs/PC.md`) built as a homebrew `.nro` for a Switch running custom firmware, with
devkitPro's devkitA64 and libnx. It plays the game handheld at the screen's 1280x720 and, in towns,
at 60 frames a second. Like the PC port it needs your own disc's data.

- [What you need](#what-you-need)
- [Quick start](#quick-start)
- [Building](#building)
- [Installing](#installing)
- [Playing](#playing)
- [Saves](#saves)
- [Logs and crashes](#logs-and-crashes)
- [Testing from a computer](#testing-from-a-computer)
- [How it runs](#how-it-runs)
- [What the port changes for the Switch](#what-the-port-changes-for-the-switch)
- [Performance](#performance)
- [Status and known issues](#status-and-known-issues)

## What you need

- A Switch that runs homebrew: Atmosphère with the Homebrew Menu. Developed on a Switch OLED.
- About 2 GB free on its SD card (the game's data is 1.8 GB).
- A _Dark Cloud_ disc image, ideally PAL (`docs/PC.md`, "Game data"). The port is always the PAL
  release; NTSC saves load in it.
- A Linux computer with podman (or docker) to build, and the PC port built once for `dcdata`,
  which extracts the disc.
- A way to copy files to the SD card: a card reader, or MTP over USB from the console (DBI's MTP
  responder, which Linux mounts with `jmtpfs`).

## Quick start

```sh
git clone --recurse-submodules -b switch-port https://github.com/columna1/Chronicle-nintendo-switch.git Chronicle && cd Chronicle
./build.sh linux-x64-release                                     # the PC build, for dcdata
port/build/pc-release/dcdata extract "rom/pal/Dark Cloud (PAL).iso" data
./build.sh switch                                                # port/build/switch/darkcloud.nro
```

Copy to the SD card ([Installing](#installing)):

```
sdmc:/switch/chronicle/darkcloud.nro   port/build/switch/darkcloud.nro
sdmc:/switch/chronicle/lang/           port/build/switch/lang/
sdmc:/switch/chronicle/glyphs/         port/build/switch/glyphs/
sdmc:/switch/chronicle/data/           data/ (from dcdata)
```

and start **Chronicle** from the Homebrew Menu, opened by holding R while starting a game.

## Building

The toolchain is a container image, built once and tagged by a hash of its Containerfile and patches, so an
edit to it builds a new image and an old checkout keeps its own. podman by default; `BUILDER=docker`
uses docker. The build runs as the calling user, so the tree it writes stays yours.

```sh
port/switch/build.sh            # the image if needed, then port/build/switch/darkcloud.nro (Vulkan)
port/switch/build.sh -v         # arguments go to the build
port/switch/build.sh shell      # a shell in the image
```

The first image build takes about an hour: Mesa with NVK, Rust's standard library for the Switch,
SPIRV-Tools, glslang and SDL3. It starts from a pinned `devkitpro/devkita64` image and pins every
source it builds by commit or release. The build is CMake with Ninja and devkitPro's `Switch.cmake` toolchain
(`CMakePresets.json`); `port/switch/switch.cmake` adds what the Switch needs and packages the `.nro`
with its title, author and icon (`port/switch/icon.jpg`).

VS Code can open the checkout in the same image: `.devcontainer/switch/devcontainer.json` (with
podman, set `dev.containers.dockerPath` to `podman`).

## Installing

Everything lives in `sdmc:/switch/chronicle/`:

| On the SD card | From | What |
|---|---|---|
| `darkcloud.nro` | `port/build/switch/darkcloud.nro` | the game |
| `lang/` | `port/build/switch/lang/` | the port's own strings and message fonts |
| `glyphs/` | `port/build/switch/glyphs/` | the button symbols |
| `data/` | `dcdata extract` | the disc's files |
| `save/` | made by the game | `config.json`, the saves, the Vulkan pipeline cache |
| `args.txt` | optional | arguments ([Playing](#playing)) |

The Homebrew Menu starts the game in its own directory, so `data/` and `save/` beside the `.nro` are
found as they are beside the PC executable. `data/` is checked against the list in the disc's
`DATA.HD2` at start; files missing or of the wrong size are reported in the log.

Over MTP, copying `data/` (about 5000 files) takes a while; a card reader is faster. To update the
game later only `darkcloud.nro` changes, and `lang/` when its strings do.

## Playing

Start the game from the Homebrew Menu **with a game's title overridden**: hold R while starting any
game, then pick Chronicle. Started from the album applet it gets a fraction of the memory and cannot
run.

- **Controls** are the PC port's gamepad layout (`docs/PC.md`), handheld or with a Pro Controller.
- **Settings** are the PC port's `save/config.json` and in-game Options. The Switch's own
  defaults: `video.interpolation` (smooth motion) off, since a display frame between ticks costs its
  GPU as much as a tick does; and `video.switch_gpu_boost` on ([What the port changes for the
  Switch](#what-the-port-changes-for-the-switch)). `video.present_mode` is `fifo` (vsync);
  `immediate` presents without waiting and shows no tearing handheld.
- **Arguments.** The Homebrew Menu passes none, so `args.txt` beside the `.nro` supplies them: words
  separated by whitespace (`--jump edit:0 --fast-load`, say; `docs/PC.md` lists them).
- **Quit** with HOME and closing the software.

## Saves

The saves are the PC port's (`docs/PC.md`, "Saves and host files"): `save/saves/<board>/save.dat`,
retail's card image of a save unchanged, and `save/saves/state.json`, which remembers the last
board. A save from a PC install loads as it is, and so does one from a PS2 memory card.

`tools/ps2card/ps2card.py` reads a PS2 memory card image (PCSX2's `.ps2`, or a raw dump) and exports
its Dark Cloud saves (PAL, NTSC or Japanese) into a save directory, after the boards it holds and
never over one:

```sh
tools/ps2card/ps2card.py list ~/.config/PCSX2/memcards/Mcd001.ps2
tools/ps2card/ps2card.py export ~/.config/PCSX2/memcards/Mcd001.ps2 /path/to/sd/switch/chronicle/save
```

A card's `darkcloud0` to `darkcloud11` (retail's twelve files) become consecutive boards in card
order. The card image is only read.

## Logs and crashes

Started from the Homebrew Menu, the game writes its log (stdout and stderr) to `darkcloud.log`
beside the `.nro`, unbuffered, so a run that stops short leaves its last lines there. Sent with
nxlink ([Testing from a computer](#testing-from-a-computer)), it prints the log on the computer that
sent it instead.

A crash leaves Atmosphère's report in `sdmc:/atmosphere/crash_reports/`; the newest is the last by
name (the names lead with the console's clock, which may be wrong). Its addresses read
`darkcloud + 0x...`; the build's `port/build/switch/darkcloud.elf` turns them into functions and
lines:

```sh
port/switch/build.sh shell -c 'aarch64-none-elf-addr2line -f -C -i -e port/build/switch/darkcloud.elf 0x40e820'
```

## Testing from a computer

`port/switch/run.sh` sends a build to the Homebrew Menu's netloader (Y), which saves it as
`sdmc:/switch/chronicle/darkcloud.nro` and starts it, and prints the game's log as it runs. Its
arguments after `--` go to the game. `SWITCH_ADDRESS` names the Switch (the netloader shows its
address) when nxlink's broadcast does not reach it, as on a computer with several networks.

```sh
SWITCH_ADDRESS=192.168.1.99 port/switch/run.sh                       # the build, as installed
SWITCH_ADDRESS=192.168.1.99 port/switch/run.sh -- --fast-load --frames 5300 \
    --input sdmc:/switch/chronicle/test/norune.input --save sdmc:/switch/chronicle/save-test
```

The game reads `--input` scripts on the console as on a PC (`docs/PC.md`, "Scripted input"; give
them `sdmc:/` paths), so a scripted run is repeatable: from the title, Continue, the last file, then
a route through a town. Test against a copy of a save directory (`--save`) so a run never touches
the real one. `--jump` starts a mode with the developer menu's defaults rather than a save's state,
which in a town is a much lighter scene than a played game's.

One build runs per press of Y: when the game exits, the Switch returns to the Homebrew Menu. A
loader that stayed resident and started each build sent to it did not hold up, because NVK leaves
memory behind when the game exits that the next program in the same process trips over.

## How it runs

**The renderer.** Kazarov's Vulkan renderer (`tools/kazarov/src/cpp/gfx`) runs unchanged on
[etonedemid/mesa-switch](https://github.com/etonedemid/mesa-switch), Mesa 26.2 with NVK (Vulkan 1.3)
on the Tegra X1, linked statically (there is no loader). SDL3 is devkitPro's `switch-sdl-3.4` branch
with [etonedemid/SDL](https://github.com/etonedemid/SDL)'s Vulkan surfaces (`VK_NN_vi_surface`),
plus one patch (`port/switch/patches/sdl-vulkan-window-without-egl.patch`): the fork refused every
window until EGL was loaded, which SDL does only for OpenGL windows. NVK compiles the shaders through
NAK; the pipeline cache in `save/` keeps later starts quick.

**The platform underneath Kazarov** (`port/switch/src/`, with the headers in `port/switch/include/`)
supplies what libnx lacks:

- `memory.cpp`: the game's arenas on page-aligned heap, each followed by a guard page that
  `svcSetMemoryPermission` makes inaccessible, in place of `mmap`. Every guard page still standing
  is made accessible again when the game exits, since the Homebrew Menu's loader starts the next
  program on the same heap.
- `ucontext.h`, `ucontext.cpp`: `getcontext`, `makecontext` and `swapcontext` for the emulated EE
  kernel's threads, which newlib does not have.
- `switch_compat.h`: POSIX flags newlib lacks, included ahead of every unit.
- `nxlink.cpp`: the log, to nxlink's host or `darkcloud.log`.
- `main.cpp`: the arguments from `args.txt`, and the render scale from the screen's height.
- `clocks.cpp`: the handheld GPU clock.
- `assert.cpp`, `discord.cpp`, `firstrun.cpp`: newlib's assertion message, and no Discord or
  first-run setup window on a console.
- `patch_simdjson.cmake`: simdjson without its `mmap` file reader, which fastgltf does not use.

GCC has no `-fno-strict-return` and miscompiles a function that falls off its end (it drops the
branch that leads there, or runs on into the next function), so the 19 such functions in `ps2/src`
end in a `PORT_RETURN`, and `ps2/src` is compiled with `-Werror=return-type` so that a new one stops
the build.

Kazarov's `platform/files.cpp` has a Horizon branch for its atomic writes: Horizon opens no directory
as a file (so a directory is not synced; the file system commits a name when the call returns), and
its `rename` never replaces, so a replacing write moves the old file aside first. Discord Rich
Presence is off: there is no client to talk to.

## What the port changes for the Switch

What a console needed that a PC did not, roughly in the order it came up:

- **Settings saved** through Kazarov's `FilesWrite` (`port/src/platform/config.cpp`): Horizon's
  `rename` never replaces a file, so `config.json` was written once and every later change was lost.
- **Plain `char` is signed** (`-fsigned-char`), as on the PS2 and x64. AArch64's is unsigned, and
  `ps2/include/types.h` makes `s8` a `char`: a sound table's -128 terminator read as 128 and
  `SndInitSeTable` wrote past it, which crashed the game entering a dungeon.
- **The GPU clock.** A homebrew title starts with the handheld GPU at 307.2 MHz; retail games get
  384 and some 460.8. `clocks.cpp` asks for the 460.8 MHz configuration, an official one (CPU 1020
  MHz, memory 1600 MHz), unless `"switch_gpu_boost": false` is set under `video`. Docked is left
  alone, and the setting ends with the game.
- **Render targets at the screen's scale**: the render scale is the screen's height over 480 (1.5
  handheld) rather than rounded up to 2.
- **`draw.frag` cut down for the Tegra X1**, where a fragment costs about its instruction count.
  NAK compiles a uniform branch into a sync and padding, so the shader selects between each draw's
  choices instead of branching on them; the CPU works out once a draw what the shader would
  otherwise decode from packed bits every pixel (fog colour, blend factor, TA0, the alpha test's
  verdicts), which Maxwell's integer conversions made slow; and the fog that stays continuous
  across clipped edges takes its per-draw terms from push constants, so a vertex passes three fog
  values instead of eight and only fogged draws work it out. In Norune the GPU's render of a tick
  went from 17 to 18.5 ms back to about 11.8.
  For the same reason the draw shader leaves out the smooth-motion debugging tint
  (`KZ_PORT_INTERPOLATION_TINT`, on for PC builds), which made Norune's frames 3 ms slower.
- **Palette textures expanded** to RGBA on the CPU (Kazarov's `RendererConfig::expand_palettes`),
  filtered by the sampler instead of looked up four times a pixel.
- **Cheaper effects**, which help the PC build too: the shadow composites scissored to their
  volumes' screen bounds, a frame's later shadow groups clearing only what the earlier ones drew,
  the shadow volumes drawn as indexed corners, and each depth-of-field band in one premultiplied draw where the GS needed two.
- **The render thread on the second CPU core.** Kazarov's render thread (`video.render_thread`,
  `async` by default) presents while the game runs the next tick, and on the Switch it renders each
  tick's canonical image too (`port/src/gameloop.cpp`, `kTickOnWorker`): rendered on the game thread,
  the render and the game's work came one after the other and the opening drew 20 to 25 frames a
  second; on the worker it draws 40 to 50. libnx starts every thread on the process's core, where
  the game runs, so on the Switch the worker moves itself to core 1. Kazarov keeps four submissions (two ticks) in flight and answers the game's depth queries
  (the sun's lens flare, pick-Z) without waiting for the GPU.

## Performance

Handheld at 1280x720, from a played save, running laps of Norune: 60 frames a second, with under 1%
of ticks over 16.7 ms. The GPU takes about 15 ms of a tick there; the game's own work about 10 ms of
CPU on the main core, the render and present about 6 on the second.

Busy dungeon scenes are the GPU's limit: the first floor's tutorial, two lit characters by the
torches, takes about 23 ms of GPU a tick (the shadow counting and the main scene about 7.5 ms each,
full-screen copies about 4), so it runs near 40.

Muska Lacka's heat haze accumulates the frame beyond eight depth planes into two full-screen images.
Drawn as eight blended passes each, those took 13 ms of GPU a tick and the town ran near 40; drawn as
one pass each that reads the depth buffer (Kazarov's `DrawState::depth_coverage`,
`port/src/effectmacro.cpp`) they take about 3, and the town holds 60 with the GPU at 7 ms a
submission.

Profiling on the console: GPU timestamps count 19.2 MHz ticks (nanoseconds times 384 / 625) on both
NVK, which reports a `timestampPeriod` of 1, and devkitPro's Mesa. A profile must scale them by
625 / 384, or the GPU looks idle a third of the time it is busy.

## Status and known issues

- Plays handheld on a Switch OLED: the intro, the title, new and loaded games (PS2 saves included),
  towns and dungeons. Docked has not been tuned.
- Busy dungeon scenes run near 40 frames a second ([Performance](#performance)).
- Ryujinx runs the `.nro` only on its OpenGL backend, with a black screen.
- What the PC port does not do yet, the Switch does not either (`docs/PC.md`).
