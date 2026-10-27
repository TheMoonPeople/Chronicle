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
  It also installs Rust and FFmpeg (see "Rust and FFmpeg" below), so the first run downloads them.
- `configure.ps1` runs only when the build directory has no `build.ninja`, or its
  `CMakeCache.txt` was made for another checkout path, which deletes the cache files first,
  or without the current `KZ_FFMPEG_DIR`.
- `build.ps1` builds the executables with Ninja, so an unchanged tree rebuilds nothing.
- A lock in `.kazarov\locks` serialises runs of one configuration; other configurations run in parallel.

## Game data

`run.bat` extracts the data with `dcdata.exe` from `rom\pal\Dark Cloud (PAL).iso`,
or from `rom\Dark Cloud (PAL).iso` when the first is missing, into `port\run\data\` in the checkout.
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
& "$root/port/build/windows-x64-release/dcdata.exe" pack "$root/data"
```

`pack` writes `$root/data.zip`, including the normalized copies and language list. The game accepts `data.zip` beside the logical `data` path without keeping loose files installed; the archive takes priority when both are present. `run.bat` still extracts into `port\run\data` for its automatic setup.

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
`run.bat` explicitly selects `port\run\data` and `port\run\save` and launches
from `port\run`, keeping runtime output there. Pass `--data` or `--save` to the
script to override its defaults; use absolute paths for inputs outside that folder.

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
1.13.2, SDL3 3.4.18, an LGPL shared FFmpeg n9.0.2 build, glslang 16.6.0 and Vulkan headers 1.4.363 from their
upstream release archives into `<root>/archives` and `<root>/deps`; no toolchain
installer is run. It verifies every pinned archive against its recorded SHA-256
before extraction. CMake also fetches nlohmann/json 3.12.0 and, with tests
enabled, GoogleTest v1.18.0. Vulkan links to the system's `vulkan-1.dll`. The
scripts do not install validation layers.

On a machine without a Vulkan loader (CI), `-VulkanLoader` also fetches
LunarG's 1.4.363 loader. Pass it to `configure.ps1` with `-VulkanLibrary` and
copy it beside the built executables, as the Windows job does.

### Rust and FFmpeg

`dcdata` links Kazarov's `kazarov_bridge` (Rust, with FFmpeg for the movies and
texture baking), so the port graph needs both. `acquire.ps1` installs them under `<root>/deps`:

- FFmpeg n9.0.2 from BtbN's `win64-lgpl-shared` build, verified by SHA-256. The script
  rejects an archive whose `LICENSE.txt` is not the LGPL. `configure.ps1` passes it as
  `-DKZ_FFMPEG_DIR`, and `build.ps1` copies its DLLs beside the executables.
- `acquire-rust.ps1`: Rust 1.99.0 for `x86_64-pc-windows-gnullvm` (the version in
  `tools/kazarov/rust-toolchain.toml`) from `static.rust-lang.org`, Corrosion 0.6.1, the
  prebuilt `cxxbridge` 1.0.205 and the locked Cargo crates, in `<root>/deps/rust`. It
  keeps its own `CARGO_HOME` there. `-SkipRustfmt` and `-SkipClippy` leave out those tools.
  `configure.ps1` passes the compiler, `Rust_CARGO`, the target, `INSTALLED_CXXBRIDGE` and
  `FETCHCONTENT_SOURCE_DIR_CORROSION`, and `build.ps1` puts the toolchain on `PATH`.

No system Rust or `rustup` is used. On Linux and macOS the Kazarov image provides
both (`/opt/kazarov/ffmpeg`); on macOS outside the container run
`tools/kazarov/scripts/build/kz-ffmpeg --out .kazarov/ffmpeg/darwin-arm64` and install Rust 1.99.0 and
`cxxbridge-cmd` 1.0.205 yourself (`docs/MACOS.md`).

### Linking

`port/src`'s replacements link with `port/src/game` as on Linux and
macOS (`docs/PC.md`, "The game's code in the port"). One thing is there
for COFF:

- lld resolves every name a COFF object uses before it drops the sections
  nothing reaches, so `port/src/app/runtime.cpp` replaces `port/src/game`'s stand-ins
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
