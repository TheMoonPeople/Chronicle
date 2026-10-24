# Native port review ledger

This ledger tracks the coordinated C++26 and storage review. It does not claim
that every line in the native port was manually reviewed. PS2 sources and
generated output were excluded. Concurrent changes under `port/src/audio/`,
`port/include/audio/`, build/run scripts, and `tools/kazarov` were not reviewed
or edited here.

## UI, menu, framework, quest, and shared headers

The UI reviewer completed line-by-line reads of 85 assigned production and
header files from `area_card*`, `battlemenu/**`, `btitem*`, `caption*`,
`clsmes*`, `custom_shop*`, `dialog*`, `dualsense*`, `item_icon3d*`, `menu*`,
`memcard.cpp`, `mouse*`, `name_mouse*`, `options/**`, `presence*`, `quest*`,
`quests/**`, `save_slots*`, `screen.hpp`, and `ui_framework/**`, plus the
shared `port/include/**` headers except `port.h`, `stubs/**`, and concurrent
audio headers. `exitcodes.hpp`, `normal_smoothing.hpp`, and `text_shadow.hpp`
were included. Six additional paths received consolidation-block diff review
only: `battlemenu.cpp`, `shop.cpp`, `menu_draw.cpp`, `memcard.cpp`,
`menu_save.cpp`, and `clsmes.cpp`. Their moved implementations had already
been read in their prior source units; this review checked forwarding
signatures, override ownership, and the exact changed blocks. The UI source
and shared-header formatting pass covered 91 files. It excluded tests,
`port/include/stubs/**`, `port/include/port.h`, and concurrent audio files.
The UI test-array scan was limited to `port/src/tests/**`; the runtime reviewer
owns the 42-file test line-by-line inventory below.

In `btitem.cpp`, `item_icon3d.cpp`, `menu_dungeon.cpp`, `menu_manual.cpp`, and
`menu_misc.cpp`, path/name construction now uses strings or variable-size
collections instead of fixed path/name scratch buffers. `menu_misc.cpp` also
removes overlapping formatting into the same weapon-name buffer. Legacy APIs
whose declarations accept mutable `char *` continue to receive writable
`std::string::data()` where their implementations do not mutate the path.
`custom_shop.cpp` builds temporary paths as strings and retains its
capacity-less caller output signature. Mutable texture-load descriptor scratch
in `menu_misc.cpp` and `options/screen.cpp` and the local element-choice output
buffer in `menu_dungeon.cpp` now use `std::unique_ptr`-owned storage. The
element picker's six-choice state and ring coordinates remain fixed state/math
tables.

UI test output and working buffers in `custom_shop_test.cpp`,
`tex_decode_test.cpp`, `menu_option_test.cpp`, `save_slots_test.cpp`,
`platform_pad_test.cpp`, and `platform_mouse_look_test.cpp` use
`std::unique_ptr`-owned storage. Pad DMA storage keeps 64-byte alignment through
an aligned element type. Remaining fixed test arrays are format/math/ABI data,
such as the 24-byte PNG header, texture/image fixtures, exact game-record
snapshots, geometry, and placement-test objects. `CTexture::name` and other
fixed game-facing fields remain unchanged to preserve layout. Legacy output
functions without a capacity parameter remain constrained by their existing
contract.

`LocalizeOptions.ShippedTranslationsHaveExactlyTheCodesKeys` checks exact key
parity for the namespaces returned by `OptionStrings()` (`options.`, `area.`,
`battlemenu.`, and `journal.`); unrelated shipped namespaces such as
`dialog.common.*` are outside that export's contract. The test still rejects
unknown keys inside those owned namespaces.

`port/.clang-tidy` follows the port rules from ChronicleTwo. The CMake language
standard and `docs/PC.md` specify C++26. `caption_text.cpp` retains its compile-time `#embed` font path.
The UI reviewer ran focused `misc-use-anonymous-namespace`,
`modernize-use-nullptr`, `google-readability-casting`,
`cppcoreguidelines-pro-type-cstyle-cast`, and
`readability-identifier-naming` checks over the 46 existing owned translation
units. UI-owned findings were corrected, including typed indexing for the
consumable inventory in `shop.cpp`; the final pass reported no diagnostics.
The separate global sweep uses the current-source inventory because tracked
paths removed by source consolidation still appear in `git ls-files`.
`git diff --check` reported no whitespace errors.

