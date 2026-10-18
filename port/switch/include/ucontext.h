// What Kazarov's eekernel threads (sce/eekernel_threads.cpp) use of POSIX ucontext, which newlib
// does not have: a context made on a stack of its own with makecontext and entered with swapcontext.
// The threads are cooperative and switch only through swapcontext, a call, so a context holds the
// registers a call preserves (x19 to x30, sp, d8 to d15) and no signal mask.
#pragma once

#include <cstddef>
#include <cstdint>

struct ucontext_t {
    std::uint64_t x[12] = {}; // x19 to x30; x30 is where the context resumes
    std::uint64_t sp = 0;
    std::uint64_t d[8] = {};  // d8 to d15
    struct {
        void       *ss_sp = nullptr;
        int         ss_flags = 0;
        std::size_t ss_size = 0;
    } uc_stack;
    ucontext_t *uc_link = nullptr;
};

// Only prepares ucp for makecontext: no caller resumes a context it saved this way.
int getcontext(ucontext_t *ucp);
// ucp, entered, calls func on uc_stack; when func returns, uc_link is entered, or the program stops
// if there is none. The arguments after argc are not passed: eekernel's entry takes none.
void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
// Saves the caller into from and enters to; returns 0 once something swaps back to from.
int swapcontext(ucontext_t *from, const ucontext_t *to);
