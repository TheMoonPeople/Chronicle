# Windows on x64

The PC port (`docs/PC.md`) builds natively with llvm-mingw clang on x64
Windows, using SDL3 and Vulkan. It needs your own PAL disc, as on Linux and
macOS.

## Dependencies

Install PowerShell 7, Python 3 (with `py` or `python` on PATH) and 7-Zip. CMake
3.28 or later is needed too; the scripts find `cmake` and `7z` on PATH, then try
their standard installations under `C:/Program Files`. A Vulkan driver with the
features listed in `tools/kazarov/src/cpp/gfx/README.md`, "Device", is required.

The build system is Kazarov, a submodule at `tools/kazarov`. A clone needs it:

```bat
git clone --recurse-submodules https://github.com/TheMoonPeople/Chronicle.git
```

or `git submodule update --init tools/kazarov` in an existing checkout. `build.bat`
and `run.bat` run that command themselves when the directory is empty.

## Building and running

From the checkout, in a Command Prompt:

```bat
build.bat                  & rem windows-x64-release: optimised, port\build\windows-x64-release
build.bat windows-x64      & rem Debug, port\build\windows-x64
run.bat                    & rem builds, extracts the game data once, then starts darkcloud.exe
```

`run.bat` takes the arguments for `darkcloud.exe` after the configuration name, and
`set CLEAN=1` before a build deletes the configuration's build directory first.
`set JOBS=N` sets the number of parallel jobs.

`tools/windows/kazarov.ps1` does the work. It runs `build.ps1` and `configure.ps1`
for the configuration, through the same `acquire.ps1` dependencies, and keeps the
downloads and locks in `<checkout>\.kazarov`, which both configurations share:

- `acquire.ps1` runs only when a dependency is missing. It verifies each pinned archive's SHA-256.
- `configure.ps1` runs only when the build directory has no `build.ninja`, or its
  `CMakeCache.txt` was made for another checkout path. Then the cache files are deleted first.
- `build.ps1` builds the executables with Ninja, so an unchanged tree rebuilds nothing.
- A lock in `.kazarov\locks` serialises runs of one configuration; other configurations run in parallel.

## Game data

`run.bat` extracts the data with `dcdata.exe` from `rom\pal\Dark Cloud (PAL).iso`,
or from `rom\Dark Cloud (PAL).iso` when the first is missing, into `data\` in the checkout.
It records the disc's and `dcdata.exe`'s sizes and times in `data\.kazarov-data`, so a
later run with both unchanged skips the extraction.

The PowerShell examples in this file use the variables below. Set them once,
from the checkout root:

```powershell
$root = (Get-Location).Path
$build = "$root\port\build\windows-x64-release"
```

To extract by hand:

```powershell
& "$root/port/build/windows-x64-release/dcdata.exe" extract "path/to/Dark Cloud (PAL).iso" "$root/data"
```

Use only an owned PAL disc image, or a directory holding its `DATA.DAT` and
`DATA.HD2`. Keep the image and extracted files outside git. Non-ASCII disc
filename bytes are written with `%xx` escapes on Windows, as on macOS; the
game uses the same lookup rule.

## Running

```powershell
& "$root/port/build/windows-x64-release/darkcloud.exe" --data "$root/data" --save "$root/save"
```

Options, controls and `config.json` are described in `docs/PC.md`. Without
explicit paths or local directories, Windows uses
`%LOCALAPPDATA%/chronicle/data` and `%LOCALAPPDATA%/chronicle/save` before the
XDG/HOME fallbacks. `--data`, `--save`, `DC_DATA` and `DC_SAVE` take precedence.

`--offscreen`, `--frames`, `--input` and `--screenshot` work as on Linux for
bounded runs; `DC_AUDIO=off` disables the audio device. Offscreen rendering
needs no Vulkan surface. A headless Vulkan window is a hidden Win32 window,
since SDL's offscreen video driver cannot make a Vulkan surface on Windows.

## Scripts for CI and manual builds

`.github/workflows/pc.yml` calls the scripts directly, and so can you. They
default `Root` to the checkout's parent directory; pass `-Root` to use another.
Sources always come from the checkout containing the scripts. `kazarov.ps1`
passes `Root` as `<checkout>\.kazarov\windows`.

```powershell
./tools/windows/acquire.ps1 -Root $root [-VulkanLoader]
./tools/windows/configure.ps1 -Root $root -BuildDirectory $build [-Tests] [-BuildType RelWithDebInfo] [-VulkanLibrary <dll>]
./tools/windows/build.ps1 -Root $root -BuildDirectory $build [-Tests] [-Jobs N]
```

Builds default to `<root>/build`, or `<root>/build-tests` for
`configure-tests.ps1`; `-BuildDirectory` overrides that location. Data and
saves may live beside them.

`acquire.ps1` downloads llvm-mingw 20260922 (Clang/lld 23.1.2, x64 UCRT), Ninja
1.13.2, SDL3 3.4.18, glslang 16.6.0 and Vulkan headers 1.4.363 from their
upstream release archives into `<root>/archives` and `<root>/deps`; no toolchain
installer is run. It verifies every pinned archive against its recorded SHA-256
before extraction. CMake also fetches nlohmann/json 3.12.0 and, with tests
enabled, GoogleTest v1.18.0. Vulkan links to the system's `vulkan-1.dll`. The
scripts do not install validation layers.

On a machine without a Vulkan loader (CI), `-VulkanLoader` also fetches
LunarG's 1.4.363 loader. Pass it to `configure.ps1` with `-VulkanLibrary` and
copy it beside the built executables, as the Windows job does.

### Linking

`port/src`'s `PORT_OVERRIDE` definitions replace `ps2/src`'s as on Linux and
macOS (`docs/PC.md`, "How `port/src` takes precedence"). Two things are there
for COFF:

- lld's MinGW driver has no `--defsym`, so the aliases `draw_rect` and
  `WorkBuffer__2` are `/alternatename` directives in `port/src/linknames.cpp`.
- lld resolves every name a COFF object uses before it drops the sections
  nothing reaches, so `port/src/runtime.cpp` replaces `ps2/src`'s stand-ins
  for the PS2 runtime, which use names only the PS2 link defines
  (`docs/PC.md`, "What is still a stub").

The build needs llvm-mingw's clang. MSVC and other architectures are not
supported.

## Tests

```powershell
$build = Join-Path $root 'build-tests'
./tools/windows/configure-tests.ps1 -Root $root -BuildDirectory $build
./tools/windows/build.ps1 -Root $root -BuildDirectory $build -Tests
ctest --test-dir $build -j 1 --output-on-failure
```

Use `ctest.exe` beside the discovered `cmake.exe` if CMake is not on PATH.
The suite is the one Linux runs plus Windows-only cases for
`%LOCALAPPDATA%`, Unicode paths and memory-card path handling. It takes
seconds. Set `DC_GFX_OFFSCREEN=1` to use surface-less fixtures instead of the
default hidden-window swapchains.

## Limits

Native builds, unit tests and bounded offscreen gameplay have been checked.
Interactive controls, visible windowed presentation, audible output, first-run
disc selection, gameplay save/reload and a full campaign are not qualified by
these checks. Validation-layer cleanliness is unverified without installed
layers. The game's data, save, input-script and screenshot paths support
Unicode; the standalone `dcdata` command, optional WAV capture and
memory-card leaf-name conversions retain narrow handling. Renderer
approximations, overlay reinitialization gaps, arena headroom and aborting
PS2 hardware stubs remain as described in `docs/PC.md`.
