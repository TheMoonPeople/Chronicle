#include "title/sprite.hpp"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <cmath>
#include <cstddef>
#include <cstdlib>

#include "mathutil.hpp"
#include "mglib.hpp"
#include "texture.hpp"
#include "title/scfader.hpp"
#include "title/title.hpp"

namespace dc::game {

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
    for (std::size_t point = 0; point < x.size(); point++) {
        x[point] = 0.0f;
        y[point] = 150.0f;
    }

    angle = 2.57f;
    started = false;
}

void CSprite::Init() {
    for (std::size_t point = 0; point < x.size(); point++) {
        x[point] = 0.0f;
        y[point] = 150.0f;
    }

    angle = 2.57f;
    started = false;
}

void CSprite::Move() {
    started = true;

    for (std::size_t point = x.size() - 1; point > 0; point--) {
        x[point] = x[point - 1];
        y[point] = y[point - 1];
    }

    if (x[0] < 700.0f) {
        float target_x = 700.0f;
        float target_y = 94.0f;

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

        const float target_angle = atan2(target_x - x[0], target_y - y[0]);

        if (angle > target_angle) {
            angle = angle - 0.08f;
        }

        if (angle < target_angle) {
            angle += 0.08f;
        }

        x[0] = x[0] + static_cast<float>(sin(angle) * 7.5);
        y[0] = y[0] + static_cast<float>(cos(angle) * 7.5);
    }
}

void CSprite::Draw() {
    if (x[0] < 700.0f) {
        u_char size[5] = {250, 60, 40, 20, 20};

        for (int point = 0; point < 5; point++) {
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat01", -1), CRect<int>(static_cast<int>(x[point]), static_cast<int>(y[point] - 16.0f), 64, 64), CRect<int>(0, 0, 64, 64), static_cast<u_char>(CFade.Get(rand() % size[point])));
        }

        set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat02", -1), CRect<int>(static_cast<int>(8.0f + (x[0] - static_cast<float>(rand() % 8))), static_cast<int>(y[0] + static_cast<float>(rand() % 16)), 16, 16), CRect<int>(0, 0, 32, 32), static_cast<u_char>(CFade.Get(rand() % 128)));

        set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat02", -1), CRect<int>(static_cast<int>(8.0f + (x[0] - static_cast<float>(rand() % 16))), static_cast<int>(y[0] + static_cast<float>(rand() % 24) - 6.0f), 16, 16), CRect<int>(0, 0, 32, 32), static_cast<u_char>(CFade.Get(rand() % 128)));

        {
            u_char alpha[11] = {90, 80, 70, 60, 50, 40, 30, 20, 20, 10, 10};

            for (int point = 1; point < 12; point++) {
                set2DSprite(GetVif1Packet(), TexManager.GetTexture("pat02", -1), CRect<int>(static_cast<int>(8.0f + (x[point] - static_cast<float>(rand() % 8))), static_cast<int>(4.0f + (y[point] + static_cast<float>(rand() % 16))), 8, 8), CRect<int>(0, 0, 32, 32), static_cast<u_char>(CFade.Get(alpha[point])));
            }
        }
    }
}

int CSprite::Se() {
    return started;
}

} // namespace dc::game
