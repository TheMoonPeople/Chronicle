#include "wind.hpp"

#include <cstdlib>

void CWind::SetDir(float *direction) {
    sceVu0Normalize(dir, direction);
}

void CWind::SetVelocity(float speed) {
    velocity = speed;
}

void CWind::GetWind(float *out) {
    sceVu0ScaleVector(out, dir, velocity);
}

void CWind::GetWindNoise(float *out) {
    sceVu0ScaleVector(out, dir, velocity * gust);
}

void CWind::Step() {
    // A random walk of at most a tenth of the range per step, held inside it.
    gust = gust + 0.2f * ((float) rand() / 2147483648.0f - 0.5f);

    if (gust < 0.0f) {
        gust = 0.0f;
    }

    if (!(gust <= 1.0f)) {
        gust = 1.0f;
    }
}

CWind::CWind() {
    velocity = 0.0f;
    gust = (float) rand() / 2147483648.0f;
}
