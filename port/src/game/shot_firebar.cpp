#include "shot_firebar.hpp"

#include <libvu0.h>

#include <cstdlib>

#include "collisiondata.hpp"
#include "dun/gameloop.hpp"
#include "itemdata.hpp"
#include "mglib.hpp"
#include "motionmodel.hpp"
#include "shot_utils.hpp"
#include "texture.hpp"
#include "weaponelement.hpp"

namespace dc::game {

namespace {

constexpr int kStreamParticles = 24;
constexpr int kInactive = -1;
constexpr int kHitInterval = 30;

} // namespace

int CSHOT_FIREBAR::Init(float *origin, float *direction, int collision_damage, int element) {
    sceVu0FVECTOR step;

    direction[3] = 1.0f;
    sceVu0Normalize(direction, direction);
    sceVu0ScaleVectorXYZ(step, direction, 2.0f);

    for (int particle = 0; particle < kStreamParticles; particle++) {
        const int slot = particle + start_index;
        sceVu0CopyVector(position[slot], origin);
        const auto distance = static_cast<float>(particle);
        position[slot][0] = origin[0] + step[0] * distance;
        position[slot][1] = origin[1] + step[1] * distance;
        position[slot][2] = origin[2] + step[2] * distance;
        velocity[slot][0] = direction[0] * 0.01f;
        velocity[slot][1] = direction[1] * 0.01f;
        velocity[slot][2] = direction[2] * 0.01f;
        size[slot] = 3.0f + distance * 0.3f;
        opacity[slot] = 180.0f - distance * 8.0f;
        state[slot] = 0;
    }

    init_damage = collision_damage;
    init_element = element;
    return -1;
}

int CSHOT_FIREBAR::Set(float *origin, float *direction, int collision_damage, int element) {
    sceVu0FVECTOR step;

    direction[3] = 1.0f;
    sceVu0Normalize(direction, direction);
    sceVu0ScaleVectorXYZ(step, direction, 2.0f);

    // Each particle travels from where it is to its place along the new stream, the farther
    // ones over more steps.
    for (int particle = 0; particle < kStreamParticles; particle++) {
        const int   slot = particle + start_index;
        const auto  distance = static_cast<float>(particle);
        const float steps = 1.0f + 0.5f * distance;
        velocity[slot][0] = (origin[0] + step[0] * distance - position[slot][0]) / steps;
        velocity[slot][1] = (origin[1] + step[1] * distance - position[slot][1]) / steps;
        velocity[slot][2] = (origin[2] + step[2] * distance - position[slot][2]) / steps;
        state[slot] = 0;
        size[slot] = 3.0f + 0.3f * distance + 3.0f * static_cast<float>(std::rand()) / 2147483648.0f;
        opacity[slot] = 180.0f - 8.0f * distance;
        damage[particle] = collision_damage;
        particle_element[particle] = element;
    }

    return -1;
}

void CSHOT_FIREBAR::Rset() {
    for (int particle = 0; particle < kStreamParticles; particle++) {
        state[particle] = kInactive;
    }
}

void CSHOT_FIREBAR::Step() {
    static int hit_timer = 0;

    hit_timer++;

    if (hit_timer >= kHitInterval) {
        hit_timer = 0;
    }

    for (int particle = 0; particle < kStreamParticles; particle++) {
        if (state[particle] != 0) {
            continue;
        }

        opacity[particle] -= 4.0f;
        size[particle] += 0.06f;

        if (hit_timer == 0) {
            NowColData->Set(position[particle], damage[particle], 2, 4.0f, 1.0f, 2, 2, 0, 0);
            NowColData->SetUserID(5, 6);
            NowColData->hit[NowColData->now_hit].weapon_flags = NowWeaponHave->flags;
            NowColData->hit[NowColData->now_hit].vs_monster = NowWeaponHave->vs_monster;
            NowColData->hit[NowColData->now_hit].flags = GetWeaponElementAttr(NowWeaponHave->best_elem);
        }

        position[particle][0] += velocity[particle][0];
        position[particle][1] += velocity[particle][1];
        position[particle][2] += velocity[particle][2];

        if (opacity[particle] <= 32.0f) {
            state[particle] = kInactive;
        }
    }
}

void CSHOT_FIREBAR::Draw() {
    bool texture_loaded = false;

    for (int particle = 0; particle < kStreamParticles; particle++) {
        if (state[particle] == kInactive) {
            continue;
        }

        if (!texture_loaded) {
            TexManager.ReloadTexture(Vif1Packet, 0x46);
            texture_loaded = true;
        }

        int column = 0;
        int row = 1;

        switch (particle_element[particle]) {
            case WEAPON_ELEMENT_COLD:
                column = 1;
                row = 1;
                break;
            case WEAPON_ELEMENT_THUNDER:
                column = 1;
                row = 0;
                break;
            case WEAPON_ELEMENT_WIND:
                column = 0;
                row = 0;
                break;
            case WEAPON_ELEMENT_HOLY:
                column = 2;
                row = 1;
                break;
            default:
                break;
        }

        set3DCellModel(position[particle], "c05w_h", size[particle], column << 7, row << 7, 0x80, 0x80, static_cast<u8>(static_cast<int>(opacity[particle])));
    }
}

} // namespace dc::game
