# PC port plan

This is the working plan for the native PC port of Dark Cloud (PAL). It
extends `docs/PC.md`, which describes the mechanism the port is built on and
stays the reference for that mechanism. Everything below is the target
architecture and the order of work. Keep it current: when a phase lands,
update its status here and move the architectural facts into `docs/PC.md`.

## 1. Goals and hard rules

- **C++26, SDL3, Vulkan 1.4, clang 20 or newer, lld.** The port lives in
  `port/src` and `port/include`. `ps2/src` and `ps2/include` are the PAL
  game as the PS2 build compiles it and are never edited for the port beyond
  the rules in `docs/PC.md`.
- **`PC_OVERRIDE` replacement is the only mechanism.** Any function in
  `ps2/src` that depends on PS2 hardware (GS, VU0, VU1, VIF, GIF, DMA, SPU2,
  IOP, scratchpad, timers, memory card, CD) is replaced by a definition in
  `port/src`, tagged `PC_OVERRIDE` and written from scratch. A `static`
  function cannot be replaced, so its non-static callers are replaced instead.
- **No emulation of the PS2.** No GS register interpreter, no VU1
  interpreter, no VIF or GIF packet decoder, no DMA chain walker, no SPU2
  register model, no memory mapping of PS2 address ranges. The libpkt,
  libdma and libgraph stubs stay as `PS2_UNIMPLEMENTED()` so any path the
  port has not replaced aborts loudly. Code that needs GS semantics (blend
  equation, alpha test, fog formula, texture formats, 12.4 coordinates)
  reimplements the semantics natively; https://github.com/pcsx2/pcsx2 may be
  read to understand the hardware but nothing is copied from it.
- **No fixed resolution or frame rate.** The window is resizable, 3D renders
  at window resolution, 2D is laid out in the game's 640x480 logical space
  and mapped to the window with aspect-correct letterboxing. The game's
  logic tick rate is a runtime setting (default 50 Hz, the PAL field rate),
  never a compile-time constant in port code, and presentation is not tied
  to it.
- **No memory overlays.** `TITLE.BIN` and `DUN.BIN` are linked in
  permanently; `LoadOverlay` does nothing.
- **Game data is a plain directory.** `tools/dcdata` extracts the disc's
  `DATA.DAT` into `data/` (gitignored). The game reads it with
  `std::filesystem` and `std::ifstream`. Saves go to `save/`.
- **Asset conversion is a last resort.** Every format the game ships (MDS,
  MDT, TIM2, IMG, packs, HD/BD/SQ, scripts) is consumed natively at load.
  A pre-processing step is added only if a format cannot be consumed without
  emulating hardware, and then it lives in `tools/dcdata`.
- **Comments.** Only to explain why a non-obvious engineering decision,
  constraint or workaround exists. Never what the code does.

## 2. Where the port stands

Waves 1 to 3 have landed (section 5), and shadow volumes after them.
`darkcloud` runs the game's main loop and no stub is reachable from `main`:
with the PAL data it boots to the language select, and with input reaches
the attract movie, which faults on a title-overlay layout (`docs/PC.md`,
"How far the game runs" and "Known gaps"). The four subsystem surveys that back this plan are
summarised in section 3. Key facts:

- The game has no threads, semaphores, interrupt handlers or alarms. The one
  asynchronous thing is the GS VSync callback, which drives the loading
  screen, play-time counting, background CD reads and every spin-wait.
- Skinning, morphing and material animation run on the CPU and rewrite MDT
  arrays in place; VU1 only transforms, lights, fogs, clips and emits. MDT
  files are plain float4 vertex/normal/uv/colour arrays with strip indices.
- Textures are TIM2 (8-bit palettised, 24-bit, 32-bit) inside IMG packs.
  The game moves VRAM rectangles around for texture animation, face changes,
  frame grabs and render-to-texture, all through `MG*` calls and a few
  hand-written units.