## Gameplay, world, dungeon, edit, and title

The gameplay reviewer’s final organization and source checks covered
`port/src/camera_zoom.cpp`, `port/src/first_person_walk.cpp`,
`port/src/title/titleloop.cpp`, `port/src/edit.cpp`, `port/src/edit_in.cpp`,
`port/src/editloop.cpp`, `port/src/editloop3.cpp`,
`port/src/title/opening.cpp`, `port/src/title/op_a.cpp`,
`port/src/title/op_b.cpp`, `port/src/title/rushmovi.cpp`,
`port/src/title/title.cpp`, and `port/src/dun/movechara.cpp`.
`port/src/gameloop.cpp` received the caller-boundary fix described below;
`port/src/tests/integration_gameloop_test.cpp` received the matching regression
assertions and a full formatting pass. The verifier owns `op_c.cpp`; its
recovery and patch audit are recorded separately below. The other assigned
headers and translation units were read as part of the gameplay review; the
final namespace/style pass and the checks listed here are limited to the
enumerated files.

The source recovery followed an accidental namespace-edit script that had
placed closing braces inside function bodies. Gameplay-owned affected files
were reconstructed from staged pre-script snapshots and logged source reads,
then their recorded intentional patches were replayed. `op_c.cpp` was restored
by the recovery verifier from HEAD plus the recorded texture-buffer and frame
name patches. The recovery verifier and gameplay reviewer compared the
restored `op_c.cpp` resource rows with HEAD and the logged patch ordinals; no
resource-table changes were retained. The reviewer also checked the moved
`BtCheckDamageProc` and `EyeCamera` definitions and their call sites after
same-origin consolidation into `dun/gameloop.cpp`.

The title scenes’ private state and helpers now use anonymous namespaces,
while their `PORT_OVERRIDE` functions and cross-translation-unit APIs remain
external. The verifier’s final `op_c.cpp` pass placed its file-private helper
declarations, definitions, and state in six balanced anonymous-namespace
blocks; all eight `PORT_OVERRIDE` functions and the `Water` override remain at
namespace scope. Its focused parse and Clang Tidy linkage checks reported no
diagnostics. The same treatment was applied to the editor helpers and
quick-change state in `movechara.cpp`. In `GameSetGive`, validation now rejects the
exclusive `ITEM_WEAPON_END` value; `GiveListParses` verifies the last valid ID
376 and rejects 377 before item lookup. The complete
`integration_gameloop_test.cpp` file was formatted. The relevant translation
units passed `misc-use-anonymous-namespace` Clang Tidy checks, Clang Format,
and `git diff --check`. The verifier also ran the configured default Clang Tidy
checks over twelve owned translation units; parsing exited successfully, with
the existing modernization warnings recorded rather than mass-edited.

Title recovery retained the aligned, workload-sized dance-motion buffers in
`op_b.cpp` and the workload-owned frame-info buffer in `rushmovi.cpp`. The
layout-bound retail face records, scene tables, fixed state, and renderer/math
arrays remain fixed. The capacity-less legacy helpers and PS2-facing layouts
remain unchanged. `GameSetGive` changes only host-side validation; no PS2
source was edited.

