# gfx

The port's Vulkan renderer (1.3 or later, see "Device"). `gfx.hpp` is the whole public API (namespace `gfx`); everything
else is internal. No game header is reachable from here: `platform/`, `gfx/` and `audio/` build as
`dc_host`, without `port.h` or the game's include paths. Replacement units include
`"gfx/gfx.hpp"`.

## Lifecycle

- `RendererInit(window, config)` creates the device, the swapchain, the main targets and every
  pipeline (below). `config.pipeline_cache` defaults to `save/pipeline_cache.bin`;
  `config.progress(done, total)` is called on the calling thread while pipelines compile.
- `BeginFrame()` / `EndFrame()` bracket a frame drawn as it is called. Two frames are in flight.
  `BeginFrame` returns false when there is nothing to draw to (minimised); draws are then dropped and
  `EndFrame` does nothing. Every frame starts on the main target. `EndFrame` presents. The game's
  ticks are recorded instead and rendered from their display lists (below).
- `config.present_mode`: FIFO, Mailbox (FIFO where unsupported) or Immediate (Mailbox, then FIFO,
  where unsupported).
- `config.offscreen` renders without a surface or swapchain: each frame is drawn to the main target
  alone, at the window's pixel size, and `EndFrame` submits it and presents nothing. `ReadbackFrame`,
  `SnapshotFrame`, `kPreviousFrame`, depth queries and resizes behave as with a swapchain, so a
  headless run reads back the same pixels. `darkcloud --headless` uses it when the Vulkan loader has
  no `VK_EXT_headless_surface` (SDL's offscreen driver cannot then make a Vulkan window), and
  `--offscreen` forces it.
- `RendererResize()` after the window's pixel size changes (`WindowPollEvents` does it); the
  swapchain and main target follow at the next `BeginFrame`. Main-target contents are lost.
- Single-threaded: call everything from one thread.
- Textures, palettes, meshes and render targets can be created, updated and destroyed at any time,
  inside or outside a frame. Destruction is deferred until the GPU is done with them; a handle is
  dead as soon as it is destroyed (handles carry a generation).

## Spaces

- **Logical space** is the game's 640x480, y down. `Draw2D` vertices, `DrawState::scissor_rect`,
  `Clear` rects, `ReadDepth` rects and copy rects on `kMainTarget`/`kPreviousFrame` are in it.
- On the **main target** (window pixel size) logical space is scaled by
  `min(W / 640, H / 480)` and centred with whole-pixel offsets, so the 640x480 frame always fits
  the window. Coordinates outside 0..640 x 0..480 reach past the frame; nothing clips to the 4:3
  area unless a scissor says so. `GetLogicalMapping(target)` returns
  `pixel = logical * scale + offset`. What the window shows past the frame depends on the frame
  layout ("Aspect").
- A **render target** is `logical_width x logical_height` scaled by the render scale (pixels per
  logical texel; `config.render_scale`, 0 = `round(window height / 480)`, at least 1), offset 0.
  `SetRenderScale` recreates every render target, blitting its contents over.
- A render target created with **`share_main_depth`** (`CreateRenderTarget`, `NamedRenderTarget`)
  has no depth buffer of its own: drawing into it tests and writes the main target's depth and
  stencil as drawn so far this frame, which is how the GS draws into another FRAME_1 with the same
  ZBUF (shadow volumes counted into `shadow_buf` against the scene). Its pixels must line up with
  the main target's, so it is the main target's pixel size and mapped like it (640x480 logical,
  letterboxed) whatever size is asked for, ignores the render scale, and is recreated with the
  main target on resize (contents lost). `Clear` of depth or `ClearStencil` while it is the target
  clears the main target's.
- **Texture coordinates** in `Draw2D` are logical texels of the texture (its `TextureDesc` size,
  or its logical size for render targets, 640x480 for `kPreviousFrame`). In `Vertex3D` they are
  normalised.
- **Meshes** draw over the whole target (not letterboxed). With `kMeshScreenUv` a vertex samples
  an image of the target where it lands itself, `Vertex3D::uv` an offset from there. `MeshConstants::mvp` (column-major)
  maps to Vulkan clip space: x right, y down, z/w in [0, 1] with **near at 1** (reverse-Z).
  Front faces are counter-clockwise as seen on the target with y down.
- **Depth** is D32 float with an 8-bit stencil (`D32_SFLOAT_S8_UINT`; `D24_UNORM_S8_UINT` where
  the device cannot attach the former), reverse-Z: cleared to 0 (far), tests are GEQUAL/GREATER,
  so the game's ZTST values map one to one. A `Vertex2D::z` is the same depth (1 near), so 3D sprites can test
  against meshes when the projection agrees. `ReadDepth` returns these units.

## Aspect

`RendererConfig::layout` (`SetFrameLayout` outside a frame) is a `FrameLayout`: the aspect mode and
`ui_scale`. The logical frame is centred in the window in both modes.

- **`AspectMode::Fill`** (`video.aspect` `auto`). The window past the frame is the game's too:
  - Meshes draw through the target's mapping, so the frame's 480 rows keep their place and a wider
    window shows more of the world at the sides at retail's vertical field of view (a narrower one
    keeps the frame's width and shows more above and below). `VisibleLogicalRect(target)` is the
    logical rect the target's pixels cover; the game's culls test against it.
  - **The full-frame rule.** Along each axis on which the target shows past its frame (both on the
    main target and the targets mapped like it, x on a frame target), a scissor or clear rect, and
    every axis-aligned rectangle of a `Quads` or four-vertex strip `Draw2D` whose texture is none, an
    image of the frame (`kPreviousFrame`, a frame target, a target sharing the main depth) or
    another image that shows past its frame, is carried to the target's edge on every side where it
    reaches the frame's edge from inside (its span crosses or ends on the edge, starting inside).
    The rectangle itself is drawn as given; flanking quads cover the rest, with texture coordinates
    continued (so an image of the frame shows its own sides) and colour, fog and depth held at the
    edge. Fades, however tiled, full-frame fills and bands, the previous-frame feedback and frame
    grabs drawn back therefore reach the window's edges; HUD pieces (textured from ordinary
    textures) and anything wholly outside the frame do not grow. Nothing of a host list is carried:
    a display render's `options.overlay`, and the list itself with `options.host` (the FPS counter,
    whose glyph runs would otherwise streak to the window's edge wherever one lies across the
    frame's).
    A copy or blit between two images that show past their frames, whose rects cover both frames
    along an axis, copies them edge to edge along it.
  - **Frame targets** (`CreateRenderTarget(..., frame_target = true)`; the game's `frame_*` grabs)
    are the main target's pixel width with their logical width laid over its logical frame, and
    rows at the main target's scale, so a grab of the frame keeps its sides. Other render targets
    keep their logical size at the render scale.
- **`UiAnchor`** (`SetUiAnchor`, `UiAnchorScope`; the default anchors nothing). While one is set,
  each `Draw2D` the UI mapping places on the main target is classified by the centre of its bounds
  against the anchor's four limits and moved, whole and unscaled, by the pixels between the UI
  frame's edge and the edge of what the target shows on each side it falls to: a piece 40 units
  from the frame's left edge lands 40 units from the window's. `UiAnchor::Side` sends every draw
  to one side or corner (an element made of many draws, the mini map); limits inside the frame
  split a panel whose pieces belong to different corners. The side is chosen when the draw is
  made and travels with it in a display list; the shift is taken at render time, so it follows
  the window and `ui_scale` (a HUD scaled past the window is pulled back to its edges). An
  anchored draw is never flanked by the full-frame rule and its scissor stays the frame's. With
  `AspectMode::Letterbox` at `ui_scale` 1 the shift is zero.
- **`AspectMode::Letterbox`** (`aspect = 4:3`). Nothing is carried past the frame and frame targets
  are ordinary render targets: the letterboxed output, byte for byte.
- **`ui_scale`** (default 1) scales 2D that tests no depth, writes none and samples no image of the
  frame about the main target's centre (`GetUiMapping`); 2D with depth (3D sprites) and images of
  the frame stay on the logical mapping, among the meshes. It does not know which 2D the game
  placed from a 3D projection (lock-on corners, name tags): at a scale other than 1 those move with
  the HUD.

## Colour and alpha units

| Quantity | Units |
|---|---|
| Colour in textures, palettes, targets, `Clear` | bytes as the GS stores them |
| Alpha in textures, palettes, targets | **0xFF is GS 0x80** (saturating). Upload GS alpha through `ConvertPs2Alpha`. |
| Vertex colour and alpha (`Vertex2D`, `Vertex3D`) | GS bytes |
| `Clear` alpha | GS byte (0x80 opaque) |
| `MeshConstants` colours | modulation, 1.0 = GS 0x80 |
| `DrawState::alpha_ref`, `texa_ta0`, `GsBlend::fix` | GS bytes |

- Untextured, the vertex colour is written as is (0x80 is mid grey). Textured, MODULATE:
  `Cv = Ct * Cf / 0x80`, `Av = At * Af / 0x80`, so 0x80 is 1.0 and 0xFF nearly doubles.
- Mesh lighting: `colour = ambient * ambient_material + sum(max(0, n . -light_direction[i]) *
  light_color[i]) * diffuse`, alpha `diffuse.w`, times the vertex colour / 0x80 with
  `kMeshVertexColor`, saturated at 0xFF as VU1 saturates. Without `kMeshLit` it is `diffuse`
  (times the vertex colour). `kMeshShadow` is flat `diffuse`. `specular` is carried, unused.
- **TEXA**: a texture with `has_alpha = false` (24-bit TIM2, a 24-bit render target) gets
  `alpha = (texa_aem && rgb == 0) ? 0 : texa_ta0` per texel, before filtering.
- **Palettes**: an `Index8` texture is looked up in its palette before filtering, so a linear
  filter blends four palette entries (base level only). Palettes are 256x1 RGBA8 textures;
  swapping one between two draws of a frame is ordered as written.
- **Fog**: `Cv = Cs * F + FOGCOL * (1 - F)`, F per vertex (`Vertex2D::fog`; for meshes
  `clamp(fog[0] + fog[1] / w, fog[2], fog[3]) / 255` with `kMeshFog`), applied when
  `DrawState::fog`.
- **Alpha test** compares `Av` in GS units (rounded) with `alpha_ref` by ATST. AFAIL is KEEP
  (the only value the game uses).
- A **target without alpha** (`has_alpha = false`) stores alpha 0x80 whatever is drawn, so its
  destination alpha reads 0x80, as the GS reads it for PSMCT24.

## Blending

GS `((A - B) * C >> 7) + D`, A/B/D in {Cs, Cd, 0}, C in {As, Ad, FIX}. The fragment shader writes
the source colour and, on the second dual-source output, C when it can compute it (As or
FIX / 0x80). Vulkan then computes `src * Fs (op) dst * Fd`. `F` below is `SRC1_ALPHA` for As and
FIX, `DST_ALPHA` for Ad; `1-F` its complement. All 81 combinations reduce to 28 Vulkan blend
states.

| A | B | D | GS result | Fs, Fd, op | shader writes | exact |
|---|---|---|---|---|---|---|
| = | = | Cs | Cs | blend off | Cs | yes |
| = | = | Cd | Cd | 0, 1, add | | yes |
| = | = | 0 | 0 | 0, 0, add | | yes |
| Cs | Cd | Cd | Cs c + Cd (1 - c) | F, 1-F, add | Cs | yes |
| Cs | Cd | 0 | (Cs - Cd) c | F, F, sub | Cs | yes |
| Cs | Cd | Cs | Cs (1 + c) - Cd c | 1, F, sub | Cs (1 + c) | no, see below |
| Cd | Cs | Cs | Cd c + Cs (1 - c) | 1-F, F, add | Cs | yes |
| Cd | Cs | 0 | (Cd - Cs) c | F, F, rev-sub | Cs | yes |
| Cd | Cs | Cd | Cd (1 + c) - Cs c | F, 1, rev-sub | Cs | no: Cd - Cs c |
| Cs | 0 | Cd | Cd + Cs c | F, 1, add | Cs | yes |
| Cs | 0 | 0 | Cs c | F, 0, add | Cs | yes |
| Cs | 0 | Cs | Cs (1 + c) | 1, 0, add | Cs (1 + c) | As/FIX yes; Ad: Cs |
| Cd | 0 | Cs | Cs + Cd c | 1, F, add | Cs | yes |
| Cd | 0 | 0 | Cd c | 0, F, add | Cs | yes |
| Cd | 0 | Cd | Cd (1 + c) | DST_COLOR, 1, add | c | As/FIX yes; Ad: Cd |
| 0 | Cs | Cd | Cd - Cs c | F, 1, rev-sub | Cs | yes |
| 0 | Cs | Cs | Cs (1 - c) | 1-F, 0, add | Cs | yes |
| 0 | Cs | 0 | 0 (saturated) | 0, 0, add | | yes |
| 0 | Cd | Cs | Cs - Cd c | 1, F, sub | Cs | yes |
| 0 | Cd | Cd | Cd (1 - c) | 0, 1-F, add | Cs | yes |
| 0 | Cd | 0 | 0 (saturated) | 0, 0, add | | yes |

Where it is not exact, and why:

- **C above 0x80.** Blend factors and shader outputs to a UNORM attachment saturate at 1.0, so
  As, Ad and FIX above 0x80 blend as 0x80. The alpha test still sees the unsaturated value. The
  game's alpha values are 0x80 or below except in rare overbright effects.
- **Cs (1 + c) - Cd c** (A=Cs, B=Cd, D=Cs). The shader scales the source and the blend subtracts;
  the scaled source saturates before the subtraction rather than after. With C = Ad the source
  cannot be scaled and the result is `Cs - Cd Ad`.
- **Cd (1 + c) - Cs c** (A=Cd, B=Cs, D=Cd). No destination factor exceeds 1; it becomes
  `Cd - Cs c`.
- **D = A, B = 0 with C = Ad.** `Cs (1 + Ad)` and `Cd (1 + Ad)` need a factor above 1 that the
  shader cannot know; they become Cs and Cd.
- The destination alpha is the stored alpha, which is the last source alpha written (the GS does
  not blend alpha either), saturated at 0x80.

## Stencil and colour writes

| `DrawState` field | Meaning |
|---|---|
| `stencil_test` | stencil test on; off, the stencil is neither tested nor written |
| `stencil_front`, `stencil_back` | per face (by winding, as culling tells them; lines are front): `compare` (Vulkan's order), `fail`, `pass`, `depth_fail` ops, `reference`, `compare_mask`, `write_mask` |
| `color_write_mask` | `ColorWriteBits`; 0 writes no colour (a stencil or depth-only pass) |

`ClearStencil(value, rect)` clears the current target's stencil (the main one for a target that
shares it). Every frame's stencil keeps what the last frame left until cleared. The GS has no
stencil; it is here for techniques the GS did with destination alpha (DATE) or colour counting.

All stencil state is core dynamic state (Vulkan 1.3's `STENCIL_TEST_ENABLE` and `STENCIL_OP`, 1.0's
masks and reference), so it adds no pipelines. The colour write mask is dynamic through
`VK_EXT_extended_dynamic_state3` (`extendedDynamicState3ColorWriteMask`) where the device has it;
otherwise a colourless pipeline per family, texture mode and alpha test (18) is precompiled, and a
partial mask writes every channel. `RendererConfig::dynamic_color_write_mask = false` forces that
path.

## Ordering

Everything happens in the order it was called. An update or copy into a texture or mesh the
frame's draws have not touched yet goes into a batch that runs before the frame's draws, keeping
the render pass whole; one into something already drawn with this frame splits the pass and runs
in place. Copies and blits inside a frame always run in place. Sampling the target being drawn
to is refused: snapshot it first (`SnapshotFrame`, `CopyTexture` from `kMainTarget`).

## Pipelines

3 families (2D triangles, 2D lines, meshes) x 28 blend states x 3 texture modes (none, RGBA,
index + palette) x alpha test on/off = 504 pipelines (522 without a dynamic colour write mask,
above), all created in `RendererInit` on up to eight threads against the pipeline cache. Depth
test, depth write, compare op, the whole stencil state, cull mode, front face, viewport, scissor
and topology within a class are dynamic state, so they add none. The cache file is checked against the device before use and written through a temporary
file and a rename.

## Readback

- `ReadDepth(id, rect)` (16 ids) reads the main depth buffer at `EndFrame` (or at the end of a
  list's canonical render), at most 64x64 pixels
  around the rect's centre, and keeps the farthest (smallest) value: the game's pick-Z keeps the
  farthest of an 8x8 block. `EndFrame` waits for that frame when a query was made, so
  `DepthResult(id)` holds the answer as soon as `EndFrame` returns.
- `ReadbackFrame` (the last presented frame: a display render, else the newest main image) and
  `ReadbackTexture` are synchronous and only allowed outside a frame or recording. `WritePng`
  writes an RGB PNG with stored (uncompressed) deflate.

## Display lists

The game runs at a fixed tick rate; the window is presented at its own. `BeginRecording()` /
`EndRecording()` bracket one tick and return its `DisplayList`; `RenderList(list, alpha, options)`
renders one.

- **Recording.** Between the two, `Draw2D`, `DrawMesh`, `DrawMeshImmediate`, `Clear`, `ClearStencil`
  and `SetRenderTarget` (draw state travels with each draw) are appended to the list, and so are the
  stateful calls: `CopyTexture`, `BlitTexture`, `SnapshotFrame`, `UpdateTexture`, `UpdatePalette`,
  `UpdateMeshVertices`, `ReadDepth`. Arguments are validated and copied at once, so each call returns
  what it would have, and `CurrentRenderTarget`, `GetTextureInfo` and `GetLogicalMapping` answer as
  inside a frame. Creating a texture, mesh or render target happens at once (the handle is needed);
  `DestroyTexture` and `DestroyMesh` make the handle dead to every call but the list's own replays
  and destroy the resource when the list is released. `InFrame()` is true while recording. Outside
  a recording everything runs immediately, as before (`BeginFrame` / `EndFrame`, tests, the loading
  screen).
- **Canonical render** (`options.canonical`): every entry runs, in order, into the next of the two
  main colour images, which then becomes what `kPreviousFrame` samples and what a copy or snapshot
  of `kMainTarget` reads; depth queries are answered from it before `RenderList` returns. It is the
  frame `BeginFrame` / `EndFrame` would have drawn, pixel for pixel, and nothing is presented.
  `PresentCanonical()` presents it as it is.
- **Display render**: only the drawing entries run, into a third colour image with its own depth
  buffer (the window's size). There `kMainTarget` is that image, `kPreviousFrame` the image the
  tick's canonical render sampled, and a target sharing the main depth shares that image's. Render
  targets are drawn again (the shadow volumes follow the interpolated models); textures the stateful
  entries wrote (texture animation, palette swaps, uploads) hold what the canonical
  render left. A grab of the frame is the exception: a copy or blit out of `kMainTarget`, or out
  of a texture grabbed into earlier in the list, runs again from the display image into a twin of
  its destination (the destination's contents with the grab laid over them), and the draws and
  target changes that follow use the twin, so what is drawn back from the grab (the depth of
  field's blurred bands, the water) lines up with the interpolated scene. The destination itself
  keeps the canonical grab. With `options.present` it is presented and becomes what `ReadbackFrame` reads. A list
  that draws to the main target before clearing its logical frame starts from the canonical image.
  Display renders are refused once an immediate frame has followed the canonical render (the
  loading screen took the window). `options.overlay` names a second list whose drawing entries run
  after the first's, uninterpolated, on `kMainTarget`, before the present: the host's FPS counter,
  which so never reaches a canonical image.
- **Interpolation.** A mesh draw may carry a `MeshTransform` (projection, view, middle, model,
  local: `mvp` is their product) and is tagged with the current `InterpKey` (`SetInterpKey`). A
  display render at `alpha` < 1 with `options.previous` matches each tagged draw with the previous
  list's draw of the same key and occurrence (below for a key drawn at several places), and draws
  the model interpolated: translation lerped,
  rotation slerped (from the Gram-Schmidt rotation of the 3x3), the remaining scale and shear lerped
  in that rotation's frame; `normal_matrix` turns with the rotation. Views are numbered by first
  appearance in a list and the n-th is interpolated with the previous list's n-th as a placed
  camera (inverse, interpolate, inverse), for every draw that carries a transform, tagged or not.
  `mvp` is rebuilt only where the model or the view changed, so a still scene replays its recorded
  constants bit for bit. Not interpolated: draws without a transform or without a match, those tagged
  `no_interpolation` or whose model moved further than the tag's teleport distance, every draw of a
  list after `CutInterpolation()`, the views after `CutCameraInterpolation()`, non-affine or
  singular matrices, and 2D that tests no depth: they replay as recorded. 2D that tests depth (a
  3D sprite: a fire, its glow) is recorded as the screen corners the tick's camera gave it, so it
  is carried to the interpolated camera instead: each corner goes back through the projection and
  view of the last mesh drawn on its target to where it stood in the world, and is seen again from
  the interpolated view. Without that a torch's flame stands a tick ahead of its torch in every
  display frame while the camera moves. The sprite's own movement is not interpolated.
  Vertex animation (skinning written with `UpdateMeshVertices`) is interpolated too: the rewrite a
  matched draw's mesh took this tick is blended with the one its match's mesh took in the previous
  tick (the game keeps a mesh per display buffer, so the two differ), positions lerped and normals
  lerped and normalised, unless a vertex moved further than the draw's teleport distance. The mesh
  holds the tick's pose again when the display render ends. Left in the tick's pose, a skinned body
  stood ahead of what hangs from it by matrices or vertices of its own (a weapon, the cloth) by
  what was left of the tick, every display frame.
  A matched immediate mesh (`DrawMeshImmediate`) tagged `blend_vertices` in both ticks, with as
  many vertices as its predecessor, has them interpolated instead: positions lerped, normals lerped
  and normalised, unless one moved further than the teleport distance. The cloth asks for it: its
  vertices are the same grid points in world space, under an identity model, every tick. The shadow
  volumes must not: they are rebuilt in each tick's eye space, so a vertex of one tick is not the
  same point as that vertex of the next.
- **Instanced keys.** A key drawn with more than one model in a tick is one object placed several
  times (a town's parts and tiles share their frames), and culling changes which places are drawn,
  so occurrences do not line up from tick to tick. Such a key, from the tick it is first seen so
  until the next `CutInterpolation()`, matches by place instead: each draw with the previous draw
  whose model is identical, and what is left with the nearest previous model that nothing stood
  still at, no further than 25 units away. A place that comes into view therefore starts where it
  is instead of sliding in from a neighbour's.
- **Faces that depend on the view.** A display render replays a draw under a camera the tick did
  not have, so anything the recorder decided from the tick's eye can be wrong in it. What turns
  towards or away from the eye must be left to `DrawState::cull`: the shadow volumes are recorded
  wound outwards and counted by a back-culled adding draw and a front-culled subtracting draw.

## Device

`RendererInit` takes the best device that has everything below (a discrete GPU over an integrated
one over the rest, and Vulkan 1.4 over 1.3 within a kind) and prints, for every device it passes
over, each requirement that device lacks. `requirements.cpp` holds the check as a pure function of
the device's capabilities.

Nothing the renderer calls was introduced by Vulkan 1.4, so 1.3 is the floor:

| Requirement | Introduced by |
|---|---|
| `dualSrcBlend`, `shaderClipDistance`, `shaderSampledImageArrayDynamicIndexing` | 1.0 features |
| `vkGetPhysicalDeviceFeatures2`, `vkGetPhysicalDeviceProperties2` | 1.1 |
| `descriptorBindingPartiallyBound`, `descriptorBindingSampledImageUpdateAfterBind`, `descriptorBindingUpdateUnusedWhilePending`; 8192 update-after-bind sampled images per stage and per set, and that many update-after-bind resources per stage | 1.2 (descriptor indexing) |
| `dynamicRendering` (`vkCmdBeginRendering`, `VkPipelineRenderingCreateInfo`) | 1.3 feature |
| `synchronization2` (`vkCmdPipelineBarrier2`, `vkQueueSubmit2`, the `*_2` stages and accesses) | 1.3 feature |
| Cull mode, front face, topology, depth test/write/compare, stencil test and ops as dynamic state | 1.3 core (no feature bit) |
| `D32_SFLOAT_S8_UINT` or `D24_UNORM_S8_UINT` as an attachment that copies both ways | format support |
| `maxPushConstantsSize` of 56 bytes | 1.0 limit (128 guaranteed) |
| `VK_KHR_swapchain` and a graphics queue that presents to the window | not when offscreen |

Optional, used when present:

- `VK_EXT_extended_dynamic_state3` with `extendedDynamicState3ColorWriteMask` (below); without it
  the colourless pipelines are added. Chosen by the device's capability, and
  `config.dynamic_color_write_mask = false` forces the fallback.
- `VK_KHR_portability_enumeration` (instance), enabled with
  `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` whenever the loader has it, so drivers that
  implement Vulkan on Metal (MoltenVK) are enumerated.
- `VK_KHR_portability_subset` (device), enabled whenever offered, as the spec requires. Of its
  restrictions the renderer meets two: without `triangleFans` a `Primitive::TriangleFan` is drawn as
  an indexed list in Vulkan's fan order (identical pixels; `config.triangle_fans = false` forces
  it), and without `separateStencilMaskRef` both faces take one reference and compare and write
  masks (the face culling keeps; the front's when both are drawn, with one warning if they differ;
  `config.separate_stencil_masks = false` forces it). The vertex strides (28 and 36 bytes) must be
  multiples of `minVertexInputBindingStrideAlignment`. Point polygons, events, format swizzles and
  reinterpretation, constant-alpha blend factors, mip LOD bias and comparison samplers are not used.
- `VK_EXT_debug_utils` and `VK_LAYER_KHRONOS_validation` for validation.

Shaders are compiled for `--target-env vulkan1.4`, which emits SPIR-V 1.6 with no capability beyond
what 1.3 supports (`spirv-val --target-env vulkan1.3` accepts all three).

## Not done here

- DATE/DATM (destination alpha test; `clsmes.cpp` MakeFukidashi) is not emulated; a stencil pass
  marking the pixels whose alpha passes, then a test against it, is the way.
- GS mip LOD (TEX1 L and K) is ignored; filtering is standard trilinear over the supplied levels.
- The device requirements are listed under "Device".
