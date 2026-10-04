# macOS and Apple Silicon support plan

This plan brings the PC port (`docs/PC.md`, `docs/PC_PORT_PLAN.md`) to macOS on
arm64, rendering through KosmicKrisp, Mesa's Vulkan driver on top of Metal.
Nothing here changes the port's rules: C++26, SDL3, native Vulkan, weak-linkage
replacement, no PS2 emulation, no edits under `ps2/src`.

## 1. What KosmicKrisp is, and what must be checked

KosmicKrisp is LunarG's Vulkan driver in Mesa (first released in Mesa 26.0;
`mesa-25.3.0`'s `meson.options` does not list it) that
implements Vulkan on Apple's Metal API, with shaders compiled from NIR to the
Metal Shading Language. It is a normal Mesa ICD, loaded by the Khronos Vulkan
loader on macOS like any other driver, and it was announced as Vulkan 1.3
conformant on Apple Silicon. The documentation (`docs.mesa3d.org/drivers/kosmickrisp.html`)
and the Mesa tree were not reachable from the environment this plan was written
in, so the implementing agents verify, against the Mesa source they build, every
item marked **[verify]**. The items below were since read from `mesa-26.2.4`
(the tag `scripts/host/mesa-macos.sh` pins) through the GitHub mirror
`FireBurn/mesa` (`gitlab.freedesktop.org`, `docs.mesa3d.org` and the
`Mesa3D/mesa` mirror were not reachable).
Source-read is not run: what only a Mac can show stays **[verify]**.

- **[verified: 1.4]** The Vulkan API version KosmicKrisp reports (1.3, or 1.4 in a
  newer Mesa). The renderer currently requires 1.4 (section 4.2).
  The device reports `VK_MAKE_VERSION(1, 4, VK_HEADER_VERSION)`
  (`kk_physical_device.c`, `kk_get_vk_version`) and the manifest says 1.4;
  the driver's own `vkEnumerateInstanceVersion` says 1.3.
- **[verified, one absent]** Device features the renderer requires: `dualSrcBlend`,
  `shaderClipDistance`, descriptor indexing with update-after-bind sampled
  images (8192), dynamic rendering, synchronization2, `D32_SFLOAT_S8_UINT` as a
  depth/stencil attachment, and the optional
  `VK_EXT_extended_dynamic_state3` colour write mask (a pipeline-variant
  fallback exists).
  All advertised (update-after-bind sampled image limits are 2^20;
  `D32_SFLOAT_S8_UINT` is Metal's native depth32/stencil8, and there is no
  D24S8) except `extendedDynamicState3ColorWriteMask`: EDS3 is exposed for
  depth clamp, depth clip -1..1, line rasterization mode, sample locations
  and tessellation origin only, so the pipeline-variant fallback is the path
  taken on KosmicKrisp.
- **[verified: no]** Whether it reports `VK_KHR_portability_subset`; if so the
  instance must enable `VK_KHR_portability_enumeration` and set
  `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`, and the subset's
  restrictions must be checked against the renderer's use (point size,
  triangle fans, separate stencil masks, image view formats).
  Not in its device extensions, and its manifest is not generated with
  `is_portability_driver`, so the loader enumerates it without the
  portability bit. Enabling portability enumeration when the extension
  exists is still right for MoltenVK.
- **[verified: metal yes, headless no]** Surface support: `VK_EXT_metal_surface` through SDL3 (SDL3's
  macOS Vulkan path), and whether `VK_EXT_headless_surface` exists; if not,
  headless runs need the renderer's own surface-less mode (section 4.3).
  `kk_instance.c` advertises `VK_EXT_headless_surface` only
  `#ifndef VK_USE_PLATFORM_METAL_EXT`, i.e. never in a `-Dplatforms=macos`
  build: headless runs on KosmicKrisp need the surface-less mode. Lavapipe
  built the same way has both. SDL3's Metal path itself is **[verify]**.
