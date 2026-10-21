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

Native runs through `run.bat` or `run.sh` keep runtime files in the ignored
`port/run/` directory: extracted data in `data/`, saves and settings in `save/`,
and soundtrack files, icons and relative screenshot output alongside them.
Relative input and output arguments resolve from `port/run/`; use absolute paths
for files elsewhere. Build output stays in `port/build/`.

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

## Android build

This fork builds Chronicle as a native Android app for `arm64-v8a` and `x86_64`, named Chronicle. It
includes a multi-touch PS2 control overlay with separate movement and camera sticks; unused L3/R3 buttons
are omitted, touches outside the controls do not send game input, and the overlay hides when a hardware
controller, keyboard, or mouse is connected. Android settings, controller bindings, Foldable display
changes, 16 KB page-size compatibility, and renderer/performance work are included. The renderer requires
Vulkan 1.3 and a compatible GPU. Android support remains experimental and device compatibility varies.

The app does not include Dark Cloud game data. On first launch, select your own PAL disc image; the app
imports it into private storage. Keep disc images and extracted `DATA.DAT`/`DATA.HD2` files out of this
repository and APK.

### Build on Windows

Install JDK 17 and Android Studio's SDK platform 35, build tools 35.0.0, NDK 27.0.12077973, and CMake
3.31.6. Clone this fork with its Kazarov submodule:

```powershell
git clone --recurse-submodules https://github.com/anxiousintrovert/Chronicle-Android.git
cd Chronicle-Android\port\android
.\fetch-deps.ps1
.\gradlew.bat assembleDebug
```

The Gradle wrapper pins Gradle 8.13. `fetch-deps.ps1` downloads SDL 3.4.16 and glslang 16.6.0, checking
their archive SHA-256 values before extraction. The debug APK is written to
`port/android/app/build/outputs/apk/debug/app-debug.apk`.

For local device testing, `port/android/publish-test-apk.ps1` builds and zips a timestamp-labeled package,
archives previous Chronicle test packages in `DARKCLOUDBUILDS`, and leaves the current ZIP in the chosen
destination root. Its default destination is the maintainer's mapped `Z:\` Games share; pass
`-DestinationRoot` to use another folder.

See [`port/android/README.md`](port/android/README.md) for prerequisites, setup, and build details. Prebuilt test
packages, when available, are published on the [GitHub Releases page](https://github.com/anxiousintrovert/Chronicle-Android/releases).
They contain the app only; users must supply their own compatible PAL disc image.
