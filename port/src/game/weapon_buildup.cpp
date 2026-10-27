#include "weapon_buildup.hpp"

#include <algorithm>
#include <array>

#include "battlemenu.hpp"
#include "itemdata.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_save.hpp"
#include "shop.hpp"

namespace dc::game {

namespace {

// A non-negative requirement is a minimum; a negative one is a maximum of its magnitude.
int BuildMenuCompVolume(char requirement, char value) {
    if (requirement >= 0) {
        return value >= requirement ? 1 : 0;
    }

    requirement = static_cast<char>(-requirement);
    return requirement < value ? 0 : 1;
}

int BuildMenuCompVolume(short requirement, short value) {
    if (requirement >= 0) {
        return value >= requirement ? 1 : 0;
    }

    requirement = static_cast<short>(-requirement);
    return requirement < value ? 0 : 1;
}

constexpr std::array<s16, 15> kLastWeapons = {
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

constexpr std::array<int, 2> kRequiredMardanFish = {5, 15};

} // namespace

int CompareBuildUpModelData2(WEAPON_HAVE *destination, WEAPON_HAVE *weapon) {
    WEAPON_HAVE held_values;
    WeaponAllValueSet(weapon, &held_values, 1);

    const std::array<s16, 2> held_status = {held_values.attack, held_values.magic};
    const std::array<s16, 2> required_status = {destination->attack, destination->magic};

    for (int i = 0; i < 2; i++) {
        if (!BuildMenuCompVolume(required_status[i], held_status[i])) {
            return 0;
        }
    }

    for (int i = 0; i < 5; i++) {
        if (!BuildMenuCompVolume(destination->elem[i], held_values.elem[i])) {
            return 0;
        }
    }

    for (int i = 0; i < 10; i++) {
        if (!BuildMenuCompVolume(destination->vs_monster[i], held_values.vs_monster[i])) {
            return 0;
        }
    }

    return 1;
}

int IsLastWeapon(int weapon_no) {
    for (const s16 last_weapon : kLastWeapons) {
        const int translated_no = TransWepNo(last_weapon);

        if (weapon_no == translated_no) {
            return 1;
        }

        if (translated_no <= 0) {
            break;
        }
    }

    return 0;
}

int IsNotBuildUpWeapon(int weapon_no) {
    if (weapon_no < ITEM_WEAPON_START) {
        return 1;
    }

    const WEAPON_DATA *weapon = GetWeaponData(weapon_no);

    if (weapon->buildup_mask0 != 0 || weapon->buildup_mask1 != 0) {
        return 0;
    }

    return 1;
}

void SetWeaponBuildValue(WEAPON_HAVE *weapon, int destination_no) {
    const WEAPON_DATA *destination = GetWeaponData(destination_no);

    weapon->endurance = std::max(weapon->endurance, destination->endurance);
    weapon->speed = std::max(weapon->speed, destination->speed);
    weapon->durability = std::max(weapon->durability, destination->durability);
}

int EnableBuildUpModelSpecial(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon) {
    if (weapon == nullptr) {
        return 0;
    }

    int result = 0;

    build_info[0].weapon_no = -1;
    build_info[0].enabled = false;

    if (weapon->item_no == ITEM_WEAPON_MARDAN_EINS || weapon->item_no == ITEM_WEAPON_MARDAN_TWEI) {
        build_info[0].enabled = false;
        build_info[0].weapon_no = static_cast<s16>(weapon->item_no + 1);

        const int fish_count = GetFishMardanGarayanNum();

        if (kRequiredMardanFish[weapon->item_no - ITEM_WEAPON_MARDAN_EINS] <= fish_count) {
            result = 1;
            build_info[0].enabled = true;
        }
    }

    build_info[1].weapon_no = -1;
    build_info[1].enabled = false;
    return result;
}

int EnableBuildUpModel(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon) {
    if (weapon == nullptr) {
        return 0;
    }

    const int weapon_no = weapon->item_no;

    if (weapon_no == ITEM_WEAPON_MARDAN_EINS || weapon_no == ITEM_WEAPON_MARDAN_TWEI) {
        EnableBuildUpModelSpecial(build_info, weapon);
        return 1;
    }

    const WEAPON_DATA *weapon_data = GetWeaponData(weapon_no);
    const s8           owner = weapon_data->owner;
    const int          default_weapon_no = GetDefaultWeaponNo(owner);
    const int          weapon_max = MenuCharaWeaponMax[owner];
    int                count = 0;

    // Positions 0-20 are flagged in buildup_mask0, positions above 20 in buildup_mask1.
    for (int position = 2; position <= weapon_max; position++) {
        const bool low_allowed = position > 20 || (weapon_data->buildup_mask0 & (1U << position)) != 0;
        const bool high_allowed = position <= 20 || (weapon_data->buildup_mask1 & (1U << (position - 20))) != 0;

        if (low_allowed && high_allowed) {
            build_info[count].weapon_no = static_cast<s16>(default_weapon_no + position);

            WEAPON_HAVE destination_have;
            WepDataListToHaveCopy(build_info[count].weapon_no, &destination_have);

            build_info[count].enabled = CompareBuildUpModelData2(&destination_have, weapon) ? 1 : 0;

            count++;
        }
    }

    build_info[count].weapon_no = -1;
    return 1;
}

} // namespace dc::game
