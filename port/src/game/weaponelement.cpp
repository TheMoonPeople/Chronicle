#include "weaponelement.hpp"

#include <libvu0.h>

#include <array>
#include <cstdlib>

#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "texture.hpp"

namespace dc::game {

namespace {

// rand() spans [0, 2^31), so dividing by this gives [0, 1).
constexpr float kRandRange = 2147483648.0f;

// The four screen corners of one sprite: top left, top right, bottom left, bottom right.
using Corners = std::array<std::array<int, 4>, 4>;

int RandomFrame() {
    return static_cast<int>(5.0f * static_cast<float>(rand()) / kRandRange) * 0x30;
}

s16 RandomSpawnDelay(s16 spawn_delay_max) {
    return static_cast<s16>(static_cast<int>(static_cast<float>(spawn_delay_max) * static_cast<float>(rand()) / kRandRange) + 1);
}

// The sprite call gives back two opposite corners, so the other two take one part from each.
void FillSideCorners(Corners &corner) {
    corner[1][0] = corner[3][0];
    corner[1][1] = corner[0][1];
    corner[1][2] = corner[0][2];
    corner[2][0] = corner[0][0];
    corner[2][1] = corner[3][1];
    corner[2][2] = corner[3][2];
}

// Shared by the holy and fire elements.
void StartRisingSpark(CWeaponElement &fx, int i) {
    const float spread = fx.spread;

    fx.size[i] = 2.0f + 6.0f * static_cast<float>(rand()) / kRandRange;
    fx.shrink[i] = 1.0f;
    fx.alpha[i] = 1.0f + 48.0f * static_cast<float>(rand()) / kRandRange;
    fx.fading[i] = 0;
    fx.offset[i][0] = 2.0f * (spread * static_cast<float>(rand())) / kRandRange - spread;
    fx.offset[i][1] = spread * static_cast<float>(rand()) / kRandRange - spread / 2.0f;
    fx.offset[i][2] = 2.0f * (spread * static_cast<float>(rand())) / kRandRange - spread;
    fx.offset[i][3] = 1.0f;
    fx.frame[i] = static_cast<s16>(RandomFrame());
}

void StartColdSpark(CWeaponElement &fx, int i) {
    const float spread = fx.spread;

    fx.size[i] = 3.0f + 6.0f * static_cast<float>(rand()) / kRandRange;
    fx.shrink[i] = 1.0f;
    fx.alpha[i] = 1.0f + 48.0f * static_cast<float>(rand()) / kRandRange;
    fx.fading[i] = 0;
    fx.offset[i][0] = 2.0f * (spread * static_cast<float>(rand())) / kRandRange - spread;
    fx.offset[i][1] = spread / 2.0f + spread * static_cast<float>(rand()) / kRandRange;
    fx.offset[i][2] = 2.0f * (spread * static_cast<float>(rand())) / kRandRange - spread;
    fx.offset[i][3] = 1.0f;
    fx.frame[i] = static_cast<s16>(RandomFrame());
}

void StartWindSpark(CWeaponElement &fx, int i) {
    const float spread = fx.spread;

    fx.size[i] = 2.0f + 4.0f * static_cast<float>(rand()) / kRandRange;
    fx.shrink[i] = 1.0f;
    fx.alpha[i] = 1.0f + 48.0f * static_cast<float>(rand()) / kRandRange;
    fx.fading[i] = 0;
    fx.offset[i][0] = 2.0f * (spread * static_cast<float>(rand())) / kRandRange - spread;
    fx.offset[i][1] = 2.0f * (spread * static_cast<float>(rand())) / kRandRange - spread;
    fx.offset[i][2] = 2.0f * (spread * static_cast<float>(rand())) / kRandRange - spread;
    fx.offset[i][3] = 1.0f;

    // Each spark blows along the line it starts on, at its own speed.
    sceVu0CopyVector(fx.velocity[i], fx.offset[i]);
    sceVu0Normalize(fx.velocity[i], fx.velocity[i]);
    sceVu0ScaleVector(fx.velocity[i], fx.velocity[i], 0.3f * static_cast<float>(rand()) / kRandRange);
    fx.spin[i] = 2.0f * (PI_SHORT * static_cast<float>(rand())) / kRandRange - PI_SHORT;
    fx.spin_speed[i] = 0.09817475f + SIXTEENTH_PI_SHORT * static_cast<float>(rand()) / kRandRange;
    fx.frame[i] = static_cast<s16>(RandomFrame());
}

// Draws every live spark as an upright sprite around base, from texture column u.
void DrawSparks(const CWeaponElement &fx, CTexture *texture, const sceVu0FVECTOR base, int u) {
    Corners       corner;
    sceVu0FVECTOR pos;

    pos[3] = 1.0f;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (fx.alpha[i] <= 0.0f) {
            continue;
        }

        const float width = fx.scale * (fx.size[i] * fx.shrink[i]);
        pos[0] = base[0] + fx.offset[i][0];
        pos[1] = base[1] + fx.offset[i][1];
        pos[2] = base[2] + fx.offset[i][2];

        if (MGRotTransPers3DSprite(corner[0].data(), corner[3].data(), pos, width, width / 2.0f, 0) != 1) {
            continue;
        }

        FillSideCorners(corner);
        set3DSprite(Vif1Packet, texture, CRect_i_(u, fx.frame[i], 0x30, 0x30), corner[0].data(), corner[1].data(),
                    corner[2].data(), corner[3].data(), static_cast<u8>(fx.alpha[i]));
    }
}

} // namespace

