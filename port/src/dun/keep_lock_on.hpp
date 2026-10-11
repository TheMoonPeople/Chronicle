#pragma once

// Retail's quick change drops the lock-on: once the new character has loaded, CharaChangeKey calls
// LockOffTargte. With game.swap_keeps_lock_on on, the lock the player had when SELECT opened the
// quick change comes back after it, if its monster is still alive and can be locked on. The game's
// own test in setTargetCursor then holds it or lets it go, as it does while walking, so a monster out
// of the new character's range is let go as one walked away from is.

// Called as SELECT opens the quick change: keeps the lock-on as it is (or that there is none).
void KeepLockOnSave();

// Called as the quick change closes: puts the kept lock-on back, and forgets it.
void KeepLockOnRestore();