- All music and sound effects are sequenced: Sony HD/BD banks (VAG ADPCM
  samples, program/split tables) and SQ sequences, played by the IOP's
  MODHSYN synth through a custom EZMIDI RPC. There is no streaming audio and
  there are no movies.
- All data is in `DATA.DAT`, indexed by `DATA.HD2` (32-byte records, no
  compression, case-insensitive paths). Packs inside it are 76-byte entry
  chains with relative offsets.
- Memory card saves are a 0x136A7-byte image: `CSaveData` (POD, 0x131C0),
  version string, checksum bytes, in directory `BESCES-50295dkcloud`.

## 3. Survey summaries

The full survey notes are in the session scratchpad; the parts each phase
needs are restated in its section. Cross-cutting facts:

**Entry and frame flow (`ps2/src/main.cpp:436-1044`).** `init_all` (IOP
boot, `InitCDFile`, `DevInit`, `MGInit`, `BufferAllClear`, `InitReadBG`),
pad init, 60 warm-up frames, then a mode loop: `LoadOverlay`,
`MGSetRenderInfo(800,10,65535)`, VU1 program upload, `init_now_loading`,
mode `Init`, spin on `check_now_loading`, `MGInitVSyncCallBack(PlayTimeCount)`,
Timer 0 setup, then per frame `MGBeginFrame`, `SetEnv`, `sceVif1PkCall(Vu_prog0f)`,
mode `Loop`, `GamePad.UpDate/Step`, `MGEndFrame`. Mode transitions via
`NextMapNo`. Modes: TITLE 0, RUSH_MOVIE 1, EDIT 2, DUNGEON 3, OPENING 5,
MENU 7, LOADER 9, MEMORY_CHECK 10, TRIAL_END 11, SAVE 13, LANGUAGE 14.

**Timing.** `vcount` (VSync count) is `static` in `mglib.cpp` alongside
`call_back_active`, `VSyncCallBack2`, `over_vsync`, `h_count`, so `MGInit`,
`MGInitVSyncCallBack`, `MGGetVSyncCount`, `MGBeginFrame`, `MGEndFrame`,
`MGFlipWaitVSync` are replaced as one group. `nowload.cpp` keeps `count`
and `VSyncField` static, so `init_now_loading`, `check_now_loading`,
`wait_now_loading_vsync`, `now_loading_off`, `VSyncCallBack_Load` are
replaced as one group. Spin sites that need the count to advance:
`check_now_loading` (main.cpp:700, editloop.cpp:4943),
`wait_now_loading_vsync` (~80 sites), `WaitVSync`, `ReadBGSync`
(main.cpp:1041, dun/gameloop.cpp:2049, title/title.cpp:243-287),
`MGInitVSyncCallBack`, `sceGsSyncV` (gamepad.cpp, snd.cpp:648,
editloop.cpp:1222-1533). `EditLoop` calls `MGEndFrame`/`MGBeginFrame` itself
for fades (nested presents must work).

**Resolution.** PAL field buffers are 640x240, display 640x480;
`SCREEN_HEIGHT` 480, `SCREEN_HALF_HEIGHT` 240 (`ps2/include/common.h`).
GS 12.4 coordinates with origin X 1728, Y `GS_Y_OFFSET` 0x7880; 2D rects
are `X=(x<<4)+27648`, `Y=(y<<3)+GS_Y_OFFSET` (Y halved into the field).
`MGSetViewMatrix_sub` squeezes Y by 0.5 for the field; `MGRotTransPers2D`
doubles it back. The port renders progressive full-height frames: the field
squeeze and half-line offsets are removed, not reproduced.

