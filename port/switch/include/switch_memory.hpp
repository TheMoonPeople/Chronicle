#pragma once

namespace dc::platform {

/**
 * Restores arena guard pages before the Homebrew Menu reuses the process heap.
 * Call during libnx application cleanup, including its early process-exit path.
 */
void SwitchRestoreMemoryGuards();

} // namespace dc::platform

// DC_TRANSITIONAL_BEGIN
#ifndef DC_NO_TRANSITIONAL_USING
using namespace dc::platform;
#endif
// DC_TRANSITIONAL_END
