#include "stubs/dngstatusdata.hpp"

#include "dngstatusdata.hpp"

#include "dun/gameloop.hpp"
#include "dungeonparts.hpp"
#include "itemdata.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "userstatus.hpp"

/* CDngStatusData's and CUserStatus's methods are interleaved in retail
 * (SetNowFloor..SearchItemIndexNo, ChkEventFlag..ClearEventFlag, LostItem..
 * CheckDefaultWeapon, AddDrink..Init, SetDead..GetAtraData), so both classes
 * live in this one translation unit, in that order.
 */

#include <cstdio>
#include <cstdlib>

/** Default weapon id per character. */
static s32 defWeapon[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

static inline int GetMaxDungeonItems() {
    return 100;
}

/* Number of atra_grid slots per floor (atra_grid[georama][floor][0..8)). */
static inline int GetMaxAtraSlotNo() {
    return 8;
}

/* @ 0x1BD900 (0x40 bytes) -- SetNowFloor__14CDngStatusDataFi */
void CDngStatusData::SetNowFloor(int floor) {

    this->prev_floor = this->cur_floor;
    this->cur_floor = floor;

    int cur_georama = this->cur_georama;
    s8 *reached_table = this->floor_reached;
    s8 *reached = reached_table + cur_georama;

    if (floor > *reached) {
        *reached = floor;
    }
}

/* Item-id ranges are semantic: [132,257)=dungeon items (quick-slots + 103-slot
 * inventory), [81,132)=consumables (43-slot array, only first 40 searched
 * here), [257,+)=weapons (per-character 11-slot equipment, only first 10 of
 * 11 slots searched here). */
/* @ 0x1BD940 (0x180 bytes) -- SearchItemIndexNo__14CDngStatusDataFi */
int CDngStatusData::SearchItemIndexNo(int item_id) {
    int i;
    int valid;

    if (!(valid = item_id < ITEM_DUNGEON_START) && item_id < ITEM_WEAPON_START) {
        for (i = 0; (valid = i < 3) != 0; i++) {
            if (this->item_pack.quick_item_slot[i] == item_id) {
                return i;
            }
        }
    }

    if (!(valid = item_id <= ITEM_DUNGEON_START - 1) && item_id < ITEM_WEAPON_START) {
        for (i = 0; (valid = i < this->item_pack.num) != 0; i++) {
            if (this->item_pack.item[i] == item_id) {
                return i;
            }
        }
    }

    if (!(valid = item_id < ITEM_ATTACH_START) && item_id < ITEM_DUNGEON_START) {
        for (i = 0; (valid = i < 40) != 0; i++) {
            if (this->consumable_items[i].id == item_id) {
                return i;
            }
        }
    }

    if (!(valid = item_id < ITEM_WEAPON_START)) {
        int j;

        for (j = 0; (valid = j < this->party_size) != 0; j++) {
            for (i = 0; (valid = i < 10) != 0; i++) {
                if (this->chara_weapons[j][i].item_no == item_id) {
                    return i;
                }
            }
        }
    }

    return -1;
}

/* Reads one of the 50 story event flags. The range guard is `&&` in retail,
 * not `||` -- both halves can never hold at once, so it never fires and the
 * out-of-range read it was meant to stop happens anyway. Reproduced as
 * written; that dead guard is also why nobody noticed its bound is 51, one
 * past the 50 flags ClearEventFlag actually clears. */
/* @ 0x1BDAC0 (0x60 bytes) -- ChkEventFlag__11CUserStatusFi */
int CUserStatus::ChkEventFlag(int flag_no) {
    if (flag_no < 0 && flag_no > 50) {
        printf("err flag\n");
        return 0;
    }

    return this->event_flags[flag_no];
}

/* @ 0x1BDB20 (0x40 bytes) -- ClearEventFlag__11CUserStatusFv */
void CUserStatus::ClearEventFlag() {
    int i;
    int valid;

    for (i = 0; (valid = i < 50) != 0; i++) {
        this->event_flags[i] = 0;
    }
}

/* @ 0x1BDB60 (0x60 bytes) -- LostItem__14CDngStatusDataFi */
int CDngStatusData::LostItem(int item_id) {
    int i;
    int valid;

    for (i = 0; (valid = i < 103) != 0; i++) {
        if (this->item_pack.item[i] == item_id) {
            this->item_pack.item[i] = -1;
            this->item_pack.item_vol[i] = 0;
            return i;
        }
    }

    return -1;
}

/* @ 0x1BDBC0 (0x130 bytes) -- LostGateKey__14CDngStatusDataFv */
void CDngStatusData::LostGateKey() {
    int i;
    int valid;

    for (i = 0; (valid = i < 103) != 0; i++) {
        if (this->item_pack.item[i] == ITEM_DRAN_S_CREST) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_SHINY_STONE) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_RED_BERRY) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_HOOK) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_KING_S_SLATE) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_GUN_POWDER) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_CLOCK_HANDS) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_POINTY_CHESTNUT) {
            this->item_pack.item[i] = -1;
        }

        if (this->item_pack.item[i] == ITEM_BLACK_KNIGHT_CREST) {
            this->item_pack.item[i] = -1;
        }

        if (!(valid = this->item_pack.item[i] < ITEM_BONE_KEY) && this->item_pack.item[i] <= ITEM_SILVER_KEY) {
            this->item_pack.item[i] = -1;
        }
    }
}

