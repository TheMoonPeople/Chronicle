#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "title/bombeffect.hpp"

#include "camera.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

void CBombEffect::Draw(CCamera *camera, int dust_only) {
    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR position;
    int           top_left[4];
    int           bottom_right[4];
    int           top_right[4];
    int           bottom_left[4];
    sceGsAlpha    alpha;
    sceGsZbuf     zbuf;
    s32           i;

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

    for (i = 0; i < 8; i++) {
        if (active[i] != 1) {
            continue;
        }

        if (counters[i] < 0) {
            continue;
        }

        direction[0] = camera_position[0] - positions[i][0];
        direction[1] = 0.0f;
        direction[2] = camera_position[2] - positions[i][2];
        sceVu0Normalize(direction, direction);
        direction[0] *= 7.0f * i + phases[i];
        direction[2] *= 7.0f * i + phases[i];

        position[0] = direction[0] + positions[i][0];
        position[1] = direction[1] + positions[i][1];
        position[2] = direction[2] + positions[i][2];
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

        if (i < 5) {
            CRect_i_ rect;

            if (dust_only != 0) {
                continue;
            }

            rect.x = 0;
            rect.y = 0;
            rect.width = 128;
            rect.height = 128;
            set3DSprite(Vif1Packet, TexManager.GetTexture("bakuha1", -1), rect, top_left, top_right, bottom_left, bottom_right, static_cast<u8>(alphas[i]));
        } else {
            CRect_i_ rect;

            rect.x = 0;
            rect.y = 0;
            rect.width = 128;
            rect.height = 128;
            set3DSprite(Vif1Packet, TexManager.GetTexture("sunakemuri", -1), rect, top_left, top_right, bottom_left, bottom_right, static_cast<u8>(alphas[i]));
        }
    }

    MGSetGsALPHA(0);
    MGSetGsZBUF(0);
}

void CBombEffect::Step() {
    s32 i;

    for (i = 0; i < 5; i++) {
        if (active[i] != 1) {
            continue;
        }

        switch (phases[i]) {
            case BOMB_PHASE_BURST:
                counters[i]++;
                sizes[i] += 8.0f;
                alphas[i] += 4.0f;

                if (counters[i] < 3) {
                    break;
                }

                counters[i] = 0;
                phases[i]++;
                break;
            case BOMB_PHASE_BLOOM:
                counters[i]++;
                sizes[i] += 1.0f;
                alphas[i] += 8.0f;

                if (counters[i] < 4) {
                    break;
                }

                counters[i] = 0;
                phases[i]++;
                break;
            case BOMB_PHASE_LINGER:
                counters[i]++;
                sizes[i] += 0.3f;
                alphas[i] -= 3.0f;

                if (counters[i] < 20) {
                    break;
                }

                counters[i] = 0;
                phases[i]++;
                break;
            case BOMB_PHASE_DISSIPATE:
                counters[i]++;
                sizes[i] -= 0.1f;
                alphas[i] -= 2.0f;

                if (counters[i] < 80) {
                    break;
                }

                active[i] = 0;
                break;
        }
    }

    for (i = 5; i < 8; i++) {
        if (active[i] != 1) {
            continue;
        }

        switch (phases[i]) {
            case BOMB_PHASE_BURST:
                counters[i]++;
                sizes[i] += 16.0f;

                if (counters[i] < 3) {
                    break;
                }

                counters[i] = 0;
                phases[i]++;
                break;
            case BOMB_PHASE_BLOOM:
                counters[i]++;
                sizes[i] += 8.0f;

                if (counters[i] < 4) {
                    break;
                }

                counters[i] = 0;
                phases[i]++;
                break;
            case BOMB_PHASE_LINGER:
                counters[i]++;
                sizes[i] += 0.3f;

                if (counters[i] < 20) {
                    break;
                }

                counters[i] = 0;
                phases[i]++;
                break;
            case BOMB_PHASE_DISSIPATE:
                counters[i]++;
                sizes[i] -= 0.1f;
                alphas[i] -= 2.0f;

                if (counters[i] < 80) {
                    break;
                }

                active[i] = 0;
                break;
        }
    }
}
