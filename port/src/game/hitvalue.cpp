#include "hitvalue.hpp"

#include <libvu0.h>

#include <array>
#include <cmath>

#include "dngmessageman.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "itemdata.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"

namespace dc::game {

int defWeapon__2[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

int element_tbl[6] = {1, 2, 4, 8, 0x10, 0};

char *LanguageStr[7] = {"dun/img/jp/", "dun/img/us/", "dun/img/us_e/", "dun/img/fr/",
                        "dun/img/gr/", "dun/img/it/", "dun/img/sp/"};

int BattleSubWeaponDmg(float amount, int kind) {
    int          chara_no = UserStatus->cur_chara;
    WEAPON_HAVE *weapon = &UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]];

    if (weapon->item_no == ITEM_WEAPON_SERPENT_SWORD && SaveData->GetGameFlag(0x30) == 0) {
        return 0;
    }

    if (weapon->flags & 0x200) {
        amount *= 0.5f;
    }

    if (weapon->flags & 0x100) {
        amount *= 2.0f;
    }

    int default_weapon = defWeapon__2[chara_no];

    if (default_weapon == UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]].item_no) {
        return 0;
    }

    float wear = 1.5f - 0.01f * NowWeaponHave->endurance;
    float old_durability = weapon->durability_f;
    wear *= amount;
    wear += 0.1f * kind;
    weapon->durability_f = old_durability - wear;

    if (weapon->durability_f <= 0.0f) {
        int powder_slot = static_cast<CDngStatusData *>(UserStatus)->CheckActItemSlot(ITEM_AUTO_REPAIR_POWDER);

        if (powder_slot != -1) {
            DelActiveItem(powder_slot + 1);
            weapon->durability_f = weapon->durability;
            DngMessMan.message = 0xBC;
            DngMessMan.timer = 0xF0;
            DngMessMan.steev_window = false;
            SndSePlay(SE_WEAPON_REPAIR, -1, 0);
        }
    }

    if (weapon->durability_f <= 0.0f) {
        weapon->durability_f = 0.0f;
        SndSePlay(SE_WEAPON_BREAK, -1, 0);

        int now_item = UserStatus->chara_weapons[chara_no][UserStatus->equipped_weapon_slot[chara_no]].item_no;

        if (default_weapon + 1 == now_item) {
            DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
            DngMessMan.insert_value_1 = weapon->level;
            DngMessMan.message = 0xA1;
            DngMessMan.timer = 0x1E0;
            DngMessMan.steev_window = false;
            WepDataListToHaveCopy(default_weapon, weapon);
            SetWeaponAttachStatus(NowWeaponHave);
            NowWeaponHave->durability_f = 0.0f;
            weapon->durability_f = 0.0f;
            return 1;
        }

        if (default_weapon + 1 != static_cast<s16>(now_item)) {
            for (int slot = 0; slot < 10; slot++) {
                WEAPON_HAVE &replacement = UserStatus->chara_weapons[chara_no][slot];

                if (replacement.item_no == default_weapon) {
                    UserStatus->equipped_weapon_slot[chara_no] = static_cast<s8>(slot);
                    SetWeaponAttachStatus(NowWeaponHave);
                    DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
                    DngMessMan.insert_value_1 = weapon->level;
                    DngMessMan.insert_mes_2 = GetCommonItemDataSystemMsg(replacement.item_no);
                    DngMessMan.insert_value_2 = replacement.level;
                    DngMessMan.message = 0xA0;
                    DngMessMan.timer = 0x1E0;
                    DngMessMan.steev_window = false;
                    weapon->item_no = -1;
                    return 1;
                }

                if (replacement.item_no == default_weapon + 1) {
                    UserStatus->equipped_weapon_slot[chara_no] = static_cast<s8>(slot);
                    SetWeaponAttachStatus(NowWeaponHave);
                    DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
                    DngMessMan.insert_value_1 = weapon->level;
                    DngMessMan.insert_mes_2 = GetCommonItemDataSystemMsg(replacement.item_no);
                    DngMessMan.insert_value_2 = replacement.level;
                    DngMessMan.message = 0xA0;
                    DngMessMan.timer = 0x1E0;
                    DngMessMan.steev_window = false;

                    if (chara_no == 3) {
                        replacement.best_elem = 0;
                    }

                    weapon->item_no = -1;
                    return 2;
                }
            }
        }
    }

    if (0.1f * weapon->durability <= old_durability && 0.1f * weapon->durability > weapon->durability_f) {
        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
        DngMessMan.insert_value_1 = weapon->level;
        DngMessMan.message = 0x97;
        DngMessMan.timer = 0xF0;
        DngMessMan.steev_window = false;
    }

    if (0.05f * weapon->durability <= old_durability && 0.05f * weapon->durability > weapon->durability_f) {
        DngMessMan.insert_mes_1 = GetCommonItemDataSystemMsg(weapon->item_no);
        DngMessMan.insert_value_1 = weapon->level;
        DngMessMan.message = 0x98;
        DngMessMan.timer = 0xF0;
        DngMessMan.steev_window = false;
    }

    return 0;
}