/* @ 0x1BDCF0 (0x50 bytes) -- GetLiveUnit__14CDngStatusDataFv */
int CDngStatusData::GetLiveUnit() {
    int result = 0;
    int i;
    int valid;

    for (i = 0; (valid = i < this->party_size) != 0; i++) {
        if (this->hp[i] > 0) {
            result++;
        }
    }

    return result;
}

/* @ 0x1BDD40 (0x190 bytes) -- CheckItemGet__14CDngStatusDataFi */
int CDngStatusData::CheckItemGet(int item_id) {
    int result = 0;

    if ((u32) (item_id - ITEM_MAP) <= 1 || item_id == ITEM_WORLDMAP) {
        return 0;
    }

    {
        int valid;
        int i;

        if (!(valid = item_id < GetDungeonItemStart()) && item_id < GetDungeonItemEnd()) {
            int count = 0;
            int k;

            for (i = 0; (valid = i < GetMaxDungeonItems()) != 0; i++) {
                if (!(valid = this->item_pack.item[i] < ITEM_DUNGEON_START) && this->item_pack.item[i] < ITEM_WEAPON_START) {
                    count++;
                }
            }

            for (k = 0; (valid = k < 3) != 0; k++) {
                if (this->item_pack.quick_item_slot[k] != -1) {
                    count += this->item_pack.quick_item_qty[k];
                }
            }

            if (count < this->item_pack.num) {
                result = 0;
            } else {
                return 1;
            }
        }

        if (!(valid = item_id < ITEM_ATTACH_START) && item_id < ITEM_DUNGEON_START) {
            int count = 0;
            int j;

            for (j = 0; (valid = j < 40) != 0; j++) {
                s16 held_id = this->consumable_items[j].id;

                if (!(valid = held_id < ITEM_ATTACH_START) && held_id < ITEM_DUNGEON_START) {
                    count++;
                }
            }

            /* Retail assigns `result` here rather than returning it, so the
             * two-constant tail stays a real branch instead of collapsing to
             * a movn/movz conditional move. */
            if (count < 40) {
                result = 0;
            } else {
                return 2;
            }
        }
    }

    return result;
}

/* @ 0x1BDED0 (0xC0 bytes) -- CheckWeaponUser__14CDngStatusDataFi */
int CDngStatusData::CheckWeaponUser(int weapon_id) {
    int result = -1;
    int valid;

    if (!(valid = weapon_id < ITEM_WEAPON_DAGGER_BROKEN) && weapon_id < ITEM_WEAPON_WOODENSLINGSHOT_BROKEN) {
        result = CHARA_TOAN;
    }

    if (!(valid = weapon_id < ITEM_WEAPON_WOODENSLINGSHOT_BROKEN) && weapon_id < ITEM_WEAPON_MALLET_BROKEN) {
        result = CHARA_XIAO;
    }

    if (!(valid = weapon_id < ITEM_WEAPON_MALLET_BROKEN) && weapon_id < ITEM_WEAPON_GOLD_RING_BROKEN) {
        result = CHARA_GORO;
    }

    if (!(valid = weapon_id < ITEM_WEAPON_GOLD_RING_BROKEN) && weapon_id < ITEM_WEAPON_FIGHTING_STICK_BROKEN) {
        result = CHARA_RUBY;
    }

    if (!(valid = weapon_id < ITEM_WEAPON_FIGHTING_STICK_BROKEN) && weapon_id < ITEM_WEAPON_MACHINE_GUN_BROKEN) {
        result = CHARA_UNGAGA;
    }

    if (!(valid = weapon_id < ITEM_WEAPON_MACHINE_GUN_BROKEN) && weapon_id < ITEM_WEAPON_END) {
        result = CHARA_OSMOND;
    }

    return result;
}

