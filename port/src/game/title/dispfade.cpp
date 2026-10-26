#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "title/dispfade.hpp"

#include "mglib.hpp"
#include "rect.hpp"

void CDispFade::FadeOutStart(float speed, int colour) {
    fade_speed = speed;
    mode = 1;
    fade_colour = colour;
}

void CDispFade::FadeInStart(float speed, int colour) {
    fade_speed = speed;
    mode = 2;
    fade_colour = colour;
}

void CDispFade::FadeInit(float initial_rate) {
    fade_rate = initial_rate;
    mode = -1;
    fade_colour = 0;
}

float CDispFade::GetRate() {
    return fade_rate;
}

float CDispFade::FadeOut(sceVif1Packet *packet) {
    if (mode != 1) {
        return fade_rate;
    }

    if (fade_rate == 0.0f && fade_speed == 0.0f) {
        return fade_rate;
    }

    fade_rate += fade_speed;

    if (fade_rate > 128.0f) {
        fade_rate = 128.0f;
    }

    for (s32 x = 0; x < 640; x += 64) {
        if (fade_colour != 0) {
            for (s32 y = 0; y < SCREEN_HALF_HEIGHT; y += 16) {
                CRect_i_ rect;
                rect.x = x * 16;
                rect.y = y * 16;
                rect.width = 1024;
                rect.height = 256;
                MGFillBox(rect, 255, 255, 255, static_cast<u8>(fade_rate));
            }
        } else {
            for (s32 y = 0; y < SCREEN_HALF_HEIGHT; y += 16) {
                CRect_i_ rect;
                rect.x = x * 16;
                rect.y = y * 16;
                rect.width = 1024;
                rect.height = 256;
                MGFillBox(rect, 0, 0, 0, static_cast<u8>(fade_rate));
            }
        }
    }

    if (fade_rate >= 128.0f) {
        fade_rate = 128.0f;
        fade_speed = 0.0f;
    }

    return fade_rate;
}

float CDispFade::FadeIn(sceVif1Packet *packet) {
    if (mode != 2) {
        return fade_rate;
    }

    if (fade_rate == 0.0f && fade_speed == 0.0f) {
        return fade_rate;
    }

    fade_rate -= fade_speed;

    if (fade_rate < 0.0f) {
        fade_rate = 0.0f;
    }

    for (s32 x = 0; x < 640; x += 64) {
        if (fade_colour != 0) {
            for (s32 y = 0; y < SCREEN_HALF_HEIGHT; y += 16) {
                CRect_i_ rect;
                rect.x = x * 16;
                rect.y = y * 16;
                rect.width = 1024;
                rect.height = 256;
                MGFillBox(rect, 255, 255, 255, static_cast<u8>(fade_rate));
            }
        } else {
            for (s32 y = 0; y < SCREEN_HALF_HEIGHT; y += 16) {
                CRect_i_ rect;
                rect.x = x * 16;
                rect.y = y * 16;
                rect.width = 1024;
                rect.height = 256;
                MGFillBox(rect, 0, 0, 0, static_cast<u8>(fade_rate));
            }
        }
    }

    if (fade_rate <= 0.0f) {
        fade_rate = 0.0f;
        fade_speed = 0.0f;
        mode = -1;
    }

    return fade_rate;
}
