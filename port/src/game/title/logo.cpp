#include "title/logo.hpp"

#include <libgraph.h>
#include <libvu0.h>

#include "camera.hpp"
#include "character.hpp"
#include "main.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "title/title.hpp"
#include "title/titleloop.hpp"

CLogo::CLogo() {
    int i;

    for (i = 0; i < 9; i++) {
        alpha[i] = 0;
    }

    count = 0;
    unused_count = 0;
}

void CLogo::Init() {
    int i;

    for (i = 0; i < 9; i++) {
        alpha[i] = 0;
        light[i] = 40;
    }

    count = 0;
    unused_count = 0;
    position[0] = 0.0f;
    position[1] = 17.599f;
    position[2] = -19.099f;
    Logo.frame->SetPosition(position);
    Logo.motion_type.state.time = 1.0f;

    for (i = 0; i < 9; i++) {
        Spark[i].motion_type.state.time = 1.0f;
    }
}

void CLogo::Fade() {
    int i;

    if (count < 2000) {
        count++;

        for (i = 0; i < 9; i++) {
            if (count > i * 10) {
                alpha[i] += 4;

                if (alpha[i] > 128) {
                    alpha[i] = 128;
                }
            }
        }
    }
}

void CLogo::FadeOut() {
    int i;

    for (i = 0; i < 9; i++) {
        if (alpha[i] > 0) {
            alpha[i] -= 4;
        }
    }
}

void CLogo::Sparkdraw(float time) {
    sceVu0FVECTOR color = {0.0f, 0.0f, 0.0f, 120.0f};

    MGSetAmbient(color);

    if (time > 1.0f && time < 41.0f) {
        Spark[0].Draw();
    }

    if (time > 10.0f && time < 50.0f) {
        Spark[1].Draw();
    }

    if (time > 20.0f && time < 60.0f) {
        Spark[2].Draw();
    }

    if (time > 30.0f && time < 70.0f) {
        Spark[3].Draw();
    }

    if (time > 40.0f && time < 80.0f) {
        Spark[4].Draw();
    }

    if (time > 50.0f && time < 90.0f) {
        Spark[5].Draw();
    }

    if (time > 60.0f && time < 100.0f) {
        Spark[6].Draw();
    }

    if (time > 70.0f && time < 110.0f) {
        Spark[7].Draw();
    }

    if (time > 80.0f && time < 105.0f) {
        Spark[8].Draw();
    }
}

void CLogo::Draw() {
    sceVu0FVECTOR color = {0.0f, 0.0f, 0.0f, 128.0f};
    CFrame       *frame;
    int           i;

    for (i = 0; i < 9; i++) {
        char *name[9] = {
            "logo_D1", "logo_A1", "logo_R1", "logo_K1", "logo_C1",
            "logo_L1", "logo_O1", "logo_U1", "logo_D2"};

        frame = Logo.frame->SearchFrame(name[i]);
        color[3] = (float) alpha[i];
        light[i] = 40;
        color[0] = (float) light[i] / 2.0f;
        color[1] = (float) light[i] / 4.0f;
        color[2] = (float) light[i];
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
