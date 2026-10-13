#pragma once

// On a crash (an access violation and the like, not the SIGABRT of a PS2_UNIMPLEMENTED stub or a
// failed assertion, which stop where a debugger can see them), prints what happened and the call
// stack, each frame as module+offset, to stderr, before the process ends as it would have.
void CrashReportInstall();
