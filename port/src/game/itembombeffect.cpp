#include "itembombeffect.hpp"

#include <cmath>

#include "camera.hpp"
#include "collisiondata.hpp"
#include "dun/gameloop.hpp"
#include "itemdata.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

/**
 * Reports whether one running item is still in use.
 *
 * @mangled checkItemUsed__Fi
 * @address 0x1D5580
 * @size 0x16C
 */
int checkItemUsed(int slot) {
    int   character;
    float water_now;
    float water_max;
    int   condition;
    s16   hp;
    s16   max_hp;

    character = UserStatus->cur_chara;
    water_now = UserStatus->water_now[character];
    water_max = UserStatus->water_max[character];
    hp = UserStatus->hp[character];
    max_hp = UserStatus->max_hp[character];
    condition = UserStatus->ailments[character];
    int        usable = 1;
    ITEM_PACK *pack = &UserStatus->item_pack;
    s16        item = pack->quick_item_slot[slot];

    if (item == -1) {
        return 0;
    }

    switch (item) {
        case ITEM_REGULAR_WATER:
        case ITEM_TASTY_WATER:
        case ITEM_PREMIUM_WATER:
            if (!(0.2f + water_now < water_max)) {
                usable = 0;
            }

            break;
        case ITEM_ANTIDOTE_DRINK:
            if ((condition & AILMENT_POISON) == 0) {
                usable = 0;
            }

            break;
        case ITEM_SOAP:
            if ((condition & AILMENT_GOO) == 0) {
                usable = 0;
            }

            break;
        case ITEM_MIGHTY_HEALING:
            if ((condition & (AILMENT_FREEZE | AILMENT_POISON | AILMENT_CURSE | AILMENT_GOO)) == 0) {
                usable = 0;
            }

            break;
        case ITEM_BREAD:
        case ITEM_PREMIUM_CHICKEN:
        case ITEM_CHEESE:
        case ITEM_MELLOW_BANANA:
            if (hp >= max_hp) {
                usable = 0;
            }

            break;
    }

    return usable;
}

/**
 * Spends one use of a running item.
 *
 * @mangled usedActiveItem__FP11CUserStatusi
 * @address 0x1D56F0
 * @size 0x248
 */
void usedActiveItem(CUserStatus *status, int item) {
    int vol_index;
    int character = status->cur_chara;

    if (item == ITEM_MELLOW_BANANA) {
        status->AddNowLife(character, 200, 100.0f);
        status->AddDrink(status->cur_chara, -20, 5.0f);
        return;
    }

    item -= ITEM_ATTACH_START;

    if ((ITEM_LIST[item].kind_flags & ITEMKINDF_CONSUMABLE) != 0) {
        vol_index = 0;

        if ((ITEM_LIST[item].use_flags & ITEMUSE_HEAL_HP) != 0) {
            s16 life = ITEM_LIST[item].vol;
            status->AddNowLife(character, life, 100.0f);
            vol_index++;
        }

        if ((ITEM_LIST[item].use_flags & ITEMUSE_DRINK) != 0) {
            s16 drink = (&ITEM_LIST[item].vol)[vol_index];
            status->AddDrink(status->cur_chara, drink, 5.0f);
        }
    }

    if ((ITEM_LIST[item].kind_flags & 1) != 0) {
        if ((ITEM_LIST[item].use_flags & 0x1000) != 0) {
            BtSetStatusErr(AILMENT_STAMINA);
            SndSePlay(SE_POWER_UP, -1, 0);
        }

        if ((ITEM_LIST[item].use_flags & 0x20000) != 0 && (UserStatus->ailments[character] & AILMENT_GOO) != 0) {
            UserStatus->ailments[character] = 0;
        }

        if ((ITEM_LIST[item].use_flags & 0x8000) != 0 && (UserStatus->ailments[character] & AILMENT_POISON) != 0) {
            UserStatus->ailments[character] = 0;
        }

        if ((ITEM_LIST[item].use_flags & 0x3C000) != 0 && (UserStatus->ailments[character] != 0 || UserStatus->ailments[character] != AILMENT_STAMINA)) {
            UserStatus->ailments[character] = 0;
        }
    }
}

/**
 * Starts a bomb effect at a position and gives back the slot it took.
 *
 * @mangled SetBombEffect__FPfiif
 * @address 0x1D5940
 * @size 0x1F0
 */