**Memory.** `GlobalDataBuffer` is a 27 MB static arena carved into
`VisualData`, `MotionData`, `TextureData`, `WaterData`, `ActiveData0/1`,
`read_buffer`, packet buffers, per mode. Sizes are PS2 quadword counts of
PS2 `sizeof`s; host classes with 8-byte pointers are larger (`Alloc(3)` for
`CVisualShadow`, `Alloc(0x81C)` for `CEditArea[4]`, script VM stacks sized
8 and 12 bytes per entry). The game casts arena pointers to `int`; the
executable is non-PIE so globals stay below 4 GB. `LoadFile2`/`LoadFileBG`
hang on buffers above 32 MB.

## 4. Target architecture

### 4.1 Layout

```
port/src/
  CMakeLists.txt          port build (+ tools, tests, shaders)
  main.cpp                main(): CLI/config, platform init, runs the game's main loop
  gameloop.cpp            replacement of ps2/src/main.cpp's main() body as a function
  platform/               host side; no game headers in interfaces
    window.{hpp,cpp}      SDL3 window, events, resize
    input.{hpp,cpp}       SDL3 gamepad/keyboard -> DualShock-shaped state, rumble
    clock.{hpp,cpp}       logic tick clock, VSync-count equivalent, frame pacing
    audio.{hpp,cpp}       SDL3 audio device, mixing callback
    paths.{hpp,cpp}       data/ and save/ roots (CLI --data/--save, env DC_DATA/DC_SAVE)
    config.{hpp,cpp}      runtime settings (tick rate, vsync mode, window, volume)
  gfx/                    Vulkan renderer; no game headers in interfaces
    renderer.{hpp,cpp}    device, swapchain, frames in flight, present, resize
    pipelines.{hpp,cpp}   all pipeline permutations, VkPipelineCache on disk
    resources.{hpp,cpp}   textures, palettes, meshes, render targets, staging
    draw.{hpp,cpp}        immediate API: 2D quads/lines, 3D meshes, copies, blits
    readback.{hpp,cpp}    depth/colour readback for pick-Z and screenshots
    shaders/*.vert|frag   GLSL 4.6, compiled to SPIR-V by glslang at build time
  sce/                    SDK implementations with a host equivalent (libvu0, libpad, libmc, eekernel)
  stubs/                  PS2_UNIMPLEMENTED stubs; shrinks as phases land
  <unit>.cpp              replacements for ps2/src/<unit>.cpp (dataread, mglib, texture, ...)
  title/<unit>.cpp, dun/<unit>.cpp   replacements for the overlay units
  audio/                  HD/BD/SQ player: vag.cpp, hdbank.cpp, sequencer.cpp, synth.cpp
  tests/                  darkcloud_tests (unit tests, headless render tests)
port/include/
  port.h, stubs/          as today
  platform/, gfx/, audio/ public headers of the above (only if a header must be shared)
tools/dcdata/             data extraction tool (C++26, std::filesystem)
```

Rules: `platform/`, `gfx/` and `audio/` never include a game header. The
replacement units (`port/src/<unit>.cpp`) are the only place game types and
port services meet.

### 4.2 Renderer (`port/src/gfx`)

Vulkan 1.4 core features only (dynamic rendering, synchronization2, timeline
semaphores, descriptor indexing for a bindless texture array, push
constants). One graphics+present queue. Frames in flight: 2.

- **Targets.** Main colour (B8G8R8A8 or R8G8B8A8 unorm) and depth
  (D32 float) at window pixel size, recreated on resize. A persistent
  "previous frame" colour image (the game samples the back buffer for
  motion trails). Named render-target textures for the game's `#name#w#h#bpp`
  placeholders (`shadow_buf`, `frame_image*`, `water*`, `blender`,
  `fukidashibase`, `fontbase`, ...) allocated at the requested logical size
  scaled by the current render scale.
- **Textures.** RGBA8 for 24/32-bit TIM2; R8 index + a 256xRGBA8 palette
  texture for 8-bit TIM2 so the game's per-frame CLUT swaps (text colour
  blink, `SetClut`) remain cheap. Sampler: linear or nearest per draw,
  clamp/repeat per draw. Mip levels as supplied. A handle-indexed registry;
  copy/blit operations resolve by handle. Uploads go through a staging ring.