/* @ 0x1BDF90 (0xC0 bytes) -- CheckWeaponRot__14CDngStatusDataFi */
int CDngStatusData::CheckWeaponRot(int weapon_id) {
    int chara_no = this->CheckWeaponUser(weapon_id);

    if (chara_no == -1) {
        return -1;
    }

    /* Retail places this early-out physically LAST (right before the
     * epilogue, falling through with no extra jump) rather than as an
     * early return -- the loop's "return count" path is emitted first.
     * The goto reproduces that exact block layout/branch-threading. */
    int valid;

    if ((valid = weapon_id < ITEM_WEAPON_START) != 0) {
        goto ret_minus1;
    }

    {
        int count = 0;
        int i;

        for (i = 0; (valid = i < 10) != 0; i++) {
            if (!(valid = this->chara_weapons[chara_no][i].item_no < ITEM_WEAPON_START)) {
                count++;
            }
        }

        return count;
    }

ret_minus1:
    return -1;
}

/* Index 6 is the bonus "Deamon Shaft" dungeon (0-5 are the six story
 * georama dungeons). */
/* @ 0x1BE050 (0x10 bytes) -- ClearDeamonShaft__14CDngStatusDataFv */
void CDngStatusData::ClearDeamonShaft() {
    this->floor_reached[6] = -1;
}

/* Routes by the same id ranges as SearchItemIndexNo. Dungeon items (103-slot
 * array) and consumables (43-slot array) are both sized capacity+3 to allow
 * temporary overflow before overflow_flag is latched. Weapons are auto-routed
 * to their owning character via the per-item ownership table (see
 * ItemPutListTbl12 above), not the id-range heuristic CheckWeaponUser uses. */
/* @ 0x1BE060 (0x400 bytes) -- GetItem__14CDngStatusDataFii */
int CDngStatusData::GetItem(int item_id, int qty) {
    int valid;

    printf("GetITEM No === %d\n", item_id);

    if (!(valid = item_id < ITEM_DUNGEON_START) && item_id < ITEM_WEAPON_START) {
        int i;

        for (i = 0; (valid = i < 103) != 0; i++) {
            if (this->item_pack.item[i] < ITEM_DUNGEON_START) {
                if (item_id == ITEM_WORLDMAP) {
                    this->special_flag_238 = true;
                    return 0;
                }

                int count = 0;
                int j;

                for (j = 0; (valid = j < 103) != 0; j++) {
                    if (!(valid = this->item_pack.item[j] < ITEM_DUNGEON_START)) {
                        count++;
                    }
                }

                int k;

                for (k = 0; (valid = k < 3) != 0; k++) {
                    if (this->item_pack.quick_item_slot[k] != -1) {
                        count += this->item_pack.quick_item_qty[k];
                    }
                }

                if (count + 1 > this->item_pack.num) {
                    this->overflow_flag = true;
                }

                int have_copy = ItemDataToHaveCopy(item_id);

                if (this->overflow_flag != 0) {
                    int m;

                    for (m = 0; (valid = m < 3) != 0; m++) {
                        if (this->item_pack.item[this->item_pack.num + m] == -1) {
                            this->item_pack.item[this->item_pack.num + m] = item_id;
                            this->item_pack.item_vol[this->item_pack.num + m] = have_copy;
                            return i;
                        }
                    }
                } else {
                    this->item_pack.item[i] = item_id;
                    this->item_pack.item_vol[i] = have_copy;
                }

                return i;
            }
        }
    }

    if (!(valid = item_id < ITEM_ATTACH_START) && item_id < ITEM_DUNGEON_START) {
        int n;

        for (n = 0; (valid = n < 43) != 0; n++) {
            if (this->consumable_items[n].id < ITEM_ATTACH_START) {
                this->consumable_items[n].id = item_id;
                SetAttachMentValue(item_id, n, qty, (ATTACH_LIST *) 0);

                int count = 0;
                int ii;

                for (ii = 0; (valid = ii < 43) != 0; ii++) {
                    s16 held_id = this->consumable_items[ii].id;

                    if (!(valid = held_id < ITEM_ATTACH_START) && held_id < ITEM_DUNGEON_START) {
                        count++;
                    }
                }

                if (count > 40) {
                    this->overflow_flag++;
                }

                return n;
            }
        }
    }

    if (!(valid = item_id < ITEM_WEAPON_START)) {
        printf("get weapon!! %d\n", item_id);
        int chara_no = ItemPutListTbl12_bytes[750 + item_id * 76];

        int jj;

        for (jj = 0; (valid = jj < 11) != 0; jj++) {
            if (this->chara_weapons[chara_no][jj].item_no < ITEM_WEAPON_START) {
                WepDataListToHaveCopy(item_id, &this->chara_weapons[chara_no][jj]);

                int count = 0;
                int kk;

                for (kk = 0; (valid = kk < 10) != 0; kk++) {
                    if (!(valid = this->chara_weapons[chara_no][kk].item_no < ITEM_WEAPON_START)) {
                        count++;
                    }
                }

                if (!(valid = count < 10)) {
                    this->overflow_flag = true;
                }

                return jj;
            }
        }
    }

    return -1;
}

