# Loading and character switching

## Scope and status

The loading changes below reduce actual asset work. Title-card, logo, fade,
party-ring and story-event durations are unchanged. In particular, `nowload.cpp`
is unchanged from the preceding release. Do not shorten its hold to improve a
loading measurement.

The 2026-10-26 measurements in this document are PC measurements. Console build
and runtime qualification of these new changes is pending. Earlier console
rendering results in [SWITCH.md](SWITCH.md) do not qualify these loading changes.

## Source lookup: the largest character-switch cost

Metadata source resolution previously scanned the entire effective asset catalog
and folded every source name for each cache miss. A quiet PC profile of a repeat
Toan/Xiao switch measured 42.012 ms in metadata snapshot lookup/read inside a 49.109 ms
party-loop call. Texture API work was 0.760 ms; visual construction was 0.476 ms,
including 0.168 ms in mesh finishing. The dominant blocking work lay in the
metadata snapshot path; those measurements do not establish a GPU execution stall.

`InitCDFile` now constructs a hash index of folded source identities after all
normalized overrides are applied. Resolution uses that index. Duplicate folded
identities remain ambiguous and resolve to failure, preserving the old scan's
behavior. The index contains pointers to the effective file bindings and is
cleared before those bindings are replaced or reset.

The immutable archive metadata snapshot cache keeps its existing four-entry,
16 MiB policy. Epoch invalidation, archive integrity checks, loose-file freshness
and externally pinned snapshots retain their previous semantics.

One quiet same-binary comparison, with PNG and document caching enabled in both:

| Confirmation | Old resolver | Indexed resolver, same cache policy |
|---|---:|---:|
| First Xiao | 24.800 ms | 8.427 ms |
| Return to Toan | 51.676 ms | 4.603 ms |
| Repeat Xiao | 47.595 ms | 7.231 ms |
| Repeat Toan | 46.926 ms | 8.107 ms |

The table contains complete individual party-loop calls, not complete rendered
frames or console timings. Their nested metadata snapshot spans fell from
39.055–43.139 ms to 1.465–2.285 ms. Audio was disabled for this comparison.

A separate default-launch check, without a map jump, measured first-mode setup
at 46.676 ms before and 8.735 ms after the source-index change. Both reached the
first mode tick at logical clock tick 72 with matching RNG state. This unpaced
300-frame check validates the launch path and its setup work; renderer startup
varied independently and the pair does not establish total launch latency.

## Reuse decoded PNGs

`PngDecodeRgba` retains successful decoded images in a process-wide LRU. Its
64 MiB retained-data budget counts complete encoded PNG keys plus RGBA pixels;
allocator bookkeeping, caller-owned output and temporary allocations are
additional. Oversized images still decode normally without being retained.

Keys use a hash followed by complete byte equality. File paths and timestamps
are insufficient identities: edited loose files, different data roots and hash
collisions must not return stale pixels. Every caller receives its own output
vector. The decoder allocation is retained on a miss, avoiding a second pixel
copy solely for cache insertion. Input and output may alias. Failed decodes are
not cached. Cache allocation failure does not invalidate an already decoded
output. Access is protected by a mutex; decoding happens outside that lock.

Reads and archive CRC checks still occur. This optimization avoids repeated PNG
inflation and filtering, not storage reads or GPU texture creation. It also
does not retain mutable palette decisions, texture handles or GPU resources.

A four-leg PC route, Norune → Queens → Muska Lacka → Queens → Norune, read
656 baked PNGs. The complete archive trace contained 334 distinct PNG paths;
different paths can also contain identical bytes. With the decoded cache,
273 baked decode calls remained and 383 reads reused decoded pixels. The last
sample retained 57,716,996 bytes, below the 64 MiB budget.

| Stage | PNG reads | Cache hits | PNG materialization, off / on |
|---|---:|---:|---:|
| Initial Norune, including common data | 154 | 4 | 62.106 / 63.582 ms |
| First Queens visit | 124 | 65 | 77.319 / 40.237 ms |
| First Muska Lacka visit | 112 | 57 | 84.970 / 47.281 ms |
| Return to Queens | 124 | 121 | 96.055 / 9.128 ms |
| Return to Norune | 142 | 136 | 68.266 / 10.754 ms |

This is one PC pair, not a statistical console speedup claim. First-use images
still require decoding; this cache primarily helps shared assets and revisits.
The full final image was pixel-identical with caching enabled and disabled.

Set `DC_PNG_CACHE=0` before launch to disable the cache for comparison.

## Reuse consecutive texture documents

`PortLoadBakedTexture` often selects several consecutive pictures from the same
schema-3 JSON document. Each loading thread retains only its most recent parsed
document, and only when its encoded input is at most 64 KiB. Complete bytes are
read and compared on every request. The retained JSON DOM uses additional memory;
64 KiB is the input-key limit, not an exact DOM allocation limit.

Shared immutable ownership keeps references valid through nested calls or cache
replacement. Schema, ordinal/name, CLUT variant, relative-path and dimension
checks still run. Dynamic selection and source format decisions are not cached.
Malformed input is retried, and edits at the same path are observed immediately.
Set `DC_TEXTURE_JSON_CACHE=0` before launch to disable this reuse.

## Measurement controls

