#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "title/seireiking.hpp"

#include <cstdlib>

#include "camera.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

/* The jitter is taken in hundredths so that `spread` reads as a whole-unit radius: the caller in
   the movie asks for 2 and gets an offset anywhere in [-2, 2] to a hundredth. */
void CSeireiKing::Draw(float *origin, float size, int spread) {
    sceVu0FVECTOR point;
    int           corner0[4];
    int           corner1[4];
    int           corner2[4];
    int           corner3[4];
    sceGsAlpha    blend;
    sceGsZbuf     zbuf;
    int           i;

    blend = mgAlpha;
    blend.bits.a = 0;
    blend.bits.b = 2;
    blend.bits.c = 0;
    blend.bits.d = 1;
    MGSetGsALPHA(&blend);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    sceVu0CopyVector(this->position[0], origin);
    position[0][0] += (float) (spread * 100 - rand() % (spread * 200)) / 100.0f;
    position[0][1] += (float) (spread * 100 - rand() % (spread * 200)) / 100.0f;
    position[0][2] += (float) (spread * 100 - rand() % (spread * 200)) / 100.0f;

    for (i = 0; i < 19; i++) {
        if (count[i] < 0) {
            continue;
        }

        point[0] = position[i][0];
        point[1] = position[i][1];
        point[2] = position[i][2];
        point[3] = 1.0f;

        if (MGRotTransPers3DSprite(corner0, corner1, point, size, size / 2.0f, 0) == 1) {
            corner2[0] = corner1[0];
            corner2[1] = corner0[1];
            corner2[2] = corner0[2];
            corner2[3] = corner0[3];
            corner3[0] = corner0[0];
            corner3[1] = corner1[1];
            corner3[2] = corner1[2];
            corner3[3] = corner1[3];

            switch (rand() % 3) {
                case 0: {
                    CRect_i_ rect;

                    rect.x = 0;
                    rect.y = 0;
                    rect.width = 64;
                    rect.height = 64;
                    set3DSprite(Vif1Packet, TexManager.GetTexture("eef01", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[i]);
                    break;
                }

                case 1: {
                    CRect_i_ rect;

                    rect.x = 0;
                    rect.y = 0;
                    rect.width = 64;
                    rect.height = 64;
                    set3DSprite(Vif1Packet, TexManager.GetTexture("eef02", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[i]);
                    break;
                }

                case 2: {
                    CRect_i_ rect;

                    rect.x = 0;
                    rect.y = 0;
                    rect.width = 64;
                    rect.height = 64;
                    set3DSprite(Vif1Packet, TexManager.GetTexture("eef04", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[i]);
                    break;
                }
            }
        }

        point[0] += (float) (spread * 100 - rand() % (spread * 200)) / 100.0f;
        point[1] += (float) (spread * 100 - rand() % (spread * 200)) / 100.0f;
        point[2] += (float) (spread * 100 - rand() % (spread * 200)) / 100.0f;

        if (MGRotTransPers3DSprite(corner0, corner1, point, size, size / 2.0f, 0) == 1) {
            corner2[0] = corner1[0];
            corner2[1] = corner0[1];
            corner2[2] = corner0[2];
            corner2[3] = corner0[3];
            corner3[0] = corner0[0];
            corner3[1] = corner1[1];
            corner3[2] = corner1[2];
            corner3[3] = corner1[3];

            switch (rand() % 4) {
                case 0: {
                    CRect_i_ rect;

                    rect.x = 0;
                    rect.y = 0;
                    rect.width = 32;
                    rect.height = 32;
                    set3DSprite(Vif1Packet, TexManager.GetTexture("eef05", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[i]);
                    break;
                }

                case 1: {
                    CRect_i_ rect;

                    rect.x = 32;
                    rect.y = 0;
                    rect.width = 32;
                    rect.height = 32;
                    set3DSprite(Vif1Packet, TexManager.GetTexture("eef05", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[i]);
                    break;
                }

                case 2: {
                    CRect_i_ rect;

                    rect.x = 0;
                    rect.y = 32;
                    rect.width = 32;
                    rect.height = 32;
                    set3DSprite(Vif1Packet, TexManager.GetTexture("eef05", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[i]);
                    break;
                }

                case 3: {
                    CRect_i_ rect;

                    rect.x = 32;
                    rect.y = 32;
                    rect.width = 32;
                    rect.height = 32;
                    set3DSprite(Vif1Packet, TexManager.GetTexture("eef05", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[i]);
                    break;
                }
            }
        }
    }

    MGSetGsALPHA(0);
    MGSetGsZBUF(0);
}

/* The head, which the caller places rather than the effect. It is projected twice and only the
   depth of the second projection is kept, the second being the same point moved twenty-five units
   towards the camera, so the whole sprite takes a depth the thing carrying it cannot be in front
   of. */
void CSeireiKing::Draw2(CCamera *camera, float *head, float *source) {
    sceVu0FVECTOR direction;
    sceVu0FVECTOR point;
    int           corner0[4];
    int           corner1[4];
    int           near0[4];
    int           near1[4];
    int           corner2[4];
    int           corner3[4];
    sceGsAlpha    blend;
    sceGsZbuf     zbuf;

    camera->GetPos(direction);
    direction[0] -= source[0];
    direction[1] -= source[1];
    direction[2] -= source[2];
    sceVu0Normalize(direction, direction);
    direction[0] *= 25.0f;
    direction[1] *= 25.0f;
    direction[2] *= 25.0f;

    blend = mgAlpha;
    blend.bits.a = 0;
    blend.bits.b = 2;
    blend.bits.c = 0;
    blend.bits.d = 1;
    MGSetGsALPHA(&blend);

    zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);

    if (alpha[19] > 0.0f) {
        alpha[19] -= 1.0f;
        sceVu0CopyVector(position[19], head);

        point[0] = position[19][0];
        point[1] = position[19][1];
        point[2] = position[19][2];
        point[3] = 1.0f;

        if (MGRotTransPers3DSprite(corner0, corner1, point, 5.0f, 2.5f, 0) == 1) {
            CRect_i_ rect;

            corner2[0] = corner1[0];
            corner2[1] = corner0[1];
            corner2[2] = corner0[2];
            corner2[3] = corner0[3];
            corner3[0] = corner0[0];
            corner3[1] = corner1[1];
            corner3[2] = corner1[2];
            corner3[3] = corner1[3];

            point[0] += direction[0];
            point[1] += direction[1];
            point[2] += direction[2];

            if (MGRotTransPers3DSprite(near0, near1, point, 5.0f, 2.5f, 0) == 1) {
                corner1[2] = near0[2];
                corner3[2] = near0[2];
                corner0[2] = near0[2];
                corner2[2] = near0[2];
            }

            rect.x = 0;
            rect.y = 0;
            rect.width = 64;
            rect.height = 64;
            set3DSprite(Vif1Packet, TexManager.GetTexture("eef01", -1), rect, corner0, corner2, corner3, corner1, (u8) alpha[19]);
        }
    }

    MGSetGsALPHA(0);
    MGSetGsZBUF(0);
}

/* The trail is shifted rather than advanced: every element takes the position of the one in front
   of it and `Draw` writes the new head, so an element's own age is all that decides how it looks. */
void CSeireiKing::Step() {
    int i;

    for (i = 0; i < 18; i++) {
        sceVu0CopyVector(position[18 - i], position[17 - i]);
        count[i]++;
    }
}
