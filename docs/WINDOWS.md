# Windows on x64

The PC port (`docs/PC.md`) builds natively with llvm-mingw clang on x64
Windows, using SDL3 and Vulkan. It needs your own PAL disc, as on Linux and
macOS.

## Dependencies

Install PowerShell 7, CMake 3.28 or later, Python 3 (with `py` or `python` on
PATH), and 7-Zip. A Vulkan driver with the features listed in
`port/src/gfx/README.md`, "Device", is required. The scripts find `cmake` and
`7z` on PATH, then try their standard installations under `C:/Program Files`.

From the checkout, acquire the portable dependencies:

```powershell
$root = Split-Path -Parent (Get-Location).Path
./tools/windows/acquire.ps1 -Root $root
```

This downloads llvm-mingw 20260922 (Clang/lld 23.1.2, x64 UCRT), Ninja 1.13.2,
SDL3 3.4.18, glslang 16.6.0 and Vulkan headers 1.4.363 from their upstream
release archives. Archives and dependencies go into `<root>/archives` and
`<root>/deps`; no toolchain installer is run. CMake also fetches nlohmann/json
3.12.0 and, with tests enabled, GoogleTest v1.18.0. Vulkan links to the
system's `vulkan-1.dll`. The scripts do not install validation layers.

All scripts default `Root` to the checkout's parent directory. Pass `-Root`
to use another directory. Sources always come from the checkout containing
the scripts. Builds default to `<root>/build`, or `<root>/build-tests` for
`configure-tests.ps1`; `-BuildDirectory` overrides that location. Data and
saves may live beside them.

`acquire.ps1` verifies every pinned archive against its recorded SHA-256 before extraction.

On a machine without a Vulkan loader (CI), `-VulkanLoader` also fetches
LunarG's 1.4.363 loader. Pass it to `configure.ps1` with `-VulkanLibrary` and
copy it beside the built executables, as the Windows job in
`.github/workflows/pc.yml` does.

## Building

```powershell
./tools/windows/configure.ps1 -Root $root
./tools/windows/build.ps1 -Root $root
```

The scripts configure Ninja with `PLATFORM=PC`, a Debug build
(`-BuildType RelWithDebInfo` for an optimised one) and the downloaded
dependencies, then build `darkcloud` and `dcdata`. The build script copies
`libc++.dll`, `libunwind.dll`, `libwinpthread-1.dll` and `SDL3.dll` beside the
executables. `-Jobs N` sets parallelism.

### Linking

`port/src`'s `PC_OVERRIDE` definitions replace `ps2/src`'s as on Linux and
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

## Game data

```powershell
& "$root/build/dcdata.exe" extract "path/to/Dark Cloud (PAL).iso" "$root/data"
```

Use only an owned PAL disc image, or a directory holding its `DATA.DAT` and
`DATA.HD2`. Keep the image and extracted files outside git. Non-ASCII disc
filename bytes are written with `%xx` escapes on Windows, as on macOS; the
game uses the same lookup rule.

## Running

```powershell
& "$root/build/darkcloud.exe" --data "$root/data" --save "$root/save"
```

Options, controls and `config.json` are described in `docs/PC.md`. Without
explicit paths or local directories, Windows uses
`%LOCALAPPDATA%/chronicle/data` and `%LOCALAPPDATA%/chronicle/save` before the
XDG/HOME fallbacks. `--data`, `--save`, `DC_DATA` and `DC_SAVE` take precedence.

`--offscreen`, `--frames`, `--input` and `--screenshot` work as on Linux for
bounded runs; `DC_AUDIO=off` disables the audio device. Offscreen rendering
needs no Vulkan surface. A headless Vulkan window is a hidden Win32 window,
since SDL's offscreen video driver cannot make a Vulkan surface on Windows.

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
