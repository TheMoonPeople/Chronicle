# PC port

`PLATFORM=PC` builds the game's code as a native x64 Linux program with clang
20, as C++26, on SDL3 and Vulkan 1.4 (`docs/MACOS.md` covers macOS on Apple
Silicon). The port is always the PAL release;
there is no region setting. `docs/PC_PORT_PLAN.md` is the plan it was built
to and records the phases; this document describes what is built.

## Building and running

```sh
./build.sh linux-x64
(cd port/build/pc && ctest --output-on-failure -j4)
```

The build runs in the dev container (in place when already inside one) using
the Debug `linux-x64` preset, and discards a `CMakeCache.txt` configured for
another source path. `CLEAN=1`
discards `port/build/pc` first; `JOBS=N` sets the number of parallel build jobs.

The tests are GoogleTest cases, each a unit test that finishes in
milliseconds; none of them runs the whole game. The `AudioRealData` cases read
the title pack out of the extracted data and are skipped unless `DC_DATA` names
it.

`cmake --preset linux-x64` (and `linux-x64-release`, in `port/build/pc-release`)
with `cmake --build --preset` and `ctest --preset` of the same name does the
same.

It needs clang 20 with lld and the LLVM binary tools (`llvm-objcopy`,
`llvm-nm`, `llvm-objdump`, `llvm-readobj`, `llvm-lipo`), Python 3, CMake 3.28, Ninja,
`glslangValidator`, SDL3 (3.4) and the Vulkan 1.4 headers and loader, and at
run time a device with Vulkan 1.3 or later, `dualSrcBlend` and `shaderClipDistance`
(any desktop driver; Mesa's lavapipe in CI; `port/src/gfx/README.md`, "Device", has the whole list). `.github/workflows/pc.yml` is a
complete recipe on Ubuntu 24.04.

The game's files come from the disc (see "Game data"). Without them a
windowed start asks for the disc image and extracts it; `dcdata` does the
same from a shell:

```sh
port/build/pc/dcdata extract "rom/Dark Cloud (PAL).iso" data
port/build/pc/darkcloud --data data --save save
```

`darkcloud` takes:

| Option | Meaning |
|---|---|
| `--data DIR` | the extracted data (default: `DC_DATA`, then `./data`, then `data/` beside the executable, then `$XDG_DATA_HOME/chronicle/data`) |
| `--save DIR` | memory cards, `config.json`, the pipeline cache and host files (default: `DC_SAVE`, then `./save`, then `save/` beside the executable, then `save/` beside a local `data/`, then `$XDG_DATA_HOME/chronicle/save`); created on first use |
| `--headless` | SDL's offscreen video driver with `VK_EXT_headless_surface`, SDL's dummy audio driver, and the game clock unbounded (one tick per pump, no sleeping) |
| `--offscreen` | `--headless` without a Vulkan surface: frames are drawn to an image only (what `--headless` does by itself when the loader has no `VK_EXT_headless_surface`) |
| `--frames N` | stop after N frames of the game's main loop |
| `--screenshot PATH` | after the run, write the last tick's canonical image (or the loading screen's frame, if it presented since) to PATH as a PNG; never a display frame, so never the FPS counter |
| `--input FILE` | drive the pads from a script (default: `DC_INPUT`; see "Scripted input") |
| `--width W`, `--height H` | window size in pixels, over `config.json` (default: the monitor's resolution) |
| `--jump MODE[:MAP]` | test hook: start in a mode (see "Test hooks"); also `DC_JUMP` |
| `--fast-load` | test hook: loading-screen holds and fades of a few ticks; also `DC_FAST_LOAD=1` |
| `--display-per-tick N` | headless test aid: render N interpolated display frames per tick (offscreen, not presented) before presenting the tick's canonical image |
| `--show-fps` | draw the FPS counter when headless too (a headless run leaves `show_fps` off) |

Environment: `DC_DATA` and `DC_SAVE` (above), `DC_INPUT` (above), `DC_AUDIO=off` (no audio
device), `DC_AUDIO_WAV` and `DC_AUDIO_TRACE` (see "Audio"), `DC_VULKAN_VALIDATION` (enable the Khronos validation layer in a
release build; a debug build always asks for it), `DC_PRESENT_STATS=1` (print
draws per tick and render times at exit, and what the FPS counter said last
when it is on), and SDL's own variables.

Exit statuses (`port/src/exitcodes.hpp`): 0 when the window was closed or
`--frames` ran out, 1 when the window, the renderer or the screenshot failed,
2 for bad arguments, 3 when the data directory is missing or holds no file
and no disc was given at the first-run prompt (one line names it and the
`dcdata` command; a headless run checks before any window opens), 4 for a failed game assertion (retail's `__assert`, after the game's
own message: `LoadFile` prints `File open error "<path>"`). A `PS2_UNIMPLEMENTED`
stub aborts (SIGABRT) so a debugger or a core dump stops at it.

`<save>/config.json` (`port/src/platform/config.cpp`, read with
[nlohmann/json](https://github.com/nlohmann/json); `//` and `/* */` comments
are allowed, unknown keys and bad values are reported and ignored, and a file
that is not valid JSON is reported and leaves every default in place). When
there is no `config.json`, `darkcloud` creates one holding the defaults (the
file it writes has no comments). It prints `config: loaded <path>` when it
has read the file and `config: saved <path>` when it has written it. Every
key is optional; these are the defaults:

```jsonc
{
    "game": {
        "tick_rate": 50,            // logic ticks (the game's VSyncs) per second
        "debug_mode": true          // DebugMode: start in the developer menu; Start + Select returns to it
    },
    "video": {
        "present_mode": "fifo",     // fifo, mailbox or immediate (each falls back to the next safer one)
        "interpolation": true,      // false: present each tick's image once, as rendered
        "max_fps": 0,               // display frames per second at most; 0: as the present mode allows
        "width": 0,                 // window size in pixels; 0: the monitor's resolution. Both 0: fullscreen on the
        "height": 0,                //   monitor, so "aspect": "auto" takes the monitor's shape; headless: 1280x960
        "fullscreen": false,        // fullscreen at a given width and height
        "aspect": "auto",           // auto: the world fills the window, the HUD in its corners, other 2D in the centred 4:3 frame; 4:3: letterboxed
        "ui_scale": 1.0,            // 0.25 to 4: the HUD scaled about the window's centre
        "show_fps": true,           // the FPS counter at the window's top-left corner (headless: --show-fps)
        "detail_distance": 0,       // how far full detail reaches (world units); 0: at any distance
        "shadow_distance": 0        // how far town parts cast full shadows (world units); 0: at any distance
    },
    "audio": {
        "master_volume": 1.0        // 0 to 1
    },
    "input": {
        "mouse_sensitivity": 0.1,   // right-stick deflection (1 = full) per pixel moved in one tick
        "mouse_invert_y": false,
        "mouse_capture": true,      // SDL relative mouse mode while the window has focus
        "mouse_release": ["Escape"], // keys that give the cursor back in a window ([]: none)
        "bindings": {
            "cross": ["Mouse1", "Space"], // an action: its keys and mouse buttons; replaces the defaults
            "ry": "-MouseY",        // lx ly rx ry take MouseX or MouseY, with a sign and a scale (MouseX*0.5)
            "fps_toggle": "F3"      // the FPS counter on and off
        }
    }
}
```

`video.detail_distance` is how far away town houses, villagers and dungeon
monsters keep their full detail; past it the game's own distances apply, so a
small value such as `1` is retail's behaviour. A town part within that
distance of the eye draws its finest level of detail (retail steps down from
300, 500 and 800 plus a share of the part's size). A villager within it of
the player steps and draws (retail: the two nearest in front of the camera,
within 150). A dungeon monster within it of the player draws and animates
while still dormant: it wakes, acts, and shows on the map only at its own
clip distance (300 unless its script sets one) and under retail's limit of
four awake at once. The overhead view of georama mode keeps its coarse
models, and the 1200 cull of far town parts is unchanged.

`video.shadow_distance` is how deep into the view a town part (a house, a
tree, a fixed part) casts its shadow at full strength; past it the game's own
distances apply, so a small value such as `1` is retail's behaviour. Retail
sorts the parts into two bands by their depth from the eye
(`ps2/src/editloop.cpp:2356`, `CEditGround::DrawShadow`): those nearer than
the map's `shadow_near` (320 unless its `SHADOW_LEVEL` says otherwise) darken
the ground by `shadow_mode` (0x34 of 0x80), those out to `shadow_far` (740) by
the fainter `shadow_mode_2` (0x20), and a part past that casts none, so a
shadow lightens and then goes as the camera backs away. A part deeper than
300 also takes the cheaper volume program, and one with a `draw_distance`
drops its shadow past it. A part within `shadow_distance` belongs to the near
band with the precise program (`port/src/editground.cpp`). The characters'
shadows have no such distances, the overhead view of georama mode keeps its
own faint shadows, and a map whose `SHADOW_LEVEL` is 0 still has none.

`video.vsync` is shorthand for `present_mode`: `true` is `fifo`, `false`
`immediate`. A binding is one name or a list of names. The `bindings` shown
are examples of the form (the defaults are in the table below); a
`config.ini` from an earlier build is not read, and `darkcloud` says so when
it finds one without a `config.json`.

### Keyboard and mouse

The keyboard and the mouse drive pad 1 next to the first gamepad
(`port/src/platform/input.cpp`, `mouse.cpp`); a second gamepad is pad 2.
The defaults, and what the game does with each button (PAL, English: the
dungeon's `PadInput_OK` is cross and `PadInput_NO` circle,
`ps2/src/dun/gameloop.cpp:1666`):

| Input | Pad | What the game does with it |
|---|---|---|
| WASD | left stick | walk (`dun/gameloop.cpp:3006`, the town's `EdMoveChara`); menus with `MenuModeOn` turn it into the d-pad; d-pad as well where nothing reads the stick (below) |
| mouse motion | right stick | camera: `AddAngle(0.04 * -GetRXf())`, `AddHeight(-GetRYf())` (`dun/gameloop.cpp:4327`; the town's `MoveCamera`, `editloop.cpp:3793`) |
| left click, Space | cross | attack, open, talk (`dun/gameloop.cpp:3585`, `editloop.cpp:3885`); confirm |
| right click, X | R1 | held while locked on: guard (`dun/gameloop.cpp:3347`, `guard_mode = 5` at :3374); unlocked, turns the camera (:4338) |
| F | circle | lock on to the nearest enemy or let go; with none in range, swing the camera behind the character (`dun/gameloop.cpp:3243`); back in menus |
| E | square | use the active item (`dun/gameloop.cpp:3775`); held, a feather's speed boost (:3745) |
| Tab | triangle | the menu (`dun/gameloop.cpp:3134`, `editloop.cpp:1824`) |
| Z | L1 | next lock-on target while locked on (`dun/gameloop.cpp:3260`); otherwise turns the camera (:4342) |
| Q | L2 | held, the camera behind the character (`dun/gameloop.cpp:4347`) |
| R | R2 | first-person look (`dun/gameloop.cpp:4354`) |
| C, Backspace | select | switch character (`dun/gameloop.cpp:3185`) |
| Return | start | pause (`dun/gameloop.cpp:3045`); the title's prompts |
| arrows | d-pad | left and right pick the active item in the dungeon (`dun/gameloop.cpp:3218`); menus |
| V; middle click, B | L3; R3 | debug and editor functions only |
| IJKL | right stick | the camera from the keyboard |
| F3 | none | the FPS counter on and off (see "The FPS counter") |

The square button is not a guard in this game: the guard is R1 held while
locked on, so right click is R1. The camera turns the way the view moves on
PC: the follow camera sits at `follow + distance * (sin a, cos a)`
(`camerafollow.cpp`) and the stick's right is the screen's right
(`move_x = lx cos a + ly sin a`), so a positive RX, which lowers `a`, turns
the view right; a positive RY lowers the camera, which looks up, so the
default `ry = -MouseY` makes mouse up look up and `mouse_invert_y` flips it.

The actions are `up down left right cross circle square triangle l1 r1 l2
r2 l3 r3 start select lx- lx+ ly- ly+ rx- rx+ ry- ry+ lx ly rx ry`, and the
port's own `fps_toggle`, which presses no pad button. Keys are
SDL names (any case, `_` for a space; `Grave`, `Backquote` or `Backtick` for
the key left of 1); `Mouse1` to `Mouse5` are left, right,
middle and the two side buttons. `fps_toggle` also takes
`Gamepad:<button>` with SDL's gamepad button names (`Gamepad:guide`,
`Gamepad:misc1`), on any connected gamepad; the pad actions take a gamepad's
buttons from the gamepad itself. A gamepad axis deflected past the game's
dead zone wins over the keyboard and mouse on that axis; buttons add.
Each key-down of a toggle's key counts once, however briefly it is held.

- **Sticks.** A key is full deflection; two keys at right angles make a
  diagonal of the same length. Values go through the inverse of the game's
  `AxisCalibration` (a dead zone of 49 above and 50 below the centre, then
  78 steps for 128), so a deflection reaches the game exactly and small
  mouse motion is not lost in the dead zone.
- **Mouse.** The right stick at each pad read is the motion since the
  previous read, divided by the ticks between them, times
  `mouse_sensitivity` (0.1: ten pixels in one tick is full deflection,
  0.04 radians of dungeon camera per frame). A mouse that stops reads
  centred at the next read; motion during a load does not land at once.
- **Capture.** With `mouse_capture`, SDL relative mode holds the cursor
  while the window has focus. Losing focus releases it, and so does
  `mouse_release` (Escape) when the window is not fullscreen; a click in
  the window captures again and does nothing else. While released, motion
  and clicks do not reach the game. Without capture they always do.
- **WASD on the d-pad.** The developer menu (`MenuLoop`) and the dungeon
  loader read only the d-pad and never call `MenuModeOn`; the dungeon reads
  the d-pad to pick the active item while WASD walks. So the movement keys
  also press the d-pad exactly when the game did not read pad 1's left
  stick (`CGamePad::GetLX`/`GetLY`, also called by `GetLXf`, `AllOn` and
  `UpDate`'s menu mode) between its last two pad reads.
  `port/src/gamepad.cpp` replaces `pad_button_read` and `GetLX`/`GetLY`
  with retail's bodies plus the notes the host needs. Menus with
  `MenuModeOn(120)` (the title, the language select, the save screens, the
  dungeon and town menus) read the stick, so the game's own conversion
  turns a full WASD deflection (128) into the d-pad; a diagonal (91 per
  axis) stays under their threshold, as a gamepad's does.

### Scripted input

`--input FILE` (`port/src/platform/input_script.cpp`) replaces the pads with
a script, through `InputSetOverride`. Each line is

```
<frame> [pad1|pad2] [button ...] [key:NAME ...] [mouseN ...] [mouse:DX,DY] [lx ly rx ry]
```

and holds the named buttons (`cross circle square triangle start select l1
r1 l2 r2 l3 r3 up down left right`, any case) and the four stick bytes
(0-255, centred at 128 when left out) on that pad, pad 1 unless the line
says `pad2`, from that frame until the pad's next line. On pad 1 lines,
`key:NAME` (an SDL key name, `_` for a space), `mouse1` to `mouse5` and
`mouse:DX,DY` (pixels per tick) go through the keyboard and mouse bindings
as live input does, d-pad rule included, and reach `fps_toggle` too: a
line that starts holding `key:F3` is one press of it. A line with no
button releases everything. Frames count the game's main loop as
`--frames` does; frame 0 also covers the 60-tick warm-up and the loading
screens before the first frame. `#` starts a comment; a pad's frames must
not decrease. Pad 1 is held released until its first line; pad 2 keeps its
device unless a line names it. A bad script stops `darkcloud` with status 2
before the window opens. `GamePad.Down` fires on a press edge, so a press
needs a later line that releases it:

```
# language select (English), memory check, attract movie, title logo, menu
0
70 cross
75
90 cross
95
200 start
205
480 start
485
520 start
525
```

reaches the title menu at frame 560.

### Test hooks

For tests and debugging only; nothing the game does depends on them.

- `--jump MODE[:MAP]` (or `DC_JUMP`) sets `DebugMode`, skips the 60-tick warm-up
  and the developer menu, and starts `RunGame` in the mode with the globals the
  developer menu (and for a dungeon, the dungeon loader) would have left:
  `edit:<map>` (the game's `MapNo`: 0-4 the five towns, 11 and up the sub maps,
  99 the interior), `dungeon:<n>` (dungeon n, 0-6, as `MapJump(200 + n)`, at its
  floor select), `title`, `rush` (the attract movie), `opening`, or `menu` (the
  developer menu itself). A bad value exits with status 2.
- `--fast-load` (or `DC_FAST_LOAD=1`) cuts the loading screen's start delay to
  one tick, its fades to two or four ticks and its holds (PAL 120, 183 and 83
  ticks) to two. The modes' own fades are untouched.

`darkcloud --headless --jump dungeon:0 --fast-load --frames 60` shows the first
dungeon's floor select after about five seconds on lavapipe.

### Developer menu

PAL retail's `main` sets `DebugMode` when pad 2 holds L1+R1+L2+R2 through
the warm-up; the game then starts in `GAME_MODE_MENU`, the developer menu
(`MenuLoop`, `ps2/src/main.cpp`), instead of the language select, and leaves
pad 2 unlocked. The port takes `DebugMode` from `game.debug_mode` in
`config.json`, which is on by default, so it starts in the developer menu;
with the flag off it starts at the language select as retail does, and
nothing held or pressed changes that: the flag is the one way into debug
mode. `darkcloud` prints `debug mode on: the developer menu` when it starts
there.

While `DebugMode` is set, Start and Select on pad 1 go to the developer menu
at once from every mode: the language select, the memory card check, the
attract movie, the title, the opening, a town or an interior, the dungeon
loader, a dungeon and the save screen (`GameDeveloperMenuRequested`,
`port/src/gameloop.cpp`, after every frame of the main loop). The frame the
second of the two goes down ends the mode where it stands: there is no fade
and the town is not saved back to the save data; sound stops, the pad is
unlocked and a pending map jump or start event is dropped
(`GameEnterDeveloperMenu`). The chord is read from the host's pad, past the
game's pad lock, so it works in events and fades; the loading screen between
two modes and the fades a town presents on its own run to their end first.
Holding it into the next mode does nothing until it is pressed again.

After start-up, retail PAL flips `DebugMode` after every frame of the main
loop where pad 2 holds L1+R1+L2+R2 and R3 is pressed (`ps2/src/main.cpp:963`);
the port reads the same combination on pad 1 instead (`GameCheckDebugToggle`,
past the game's pad lock), and only while `game.debug_mode` is on, printing
`debug mode on` or `debug mode off`. It does not move the game anywhere:
`DebugMode` is a flag the modes read. What it does once set:

- The town (`EditLoop`, `ps2/src/editloop.cpp:2041`) and the dungeon
  (`GameLoop`, `ps2/src/dun/gameloop.cpp:2087`) leave on Select held with
  Start (in the town after a fade); their loop results send the game to
  `GAME_MODE_MENU` (`GameApplyLoopResult`: the town's result 1, any dungeon
  result), and the top of `main`'s loop runs the developer menu there only
  while `DebugMode` is set (otherwise the attract movie, `ps2/src/main.cpp:571`).
  These are retail's own ways back, which the port's Start and Select
  (above) now takes ahead of: the interior's (`EditInLoop`) went to the
  title, not the menu.
- In the town L3 shows the editor's debug overlay and R3 opens its debug
  menu (`editloop.cpp:1849`); in an interior Select walks out of the door
  (`edit_in.cpp:1342`); in the dungeon R3 opens the debug options outside an
  event (`dun/gameloop.cpp:3109`); a town event pauses on Start and stops on
  R3 (`editloop3.cpp:8678`); shops, menus and battles have their own, some
  on pad 2.
- Nothing on the language select, the attract movie, the title or the
  opening reads it; the port's Start and Select is the one thing that works
  there.

A boot without debug mode locks pad 2 (`GamePad.KeyLock2(1)`, as retail),
so the game's own debug functions that read pad 2 stay dead. In the
developer menu, up and down (pad 1) pick a row,
left and right change its number, circle or triangle enters it:

| Row | Goes to |
|---|---|
| game start | the attract movie (`MapNo` 801), then the title |
| `e0N` | the town `N` (1-5; `main_select_menu_no` N-1, `GAME_MODE_EDIT`) |
| `sN` | the sub map `N` (map number N+10, `GAME_MODE_EDIT`); R1/L1 step by ten |
| interior | map 99, `GAME_MODE_EDIT` |
| dungeon | the dungeon loader (`LoaderLoop`): up and down pick one of the seven dungeons, circle, cross or start enters floor 1 |
| opening | the opening (`GAME_MODE_OPENING`, scenes op_a to op_d) |
| `eventN` | one of three story events (map 23 event 310, map 41 event 150, map 19 event 305) |
| `memory card N` | the save screen in mode N |
| `Language N` | sets `LanguageCode` (PAL default 2, British English) |

```
0 pad2 l1 r1 l2 r2
1 pad2
10 down
12
20 circle
22
```

enters town 1 (Norune); `0 key:grave` with `1` releasing it does the same
from the keyboard.

## Start-up and the main loop

`port/src/main.cpp` is the executable's `main`. In order: `PathsConsumeArgs`
takes `--data`/`--save` out of argv; the other options are parsed; the data
directory is checked; `ConfigLoad` reads `config.json`; `WindowInit` opens the
window (size and fullscreen from the config, offscreen when headless);
`InputInit`; `gfx::RendererInit` with the config's present mode, the pipeline
cache at `<save>/pipeline_cache.bin` and a progress callback that prints
`compiling shaders n/total` at quarters; `AudioOutputStart` pulls
`audio::DefaultMixer()` at its rate with the config's master gain; the clock
gets the config's tick rate (unbounded when headless); a pump hook is
installed that pumps window events (a close request stops the game) and
samples input; then `RunGame`. After it returns: the screenshot, then
`AudioOutputStop`, `InputShutdown`, `RendererShutdown`, `WindowShutdown`.
`main.cpp` also forwards the names MWCC gives the calls in retail `main`
(below).

`RunGame` (`port/src/gameloop.cpp`) is retail `main` (`ps2/src/main.cpp`)
with the hardware taken out, line for line otherwise:

- `init_all` is reduced to `InitCDFile`, `MGInit`, `InitMemoryFile`,
  `BufferAllClear`, `InitReadBG`: no IOP reboot or module loads, no
  `sceCdInit`/`sceCdMmode`/`sceFsReset`, no `DevInit` DMA reset or channel
  handles. `mwInit` is not called; the host has run the static constructors.
- Every `sceGsSyncV` wait is `ClockSyncV()`; Timer 0, the DMA channel kick,
  `sceGsSyncPath` and the `FlushCache` calls go.
- The two uploads of `My_dma_start0` (the GS environment chain) and
  `Vu_progmain` (VU1 microcode) around a mode's `Init` become `LoadDrawEnv`,
  which sets the one register of `My_DrawEnv` the renderer still reads,
  TEXA (TA0 0x80, AEM 1, TA1 0x80).
- Per frame, `SetEnv` (replaced in `gameloop.cpp`) resets TEST, ZBUF and
  ALPHA to mglib's shadows as retail's A+D packet did, and the window
  rectangle to the full frame, which `sceGsSwapDBuff`'s draw environment did;
  TEX1 is read where a texture is bound and CLAMP is the sampler's.
  `sceVif1PkCall(Vu_prog0f)` goes.
- `save_data` and `config_data` are static in retail's unit, so `RunGame` has
  its own and reaches the rest through `SaveData`, as retail does.
- Mode 12's loop reads a register nothing set; the port's zero keeps it
  running.
- The transitions are `GameApplyLoopResult` (what each mode's loop result
  does) and `GameFollowMapJump` (`NextMapNo` into the next mode), exported for
  the tests.
- Each pass of the loop is one logic tick, recorded and rendered as described
  in "Ticks and display frames" below.
- `--frames` counts frames of this loop (one per `MGEndFrame` it calls);
  `RunGame` returns `kExitOk` at the top of the next frame once the budget
  is spent or a stop was requested, outside any frame, so the last frame can
  be read back. Presents a mode makes itself (`EditLoop`'s fades) and the
  loading screen's are not counted.

### Ticks and display frames

The game logic runs at the fixed tick rate; the window is presented at its own
rate, with motion interpolated between ticks (`port/src/gfx/README.md`,
"Display lists"):

1. `MGBeginFrame` starts recording the tick's display list: what the game draws,
   copies and uploads is appended to the list rather than executed.
2. `MGEndFrame` seals it and `GameRenderTick` (`gameloop.cpp`) renders it once,
   in full, as the tick's canonical image. That image is what the next tick's
   `MGGetFBuffBackTex` / `kPreviousFrame` samples and what frame copies and
   `mgPickZBuff` read, and every copy, blit, texture or palette update in the
   list (frame grabs into `frame_image` and `water`, texture animation,
   `MGMoveImage`, `MGStretchMoveImage`) runs there, once per tick. The
   previous-frame effects (the title's trail, water, depth of field) are
   therefore tick-exact whatever the display rate.
3. `MGEndFrame` then waits for the next tick as before (`mgWaitVSync`, the
   count, the callbacks and every spin on the clock are unchanged), and while
   it waits `GamePresentBetweenTicks` renders display frames from the last two
   lists at alpha = the elapsed fraction of the tick and presents them, as fast
   as `present_mode` and `max_fps` allow. A display frame replays the newest
   list's drawing only, into its own image, with each `CFrame`'s model matrix
   (`frame_draw.cpp` tags its draws with the frame's address; a visual drawn
   outside one with its record) and the camera interpolated between the two
   ticks. A frame drawn at several places in a tick (a town's tiles and parts
   share theirs) is matched place by place, not in draw order. A tick that did not wait, or presented nothing, presents its
   canonical image.

A grab of the frame that the tick draws back (the depth of field's blur, the water)
is taken again by each display frame from its own image, so it lines up with the
interpolated scene; the game's texture keeps the tick's grab. A cloth is blended
vertex by vertex unless one moved more than 10 units in the tick, which is where
`CCloth::Step` stops simulating and carries the grid along.

What a display frame does not interpolate: 2D (the HUD is tick-exact), 3D
sprites (2D quads with depth), skinned poses, objects that moved more than
`kDraw3DTeleportDistance` (200 units) in a tick, a camera that moved more than
200 units or turned more than 45 degrees, and the first tick of every mode
(`RunGame` cuts there). `interpolation = off` presents each canonical image once,
which is the PS2's picture at the tick rate. Headless runs (unbounded clock)
present one canonical image per tick, so screenshots are those images.

The loading screen still presents from the idle hook as immediate frames; once
it has, display frames stop until the next tick's canonical render.

### The FPS counter

With `video.show_fps` (on by default; `fps_toggle`, F3, flips it at any
time), every presented frame carries one line at the window's top-left
corner, in the port's own 5x7 font (`port/src/platform/overlay.cpp`, white
on a translucent black backdrop, on whole pixels: one per logical unit,
rounded):

```
FPS 143.9  TICK 50.0/50  DRAWS 412
```

the frames presented per second and the logic ticks rendered per second,
each over the last half second or so, the configured tick rate, and the
newest tick's mesh and 2D draws. It is drawn with `gfx::Draw2D` into a
display list of its own, recorded only when the text or the window's mapping
changes, never into a tick's list: a display frame draws it after the tick's
list and before the present (`gfx::RenderOptions::overlay`), and a tick
presented as its canonical image is presented as a display render of the
counter alone, which starts from that image. So the canonical image, which
`kPreviousFrame`, frame copies, pick-Z and `--screenshot` read, never holds
it, and screenshots are the same byte for byte with it on or off. The
loading screen's own presents do not carry it. Headless runs leave it off
unless `--show-fps` is given.

A display frame costs what drawing the tick costs on the GPU, plus little on the
CPU: in the opening's first scene (about 2,100 mesh draws and 100 2D draws per
tick, all of them keyed), a release build spends 1.6 ms interpolating and 1.8 ms
replaying a list, and lavapipe about 150 ms rasterising it at 1280x960. The
canonical render is the frame the port drew before, and the only one that pays
for copies and readbacks. The first dungeon's floor B1 draws about 265 mesh and 145 2D
draws a tick (at most 666). `darkcloud --headless --display-per-tick 4` leaves
every screenshot of the title, the attract movie, the opening and that floor
byte for byte the same.

Overlays are not re-initialised: `TITLE.BIN` and `DUN.BIN` are linked in
once, where retail reloaded the overlay's data, zeroed its `.bss` and re-ran
its static constructors on every switch between the title and the dungeon.
`LoadOverlay` only rebuilds the title objects the port defines with host
classes ("The title overlay's own class declarations"). A mode that relies
on fresh overlay globals is a known gap (below).

Each mode's loop result is applied by the mode that ran the loop
(`GameApplyLoopResult(old_main_mode, result)`), as retail handles it inside
that mode's `case`: the developer menu sets `mode` itself and returns 1.

## Platform

`port/src/platform`, `port/src/gfx` and `port/src/audio` are the host side.
They build as `dc_host` without `port.h` or the game's include paths, so no
game header or SDK type reaches them. Game types meet them only in the
replacement units.

- **Window** (`platform/window`): SDL3 window, resizable, high pixel density,
  optionally fullscreen; `WindowPollEvents` pumps events, reports a close,
  and forwards pixel-size changes to the renderer. `WindowAddEventHook` lets
  input see every event.
- **Input** (`platform/input`, `platform/mouse`, `sce/libpad.cpp`,
  `gamepad.cpp`): two DualShock 2-shaped pads from SDL gamepads, the
  keyboard and the mouse, with rumble. libpad's nine
  functions read them: buttons active-low in bytes 2-3, sticks in 4-7,
  `scePadGetState` stable, `scePadInfoMode` DualShock. `InputSetOverride`
  replaces a pad for tests.
- **Clock** (`platform/clock`): the stand-in for the VSync interrupt. One
  tick is one VSync, the count is the number of tick periods since the
  anchor, and the game's tick callback (`PlayTimeCount` or the loading
  screen) runs inside `ClockPump`, once per elapsed tick. Every retail spin
  on the interrupt pumps (`sceGsSyncV`, `WaitVSync`, `check_now_loading`,
  `wait_now_loading_vsync`, `ReadBGSync`, `MGEndFrame`). After the ticks a
  pump runs the pump hooks (the host's, added with `ClockAddPumpHook`: the
  window and input) and then the single idle hook (the game side's,
  `ClockSetIdleHook`: the loading screen's presenter), so spin-waits keep the
  window alive and the pads fresh whatever the loading screen does.
  `sceGsSyncV` returns the parity of the tick, as the interlaced field
  alternated: `CGamePad::Init` and `main` spin until it reads 1. The tick
  rate is a setting (50 Hz by default); presentation is not tied to it (below).
  `ClockWaitNextTick(hook)` runs a hook over and over while it waits, with the
  elapsed fraction of the tick; `MGEndFrame` presents display frames through it.
- **Config** (`platform/config`) and **paths** (`platform/paths`): above.
- **Overlay** (`platform/overlay`): the port's 5x7 font (the first-run screen
  draws with it too), a line of text drawn with `gfx::Draw2D` on whole pixels
  at the window's corner and recorded as a display list of its own, and the
  rate meter behind the FPS counter ("The FPS counter").
- **Audio output** (`platform/audio`): an SDL3 float stereo stream that pulls
  frames from a render callback on SDL's audio thread; `DC_AUDIO=off` or no
  device leaves the game silent.

## Rendering

`port/src/gfx` is a Vulkan 1.3+ renderer: one graphics queue that presents,
two frames in flight, dynamic rendering, synchronization2, a bindless texture
array, every pipeline created at start-up against the on-disk pipeline
cache, reverse-Z D32 depth with stencil, an immediate 2D API in the game's 640x480 logical
space (centred in the window) and a mesh API in 3D, named render targets,
copies, blits, depth readback and screenshots. `port/src/gfx/README.md` is its
contract: spaces, colour and alpha units, how the GS blend equation maps to
Vulkan blending and where it does not.

### Window aspect

The logical 640x480 frame is centred in the window at `min(W / 640, H / 480)`
pixels per unit, so the HUD keeps its place relative to the frame at any window
shape. With `video.aspect` `auto` the rest of the window is not bars:

- **3D.** `Draw3DEyeToClip` (`mglib.cpp`) maps the game's projection, logical
  `(320 + 800 x / z, 240 + 800 y / z)` (`MGSetRenderInfo`'s scale over the
  frame's 240 half-rows), through the target's logical mapping. The frame's rows
  fill the window's height, so the vertical field of view is retail's
  (2 atan(240 / 800), 33.4 degrees) and the horizontal one is
  2 atan(240 W / H / 800): 16:9 shows 107 logical units more on each side. A
  window narrower than 4:3 keeps the frame's width (retail's horizontal field of
  view) and shows more above and below, so the HUD always fits.
- **Culls.** `frame_draw.cpp`'s screen-bound test and `MGClipVertex` (so
  `MGClipBox`) take their half extents from what the current target shows
  (`Draw3DVisibleExtent`, from `gfx::VisibleLogicalRect`) instead of 320 by 240:
  a model past the 4:3 edge is drawn at 16:9 and dropped at 4:3. The GS guard
  band (2048 units about the centre) is far outside any window's extent and is
  unchanged.
- **2D** stays in logical space. Full-frame 2D (fades, the screen filter's
  `MGFillBox`, the previous-frame feedback, frame grabs drawn back; the loading
  screen clears the whole window) reaches the window's edges by the rule in
  `port/src/gfx/README.md`, "Aspect": an untextured or frame-image rectangle that
  reaches an edge of the frame from inside is carried to the window's edge;
  textured HUD pieces and 4:3 pictures (the floor select's backdrop) are not.
  `video.ui_scale` scales the depthless 2D about the window's centre.
- **HUD.** The pieces that sit by an edge of the frame keep their distance to
  that edge of the window instead, at their own size (`gfx::UiAnchor`,
  `port/src/gfx/README.md`, "Aspect"): the dungeon's status panel (life, weapon
  and water to the top left, the quick items top centre, the floor plate and the
  mini map to the top right, the weapon's portrait and gauge to the bottom left;
  `DrawGame`, `port/src/dun/gameloop.cpp`) and the town's clock (top right;
  `EdDrawClock`, `port/src/editloop.cpp`). Menus, message windows, the Georama
  editor's panels and 2D placed from 3D projections stay in the frame.
- **Frame grabs** (`frame_image`, `frame_buff` and the other `frame_*`
  placeholders, and the water's `water` and `water_buff`; `MGPortFrameTarget`)
  are as wide as the window, so what they hold and draw back includes the
  sides; the canonical and display images are the window's size. The game's
  other 640xN targets (`blender`, `shadow_buf`) keep their size at the render
  scale.
- **Depth of field** (`DepthOfField`, `port/src/effectmacro.cpp`; the towns'
  heat haze is its jittered second pass) is not retail's two shrunk copies of
  the whole frame, which blurred whatever stood in front of a focus plane into
  the scenery behind it and came back as blocks. Each pass blurs only what
  lies beyond its own plane: the frame is drawn into a target sharing its
  depth buffer under the plane's depth test (the rest stays transparent
  black, so the image is premultiplied by its coverage), resized to half size
  (and for the second pass to quarter size and back to half, which rounds the
  texels off) and laid back in retail's bands, the frame first scaled down by
  the image's coverage and the image then added. The images are as wide as
  what the target shows, so the blur and the haze reach the window's edges;
  the two outermost haze columns do not wander, which on the PS2 left a
  ragged strip of the sharp frame in the overscan; `frame_image` is no longer
  written.
- **Water** (`port/src/water_draw.cpp`) refracts the frame at its own
  resolution instead of the game's field copy, and only from where water
  shows: a ripple's sample that lands on a bank or a character in front of the
  water gives way to the pixel's own colour. `MGMoveImage` takes a whole
  picture of the frame beside the game's copy (`Draw3DFramePicture`); the
  surface is drawn flat into a target sharing the frame's depth buffer (what
  the water covers), the rippled samples of that are laid over a second flat
  drawing by their coverage, and the frame takes the result with the game's
  colour and blend. Every sample is taken where the vertex lands
  (`gfx::kMeshScreenUv`), so it follows a display render's camera and reaches
  the window's sides. A surface drawn with no frame copy to hand, or into
  another target, samples the game's copy as before.
- **The title's backdrops** are built for the frame's width: the title
  screen's sky model, its cloud (whose animation parks spheres just outside
  the frame's sides; it is spread across, not enlarged) and the attract
  movie's turning title card are drawn larger by as much as the view is wider
  (`TitlePortBackdropScale`, `port/src/title/title_port.cpp`).
- **The FPS counter** is the host's and is never carried past the frame
  (`gfx::RenderOptions::host`).
- **Game logic** is unchanged: `MGRotTransPers*` answer in retail's GS and
  logical coordinates, which land on the meshes through the same mapping (the
  dungeon's lock-on corners, the town's edit cursor and name tags).

`aspect = 4:3` letterboxes everything as before, byte for byte. Known
differences at other aspects: game code that
drops 2D it projects outside 0..640 (effects, name tags) still does; at a
`ui_scale` other than 1 the 2D the game places from 3D projections moves with
the HUD.

The game's drawing reaches it through replacement units, in four groups that
share small internal headers:

- **`mglib_port.hpp`** (`mglib.cpp`, `mglib_port.cpp`, `mglib_math.cpp`):
  every `MG*` function. The GS register shadows the game sets
  (`MGSetGsTEST/ZBUF/ALPHA/TEXA`, `MGSetWindowRect`, and the clears, fills
  and stretches that leave them set) become the current `gfx::DrawState`
  (`MGPortDrawState`); GS 12.4 coordinates and 24-bit Z convert into logical
  space and reverse-Z (`MGPortLogicalX/Y`, `MGPortDepth`); TBP0 0 and 0xFFF
  stand for the frame and the previous frame. The VSync group (`MGInit`,
  `MGInitVSyncCallBack`, `MGGetVSyncCount`, `MGBeginFrame`, `MGEndFrame`,
  `MGFlipWaitVSync`) sits on the clock; `MGBeginFrame` starts recording a
  tick, `MGEndFrame` renders it, presents between ticks and keeps retail's
  "do not wait twice" rule. Pick-Z reads the canonical render's depth buffer.
  `MGSetRenderInfo` keeps retail's matrices; the field squeeze is undone
  where the game's rects reach the renderer.
- **`texture_port.hpp`** (`texture.cpp`, `texture_port.cpp`,
  `textureanime.cpp`, `nowload.cpp`): TIM2 (IDTEX4, IDTEX8, RGB16/24/32,
  mip levels, the IM2 swizzle undone) decoded into renderer textures with
  index textures and 256-entry palettes; a registry keyed by unique TBP0/CBP
  values so any TEX0 the game hands around resolves to one image;
  placeholder names (`#name#w#h#bpp`) become named render targets; texture
  animation and CLUT swaps are copies. The loading screen draws from the
  idle hook, never inside a frame the game has open.
- **`draw2d_port.hpp`** (`snd.cpp`, `gameutil_sprite.cpp`, `clsmes.cpp`,
  `spritetable.cpp`, `dispctrl.cpp`, `editloop_sprite.cpp`): the sprite
  primitives, `SetClut`, message windows, sprite tables and the debug font
  as glyph quads, mapped into the current target (render targets hold field
  rows).
- **`draw3d.hpp`** (`frame_draw.cpp`, `visualvu1.cpp`, `visualshadow.cpp`,
  `cloth_draw.cpp`, `water_draw.cpp`, the `*_math.cpp` units): MDT data built
  into meshes keyed by the game's vu_data block (the record dies with the
  block), `DrawVu1` submitting meshes with the constants the VU1 header
  carried (matrices, four lights, ambient, material, fog), shadow volumes
  extruded to the shadow plane and counted into `shadow_buf` against the
  scene's depth, then composited, cloth rebuilt per draw, water sampling the
  last frame copy. The assembly functions (`MulMatrix`, `MotionProc2`,
  `CCloth::Step`, the shadow CLIP builder and the rest) are C++ with the
  lanes retail writes.
- The long tail on top of those: `dun/gameloop.cpp` (`DunMainDraw`,
  `LoaderLoop`), `effectmacro.cpp`, `runeffect.cpp`, `fireomni.cpp`,
  `fishing.cpp`, `shot_freefuncs.cpp`, `battlemenu.cpp`, `clothread.cpp`,
  `langset.cpp`, and `editloop_init.cpp` (`EditInit`, its town objects
  carved out of `EtcDataBuffer` at host sizes where retail's quadword counts
  are the PS2's).

## Audio

All music and effects are sequenced. `port/src/audio` decodes VAG ADPCM,
parses HD banks and SQ sequences, and runs a sixteen-port MIDI player into a
48-voice synth with envelopes and an approximated reverb (`audio::Mixer`).
`port/src/sound.cpp` replaces `CSound` (bank and sequence transfers, play,
stop, fades, volumes, effect messages) on that mixer, and
`gameutil_midi.cpp` answers the EZMIDI RPC commands for anything that still
sends them. `main` starts the output; `CSound::Init` starts it too if it is
not running.

The formats and the synth's arithmetic were checked against every bank and
sequence on the PAL disc and against the IOP modules the game loads
(`EZMIDI.IRX`, `MODMIDI.IRX`, `MODHSYN.IRX`):

- **SQ** (`sq.cpp`): `SCEIVers`, `SCEISequ`, then `SCEIMidi` with a song table
  (offsets from the chunk) whose blocks start with a 32-bit data offset (always
  6) and a 16-bit division (always 480). The events are SMF-like with running
  status, but a note-off carries only its note number, and bit 7 on a channel
  message's last data byte means the next event has no delta. All 93
  sequences parse to their end-of-track meta exactly at the chunk's end.
- **Loops** (`sequencer.cpp`): NRPN 0 (`B0 63 00`) with data entry `n` opens loop
  `n` just after that data entry; NRPN 1 with data entry `n` and data entry LSB
  (controller 38) `c` jumps back `c` more times, forever for 0. Every sequence
  on the disc has one endless loop.
- **HD** (`hdbank.cpp`): the `SCEIHead` addresses, 36-byte programs, 20-byte
  splits, 4+2n-byte sample sets, 42-byte samples and 8-byte VAG entries as
  laid out there. Split bend ranges and every detune are in 128ths of a
  semitone (most splits say 0x600, an octave). A sample set's first byte picks
  a velocity curve (linear, inverse, squared and their mirrors). A sample's
  last byte is its SPU attribute: bits 4-5 pin its voices to core 0 or 1 (or
  either, whichever has more free voices), bits 0-3 are its dry left/right and
  effect-send left/right switches, so only those samples feed the core's reverb.
- **Volume**: a voice sounds at velocity (through the curve) x program x split
  x sample volume, each a fraction of 128, x channel volume x expression, each
  a fraction of 128, x port volume / 256 (`ezMidi(0xB0 + port)`, so the SE
  ports' 256 is unity and a sequence plays at its `sqtbl.txt` volume over 256).
  The pan law keeps the near side at full scale and fades the far side to
  silence 63 steps from the centre.
- **Pitch**: 4096 x rate / 48000 x 2^((note + transposes - base note + fine / 128
  + bend x range / 8192 / 128) / 12), at most 0x3FFF, as the SPU2 pitch register.

`DC_AUDIO_TRACE=1` prints every `CSound` call (bank and sequence loads with
their names, `SQ_Play` with its table volume, `SetVol`, `SE_Play`, fades,
reverb), every `ezMidi` command, every channel message a sequence sends with
its output time, every key-on's resolved sample, pitch, gain and core, and the
sequencer's tempo and loop events. `DC_AUDIO_WAV=<path>` opens no device and
instead pulls the mixer on the game thread as the clock ticks, writing 16-bit
stereo at 48 kHz: N ticks write N/50 s whatever the run's real speed, so
`DC_AUDIO_WAV=title.wav darkcloud --headless --jump title --fast-load --frames
600` records what the title would have played. `audio_real_title_*` render the
title pack's music on the real data (`DC_DATA`); with `DC_AUDIO_TEST_WAV=<dir>`
they also write what they rendered there.

What still differs from the PS2: the reverb is a generic room, not the SPU2's
effect programs and work area; core 0's output does not pass through core 1;
the ADSR runs at the SPU's rate but key follow of the envelope, LFOs, velocity
crossfades and per-key pan follow are not applied; effect messages resolve
their tone by key range, then split index.

## Saves and host files

`sce/libmc.cpp` implements libmc on `<save>/mc0/` and `<save>/mc1/`, a
directory per card, with the game's own directory and file names. Every
command finishes inside the call that issues it, and the next `sceMcSync`
reports the function number and result the game checks. The save itself is
retail's 0x136A7-byte image. `sce/sifdev.cpp` implements `sceOpen`,
`sceRead`, `sceWrite`, `sceLseek` and `sceClose` on `<save>/host0/` with the
device prefix stripped (the debug dump `edit.cpp` writes to `host0:`), and
`WriteFile` writes there too.

## Arenas

`CDataAlloc2<1>::Alloc/Alloc64/Align64` and the carving in
`InitializeDataBuffer`, `BufferAllClear`, `SetDataBuffer` and
`SetPacketReadBuffer` (`dataset.cpp`, `dataalloc2_1.cpp`) give each arena its
own block, an ordinary anonymous mapping wherever the system puts it
(`port/src/platform/memory.hpp`), sized at four times the quadwords retail
asked for (`kArenaHeadroom`, `port/src/arena.hpp`), because the game sizes
allocations with the host's larger `sizeof`s. A guard page follows each
block, and an overflow aborts naming the arena instead of retail's endless
loop. A debug build stops when no block lies above 4 GiB, so a mapping that
drifted low cannot hide a pointer the game truncates (below).

Retail places objects in runs whose quadword counts it wrote by hand
(`new ((u_long128 *) alloc->Alloc(4)) CCollisionMDT`), and the host's object is
larger wherever it holds a pointer or a vtable: the collision is five
quadwords, the cloth, its bounds, the fish and the visuals outgrow theirs too.
The placement operators (`dataalloc2_1.cpp`) therefore grow the run they are
handed to the object's size while it is still the arena's last, so nothing
allocated afterwards lies under the object's tail. At retail's four quadwords
the collision's `mesh_count` landed on the x of the first triangle built
behind it, which opened holes in dungeon floors. An allocation that is cast
rather than placed has no size to go by and is sized in `port/src` instead
(`Quadwords<T>` in `editloop_init.cpp`).

## Pointers and 32-bit integers

The game was written for a 32-bit ABI and casts pointers to `int` and back.
`darkcloud` is position-independent (`-pie -z separate-code`) and its arenas
lie above 4 GiB, as on arm64 macOS, whose low 4 GiB are a hard page zero, so
such a round trip faults where it happens instead of working by luck.
`scripts/port/truncations.py` finds every cast of a pointer to a narrower
integer with libclang, follows the value to where it becomes a pointer
again, and says whether the function holding it runs in the linked
executable; `docs/port/truncations.md` is its output. CI runs it with
`--check`, which fails while the file is stale or lists a round trip that
can run.

Each retail function holding such a round trip is replaced by a copy of its
body with the cast widened (a `uintptr_t` for an address formed from an
integer, the pointer itself where retail kept it in an `int`) and nothing
else changed:

| Function | Unit | Change |
|---|---|---|
| `DunMoveChara`, `BtCheckDamageProc` | `dun/movechara.cpp` | addresses of `NowDngMap`'s parts and of the active item counters on a `uintptr_t` |
| `CDungeonMap::BuildCharaSpecialParts`, `SetCharaDoor` | `dungeonmap.cpp` | the key door's cell on a `uintptr_t` |
| `BtGetTreasureboxBig_Init`/`_Loop`, `BtGetTreasureboxSmall_Init`/`_Loop`, `BtGetGateKey_Init`/`_Loop`, `BtEscape_Init`/`_Loop` | `btitem.cpp` | the files read between the two halves kept as pointers, not in `itemOpenItemMds`, `itemOpenItemImg` and `escape_chr` |
| `_ITEM_USE_WINDOW`, `BtMiniItemSelect_Loop` | `btsysscript.cpp`, `btitem.cpp` | the script slot the choice goes to kept as a pointer (`btitem_port.hpp`) and written through the host's `RS_STACKDATA`, whose value is at byte 8 |
| `BtSystemScriptLoad`, `BtSystemScriptRun` | `btsysscript.cpp` | the event script kept as a pointer, not in `BtEventData` |
| `EditInit`, `InitWorkBuffer` | `editloop_init.cpp` | arena starts rounded on the whole pointer |
| `CommandWATER_SHAKE` | `editmapscript.cpp` | the wave slot on a `uintptr_t` |
| `CEditGround::DrawPartsCursor` | `editground.cpp` | the preview frame `LoadObjectParts` keeps in an `s32` matched against the part's own frames |
| `FishingExchangeKey` | `shop.cpp` | the attachment slots on a `uintptr_t` |
| `EnterWeaponModel`, `WeaponModelBuildFunc`, `DngWeaponEquipModelBuild` | `menu_misc.cpp` | the weapons' files in a table of pointers; retail's `int` table read back as `u_int *` at a 4-byte stride |

Records read from the disc with pointer-sized fields are decoded into host
records rather than used in place: `.pts` part definitions
(`eparts_port.cpp`, for `LoadPTS` in `editloop_parts.cpp` and `GetFuncPoint`
in `edit_in_parts.cpp`; `EdInitToEPInfo`, `editloop3.cpp`, lays its own out
past the host header) and the event scripts' function records and stacks
(`runscript.cpp`). The wind a title scene stores in `CCharacter::wind`, an
`int`, is recovered by `CCharacter::ClothStep` with `PortImagePointer`, which
widens it against the image's own address.

## What is still a stub

`port/src/stubs/sce/` holds the only stubs left: libdma, libgraph (all but
`sceGsSyncV`, which is `port/src/sce/libgraph.cpp`) and libpkt. Each calls
`PS2_UNIMPLEMENTED()` (`port/include/port.h`), which prints the function,
file and line and aborts. They stay stubs by design: the port does not
emulate DMA chains, VIF/GIF packets or the GS, so a call that reaches one
is a drawing path no replacement unit covers yet.

Everything else from the SDK is implemented in `port/src/sce/`: libvu0 in
C++ (static constructors call it before `main`), libpad, libmc, sifdev,
eekernel (`FlushCache` and friends do nothing, `Exit` exits), libcdvd's
`sceCdInit`/`sceCdMmode` and sifrpc's IOP boot and module loads (no-ops).
The Metrowerks runtime calls are in `port/src/runtime.cpp`: `mwInit` and
`LoadOverlay` do nothing, `mwLoadOverlay` succeeds, `__assert` prints and
exits with status 4, `exit__2` exits.

None of the 39 stubbed functions is linked into `darkcloud`, and neither is
`Ps2Unimplemented` itself: `--gc-sections` keeps only what `main` reaches,
and no function it reaches calls a stub. `darkcloud_tests` still links some
through the units the tests call directly. To check after a change,
disassemble `port/build/pc/darkcloud` (`llvm-objdump -d`), collect the functions
with a `call` to a stub's address and map them to their source with
`llvm-addr2line`; static helpers inlined into a caller show under that
caller.

At the last count the final link held 249 `ps2/src` definitions displaced
by a strong one in `port/src` and 3,912 that survive as the game's own
(`nm` of `dc_ps2.o`'s weak definitions against the port's objects and the
executable's symbols).

## Known gaps

- **Rendering approximations.** DATE/DATM (destination alpha test,
  `MakeFukidashi`'s mask) is not emulated. TEX1 LOD (L, K) is ignored in
  favour of standard trilinear filtering. The GS blends that need a factor
  above 1 or a destination scaled past 1 are approximated
  (`port/src/gfx/README.md`, "Blending" and "Not done here").
- **Overlay re-initialisation.** Retail reloads `TITLE.BIN` or `DUN.BIN` and
  re-runs its constructors on every switch; the port links both once and
  runs nothing again but the title objects it lays out with host classes
  (below). Other globals a mode expects fresh keep the previous visit's
  values.
- **Arena headroom** is four times retail's request across the board, a
  stopgap rather than measured peaks.
- **Retail statics of the title units.** The title units' own static
  constructors still build their file-local `CFireOmni`, `CMapObject`,
  `CEffectGroup` and `OBJ_ANIME_SEQ` objects at PS2 sizes with the host
  constructors, which write up to 16 bytes past each. Every such object is
  dead in the port (its users are the port's copies, with their own
  statics), and what the writes reach is another dead title static or
  padding, but the bytes are written.

## How far the game runs

With the PAL data extracted and no input, `darkcloud --headless --frames 120`
boots, compiles the pipelines, runs the 60-tick warm-up and the loading
screen, draws the language select (English highlighted) and exits 0 with
that frame in the screenshot. Scripted (above), it plays the attract movie,
the title screen and its menu, and START opens the opening book. Through the
developer menu, the opening's scenes play, the dungeon loader lists the
seven dungeons, floor 1 of the first starts through its name card and Toan
walks it until the Mayor's event script takes over, and the town `e01`
(Norune) loads its parts and is walked. No stub is reached on the way.

## The title overlay's own class declarations

The title units (`ps2/src/title/*.cpp`) declare other units' classes
themselves, with only the members they touch named and the PS2 extents
padded out, and the PS2 link binds them to the main executable's code. On
the host the main executable's code uses the real classes, with 8-byte
pointers:

| Class | Title units' size | Host size | Declared by |
|---|---|---|---|
| `OBJ_ANIME_SEQ` | 144 | 192 | op_a, op_b, op_c, opening, rushmovi, title |
| `CMap` | 2800 | 3120 | op_a, opening, rushmovi, title, titleloop |
| `CMapObject` | 240 (op_a), 256 | 272 | all but sprite |
| `CObjectFrame` | 176 | 224 | op_b, op_c, op_d, opening, rushmovi, title, titleloop |
| `CWater` | 816 | 848 | op_c, rushmovi, title, titleloop |
| `CFireOmni` | 64 | 80 | op_a, op_b, op_c, rushmovi, title, titleloop |
| `CEffectParam` | 240 | 256 | op_a, op_c |
| `CEffectGroup` | 8 | 16 | op_a, op_c |
| `CategoryAttr` (`CMapCategoryAttr`) | 24 | 24 | op_a, opening |
| `CRunEffect` | 208 | 208 | rushmovi, title, titleloop |
| `CRect<int>` (`CRect_i_`), `RECT` | 16 | 16 | all |
| `SND_INFO` (op_c's own table row, unrelated to `snd.hpp`'s) | 24 | 16 | op_c |

The port defines every shared object of a mismatched class with the real
one: op_a's `OP_GroundMap`, `OP_BuildingMap`, `OP_BuildingMap2`,
`OP_AnimeSeq[32]` and `CFire`, op_b's `OP_NornMapObj[76]` and
`OP_NornMapObj2[87]`, op_c's `Water`, rushmovi's `Water__2` and `CFire__4`.
Every function that indexes or sizes them is the port's, compiled against
the real headers: the long-tail copies of op_a, op_b, op_c, opening and
rushmovi, `port/src/title/title.cpp` (all of title.cpp's scene set-ups and
draws), `opening_mds.cpp` (`OPAnalyz`, `OPMdsLoad` and the definition
reader's state) and op_d's `OpD_InitProcess`, `OpD_InitProcess2` and
`OpD_DrawProcess`, which reach op_d's statics through names its stub header
gives them. The smoke pools are sized from the host `CEffect` (288 bytes,
where retail asked for fifty 256-byte ones). `title_layout_test.cpp` checks
the linked symbols' sizes.

The title units' static constructors still run after the port's, over the
port's objects, at the PS2 strides and through op_a's inline `CMap`
constructor. `LoadOverlay` (`port/src/runtime.cpp`) therefore does what
retail's overlay loader did when a mode needs TITLE.BIN and DUN.BIN (or
nothing) was loaded before: `TitleOverlayConstruct` zeroes those objects and
constructs them again (and initialises the maps, as op_a's constructor did).

## Layout

The root `CMakeLists.txt` only picks the platform:

- `ps2/src` is the game's code, exactly as the PS2 build compiles it, and
  nothing else. Port accommodations never live in `ps2/src` or `ps2/include`;
  the one exception is the `#ifndef PORT` around functions written in
  assembly (below).
  `ps2/CMakeLists.txt` (with `ps2/cmake/`) is the PS2 build.
- `port/src` is code only the port compiles. `port/CMakeLists.txt` is the
  port's build. `main.cpp` and `gameloop.cpp` start the game;
  `platform/`, `gfx/` and `audio/` are the host side; `sce/` implements the
  SDK; `stubs/sce/` holds the stubs left; `<unit>.cpp` (and `dun/`) replace
  functions of `ps2/src/<unit>.cpp`, sometimes split as `<unit>_math.cpp`,
  `<unit>_draw.cpp` or `<unit>_port.cpp`; `linknames.cpp` supplies link-time
  names (below); `tests/` is `darkcloud_tests`, GoogleTest cases that ctest
  runs one process each.
- `tools/dcdata` is the data extraction tool.
- `ps2/include` holds the game's headers and, under `ps2/include/sce` and
  `ps2/include/std`, the SDK and standard headers MWCC compiles against. Both
  builds use them.
- `port/include` holds headers only the port uses: `port.h`, included ahead
  of every unit it compiles, and `stubs/` (below).

## How `port/src` takes precedence

`port/CMakeLists.txt` builds the two halves like this:

1. Every unit in `ps2/src` is compiled as it is, with `PORT` defined.
2. The objects are merged into one relocatable object, `port/build/pc/dc_ps2.o`,
   and `llvm-objcopy --weaken` makes every definition in it weak. Two units
   defining the same strong name fail the merge. References
   stay strong, so a missing function is still a link error.
3. The units in `port/src` are compiled and linked with it. Their definitions
   are strong, so any function or variable `port/src` defines replaces the
   `ps2/src` one. `--gc-sections` then drops the `ps2/src` body.

`ps2/src` units are compiled with `-fPIC -fsemantic-interposition`. That stops
clang from inlining or folding a call to a function `port/src` may replace, so
calls inside a `ps2/src` unit reach the replacement too. `ps2_interposition_check`
(`tools/weaken/interposition_check.py`, part of every build) disassembles
`dc_ps2.o` and fails if a call to a replaced function was bound inside it or
a definition in it is still strong. macOS does the same with other tools
(`docs/MACOS.md`).

To replace a function, define it with the same signature in `port/src`. By
convention it goes in the file that mirrors its unit: `port/src/mglib.cpp`
holds the replacements for `ps2/src/mglib.cpp`. A `static` function cannot be
replaced this way; it keeps the body `ps2/src` gives it.


## Per-unit adjustments

`port/include/stubs/<unit>.hpp`, if it exists, is included ahead of
`ps2/src/<unit>.cpp`, after `port.h`, and nothing else. Only the port's build
sees `port/include`; the PS2 build compiles nothing differently. It declares
what the port defines in place of code `PORT` leaves out, and supplies or
renames what the unit takes from MWCC or from the PS2 link alone:

- `bound`, `cloth`, `collisionmdt`, `frame`, `gameutil`, `mglib`, `visualvu1`
  and `water` get declarations of their `static` assembly functions, which
  the port defines.
- `battlemenu` and `editground` pass each temporary `CRect_i_` as an lvalue
  (`Ps2Lvalue`, `port/include/port.h`): MWCC binds a temporary to the non-const
  references of `DrawMenuColorGradation` and `CEditGround::CheckPartsRect`.
- `editloop3` gives its static `EdSetVillagerNextPos` a global forwarder,
  `PortEdSetVillagerNextPos`, for the port's `EdMoveVillager`, and declares
  its static `EdEventScript` `extern` first, for the port's `EdRunEvent`
  (`port/src/runscript.cpp`): retail's has no return statement and leaves
  `CRunScript::run`'s result in `v0` for `EdEventInit`.
- `main` gets an overload of `LoadFileMenuData` for a `const char *`: one call
  names its file with a comma expression ending in a string literal.
- `mathutil` gets the Metrowerks runtime's own `std::exception` and
  `std::bad_exception` renamed apart from the host library's, and
  `__exception_magic`, which MWCC provides inside an exception handler.
- `menu_save` and `memcard` export statics the other calls: memcard's
  `SaveMenuFunc` table names menu_save's eighteen `SaveMenuKey*` steps and
  menu_save calls memcard's `ExitSaveSelect`. Each header defines a global
  forwarder under the external name (an asm label), which also keeps clang
  from dropping the unused static.
- `title/rushmovi` declares title.cpp's `DataLoad`, `DrawProcA`..`I` and
  `DrawProcTitle` static; its header defines those statics as forwarders to
  the global ones.
- `mapparts` has its `CMapParts::DrawLOD` replaced (`port/src/mapparts.cpp`):
  retail lifts a part off the ground by its own distance from the eye, which
  leaves neighbouring road tiles at different heights and the ground showing
  through the step between them at any resolution above the PS2's; the port
  lifts every part by the height retail gives one 100 units away.
- `title/op_d` declares the state and the helpers `OpD_InitProcess`,
  `OpD_InitProcess2` and `OpD_DrawProcess` share with the rest of the unit
  `extern` under `OpD_*` names before the unit declares them `static`, and
  gives the static helpers global forwarders, so the port's copies of those
  three reach them.
- The per-object renames below.

## Names the PS2 build renames

`main.cpp` calls other units through the names MWCC gives them
(`init_all__Fv`) and calls the overlays' entry points through their retail
addresses (`func_01DAC1C0`). `port/src/main.cpp` forwards each one to the real
function.

The PS2 link binds some names through `ps2/config/pal/object_fixups.json` and the
linker script rather than through the source. The port reproduces each:

- **Per-object renames**, by `#define` in the unit's header: the opening
  scenes' four `FaceChange(int)` become op_b's `FaceChange`, op_c's
  `FaceChangeC`, op_d's `FaceChangeD` and rushmovi's `FaceChangeMovie`, the
  names their neighbours call them by; the dungeon's `MainDraw` and
  `MoveChara` become `DunMainDraw` and `DunMoveChara`, apart from editloop's
  (`port/src/dun/gameloop.cpp` replaces `DunMainDraw`); edit_in's `Chara`,
  `MainCamera`, `NowCamera`, `TalkCamera`, `NowTime`, `TexAnimeData`,
  `camera_dist_mode`, `door_open_cnt`, `fix_chara_pos`, `fix_chara_rot`,
  `goto_menu`, `goto_return_menu`, `key_counter` and `loop_counter`, which it
  redeclares `static` after a header declares them `extern` (MWCC makes them
  file-local, clang's `-fms-extensions` keeps them global), become
  `EditIn_*`.
- **Pooled literals under extern names** (`BtAtraShortCharaFile`,
  `MdsExtension`, `gamemode_empty_string`, `allmenu_mes` and six more) and
  **the title overlay's own `CRect<int>`** spelling of the rectangle in
  `MGFillBox`, `MGMoveImage`, `MGStretchMoveImage`, `MoveImageTest` and the
  `set2DSprite` overloads, plus op_c's `CWater::DrawVu1` and main's
  `MAP_NPC_MODEL::operator=`: weak definitions and forwarders in
  `port/src/linknames.cpp`.
- **Aliases**, by `--defsym` (ld64's `-alias` on macOS) in
  `port/CMakeLists.txt`: `ItemPutListTbl12_bytes` = `ItemPutListTbl12`,
  `draw_rect` = `draw_rect_store` and `WorkBuffer__2` = `WorkBuffer`.
  `EditGaijiTbl` is `GaijiDataTbl + 0x601C` on the PS2 link. The linker
  script places it inside `EditPartsData`, but the codes `clsmes.cpp`
  indexes it with (-0x300 and up) only ever land on the last word of a
  `GaijiDataTbl` entry, and `GaijiDataTbl` keeps its layout on the host
  where `EditPartsData`'s pointers grow. ld64 cannot alias with an offset,
  so `linknames.cpp` defines it on every platform: storage for the 0x300
  entries below it, filled from `GaijiDataTbl` before `main`, with
  `EditGaijiTbl` an assembler alias of the storage's end.

Data the title overlay's units type themselves with PS2 layouts: see "The
title overlay's own class declarations".

## Game headers

`port/include/port.h` adjusts two game headers for the host, from outside:

- `types.h` defines the PS2's `size_t` and `NULL`. `port.h` includes it with
  `size_t` renamed and `NULL` saved, so the host's stay in force, and
  `#pragma once` keeps the game from including it again.
- `common.h` defines `STATIC_ASSERT`, which checks the PS2's layouts. `port.h`
  includes it and redefines the macro to check nothing.


## Keeping the PS2 build matching

Edits to `ps2/src` or `ps2/include` that help clang must leave both PS2 builds
byte-identical. They are corrections that are standard C++ either way, never
code for the port:

- **Initialisations a `switch` jumps over.** They are split into a declaration
  and an assignment.
- **Language linkage.** `BtEnemyLayoutList` and `BtUraEnemyLayoutList` are
  declared `extern "C"` to match their definitions.
- **Exception specifications.** `__dl` is declared `throw()`, as it is
  defined.
- **Sizes spelled as `sizeof`.** `MotionParam::storage`, where a character
  builds each motion set beyond its first, is `sizeof(tagMOTION_TYPE)` bytes
  rather than 128, and `CCharacter::Initialize` clears it by its `sizeof`.
  Both are 0x80 on the PS2; on the host a `tagMOTION_TYPE` is 0xB0, and with
  128 bytes a set's `frame_info` and `motion_info` lay in the next set's
  storage (`bits64_motion_storage_test.cpp`).
- **`#ifndef PORT` around assembly functions**, the one exception: clang
  cannot parse them. It replaces blank lines, so no line number moves. The
  generic `CDataAlloc<Kind, Size>::Align64()` in `ps2/include/dataalloc.hpp`
  is guarded too; nothing instantiates it, since both arenas specialise it.

Retail's own mistakes stay in `ps2/src`, because the match reproduces them:
locals read before anything sets them (`SaveToMc`'s `status`, `main`'s
`idle_result` and six more), non-void functions that fall off the end,
format strings that do not fit their arguments. Initialising any of the
locals changes what MWCC emits. The port gives them defined behaviour
instead:

- `-ftrivial-auto-var-init=zero`: locals start at zero.
- `-fno-strict-return`: falling off the end returns an unspecified value
  instead of being undefined.
- `-fno-strict-aliasing` and `-fwrapv`: the type punning and wraparound the
  code assumes.

`ps2/src` is compiled with `-Wall`. The warnings left on are the porting
work: uninitialised reads, missing returns, format strings, copies over
objects with a vtable and the like. Those that only describe how MWCC-era
code is spelled -- string literals as `char *`, MWCC's pragmas, 32-bit pointer
casts, the unused names and expressions matching leaves behind -- are off.

Clang also needs a few flags to accept code MWCC accepts:

- `-fms-extensions`: pointers truncated to `int`.
- `-Wno-c++11-narrowing`: narrowing in braced initialisers.
- `-Wno-register`: the `register` keyword.
- `-Wno-return-mismatch`: a bare `return;` in a non-void function.

The casts themselves compile; where one's value comes back as a pointer,
the function is the port's ("Pointers and 32-bit integers").

## Checking the PS2 build

An edit to `ps2/src` or `ps2/include` made for the port is checked by building
both regions (`scripts/build/cmake.sh build`, and again with `REGION=PAL`),
which verifies every image byte for byte.

## Game data

The game reads its files from a plain directory, not from the disc.
`port/build/pc/dcdata` (`tools/dcdata`) makes it:

```sh
port/build/pc/dcdata extract "rom/Dark Cloud (PAL).iso" data
port/build/pc/dcdata list "rom/Dark Cloud (PAL).iso"
```

The source is a disc image, read through its ISO 9660 tree, or a directory
that holds `DATA.DAT` and `DATA.HD2` (such as `rom/pal/extracted/iso`).
`extract` writes every file `DATA.HD2` indexes to `<data>/<path>`, the path
lowercased with `/` separators and no leading separator, at its exact size,
and copies the index itself to `<data>/data.hd2`. A file already present at
its size is kept, so a rerun only repairs what is missing. When a path is
listed twice, only the first is written, as the game only ever finds the
first. `list` prints each file's sector, size and path.

The layout of `data/` is the archive's own: `dun/pack/maindat.pac`,
`commenu/a_eng/savetex.pak`, `sound/bgm/...` and so on. Lookups fold case,
as the game's `strcasecmp` does, so the case on disk does not matter.

`port/src/platform/paths.cpp` finds the data directory from `--data <dir>`,
then `DC_DATA`, then `data/` in the working directory, then `data/` beside
the executable, and failing all of those `$XDG_DATA_HOME/chronicle/data`
(`~/.local/share/chronicle/data` when `XDG_DATA_HOME` is unset or relative),
where an installed copy keeps it. The save directory comes from `--save`,
`DC_SAVE`, or `save/` in the same two places; without one, saves go beside a
local `data/` if there is one, so a checkout or an unpacked copy stays
self-contained, and otherwise to `$XDG_DATA_HOME/chronicle/save`. It is
created when first used.

When the data directory is missing or holds no file and the run is not
headless, `FirstRunIfNoData` (`port/src/platform/firstrun.cpp`) opens the
window and asks for the disc: a disc image through SDL's file dialog (the
file-chooser portal under Flatpak, zenity elsewhere), or a folder holding
`DATA.DAT` and `DATA.HD2`. It extracts with `dcdata`'s core on a worker
thread into `<data>.partial`, drawing the file and byte counts through
`gfx::Draw2D` while the main thread pumps events, checks every file and
renames the directory into place, then closes its window and `main` goes
on. Escape or closing the window stops it (a later start resumes from the
partial directory); a failed extraction shows the extractor's message in a
message box and exits with status 1. `FirstRunSetChooser` replaces the
dialog, which is how the tests drive the flow headless. Declined, or
headless, `main` stops with status 3 and one line naming the directory and
the `dcdata` command, before anything else starts; `InitCDFile` would abort
on the same conditions. `InitCDFile` warns
when `data.hd2` lists a file that is missing or the wrong size. Files the
game looks for and does not find behave as on the disc: `LoadFile2` returns
0 and `LoadFile` asserts, naming the file (status 4).

## Packaging

`cmake --install port/build/pc --prefix <dir>` installs `darkcloud` and `dcdata`
to `<dir>/bin`; an installed copy finds its data and saves under
`$XDG_DATA_HOME/chronicle` (above). The Linux release is a Flatpak,
`org.themoonpeople.Chronicle`, built from `port/flatpak/` by
`.github/workflows/flatpak.yml`; `docs/FLATPAK.md` covers building,
installing, the first start and where the data and saves live.