/* @ 0x1BE460 (0x50 bytes) -- CheckActItemSlot__14CDngStatusDataFi */
int CDngStatusData::CheckActItemSlot(int item_id) {
    int i;
    int valid;

    for (i = 0; (valid = i < 3) != 0; i++) {
        if (this->item_pack.quick_item_slot[i] == item_id) {
            return i;
        }
    }

    return -1;
}

/* True if the character's currently-equipped weapon slot doesn't hold their
 * canonical default weapon (defWeapon[chara_no]). */
/* @ 0x1BE4B0 (0x60 bytes) -- CheckDefaultWeapon__14CDngStatusDataFi */
int CDngStatusData::CheckDefaultWeapon(int chara_no) {
    s32 def_weapon = defWeapon[chara_no];

    if (def_weapon == this->chara_weapons[chara_no][this->equipped_weapon_slot[chara_no]].item_no) {
        return 0;
    }

    return 1;
}

/* Adds to a character's water gauge. With ratio == 0 the gauge moves
 * instantly; otherwise the target is latched into drink_next[] and
 * drink_step[] holds the per-frame delta CUserStatus::Step applies, signed so
 * that a zero-rounding step still moves the gauge one unit in the right
 * direction. */
/* @ 0x1BE510 (0x200 bytes) -- AddDrink__11CUserStatusFisf */
void CUserStatus::AddDrink(int chara_no, s16 amount, float ratio) {
    if (this->drink_step[chara_no] != 0) {
        this->water_now[chara_no] = (float) this->drink_next[chara_no];
        this->drink_step[chara_no] = 0;
    }

    if (0.0f == ratio) {
        this->water_now[chara_no] = this->water_now[chara_no] + (float) amount;

        if (this->water_now[chara_no] >= this->water_max[chara_no]) {
            this->water_now[chara_no] = this->water_max[chara_no];
        }
    } else {
        this->drink_next[chara_no] = this->water_now[chara_no] + (float) amount;

        if (this->drink_next[chara_no] <= 0) {
            this->drink_next[chara_no] = 0;
        }

        if ((float) this->drink_next[chara_no] >= this->water_max[chara_no]) {
            this->drink_next[chara_no] = this->water_max[chara_no];
        }

        this->drink_step[chara_no] = ratio * (((float) this->drink_next[chara_no] - this->water_now[chara_no]) / 100.0f);

        if (this->drink_step[chara_no] == 0) {
            if ((float) this->drink_next[chara_no] - this->water_now[chara_no] < 0.0f) {
                this->drink_step[chara_no] = -1;
            } else {
                this->drink_step[chara_no] = 1;
            }
        }
    }
}

/* Same instant/interpolated split as AddDrink, for HP. */
/* @ 0x1BE710 (0x180 bytes) -- AddNowLife__11CUserStatusFisf */
void CUserStatus::AddNowLife(int chara_no, s16 amount, float ratio) {
    int valid;

    if (this->life_step[chara_no] != 0) {
        this->hp[chara_no] = this->next_hp[chara_no];
        this->life_step[chara_no] = 0;
    }

    if (0.0f == ratio) {
        this->hp[chara_no] = this->hp[chara_no] + amount;

        if (this->hp[chara_no] <= 0) {
            this->hp[chara_no] = 0;
        }

        {
            s16 life_value = this->hp[chara_no];

            if (!(valid = life_value < this->max_hp[chara_no])) {
                this->hp[chara_no] = this->max_hp[chara_no];
            }
        }
    } else {
        this->next_hp[chara_no] = this->hp[chara_no] + amount;

        if (this->next_hp[chara_no] <= 0) {
            this->next_hp[chara_no] = 0;
        }

        {
            s16 life_value = this->next_hp[chara_no];

            if (!(valid = life_value < this->max_hp[chara_no])) {
                this->next_hp[chara_no] = this->max_hp[chara_no];
            }
        }

        this->life_step[chara_no] = ratio * ((float) (this->next_hp[chara_no] - this->hp[chara_no]) / 100.0f);

        if (this->life_step[chara_no] == 0) {
            if (this->next_hp[chara_no] - this->hp[chara_no] < 0) {
                this->life_step[chara_no] = -1;
            } else {
                this->life_step[chara_no] = 1;
            }
        }
    }
}

