#pragma once

namespace dc::app {

/**
 * The process's exit statuses. A PS2_UNIMPLEMENTED stub still aborts (SIGABRT, 134 from a shell)
 * so that a debugger or a core dump stops at it.
 */
enum ExitStatus : int {
    kExitOk = 0,         /**< Normal exit. */
    kExitFailure = 1,    /**< The window, the renderer or a file the port writes (the screenshot) failed. */
    kExitUsage = 2,      /**< Invalid command line. */
    kExitNoData = 3,     /**< The data directory is missing or holds no file. */
    kExitGameAssert = 4, /**< The game's own __assert, such as LoadFile on a missing file. */
};

} // namespace dc::app
