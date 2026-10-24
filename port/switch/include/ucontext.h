// What Kazarov's eekernel threads (sce/eekernel_threads.cpp) use of POSIX ucontext, which newlib
// does not have: a context made on a stack of its own with makecontext and entered with swapcontext.
// The threads are cooperative and switch only through swapcontext, a call, so a context holds the
// registers a call preserves (x19 to x30, sp, d8 to d15) and no signal mask.
#pragma once

#include <cstddef>
#include <cstdint>

/**
 * The callee-saved AArch64 registers used by Kazarov's cooperative host threads.
 * The field order is shared with the register offsets in ucontext.cpp.
 */
struct ucontext_t {
    std::uint64_t x[12] = {}; /**< x19 to x30; x30 is where the context resumes. */
    std::uint64_t sp = 0;     /**< Stack pointer. */
    std::uint64_t d[8] = {};  /**< d8 to d15. */

    struct {
        void       *ss_sp = nullptr; /**< Stack memory. */
        int         ss_flags = 0;    /**< Stack flags. */
        std::size_t ss_size = 0;     /**< Stack size in bytes. */
    } uc_stack;                      /**< Stack used to prepare this context. */

    ucontext_t *uc_link = nullptr; /**< Context entered when this context returns. */
};

/**
 * Initializes a context for makecontext without capturing the current registers.
 * Contexts saved by this function are only entered after makecontext prepares them.
 */
int getcontext(ucontext_t *ucp);

/**
 * Prepares ucp to call func on its configured stack and enter uc_link when func returns.
 * The Switch host thread entry functions take no arguments.
 */
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);

/**
 * Saves the calling context into from and enters to.
 * Returns zero when a later context switch resumes from.
 */
extern "C" int swapcontext(ucontext_t *from, const ucontext_t *to);