- **Draw API** (immediate mode, recorded into the current frame's command
  buffer):
  - `Draw2D`: quads and lines in logical 640x480 space with per-vertex
    colour (0x80 = 1.0 scale applied in-shader), uv, optional texture and
    palette, draw state.
  - `DrawMesh`: 3D mesh (vertex buffer: pos float3, normal float3, uv
    float2, colour rgba8; index buffer; strips expanded to lists at build
    time) with per-draw constants: model-view-projection, normal matrix,
    up to 4 light directions and colours, ambient, material
    diffuse/ambient/specular, fog parameters, flags (lit, vertex colour,
    fog, textured, shadow pass), clip planes.
  - `DrawState`: blend from GS ALPHA (A,B,C,D,FIX), depth test
    (always/gequal/greater/never) and write, alpha test (func, ref),
    scissor in logical space, cull mode, fog on/off.
  - Copies and blits: `CopyTexture(src, rect, dst, x, y)`,
    `BlitTexture(src, rect, dst, rect, filter)`, `SnapshotFrame(dst)`,
    `SetRenderTarget(handle | main)`.
  - `ReadDepth(x, y)` queued, result available next frame (pick-Z).
- **Shaders.** GLSL 4.6 under `port/src/gfx/shaders`, compiled with
  glslang at build time into SPIR-V headers. One 2D vertex/fragment pair,
  one 3D pair with specialization constants for lit/vertex-colour/fog/
  textured/palette, a blit pair. GS semantics implemented in the fragment
  shaders: blend `((A - B) * C >> 7) + D` with clamp; alpha test; fog
  `Cv = (Cs * F + FOGCOL * (255 - F)) / 255`; MODULATE with 0x80 = 1.0 and
  2x headroom; TEXA alpha expansion for 24-bit textures.
- **Startup precompilation.** `pipelines.cpp` enumerates every pipeline
  permutation (blend equation variants, depth modes, specialization sets,
  topology) and creates them all during `RendererInit`, feeding a
  `VkPipelineCache` loaded from and saved to `save/pipeline_cache.bin`.
  A progress callback lets the window show a "compiling shaders" state.
  No pipeline is created lazily on the draw path.
- **Depth.** Reverse-Z (near = 1, far = 0) with GREATER_OR_EQUAL, matching
  the game's 16.7M-near/1-far convention, so the game's `ignore_depth`,
  `depth_write`, `MGClearZBuffer(mode)` map directly.
- **Headless.** `--headless` uses SDL's offscreen driver and
  `VK_EXT_headless_surface`; `--frames N` exits after N frames;
  `--screenshot path.png` writes the final frame. This is how CI and the
  executing agents run on lavapipe.

### 4.3 Time and VSync

`platform/clock.cpp` owns a monotonic clock and the logic tick period
(`1 / tick_rate`). The game's VSync counter is `elapsed / period`. The
replaced `MGEndFrame` presents, then sleeps or spins to the next tick when
`mgWaitVSync` is set, advancing the counter and invoking the installed
callback (`PlayTimeCount` or the loading screen) once per elapsed tick. Every
replaced spin-wait (`WaitVSync`, `ReadBGSync`, `check_now_loading`,
`wait_now_loading_vsync`, `sceGsSyncV`) calls `ClockPump()`, which advances
the counter, runs callbacks, pumps window events and, for the loading
screen, renders and presents a loading frame. No thread is needed because
the game never relies on true concurrency. Headless mode runs the clock at
unbounded speed.

### 4.4 Data

