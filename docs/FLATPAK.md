# Flatpak

The Linux release of the PC port (`docs/PC.md`) is a Flatpak,
`org.themoonpeople.Chronicle`. It holds `darkcloud`, `dcdata` and SDL3 on the
freedesktop 26.08 runtime. No game data is in it: the first start asks for
your own PAL disc and extracts it.

## Installing the bundle

CI (`.github/workflows/flatpak.yml`) uploads `chronicle.flatpak` with every
build. The runtime comes from Flathub:

```sh
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user chronicle.flatpak
flatpak run org.themoonpeople.Chronicle
```

It also appears in the desktop's application list as Chronicle.

## The first start

With no game data, the game opens a window that asks for it and, straight
away, the system's file selector for a `.iso`, `.bin` or `.img` of the PAL
disc. The window's **Disc image** button (Enter) opens it again; **Folder with
DATA.DAT and DATA.HD2** (F) takes a folder copied from the disc instead. Inside the Flatpak the selector is the desktop's
file-chooser portal, so the game reads the chosen file without any
permission to your files. The game then extracts every file the disc's index
lists, showing the file count and megabytes, checks each one and continues
into the game. Extraction takes a few minutes and needs as much free space
as the disc's `DATA.DAT`.

Cancelling the file selector returns to the prompt, to try again or pick a
folder. Closing the window or pressing Escape during extraction stops it; the next
start picks up where it stopped. Quitting the prompt instead exits with
status 3 and prints the `dcdata` command below. A failed extraction (an image
of another disc, a truncated image, a full disk) shows the extractor's
message and exits with status 1, leaving nothing the game would mistake for
complete data.

Where no file selector is available (no portal, and no zenity outside a
sandbox), the prompt says so on stderr and exits with status 3.

### From a shell

`dcdata` does the same from a script. It needs read access to the folder
holding the image, granted for that one command:

```sh
flatpak run --filesystem="$HOME/Games:ro" --command=dcdata org.themoonpeople.Chronicle \
    extract "$HOME/Games/Dark Cloud (PAL).iso" ~/.var/app/org.themoonpeople.Chronicle/data/chronicle/data
```

## Where things live

Inside the sandbox `XDG_DATA_HOME` is `~/.var/app/org.themoonpeople.Chronicle/data`,
so:

| | |
|---|---|
| game data | `~/.var/app/org.themoonpeople.Chronicle/data/chronicle/data` |
| saves, `config.json`, the pipeline cache | `~/.var/app/org.themoonpeople.Chronicle/data/chronicle/save` |

`config.json` is described in `docs/PC.md`. `flatpak uninstall --delete-data
org.themoonpeople.Chronicle` removes both. The usual options still apply:
`flatpak run org.themoonpeople.Chronicle --width 1920 --height 1080`.

## Controls

Keyboard, mouse and gamepads as in `docs/PC.md`, "Keyboard and mouse".

## Permissions

| finish-arg | why |
|---|---|
| `--socket=wayland` | the window, on Wayland |
| `--socket=fallback-x11` | the window on an X11 session; ignored when Wayland is there |
| `--share=ipc` | X11 shared memory, without which fallback-x11 is slow or broken |
| `--device=dri` | the GPU, for Vulkan |
| `--socket=pulseaudio` | sound, through PipeWire's or PulseAudio's PulseAudio socket |
| `--device=all` | gamepads: `/dev/input`, and `/dev/hidraw*` for those SDL reads through HIDAPI `--device=input` covers only `/dev/input` |

There is no network and no filesystem access. The disc reaches the game
through the file-chooser portal, which every Flatpak may call without a
`--talk-name`.

## Building

```sh
port/flatpak.sh
```

builds `chronicle.flatpak` in the repository root from the working tree,
installing the runtime, the SDK and the llvm22 extension from Flathub when
they are missing. `INSTALL=1` also installs the bundle for this user;
`CLEAN=1` discards flatpak-builder's cache first. By hand, the same is:

```sh
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.freedesktop.Platform//26.08 org.freedesktop.Sdk//26.08 \
    org.freedesktop.Sdk.Extension.llvm22//26.08
flatpak-builder --repo=repo build-dir port/flatpak/org.themoonpeople.Chronicle.yml
flatpak build-bundle repo chronicle.flatpak org.themoonpeople.Chronicle
```

(`flatpak-builder --install-deps-from=flathub ...` installs the three for
you.) Run from the repository root; `build-dir/`, `repo/`, `.flatpak-builder/`
and `chronicle.flatpak` are ignored by git. `flatpak-builder --user --install
build-dir port/flatpak/org.themoonpeople.Chronicle.yml` installs the build
directly instead of through a bundle.

The manifest (`port/flatpak/org.themoonpeople.Chronicle.yml`) builds:

- **Vulkan-Headers 1.4.365**, for the build only: the release `pc.yml` builds
  against, a little ahead of the SDK's 1.4.357. The loader is the runtime's.
- **SDL3 3.4.18**, newer than the runtime's 3.4.14 and ahead of it on the
  app's library path, with its Wayland, X11, PipeWire, PulseAudio and D-Bus
  (portal) backends loaded from the runtime.
- **The port**, `PLATFORM=PC`, Release, with clang 22 from the
  `org.freedesktop.Sdk.Extension.llvm22` extension (the SDK's own compiler is
  GCC, and the port is built with clang everywhere). Shaders are compiled with
  the SDK's glslang, which targets `vulkan1.4`. The SDK's default compiler flags are
  replaced: they add `_GLIBCXX_ASSERTIONS`, `-Werror=format-security` and
  `-g`, none of which the game is built with elsewhere.

The app module's source is the working tree (`type: dir`); CI swaps it for a
`git` source at the commit under test. Vulkan validation layers are not
shipped. The bundle carries no Vulkan driver of its own: the runtime's Mesa
(`org.freedesktop.Platform.GL.default`) or the host's NVIDIA driver extension
provides it.
