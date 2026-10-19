# Nintendo Switch homebrew

A work in progress: the port (`docs/PC.md`) built as a homebrew `.nro` for a Switch running custom
firmware, with devkitPro's devkitA64 and libnx. It needs your own disc's data, as the PC port
does.

## How it runs

Two renderers, picked at build time (`DC_RENDERER`):

- **GL** (`port/gl`, the `switch-gl` preset): Kazarov's renderer redone over OpenGL 4.3 on
  devkitPro's own packaged Mesa (`switch-mesa`, nouveau GL), with SDL3 from devkitPro's
  `switch-sdl-3.4` branch, which devkitPro does not package yet and `tools/switch/Containerfile.gl`
  builds at a pinned commit. `port/gl/README.md` tells how it maps Vulkan's conventions onto GL.
- **Vulkan** (the `switch` preset): Kazarov's shared renderer unchanged, on
  [etonedemid/mesa-switch](https://github.com/etonedemid/mesa-switch), Mesa 26.2 with NVK (Vulkan
  1.3) on the Tegra X1, linked statically (there is no loader), and
  [etonedemid/SDL](https://github.com/etonedemid/SDL)'s Vulkan surfaces (`VK_NN_vi_surface`) on
  devkitPro's SDL3. `tools/switch/Containerfile` builds both. NVK on the Switch is young: it runs
  the game, but parts of Norune stay at 30 frames a second with the GPU half idle, waiting on fences.

`port/switch/` holds what libnx lacks underneath Kazarov:

- `memory.cpp`: the arenas on page-aligned heap, each followed by a guard page
  `svcSetMemoryPermission` makes inaccessible, in place of `mmap`.
- `ucontext.h`, `ucontext.cpp`: `getcontext`, `makecontext` and `swapcontext` for the eekernel
  threads, which newlib does not have.
- `switch_compat.h`: POSIX flags newlib lacks, included ahead of every unit.
- `fall_off_returns.txt`, `fall_off_returns.py`: GCC has no `-fno-strict-return` and miscompiles a
  function that falls off its end (it drops the branch that leads there, or runs on into the next
  function), so the build's copies of the 19 such functions in `ps2/src` get a return. ps2/src is
  compiled with `-Werror=return-type`, so a new one stops the build until it is listed.

Kazarov's `platform/files.cpp` has a Horizon branch for its atomic writes: Horizon opens no
directory as a file (so a directory is not synced; the file system commits a name when the call
returns), and its `rename` never replaces, so a replacing write moves the old file aside first.

Discord Rich Presence is off: there is no client to talk to.

## Building

The toolchain is a container image per renderer, built once: `tools/switch/Containerfile.gl` (a
few minutes: glslang, the Vulkan headers and SDL3 over devkitPro's image) and
`tools/switch/Containerfile` (about an hour: Mesa, Rust's standard library for the Switch,
SPIRV-Tools, glslang, SDL3). podman by default; `BUILDER=docker` uses docker.

```sh
SWITCH_PRESET=switch-gl tools/switch/build.sh   # port/build/switch-gl/darkcloud.nro: the GL renderer
tools/switch/build.sh            # builds the image if needed, then port/build/switch/darkcloud.nro (Vulkan)
tools/switch/build.sh -v         # arguments go to the build
tools/switch/build.sh shell      # a shell in the image
```

The GL renderer also builds for a PC (`cmake --preset linux-x64-gl-release`), where it draws the
same pictures as the Vulkan renderer, so it can be checked against it without a console.

The image is tagged by a hash of the Containerfile, so editing it builds a new one. VS Code can open
the checkout in the same image: `.devcontainer/switch/devcontainer.json` (with podman, set
`dev.containers.dockerPath` to `podman`).

## Installing

Copy to the SD card:

```
sdmc:/switch/chronicle/darkcloud.nro
sdmc:/switch/chronicle/lang/        (port/build/switch/lang)
sdmc:/switch/chronicle/glyphs/      (port/build/switch/glyphs)
sdmc:/switch/chronicle/data/        the extracted game data (dcdata extract, on a computer)
```

The Homebrew Menu starts the game in its own directory, so `data/` and `save/` beside the `.nro`
are found as they are beside the PC executable. Start it with a game's title overridden (hold R
while launching a game) rather than from the album applet: an applet gets a fraction of the
memory.

## Arguments

The Homebrew Menu passes no arguments, so `args.txt` beside the `.nro` supplies them: words
separated by whitespace, after any the launcher gave (`--jump edit:0`, say).

## Logs

Sent with `nxlink -s darkcloud.nro` (from devkitPro's switch-tools; the Homebrew Menu waits for it
on Y), the game prints its log, stdout and stderr, on the computer that sent it. Started any other
way it writes them to `darkcloud.log` beside the `.nro`, unbuffered, so a run that stops short
leaves its last lines there. nxlink sends only the `.nro`, which then runs from `sdmc:/switch/`, so
put `data/` and the other folders beside it there (or pass `--data`, e.g.
`nxlink -s darkcloud.nro -- --data sdmc:/switch/chronicle/data`).

## Testing from a computer

`tools/switch/run.sh` sends a build with nxlink and prints the game's log until it exits; its
arguments after `--` go to the game, which reads `--input` scripts on the console as it does on a
PC (absolute `sdmc:/` paths). `SWITCH_ADDRESS` names the Switch when nxlink's broadcast does not
reach it (a computer with several networks).

The Homebrew Menu's netloader (Y) takes one build, which runs from `sdmc:/switch/chronicle/` and
returns to the menu when it exits.

```sh
SWITCH_ADDRESS=192.168.1.99 tools/switch/run.sh -- --fast-load --frames 3000 \
    --input sdmc:/switch/chronicle/test/norune.input --save sdmc:/switch/chronicle/save-test
```

## Performance

Handheld, at the screen's 1280x720, Norune runs at 60 frames a second (laps of the village from a
save: under 1% of ticks over 16.7 ms). What gets it there:

- **GPU clock.** A homebrew title starts with the handheld GPU at 307.2 MHz; retail games get 384
  and some ask for 460.8 MHz. `port/switch/clocks.cpp` asks for the 460.8 MHz configuration (an
  official one: CPU 1020 MHz, memory 1600 MHz) unless `"switch_gpu_boost": false` is set under
  `video` in `save/config.json`. Docked is left alone, and the setting ends with the game.
- **Render targets at the screen's scale.** The render scale is the screen's height over 480
  (1.5 handheld) rather than rounded up to 2.
- **Two ticks in flight** (Kazarov's `kFramesInFlight`), so the CPU records a tick while the GPU
  draws the last.
- **No per-draw branches in `draw.frag`.** NAK compiles a uniform branch into a sync and nops, and on
  the Tegra X1 a fragment costs about its instruction count; the push constants carry each draw's
  choices as values to select between.
- **Palette textures expanded** to RGBA (Kazarov's `RendererConfig::expand_palettes`), filtered by
  the sampler instead of looked up four times a pixel. At a distance they use their mip levels,
  which the shader lookup did not.
- **Shadow composites scissored** to the screen bounds of their volumes (`Draw3DShadowCover`),
  which also helps the PC build.
- **Smooth motion off** by default: a display frame between ticks costs the GPU about what a tick
  does.
- **A render thread** on the second CPU core (`gameloop.cpp`, Kazarov's gfx README "Threads"): a
  tick's render and present run while the game runs the next tick. One after the other they took
  about 17 ms of CPU a tick in Norune, the game's own work about 10 of it.
- **Depth queries answered when their frame is done** (Kazarov's `kAsyncDepthQueries`), a tick or
  two late, instead of the CPU waiting for the frame whenever the game reads depth (the sun's lens
  flare, every tick in Norune).

GPU timestamps on the Switch count 19.2 MHz ticks (nanoseconds times 384 / 625) on both NVK and
devkitPro's Mesa: a profile must scale them by 625 / 384. Norune's GPU now takes about 15 ms of a
tick; busy dungeon scenes (the first floor's tutorial, with its shadows) take more and run near 40.

## Status

- Runs on a Switch OLED (Atmosphère, from the Homebrew Menu with a title overridden): the intro,
  the title, new and loaded games, Norune.
- Ryujinx runs the GL `.nro` with its picture (at 60 frames a second on a desktop GPU, which says
  nothing of the console's speed); `--screenshot` there is wrong (`port/gl/README.md`). It runs the
  Vulkan `.nro` only with its OpenGL backend, and shows a black screen.