void CWeaponElement::Initialize() {
    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        size[i] = 1.0f;
        alpha[i] = 1.0f;
    }

    on = 0;
}

void CWeaponElement::Set(sceVu0FVECTOR *target, float *position, float strength, int element, float radius) {
    power = 0.01f * (1.0f + strength);
    kind = static_cast<s16>(element);
    spread = radius;
    origin = target;

    switch (element) {
        default:
        case WEAPON_ELEMENT_COLD:
            Init_Cold(position);
            break;
        case WEAPON_ELEMENT_WIND:
            Init_Wind(position);
            break;
        case WEAPON_ELEMENT_FIRE:
            Init_Fire(position);
            break;
        case WEAPON_ELEMENT_THUNDER:
            Init_Thunder(position);
            break;
        case WEAPON_ELEMENT_HOLY:
            Init_Holy(position);
            break;
    }

    on = 1;
}

void CWeaponElement::Step() {
    if (on == 0) {
        return;
    }

    switch (kind) {
        default:
        case WEAPON_ELEMENT_COLD:
            Step_Cold();
            break;
        case WEAPON_ELEMENT_WIND:
            Step_Wind();
            break;
        case WEAPON_ELEMENT_FIRE:
            Step_Fire();
            break;
        case WEAPON_ELEMENT_THUNDER:
            Step_Thunder();
            break;
        case WEAPON_ELEMENT_HOLY:
            Step_Holy();
            break;
    }
}

void CWeaponElement::Draw() {
    if (on == 0) {
        return;
    }

    switch (kind) {
        default:
        case WEAPON_ELEMENT_COLD:
            Draw_Cold();
            break;
        case WEAPON_ELEMENT_WIND:
            Draw_Wind();
            break;
        case WEAPON_ELEMENT_FIRE:
            Draw_Fire();
            break;
        case WEAPON_ELEMENT_THUNDER:
            Draw_Thunder();
            break;
        case WEAPON_ELEMENT_HOLY:
            Draw_Holy();
            break;
    }
}

void CWeaponElement::Init_Holy(float * /*position*/) {
    count = static_cast<s16>(static_cast<int>(12.0f * power) + 2);
    spawn_budget = static_cast<s16>(static_cast<int>(20.0f * power) + 10);
    spawn_delay_max = static_cast<s16>(6 - static_cast<int>(3.0f * power));
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * static_cast<float>(0.8 + 0.4f * power);
    scale = 0.5f + 0.7f * power;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        StartRisingSpark(*this, i);
    }
}

