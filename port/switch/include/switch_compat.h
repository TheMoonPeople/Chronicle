// Included ahead of every unit of the Switch build: what newlib and libnx lack of the POSIX
// interfaces the port's host code uses.
#pragma once

#include <fcntl.h>

// Horizon has no exec, so no descriptor outlives the process in a child.
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
