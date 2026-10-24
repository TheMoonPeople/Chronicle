// ucontext.h's three calls on AArch64: the context switch is a call, so only the registers the AAPCS64
// has a callee preserve are saved and loaded. The offsets follow ucontext_t: x19-x30 at 0, sp at 96,
// d8-d15 at 104.

#include <ucontext.h>

#include <cstdio>
#include <cstdlib>

static_assert(offsetof(ucontext_t, sp) == 96 && offsetof(ucontext_t, d) == 104);

extern "C" void dc_context_start();

// from in x0, to in x1.
asm(R"(
    .text
    .global swapcontext
    .type swapcontext, %function
    .p2align 2
swapcontext:
    stp x19, x20, [x0, #0]
    stp x21, x22, [x0, #16]
    stp x23, x24, [x0, #32]
    stp x25, x26, [x0, #48]
    stp x27, x28, [x0, #64]
    stp x29, x30, [x0, #80]
    mov x9, sp
    str x9, [x0, #96]
    stp d8, d9, [x0, #104]
    stp d10, d11, [x0, #120]
    stp d12, d13, [x0, #136]
    stp d14, d15, [x0, #152]

    ldp x19, x20, [x1, #0]
    ldp x21, x22, [x1, #16]
    ldp x23, x24, [x1, #32]
    ldp x25, x26, [x1, #48]
    ldp x27, x28, [x1, #64]
    ldp x29, x30, [x1, #80]
    ldr x9, [x1, #96]
    mov sp, x9
    ldp d8, d9, [x1, #104]
    ldp d10, d11, [x1, #120]
    ldp d12, d13, [x1, #136]
    ldp d14, d15, [x1, #152]
    mov w0, #0
    ret
    .size swapcontext, . - swapcontext

    // A made context's first entry: x19 is its function, x20 the context.
    .global dc_context_start
    .type dc_context_start, %function
    .p2align 2
dc_context_start:
    blr x19
    mov x0, x20
    bl dc_context_finished
    brk #0
    .size dc_context_start, . - dc_context_start
)");

extern "C" [[noreturn]] void dc_context_finished(ucontext_t *ucp) {
    if (ucp->uc_link != nullptr) {
        ucontext_t done;
        swapcontext(&done, ucp->uc_link);
    }
    std::fprintf(stderr, "ucontext: a context's function returned with no uc_link\n");
    std::abort();
}

int getcontext(ucontext_t *ucp) {
    *ucp = {};
    return 0;
}

void makecontext(ucontext_t *ucp, void (*func)(), int, ...) {
    auto top = reinterpret_cast<std::uintptr_t>(ucp->uc_stack.ss_sp) + ucp->uc_stack.ss_size;
    ucp->sp = top & ~std::uintptr_t{15};
    ucp->x[0] = reinterpret_cast<std::uint64_t>(func);               // x19
    ucp->x[1] = reinterpret_cast<std::uint64_t>(ucp);                // x20
    ucp->x[10] = 0;                                                  // x29: the frame chain ends here
    ucp->x[11] = reinterpret_cast<std::uint64_t>(&dc_context_start); // x30
}