int SetBombEffect(float *position, int owner, int damage, float scale) {
    int collision_slot = -1;

    for (int bomb = 0; bomb < 3; bomb++) {
        if (NowBombEffect[bomb].CheckBomb() != 0) {
            continue;
        }

        NowBombEffect[bomb].SetBomb(position, scale);
        SndSePlay(SE_EXPLOSION, -1, 0);
        collision_slot = NowColData->Set(position, damage, (int) (45.0f * scale), 20.0f * scale, 0.0f, owner, 3, 0, 0);

        if (collision_slot != -1) {
            CCollisionData *collision = NowColData;
            collision->hit[collision->now_hit].phase = 10;
            collision->hit[collision->now_hit].ready_phase = 10;
        }

        if (scale > 1.0f) {
            CShockWave *wave = NowShockWave;
            sceVu0CopyVector(wave->position, position);
            wave->position[3] = 1.0f;
            wave->base_radius = wave->expand_radius = 30.0f * scale;
            wave->radius = 0.0f;
            wave->expand_steps = 15.0f * scale;
            wave->phase = 0.0f;
            wave->alpha = 0.0f;
            wave->active = true;
        }

        break;
    }

    return collision_slot;
}

/**
 * Draws the bomb's blast and its shock wave.
 *
 * @mangled Draw__15CItemBombEffectFP7CCamera
 * @address 0x1D5B30
 * @size 0x374
 */