/* Alive iff the active character has HP left -- and, while an interpolated HP
 * change is in flight, iff its target is above zero too. */
/* @ 0x1BE890 (0x70 bytes) -- CheckLife__11CUserStatusFv */
int CUserStatus::CheckLife() {
    if (this->life_step[this->cur_chara] != 0 && this->next_hp[this->cur_chara] <= 0) {
        return 0;
    }

    if (this->hp[this->cur_chara] <= 0) {
        return 0;
    }

    return 1;
}

/* Sets an absolute HP target (clamped to 0..max_hp), instantly or
 * interpolated, per the same rules as AddNowLife. */
/* @ 0x1BE900 (0x150 bytes) -- SetNextLife__11CUserStatusFisf */
void CUserStatus::SetNextLife(int chara_no, s16 value, float ratio) {
    if (this->life_step[chara_no] != 0) {
        this->hp[chara_no] = this->next_hp[chara_no];
        this->life_step[chara_no] = 0;
    }

    if (value <= 0) {
        value = 0;
    }

    if (value >= this->max_hp[chara_no]) {
        value = this->max_hp[chara_no];
    }

    if (ratio == 0.0f) {
        this->hp[chara_no] = value;
    } else {
        this->next_hp[chara_no] = value;

        this->life_step[chara_no] = ratio * ((this->next_hp[chara_no] - this->hp[chara_no]) / 100.0f);

        if (this->life_step[chara_no] == 0) {
            if (this->next_hp[chara_no] - this->hp[chara_no] < 0) {
                this->life_step[chara_no] = -1;
            } else {
                this->life_step[chara_no] = 1;
            }
        }
    }
}

/* Per-frame update: drains the active character's water gauge at a rate that
 * scales with dungeon depth (and is multiplied by the level-11 restriction
 * zone and by two equipped-weapon flags), costs 1 HP per 120 frames once the
 * gauge is empty, then advances every character's in-flight water and HP
 * interpolations by one step. */