`tools/dcdata extract <iso-or-dir> <data-dir>`: reads the ISO 9660 volume
(own reader, 2048-byte sectors, primary volume descriptor, path walk;
pycdlib's fallback in `scripts/build/extract.py` is the reference for the
PAL prototype's quirks), locates `DATA.DAT` and `DATA.HD2`, and writes every
record to `<data-dir>/<lowercased path>` with `std::filesystem`. It also
accepts a directory already holding `DATA.DAT` and `DATA.HD2`. It verifies
sizes, prints a manifest and is idempotent.

`port/src/dataread.cpp` replaces `InitCDFile`, `LoadFile`, `LoadFile2`,
`LoadFileBG`, `ReadBG`, `ReadBGSync`, `BreakReadBG`, `StartReadBG`,
`InitReadBG`, `GetReadBGFile`, `WriteFile`: a case-folded index of `data/`
built once with `recursive_directory_iterator`; `LoadFile2` strips any
`device:` prefix, reads exactly `size` bytes into the caller's buffer and
zero-fills to the next 2048 boundary (callers rely on sector rounding);
`LoadFileBG` reads synchronously and marks the slot done; `WriteFile` writes
under `save/host0/`. Missing files return 0 as retail does.

### 4.5 Input, saves, runtime

- `sce/libpad.cpp` implements the nine libpad functions on
  `platform/input`: DualShock 2 layout (buttons active-low in bytes 2-3,
  sticks in 4-7), `scePadGetState` stable, `scePadInfoMode` DualShock,
  `scePadSetActDirect` rumble. Keyboard fallback mapping lives in
  `platform/input` and is configurable.
- `sce/libmc.cpp` implements libmc on `save/mc0/` and `save/mc1/` with the
  `sceMcSync` command codes the game checks (2 open, 3 close, 5 read, 6
  write, 0xA flush, 0xB mkdir, 0xC chdir, 0xD getdir). Directory and file
  names are the game's own.
- `sce/eekernel.cpp`: `FlushCache`, `iFlushCache`, `iSyncDCache` are
  no-ops; `Exit` exits.
- `sce/sifrpc.cpp`, `sifdma.cpp`, `sifdev.cpp`, `libcdvd.cpp`: IOP boot,
  module loading, RPC binding succeed as no-ops, because `init_all` is
  replaced and nothing else should reach them; whatever still does stays a
  loud stub.
- `runtime.cpp`: `mwInit` no-op (host runs static constructors),
  `LoadOverlay`/`mwLoadOverlay` no-op, `exit__2`, `__assert`.
- **Arenas.** `CDataAlloc2<1>::Alloc/Alloc64`, `Align64` and the
  `InitializeDataBuffer`/`BufferAllClear`/`SetDataBuffer` carving are
  replaced so each arena is backed by its own host allocation sized with
  headroom (request x4 rounded up), independent of the 27 MB
  `GlobalDataBuffer`, with a bounds assert instead of the retail infinite
  loop. This absorbs the 64-bit `sizeof` growth without touching call
  sites. Known over-small sites (script VM stacks, `EPARTS_*` records) are
  fixed individually in their replacement units.

### 4.6 Rendering replacement units

Replaced on top of `port/src/gfx`:

- `mglib.cpp`: every `MG*` function in `ps2/include/mglib.hpp` plus the
  VSync group. `MGSetRenderInfo`/`MGSetViewMatrix` build real projection
  and view matrices (perspective from scale 800, near 10, far 65535,
  window aspect) in `mgRenderInfo` and in port-side state; the field
  squeeze is dropped. Render state setters update the port's `DrawState`.
  `MGFillBox`, clears, `MGMoveImage`, `MGStretchMoveImage`,
  `MGMoveFrameBuffImage`, `MGGetFBuffTex`/`BackTex`, shadow begin/end,
  pick-Z, `MGRotTransPers*`, `MGClip*`, `MGCalcColor` are native.
- `visualvu1.cpp`, `visualshadow.cpp`, `cloth.cpp`, `water.cpp`,
  `frame.cpp`: `CreateVUdataFromMDT(Remake)` and the shadow/cloth/water
  builders produce GPU meshes (the "vu_data" pointer becomes a mesh handle
  stored where the game stores the block pointer); `DrawVu1` overloads
  submit `DrawMesh` with the constants the VU1 header carried;
  `CFrameVu1::DrawVu1` keeps its culling, attribute and hierarchy logic and
  emits draw state instead of GS registers.
- `texture.cpp`, `textureanime.cpp`, `nowload.cpp`: `CTextureManager`
  decodes TIM2 to GPU textures (no swizzle, no VRAM allocator), placeholder
  names become render targets, `ReloadTexture` is a no-op,
  `EnterFixTextureZ` becomes a full-screen background texture, texture
  animation and face changes become `CopyTexture`.
- 2D: `snd.cpp` sprite functions (`set2DSprite*`, `set3DSprite*`,
  `setColSprite`, `setAlphaFlag`, `setbilinear`, `LensFlare`),
  `gameutil.cpp` (`set2DSprite_Start/Core/End`, `SetClut`), `clsmes.cpp`,
  `spritetable.cpp`, `dispctrl.cpp` (debug font rasterised as glyph quads),
  `menu_draw.cpp` where it emits packets, fades.
- Long tail, each replaced function-by-function: `effectmacro.cpp`
  (DepthOfField), `runeffect.cpp`, `fireomni.cpp`, `fishing.cpp`
  (FishLineDraw), `edit.cpp`, `edit_in.cpp`, `editloop.cpp`,
  `editloop3.cpp`, `battlemenu.cpp`, `shot_freefuncs.cpp`, `clothread.cpp`,
  `langset.cpp`, `dun/gameloop.cpp` (MainDraw, shadows, water grab, frame
  grab, cursors, LoaderLoop), `title/*` (titleloop, opening, rushmovi,
  op_a..op_d, sprite, dispfade, scfader).

### 4.7 Audio (`port/src/audio`)

A from-scratch player for the game's banks: VAG/SPU ADPCM decoder, HD bank
parser (programs, splits, sample sets, envelopes), SQ sequence reader,
a MIDI-like sequencer driving a polyphonic synth (pitch from note and
sample rate, ADSR, pan, volume, reverb approximated), mixed at 48 kHz into
the SDL3 audio stream. `CSound` (`ps2/src/sound.cpp`, ~35 non-static
methods) plus `TransHdBd`, `set_spu`, `ezMidiInit`, `ezMidi`,
`ezTransToIOP` are the replacement seam; the HS messages the game sends
(F9 volume/pan, FD key-on/off) map to synth calls.

## 5. Phases and agent assignment

Each phase is one agent's job unless noted. Every phase ends with: the port
building with `-Wall` clean in its files, `darkcloud_tests` passing, a
headless run not regressing, and a commit on the agent's branch. Agents
work in their own git worktree branch; the orchestrator merges.

### Wave 1 (parallel, independent)

**P0+P3 Renderer core and build foundation** (`port/CMakeLists.txt`,
`port/src/gfx/*`, `port/src/platform/window.*`, `port/src/main.cpp` only
for the CLI, `port/src/tests/`). Deliver the full `gfx` API of 4.2 with a
sample headless test that draws textured 2D quads, a lit 3D mesh, a copy,
a blit and a depth readback on lavapipe and checks pixels. Shader build
step with glslang. Pipeline precompilation with on-disk cache. Resize.
Acceptance: `darkcloud --headless --frames 3 --screenshot out.png` works on
lavapipe; tests pass; `gfx/` has no game headers.
**Status: landed.** gfx Vulkan 1.4 renderer, shader build, pipeline cache, headless tests.

**P1 Data** (`tools/dcdata`, `port/src/dataread.cpp`, `platform/paths.*`,
tests). Deliver the extractor (ISO 9660 + HD2/DAT), the `dataread`
replacement, pack-file tests with synthetic data, and a documented
`data/` layout. No disc image is available in this environment: build the
test fixtures (a tiny ISO, an HD2, a DAT, packs) in the tests.
**Status: landed.** `tools/dcdata`, the `dataread` replacement, `platform/paths`.

**P2+P9 Platform and runtime** (`platform/input,clock,config,audio-device`,
`sce/libpad,libmc,eekernel,sifrpc,sifdma,sifdev,libcdvd`, `runtime.cpp`,
`dataset.cpp` arenas, `dataalloc2_1.cpp`, `main.cpp` arena Align64,
`nowload.cpp` timing group, the `mglib.cpp` VSync group's clock side as a
`platform/clock` service, `gameloop.cpp` skeleton). Deliver input on
SDL3, saves on files, the clock and pump, arenas on host allocations,
runtime no-ops, with tests for libmc (round-trip a save image through the
state machine), the clock and the arenas.
**Status: landed.** Input, clock, config, libpad/libmc/eekernel/sifrpc, arenas, runtime no-ops.

**P8 Audio** (`port/src/audio/*`, `platform/audio.*`, `port/src/sound.cpp`,
`port/src/gameutil.cpp` EZMIDI seam). Deliver VAG decoding, HD/BD/SQ
parsing, sequencer, synth, SDL3 output, and `CSound` on top. Tests with
synthetic banks and sequences (generate a VAG from a sine, an HD with one
program, an SQ with a few notes; check the mix).
**Status: landed.** VAG, HD/BD/SQ, sequencer, synth and mixer, SDL3 output, `CSound` on top.

**P5a CPU math replacements** (`port/src/mathutil.cpp`, `frame.cpp`
helpers, `collisionmdt.cpp`, `gameutil.cpp` `MotionProc2`,
`chararead.cpp`, `cloth.cpp` `CCloth::Step`, `water.cpp`
`pretest`/`Trans_AddCell`, `visualvu1.cpp` `InverseLength`,
`visualshadow.cpp` `CreateVUdataShadowCLIP` CPU clip, `mglib.cpp`
`MGRotTransPers3DSprite`/`MGCalcColor` math, `bound.cpp`). These are the
`#ifndef PORT` assembly functions: reimplement each from the retail
disassembly's semantics (the surrounding C++ and the VU0 instruction
sequences in `ps2/src`), keep the exact lane behaviour the game relies on
(which components are written, w handling), with tests against hand
computed values. Where a function emits VU1 data (the shadow CLIP builder)
produce the same CPU-side clipped triangle list the renderer unit will
consume, as plain arrays.
**Status: landed.** Every `#ifndef PORT` assembly function in C++ with retail's lanes.

