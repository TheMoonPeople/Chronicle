#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "weapon_buildup.hpp"

#include <cstring>

#include "battlemenu.hpp"
#include "itemdata.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_save.hpp"
#include "shop.hpp"

static int BuildMenuCompVolume(char requirement, char value) {
    int passes;

    if (0 <= requirement) {
        if (value >= requirement) {
            passes = 1;
        } else {
            passes = 0;
        }

        return passes;
    }

    requirement = -requirement;
    return requirement < value ? 0 : 1;
}

static int BuildMenuCompVolume(short requirement, short value) {
    int passes;

    if (0 <= requirement) {
        if (value >= requirement) {
            passes = 1;
        } else {
            passes = 0;
        }

        return passes;
    }

    requirement = -requirement;
    return requirement < value ? 0 : 1;
}

int CompareBuildUpModelData2(WEAPON_HAVE *destination, WEAPON_HAVE *weapon) {
    WEAPON_HAVE held_values;
    WeaponAllValueSet(weapon, &held_values, 1);

    s16 held_status[2] = {0, 0};
    held_status[0] = held_values.attack;
    held_status[1] = held_values.magic;

    s16 required_status[2] = {0, 0};
    required_status[0] = destination->attack;
    required_status[1] = destination->magic;

    for (int i = 0; i < 2; i++) {
        if (!BuildMenuCompVolume(required_status[i], held_status[i])) {
            return 0;
        }
    }

    for (int i = 0; i < 5; i++) {
        s8 required = destination->elem[i];

        if (!BuildMenuCompVolume(required, held_values.elem[i])) {
            return 0;
        }
    }

    for (int i = 0; i < 10; i++) {
        s8 required = destination->vs_monster[i];

        if (!BuildMenuCompVolume(required, held_values.vs_monster[i])) {
            return 0;
        }
    }

    return 1;
}

int IsLastWeapon(int weapon_no) {
    int result = 0;
    s16 last_weapons[15] = {
        ITEM_WEAPON_ARISE_MARDAN,
        ITEM_WEAPON_7THHEAVEN,
        ITEM_WEAPON_SWORD_OF_ZEUS,
        ITEM_WEAPON_CHRONICLE_SWORD,
        ITEM_WEAPON_SUPER_STEVE,
        ITEM_WEAPON_ANGE_GEAR,
        ITEM_WEAPON_TALL_HAMMER,
        ITEM_WEAPON_INFERNO,
        ITEM_WEAPON_MOBIUS_RING,
        ITEM_WEAPON_SECRET_ARMLET,
        ITEM_WEAPON_HERCULES_WRATH,
        ITEM_WEAPON_BABEL_S_SPEAR,
        ITEM_WEAPON_STAR_BREAKER,
        ITEM_WEAPON_SUPERNOVA,
        -1,
    };
    int i = 0;

    do {
        int translated_no = TransWepNo(last_weapons[i]);

        if (weapon_no == translated_no) {
            result = 1;
        } else {
            i++;

            if (translated_no > 0 && i < 15) {
                continue;
            }
        }

        break;
    } while (true);

    return result;
}

int IsNotBuildUpWeapon(int weapon_no) {
    if (weapon_no < ITEM_WEAPON_START) {
        return 1;
    }

    WEAPON_DATA *weapon = GetWeaponData(weapon_no);

    if (weapon->buildup_mask0 != 0 || weapon->buildup_mask1 != 0) {
        return 0;
    }

    return 1;
}

void SetWeaponBuildValue(WEAPON_HAVE *weapon, int destination_no) {
    WEAPON_DATA *destination = GetWeaponData(destination_no);

    if (weapon->endurance <= destination->endurance) {
        weapon->endurance = destination->endurance;
    }

    if (weapon->speed <= destination->speed) {
        weapon->speed = destination->speed;
    }

    if (weapon->durability < destination->durability) {
        weapon->durability = destination->durability;
    }
}

int EnableBuildUpModelSpecial(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon) {
    int result = 0;

    if (weapon == NULL) {
        return 0;
    }

    build_info[0].weapon_no = -1;
    build_info[0].enabled = false;

    switch (weapon->item_no) {
        case ITEM_WEAPON_MARDAN_EINS:
        case ITEM_WEAPON_MARDAN_TWEI:
            build_info[0].enabled = false;
            build_info[0].weapon_no = weapon->item_no + 1;

            int fish_count = GetFishMardanGarayanNum();
            int required_fish[2] = {5, 15};

            if (required_fish[weapon->item_no - ITEM_WEAPON_MARDAN_EINS] > fish_count) {
                break;
            }

            result = 1;
            build_info[0].enabled = true;
            break;
    }

    build_info[1].weapon_no = -1;
    build_info[1].enabled = false;
    return result;
}

int EnableBuildUpModel(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon) {
    if (weapon == NULL) {
        return 0;
    }

    int weapon_no = weapon->item_no;

    if (weapon_no != ITEM_WEAPON_MARDAN_EINS && weapon_no != ITEM_WEAPON_MARDAN_TWEI) {
    } else {
        EnableBuildUpModelSpecial(build_info, weapon);
        return 1;
    }

    WEAPON_DATA *weapon_data = GetWeaponData(weapon_no);
    WEAPON_HAVE  weapon_copy;
    memcpy(&weapon_copy, weapon, sizeof(WEAPON_HAVE));
    s8  owner = weapon_data->owner;
    int default_weapon_no = GetDefaultWeaponNo(owner);
    int weapon_max = MenuCharaWeaponMax[owner];
    int count = 0;

    for (int position = 2; position <= weapon_max; position++) {
        if ((position > 20 || (weapon_data->buildup_mask0 & (1 << position))) && (position <= 20 || (weapon_data->buildup_mask1 & (1 << (position - 20))))) {
            build_info[count].weapon_no = default_weapon_no + position;

            WEAPON_HAVE destination_have;
            WepDataListToHaveCopy(build_info[count].weapon_no, &destination_have);

            if (CompareBuildUpModelData2(&destination_have, weapon)) {
                build_info[count].enabled = true;
            } else {
                build_info[count].enabled = false;
            }

            count++;
        }
    }

    build_info[count].weapon_no = -1;
    return 1;
}
