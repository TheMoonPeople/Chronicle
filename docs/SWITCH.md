# Nintendo Switch

Chronicle can be built as a homebrew `.nro` for a Switch running custom firmware. It uses devkitPro's devkitA64 and libnx, SDL3 with the Switch Vulkan surface, and Mesa NVK for Vulkan 1.3. Supply your own Dark Cloud data extracted from a disc; game data stays outside Git.

## Build

The Switch toolchain image is built with Docker and contains pinned devkitPro, Mesa/NVK, SDL3, glslang and Rust tooling. A Linux host or WSL with Docker is required.

```sh
./build.sh linux-x64-release
port/build/pc-release/dcdata extract "rom/pal/Dark Cloud (PAL).iso" data
port/build/pc-release/dcdata pack data
./build.sh switch
```

On Windows, run `build.bat switch` from a WSL-enabled system with Docker available to WSL. The output is `port/build/switch/darkcloud.nro`. The first container image build takes a while because it builds Mesa's NVK driver and SDL3. `BUILDER` selects an OCI-compatible container runtime; Docker is used by default. Run `port/switch/build.sh shell` to open the build image.

The first launch compiles Chronicle's Vulkan pipelines with NVK during startup and saves an application pipeline cache in `save/pipeline_cache.bin`. Later launches reuse compatible entries; a missing, incompatible or incomplete cache is rebuilt. Compilation may lengthen the first startup, and internal NVK shaders may compile when their features are first used.

## Install

Copy these files to `sdmc:/switch/chronicle/`:

- `darkcloud.nro` from `port/build/switch/`
- `lang/` and `glyphs/` from `port/build/switch/`
- `data.zip` from the packing step

`data.zip` contains every extracted file, including the normalized copies and language list. It is a standard stored ZIP and stays outside Git. An existing extracted `data/` directory also works; when both are installed, `data.zip` takes precedence. `--data` still names the logical `data/` path beside the archive.

Start Chronicle from the Homebrew Menu with a game's title override by holding R while starting a game. Launching from the album applet leaves too little memory for the game. `save/` stores settings, saves and the Vulkan pipeline cache created at startup.

## Logs

Sent with `nxlink -s`, stdout and stderr go back to the sending computer and no log file is written. Otherwise every line also goes to the system log manager (the `lm` service), which Atmosphère can retain. Started from the Homebrew Menu, the game also writes `darkcloud.log` beside the `.nro`. Each start replaces the previous file, and a run writes at most 4 MiB to it, then notes that it stopped. A low-priority thread writes the file because a single SD card write can take 10 to 25 ms on hardware, and the game thread would otherwise stall on every log line. An installed application writes no log file and removes one an earlier run left in its directory. Saves, settings and the pipeline cache are never touched by log handling.

The Homebrew Menu does not pass arguments to the application. Put whitespace-separated arguments in `args.txt` beside the `.nro`; supported options are listed in [PC port documentation](PC.md).

With asynchronous rendering, Chronicle places the game and render threads on separate cores from the process's allowed CPU mask and logs their actual masks and priorities. Add `--switch-core-affinity auto` to `args.txt` to use libnx's normal scheduling for comparison. This setting has no effect when the render thread is off or the process has fewer than two available cores.

## Platform support

