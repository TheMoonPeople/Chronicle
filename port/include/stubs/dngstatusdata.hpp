#pragma once

#include "itemdata.hpp"

/**
 * Weapon table, one entry per weapon from ITEM_WEAPON_START on. CDngStatusData::GetItem reads a
 * picked-up weapon's owner from it.
 */
extern WEAPON_DATA WeaponList[];