void CWeaponElement::Step_Holy() {
    int dead = 0;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
            continue;
        }

        offset[i][1] += 0.5f;
        shrink[i] -= 0.01f;

        if (shrink[i] <= 0.0f) {
            shrink[i] = 0.0f;
            alpha[i] = 0.0f;
        }

        if (frame_timer == 4) {
            frame[i] = static_cast<s16>(RandomFrame());
        }

        if (fading[i] != 0) {
            alpha[i] -= 8.0f;

            if (alpha[i] <= 0.0f) {
                alpha[i] = 0.0f;
            }
        } else {
            alpha[i] += 16.0f;

            if (!(alpha[i] < 128.0f)) {
                fading[i] = 1;
            }
        }
    }

    frame_timer--;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    // While the budget lasts, a free slot is filled again every so often.
    if (spawn_budget > 0) {
        spawn_budget--;
        spawn_delay--;

        if (spawn_delay <= 0) {
            for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    StartRisingSpark(*this, i);
                    spawn_delay = RandomSpawnDelay(spawn_delay_max);
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Holy() {
    CTexture *texture = TexManager.GetTexture("element", -1);

    sceVu0FVECTOR base;
    sceVu0CopyVector(base, *origin);
    DrawSparks(*this, texture, base, 0xD0);
}

void CWeaponElement::Init_Cold(float * /*position*/) {
    count = static_cast<s16>(static_cast<int>(12.0f * power) + 2);
    spawn_budget = static_cast<s16>(static_cast<int>(20.0f * power) + 10);
    spawn_delay_max = static_cast<s16>(6 - static_cast<int>(3.0f * power));
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * static_cast<float>(0.8 + 0.4f * power);
    scale = 0.5f + 0.7f * power;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        StartColdSpark(*this, i);
    }
}

void CWeaponElement::Step_Cold() {
    int dead = 0;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
            continue;
        }

        offset[i][1] -= 0.1f + 0.3f * static_cast<float>(rand()) / kRandRange;
        shrink[i] -= 0.01f;

        if (shrink[i] <= 0.0f) {
            shrink[i] = 0.0f;
            alpha[i] = 0.0f;
        }

        if (frame_timer == 4) {
            frame[i] = static_cast<s16>(RandomFrame());
        }

        if (fading[i] != 0) {
            alpha[i] -= 6.0f;

            if (alpha[i] <= 0.0f) {
                alpha[i] = 0.0f;
            }
        } else {
            alpha[i] += 12.0f;

            if (!(alpha[i] < 128.0f)) {
                fading[i] = 1;
            }
        }
    }

    frame_timer--;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    if (spawn_budget > 0) {
        spawn_budget--;
        spawn_delay--;

        if (spawn_delay <= 0) {
            for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    StartColdSpark(*this, i);
                    spawn_delay = RandomSpawnDelay(spawn_delay_max);
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Cold() {
    CTexture *texture = TexManager.GetTexture("element", -1);

    sceVu0FVECTOR base;
    sceVu0CopyVector(base, *origin);
    DrawSparks(*this, texture, base, 0xA0);
}

void CWeaponElement::Init_Wind(float * /*position*/) {
    count = static_cast<s16>(static_cast<int>(10.0f * power) + 1);
    spawn_budget = static_cast<s16>(static_cast<int>(20.0f * power) + 10);
    spawn_delay_max = static_cast<s16>(8 - static_cast<int>(4.0f * power));
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * static_cast<float>(0.8 + 0.4f * power);
    scale = 0.5f + 1.3f * (0.7f * power);

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        StartWindSpark(*this, i);
    }
}

void CWeaponElement::Step_Wind() {
    int dead = 0;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
            continue;
        }

        if (fading[i] != 0) {
            alpha[i] -= 4.0f;

            if (alpha[i] <= 0.0f) {
                alpha[i] = 0.0f;
            }
        } else {
            alpha[i] += 32.0f;

            if (!(alpha[i] < 128.0f)) {
                fading[i] = 1;
            }
        }

        if (frame_timer == 4) {
            frame[i] = static_cast<s16>(RandomFrame());
        }

        spin[i] += spin_speed[i];

        if (!(spin[i] <= PI_SHORT)) {
            spin[i] -= TWO_PI_SHORT;
        }

        offset[i][0] += velocity[i][0];
        offset[i][1] += 0.2f;
        offset[i][2] += velocity[i][2];
    }

    frame_timer--;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    if (spawn_budget > 0) {
        spawn_budget--;
        spawn_delay--;

        if (spawn_delay <= 0) {
            for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    StartWindSpark(*this, i);
                    spawn_delay = RandomSpawnDelay(spawn_delay_max);
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Wind() {
    Corners       corner;
    sceVu0FVECTOR base;
    sceVu0FVECTOR pos;
    sceVu0FMATRIX unit;
    sceVu0FMATRIX turn;

    CTexture *texture = TexManager.GetTexture("element", -1);
    sceVu0CopyVector(base, *origin);
    pos[3] = 1.0f;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            continue;
        }

        const float width = scale * (size[i] * shrink[i]);

        sceVu0UnitMatrix(unit);
        sceVu0RotMatrixY(turn, unit, spin[i]);
        sceVu0ApplyMatrix(pos, turn, offset[i]);
        pos[0] += base[0];
        pos[1] += base[1];
        pos[2] += base[2];

        if (MGRotTransPers3DSprite(corner[0].data(), corner[3].data(), pos, width, width / 2.0f, 0) != 1) {
            continue;
        }

        FillSideCorners(corner);
        set3DSprite(Vif1Packet, texture, CRect_i_(0x60, frame[i], 0x40, 0x30), corner[0].data(), corner[1].data(),
                    corner[2].data(), corner[3].data(), static_cast<u8>(alpha[i]));
    }
}

