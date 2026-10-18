# Nintendo Switch homebrew

A work in progress: the port (`docs/PC.md`) built as a homebrew `.nro` for a Switch running custom
firmware, with devkitPro's devkitA64 and libnx. It needs your own disc's data, as the PC port
does.

## How it runs

Kazarov's shared host code runs unchanged:

- **Vulkan** comes from [etonedemid/mesa-switch](https://github.com/etonedemid/mesa-switch), Mesa
  26.2 with NVK (Vulkan 1.3) on the Tegra X1, linked statically (there is no loader). The renderer's
  requirements (`tools/kazarov/port/gfx/README.md`, "Device") are all features Maxwell has.
- **SDL3** is devkitPro's libnx port (its `switch-sdl-3.4` branch) with
  [etonedemid/SDL](https://github.com/etonedemid/SDL)'s Vulkan surfaces (`VK_NN_vi_surface`).

Neither is packaged by devkitPro yet; `tools/switch/Containerfile` builds both at pinned commits.
Both are young: NVK on the Switch passes 79 of 84 of Sascha Willems' Vulkan samples, but its author
reports a console hang with SDL_GPU (which the port does not use) and unprofiled performance.

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

The toolchain is a container image, built once from `tools/switch/Containerfile` (about an hour:
Mesa, Rust's standard library for the Switch, SPIRV-Tools, glslang, SDL3). podman by default;
`BUILDER=docker` uses docker.

```sh
tools/switch/build.sh            # builds the image if needed, then port/build/switch/darkcloud.nro
tools/switch/build.sh -v         # arguments go to the build
tools/switch/build.sh shell      # a shell in the image
SWITCH_PRESET=switch-gl tools/switch/build.sh   # port/build/switch-gl: the GL renderer instead
```

The GL renderer (`port/switch/gl`, `DC_SWITCH_RENDERER=gl`) is kept in reserve should NVK fail on
a console. It is a null backend for now: it answers every `gfx.hpp` call over Kazarov's display
lists and TIM2 decoding but draws nothing, which still runs the game without its picture, so
input, audio and saving can be told apart from the renderer.

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

## Performance

Handheld, at the screen's 1280x720, Norune runs at 50 to 60 frames a second. What gets it there:

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

What remains is the CPU, at about 13 ms a tick (the game's own work and recording the draws).

## Status

- Runs on a Switch OLED (Atmosphère, from the Homebrew Menu with a title overridden): the intro,
  the title, new and loaded games, Norune.
- Ryujinx runs the `.nro` (its OpenGL backend; its Vulkan backend crashes on NVK's images) but
  shows a black screen, which is still useful for crashes before the first frame.
