#pragma once

namespace dc::app {

/**
 * Names the game to the shared host: its environment prefix, per-user directories and window
 * identity. Called before anything else in a game's entry point.
 */
void GameSetIdentity();

/**
 * Swaps the two moons the build ships for the lamp, the memory card icon (dkicon.ico in
 * commenu/<lang>/savetex.pak), drawn from the extracted data so no build carries a copy of it. On
 * Windows the first launch with the data also writes darkcloud.ico and a "Dark Cloud" shortcut
 * carrying it beside data/.
 */
void GameSetDataIcon();

} // namespace dc::app
