#include "title/logo.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include <algorithm>
#include <array>

#include "camera.hpp"
#include "character.hpp"
#include "main.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "title/title.hpp"
#include "title/titleloop.hpp"

namespace dc::game {

namespace {

struct SparkWindow {
    float start;
    float end;
};

// Open motion-time interval during which each sparkle is drawn.
constexpr std::array<SparkWindow, CLogo::kLetterCount> kSparkWindows = {
    {
     {.start = 1.0f, .end = 41.0f},
     {.start = 10.0f, .end = 50.0f},
     {.start = 20.0f, .end = 60.0f},
     {.start = 30.0f, .end = 70.0f},
     {.start = 40.0f, .end = 80.0f},
     {.start = 50.0f, .end = 90.0f},
     {.start = 60.0f, .end = 100.0f},
     {.start = 70.0f, .end = 110.0f},
     {.start = 80.0f, .end = 105.0f},
     }
};

constexpr std::array<const char *, CLogo::kLetterCount> kLetterFrameNames = {
    "logo_D1", "logo_A1", "logo_R1", "logo_K1", "logo_C1", "logo_L1", "logo_O1", "logo_U1", "logo_D2"};

constexpr int kFadeStep = 4;
constexpr int kMaxAlpha = 128;
constexpr int kLetterLight = 40;

} // namespace

CLogo::CLogo() : alpha{}, count(0), unused_count(0) {}

void CLogo::Init() {
    alpha.fill(0);
    light.fill(kLetterLight);

    count = 0;
    unused_count = 0;
    position[0] = 0.0f;
    position[1] = 17.599f;
    position[2] = -19.099f;
    Logo.frame->SetPosition(position);
    Logo.motion_type.state.time = 1.0f;

    for (int i = 0; i < kLetterCount; i++) {
        Spark[i].motion_type.state.time = 1.0f;
    }
}

void CLogo::Fade() {
    if (count >= 2000) {
        return;
    }

    count++;

    // Each letter starts fading in ten ticks after the previous one.
    for (int i = 0; i < kLetterCount; i++) {
        if (count > i * 10) {
            alpha[i] = std::min(alpha[i] + kFadeStep, kMaxAlpha);
        }
    }
}

void CLogo::FadeOut() {
    for (int &letter_alpha : alpha) {
        if (letter_alpha > 0) {
            letter_alpha -= kFadeStep;
        }
    }
}

void CLogo::Sparkdraw(float time) {
    sceVu0FVECTOR color = {0.0f, 0.0f, 0.0f, 120.0f};

    MGSetAmbient(color);

    for (int i = 0; i < kLetterCount; i++) {
        if (time > kSparkWindows[i].start && time < kSparkWindows[i].end) {
            Spark[i].Draw();
        }
    }
}

void CLogo::Draw() {
    sceVu0FVECTOR color = {0.0f, 0.0f, 0.0f, 128.0f};

    for (int i = 0; i < kLetterCount; i++) {
        CFrame *frame = Logo.frame->SearchFrame(const_cast<char *>(kLetterFrameNames[i]));
        color[3] = static_cast<float>(alpha[i]);
        light[i] = kLetterLight;
        color[0] = static_cast<float>(light[i]) / 2.0f;
        color[1] = static_cast<float>(light[i]) / 4.0f;
        color[2] = static_cast<float>(light[i]);
        MGSetAmbient(color);
        MGDraw(frame);
    }
}

void CLogo::Move() {
    if (Fade4 < 128) {
        Fade4 += 4;
    }

    if (Fade4 > 64) {
        FadeOut();
    }
}

} // namespace dc::game
