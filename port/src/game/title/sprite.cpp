#pragma helper_mask_gpr 0x30

#include "title/sprite.hpp"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <cmath>
#include <cstdlib>

#include "mathutil.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "title/scfader.hpp"
#include "title/title.hpp"

/* The rectangle every 2D draw takes, declared here rather than reached through rect.h because the
   two constructors that header states are not this file's: every rectangle here is built by one
   that assigns x, y, w and h in that order, and rect.h's assigns them in the other. */
template <class T>
class CRect {
public:
    T x; /**< Left edge. */
    T y; /**< Top edge. */
    T w; /**< Width. */
    T h; /**< Height. */

    CRect() {}

    CRect(T left, T top, T width, T height) {
        x = left;
        y = top;
        w = width;
        h = height;
    }
};

void set2DSprite(sceVif1Packet *packet, CTexture *texture, const CRect<int> &dst, const CRect<int> &src, u_char alpha);

CSprite::CSprite() {
    int i;

    for (i = 0; i < 12; i++) {
        x[i] = 0.0f;
        y[i] = 150.0f;
    }

    angle = 2.57f;
    started = false;
}

void CSprite::Init() {
    int i;

    for (i = 0; i < 12; i++) {
        x[i] = 0.0f;
        y[i] = 150.0f;
    }

    angle = 2.57f;
    started = false;
}

void CSprite::Move() {
    float target_x;
    float target_y;
    float target_angle;
    int   i;

    if (started == 0) {
        started = true;
    }

    for (i = 0; i < 11; i++) {
        x[11 - i] = x[10 - i];
        y[11 - i] = y[10 - i];
    }

    if (x[0] < 700.0f) {
        if (x[0] < 700.0f) {
            target_x = 700.0f;
            target_y = 94.0f;
        }

        if (x[0] < 510.0f) {
            target_x = 510.0f;
            target_y = 154.0f;
        }

        if (x[0] < 400.0f) {
            target_x = 400.0f;
            target_y = 194.0f;
        }

        if (x[0] < 320.0f) {
            target_x = 320.0f;
            target_y = 154.0f;
        }

        if (x[0] < 230.0f) {
            target_x = 250.0f;
            target_y = 94.0f;
        }

        if (x[0] < 150.0f) {
            target_x = 150.0f;
            target_y = 154.0f;
        }

        target_angle = (float) atan2(target_x - x[0], target_y - y[0]);

        if (this->angle > target_angle) {
            this->angle = this->angle - 0.08f;
        }

        if (this->angle < target_angle) {
            this->angle += 0.08f;
        }

        x[0] = x[0] + (float) (sin(this->angle) * 7.5);
        y[0] = y[0] + (float) (cos(this->angle) * 7.5);
    }
}

void CSprite::Draw() {
    int i;

    if (x[0] < 700.0f) {
        u_char size[5] = {250, 60, 40, 20, 20};

        for (i = 0; i < 5; i++) {
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat01", -1), CRect<int>((int) x[i], (int) (y[i] - 16.0f), 64, 64), CRect<int>(0, 0, 64, 64), (u_char) CFade.Get(rand() % size[i]));
        }

        set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat02", -1), CRect<int>((int) (8.0f + (x[0] - (float) (rand() % 8))), (int) (y[0] + (float) (rand() % 16)), 16, 16), CRect<int>(0, 0, 32, 32), (u_char) CFade.Get(rand() % 128));

        set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat02", -1), CRect<int>((int) (8.0f + (x[0] - (float) (rand() % 16))), (int) (y[0] + (float) (rand() % 24) - 6.0f), 16, 16), CRect<int>(0, 0, 32, 32), (u_char) CFade.Get(rand() % 128));

        {
            u_char alpha[11] = {90, 80, 70, 60, 50, 40, 30, 20, 20, 10, 10};

            for (i = 1; i < 12; i++) {
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat02", -1), CRect<int>((int) (8.0f + (x[i] - (float) (rand() % 8))), (int) (4.0f + (y[i] + (float) (rand() % 16))), 8, 8), CRect<int>(0, 0, 32, 32), (u_char) CFade.Get(alpha[i]));
            }
        }
    }
}

int CSprite::Se() {
    return started;
}
