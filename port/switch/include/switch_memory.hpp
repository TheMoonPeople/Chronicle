#pragma once

/**
 * Restores arena guard pages before the Homebrew Menu reuses the process heap.
 * Call during libnx application cleanup, including its early process-exit path.
 */
void SwitchRestoreMemoryGuards();
