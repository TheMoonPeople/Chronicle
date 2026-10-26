#include "mathconst.h"
#include "runeffect.hpp"

#include <cmath>

float waveAnimeCnt[32];
int   sw;

void initBlendCnt(int count, float scale) {
    for (int i = 0; i < count; i++) {
        float degrees = (360.0f / static_cast<float>(count)) * static_cast<float>(i);
        float wave = std::sin(PI * degrees / 180.0f);
        waveAnimeCnt[i] = wave * scale;
    }
}

void CRunEffect::Lighting(int enabled) {
    lighting = enabled;
}

void CRunEffect::Set(float *origin) {
    int slot = -1;

    for (int i = 0; i < 8; i++) {
        if (life[i] == 0) {
            slot = i;
        }
    }

    if (slot != -1) {
        position[slot][0] = origin[0];
        position[slot][1] = origin[1];
        position[slot][2] = origin[2];
        position[slot][3] = 1.0f;
        velocity_y[slot] = 0.3f;
        life[slot] = 16;
    }
}

void CRunEffect::Step() {
    for (int i = 0; i < 8; i++) {
        if (life[i] != 0) {
            life[i]--;
            velocity_y[i] -= 0.03f;
            position[i][1] += velocity_y[i];
        }
    }
}

CRunEffect::CRunEffect() {
    for (int i = 0; i < 8; i++) {
        velocity_y[i] = 0.0f;
        life[i] = 0;
    }

    lighting = false;
}
