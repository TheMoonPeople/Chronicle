# Chronicle

[![NTSC Progress]](https://decomp.dev/TheMoonPeople/Chronicle/ntsc)
[![PAL Progress]](https://decomp.dev/TheMoonPeople/Chronicle/pal)

[NTSC Progress]: https://decomp.dev/TheMoonPeople/Chronicle/ntsc.svg?mode=shield&label=NTSC&measure=matched_code_percent
[PAL Progress]: https://decomp.dev/TheMoonPeople/Chronicle/pal.svg?mode=shield&label=PAL&measure=matched_code_percent
[progress_link]: https://decomp.dev/TheMoonPeople/Chronicle/

[<img src="https://decomp.dev/TheMoonPeople/Chronicle/ntsc.svg?w=512&h=256" width="512" height="256" alt="A visual">][progress_link]

Chronicle is a decompilation project and port of Dark Cloud for the PlayStation 2.

This project produces 100% matching text and data sections for the main executable for the NTSC 1.02 and PAL review versions, and 100% matching `TITLE.BIN` and `DUN.BIN` overlays. Matching the main executable's debug symbols may be explored in the future.

# Building and running

1. Clone the repository with `git clone --recurse-submodules https://github.com/TheMoonPeople/Chronicle.git`.
   The build system is Kazarov, in `tools/kazarov`; the first build fetches it if it is missing.
2. Place your disc image in `rom/<region>/`: `rom/pal/Dark Cloud (PAL).iso` for the PAL release, or
   `rom/ntsc/Dark Cloud (NTSC).iso` for NTSC 1.02. An image at `rom/Dark Cloud (<REGION>).iso` is also accepted.
3. Run `./build.sh <configuration>`, or `build.bat <configuration>` on Windows.

`./run.sh <configuration> [arguments]` builds the same way and then launches the result. PS2
configurations boot the disc image in PCSX2; port configurations start the game, with the arguments
passed on to it.

| Configuration | Builds |
|---|---|
| `ps2-pal` (`ps2`, `pal`) | the PAL PS2 executable and disc image |
| `ps2-ntsc` (`ntsc`) | the NTSC 1.02 PS2 executable and disc image |
| `linux-x64-release` (`linux`) | the optimized Linux port, in `port/build/pc-release` |
| `linux-x64` (`linux-x64-debug`) | the Debug Linux port, in `port/build/pc` |
| `macos-arm64-release` (`macos`) | the optimized Apple Silicon port, in `port/build/macos-arm64-release` |
| `macos-arm64` | the Debug Apple Silicon port, in `port/build/macos-arm64` |
| `windows-x64-release` (`windows`) | the optimized Windows port |
| `windows-x64` | the Debug Windows port |

With no configuration, the scripts use the port for the host's operating system. PS2 builds must name
their configuration. The Linux and PS2 configurations build in a container, which needs Docker or Podman;
the macOS and Windows ports build on the host, with the dependencies in `docs/MACOS.md` and `docs/WINDOWS.md` installed.

`CLEAN=1` discards the configuration's build directory before building, and `JOBS=N` sets the number of
parallel jobs. The configurations and their settings are listed in `kazarov.conf` and `tools/kazarov/README.md`.

## The PC port

`PLATFORM=PC` builds the game's code as a native Linux, macOS and Windows program on SDL3 and Vulkan
(`docs/PC.md`, `docs/MACOS.md`). On Linux it is packaged as a Flatpak, built by CI as
`chronicle.flatpak`; `docs/FLATPAK.md` covers installing it. It needs your own PAL disc: the first
start asks for the disc image and extracts the game's files from it.

### Translations and the message font

Game text can come from JSON language files (one per language, laid out as Minecraft's are) and be drawn from a
TrueType font so it stays sharp at any window size. The first start writes the language files from your own
game data; the Dark Cloud community font is bundled. See `docs/LOCALIZATION.md`.

### The Demon Shaft georama mod

The port builds a mod that gives the Demon Shaft a georama of its own, built from another town's
models: atla on its floors, and a town reached with Triangle on the Demon Shaft on the world map.
`DSG=1 ./build.sh ps2-pal` builds it into the PS2 executable, which then no longer matches retail.
See `docs/DEMON_SHAFT_GEORAMA.md`.

## Repository layout

- `ps2/src`, `ps2/include`: matching game sources and headers.
- `ps2/config`, `ps2/asm`: per-region split configuration and generated reference assembly.
- `ps2/CMakeLists.txt`, `ps2/cmake`: PS2 build rules.
- `port/src`, `port/include`: native replacements and platform support.
- `port/CMakeLists.txt`, `port/flatpak`: native build rules and Linux packaging.
- `scripts`: build, port and tooling scripts.
- `tools`: compilers, the Kazarov build system (`tools/kazarov`) and helpers.
- `rom`: disc images and their checksums.

## Documentation

Source documentation is available on [GitHub Pages](https://themoonpeople.github.io/Chronicle/).
