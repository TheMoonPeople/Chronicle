#pragma helper_mask_gpr 0x30

#include "title/majinbeem.hpp"

#include <cmath>

#include "camera.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

/* The tail, which is drawn where its elements already are: the head is the only element `Step`
   moves and every other one is a place the head has been. */
void CMajinBeem::Draw(CCamera *camera) {
    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR position;
    int           top_left[4];
    int           bottom_right[4];
    int           top_right[4];
    int           bottom_left[4];
    sceGsAlpha    alpha;
    sceGsZbuf     zbuf;
    int           i;

    camera->GetPos(camera_position);

    alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    MGSetGsALPHA(&alpha);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    for (i = 0; i < 59; i++) {
        if (active != 1) {
            continue;
        }

        if (counters[i] < 0) {
            continue;
        }

        position[0] = positions[i][0];
        position[1] = positions[i][1];
        position[2] = positions[i][2];
        position[3] = 1.0f;

        if (MGRotTransPers3DSprite(top_left, bottom_right, position, sizes[i], sizes[i] / 2.0f, 0) != 1) {
            continue;
        }

        top_right[0] = bottom_right[0];
        top_right[1] = top_left[1];
        top_right[2] = top_left[2];
        top_right[3] = top_left[3];
        bottom_left[0] = top_left[0];
        bottom_left[1] = bottom_right[1];
        bottom_left[2] = bottom_right[2];
        bottom_left[3] = bottom_right[3];

        {
            CRect_i_ rect;

            rect.x = 0;
            rect.y = 0;
            rect.width = 128;
            rect.height = 128;
            set3DSprite(Vif1Packet, TexManager.GetTexture("beem", -1), rect, top_left, top_right, bottom_left, bottom_right, (u_char) alphas[i]);
        }
    }

    MGSetGsALPHA(0);
    MGSetGsZBUF(0);
}

/* The head, which the caller places rather than the effect: whatever is carrying the beam hands
   its own position in every tick.
   The head is projected twice and only the depth of the second projection is kept. The second is
   the same point moved fifteen units towards the camera, so all four corners take a depth the
   thing that fired the beam cannot be in front of. */
void CMajinBeem::Draw2(CCamera *camera, float *head, float *source) {
    sceVu0FVECTOR direction;
    sceVu0FVECTOR position;
    int           top_left[4];
    int           bottom_right[4];
    int           near_top_left[4];
    int           near_bottom_right[4];
    int           top_right[4];
    int           bottom_left[4];
    sceGsAlpha    alpha;
    sceGsZbuf     zbuf;

    camera->GetPos(direction);
    direction[0] -= source[0];
    direction[1] -= source[1];
    direction[2] -= source[2];
    sceVu0Normalize(direction, direction);
    direction[0] *= 15.0f;
    direction[1] *= 15.0f;
    direction[2] *= 15.0f;

    alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    MGSetGsALPHA(&alpha);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    if (alphas[59] > 0.0f) {
        alphas[59] -= 1.0f;
        sizes[59] -= 0.25f;
        sceVu0CopyVector(positions[59], head);

        position[0] = positions[59][0];
        position[1] = positions[59][1];
        position[2] = positions[59][2];
        position[3] = 1.0f;

        if (MGRotTransPers3DSprite(top_left, bottom_right, position, sizes[59], sizes[59] / 2.0f, 0) == 1) {
            CRect_i_ rect;

            top_right[0] = bottom_right[0];
            top_right[1] = top_left[1];
            top_right[2] = top_left[2];
            top_right[3] = top_left[3];
            bottom_left[0] = top_left[0];
            bottom_left[1] = bottom_right[1];
            bottom_left[2] = bottom_right[2];
            bottom_left[3] = bottom_right[3];

            position[0] += direction[0];
            position[1] += direction[1];
            position[2] += direction[2];

            if (MGRotTransPers3DSprite(near_top_left, near_bottom_right, position, sizes[59], sizes[59] / 2.0f, 0) == 1) {
                bottom_right[2] = near_top_left[2];
                bottom_left[2] = near_top_left[2];
                top_left[2] = near_top_left[2];
                top_right[2] = near_top_left[2];
            }

            rect.x = 0;
            rect.y = 0;
            rect.width = 128;
            rect.height = 128;
            set3DSprite(Vif1Packet, TexManager.GetTexture("beem", -1), rect, top_left, top_right, bottom_left, bottom_right, (u_char) alphas[59]);
        }
    }

    MGSetGsALPHA(0);
    MGSetGsZBUF(0);
}

/* The whole path is fixed on the first tick: the two angles source the head's starting point to the
   target are taken once and nothing steers afterwards, so the beam is straight. */
void CMajinBeem::Step() {
    int substep;
    int element;

    switch (state) {
        case MAJIN_BEEM_AIM:
            pitch = atan2f(target[0] - positions[0][0], target[1] - positions[0][1]);
            yaw = atan2f(target[0] - positions[0][0], target[2] - positions[0][2]);
            state++;
            break;

        case MAJIN_BEEM_TRAVEL:
            for (substep = 0; substep < 3; substep++) {
                for (element = 0; element < 58; element++) {
                    sceVu0CopyVector(positions[58 - element], positions[57 - element]);
                    counters[element]++;
                }

                positions[0][1] += speed / 3.0f * cos(pitch);
                positions[0][0] += speed / 3.0f * sin(yaw);
                positions[0][2] += speed / 3.0f * cos(yaw);
            }

            if (counters[58] >= 200) {
                active = false;
            }

            break;
    }
}
