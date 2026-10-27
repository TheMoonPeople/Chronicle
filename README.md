<div align="center">

<img src="https://i.imgur.com/pCMkgRM.png" alt="Chronicle" width="700">

**A decompilation project and modern port of Dark Cloud for the PlayStation 2.**

[![Documentation](https://img.shields.io/badge/Documentation-2563EB?style=for-the-badge&logo=readthedocs&logoColor=white)](https://themoonpeople.github.io/Chronicle/)
[![Discord](https://img.shields.io/badge/Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/RGuE998sDY)

[![NTSC Progress](https://decomp.dev/TheMoonPeople/Chronicle/ntsc.svg?mode=shield&label=NTSC&measure=matched_code_percent)](https://decomp.dev/TheMoonPeople/Chronicle/ntsc)
[![PAL Progress](https://decomp.dev/TheMoonPeople/Chronicle/pal.svg?mode=shield&label=PAL&measure=matched_code_percent)](https://decomp.dev/TheMoonPeople/Chronicle/pal)

[<img src="https://decomp.dev/TheMoonPeople/Chronicle/ntsc.svg?w=512&h=256" width="512" alt="Decompilation progress">](https://decomp.dev/TheMoonPeople/Chronicle/)

</div>

## Overview

**Chronicle** is a decompilation project and port of _Dark Cloud_ for the PlayStation 2.

The project aims to produce **100% matching text and data sections** for:

- The main executable of the **NTSC 1.02** release.
- The main executable of the **PAL review** version.
- The `TITLE.BIN` and `DUN.BIN` overlays.

## Building and Running

### Prerequisites

- A copy of _Dark Cloud_ (ideally the PAL version).
- Git, to clone the repository and its submodule.
- **Windows:** nothing else; the first build downloads its toolchain.
- **Linux:** Docker or Podman (the build runs in Kazarov's container), Python 3, and to run the
  game SDL3 3.4 and a Vulkan 1.3 driver with its loader.
- **macOS:** Xcode's command line tools and the Homebrew packages in
  [`docs/MACOS.md`](docs/MACOS.md).
- **Nintendo Switch:** Docker on Linux or in WSL (see [`docs/SWITCH.md`](docs/SWITCH.md)).

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

The first Windows build downloads the pinned toolchain into `.kazarov/windows`, including
llvm-mingw, Rust and an LGPL FFmpeg; see [`docs/WINDOWS.md`](docs/WINDOWS.md).

### 4. Run the project

```bash
./run.sh <configuration> [arguments]
```

Native runs through `run.bat` or `run.sh` keep runtime files in the ignored
`port/run/` directory: extracted data in `data/` or packed data in `data.zip`, saves and settings in `save/`,
and soundtrack files, icons and relative screenshot output alongside them.
Relative input and output arguments resolve from `port/run/`; use absolute paths
for files elsewhere. Build output stays in `port/build/`.

### Build Configurations

| Configuration / task  | Platform        | Build                               |
| :-------------------- | :-------------- | :---------------------------------- |
| `ps2-pal`             | PlayStation 2   | PAL executable and disc image       |
| `ps2-ntsc`            | PlayStation 2   | NTSC 1.02 executable and disc image |
| `linux-x64-release`   | Linux x64       | Release                             |
| `linux-x64`           | Linux x64       | Debug                               |
| `macos-arm64-release` | macOS ARM64     | Release                             |
| `macos-arm64`         | macOS ARM64     | Debug                               |
| `windows-x64-release` | Windows x64     | Release                             |
| `windows-x64`         | Windows x64     | Debug                               |
| `switch`              | Nintendo Switch | Homebrew `.nro` (see [`docs/SWITCH.md`](docs/SWITCH.md)) |
| `assembleDebug`       | Android         | Debug APK (experimental)            |
| `assembleProfile`     | Android         | Profile APK (experimental)          |

Run Android Gradle tasks from `port/android` with `gradlew.bat <task>` on
Windows; see the [Android build
documentation](https://github.com/TheMoonPeople/Chronicle/blob/master/port/android/README.md).

### Build Options

The root build scripts accept these environment variables:

| Variable  | Description                                                           |
| :-------- | :-------------------------------------------------------------------- |
| `CLEAN=1` | Removes the selected configuration's build directory before building. |
| `JOBS=N`  | Sets the number of parallel build jobs.                               |

For example:

```bash
CLEAN=1 JOBS=8 ./build.sh linux-x64-release
```
