#include "title/seireiking.hpp"

#include <array>
#include <cstdlib>
#include <utility>

#include "camera.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

namespace {

using Corner = std::array<int, 4>;

constexpr std::array kSparkTextures{"eef01", "eef02", "eef04"};

/* Quarters of the 64x64 "eef05" sheet, picked at random for the outer sparks. */
constexpr std::array<std::pair<int, int>, 4> kGlintOffsets{
    {{0, 0}, {32, 0}, {0, 32}, {32, 32}}
};

/* The jitter is taken in hundredths so that `spread` reads as a whole-unit radius: a spread of 2
   gives an offset anywhere in [-2, 2] to a hundredth. */
float Jitter(int spread) {
    return static_cast<float>((spread * 100) - (rand() % (spread * 200))) / 100.0f;
}

void SetAdditiveBlend() {
    sceGsAlpha blend = mgAlpha;
    blend.bits.a = 0;
    blend.bits.b = 2;
    blend.bits.c = 0;
    blend.bits.d = 1;
    MGSetGsALPHA(&blend);

    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);
}

void BuildQuadCorners(const Corner &top_left, const Corner &bottom_right, Corner &top_right, Corner &bottom_left) {
    top_right = {bottom_right[0], top_left[1], top_left[2], top_left[3]};
    bottom_left = {top_left[0], bottom_right[1], bottom_right[2], bottom_right[3]};
}

void DrawQuad(const char *texture, const CRect_i_ &source, Corner &top_left, Corner &top_right, Corner &bottom_left, Corner &bottom_right, float alpha) {
    set3DSprite(Vif1Packet, TexManager.GetTexture(const_cast<char *>(texture), -1), source, top_left.data(), top_right.data(), bottom_left.data(), bottom_right.data(), static_cast<u8>(alpha));
}

} // namespace

void CSeireiKing::Draw(float *origin, float size, int spread) {
    sceVu0FVECTOR point;
    Corner        top_left;
    Corner        bottom_right;
    Corner        top_right;
    Corner        bottom_left;

    SetAdditiveBlend();

    sceVu0CopyVector(position[0], origin);
    position[0][0] += Jitter(spread);
    position[0][1] += Jitter(spread);
    position[0][2] += Jitter(spread);

    for (int i = 0; i < 19; i++) {
        if (count[i] < 0) {
            continue;
        }

        point[0] = position[i][0];
        point[1] = position[i][1];
        point[2] = position[i][2];
        point[3] = 1.0f;

        if (MGRotTransPers3DSprite(top_left.data(), bottom_right.data(), point, size, size / 2.0f, 0) == 1) {
            BuildQuadCorners(top_left, bottom_right, top_right, bottom_left);
            const char *texture = kSparkTextures[rand() % 3];
            DrawQuad(texture, CRect_i_(0, 0, 64, 64), top_left, top_right, bottom_left, bottom_right, alpha[i]);
        }

        point[0] += Jitter(spread);
        point[1] += Jitter(spread);
        point[2] += Jitter(spread);

        if (MGRotTransPers3DSprite(top_left.data(), bottom_right.data(), point, size, size / 2.0f, 0) == 1) {
            BuildQuadCorners(top_left, bottom_right, top_right, bottom_left);
            const auto [offset_x, offset_y] = kGlintOffsets[rand() % 4];
            DrawQuad("eef05", CRect_i_(offset_x, offset_y, 32, 32), top_left, top_right, bottom_left, bottom_right, alpha[i]);
        }
    }

    MGSetGsALPHA(nullptr);
    MGSetGsZBUF(nullptr);
}

/* The head, which the caller places rather than the effect. It is projected twice and only the
   depth of the second projection is kept, the second being the same point moved twenty-five units
   towards the camera, so the whole sprite takes a depth the thing carrying it cannot be in front
   of. */
void CSeireiKing::Draw2(CCamera *camera, float *head, float *source) {
    sceVu0FVECTOR direction;
    sceVu0FVECTOR point;
    Corner        top_left;
    Corner        bottom_right;
    Corner        near_top_left;
    Corner        near_bottom_right;
    Corner        top_right;
    Corner        bottom_left;

    camera->GetPos(direction);
    direction[0] -= source[0];
    direction[1] -= source[1];
    direction[2] -= source[2];
    sceVu0Normalize(direction, direction);
    direction[0] *= 25.0f;
    direction[1] *= 25.0f;
    direction[2] *= 25.0f;

    SetAdditiveBlend();

    if (alpha[19] > 0.0f) {
        alpha[19] -= 1.0f;
        sceVu0CopyVector(position[19], head);

        point[0] = position[19][0];
        point[1] = position[19][1];
        point[2] = position[19][2];
        point[3] = 1.0f;

        if (MGRotTransPers3DSprite(top_left.data(), bottom_right.data(), point, 5.0f, 2.5f, 0) == 1) {
            BuildQuadCorners(top_left, bottom_right, top_right, bottom_left);

            point[0] += direction[0];
            point[1] += direction[1];
            point[2] += direction[2];

            if (MGRotTransPers3DSprite(near_top_left.data(), near_bottom_right.data(), point, 5.0f, 2.5f, 0) == 1) {
                bottom_right[2] = near_top_left[2];
                bottom_left[2] = near_top_left[2];
                top_left[2] = near_top_left[2];
                top_right[2] = near_top_left[2];
            }

            DrawQuad("eef01", CRect_i_(0, 0, 64, 64), top_left, top_right, bottom_left, bottom_right, alpha[19]);
        }
    }

    MGSetGsALPHA(nullptr);
    MGSetGsZBUF(nullptr);
}

/* The trail is shifted rather than advanced: every element takes the position of the one in front
   of it and `Draw` writes the new head, so an element's own age is all that decides how it looks. */
void CSeireiKing::Step() {
    for (int i = 0; i < 18; i++) {
        sceVu0CopyVector(position[18 - i], position[17 - i]);
        count[i]++;
    }
}

} // namespace dc::game
