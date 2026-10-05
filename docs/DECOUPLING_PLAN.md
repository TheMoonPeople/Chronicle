# 64-bit cleanliness and frame-rate / resolution decoupling plan

Two changes to the PC port (`docs/PC.md`), planned together because they are
the last two places where the port still bends to the PlayStation 2's shape:

1. The executable must run anywhere in a 64-bit address space: no non-PIE
   link, no arena below 4 GiB, no pointer that survives a trip through a
   32-bit integer.
2. Game logic must run at a fixed, configurable tick rate while rendering is
   presented at any refresh rate and any window size or aspect, with motion
   interpolated between ticks.

Both keep the port's rules (`docs/PC_PORT_PLAN.md`, section 1): no edits
under `ps2/src`, replacement only through `PC_OVERRIDE` definitions in `port/src`, no
PS2 emulation. `docs/MACOS_PLAN.md` section 3 explains why item 1 is required
on arm64 macOS; it is the right shape on every platform.

## Part A: 64-bit cleanliness

### A.1 Where the 32-bit dependence lives

- **Link layout.** `port/CMakeLists.txt` links `-no-pie` so `.data` and
  `.bss` sit below 4 GiB.
- **Arenas.** `port/src/dataset.cpp` maps every arena with `MAP_32BIT`.
- **Game code.** Retail casts pointers to `int`/`s32`/`u_int` in roughly
  seventy units. Most are alignment masks and offsets that only use the low
  bits; a minority round-trip: the integer is cast back to a pointer, stored
  into an `int` member or global that is later read as a pointer, or
  subtracted from another truncated pointer to form an offset that is then
  applied to a 64-bit base. Known round trips: `((int) buffer + 63) & ~63`
  style alignment that is cast back (`editloop.cpp`, `itemdata.cpp`,
  `clsmes.cpp`, `battlemenu.cpp`, `dungeonmap.cpp`, `visualshadow.cpp`,
  `title/*.cpp`), `BtEventData = (s32) arena` (`btsysscript.cpp`), the
  `EPARTS_INFO_HEADER`/`EPARTS_FUNC_DATA` in-place relocation
  (`editloop.cpp:5262`, `edit_in.cpp:2017`), the script VM's `funcdata` and
  stack/call records (`runscript.hpp`, `editloop3.cpp:8294`,
  `runscript_opcodes.cpp:1583`), `CSpriteTable` pointer comparisons (already
  replaced), sound bank addresses (already replaced), the title units'
  `(int)` arithmetic (partly replaced).
- **File formats with pointer-sized fields.** `EPARTS_*`, `funcdata` and
  `OBJ_ANIME_SEQ`-style records are read from disc with the PS2 layout and
  then used through host structs whose pointer members are 8 bytes.

### A.2 Method

1. **The audit is the work list.** `scripts/port/truncations.py` (from the
   macOS portability work) classifies every pointer-to-32-bit cast as a
   round trip or low-bits-only, names the enclosing function and says whether
   the port already replaces it. `docs/port/truncations.md` is regenerated
   by the script and reviewed; every round-trip row is either fixed or
   justified there.
2. **Fix in port code, by function.** A round trip inside a function the port
   already replaces is widened in place (`uintptr_t` for alignment,
   real pointers for stored addresses). A round trip inside a retail function
   is fixed by replacing that function: the retail body is copied into the
   unit's `port/src/<unit>.cpp`, the cast widened, and nothing else changed;
   static helpers it needs are copied too, as the long-tail phases did.
   File-format records with pointer fields get a port-side host struct
   filled from the on-disc layout by the replacement loader, and every reader
   of the record is a replacement that uses the host struct.
