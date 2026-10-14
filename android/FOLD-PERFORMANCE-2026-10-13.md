# Z Fold 7 performance profile — 2026-10-13

Device: Samsung SM-F966W, inner landscape display, thermal status 0. Android build with 50% internal render scale, 60 Hz game ticks, interpolation on, immediate present mode, and unlimited display FPS. The app was profiled in dungeon floor 1. These are debug builds with Android-only timing probes.

## Measurements

| Renderer | Scene/window | Ticks per 5 s | Fence slot wait | CPU shadow pass | GPU canonical | GPU display |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 2 slots | Steady corridor, 22:15:41 | 240 (~48/s) | 1,364 ms total, 2.85 ms/wait | ~1,835 ms/5 s | 6.85 ms/submit | 1.28 ms/submit |
| 3 slots | Dungeon, 22:19:27 | 259 (~52/s) | 0.7 ms total, ~0.001 ms/wait | 2,068 ms/5 s | 6.77 ms/submit | 1.44 ms/submit |

Other three-slot windows reached 300 canonical submissions per five seconds (~60 ticks/s). GPU busy samples in a two-slot dungeon window were about 39–47%; in a three-slot window, 48–55%. These scenes are not identical, so tick-rate improvement is indicative rather than a controlled benchmark. The fence-wait reduction is direct and large. `fence_depth` stayed at zero in these scenes. Pool reset averaged below 0.1 ms in steady play. Acquire and queue submit were small, while three-slot off-CPU samples moved toward Android BufferQueue / `vkQueuePresentKHR` when display FPS was unlimited.

Before the overlay moved to an Android View, `TouchOverlayDraw` was a major native CPU sample in a menu scene. It no longer appeared in the post-change Fold profile. In the two-slot dungeon CPU profile, `DunMainDraw`, `Draw_MainUnitShadow`, `MGDrawShadowFast`, and `CVisualShadow::DrawVu1` dominated active game CPU work. The shadow path constructs prism faces and dynamic vertex/index vectors on the game thread each tick. Timed `DrawShadowPass` confirms several milliseconds of work per tick in active dungeon scenes.

## Decision

Keep three frames in flight on Android for now. It removes the recurring fence wait without changing game content and improved tick throughput in the sampled dungeon windows. It does not guarantee 60 ticks in shadow-heavy scenes. The next optimization target is CPU shadow-volume construction and its dynamic allocations, followed by reducing redundant display submissions when the app is already behind its 60 Hz tick target. A frame cap aligned with the display refresh should also prevent menus from drawing hundreds of frames per second. Do not treat the fence's `vkWaitForFences` stack as proof that the GPU is saturated.

The three-slot change is limited to Android; desktop stays at two slots. Before an upstream PR, run longer gameplay and fold/unfold checks, compare controlled same-scene A/B builds, and strip or gate the diagnostic timing code.

Evidence files: `fold-vulkan-timings-20261013.log`, `../dc-overlay-new-cpu-20261013.data`, `../dc-overlay-new-offcpu-20261013.data`, `../dc-3slot-cpu-20261013.data`, `../dc-3slot-offcpu-20261013.data`.

## Shadow-volume allocation pass

The next Android build reuses the temporary face/index arrays, constructs prism faces and near caps in fixed-size arrays, and reserves the fast volume's maximum vertex count. It does not alter the projection, winding, blend, depth, or shadow-band calculations. On the Fold, sampled active-dungeon shadow calls fell from about 0.68–0.95 ms each in the earlier three-slot log to about 0.27–0.36 ms each after the change. The scenes and number of casters differed, so this is evidence of a substantial per-call CPU reduction, not a controlled frame-rate claim. In the optimized capture the canonical GPU pass was roughly 2–3 ms and the display pass roughly 5–6 ms; fence waits remained negligible. Floor 1 was reloaded with the optimized build, and the app remained running.

Ground/wall shadow flicker was reported before this allocation pass. Pre-change and post-change Fold videos were captured, but they show different locations and camera angles, so they cannot establish a change in flicker frequency. The allocation pass preserves shadow geometry ordering. Investigate the shadow target/depth reuse and frame interpolation with a stationary same-scene A/B before claiming the flicker is fixed.