void HitValueEntry(CHitValue *values, float *world, int amount, int kind, CFrame *frame) {
    for (int i = 0; i < 32; i++) {
        if (values[i].active == 0) {
            values[i].EntryValue(world, amount, kind, frame);
            return;
        }
    }
}

void CHitValue::EntryValue(float *world, int amount, int value_kind, CFrame *follow_frame) {
    int divisor = 10000;

    for (int i = 0; i < 5; i++) {
        digits[i] = -1;
        digit_angle[i] = -PI_SHORT;
    }

    kind = value_kind;
    alpha = 0.0f;
    alpha_speed = 3.0f;
    active = true;
    frame = follow_frame;
    sceVu0CopyVector(pos, world);
    pos[3] = 1.0f;

    // A zero hit shows a mark rather than a number, so it has no digits.
    if (value_kind == HIT_VALUE_ZERO) {
        digits[0] = -2;
        last_digit = 1;
        return;
    }

    bool seen_nonzero = false;

    for (int i = 4; i >= 0; i--) {
        int digit_value = amount / divisor;

        if (digit_value > 0) {
            seen_nonzero = true;
        }

        // The units place is always drawn; the rest only past the first digit.
        if (i == 0 || seen_nonzero) {
            digits[i] = digit_value;
            amount -= digit_value * divisor;

            if (divisor < 9) {
                last_digit = i;
            }
        }

        divisor /= 10;
    }

    switch (value_kind) {
        case HIT_VALUE_MONSTER:
            texel.x = 0;
            texel.y = 0x9E;
            texel.width = 0xC;
            texel.height = 0x12;
            break;
        case HIT_VALUE_GOLD:
            texel.x = 0;
            texel.y = 0x8C;
            texel.width = 0xC;
            texel.height = 0x12;
            break;
        case HIT_VALUE_PLAYER:
            texel.x = 0;
            texel.y = 0x7C;
            texel.width = 0xC;
            texel.height = 0x12;
            break;
        default:
            break;
    }
}

void CHitValue::Draw() {
    if (active == 0) {
        return;
    }

    if (kind == HIT_VALUE_PLAYER && static_cast<s32 *>(SaveData->GetConfigData())[9] == 1) {
        return;
    }

    if ((kind == HIT_VALUE_MONSTER || kind == HIT_VALUE_ZERO) && static_cast<s32 *>(SaveData->GetConfigData())[10] == 1) {
        return;
    }

    CTexture          *digit_texture = TexManager.GetTexture("stayframe", -1);
    CTexture          *mark_texture = TexManager.GetTexture("itempack", -1);
    std::array<int, 4> screen{};
    sceVu0FVECTOR      world;

    if (frame != nullptr) {
        sceVu0CopyVector(world, frame->position);
        world[0] += pos[0];
        world[1] += pos[1];
        world[2] += pos[2];
        world[3] = 1.0f;
    } else {
        sceVu0CopyVector(world, pos);
    }

    if (MGRotTransPers2D(screen.data(), world, 0) == 0) {
        return;
    }

    if (digits[0] == -2) {
        int bounce = static_cast<int>(48.0f * sinf(digit_angle[0]));
        set2DSprite(Vif1Packet, mark_texture, CRect_i_(screen[0], screen[1] - bounce - 48, 72, 24), CRect_i_(0, 128, 72, 24), static_cast<u8>(alpha));
        return;
    }

    for (int place = 0; place < 5; place++) {
        if (digits[place] == -1) {
            continue;
        }

        int bounce = static_cast<int>(48.0f * sinf(digit_angle[place]));
        set2DSprite(Vif1Packet, digit_texture, CRect_i_(screen[0] - place * texel.width, screen[1] - bounce - 48, texel.width, texel.height), CRect_i_(texel.x + digits[place] * texel.width, texel.y, texel.width, texel.height), static_cast<u8>(alpha));
    }
}

void CHitValue::Step() {
    if (active == 0) {
        return;
    }

    if (digits[0] == -2) {
        digit_angle[0] += PI_SHORT / 20.0f;

        if (digit_angle[0] >= PI_SHORT) {
            digit_angle[0] = PI_SHORT;
            alpha_speed *= -1.2f;
        }
    } else {
        // Each place further along hops more slowly.
        for (int i = 0; i < 5; i++) {
            if (digits[i] == -1) {
                continue;
            }

            digit_angle[i] += PI_SHORT / (20.0f + 5.0f * i);

            if (digit_angle[i] >= PI_SHORT) {
                digit_angle[i] = PI_SHORT;

                if (i == last_digit) {
                    alpha_speed *= -1.2f;
                    last_digit = -1;
                }
            }
        }
    }

    if (alpha_speed > 0.0f) {
        alpha += alpha_speed;

        if (alpha >= 128.0f) {
            alpha = 128.0f;
        }
    }

    if (alpha_speed < 0.0f) {
        alpha += alpha_speed;

        if (alpha <= 0.0f) {
            alpha = 0.0f;
            active = 0;
        }
    }
}

} // namespace dc::game
