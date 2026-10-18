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

## Logs

Sent with `nxlink -s darkcloud.nro` (from devkitPro's switch-tools; the Homebrew Menu waits for it
on Y), the game prints its log, stdout and stderr, on the computer that sent it. Started from the
Homebrew Menu it prints nothing. Send `data/` and the other folders to the SD card first; nxlink
sends only the `.nro`, which then runs from `sdmc:/switch/`, so put them beside it there (or pass
`--data`, e.g. `nxlink -s darkcloud.nro -- --data sdmc:/switch/chronicle/data`).

## Status

- `darkcloud.nro` builds and links (the PC build and its tests are unchanged).
- Not yet run on a console: nothing past the link is known to work.
