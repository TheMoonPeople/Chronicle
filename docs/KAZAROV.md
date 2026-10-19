# Official Kazarov dependency

Chronicle uses the unmodified `TheMoonPeople/Kazarov` submodule, pinned to
`04bd301efd8486c033caf46faf52f179a6022d7c`. This replaces the Android compatibility
fork selected by PR #188. The official history includes logical texture sizes
(`a78762c`), per-frame skinned mesh poses (`43cc847`) and Android support.

Existing checkouts must synchronize the changed submodule URL before updating:

```sh
git submodule sync -- tools/kazarov
git submodule update --init tools/kazarov
```

The renderer, mixer, platform code and model loader remain upstream code. Chronicle
owns its game-specific format adapters:

- `port/src/audio_retail*` reads the retail HD/BD sound banks and SQ sequences.
  It converts their tables and events to the official JSON inputs and supplies
  decoded sample WAVs through temporary memory-backed VFS entries. The resulting
  upstream banks and sequences own their data.
- `port/src/icon_retail*` reads the memory-card icon from existing language packs
  and converts its mesh and texture to an in-memory GLB for the official icon
  renderer. Existing extracted game data does not need to be replaced.
- `port/src/platform/touch_overlay.cpp` keeps Android JNI input in a mutex-protected
  mailbox. The game thread supplies it to `InputSetTouchOverlay` after `InputPoll`
  determines visibility. The existing Java controls and opacity setting remain.
- `port/src/platform/png_image*` preserves Chronicle UI/localization support for
  RGB, palette and grayscale PNGs using the upstream decoder. Kazarov's texture
  catalog has a narrower RGBA8 file contract; its rules remain unchanged.

No converted game assets are written to the data directory or distributed with
the source. The retained retail readers carry their upstream MIT attribution in
`port/third_party/kazarov-retail-LICENSE`. Flatpak stages the pinned fastgltf and
simdjson archives before entering its network-disabled build.

The `RetailAudio.*` and `RetailIcon.*` tests exercise conversion, malformed inputs,
sample loops/pitch, sequence events and rendering. Set `DC_DATA` to an owned
extracted game directory to also check its banks, sequences, samples and icon.
These checks do not establish audible equivalence, phone performance or a fix
for a particular reported visual defect; those require runtime validation.