void CWeaponElement::Init_Fire(float *position) {
    count = static_cast<s16>(static_cast<int>(12.0f * power) + 2);
    spawn_budget = static_cast<s16>(static_cast<int>(20.0f * power) + 10);
    spawn_delay_max = static_cast<s16>(6 - static_cast<int>(3.0f * power));
    spawn_delay = 0;
    frame_timer = 4;
    spread = spread * static_cast<float>(0.8 + 0.4f * power);
    scale = 0.5f + 0.7f * power;

    // Fire stays where it was lit rather than following the weapon.
    sceVu0CopyVector(fire_pos, position);

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        shrink[i] = 0.0f;
        alpha[i] = 0.0f;
    }

    for (int i = 0; i < count; i++) {
        StartRisingSpark(*this, i);
    }
}

void CWeaponElement::Step_Fire() {
    int dead = 0;

    for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
            continue;
        }

        offset[i][1] += 0.01f + 0.6f * static_cast<float>(rand()) / kRandRange;
        shrink[i] -= 0.01f;

        if (shrink[i] <= 0.0f) {
            shrink[i] = 0.0f;
            alpha[i] = 0.0f;
        }

        if (frame_timer == 4) {
            frame[i] = static_cast<s16>(RandomFrame());
        }

        if (fading[i] != 0) {
            alpha[i] -= 8.0f;

            if (alpha[i] <= 0.0f) {
                alpha[i] = 0.0f;
            }
        } else {
            alpha[i] += 16.0f;

            if (!(alpha[i] < 128.0f)) {
                fading[i] = 1;
            }
        }
    }

    frame_timer--;

    if (frame_timer == 0) {
        frame_timer = 4;
    }

    if (spawn_budget > 0) {
        spawn_budget--;
        spawn_delay--;

        if (spawn_delay <= 0) {
            for (int i = 0; i < WEAPON_ELEMENT_SPARK_MAX; i++) {
                if (alpha[i] == 0.0f) {
                    StartRisingSpark(*this, i);
                    spawn_delay = RandomSpawnDelay(spawn_delay_max);
                    break;
                }
            }
        }
    }

    if (dead >= WEAPON_ELEMENT_SPARK_MAX) {
        on = 0;
    }
}

void CWeaponElement::Draw_Fire() {
    CTexture *texture = TexManager.GetTexture("element", -1);

    sceVu0FVECTOR base;
    sceVu0CopyVector(base, fire_pos);
    DrawSparks(*this, texture, base, 0x30);
}

void CWeaponElement::Init_Thunder(float *position) {
    sceVu0FVECTOR reach;
    sceVu0FVECTOR dir;

    count = static_cast<s16>(static_cast<int>(18.0f * power) + 6);
    bolt_count = static_cast<s16>(static_cast<int>(7.0f * power) + 1);
    spread = spread * static_cast<float>(0.8 + 0.4f * power);

    for (int i = 0; i < count; i++) {
        velocity[i][0] = 8.0f * static_cast<float>(rand()) / kRandRange - 4.0f;
        velocity[i][1] = 4.0f * static_cast<float>(rand()) / kRandRange - 2.0f;
        velocity[i][2] = 8.0f * static_cast<float>(rand()) / kRandRange - 4.0f;

        // Each spark is thrown somewhere along the line it will travel.
        sceVu0Normalize(dir, velocity[i]);
        sceVu0ScaleVectorXYZ(reach, dir, spread);
        offset[i][0] = position[0] + velocity[i][0] + reach[0] * static_cast<float>(rand()) / kRandRange;
        offset[i][1] = position[1] + velocity[i][1] + reach[1] * static_cast<float>(rand()) / kRandRange;
        offset[i][2] = position[2] + velocity[i][2] + reach[2] * static_cast<float>(rand()) / kRandRange;
        offset[i][3] = 1.0f;
        sceVu0ScaleVectorXYZ(velocity[i], dir, 0.3f * static_cast<float>(rand()) / kRandRange);
        size[i] = 0.5f + 2.5f * static_cast<float>(rand()) / kRandRange;
        shrink[i] = 1.0f;
        alpha[i] = 96.0f + static_cast<float>(static_cast<int>(64.0f * static_cast<float>(rand()) / kRandRange));
    }

    for (int i = 0; i < bolt_count; i++) {
        bolt_head[i] = static_cast<s16>(static_cast<float>(count) * static_cast<float>(rand()) / kRandRange);
        bolt_tail[i] = static_cast<s16>(static_cast<float>(count) * static_cast<float>(rand()) / kRandRange);
        bolt_timer[i] = static_cast<s16>(static_cast<int>(6.0f * static_cast<float>(rand()) / kRandRange) * 3 + 3);
        bolt_frame[i] = static_cast<s16>(4.0f * static_cast<float>(rand()) / kRandRange);
    }
}

