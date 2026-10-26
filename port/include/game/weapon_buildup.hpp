#pragma once

#include "common.h"

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
 *
 * @mangled CompareBuildUpModelData2__FP11WEAPON_HAVEP11WEAPON_HAVE
 * @address 0x002350C0
 * @size 0x14C
 */
int CompareBuildUpModelData2(WEAPON_HAVE *destination, WEAPON_HAVE *weapon);

/**
 * Tests whether a weapon has no further build-up destination.
 *
 * @mangled IsLastWeapon__Fi
 * @address 0x00235210
 * @size 0xAC
 */
int IsLastWeapon(int weapon_no);

/**
 * Tests whether a weapon is excluded from the build-up system.
 *
 * @mangled IsNotBuildUpWeapon__Fi
 * @address 0x002352C0
 * @size 0x60
 */
int IsNotBuildUpWeapon(int weapon_no);

/**
 * Replaces a held weapon's values with those of its build-up destination.
 *
 * @mangled SetWeaponBuildValue__FP11WEAPON_HAVEi
 * @address 0x00235320
 * @size 0x78
 */
void SetWeaponBuildValue(WEAPON_HAVE *weapon, int destination_no);

/**
 * Enables build-up destinations governed by special weapon conditions.
 *
 * @mangled EnableBuildUpModelSpecial__FP16WEP_BUILDUP_INFOP11WEAPON_HAVE
 * @address 0x002353A0
 * @size 0xD8
 */
int EnableBuildUpModelSpecial(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon);

/**
 * Populates the build-up destinations currently available to a weapon.
 *
 * @mangled EnableBuildUpModel__FP16WEP_BUILDUP_INFOP11WEAPON_HAVE
 * @address 0x00235480
 * @size 0x1CC
 */
int EnableBuildUpModel(WEP_BUILDUP_INFO *build_info, WEAPON_HAVE *weapon);
