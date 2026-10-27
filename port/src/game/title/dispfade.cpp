#include "title/dispfade.hpp"

#include <algorithm>

#include "mglib.hpp"
#include "rect.hpp"

namespace dc::game {

namespace {

// Covers the screen in 64x16-pixel tiles; rect coordinates are in sixteenths of a pixel.
void DrawFadeCover(float rate, s32 colour) {
    const u8 shade = colour != 0 ? 255 : 0;
    for (s32 x = 0; x < 640; x += 64) {
        for (s32 y = 0; y < SCREEN_HALF_HEIGHT; y += 16) {
            const CRect_i_ rect(x * 16, y * 16, 1024, 256);
            MGFillBox(rect, shade, shade, shade, static_cast<u8>(rate));
        }
    }
}

} // namespace

void CDispFade::FadeOutStart(float speed, int colour) {
    fade_speed = speed;
    mode = TitleFadeMode::Out;
    fade_colour = colour;
}

void CDispFade::FadeInStart(float speed, int colour) {
    fade_speed = speed;
    mode = TitleFadeMode::In;
    fade_colour = colour;
}

void CDispFade::FadeInit(float initial_rate) {
    fade_rate = initial_rate;
    mode = TitleFadeMode::Idle;
    fade_colour = 0;
}

float CDispFade::GetRate() {
    return fade_rate;
}

float CDispFade::FadeOut(sceVif1Packet * /*packet*/) {
    if (mode != TitleFadeMode::Out) {
        return fade_rate;
    }

    if (fade_rate == 0.0f && fade_speed == 0.0f) {
        return fade_rate;
    }

    fade_rate = std::min(fade_rate + fade_speed, 128.0f);

    DrawFadeCover(fade_rate, fade_colour);

    if (fade_rate >= 128.0f) {
        fade_rate = 128.0f;
        fade_speed = 0.0f;
    }

    return fade_rate;
}

float CDispFade::FadeIn(sceVif1Packet * /*packet*/) {
    if (mode != TitleFadeMode::In) {
        return fade_rate;
    }

    if (fade_rate == 0.0f && fade_speed == 0.0f) {
        return fade_rate;
    }

    fade_rate = std::max(fade_rate - fade_speed, 0.0f);

    DrawFadeCover(fade_rate, fade_colour);

    if (fade_rate <= 0.0f) {
        fade_rate = 0.0f;
        fade_speed = 0.0f;
        mode = TitleFadeMode::Idle;
    }

    return fade_rate;
}

} // namespace dc::game
