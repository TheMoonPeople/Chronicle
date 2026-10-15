#pragma once

// Names the game to the shared host: its environment prefix, per-user directories and window
// identity. Called before anything else in a game's entry point.
void GameSetIdentity();

// Swaps the two moons the build ships for the lamp, the memory card icon (dkicon.ico in
// commenu/<lang>/savetex.pak), drawn from the extracted data so no build carries a copy of it. On
// Windows the first launch with the data also writes darkcloud.ico beside data/ and a "Dark Cloud"
// shortcut beside the executable that carries it.
void GameSetDataIcon();