The assigned source inventory was `port/src/{btsysscript.cpp,
camerafollow.cpp,camera_port.cpp,camera_port.hpp,camera_zoom.cpp,
camera_zoom.hpp,character.cpp,chararead.cpp,cloth_draw.cpp,clothread.cpp,
cloth_step.cpp,collisionmdt.cpp,dungeon_aerial.cpp,dungeon_aerial.hpp,
dungeonmap.cpp,dungeonparts.cpp,ebattle.cpp,edit.cpp,editground.cpp,
editground_port.hpp,edit_in.cpp,edit_in_parts.cpp,editloop.cpp,editloop3.cpp,
editloop_init.cpp,editloop_parts.cpp,editloop_sprite.cpp,editmapscript.cpp,
effectmacro.cpp,fireomni.cpp,first_person_walk.cpp,first_person_walk.hpp,
fish.cpp,fishing.cpp,fish_port.hpp,gameloop.cpp,gameloop.hpp,
gameutil_midi.cpp,gameutil_motion.cpp,gameutil_sprite.cpp,itemdata.cpp,
mapparts.cpp,monstorunit.cpp,runeffect.cpp,runscript.cpp,runscript_port.hpp,
shadowclip.cpp,shadowclip.hpp,shot_freefuncs.cpp,shop.cpp}`, plus all tracked
files in `port/src/dun/` and `port/src/title/`. Shared ownership files were
not edited during recovery. The source list above is the assigned inventory;
it is not a claim that every one received the final namespace tidy check.

## Other coordinated scopes

The runtime/data review covers the assigned data loading and arena units,
renderer and texture units, sound integration, localization and text units,
platform support, and their declarations. It consolidates the frame, water,
and texture-buffer overrides into `frame.cpp`, `water.cpp`, and `texture.cpp`,
their PS2 source-unit names. Helper-only modules remain split where they own
no `PORT_OVERRIDE` implementation. Arena and pack tracking now validate
allocation/member bounds and clear cached pack capacities at data-buffer
reset; texture compaction validates source ranges. Tests cover invalid arena
sizes, malformed and stale pack entries, sound loading from a host pointer
above 4 GiB, and texture block compaction/range rejection.

The runtime reviewer read all 25 assigned files line by line: `dataalloc2_1.cpp`,
`dataread.cpp`, `dataread_port.hpp`, `dataset.cpp`, `draw2d_port.cpp`,
`draw3d.hpp`, `frame.cpp`, `gamepad.cpp`, `gametext.cpp`, `gametext.hpp`,
`localize.cpp`, `localize.hpp`, `memorycardaccess.cpp`, `mglib.cpp`,
`nowload.cpp`, `snd.cpp`, `sound.cpp`, `textwipe.cpp`, `textwipe.hpp`,
`texture.cpp`, `texture_port.cpp`, `textureanime.cpp`, `visualvu1.cpp`,
`vu0_ops.hpp`, and `water.cpp`. The 42 non-external test sources were also
read line by line: `area_card_test.cpp`, `bits64_fixture.hpp`,
`bits64_placement_test.cpp`, `camera_zoom_test.cpp`, `capfont_test.cpp`,
`caption_test.cpp`, `custom_shop_test.cpp`, `data_extract_test.cpp`,
`data_fixture.hpp`, `data_read_test.cpp`, `dcdata_textures_test.cpp`,
`dialog_framework_test.cpp`, `dualsense_test.cpp`,
`dungeonparts_bounds_test.cpp`, `edit_in_func_bounds_test.cpp`,
`editpartsinfo_bounds_test.cpp`, `first_person_walk_test.cpp`,
`fish_size_test.cpp`, `gametext_test.cpp`, `glyphs_test.cpp`,
`integration_gameloop_test.cpp`, `localize_test.cpp`,
`localize_texture_test.cpp`, `mac_truncation_test.cpp`, `main.cpp`,
`menu_option_test.cpp`, `menu_save_pointer_test.cpp`,
`monster_drops_test.cpp`, `name_mouse_test.cpp`, `paths_xdg_test.cpp`,
`platform_arena_test.cpp`, `platform_config_test.cpp`,
`platform_discord_test.cpp`, `platform_mouse_look_test.cpp`,
`platform_pad_test.cpp`, `quest_framework_test.cpp`, `save_slots_test.cpp`,
`tex_decode_test.cpp`, `tex_fixture.hpp`, `texture_assets_test.cpp`,
`textwipe_test.cpp`, and `treant_chests_test.cpp`. The untracked
`renderer_performance_test.cpp` was read but left untouched because its
ownership was uncertain. `retail_audio_test.cpp` and external audio files
were excluded.