void CWeaponElement::Step_Thunder() {
    int dead = 0;

    for (int i = 0; i < count; i++) {
        if (alpha[i] <= 0.0f) {
            dead++;
            continue;
        }

        offset[i][0] += velocity[i][0];
        offset[i][1] += velocity[i][1];
        offset[i][2] += velocity[i][2];
        shrink[i] -= 0.01f;
        alpha[i] -= 4.0f;

        if (alpha[i] <= 3.0f) {
            alpha[i] = 0.0f;
        }
    }

    if (dead >= count) {
        on = 0;
        return;
    }

    for (int i = 0; i < bolt_count; i++) {
        bolt_timer[i]--;

        if (bolt_timer[i] <= 0) {
            bolt_head[i] = static_cast<s16>(static_cast<float>(count) * static_cast<float>(rand()) / kRandRange);
            bolt_tail[i] = static_cast<s16>(static_cast<float>(count) * static_cast<float>(rand()) / kRandRange);
            bolt_timer[i] = static_cast<s16>(static_cast<int>(6.0f * static_cast<float>(rand()) / kRandRange) * 3 + 3);
            bolt_frame[i] = static_cast<s16>(4.0f * static_cast<float>(rand()) / kRandRange);
        } else if (bolt_timer[i] % 3 == 0) {
            bolt_frame[i]++;

            if (bolt_frame[i] >= 4) {
                bolt_frame[i] = 0;
            }
        }
    }
}

void CWeaponElement::Draw_Thunder() {
    // A bolt reuses the four corners as the ends of its arc.
    Corners       corner;
    sceVu0FVECTOR base;

    CTexture *texture = TexManager.GetTexture("element", -1);
    sceVu0CopyVector(base, *origin);

    for (int i = 0; i < count; i++) {
        if (alpha[i] <= 0.0f) {
            continue;
        }

        const float width = size[i] * shrink[i];

        if (MGRotTransPers3DSprite(corner[0].data(), corner[3].data(), offset[i], width, width / 2.0f, 0) != 1) {
            continue;
        }

        FillSideCorners(corner);
        set3DSprite(Vif1Packet, texture, CRect_i_(0, 0, 0x30, 0x30), corner[0].data(), corner[1].data(),
                    corner[2].data(), corner[3].data(), static_cast<u8>(alpha[i]));
    }

    // Texture position of each of the four bolt shapes.
    constexpr std::array<std::array<int, 2>, 4> bolt_uv = {
        {{0x00, 0x30}, {0x18, 0x30}, {0x00, 0x98}, {0x00, 0x98}}
    };
    sceVu0FVECTOR pos;

    for (int i = 0; i < bolt_count; i++) {
        // A bolt is one tall sprite stood between the two sparks it arcs
        // across, so each end gives a point above and a point below it.
        sceVu0CopyVector(pos, offset[bolt_head[i]]);
        pos[1] += 1.0f;
        MGRotTransPers(corner[0].data(), pos, 0);
        pos[1] -= 2.0f;
        MGRotTransPers(corner[1].data(), pos, 0);
        sceVu0CopyVector(pos, offset[bolt_tail[i]]);
        pos[1] += 1.0f;
        MGRotTransPers(corner[2].data(), pos, 0);
        pos[1] -= 2.0f;
        MGRotTransPers(corner[3].data(), pos, 0);

        const auto &uv = bolt_uv[bolt_frame[i]];
        set3DSprite(Vif1Packet, texture, CRect_i_(uv[0], uv[1], 0x18, 0x68), corner[0].data(), corner[1].data(),
                    corner[2].data(), corner[3].data(), static_cast<u8>(1.6f * alpha[bolt_head[i]]));
    }
}

} // namespace dc::game