3. **Prove it on Linux.** The arenas are forced above 4 GiB
   (`platform/memory`'s high path) and the executable is linked PIE with
   `-Wl,-z,separate-code` and ASLR left on, so any surviving round trip
   crashes here. The full test suite and the real-data boot
   (`darkcloud --headless --frames 120 --data data`) run that way in CI, and
   the title, town and dungeon paths are driven with scripted input.
4. **Remove the crutches.** Once the audit shows no round trip in linked
   code, delete `MAP_32BIT`, the `-no-pie` link option, the `(int)`-dependent
   `--defsym` aliases, and the "below 4 GiB" paragraphs in `docs/PC.md`.
   A debug-only `PortAssertLow` is replaced by a debug-only check that no
   arena lies below 4 GiB, so a regression cannot hide behind a lucky
   address.

### A.3 Acceptance

- `nm`/`llvm-objdump` show `darkcloud` is `DYN` (PIE); `readelf -l` has no
  fixed low segments.
- `docs/port/truncations.md` lists zero unresolved round trips in functions
  that are linked (dead retail code that `--gc-sections` drops is listed as
  such).
- The suite and the headless boot pass with arenas above 4 GiB.
- `docs/PC.md` no longer mentions a 4 GiB or 2 GiB requirement.

## Part B: frame rate and resolution decoupling

### B.1 What couples them today

- **One tick per presented frame.** `MGEndFrame` presents, then waits for
  the next logic tick (`ClockWaitNextTick`), so presentation runs at the tick
  rate (50 Hz by default) and the window's refresh rate is irrelevant.
- **Immediate rendering during the tick.** Game code issues `Draw2D`,
  `DrawMesh`, copies and blits as it runs; the frame is the side effect of
  the tick. Nothing can be presented between ticks.
- **Logic counts frames.** Fades, timers, auto-repeat, animations and
  `PlayTimeCount` count ticks; that is fine as long as the tick rate is
  fixed, and the game code cannot be made variable-step without editing it.
- **Resolution.** 2D is laid out in the 640x480 logical space mapped
  aspect-correct with letterboxing; 3D is scissored to the same 4:3 area; the
  screen-bound cull in `frame_draw.cpp` uses a fixed +-320 guard band; field
  height render targets (640x256) are scaled by the render scale; previous
  frame effects sample at logical size.

### B.2 Target design

**Fixed logic tick, free presentation.** The main loop becomes:

```
accumulate elapsed time
while (accumulated >= tick period) { run one game tick; record its display list; }
render one display frame from the last two display lists with alpha = accumulated / period
present
```

- A **display list** is what a tick drew: every `Draw2D`, `DrawMesh`,
  `Clear`, scissor and render-target change, in order, with their state and
  constants, plus the resource operations the tick performed. `gfx` gains a
  recording layer: during a tick, draw calls append to the list instead of
  being executed; at the end of the tick the list is sealed.
- **Replay with interpolation.** A display frame replays the newest sealed
  list. Each `DrawMesh` record carries the object's identity (the `CFrame`
  pointer, or the visual's record), its model matrix and the view matrix
  separately from the projection, so the replay can interpolate the model
  and view matrices (translation lerp, rotation slerp, scale lerp) between
  the previous list's entry for the same identity and the current one, and
  rebuild `mvp` for the display frame. Entries with no match in the previous
  list render at their current transform. 2D quads are replayed as recorded
  (the HUD is tick-exact); 3D sprites interpolate their world position when
  they carry one. The camera (`MGSetViewMatrix`) is recorded per tick and
  interpolated the same way.
- **Stateful operations run once.** Copies, blits, palette and texture
  updates, frame grabs into persistent textures (`frame_image`, `water`,
  texture animation) and `ReadDepth` queries are executed once, at the
  canonical render of the tick (below), never on replay.
- **Canonical render.** When a tick is sealed, the list is rendered once at
  alpha 1 into the tick's colour target. That image is what the next tick's
  `MGGetFBuffBackTex`, `SnapshotFrame` and depth readback see, so the
  previous-frame feedback effects (title trails, water, depth of field) stay
  tick-exact and independent of the presentation rate. Display frames render
  into the swapchain-sized main target from the interpolated list and sample
  the tick's targets where the list sampled the frame.
- **Presentation rate.** `config.json` `video.present_mode` (`fifo`, `mailbox`
  or `immediate`), `max_fps`, and `interpolation` (off replays the
  canonical image, which is today's behaviour with frame duplication).
  Headless mode presents one display frame per tick at alpha 1, so the
  existing tests and screenshots are unchanged.
- **Tick rate** stays `game.tick_rate` (default 50; 60 is the NTSC feel).
  Audio is already on its own clock. Input is sampled at tick start.
- **Spin-waits and the loading screen** keep using `ClockPump`; the loading
  screen presents through the idle hook as today, outside the display-list
  path.

**Resolution and aspect.**

- The 3D projection uses the window's aspect ratio: the vertical field of
  view is retail's (scale 800 over 240 rows), the horizontal follows the
  window, so a 16:9 window shows more world at the sides instead of bars.
  The screen-bound cull and `MGClipVertex`'s guard band take the real
  horizontal extent from the renderer instead of +-320.
- 2D stays in the 640x480 logical space, anchored: elements are placed
  relative to the logical frame, which is centred in the window; a
  `video.ui_scale` setting scales it. A `video.aspect` (`auto` or `4:3`)
  setting restores letterboxed 4:3 for everything.
- Render targets the game sizes as 640xN are allocated at the render scale
  as today; the "frame" targets (`frame_image`, `frame_buff`) take the
  display frame's logical size. Nothing in port code holds 640, 480, 224,
  240 or 256 except as the names of the game's logical space.
- `MGRotTransPers*` results feed game logic (lens flare occlusion, cursor
  placement), so they stay in the game's logical coordinates; only the
  renderer's mapping of those coordinates changes with the aspect.

### B.3 Work packages (Opus agents, after the macOS portability branch lands)

**B.3.1 Display lists and interpolation (`port/src/gfx`, `mglib.cpp`,
`frame_draw.cpp`, `visualvu1.cpp`, `platform/clock.cpp`, `main.cpp`,
`gameloop.cpp`).** The recording layer, identity on mesh records, the
canonical render, replay with interpolation, the main loop restructure,
config keys, tests: a moving mesh rendered at alpha 0.5 lands halfway
between its two tick positions; stateful ops execute once per tick (a
texture scroll advances by one step per tick regardless of display frames);
the previous-frame feedback is tick-exact across display rates; headless
output is byte-identical to today's for the existing tests.

**B.3.2 Aspect and resolution (`mglib.cpp` projection and cull,
`frame_draw.cpp`, `mglib_math.cpp` guard band, `gfx` mapping, config).**
Window-aspect projection, cull extents, anchored UI, `ui_scale`, `aspect`
setting, tests at 16:9, 21:9 and portrait sizes checking that a mesh at the
frame's edge is visible at 16:9 and culled at 4:3, and that HUD pixels stay
put.

Status: done. `video.aspect` (`auto` or `4:3`) and `ui_scale`; the projection,
the screen-bound cull and `MGClipVertex` follow the window; full-frame 2D and
frame grabs cover it (`docs/PC.md`, "Window aspect"; `port/src/gfx/README.md`,
"Aspect"); `aspect_*_test.cpp`. A window narrower than 4:3 keeps retail's
horizontal field of view instead of the vertical, so the logical frame always
fits.

**B.3.3 64-bit cleanliness (Part A).** Driven by the audit; replacement of
every round-trip site; PIE link; high arenas; CI job in PIE/high-arena
configuration; documentation.

### B.4 Risks

- Replay changes the cost model: a tick's list is rendered up to
  (refresh / tick rate) + 1 times. The lists are small (hundreds of draws);
  the canonical render is the one that pays for copies and readbacks.
- Interpolation of objects the game teleports (map changes, cutscene cuts)
  would smear; a record carries a "no interpolation" flag that
  `frame_draw.cpp` sets when the `CFrame` was created or moved by more than
  a threshold since the previous tick, and the first tick of a mode never
  interpolates.
- Replacing retail functions for 64-bit cleanliness copies retail bodies
  into the port; each copy is a maintenance cost and must stay minimal and
  be listed in `docs/PC.md`.