### Wave 2 (after P0+P3 merges)

**P4+P5b 3D path** (`port/src/mglib.cpp` draw/state/matrices,
`visualvu1.cpp`, `visualshadow.cpp`, `cloth.cpp`, `water.cpp`,
`frame.cpp` `CFrameVu1::DrawVu1`). Meshes from MDT, draw submission with
the VU1 program semantics as shader constants, shadows (planar projection,
`shadow_buf` composite), shade pass, water with frame snapshot, cloth
rebuild per frame, pick-Z. Tests: build a mesh from a synthetic MDT and
render it headless; shadow composite.
**Status: landed.** MG library, meshes from MDT, `DrawVu1`, shadows, cloth, water, pick-Z.

**P6 Textures** (`port/src/texture.cpp`, `textureanime.cpp`,
`nowload.cpp` drawing side). TIM2 decode (RGB16/24/32, IDTEX8, IDTEX4),
IMG packs, placeholder render targets, `ReloadTexture` no-op, fixed
textures, Z-buffer background, texture animation on `CopyTexture`. Tests:
decode synthetic TIM2 of each type and compare pixels.
**Status: landed.** TIM2 decode, texture registry, placeholders, texture animation, loading screen.

**P7a 2D core** (`port/src/snd.cpp` sprites and `LensFlare`,
`gameutil.cpp` sprite batch and `SetClut`, `clsmes.cpp`,
`spritetable.cpp`, `dispctrl.cpp`, `menu_draw.cpp` packet sites,
`title/dispfade.cpp`, `title/scfader.cpp`, `editloop3.cpp` fades). Tests:
render a sprite table headless and check pixels.
**Status: landed.** Sprites, `SetClut`, message windows, sprite tables, debug font, fades.