void CItemBombEffect::Draw(CCamera *camera) {
    static int uvTable[4][2] = {
        {0, 0},
        {0, 1},
        {1, 0},
        {1, 1}
    };
    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR direction;
    sceVu0FVECTOR world;
    int           top_left[4];
    int           bottom_right[4];
    int           top_right[4];
    int           bottom_left[4];

    camera->GetPos(camera_position);
    sceGsAlpha alpha = mgAlpha;
    alpha.bits.a = 0;
    alpha.bits.b = 2;
    alpha.bits.c = 0;
    alpha.bits.d = 1;
    MGSetGsALPHA(&alpha);
    sceGsZbuf zbuffer = mgZBuffer;
    zbuffer.bits.zmsk = 1;
    MGSetGsZBUF(&zbuffer);

    for (int i = 0; i < 5; i++) {
        if (active[i] != 1 || counters[i] < 0) {
            continue;
        }

        int cell = i;

        if (cell >= 3) {
            cell = 3;
        }

        int u = uvTable[cell][0] << 6;
        int v = uvTable[cell][1] << 6;
        direction[0] = camera_position[0] - positions[i][0];
        float *up = &direction[1];
        *up = 0.0f;
        direction[2] = camera_position[2] - positions[i][2];
        sceVu0Normalize(direction, direction);
        direction[0] *= i * 5.0f + phases[i] * 2.0f;
        direction[2] *= i * 5.0f + phases[i] * 2.0f;
        world[0] = direction[0] + positions[i][0];
        world[1] = *up + positions[i][1];
        world[2] = direction[2] + positions[i][2];
        world[3] = 1.0f;

        float size = sizes[i] * scale;

        if (MGRotTransPers3DSprite(top_left, bottom_right, world, size, size / 2.0f, 0) != 1) {
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
        CRect_i_ source(u, v, 0x40, 0x40);
        set3DSprite(Vif1Packet, TexManager.GetTexture("bomb_ex", -1), source, top_left, top_right, bottom_left, bottom_right, (u8) alphas[i]);
    }

    MGSetGsALPHA(NULL);
    MGSetGsZBUF(NULL);
}

/**
 * Advances the bomb effect by a frame.
 *
 * @mangled Step__15CItemBombEffectFv
 * @address 0x1D5EB0
 * @size 0x1F0
 */
void CItemBombEffect::Step() {
    for (int puff = 0; puff < 5; puff++) {
        if (active[puff] != 1) {
            continue;
        }

        switch (phases[puff]) {
            case 0:
                counters[puff]++;
                sizes[puff] += 2.0f;
                alphas[puff] += 8.0f;

                if (counters[puff] >= 3) {
                    counters[puff] = 0;
                    phases[puff]++;
                }

                break;
            case 1:
                counters[puff]++;
                sizes[puff] += 1.0f;
                alphas[puff] += 8.0f;

                if (counters[puff] >= 4) {
                    counters[puff] = 0;
                    phases[puff]++;
                }

                break;
            case 2:
                counters[puff]++;
                sizes[puff] += 0.3f;
                alphas[puff] -= 3.0f;

                if (counters[puff] >= 20) {
                    counters[puff] = 0;
                    phases[puff]++;
                }

                break;
            case 3:
                counters[puff]++;
                sizes[puff] += 0.1f;
                alphas[puff] -= 2.0f;

                if (counters[puff] >= 40) {
                    active[puff] = 0;
                }

                break;
        }
    }
}

/**
 * Places the bomb's five blast puffs around a position.
 *
 * @mangled SetBomb__15CItemBombEffectFPff
 * @address 0x1D60A0
 * @size 0xBC
 */
void CItemBombEffect::SetBomb(float *position, float scale) {
    for (int puff = 0; puff < 5; puff++) {
        sceVu0CopyVector(positions[puff], position);
        phases[puff] = 0;
        counters[puff] = -(puff * 3);
        alphas[puff] = 128.0f;
        sizes[puff] = 20.0f;
        active[puff] = 1;
    }

    this->scale = scale;
    phases[0] = 2;
    phases[1] = 1;
}

/**
 * Reports whether the bomb effect is still running.
 *
 * @mangled CheckBomb__15CItemBombEffectFv
 * @address 0x1D6160
 * @size 0x48
 */
int CItemBombEffect::CheckBomb() {
    for (int puff = 0; puff < 5; puff++) {
        if (active[puff] != 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * Clears the bomb effect.
 *
 * @mangled Initialize__15CItemBombEffectFv
 * @address 0x1D61B0
 * @size 0x30
 */
void CItemBombEffect::Initialize() {
    for (int puff = 0; puff < 5; puff++) {
        active[puff] = 0;
    }
}

/**
 * Draws the expanding shock-wave ring.
 *
 * @mangled Draw__10CShockWaveFP7CCamera
 * @address 0x1D61E0
 * @size 0x2F8
 */
void CShockWave::Draw(CCamera *camera) {
    sceVu0FVECTOR corner0;
    sceVu0FVECTOR corner1;
    sceVu0FVECTOR corner2;
    sceVu0FVECTOR corner3;
    int           screen0[4];
    int           screen1[4];
    int           screen2[4];
    int           screen3[4];
    sceVu0FVECTOR camera_position;
    sceVu0FVECTOR direction;

    if (active == 0) {
        return;
    }

    camera->GetPos(camera_position);
    direction[0] = camera_position[0] - position[0];
    float *up = &direction[1];
    *up = 0.0f;
    direction[2] = camera_position[2] - position[2];
    sceVu0Normalize(direction, direction);
    direction[0] *= 10.0f;
    direction[2] *= 10.0f;

    sceVu0CopyVector(corner0, position);
    corner0[0] -= radius;
    corner0[2] -= radius;
    sceVu0CopyVector(corner1, position);
    corner1[0] += radius;
    corner1[2] -= radius;
    sceVu0CopyVector(corner2, position);
    corner2[0] -= radius;
    corner2[2] += radius;
    sceVu0CopyVector(corner3, position);
    corner3[0] += radius;
    corner3[2] += radius;

    if (MGRotTransPers(screen0, corner0, 0) == 0) {
        return;
    }

    if (MGRotTransPers(screen1, corner1, 0) == 0) {
        return;
    }

    if (MGRotTransPers(screen2, corner2, 0) == 0) {
        return;
    }

    if (MGRotTransPers(screen3, corner3, 0) == 0) {
        return;
    }

    sceGsAlpha blend = mgAlpha;
    blend.bits.a = 0;
    blend.bits.b = 2;
    blend.bits.c = 0;
    blend.bits.d = 1;
    MGSetGsALPHA(&blend);
    sceGsZbuf zbuffer = mgZBuffer;
    zbuffer.bits.zmsk = 1;
    MGSetGsZBUF(&zbuffer);
    CRect_i_ source(0x80, 0, 0x40, 0x40);
    set3DSprite(Vif1Packet, TexManager.GetTexture("bomb_ex", -1), source, screen0, screen1, screen2, screen3, (u8) alpha);
    MGSetGsALPHA(NULL);
    MGSetGsZBUF(NULL);
}

/**
 * Expands and fades the shock-wave ring by a frame.
 *
 * @mangled Step__10CShockWaveFv
 * @address 0x1D64E0
 * @size 0xD8
 */
void CShockWave::Step() {
    if (active == 0) {
        return;
    }

    const float half_pi = HALF_PI;

    if (phase < half_pi) {
        phase += half_pi / expand_steps;
        float envelope = sinf(phase);
        alpha = 160.0f * envelope;
        radius = base_radius + expand_radius * envelope;
    }

    if (phase >= half_pi) {
        alpha -= 5.0f;

        if (alpha <= 0.0f) {
            alpha = 0.0f;
            active = false;
        }
    }
}
