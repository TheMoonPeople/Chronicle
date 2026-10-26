#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 1383

#include "randomitem.hpp"

#include <libvu0.h>

#include <cmath>

#include "character.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "gameutil.hpp"
#include "hitvalue.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "texture.hpp"

void CRandomItem::Draw() {
    for (int i = 0; i < 32; i++) {
        if (id[i] == -1 || distance[i] > 200.0f) {
            continue;
        }

        sceVu0FVECTOR draw_position;
        int           texel_x;
        int           texel_y;
        sceVu0CopyVector(draw_position, position[i]);
        draw_position[1] += 8.0f * sinf(phase[i]);

        if (amount[i] != -1) {
            texel_x = 0x40;
            texel_y = 0;
        } else {
            switch (item_no[i]) {
                case ITEM_DRAN_S_CREST:
                case ITEM_HOOK:
                case ITEM_KING_S_SLATE:
                case ITEM_GUN_POWDER:
                case ITEM_CLOCK_HANDS:
                case ITEM_POINTY_CHESTNUT:
                case ITEM_BLACK_KNIGHT_CREST:
                    texel_x = 0;
                    texel_y = 0;
                    break;
                case ITEM_SHINY_STONE:
                    texel_x = 0x40;
                    texel_y = 0x40;
                    break;
                case ITEM_RED_BERRY:
                    texel_x = 0;
                    texel_y = 0x40;
                    break;
                default:
                    texel_x = 0x40;
                    texel_y = 0xC0;
                    break;
            }

            if (item_no[i] >= ITEM_ATTACH_START && item_no[i] < ITEM_ATTACH_ELEMENT_END) {
                texel_x = 0;
                texel_y = 0x80;
            }

            if (item_no[i] >= ITEM_ATTACH_SLAYER_START && item_no[i] < ITEM_ATTACH_SLAYER_END) {
                texel_x = 0x40;
                texel_y = 0x80;
            }
        }

        BtSet3DCellModel(draw_position, gold_texture, 3.5f, texel_x, texel_y, 0x40, 0x40, 0x80);
    }
}

void CRandomItem::MapSymbolDraw() {
    CTexture *texture = TexManager.GetTexture("itempack", -1);

    for (int i = 0; i < 32; i++) {
        if (id[i] != -1 && amount[i] == -1) {
            int x = (int) (0.1f * (position[i][0] - 80.0f));
            int y = (int) (0.1f * (position[i][2] - 80.0f));
            set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x184, y + 0x48, 8, 8), CRect_i_(0x50, 0x68, 8, 8));
        }
    }
}

int CRandomItem::checkEvent() {
    for (int i = 0; i < 32; i++) {
        int event = pickup_event[i];

        if (event != -1) {
            pickup_event[i] = -1;
            return event;
        }
    }

    return -1;
}

int CRandomItem::checkErr() {
    for (int i = 0; i < 32; i++) {
        if (pickup_blocked[i] > 0 && pickup_blocked[i] < 3) {
            pickup_blocked[i] = 3;
            return 1;
        }
    }

    return 0;
}

int CRandomItem::CheckPosition() {
    sceVu0FVECTOR player_position;
    int           gold = 0;

    sceVu0CopyVector(player_position, CharaMain.pos);

    for (int i = 0; i < 32; i++) {
        if (id[i] == -1 || state[i] != RANDOM_ITEM_WAITING) {
            continue;
        }

        distance[i] = DistVector(position[i], player_position);

        if (pickup_blocked[i] > 0 && distance[i] >= 8.5f) {
            pickup_blocked[i] = 0;
        }

        if (pickup_blocked[i] != 0 || distance[i] > 5.0f) {
            continue;
        }

        if (amount[i] == -1 && item_no[i] != -1) {
            int blocked = ((CDngStatusData *) UserStatus)->CheckItemGet(item_no[i]);

            if (blocked == 0) {
                pickup_event[i] = item_no[i];
                id[i] = -1;
            } else {
                pickup_blocked[i] = blocked;
            }
        } else {
            state[i] = RANDOM_ITEM_TAKEN;

            if (item_no[i] == -1) {
                SndSePlay(SE_ITEM_GET, -1, 0);
            }

            gold += amount[i];
            position[i][1] += 5.0f;
            HitValueEntry(NowHitValue, position[i], gold, HIT_VALUE_GOLD, NULL);
            position[i][1] -= 5.0f;
        }
    }

    return gold;
}

void CRandomItem::Set(float *drop_position, int slot_id, int gold, int item) {
    int slot = CheckID();

    if (slot != -1) {
        sceVu0CopyVector(position[slot], drop_position);
        position[slot][1] += 1.75f;
        id[slot] = slot_id;
        amount[slot] = gold;
        item_no[slot] = item;
        pickup_event[slot] = -1;
        state[slot] = RANDOM_ITEM_RISING;
        phase[slot] = 0.0f;
        pickup_blocked[slot] = 0;
    }
}

int CRandomItem::CheckID() {
    for (int i = 0; i < 32; i++) {
        if (id[i] == -1) {
            return i;
        }
    }

    bob_phase = 0.0f;
    return -1;
}

int CRandomItem::CheckItemNo(int item) {
    for (int i = 0; i < 32; i++) {
        if (id[i] != -1 && item_no[i] == item) {
            return item;
        }
    }

    return 0;
}

void CRandomItem::Step() {
    bob_phase += 0.05235988f;

    if (bob_phase > PI) {
        bob_phase -= PI;
    }

    for (int i = 0; i < 32; i++) {
        if (id[i] == -1) {
            continue;
        }

        switch (state[i]) {
            case RANDOM_ITEM_RISING:
                phase[i] += 0.10471976f;

                if (phase[i] > PI) {
                    state[i] = RANDOM_ITEM_WAITING;
                    phase[i] = 0.0f;

                    if (item_no[i] == -1) {
                        SndSePlay(SE_RANDOM_ITEM_EMPTY, -1, 0);
                    } else {
                        SndSePlay(SE_RANDOM_ITEM, -1, 0);
                    }
                }

                break;
            case RANDOM_ITEM_WAITING:
                break;
            case RANDOM_ITEM_TAKEN:
                phase[i] += 0.10471976f;

                if (phase[i] > 2.5132742f) {
                    id[i] = -1;
                }

                break;
        }
    }
}