### Wave 3 (after wave 2 merges)

**P7b Long tail A** (`dun/gameloop.cpp` rendering functions,
`effectmacro.cpp`, `runeffect.cpp`, `fireomni.cpp`, `fishing.cpp`,
`shot_freefuncs.cpp`, `battlemenu.cpp`, `clothread.cpp`, `langset.cpp`).
**Status: landed.** Dungeon draw and loader, effects, fishing, battle menu, cloth thread, language select.

**P7c Long tail B** (`title/titleloop.cpp`, `title/opening.cpp`,
`title/rushmovi.cpp`, `title/op_a..op_d.cpp`, `title/sprite.cpp`,
`edit.cpp`, `edit_in.cpp`, `editloop.cpp` draw functions).
**Status: landed.** Title scenes, opening, rush movie, title loop and the editor's draw functions; no stub is reachable from `main`.

**P10 Integration** (`port/src/main.cpp`, `gameloop.cpp`, `docs/PC.md`,
CI). The game's main loop runs through the title path headless with the
extracted data when present; without data the run reaches the first
`LoadFile` and reports the missing `data/` clearly. Every remaining
`PS2_UNIMPLEMENTED` is listed in `docs/PC.md` with the unit that reaches it.
**Status: landed.** `main` and `RunGame` run the game, link-time names, sifdev, smoke tests, CI, `docs/PC.md`.

