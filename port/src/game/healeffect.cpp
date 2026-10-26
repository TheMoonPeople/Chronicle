#include "healeffect.hpp"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "character.hpp"
#include "dun/gameloop.hpp"
#include "hitmark.hpp"
#include "shot_freefuncs.hpp"
#include "texture.hpp"

void CHealEffect::Set(float *world) {
    sceVu0CopyVector(this->position, world);
    this->active = true;

    for (int i = 0; i < 32; i++) {
        this->radius[i] = 3.0f + 6.0f * (float) rand() / 2.1474836e9f;
        this->angle[i] = TWO_PI * (float) rand() / 2.1474836e9f - PI;
        this->angular_velocity[i] = 0.034906585f * (5.0f * (float) rand() / 2.1474836e9f);
        this->size[i] = 0.4f + 0.6f * (float) rand() / 2.1474836e9f;
        this->alpha[i] = 0.0f;
        // The drawn phase is the one below; the value rolled here is discarded.
        this->phase[i] = 0.1f * (float) rand() / 2.1474836e9f;
        this->phase[i] = 0.0f;
        this->particle_offset[i][1] = 2.0f + 10.0f * (float) rand() / 2.1474836e9f;
    }
}

void CHealEffect::Step() {
    if (this->active != 0) {
        float         offset[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        sceVu0FMATRIX unit;
        sceVu0FMATRIX rotation;
        int           expired = 0;

        for (int i = 0; i < 32; i++) {
            if (this->phase[i] < PI) {
                // The phase runs a half sine, so the particle rises and fades once.
                float rise = sinf(this->phase[i]);

                this->phase[i] += 0.052359879f;
                this->alpha[i] = 128.0f * rise - 32.0f;

                if (this->alpha[i] <= 0.0f) {
                    this->alpha[i] = 0.0f;
                }

                this->angle[i] += this->angular_velocity[i];

                if (this->angle[i] > PI) {
                    this->angle[i] -= TWO_PI;
                }

                if (this->angle[i] < -PI) {
                    this->angle[i] += TWO_PI;
                }

                offset[0] = 0.0f;
                offset[1] = this->particle_offset[i][1] + 0.3f * (float) rand() / 2.1474836e9f;
                offset[2] = rise * this->radius[i];
                sceVu0UnitMatrix(unit);
                sceVu0RotMatrixY(rotation, unit, this->angle[i]);
                sceVu0ApplyMatrix(offset, rotation, offset);
                sceVu0CopyVector(this->particle_offset[i], offset);
            } else {
                expired++;

                if (expired == 32) {
                    this->active = false;
                    printf("emd!!\n");
                }
            }
        }
    }
}

void CHealEffect::Draw() {
    if (this->active != 0) {
        float world[4];
        // Unread; Step builds its particle offsets from the same template.
        float     offset[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        CTexture *texture = TexManager.GetTexture(HealEffectTextureName, -1);

        sceVu0CopyVector(this->position, CharaMain.pos);

        for (int i = 0; i < 32; i++) {
            if (this->phase[i] < PI) {
                world[0] = this->position[0] + this->particle_offset[i][0];
                world[1] = this->position[1] + this->particle_offset[i][1];
                world[2] = this->position[2] + this->particle_offset[i][2];
                BtSet3DCellModel(world, texture, this->size[i], 0x10, 0, 0x20, 0x20, (int) this->alpha[i]);
            }
        }
    }
}
