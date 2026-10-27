#pragma once

#include "common.h"

namespace dc::game {

struct WEAPON_HAVE;

/**
 * Describes one destination displayed by the weapon build-up menu.
 */
struct WEP_BUILDUP_INFO {
    s16 weapon_no; /**< Destination weapon identifier, or -1 after the final entry. */
    s16 enabled;   /**< Whether the held weapon meets this destination's requirements. */
};

STATIC_ASSERT(sizeof(WEP_BUILDUP_INFO) == 4);

/**
 * Tests whether a held weapon meets the build-up requirements of a destination weapon.
 */
int CompareBuildUpModelData2(WEAPON_HAVE *destination, WEAPON_HAVE *weapon);

/**
 * Tests whether a weapon has no further build-up destination.
 */
int IsLastWeapon(int weapon_no);

/**
 * Tests whether a weapon is excluded from the build-up system.
 */
int IsNotBuildUpWeapon(int weapon_no);

/**
 * Replaces a held weapon's values with those of its build-up destination.
 */
void SetWeaponBuildValue(WEAPON_HAVE *weapon, int destination_no);

/**
 * Enables build-up destinations governed by special weapon conditions.
 */
int EnableBuildUpModelSpecial(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon);

/**
 * Populates the build-up destinations currently available to a weapon.
 */
int EnableBuildUpModel(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon);

} // namespace dc::game