- `DC_PERF_TRANSITIONS=1` records startup after argument setup, data/renderer readiness, mode setup,
  loading-card checkpoints, first mode tick, walking controls and software fade
  readiness. Accepted world travel is stamped at the menu's actual map handoff,
  not a preview or ignored button edge. This observer covers outer mode changes
  and world-map travel; it does not fully instrument house/interior transitions.
- `DC_PERF_CHARACTER_SWITCH=1` times the native party-loop wrapper. It also
  collects quiet nested spans for background reads, metadata, PNGs, texture API
  calls, visual construction and mesh finishing. The element picker is separate.
- `DC_PERF_LOAD=1` adds detailed archive and baked-texture records. It is useful
  for counts and child attribution, but logging itself substantially inflates
  enclosing read/texture/party-loop times. Do not subtract these verbose parent
  times from a quiet run. `png_decode_wall_ms` includes cache lookup, copying and
  insertion; `png_decodes` counts real decoder calls and `png_cache_hits` counts
  reused results. `mips` continues to count all materialized levels.
- `DC_HEADLESS_BOUNDED=1` retains the normal frame clock in a headless PC run.
  Ordinary headless runs advance without real-time pacing and cannot measure
  player-visible loading duration.
- `DC_TEXTURE_SOURCE_CACHE=0` disables archive metadata snapshots, but does not
  disable the source identity index.
- `DC_SOURCE_LOOKUP_INDEX=0` skips constructing the source identity index and
  selects the original full scan. Use this for same-binary comparisons; the
  default is indexed lookup. Set comparison controls before launching.

All timing observers default off. A nested span includes its children; visual
construction and mesh-finishing times must not be added together. API timings
describe calling-thread wall time, which may include queue waits, not GPU work.

The launch origin is after argument setup in `Run`, not process creation, executable loading,
or a physical input-to-presented-frame measurement. `mode_init_done` includes
nested loading-card waits and must not be labeled exclusive CPU or I/O time.
Walking readiness is a software observation after input gates clear; fade
readiness waits until a subsequent loop after the fade state clears. Presentation
must be confirmed separately when qualifying an end-to-end result.

## Fixtures and interpretation

The travel fixture starts with the populated `e01` Norune jump and grants the
World Map item through `--give 238`. Untimed existing debug controls mark visited
destinations. The measured legs then use ordinary world-map selection and final
confirmation. Queens and Muska Lacka in this fixture have default boards; this
does not replace populated-town rendering qualification.

First arrival in Queens runs a story sequence: walking controls become available
734 logical ticks after the first mode tick. Keep that sequence separate from
asset loading. In a single paced comparison of the old source resolver and the
indexed resolver, request-to-software-fade readiness was 21.886 and 21.801 seconds.
Most of that interval is prescribed presentation/story time. Renderer startup
also varied independently between processes, so the apparent total launch
difference cannot be attributed entirely to these changes.

The character fixture uses `--jump dungeon:2` and an isolated save. Enter the
ordinary first floor; open Triangle → Allies; use the existing Select+L2+Cross
debug action once to recruit Xiao; then return to gameplay. The measured part
uses ordinary Select, left/right selection and Cross confirmation for four
alternating Toan/Xiao swaps. This setup was runtime-validated. Dungeon 0 instead
encounters tutorial dialogue and is not an equivalent unattended fixture.

## Remaining work

The PC validation passed 654 tests, with eight environment/resource-dependent
skips and one disabled benchmark. Targeted coverage includes cache eviction and
budget, edited/malformed inputs, input/output aliasing, normalized/ambiguous
source identities, epoch changes, corruption, loose-file freshness and palettes.

The final fixed-tick travel captures match the same-binary original-scan control
pixel-for-pixel. Disabling all three optimizations or enabling all three also
produces the same image and RNG/wind milestone sequence. Earlier PNG-only and
both-data-cache controls matched as well. Paced runs are used for duration checks
separately. Rebuilt reference executables showed a small cloth/RNG difference
that was not reproduced by toggling optimizations in one executable; do not
attribute that unresolved cross-relink variation to storage time or use it as a
new rendering baseline without checking simulation state.

1. Build and qualify these changes on the target platform with audio enabled.
   Test cold/warm launch, populated-town travel and character switching. Preserve
   all presentation periods and compare final images. PC gains do not establish
   console latency, frame-rate or memory headroom.
2. Measure cold character preview I/O. The first quiet preview in one PC process
   took 88.614 ms; later preview changes were about 1.3–1.8 ms. The preview path
   issues five synchronous background-read API calls before returning, including
   a duplicate default/equipped starter weapon. Separate storage, CRC, bookkeeping
   and localization before choosing prefetch or deduplication.
3. Consider true background reads only after auditing consumers. The API exposes
   sizes immediately for buffer layout, and the current port publishes complete
   reads synchronously. Worker reads must own their buffers, preserve read-ready
   ordering, CRC/error handling, cancellation and source registration, and avoid
   publishing partially filled native buffers. The existing ring animation can
   overlap real I/O; it must not be shortened to hide a stall.
4. First-use PNG decoding remains substantial. Prior console profiling measured
   about 357 ms for 141 unique scene PNGs. Benchmark faster lossless decoding or
   an optional efficiently decoded baked representation across platforms, with
   integrity, alpha, dimensions and fallback coverage. A larger decoded cache
   cannot eliminate first-use inflation.
5. Reprofile remaining model/texture construction after the source-index fix.
   Warm PNG work and metadata were small in the optimized character test; retain
   the quiet spans rather than assuming future pauses are storage or GPU stalls.
