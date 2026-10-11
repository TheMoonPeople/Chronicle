#pragma once

#include "kazarov/port.h"

#include "dungeon.h"
#include "language.h"
#include "mathconst.h"
#include "sound_effect.h"
#include "types.h"

#define STATIC_ASSERT(expr) typedef char _static_assert_##__COUNTER__[(expr) ? 1 : -1]

#ifdef PAL
#define SCREEN_HEIGHT 480
#define SCREEN_HALF_HEIGHT 240
#define SCREEN_HEIGHT_F 480.0f
#define SCREEN_HALF_HEIGHT_F 240.0f
#define SCREEN_QUARTER_HEIGHT_F 120.0f
#define GS_Y_OFFSET 0x7880
#define SCREEN_HEIGHT_STR "480"
#define HALF_BUFFER_HEIGHT_STR "256"
#else
#define SCREEN_HEIGHT 448
#define SCREEN_HALF_HEIGHT 224
#define SCREEN_HEIGHT_F 448.0f
#define SCREEN_HALF_HEIGHT_F 224.0f
#define SCREEN_QUARTER_HEIGHT_F 112.0f
#define GS_Y_OFFSET 0x7900
#define SCREEN_HEIGHT_STR "448"
#define HALF_BUFFER_HEIGHT_STR "224"
#endif
