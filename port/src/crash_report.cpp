#include "crash_report.hpp"

#include <cstdio>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

// The module holding address, as its file name, its preferred image base and where it lies now.
struct Module {
    char      name[MAX_PATH] = "?";
    ULONG_PTR base = 0;
    ULONG_PTR preferred = 0;
};

Module ModuleOf(DWORD64 address) {
    Module  module;
    HMODULE handle = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(address), &handle)) {
        return module;
    }
    char        path[MAX_PATH];
    DWORD       length = GetModuleFileNameA(handle, path, MAX_PATH);
    const char *name = path;
    for (DWORD i = 0; i < length; i++) {
        if (path[i] == '\\' || path[i] == '/') {
            name = path + i + 1;
        }
    }
    lstrcpynA(module.name, name, MAX_PATH);
    module.base = reinterpret_cast<ULONG_PTR>(handle);
    auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(handle);
    auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(reinterpret_cast<const char *>(handle) + dos->e_lfanew);
    module.preferred = static_cast<ULONG_PTR>(nt->OptionalHeader.ImageBase);
    return module;
}

void PrintFrame(int index, DWORD64 address) {
    Module module = ModuleOf(address);
    // The address as the module's own file numbers it, which llvm-symbolizer takes.
    std::fprintf(stderr, "  #%-2d %s+0x%llx  (0x%llx)\n", index, module.name,
                 static_cast<unsigned long long>(address - module.base),
                 static_cast<unsigned long long>(address - module.base + module.preferred));
}

LONG WINAPI Report(EXCEPTION_POINTERS *info) {
    const EXCEPTION_RECORD *record = info->ExceptionRecord;
    std::fprintf(stderr, "\ncrash: exception 0x%08lx at ", record->ExceptionCode);
    PrintFrame(0, reinterpret_cast<DWORD64>(record->ExceptionAddress));
    if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
        std::fprintf(stderr, "crash: %s address 0x%llx\n",
                     record->ExceptionInformation[0] == 0   ? "reading"
                     : record->ExceptionInformation[0] == 1 ? "writing"
                                                            : "executing",
                     static_cast<unsigned long long>(record->ExceptionInformation[1]));
    }
    std::fprintf(stderr, "crash: stack\n");
#if defined(_M_X64) || defined(__x86_64__)
    CONTEXT context = *info->ContextRecord;
    for (int frame = 0; frame < 48 && context.Rip != 0; frame++) {
        PrintFrame(frame, context.Rip);
        DWORD64           image_base = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &image_base, nullptr);
        if (function == nullptr) {
            // A leaf function: its return address is on top of the stack.
            context.Rip = *reinterpret_cast<DWORD64 *>(context.Rsp);
            context.Rsp += 8;
            continue;
        }
        void   *handler_data = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context, &handler_data, &establisher,
                         nullptr);
    }
#endif
    std::fprintf(stderr, "crash: llvm-symbolizer --obj=<the .exe> <the numbers in brackets> names the lines\n");
    std::fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void CrashReportInstall() {
    SetUnhandledExceptionFilter(Report);
}

#else

#include <unistd.h>

#include <csignal>
#include <initializer_list>

#if __has_include(<execinfo.h>)
#include <execinfo.h>
#define DC_HAVE_BACKTRACE 1
#endif

namespace {

void Report(int signal) {
    char header[64];
    int  length = std::snprintf(header, sizeof header, "\ncrash: signal %d\ncrash: stack\n", signal);
    if (length > 0) {
        ssize_t ignored = write(STDERR_FILENO, header, static_cast<size_t>(length));
        (void) ignored;
    }
#ifdef DC_HAVE_BACKTRACE
    void *frames[48];
    int   count = backtrace(frames, 48);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
#endif
    std::signal(signal, SIG_DFL);
    std::raise(signal);
}

} // namespace

void CrashReportInstall() {
    for (int signal : {SIGSEGV, SIGBUS, SIGFPE, SIGILL}) {
        std::signal(signal, Report);
    }
}

#endif
