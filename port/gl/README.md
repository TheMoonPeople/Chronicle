# gl

Kazarov's renderer (`tools/kazarov/port/gfx`, its README tells the API and its conventions) redone over
OpenGL 4.3 core, for the Switch's Mesa: devkitPro's `switch-mesa` (nouveau GL on the Tegra X1),
through SDL3's EGL. `DC_RENDERER=gl` builds it in place of Kazarov's Vulkan units on any platform:
the `switch-gl` preset for the console, `linux-x64-gl-release` on a PC, where it runs against the
PC's own GL driver so its pictures can be compared with the Vulkan renderer's.

It keeps what is not Vulkan's: the display lists, their interpolation and replay
(`displaylist.cpp`) and TIM2 decoding (`tim2.cpp`) are Kazarov's own units, and they read the
renderer's state through `context.hpp`'s `Context` as with Vulkan. The rest follows Kazarov's units
one for one:

| Unit | Kazarov's | What it holds |
|---|---|---|
| `renderer.cpp` | `renderer.cpp`, `readback.cpp` | the context, the main targets, frames, canonical and display renders, the present, depth queries, readback |
| `resources.cpp` | `resources.cpp` | textures, palettes, render targets, meshes, palette expansions |
| `draw.cpp` | `draw.cpp` | the mappings, scissors, full-frame flanks and covers, UI anchoring, draws, clears, copies |
| `shaders.cpp` | `pipelines.cpp` | programs from Kazarov's shaders, the GS blend table |
| `stream.cpp` | `memory.cpp` | the streaming buffer for per-draw data |
| `loader.cpp` | | GL's functions through `SDL_GL_GetProcAddress` |

## Conventions

- **Rows.** GL and Vulkan put the same things in the same memory: clip y = -1 is row 0 of a target in
  both, and texture coordinate 0 samples row 0, so targets and textures are drawn and read with
  Vulkan's conventions. Only the window is the other way up; the present's blit turns it over.
  Counter-clockwise faces on the target, y down, are GL's clockwise (`glFrontFace(GL_CW)`).
- **Depth** is Vulkan's: `glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)`, D32F with 8 bits of
  stencil, reverse-Z cleared to 0.
- **Shaders** are Kazarov's GLSL, embedded as text (`gl.cmake` writes `gl_shaders.hpp`) and rewritten
  when the renderer starts: the version, the specialisation constants (one program per texture mode,
  alpha test and vertex shader), push constants and descriptor sets as uniform blocks at bindings
  0 to 2, and the bindless texture array as a texture at unit 0 and the palette at unit 1. Each
  rewrite must match, so an upstream change to those declarations stops at start-up with its name.
- **Blending** is Kazarov's table (its README, "Blending") with GL's factors and equations; the
  dual-source second output is GL's `index = 1`.
- **Per-draw data** (the push block, mesh constants, Vu_prog0 blocks, 2D vertices, immediate meshes,
  quad indices) goes into one persistently mapped buffer of three segments, a frame's each, fenced
  when the frame ends. Without `ARB_buffer_storage` a segment is staged and uploaded with
  `glBufferSubData`.
- **Depth queries** are read into a pixel pack buffer as a frame closes. On a PC the frame waits for
  them, as the Vulkan renderer does; on the Switch they are harvested as the next frame closes, so
  the CPU does not wait for the GPU, and `DepthResult` answers a tick later.
- **Present modes**: FIFO and Mailbox are swap interval 1, Immediate 0.

## Not done here

- Program binaries are not cached; the twelve programs compile at every start.
- Under Ryujinx `glReadPixels` reads back only the first quarter of each row (the emulator's copy,
  not the frame: the window shows the whole picture), so `--screenshot` there is not to be trusted.
