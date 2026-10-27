#pragma once

/**
 * The PS2 SDK's integer type names. port.h includes this first, renaming u_long and size_t so the
 * host's own definitions stay in force.
 */

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using s8 = char;
using s16 = short;
using s32 = int;
using s64 = long long;

using u_char = unsigned char;
using u_short = unsigned short;
using u_int = unsigned int;
using u_long = unsigned long;
using u_long128 = unsigned __int128;

using size_t = unsigned int;
#define NULL 0