## 6. Verification

- `darkcloud_tests` (a plain `main` with assert-style checks or a tiny
  framework in `port/src/tests`) runs under `ctest`. Every phase adds
  tests; synthetic fixtures are built in code, never checked in as blobs
  unless tiny.
- Headless rendering on lavapipe: `SDL_VIDEO_DRIVER=offscreen` and
  `VK_EXT_headless_surface`, Vulkan 1.4.321 loader and 1.4.318 lavapipe are
  installed in this environment (`vulkaninfo --summary`).
- The PS2 build is untouched: nothing in `ps2/src` or `ps2/include`
  changes in this work. If a phase cannot proceed without such a change,
  it stops and reports instead.
- `-Wall` clean in `port/src`; `clang-format` per `.clang-format`.

## 7. Risks and decisions already taken

- **No disc in this environment.** All phases validate with synthetic
  fixtures; the first run with real data happens on the user's machine.
  `tools/dcdata` is therefore written defensively (size checks, manifest).
- **Overlay re-initialisation.** Retail re-runs TITLE/DUN static
  constructors and re-zeroes their `.bss` on every switch. The port links
  them once. If a mode depends on fresh globals, P10 adds an explicit
  reinit for those globals in `gameloop.cpp`; it is not assumed.
- **Frame rate.** Game logic stays one tick per frame; the tick rate is a
  setting (default 50). Rendering above the tick rate without interpolation
  would just repeat frames, so presentation runs at the tick rate with
  vsync as a setting. Decoupling with interpolation is out of scope.
- **Arena headroom x4** is a stopgap that keeps retail call sites intact;
  peak usage is still to be measured once the game runs through its modes.
- **Field rendering** is removed, not simulated: every `SCREEN_HALF_HEIGHT`
  dependent rect that reaches the port is interpreted in 640x480 logical
  space; `MGStretchMoveImage` and `MGMoveFrameBuffImage` lose their
  interlace fix-ups.
- **libpkt/libdma/libgraph stay stubs** on purpose; a stub hit is the to-do
  list for the long tail.
