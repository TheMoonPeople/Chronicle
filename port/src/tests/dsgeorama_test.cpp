#include <gtest/gtest.h>

#ifdef MOD_DEMON_SHAFT_GEORAMA

#include <set>

#include "btmisc.hpp"
#include "dsgeorama.hpp"
#include "dun/gameloop.hpp"
#include "dungeonparts.hpp"
#include "editatra.hpp"
#include "editpartsdata.hpp"
#include "mainselect.hpp"
#include "savedata.hpp"

// The Demon Shaft georama mod (ps2/src/savedata.cpp, docs/DEMON_SHAFT_GEORAMA.md).

namespace {

alignas(64) CSaveData g_dsg_save;

void NewGame() {
    g_dsg_save.Initialize();
    SaveData = &g_dsg_save;
    UserStatus = g_dsg_save.GetDngStatus();
    MapNo = -1;
}

// How many atla the donor's dungeon hands out that are not villagers.
int DonorAtlaCount() {
    int total = 0;

    for (ATRA_APPEAR *appear = AtraAppearData[DSG_DONOR_TOWN]; appear->id != -1; appear++) {
        if (appear->id < DSG_ATLA_FIRST_VILLAGER) {
            total += appear->count;
        }
    }

    return total;
}

} // namespace

// Every non-villager atla of the donor turns up in the Demon Shaft once, and then no more do.
TEST(DemonShaftGeorama, AtlaRunOutWithoutVillagers) {
    NewGame();
    int collected = 0;

    for (int floor = 0; floor < 5000 && collected < DonorAtlaCount() + 1; floor++) {
        int atra_no[8];
        int count = BtAtraFloorCyoice(DUNGEON_DEMON_SHAFT, floor % 100, atra_no);
        ASSERT_LE(count, 2);
        std::set<int> seen;

        for (int i = 0; i < count; i++) {
            ASSERT_TRUE(seen.insert(atra_no[i]).second) << "an atla lies twice on one floor";
            int id = DsgAtlaId(atra_no[i]);
            ASSERT_GE(id, 0);
            ASSERT_LT(id, DSG_ATLA_FIRST_VILLAGER);
            DsgAtlaCollected(atra_no[i]);
            collected++;
        }

        for (int i = count; i < 8; i++) {
            ASSERT_EQ(atra_no[i], -1);
        }
    }

    EXPECT_EQ(collected, DonorAtlaCount());
}

// Picking one up gives the part to the Demon Shaft's georama, not to the donor town.
TEST(DemonShaftGeorama, PickupFillsItsOwnGeorama) {
    NewGame();
    int atra_no[8];

    while (BtAtraFloorCyoice(DUNGEON_DEMON_SHAFT, 3, atra_no) == 0 || DsgAtlaId(atra_no[0]) >= 40) {
    }

    int id = DsgAtlaId(atra_no[0]);
    int before = DsgSave.atla[atra_no[0]].refcount;
    getAtraToSaveData(id, atra_no[0], SaveData, DUNGEON_DEMON_SHAFT, 3);

    EXPECT_EQ(DsgSave.atla[atra_no[0]].refcount, before - 1);
    EXPECT_EQ(DsgSave.georama.parts_info[id].obtained, 1);
    EXPECT_EQ(SaveData->GetEditPartsInfo(DSG_DONOR_TOWN, id)->obtained, 0);
    EXPECT_EQ(SaveData->GetEditPartsInfo(DSG_GEORAMA_NO, id)->obtained, 1);
}

// The donor town's save data is the Demon Shaft georama's only while the player stands in it.
TEST(DemonShaftGeorama, DonorTownRedirectsOnlyInside) {
    NewGame();
    SV_EDIT_PARTS_INFO *donor = SaveData->GetEditPartsInfo(DSG_DONOR_TOWN, 0);
    SV_EDIT_PARTS_INFO *own = &DsgSave.georama.parts_info[0];
    ASSERT_NE(donor, own);

    MapNo = DSG_DONOR_TOWN;
    DsgEnterTown(MapNo);
    EXPECT_EQ(SaveData->GetEditPartsInfo(DSG_DONOR_TOWN, 0), donor);

    DsgTownRequest = 1;
    DsgEnterTown(MapNo);
    EXPECT_EQ(DsgTownRequest, 0);
    EXPECT_EQ(SaveData->GetEditPartsInfo(DSG_DONOR_TOWN, 0), own);
    EXPECT_EQ(SaveData->GetElemData(DSG_DONOR_TOWN), DsgSave.elem_data);
    EXPECT_EQ(SaveData->GetGrdData(DSG_DONOR_TOWN), &DsgSave.georama);
    EXPECT_EQ(SaveData->GetGrdNPCData(DSG_DONOR_TOWN, 2), &DsgSave.georama.npc[2]);
    int count = -1;
    EXPECT_EQ(SaveData->GetParts(DSG_DONOR_TOWN, &count), DsgSave.georama.placed_parts);
    EXPECT_EQ(count, 0);
    // Other towns keep their own.
    EXPECT_NE(SaveData->GetEditPartsInfo(DSG_DONOR_TOWN - 1, 0), own);

    // The donor's dungeon, reached with the flag still set, is not the town.
    MapNo = 200 + DUNGEON_SUN_MOON_TEMPLE;
    EXPECT_EQ(SaveData->GetEditPartsInfo(DSG_DONOR_TOWN, 0), donor);

    // Going anywhere else ends it.
    MapNo = 0;
    DsgEnterTown(MapNo);
    MapNo = DSG_DONOR_TOWN;
    EXPECT_EQ(SaveData->GetEditPartsInfo(DSG_DONOR_TOWN, 0), donor);
}

// The Demon Shaft's georama draws its parts' records from the donor's tables.
TEST(DemonShaftGeorama, PartsComeFromTheDonor) {
    EXPECT_EQ(GetEditAtraPartsData(DSG_GEORAMA_NO, 3), GetEditAtraPartsData(DSG_DONOR_TOWN, 3));
    EXPECT_EQ(GetEditAtraData(DSG_GEORAMA_NO, 41), GetEditAtraData(DSG_DONOR_TOWN, 41));
}

// A save made in the Demon Shaft's georama returns there.
TEST(DemonShaftGeorama, SaveRecordRoundTrip) {
    NewGame();
    static char image[sizeof(SV_DSG_SAVE)];
    DsgSave.georama.placed_parts[0].part_id = 4;
    DsgTownRequest = 1;
    MapNo = DSG_DONOR_TOWN;
    DsgEnterTown(MapNo);
    DsgWriteSave(image);

    DsgInitialize();
    DsgReadSave(image, sizeof(image));
    EXPECT_EQ(DsgSave.georama.placed_parts[0].part_id, 4);
    EXPECT_EQ(DsgTownRequest, 1);

    DsgReadSave(image, sizeof(image) - 1);
    EXPECT_EQ(DsgSave.georama.placed_parts[0].part_id, -1);
    EXPECT_EQ(DsgTownRequest, 0);
}

#endif
