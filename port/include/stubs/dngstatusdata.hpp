#pragma once

#include <cstddef>

#include "dungeonparts.hpp" // declares ItemPutListTbl12_bytes before the macro below renames its use
#include "itemdata.hpp"

extern WEAPON_DATA WeaponList[];

// GetItem reads a picked-up weapon's owner as ItemPutListTbl12_bytes[750 + id * 76]. On the PS2
// link WeaponList sits 0x4F30 bytes after ItemPutListTbl12, so that byte is
// WeaponList[id - ITEM_WEAPON_START].owner; a native link places the two tables anywhere, so the
// subscript is decoded back into the typed lookup instead.
static_assert(sizeof(WEAPON_DATA) == 76 && 750 + ITEM_WEAPON_START * 76 - 0x4F30 == offsetof(WEAPON_DATA, owner));

struct PortWeaponOwners {
    s8 operator[](int offset) const {
        const int item_id = (offset - 750) / 76;
        return static_cast<s8>(WeaponList[item_id - ITEM_WEAPON_START].owner);
    }
};

#define ItemPutListTbl12_bytes (PortWeaponOwners{})