Override placement follows the PS2 translation unit that owns each original
symbol: placement allocation operators are in `frame.cpp`; `LoadOverlay`,
the four compiler-generated constructor stubs, `CDataAlloc<1, 6000>::Alloc`,
and `SetEnv` are in `main.cpp`; `mwInit`, `mwLoadOverlay`, `__unexpected`,
and the exception vtable are in `mathutil.cpp`; and the overlay address table
is in the new `literals.cpp`. `MakeRandTbl` and `MakeRandTbl2` were moved from
`gametext.cpp` to `clsmes.cpp` by the UI reviewer. `mwInit`'s original body
starts the PS2 static-initializer runner; the port stub remains a no-op because
the host has already run static constructors.
The review distinguished fixed math vectors, disc records and tables,
input/protocol mappings, and serialized save layouts from scratch buffers.
Pack entry `name[64]`, texture-port inverse lookup tables, the memory-card
image, VU/matrix vectors, and persistent packet/arena state remain fixed
because they are format, lookup, or layout data. `CTextureManager::GetTexture`
returns `textures[0]` for an invalid numeric handle, so the negative texture
handle in visual material conversion is a defined fallback and not a null
dereference. Files concurrently edited outside this scope (including
`main.cpp`, `gameloop.cpp`, platform configuration, and new audio integration
files) were treated as read-only or excluded.

The verifier additionally read all 24 `port/include/stubs/*.hpp` headers,
`port/include/port.h`, the three concurrent audio headers (read-only), port
and test CMake configuration, `flatpak.sh`, and all three Flatpak manifests.
No further layout or pointer-width issue was found beyond the weapon-list
upper-bound fix. The stub `WEAPON_DATA` proxy has explicit size and owner-offset
assertions; other PS2 layouts are not assumed to match host layouts unless a
specific proxy asserts them. Formatting dry-run and scoped whitespace checks
passed for the 92 owned C++ paths. The gameplay-owned integration test was
excluded from that dry-run because Clang Format 23 requested alignment-only
changes; its owner formatted it separately. Concurrent audio files and
generated output were excluded.

## Validation and known limitations

The final C++26 test and release builds passed, and the 10-frame offscreen
smoke run exited successfully. CTest passed 577 of 579 executed tests; three
registered tests were skipped and one disabled test did not run. The two
failures are the external AreaCard image-migration incompatibilities described
below. The `SoundLoadsPackMembersAboveFourGiBWithoutPointerNarrowing` test
passed 20 consecutive default-audio runs after its test-local RAII guard began
stopping the SDL output callback before the pack buffer leaves scope. The
pointer-width assertion remained enabled. `git diff --check` passed, and no
`ps2/` source changed.

The repository's `port_override.py check` passed for all 437 tagged overrides.
The supplemental strict same-relative-path probe surfaced two intentional
exceptions that the repository checker resolves: `DunMoveChara` is renamed by
the stub into `ps2/src/dun/gameloop.cpp`, and `mwInit` uses explicit C linkage
for the host replacement. These are not unresolved override mismatches.

The required Clang Tidy sweep covered 163 non-audio translation units; focused
post-fix checks for the verifier-owned `op_c.cpp`, `cloth.cpp`, `itemdata.cpp`,
`gameutil.cpp`, and the sound regression test passed. The broad sweep also
reported findings in separately owned gameplay paths, preserved concurrent
`main.cpp` work, redundant `static` on declarations already inside anonymous
namespaces, and external included headers (`tools/dcdata` and audio). Those
paths remained outside the verifier's write scope. Clang Format dry-run and
whitespace checks passed for the owned C++ inventory.

Two AreaCard tests fail in the concurrent Kazarov image migration. The test's
`gfx::WritePng` fixture has PNG IHDR depth/type bytes `8, 2` (8-bit RGB), while
the concurrent `imagefile::LoadPng` implementation rejects any color type
other than 6 (RGBA) at `tools/kazarov/src/cpp/platform/imagefile.cpp`. Both are
valid format-specific behaviors but are incompatible; this review did not edit
Kazarov code or alter the test to hide the mismatch. The LocalizeOptions
namespace issue was fixed in the test as described above; the final CTest run
confirmed the outcomes reported above.
