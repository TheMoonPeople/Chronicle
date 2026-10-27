#include "itembombeffect.hpp"

#include <algorithm>
#include <array>
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

namespace dc::game {

namespace {

/** Texture-sheet cell (column, row) of each blast puff; puffs past the third share the last cell. */
constexpr std::array<std::array<int, 2>, 4> kPuffCells = {
    {
     {0, 0},
     {0, 1},
     {1, 0},
     {1, 1},
     }
};

} // namespace

int checkItemUsed(int slot) {
    const int   character = UserStatus->cur_chara;
    const float water_now = UserStatus->water_now[character];
    const float water_max = UserStatus->water_max[character];
    const s16   hp = UserStatus->hp[character];
    const s16   max_hp = UserStatus->max_hp[character];
    const int   condition = UserStatus->ailments[character];
    const s16   item = UserStatus->item_pack.quick_item_slot[slot];

    if (item == -1) {
        return 0;
    }

    switch (item) {
        case ITEM_REGULAR_WATER:
        case ITEM_TASTY_WATER:
        case ITEM_PREMIUM_WATER:
            return 0.2f + water_now < water_max ? 1 : 0;
        case ITEM_ANTIDOTE_DRINK:
            return (condition & AILMENT_POISON) != 0 ? 1 : 0;
        case ITEM_SOAP:
            return (condition & AILMENT_GOO) != 0 ? 1 : 0;
        case ITEM_MIGHTY_HEALING:
            return (condition & (AILMENT_FREEZE | AILMENT_POISON | AILMENT_CURSE | AILMENT_GOO)) != 0 ? 1 : 0;
        case ITEM_BREAD:
        case ITEM_PREMIUM_CHICKEN:
        case ITEM_CHEESE:
        case ITEM_MELLOW_BANANA:
            return hp < max_hp ? 1 : 0;
        default:
            return 1;
    }
}

void usedActiveItem(CUserStatus *status, int item) {
    const int character = status->cur_chara;

    if (item == ITEM_MELLOW_BANANA) {
        status->AddNowLife(character, 200, 100.0f);
        status->AddDrink(status->cur_chara, -20, 5.0f);
        return;
    }

    const ITEM_DATA &data = ITEM_LIST[item - ITEM_ATTACH_START];

    if ((data.kind_flags & ITEMKINDF_CONSUMABLE) != 0) {
        const bool heals = (data.use_flags & ITEMUSE_HEAL_HP) != 0;

        if (heals) {
            status->AddNowLife(character, data.vol, 100.0f);
        }

        // An item that both heals and quenches keeps its drink amount in the second volume field.
        if ((data.use_flags & ITEMUSE_DRINK) != 0) {
            status->AddDrink(status->cur_chara, heals ? data.vol_range : data.vol, 5.0f);
        }
    }

    if ((data.kind_flags & 1) != 0) {
        if ((data.use_flags & 0x1000) != 0) {
            BtSetStatusErr(AILMENT_STAMINA);
            SndSePlay(SE_POWER_UP, -1, 0);
        }

        if ((data.use_flags & 0x20000) != 0 && (UserStatus->ailments[character] & AILMENT_GOO) != 0) {
            UserStatus->ailments[character] = 0;
        }

        if ((data.use_flags & 0x8000) != 0 && (UserStatus->ailments[character] & AILMENT_POISON) != 0) {
            UserStatus->ailments[character] = 0;
        }

        if ((data.use_flags & 0x3C000) != 0) {
            UserStatus->ailments[character] = 0;
        }
    }
}

int SetBombEffect(float *position, int owner, int damage, float scale) {
    for (int bomb = 0; bomb < 3; bomb++) {
        if (NowBombEffect[bomb].CheckBomb() != 0) {
            continue;
        }

        NowBombEffect[bomb].SetBomb(position, scale);
        SndSePlay(SE_EXPLOSION, -1, 0);
        const int collision_slot = NowColData->Set(position, damage, static_cast<int>(45.0f * scale), 20.0f * scale, 0.0f, owner, 3, 0, 0);

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
            wave->active = 1;
        }

        return collision_slot;
    }

    return -1;
}

void CItemBombEffect::Draw(CCamera *camera) {
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

        const std::array<int, 2> &cell = kPuffCells[std::min(i, 3)];
        const int                 u = cell[0] << 6;
        const int                 v = cell[1] << 6;
        direction[0] = camera_position[0] - positions[i][0];
        direction[1] = 0.0f;
        direction[2] = camera_position[2] - positions[i][2];
        sceVu0Normalize(direction, direction);
        direction[0] *= i * 5.0f + phases[i] * 2.0f;
        direction[2] *= i * 5.0f + phases[i] * 2.0f;
        world[0] = direction[0] + positions[i][0];
        world[1] = direction[1] + positions[i][1];
        world[2] = direction[2] + positions[i][2];
        world[3] = 1.0f;

        const float size = sizes[i] * scale;

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
        const CRect_i_ source(u, v, 0x40, 0x40);
        set3DSprite(Vif1Packet, TexManager.GetTexture("bomb_ex", -1), source, top_left, top_right, bottom_left, bottom_right, static_cast<u8>(alphas[i]));
    }

    MGSetGsALPHA(nullptr);
    MGSetGsZBUF(nullptr);
}

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
            default:
                break;
        }
    }
}

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

int CItemBombEffect::CheckBomb() {
    return std::ranges::any_of(active, [](s32 puff_active) { return puff_active != 0; }) ? 1 : 0;
}

void CItemBombEffect::Initialize() {
    std::ranges::fill(active, 0);
}

void CShockWave::Draw(CCamera * /*camera*/) {
    sceVu0FVECTOR corner0;
    sceVu0FVECTOR corner1;
    sceVu0FVECTOR corner2;
    sceVu0FVECTOR corner3;
    int           screen0[4];
    int           screen1[4];
    int           screen2[4];
    int           screen3[4];

    if (active == 0) {
        return;
    }

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
    const CRect_i_ source(0x80, 0, 0x40, 0x40);
    set3DSprite(Vif1Packet, TexManager.GetTexture("bomb_ex", -1), source, screen0, screen1, screen2, screen3, static_cast<u8>(alpha));
    MGSetGsALPHA(nullptr);
    MGSetGsZBUF(nullptr);
}

void CShockWave::Step() {
    if (active == 0) {
        return;
    }

    const float half_pi = HALF_PI;

    if (phase < half_pi) {
        phase += half_pi / expand_steps;
        const float envelope = std::sin(phase);
        alpha = 160.0f * envelope;
        radius = base_radius + expand_radius * envelope;
    }

    if (phase >= half_pi) {
        alpha -= 5.0f;

        if (alpha <= 0.0f) {
            alpha = 0.0f;
            active = 0;
        }
    }
}

} // namespace dc::game