- **[verified]** The meson options and macOS/Xcode minimums
  (`-Dvulkan-drivers=kosmickrisp`, plus `swrast` for lavapipe on macOS), the
  installed ICD name (`kosmickrisp_icd.json`) and where the loader looks for
  it (`/usr/local/share/vulkan/icd.d`, `~/.local/share/vulkan/icd.d`,
  `VK_DRIVER_FILES`).
  KosmicKrisp is built on Metal 4 and needs **macOS 26** (Mesa's
  `docs/drivers/kosmickrisp.rst`), plus Xcode, meson 1.9.1+, LLVM 20.1.8+,
  libclc, SPIRV-LLVM-Translator and SPIRV-Tools; the documented options are
  `-Dplatforms=macos -Dvulkan-drivers=kosmickrisp -Dgallium-drivers=
  -Dopengl=false -Dzstd=disabled --prefer-static`. The manifest is
  `kosmickrisp_mesa_icd.<cpu>.json` (not `kosmickrisp_icd.json`) in
  `<prefix>/share/vulkan/icd.d`. The loader's macOS search order (Vulkan-Loader
  `docs/LoaderDriverInterface.md` at v1.4.321) is the bundle's
  `Contents/Resources/vulkan/icd.d`, then `~/.config`, `/etc/xdg`,
  `/usr/local/etc`, `/etc`, `~/.local/share`, `/usr/local/share`,
  `/usr/share` (each `+ /vulkan/icd.d`); `VK_DRIVER_FILES` replaces it.
  Which Xcode version a Metal 4 build needs is **[verify]** (the CI runner's
  default Xcode on `macos-26`).
- **[verify]** Pipeline compilation cost: NIR to MSL plus Metal's own compile
  makes the 504 startup pipelines slower than on lavapipe; the on-disk
  `VkPipelineCache` must demonstrably work (Metal binary archives behind it).
  Needs a run; the CI job's KosmicKrisp log records the `compiling shaders`
  progress.

MoltenVK is not used: the point of this work is a Mesa driver with a shared
shader compiler and Vulkan 1.3 conformance; MoltenVK stays an undocumented
alternative that may happen to work through the same loader.

## 2. What is Linux-specific today

| Area | File | Linux assumption | macOS answer |
|---|---|---|---|
| Arenas | `port/src/dataset.cpp` | `mmap(MAP_32BIT)` keeps arenas below 2 GiB so the game's `(int)` pointer casts survive; `madvise(MADV_DONTNEED)` zeroes | No `MAP_32BIT`. ~~Shrink `__PAGEZERO` (`-Wl,-pagezero_size,0x1000`) and map with a hint address~~: **not possible on arm64**. XNU refuses to exec a 64-bit arm64 image whose page zero is under 4 GiB (`mach_loader.c`, macOS 13 to 26), so nothing maps below 4 GiB. Only an x86_64 build under Rosetta can (`DC_MACOS_PAGEZERO_SIZE`, default `0x1000` there) |
| Executable path | `port/src/platform/paths.cpp` | `/proc/self/exe` | `_NSGetExecutablePath` + `realpath` |
| Executable layout | `port/CMakeLists.txt` | `-no-pie` keeps `.data`/`.bss` below 4 GiB | arm64 macOS is PIE-only, loads images above 4 GiB and forbids low mappings; nothing may depend on a 32-bit round trip (section 3) |
| Weakening | `port/CMakeLists.txt` | `ld.lld -r` then `llvm-objcopy --weaken` on ELF | Apple's `ld -r` (`ld64.lld` has no `-r`); `llvm-objcopy --weaken` on Mach-O **[verified]** with LLVM 20 on an arm64 object (sets `N_WEAK_DEF`, leaves references alone). `tools/weaken` writes the same bytes and is selectable with `DC_MACHO_WEAKEN=tool` |
| Interposition | `-fsemantic-interposition` | ELF-only flag; stops clang inlining calls to functions the port replaces | The driver drops it for Mach-O, but `-Xclang -fsemantic-interposition` works there (Mach-O definitions are not `dso_local`); `-fno-inline-functions` alone is not enough, IPO still folds or deletes calls. Both are used, and `ps2_interposition_check` disassembles `dc_ps2.o` on every build, Linux included |
| Link flags | `-fuse-ld=lld -Wl,--gc-sections -Wl,--defsym=…` | GNU-style | `-Wl,-dead_strip`; `--defsym` aliases become `-Wl,-alias,_from,_to`; `EditGaijiTbl` (an offset, which `-alias` cannot express) is a port-side table in `linknames.cpp` on both platforms; `--error-limit` dropped |
| Toolchain | clang 20 from apt | | Homebrew `llvm` (20+) and `lld`; Apple clang is not current enough for C++26 |
| Shader build | `glslangValidator` from apt | | Homebrew `glslang` |
| Dependencies | SDL3 and Vulkan from `/usr/local` | | Homebrew `sdl3`, `vulkan-headers`, `vulkan-loader`, `vulkan-validationlayers`; Mesa built from source for KosmicKrisp and lavapipe |
| Floating point | x86-64 SSE, no FMA contraction by default | | arm64 clang contracts `a*b+c` into FMA by default; `ps2/src` must be compiled with `-ffp-contract=off` so game math matches the x86-64 build bit for bit |
| `strings.h` | `port/include/port.h` | glibc | present on macOS; keep |
| CI | `.github/workflows/pc.yml` Ubuntu | | a `macos-26` (Apple Silicon) job: KosmicKrisp needs Metal 4, which `macos-15` lacks |