/* @ 0x1BEA50 (0x390 bytes) -- Step__11CUserStatusFi */
void CUserStatus::Step(int mode) {
    float drain;
    int   dungeon;

    if (this->step_disable != 0) {
        return;
    }

    dungeon = this->cur_georama;
    drain = 1.0f + 0.2f * dungeon;
    drain = 0.003f * drain;

    if (this->water_drain_disable == 0 && mode == 0) {
        if (this->water_now[this->cur_chara] <= 0.0f) {
            this->water_now[this->cur_chara] = 0.0f;
        } else {
            if (this->res_limit_zone_current == RES_LIMIT_ZONE_THIRST) {
                drain = 5.0f * drain;
            }

            if (NowWeaponHave->flags & 0x8) {
                drain *= 0.8f;
            }

            if (NowWeaponHave->flags & 0x10) {
                drain *= 2.0f;
            }

            if (mode == 0) {
                this->water_now[this->cur_chara] -= drain;
            }

            if (this->water_now[this->cur_chara] <= 0.0f) {
                this->water_now[this->cur_chara] = 0.0f;
            }
        }

        if (this->water_now[this->cur_chara] <= 0.0f) {
            this->thirst_damage[this->cur_chara] += 0.008333334f;
        }

        if (this->thirst_damage[this->cur_chara] >= 1.0f) {
            this->thirst_damage[this->cur_chara] = 0.0f;
            this->AddNowLife(this->cur_chara, -1, 10.0f);
        }
    }

    for (int i = 0; i < 6; i++) {
        if (this->drink_step[i] != 0) {
            this->water_now[i] += this->drink_step[i];

            if (this->drink_step[i] < 0) {
                if (this->water_now[i] <= this->drink_next[i]) {
                    this->water_now[i] = this->drink_next[i];
                    this->drink_step[i] = 0;
                }
            }

            if (this->drink_step[i] > 0) {
                if (this->water_now[i] >= this->drink_next[i]) {
                    this->water_now[i] = this->drink_next[i];
                    this->drink_step[i] = 0;
                }
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        if (this->life_step[i] != 0) {
            this->hp[i] += this->life_step[i];

            if (this->life_step[i] < 0) {
                if (this->hp[i] <= this->next_hp[i]) {
                    this->hp[i] = this->next_hp[i];
                    this->life_step[i] = 0;
                }
            }

            if (this->life_step[i] > 0) {
                if (this->hp[i] >= this->next_hp[i]) {
                    this->hp[i] = this->next_hp[i];
                    this->life_step[i] = 0;
                }
            }
        }
    }
}

/* @ 0x1BEDE0 (0x110 bytes) -- Init__11CUserStatusFv */
void CUserStatus::Init() {
    int i;
    int valid;

    for (i = 0; (valid = i < 6) != 0; i++) {
        this->thirst_damage[i] = 0.0f;
    }

    this->water_drain_disable = 0;
    this->step_disable = 0;
    this->res_limit_zone_current = RES_LIMIT_ZONE_NONE;

    this->ClearEventFlag();

    int j;

    for (j = 0; (valid = j < 6) != 0; j++) {
        this->life_step[j] = 0;
    }

    for (j = 0; (valid = j < 6) != 0; j++) {
        this->unk_8A8C[j] = 0;
        this->unk_8A74[j] = 0;
        this->drink_step[j] = 0;
    }
}

/* @ 0x1BEEF0 (0x20 bytes) -- SetDead__14CDngStatusDataFv */
void CDngStatusData::SetDead() {
    this->money_signed = (u32) (u16) this->money_signed >> 1;
}

/* Looks up the current floor's "Res Limit Zone" id and latches it into
 * res_limit_zone_current if the floor has one assigned. */
/* @ 0x1BEF10 (0x50 bytes) -- SetResLimmitZone__14CDngStatusDataFv */
void CDngStatusData::SetResLimmitZone() {
    int zone = this->res_limit_zone_id[this->cur_georama][this->cur_floor];

    if (zone != RES_LIMIT_ZONE_NONE) {
        this->res_limit_zone_current = zone;
    }
}

/* Randomly (~50/50) assigns level-10 or level-11 restriction zones to 3-4
 * random floors per georama (georamas 1-5 only; georama 0 untouched). The
 * float divisor is RAND_MAX+1 (2^31), i.e. rand()/(RAND_MAX+1) scaled by the
 * floor count / 100 to pick a floor index / percentile roll.
 *
 * Each branch of the zone-level choice also writes `valid`; that dead store
 * is what keeps retail's real branch instead of a movn/movz conditional move,
 * and it forces the roll comparison into a real register rather than `at`. */
/* @ 0x1BEF60 (0x3E0 bytes) -- InitResLimmitZone__14CDngStatusDataFv */
void CDngStatusData::InitResLimmitZone() {
    int floor_index;
    int roll;
    int i;
    int valid;

    for (i = 0; (valid = i < 3) != 0; i++) {
        int zone;
        floor_index = (int) ((15.0f * (float) rand()) / 2147483648.0f);
        roll = (int) ((100.0f * (float) rand()) / 2147483648.0f);

        if (!(valid = roll < 50)) {
            zone = RES_LIMIT_ZONE_NO_WEAPON_CHANGE;
            valid = 0;
        } else {
            zone = RES_LIMIT_ZONE_THIRST;
            valid = 1;
        }

        this->res_limit_zone_id[1][floor_index] = zone;
    }

    for (i = 0; (valid = i < 3) != 0; i++) {
        int zone;
        floor_index = (int) ((16.0f * (float) rand()) / 2147483648.0f);
        roll = (int) ((100.0f * (float) rand()) / 2147483648.0f);

        if (!(valid = roll < 50)) {
            zone = RES_LIMIT_ZONE_NO_WEAPON_CHANGE;
            valid = 0;
        } else {
            zone = RES_LIMIT_ZONE_THIRST;
            valid = 1;
        }

        this->res_limit_zone_id[2][floor_index] = zone;
    }

    for (i = 0; (valid = i < 3) != 0; i++) {
        int zone;
        floor_index = (int) ((16.0f * (float) rand()) / 2147483648.0f);
        roll = (int) ((100.0f * (float) rand()) / 2147483648.0f);

        if (!(valid = roll < 50)) {
            zone = RES_LIMIT_ZONE_NO_WEAPON_CHANGE;
            valid = 0;
        } else {
            zone = RES_LIMIT_ZONE_THIRST;
            valid = 1;
        }

        this->res_limit_zone_id[3][floor_index] = zone;
    }

    for (i = 0; (valid = i < 4) != 0; i++) {
        int zone;
        floor_index = (int) ((14.0f * (float) rand()) / 2147483648.0f);
        roll = (int) ((100.0f * (float) rand()) / 2147483648.0f);

        if (!(valid = roll < 50)) {
            zone = RES_LIMIT_ZONE_NO_WEAPON_CHANGE;
            valid = 0;
        } else {
            zone = RES_LIMIT_ZONE_THIRST;
            valid = 1;
        }

        this->res_limit_zone_id[4][floor_index] = zone;
    }

    for (i = 0; (valid = i < 4) != 0; i++) {
        int zone;
        floor_index = (int) ((23.0f * (float) rand()) / 2147483648.0f);
        roll = (int) ((100.0f * (float) rand()) / 2147483648.0f);

        if (!(valid = roll < 50)) {
            zone = RES_LIMIT_ZONE_NO_WEAPON_CHANGE;
            valid = 0;
        } else {
            zone = RES_LIMIT_ZONE_THIRST;
            valid = 1;
        }

        this->res_limit_zone_id[5][floor_index] = zone;
    }
}

/* The five inventory fields at 0x4360..0x450A form one contiguous block that
 * Initialize's final loop addresses through a single base pointer, with the
 * two arrays reached by small (+14/+220) displacements off it rather than
 * their full struct offsets. Reproducing retail's instruction order needs the
 * base to be struct-typed: subscripting through it yields MWCC's index-first
 * `addu index, base`, whereas the equivalent raw `s8 *` plus casts yields
 * base-first. */
struct DNG_ITEM_BLOCK {
    s8   item_capacity; /**< Number of inventory slots the party may fill. */
    char unk_01;
    s16  quick_item_slot[3]; /**< Item ids held in the three quick slots. */
    s16  quick_item_qty[3];  /**< Quantity held in each quick slot. */
    s16  dungeon_items[103]; /**< Item id in each inventory slot, or -1 when empty. */
    s16  item_vol[103];      /**< How much is left in each slot's copy of its item. */
};

/* Resets the whole class to a fresh-game state: party HP/weapons/atra grid/
 * kills/restriction zones/inventory all cleared, then character 0 is given
 * the starting weapon (item 258) via GetItem. */
/* @ 0x1BF340 (0x3C0 bytes) -- Initialize__14CDngStatusDataFv */
void CDngStatusData::Initialize() {
    this->cur_georama = -1;
    this->unk_01[0] = 0;
    this->cur_floor = -1;
    this->prev_floor = -1;
    this->cur_chara = CHARA_TOAN;
    this->party_size = 1;

    s32 start_hp[6] = {70, 60, 100, 90, 110, 100};
    s32 start_stat[6] = {3, 1, 12, 23, 38, 46};

    int t;
    int q;
    int k;
    int m;
    int j;
    int i;
    int valid;

    for (t = 0; (valid = t < 6) != 0; t++) {
        this->max_hp[t] = start_hp[t];
        this->hp[t] = start_hp[t];
        this->defense[t] = start_stat[t];
        this->ailments[t] = 0;
        this->ailment_frames[t] = 0;
        this->equipped_weapon_slot[t] = -1;

        for (q = 0; (valid = q < 11) != 0; q++) {
            this->chara_weapons[t][q].item_no = -1;
        }

        this->water_max[t] = 30.0f;
        this->water_now[t] = 30.0f;
        this->skill_owned[t] = 0;
    }

    this->equipped_weapon_slot[0] = 0;
    this->GetItem(ITEM_WEAPON_DAGGER, 0);
    this->special_flag_238 = false;
    this->minimap_status = 1;
    this->overflow_flag = false;

    for (k = 0; (valid = k < 7) != 0; k++) {
        this->floor_reached[k] = -1;
    }

    for (m = 0; (valid = m < 6) != 0; m++) {
        for (k = 0; (valid = k < 100) != 0; k++) {
            this->kills[m][k] = 0;
        }
    }

    for (t = 0; (valid = t < 6) != 0; t++) {
        for (k = 0; (valid = k < 25) != 0; k++) {
            this->res_limit_zone_id[t][k] = RES_LIMIT_ZONE_NONE;
        }
    }

    this->InitResLimmitZone();

    for (t = 0; (valid = t < 6) != 0; t++) {
        for (k = 0; (valid = k < 40) != 0; k++) {
            for (j = 0; (valid = j < 8) != 0; j++) {
                this->atra_grid[t][k][j] = -1;
            }
        }
    }

    for (m = 0; (valid = m < 6) != 0; m++) {
        for (q = 0; (valid = q < 100) != 0; q++) {
            this->atra_registry[m][q].id = -1;
        }
    }

    this->money_signed = 0;
    DNG_ITEM_BLOCK *item_block = (DNG_ITEM_BLOCK *) &this->item_pack.num;
    this->item_pack.num = 50;
    this->item_pack.quick_item_slot[0] = -1;
    this->item_pack.quick_item_slot[1] = -1;
    this->item_pack.quick_item_slot[2] = -1;

    for (t = 0; (valid = t < 103) != 0; t++) {
        item_block->dungeon_items[t] = -1;
        item_block->item_vol[t] = 0;
    }

    for (i = 0; (valid = i < 43) != 0; i++) {
        this->consumable_items[i].id = -1;
    }
}

/* Per-(georama, floor) monster kill counter, indexed by the CURRENT
 * georama/floor for AddKills but by explicit params for ChkKills. */
/* @ 0x1BF700 (0x40 bytes) -- AddKills__14CDngStatusDataFv */
void CDngStatusData::AddKills() {
    this->kills[this->cur_georama][this->cur_floor]++;
}

/* @ 0x1BF740 (0x30 bytes) -- ChkKills__14CDngStatusDataFii */
s16 CDngStatusData::ChkKills(int georama_no, int floor) {
    return this->kills[georama_no][floor];
}

/* Counts already-collected (-3) atra slots on a floor. Returns 0 for
 * georama_no == 6. */
/* @ 0x1BF770 (0x80 bytes) -- GetAtraNum__14CDngStatusDataFii */
int CDngStatusData::GetAtraNum(int georama_no, int floor) {
    if (georama_no == 6) {
        return 0;
    }

    int result = 0;
    int i;
    int valid;

    for (i = 0; (valid = i < 8) != 0; i++) {
        if (this->atra_grid[georama_no][floor][i] == -3) {
            result++;
        }
    }

    return result;
}

/* Counts non-(-1) entries in atra_grid[georama_no][floor][0..8), i.e. how
 * many atra slots are assigned to this floor. Returns 0 for georama_no==6. */
/* @ 0x1BF7F0 (0x80 bytes) -- GetMaxAtraNum__14CDngStatusDataFii */
int CDngStatusData::GetMaxAtraNum(int georama_no, int floor) {
    if (georama_no == 6) {
        return 0;
    }

    int result = 0;
    int i;
    int valid;

    for (i = 0; (valid = i < 8) != 0; i++) {
        if (this->atra_grid[georama_no][floor][i] != -1) {
            result++;
        }
    }

    return result;
}

/* @ 0x1BF870 (0x80 bytes) -- SetGetAtra__14CDngStatusDataFiii */
int CDngStatusData::SetGetAtra(int georama_no, int floor, int atra_id) {
    int i;

    if (georama_no == 6) {
        i = 0;
    } else {
        int valid;

        for (i = 0; (valid = i < 8) != 0; i++) {
            if (this->atra_grid[georama_no][floor][i] == -1) {
                this->atra_grid[georama_no][floor][i] = atra_id;
                return i;
            }
        }

        i = -1;
    }

    return i;
}

/* Copies atra_grid[georama_no][floor][0..8) into out8. No-op for
 * georama_no==6. */
/* @ 0x1BF8F0 (0x60 bytes) -- SetCopyAtraList__14CDngStatusDataFiiPi */
void CDngStatusData::SetCopyAtraList(int georama_no, int floor, int *out_list) {
    if (georama_no != 6) {
        int i;
        int valid;

        for (i = 0; (valid = i < 8) != 0; i++) {
            out_list[i] = this->atra_grid[georama_no][floor][i];
        }
    }
}

/* Marks the matching (or wildcard, -2) atra slot on this floor collected (-3)
 * and drops the registry refcount for that id, freeing the entry at zero.
 * The two loops use separate counters and the second's refcount test is
 * phrased as an early return -- both are needed to reproduce retail's
 * register allocation and branch threading. */
/* @ 0x1BF950 (0x160 bytes) -- GetAtraData__14CDngStatusDataFiii */
void CDngStatusData::GetAtraData(int georama_no, int floor, int atra_id) {
    if (georama_no < 6) {
        int i;
        int valid;
        int j;

        for (i = 0; (valid = i < GetMaxAtraSlotNo()) != 0; i++) {
            if (this->atra_grid[georama_no][floor][i] == atra_id) {
                this->atra_grid[georama_no][floor][i] = -3;
                this->atra_registry[georama_no][atra_id].refcount--;

                if (this->atra_registry[georama_no][atra_id].refcount == 0) {
                    this->atra_registry[georama_no][atra_id].id = -1;
                }

                return;
            }
        }

        for (j = 0; (valid = j < GetMaxAtraSlotNo()) != 0; j++) {
            if (this->atra_grid[georama_no][floor][j] == -2) {
                this->atra_grid[georama_no][floor][j] = -3;
                this->atra_registry[georama_no][atra_id].refcount--;

                if (this->atra_registry[georama_no][atra_id].refcount != 0) {
                    return;
                }

                this->atra_registry[georama_no][atra_id].id = -1;
                return;
            }
        }
    }
}
