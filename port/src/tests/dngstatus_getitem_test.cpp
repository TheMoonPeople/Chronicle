#include <gtest/gtest.h>

#include <cstring>
#include <memory>

#include "dngstatusdata.hpp"
#include "itemdata.hpp"

// CDngStatusData::GetItem files a picked-up weapon under the character WEAPON_DATA::owner names.
// The PS2 source reads that byte as ItemPutListTbl12_bytes[750 + id * 76], an address that lands
// in WeaponList only on the PS2 link; port/include/stubs/dngstatusdata.hpp decodes it into the
// table lookup. A different native layout can make that read select the wrong character or index
// outside chara_weapons.
TEST(DngStatusGetItem, WeaponIsFiledUnderItsOwner) {
    int checked = 0;

    for (int item = ITEM_WEAPON_START; item < ITEM_WEAPON_END; item++) {
        WEAPON_DATA *data = GetWeaponData(item);

        if (data == nullptr) {
            continue;
        }

        ASSERT_LT(data->owner, 6) << "item " << item;
        auto status = std::make_unique<CDngStatusData>();
        std::memset(status->chara_weapons, 0, sizeof status->chara_weapons);
        ASSERT_EQ(status->GetItem(item, 1), 0) << "item " << item;

        for (int chara = 0; chara < 6; chara++) {
            for (int slot = 0; slot < 11; slot++) {
                int expected = chara == data->owner && slot == 0 ? item : 0;
                EXPECT_EQ(status->chara_weapons[chara][slot].item_no, expected)
                    << "item " << item << " chara " << chara << " slot " << slot;
            }
        }

        checked++;
    }

    EXPECT_GT(checked, 100);
}

// A second weapon of the same owner takes the next free slot.
TEST(DngStatusGetItem, WeaponTakesTheOwnersNextFreeSlot) {
    auto status = std::make_unique<CDngStatusData>();
    std::memset(status->chara_weapons, 0, sizeof status->chara_weapons);
    int owner = GetWeaponData(ITEM_WEAPON_BASELARD)->owner;

    // Toan's: the table's own value, not one read through the lookup under test.
    ASSERT_EQ(owner, 0);
    ASSERT_EQ(GetWeaponData(ITEM_WEAPON_DAGGER)->owner, owner);
    EXPECT_EQ(status->GetItem(ITEM_WEAPON_DAGGER, 1), 0);
    EXPECT_EQ(status->GetItem(ITEM_WEAPON_BASELARD, 1), 1);
    EXPECT_EQ(status->chara_weapons[owner][0].item_no, ITEM_WEAPON_DAGGER);
    EXPECT_EQ(status->chara_weapons[owner][1].item_no, ITEM_WEAPON_BASELARD);
}
