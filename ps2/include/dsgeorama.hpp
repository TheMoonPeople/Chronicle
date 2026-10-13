#pragma once

#include "common.h"

/**
 * @file
 * The Demon Shaft georama mod: a seventh georama whose atla lie in the Demon
 * Shaft. It is built on the ground and parts of a town of the game (the donor),
 * but keeps its own parts, placement, chips and atla, apart from the donor's.
 *
 * Retail has none of this; everything here exists only with
 * MOD_DEMON_SHAFT_GEORAMA defined. docs/DEMON_SHAFT_GEORAMA.md describes it.
 */

#ifdef MOD_DEMON_SHAFT_GEORAMA

#include "dungeon.h"

#include "savedata.hpp"

/** The georama number the Demon Shaft's georama is kept under. */
#define DSG_GEORAMA_NO 6

/** The town whose ground, parts, names and pictures the Demon Shaft's georama is built from (Muska Lacka). */
#define DSG_DONOR_TOWN 3

/** Atla records the Demon Shaft hands out from. */
#define DSG_ATLA_MAX 100

/** Atla ids from this one on are villagers, which stay in their own town. */
#define DSG_ATLA_FIRST_VILLAGER 80

/** The size of the record a save keeps after retail's checksum. */
#define DSG_SAVE_SIZE ((int) sizeof(SV_DSG_SAVE))

/**
 * The Demon Shaft georama's part of a save. It is written behind retail's save
 * image, so a save without it is still a retail save.
 */
struct SV_DSG_SAVE {
    char            magic[8];           /**< DSG_SAVE_MAGIC when the record is there. */
    s32             atla_ready;         /**< Whether atla[] has been filled from the donor's table. */
    s32             in_town;            /**< Whether the save was made in the Demon Shaft's georama. */
    ATRA_SAVE       atla[DSG_ATLA_MAX]; /**< The atla the Demon Shaft still hands out; an empty record has id -1. */
    SV_GEORAMA_DATA georama;            /**< The georama, as SV_GEORAMA_DATA keeps a town's. */
    s16             elem_data[128];     /**< The georama's chips. */
    s32             check_sum;          /**< Sum of every byte before it. */
};

/** The Demon Shaft georama's save record of the game being played. */
extern SV_DSG_SAVE DsgSave;

/** Set while the player stands in the Demon Shaft's georama: the donor town's saves then are its. */
extern int DsgTownActive;

/** Set by the world map when the next town entered is the Demon Shaft's georama. */
extern int DsgTownRequest;

/** Empties the Demon Shaft's georama, for a new game. */
void DsgInitialize();

/** The georama whose save data a town's number reaches: the Demon Shaft's in its place of the donor. */
int DsgSaveIndex(int georama_no);

/** The town whose parts, names and pictures a georama uses: the donor's for the Demon Shaft's. */
int DsgAssetIndex(int georama_no);

/** Starts the town that a map number names: the Demon Shaft's georama when the world map asked for it. */
void DsgEnterTown(int map_no);

/**
 * Picks the atla that lie on one Demon Shaft floor and writes their records to
 * atra_no[0..8), the rest -1. Returns how many there are.
 */
int DsgAtlaFloorChoose(int floor, int *atra_no);

/** The atla id of one of the Demon Shaft's atla records, or -1. */
int DsgAtlaId(int atra_no);

/** Marks one of the Demon Shaft's atla picked up. */
void DsgAtlaCollected(int atra_no);

/** Writes the Demon Shaft georama's record to a save image at dst. */
void DsgWriteSave(char *dst);

/** Reads the record from a save image at src, of size bytes; an empty or broken one leaves an empty georama. */
void DsgReadSave(const char *src, int size);

#endif