## 3. The 32-bit pointer problem on arm64 macOS

The game casts pointers to `int` in places (`docs/PC.md`, "Keeping the PS2
build matching"). On Linux the executable is non-PIE, so globals sit below
4 GiB and round-trip, and the arenas are mapped below 2 GiB.

On arm64 macOS neither is possible. XNU's Mach-O loader requires every native
arm64 executable to have a 4 GiB hard `__PAGEZERO` (`bsd/kern/mach_loader.c`,
`enforce_hard_pagezero`, checked from macOS 13 through 15 and `main`): a
binary linked with a smaller `-pagezero_size` is refused at exec, and the
process's address map starts at 4 GiB, so no `mmap`, hinted or fixed, can
ever return a low address. A low-memory arena therefore cannot exist on
arm64 macOS. The rule is keyed on `CPU_TYPE_ARM64`; an x86-64 build run under
Rosetta 2 only needs a 4 KiB page zero.

So the port stops relying on 32-bit round trips altogether:

1. **Audit tool.** `scripts/port/truncations.py` runs libclang over `ps2/src`
   and `port/src` with the port's compile flags and lists every cast of a
   pointer to a 32-bit integer, classified as a **round trip** (cast back to
   a pointer, stored in an `int` field or global later read as a pointer,
   or subtracted against another truncated pointer) or **low bits only**
   (alignment masks, modulo, hashing), with the enclosing function and
   whether that function is already a port replacement. Output committed as
   `docs/port/truncations.md`.
2. **Proof on Linux.** The arenas can be forced above 4 GiB on Linux
   (`platform/memory`'s high path), which reproduces the arm64 macOS
   condition exactly; the test suite and the real-data boot run that way.
3. **Per-site fixes**, all in `port/src`: every round-trip site in a port
   replacement unit or on the boot path is widened to keep a real pointer;
   round-trip sites that remain in retail functions are listed with the
   replacement each needs, and replaced one by one like any hardware
   dependence. Known cases: `BtEventData = (s32) arena`, the `EPARTS_*`
   records, the script VM's `funcdata` (`dataio.md`, section 2.4), the title
   units' `(int)` arithmetic.
4. **Fallback (unnecessary).** An x86-64 build under Rosetta 2, with
   `-pagezero_size 0x1000` and the Linux-style low arena, was the way to run
   on Apple Silicon until the retail list was empty. It is: the audit lists
   no round trip that can run, Linux links PIE and maps its arenas wherever
   `mmap` puts them, and the low arena, `-pagezero_size` and the option for
   it are gone.

No memory mapping of PS2 address ranges is introduced on any platform.

## 4. Work packages

### 4.1 Build system and toolchain (one agent)

- `CMakePresets.json`: `linux-x64` (today's build) and `macos-arm64`
  (Homebrew LLVM, `-DCMAKE_OSX_ARCHITECTURES=arm64`, deployment target the
  Mesa build needs **[verified]**: the executable does not link Metal, so it
  targets 14.0, the oldest macOS with Homebrew bottles; KosmicKrisp needs 26
  at run time; Homebrew prefix for SDL3, Vulkan and glslang).
- `port/CMakeLists.txt` split per platform: the merge-and-weaken step
  (`ld -r` + `llvm-objcopy --weaken` or `tools/weaken`), link flags
  (`-dead_strip`, `-alias` for `draw_rect`, `WorkBuffer__2`,
  `EditGaijiTbl`), `-pagezero_size`, no `-no-pie`, the
  interposition substitute, `-ffp-contract=off` for `ps2/src`.
- `tools/weaken` (C++26, std only): reads a Mach-O object, sets `N_WEAK_DEF`
  on every defined external symbol, writes it back; with a unit test on a
  tiny object produced in the test. Built only when needed, but kept
  portable so the Linux build also compiles it.
- `scripts/host/mesa-macos.sh`: builds a pinned Mesa tag with meson for
  `kosmickrisp` and `swrast`, installs under a prefix, writes the ICD files,
  prints the `VK_DRIVER_FILES` line. Everything pinned.
- `.github/workflows/pc.yml`: a `macos-26` job that installs Homebrew
  dependencies, builds Mesa through the script (cached by Mesa tag), builds
  the port with the preset, runs `ctest` headless on lavapipe, then runs
  `darkcloud --headless --frames 3 --screenshot` on KosmicKrisp with
  `VK_DRIVER_FILES` pointing at it (the runners have a Metal-capable GPU)
  and uploads the screenshot and `vulkaninfo --summary` as artifacts.
- `docs/MACOS.md`: how to build and run on a Mac, what is verified in CI,
  the KosmicKrisp facts from section 1 once verified.

### 4.2 Platform and renderer portability (one agent)

- `platform/paths.cpp`: `_NSGetExecutablePath`; `platform/clock.cpp`,
  `input.cpp`, `audio.cpp`, `window.cpp`: audit for POSIX or Linux calls,
  `SDL_GetWindowSizeInPixels` on Retina (already used), the Metal-backed
  window flags SDL3 needs for Vulkan on macOS.
- `dataset.cpp` arenas: `platform/memory.{hpp,cpp}` with the Linux low
  (`MAP_32BIT`) path, a high path (macOS, and forced on Linux for the proof
  in section 3) and a common zeroing call.
- Renderer: instance creation enables portability enumeration when the
  extension exists; device selection accepts a 1.3 device when every feature
  the renderer uses is present (explicit `VkPhysicalDeviceVulkan13Features`
  and extension checks) and reports exactly which requirement a rejected
  device lacks; `VK_KHR_portability_subset` restrictions handled where they
  touch the renderer (triangle fans are used by `Draw2D`: emulate as lists on
  a device that lacks them). A surface-less mode (`--headless` without
  `VK_EXT_headless_surface`): render to the main target and skip the
  swapchain, with `ReadbackFrame` still working. Depth-format fallback stays.
- Pointer-truncation audit tool and the fixes of section 3.
- Tests: the existing suite must stay green on Linux; new tests for
  `LowMemoryMap` (result below 4 GiB, guard page), the triangle-fan fallback,
  the surface-less mode (on lavapipe, forced by a config flag), and the audit
  tool's output format.

### 4.3 Verification

This environment is Linux x86-64 without a Mac, so the macOS build is
verified by the CI job in 4.1 and by the user's Mac. Everything portable is
tested on Linux first (the surface-less mode, the fan fallback, the low-memory
mapper, `tools/weaken` on a Mach-O object written by its test). The agents
report what they could not run.

## 5. Risks

- KosmicKrisp may lack a feature the renderer needs (dual-source blending or
  8192 update-after-bind images are the likeliest). Fallbacks: the blend
  table's non-dual-source variants (more pipelines, documented approximations
  in `gfx/README.md`), and a smaller texture array with per-frame descriptor
  updates.
- `llvm-objcopy --weaken` on Mach-O may be unsupported; `tools/weaken` is the
  planned fallback and costs a day, not a redesign.
- Pointer truncation sites in retail code reached through still-retail
  functions cannot be fixed without replacing those functions; the audit
  lists them and they are replaced one by one like any hardware dependence.
- Pipeline precompilation on Metal may take tens of seconds on first run;
  the progress callback and the cache make this a one-time cost.