The Switch build supplies guarded arena memory, cooperative context switching, compatibility flags for newlib and logging through nxlink, `lm` and a capped log file (see "Logs" above). Its Vulkan driver is [Mesa NVK from Dan's Switch port](https://github.com/danfromtico/mesa-switch), pinned to commit `0ff52079ca9e9e837ed27eb95a8d239f439a1864` (Mesa 26.2.4) and built as a loaderless static library with a direct libnx Horizon backend. The WSI presents scanout-backed images with the GPU completion fence, avoiding a CPU copy on the present path. The output target is 1280 × 720 at 60 Hz in handheld mode and 1920 × 1080 at 60 Hz when docked. The game fills those 16:9 targets, cropping the 4:3 image vertically as needed. Console tests have sustained 60 Hz in the title and selected populated Norune, Queens and Muska Lacka views at stock handheld clocks and full handheld render scale. Those fixtures do not qualify the entire game or docked performance.

To send an existing build through the Homebrew Menu's netloader, use `port/switch/run.sh`. Set `SWITCH_ADDRESS` when the console cannot be discovered by broadcast.

## Display

Switch selects its rendering scale from the current console mode: 1.5 for handheld and 2.25 for docked, relative to the game's 480-pixel logical height. Docking and undocking update the scale and output size. The Switch build does not offer resolution or render-scale controls; saved desktop settings and `args.txt` cannot override its output target. Motion interpolation is disabled, while the game's 60 Hz tick rate remains unchanged.

Switch enables display-list arenas, direct shadow clipping, proven-opaque alpha-test
draws, safe near-fog mesh variants, reciprocal near-fog depth, combined depth planes
and inline render-thread mesh dispatch. Each preserves the general fallback for
unsupported geometry or state. Heat haze begins farther from the player, skips
identity copies and samples a half-resolution image with linear filtering. The
renderer keeps normal ordered blending for 2D draws.

For A/B comparisons, `--env NAME=VALUE` in `args.txt` overrides these defaults:
`KZ_LIST_ARENA`, `DC_SHADOW_CLIP_DIRECT`, `KZ_ALPHA_OPAQUE`, `KZ_FOG_NEAR_SAFE`,
`KZ_DEPTH_PLANES`, `DC_HAZE_SKIP_IDENTITY`, `KZ_FOG_NEAR_RECIPROCAL` and
`KZ_INLINE_MESH_DISPATCH` are `1`; `DC_HAZE_SCALE` is `0.5`. Set the corresponding
boolean to `0`, or the haze scale to `1`, for its baseline. These switches are
diagnostic controls, not lower-quality display presets.

The archive and source-metadata improvements described in [PC](PC.md) also apply
on Switch. In a populated Norune loading fixture, archive indexing fell from about
3.55 s to 0.18 s, and the measured archive/source-read span fell from 8.2 s to 1.4 s.
That span excludes pipeline compilation and is not total startup time.
Untextured draws share the pipeline variants whose texture-alpha settings cannot
affect them, reducing startup creation from 16,851 to 13,107 pipelines. Compatible
warm caches are retained when all startup pipelines report application-cache hits;
missing feedback takes the conservative persistence path. One warm console check
reduced this stage from 1.68 s to 0.71 s, avoiding a measured 983 ms cache write.
Homebrew Menu launch-to-playable, normal UI loading and all area transitions still
require separate measurement.

## Emulator testing

Use Eden for Switch runtime testing, loading the freshly built `port/build/switch/darkcloud.nro` rather than an older copy in its game list. For the missing-data check, leave its emulated SD card without game data, verify the installation instructions and `Press + to exit.` appear, then press the controller's mapped + button and verify that Eden returns to its game list. This check passed in Eden v0.2.1 on 2026-10-23. The keyboard binding was M; a temporary toggle binding allowed automated input to span a polling frame and was restored after the test.

All Switch builds try NVK's normal 4 GiB contiguous shader address reservation first and retry at 1 GiB only if the first reservation runs out of device address space. This allows the renderer to start under Eden's smaller virtual-address allocator without lowering the reservation on hardware where 4 GiB succeeds.

The Mesa 26.2.4 NVK migration built a fresh NRO on 2026-10-23, but a bounded launch through Eden nightly `de7e6e01d4`'s `eden-cli.exe` exited with Windows status `0xC0000409` before writing stdout or stderr or producing a render capture. The attempt used an isolated temporary config and did not validate runtime rendering.

The fresh Mesa 26.2.4 NRO was also tested with Ryubing Canary 1.3.351 (Windows x64 package SHA-256 `FD3C0D247A664C1FB670D559E32A5A3AEBB8ADED7A0C33FB95F369F84D829EB0`; package: [Ryubing Canary 1.3.351](https://starforgejo.dev/archive/-/packages/generic/ryubing/canary-1.3.351)). The PAL ISO was extracted with `dcdata extract "rom/pal/Dark Cloud (PAL).iso" "<isolated-trial>/ryubing-data/sdcard/data"`, and Ryubing loaded the NRO with the extracted data. A later rendered-menu check showed the correctly colored language selection screen and confirmed the ZIP-backed texture provenance. This validates asset loading and the menu render path only; no gameplay, Temple scene, or frame-rate result was established. Earlier in the same emulator investigation, Chronicle logged `gfx: vkCreateDevice failed (-2)` and Ryubing logged a fixed-buffer mapping error before rendering. That failed run remains diagnostic evidence for that build and configuration, not a current gameplay result. Emulator config, SD data and logs stayed under isolated trial directories.

## Performance measurement

Use the same PAL data, isolated save root, scripted inputs, scene and camera path
for comparisons. Keep console mode, clocks, output resolution and render scale
fixed. `--perf-log` works on every platform and reports CPU work, render-thread
cost, Vulkan GPU timestamps, present waits and late ticks once a second.
`DC_PERF_LOAD=1` adds archive/source-read timings; `KZ_PERF_INIT_TRACE=1` reports
pipeline-cache acceptance. These timings describe separate stages, not total
launch-to-playable latency. Use `nxlink -s` to collect console output.

`args.txt` accepts `--env NAME=VALUE`. Desktop runs use environment variables.
The offline `port/switch/perf-log-audit.ps1` script summarizes saved frame reports:

```powershell
port/switch/perf-log-audit.ps1 -AuditLogPath run.log -AuditWindowCount 5
```

At stock handheld clocks, the 2026-10-26 scripted fixtures sustained about 60 Hz
with zero selected late ticks at 1280 × 720:

| View | Canonical GPU mean |
|---|---:|
| Title | 13.86 ms |
| Populated Norune | 12.41 ms |
| Muska Lacka, village | 12.16 ms |
| Muska Lacka, oasis | 13.72 ms |
| Queens, shell | 11.63 ms |
| Queens, fountain | 11.97 ms |
| Queens, palace | 10.85 ms |

These results cover specific views, not every area or docked gameplay. Compare
canonical images after rendering changes, excluding live timing text. Loading
comparisons need both empty and compatible pipeline caches: cached startup and
first launch have different costs. Internal driver pipelines can also compile
on first use. Keep loose data and packed data comparisons separate; `dcdata pack`
currently writes stored ZIP entries.

See [Loading and character switching](LOADING.md) for asset-work optimizations,
measurement controls, fixture limits and pending platform qualification.
