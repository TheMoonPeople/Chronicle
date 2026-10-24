#include <cstdio>
#include <cstdlib>

extern "C" [[noreturn]] void __assert_func(const char *file, int line, const char *function, const char *expression) {
    std::fflush(stdout);
    std::fprintf(stderr, "%s:%d: %s: assertion failed: %s\n", file, line, function, expression);
    std::fflush(nullptr);
    std::abort();
}
