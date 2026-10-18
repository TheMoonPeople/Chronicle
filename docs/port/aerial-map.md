# Native aerial dungeon map

The PC dungeon map renders the floor and game models from directly above. Discovery, the map item, the crystal, traps, chests, Atla and monster visibility follow the native minimap rules. No mod framework is required and the retail grid, chest and monster limits are unchanged.

The native Dungeon Map option chooses large, medium or small, or hides the map. `game.aerial_map` defaults to true; false restores the retail tile map, which is also used if the aerial render targets cannot be created. The map stays inside the 640 by 480 logical frame and follows the player when zoomed beyond the floor's visible bounds.

Configuration in `config.json`:

| Key under `game` | Default | Purpose |
| --- | --- | --- |
| `aerial_map` | `true` | Enable the aerial view |
| `aerial_map_ceiling` | `400` | Ceiling cut above the player, in world units |
| `aerial_map_zoom` | `1` | Magnification |
| `aerial_map_model_scale` | `8` | Enlarge character, chest and Atla models |
| `aerial_map_size` | `1.4` | Scale the native map size choices |
| `aerial_map_markers` | `false` | Add colored dots for distant objects and the player |

While the aerial map is visible, F1/F2 shrink/grow it; F5/F6 shrink/grow models; F7 increases zoom (Shift+F7 decreases it); Shift+F5/F6 lower/raise the ceiling cut. Tuning is saved immediately.

The still floor, closed chests, Atla and sleeping monsters are cached. Discovery/layout changes, native object state, map identity, configuration and render-target replacement invalidate that image; player and awake-monster models draw every tick. The overhead pass uses an orthographic projection and disables interpolation, restoring the normal projection afterward.

Conservative wall-aware native visibility culling retains nearby cells and a one-cell halo, fails open for special or unknown parts, and bypasses Moon Sea and georama 5.

The pinned Kazarov backport contains only the cached-target replay and fixed-camera corrections. The backport is published at `Jeffiest/Kazarov:fix/cached-map-display-replay-chronicle`, commit `7ad7df61c6991d44dd8a1681a00bc0eefbc4ed59`; its exact SHA was verified fetchable through the unchanged upstream URL. Use `git submodule update --init --recursive` as usual. If a host requires a fork ref, fetch that branch from `https://github.com/Jeffiest/Kazarov.git` inside `tools/kazarov` and check out the pinned SHA. The upstream renderer PR is https://github.com/TheMoonPeople/Kazarov/pull/23. Keep the upstream URL in `.gitmodules`; replace the temporary pin with an upstream commit after the renderer dependency lands.

Regression suites: `DungeonAerialCache`, `DungeonAerialProjection`, `DungeonVisibility`, and Kazarov's `GfxCachedTarget`. Run graphics tests with `DC_GFX_OFFSCREEN=1` for surface-free rendering.
