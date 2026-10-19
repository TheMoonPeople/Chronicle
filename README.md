<div align="center">

<img src="https://i.imgur.com/pCMkgRM.png" alt="Chronicle" width="700">

**A decompilation project and modern port of Dark Cloud for the PlayStation 2.**

[![Documentation](https://img.shields.io/badge/Documentation-2563EB?style=for-the-badge&logo=readthedocs&logoColor=white)](https://themoonpeople.github.io/Chronicle/)
[![Discord](https://img.shields.io/badge/Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/RGuE998sDY)

[![NTSC Progress](https://decomp.dev/TheMoonPeople/Chronicle/ntsc.svg?mode=shield&label=NTSC&measure=matched_code_percent)](https://decomp.dev/TheMoonPeople/Chronicle/ntsc)
[![PAL Progress](https://decomp.dev/TheMoonPeople/Chronicle/pal.svg?mode=shield&label=PAL&measure=matched_code_percent)](https://decomp.dev/TheMoonPeople/Chronicle/pal)

[<img src="https://decomp.dev/TheMoonPeople/Chronicle/ntsc.svg?w=512&h=256" width="512" alt="Decompilation progress">](https://decomp.dev/TheMoonPeople/Chronicle/)

</div>

> **This fork adds a Nintendo Switch build.** The PC port runs as homebrew on a Switch with custom
> firmware, handheld at 1280x720 and at 60 frames a second in towns, with saves imported from PS2
> memory cards. See [Nintendo Switch](#nintendo-switch) below and [`docs/SWITCH.md`](docs/SWITCH.md).

## Overview

**Chronicle** is a decompilation project and port of _Dark Cloud_ for the PlayStation 2.

The project aims to produce **100% matching text and data sections** for:

- The main executable of the **NTSC 1.02** release.
- The main executable of the **PAL review** version.
- The `TITLE.BIN` and `DUN.BIN` overlays.

## Building and Running

### Prerequisites

- A copy of _Dark Cloud_ (ideally the PAL version).

### 1. Clone the repository

```bash
git clone --recurse-submodules https://github.com/TheMoonPeople/Chronicle.git
cd Chronicle
```

### 2. Provide a disc image

Place your disc image in the corresponding region directory:

| Region    | Path                             |
| :-------- | :------------------------------- |
| PAL       | `rom/pal/Dark Cloud (PAL).iso`   |
| NTSC 1.02 | `rom/ntsc/Dark Cloud (NTSC).iso` |

### 3. Build the project

**Linux / macOS**

```bash
./build.sh <configuration>
```

**Windows**

```batch
build.bat <configuration>
```

### 4. Run the project

```bash
./run.sh <configuration> [arguments]
```

### Build Configurations

| Configuration         | Platform      | Build                               |
| :-------------------- | :------------ | :---------------------------------- |
| `ps2-pal`             | PlayStation 2 | PAL executable and disc image       |
| `ps2-ntsc`            | PlayStation 2 | NTSC 1.02 executable and disc image |
| `linux-x64-release`   | Linux x64     | Release                             |
| `linux-x64`           | Linux x64     | Debug                               |
| `macos-arm64-release` | macOS ARM64   | Release                             |
| `macos-arm64`         | macOS ARM64   | Debug                               |
| `windows-x64-release` | Windows x64   | Release                             |
| `windows-x64`         | Windows x64   | Debug                               |

### Build Options

The following environment variables can be used to customise the build:

| Variable  | Description                                                           |
| :-------- | :-------------------------------------------------------------------- |
| `CLEAN=1` | Removes the selected configuration's build directory before building. |
| `JOBS=N`  | Sets the number of parallel build jobs.                               |

For example:

```bash
CLEAN=1 JOBS=8 ./build.sh linux-x64-release
```

## Nintendo Switch

The port also builds as a homebrew `.nro` for a Switch running Atmosphère, in a container image
(podman or docker) with devkitPro's toolchain, Mesa's NVK Vulkan driver and SDL3.

```bash
./build.sh linux-x64-release                                       # the PC build, for dcdata
port/build/pc-release/dcdata extract "rom/pal/Dark Cloud (PAL).iso" data
tools/switch/build.sh                                              # port/build/switch/darkcloud.nro
```

Copy `darkcloud.nro`, `lang/` and `glyphs/` from `port/build/switch/`, and the extracted `data/`, to
`sdmc:/switch/chronicle/`, then start **Chronicle** from the Homebrew Menu with a game's title
overridden (hold R while starting a game). Saves from PCSX2 or a PS2 memory card dump import with
`tools/ps2card/ps2card.py`.

[`docs/SWITCH.md`](docs/SWITCH.md) covers building, installing, playing, saves, logs and crash
reports, testing from a computer, how the port runs on the console and what it changes for it.
