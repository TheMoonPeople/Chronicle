#include "title/bombeffect.hpp"

#include <array>

#include "camera.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

namespace {

constexpr s32 kBillboardCount = 8;
constexpr s32 kFireCount = 5;

} // namespace

void CBombEffect::Draw(CCamera *camera, int dust_only) {
    sceVu0FVECTOR      camera_position;
    sceVu0FVECTOR      direction;
    sceVu0FVECTOR      position;
    std::array<int, 4> top_left{};
    std::array<int, 4> bottom_right{};

    camera->GetPos(camera_position);

    sceGsAlpha alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    MGSetGsALPHA(&alpha);

    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    CRect_i_ rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = 128;
    rect.height = 128;

    for (s32 i = 0; i < kBillboardCount; i++) {
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

        if (MGRotTransPers3DSprite(top_left.data(), bottom_right.data(), position, sizes[i], sizes[i] / 2.0f, 0) != 1) {
            continue;
        }

        std::array<int, 4> top_right = {bottom_right[0], top_left[1], top_left[2], top_left[3]};
        std::array<int, 4> bottom_left = {top_left[0], bottom_right[1], bottom_right[2], bottom_right[3]};

        if (i < kFireCount) {
            if (dust_only != 0) {
                continue;
            }

            set3DSprite(Vif1Packet, TexManager.GetTexture("bakuha1", -1), rect, top_left.data(), top_right.data(), bottom_left.data(), bottom_right.data(), static_cast<u8>(alphas[i]));
        } else {
            set3DSprite(Vif1Packet, TexManager.GetTexture("sunakemuri", -1), rect, top_left.data(), top_right.data(), bottom_left.data(), bottom_right.data(), static_cast<u8>(alphas[i]));
        }
    }

    MGSetGsALPHA(nullptr);
    MGSetGsZBUF(nullptr);
}

void CBombEffect::Step() {
    for (s32 i = 0; i < kFireCount; i++) {
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
            default:
                break;
        }
    }

    for (s32 i = kFireCount; i < kBillboardCount; i++) {
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
            default:
                break;
        }
    }
}

} // namespace dc::game
