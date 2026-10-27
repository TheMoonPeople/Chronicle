#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "itemdata.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "userstatus.hpp"

namespace dc::game {

namespace {

constexpr std::array<s32, 6> defWeapon = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN,
                                          ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN,
                                          ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};

constexpr int MAX_DUNGEON_ITEMS = 100;
constexpr int INVENTORY_SLOTS = 103;
constexpr int QUICK_SLOTS = 3;
constexpr int CONSUMABLE_CAPACITY = 40;
constexpr int CONSUMABLE_SLOTS = 43;
constexpr int WEAPON_CAPACITY = 10;
constexpr int WEAPON_SLOTS = 11;
constexpr int DEAMON_SHAFT = 6;

constexpr s32 ATRA_SLOT_EMPTY = -1;
constexpr s32 ATRA_SLOT_ANY = -2;
constexpr s32 ATRA_SLOT_COLLECTED = -3;

constexpr bool IsDungeonItem(int item_id) {
    return item_id >= ITEM_DUNGEON_START && item_id < ITEM_WEAPON_START;
}

constexpr bool IsConsumable(int item_id) {
    return item_id >= ITEM_ATTACH_START && item_id < ITEM_DUNGEON_START;
}

} // namespace

void CDngStatusData::SetNowFloor(int floor) {
    this->prev_floor = this->cur_floor;
    this->cur_floor = static_cast<s8>(floor);

    s8 &reached = this->floor_reached[this->cur_georama];

    if (floor > reached) {
        reached = static_cast<s8>(floor);
    }
}

int CDngStatusData::SearchItemIndexNo(int item_id) {
    if (IsDungeonItem(item_id)) {
        for (int slot = 0; slot < QUICK_SLOTS; slot++) {
            if (this->item_pack.quick_item_slot[slot] == item_id) {
                return slot;
            }
        }

        for (int slot = 0; slot < this->item_pack.num; slot++) {
            if (this->item_pack.item[slot] == item_id) {
                return slot;
            }
        }
    }

    if (IsConsumable(item_id)) {
        for (int slot = 0; slot < CONSUMABLE_CAPACITY; slot++) {
            if (this->consumable_items[slot].id == item_id) {
                return slot;
            }
        }
    }

    if (item_id >= ITEM_WEAPON_START) {
        for (int chara_no = 0; chara_no < this->party_size; chara_no++) {
            for (int slot = 0; slot < WEAPON_CAPACITY; slot++) {
                if (this->chara_weapons[chara_no][slot].item_no == item_id) {
                    return slot;
                }
            }
        }
    }

    return -1;
}

// flag_no is not range-checked.
int CUserStatus::ChkEventFlag(int flag_no) {
    return this->event_flags[flag_no];
}

void CUserStatus::ClearEventFlag() {
    std::ranges::fill(this->event_flags, 0);
}

int CDngStatusData::LostItem(int item_id) {
    for (int slot = 0; slot < INVENTORY_SLOTS; slot++) {
        if (this->item_pack.item[slot] == item_id) {
            this->item_pack.item[slot] = -1;
            this->item_pack.item_vol[slot] = 0;
            return slot;
        }
    }

    return -1;
}

void CDngStatusData::LostGateKey() {
    constexpr std::array<int, 9> gate_keys = {
        ITEM_DRAN_S_CREST,
        ITEM_SHINY_STONE,
        ITEM_RED_BERRY,
        ITEM_HOOK,
        ITEM_KING_S_SLATE,
        ITEM_GUN_POWDER,
        ITEM_CLOCK_HANDS,
        ITEM_POINTY_CHESTNUT,
        ITEM_BLACK_KNIGHT_CREST,
    };

    for (s16 &item : this->item_pack.item) {
        const bool is_key = item >= ITEM_BONE_KEY && item <= ITEM_SILVER_KEY;

        if (is_key || std::ranges::find(gate_keys, item) != gate_keys.end()) {
            item = -1;
        }
    }
}

int CDngStatusData::GetLiveUnit() {
    int live_count = 0;

    for (int chara_no = 0; chara_no < this->party_size; chara_no++) {
        if (this->hp[chara_no] > 0) {
            live_count++;
        }
    }

    return live_count;
}

int CDngStatusData::CheckItemGet(int item_id) {
    if (static_cast<u32>(item_id - ITEM_MAP) <= 1 || item_id == ITEM_WORLDMAP) {
        return 0;
    }

    if (item_id >= GetDungeonItemStart() && item_id < GetDungeonItemEnd()) {
        int held_count = 0;

        for (int slot = 0; slot < MAX_DUNGEON_ITEMS; slot++) {
            if (IsDungeonItem(this->item_pack.item[slot])) {
                held_count++;
            }
        }

        for (int slot = 0; slot < QUICK_SLOTS; slot++) {
            if (this->item_pack.quick_item_slot[slot] != -1) {
                held_count += this->item_pack.quick_item_qty[slot];
            }
        }

        if (held_count >= this->item_pack.num) {
            return 1;
        }
    }

    if (IsConsumable(item_id)) {
        int held_count = 0;

        for (int slot = 0; slot < CONSUMABLE_CAPACITY; slot++) {
            if (IsConsumable(this->consumable_items[slot].id)) {
                held_count++;
            }
        }

        if (held_count >= CONSUMABLE_CAPACITY) {
            return 2;
        }
    }

    return 0;
}

int CDngStatusData::CheckWeaponUser(int weapon_id) {
    int chara_no = -1;

    if (weapon_id >= ITEM_WEAPON_DAGGER_BROKEN && weapon_id < ITEM_WEAPON_WOODENSLINGSHOT_BROKEN) {
        chara_no = CHARA_TOAN;
    }

    if (weapon_id >= ITEM_WEAPON_WOODENSLINGSHOT_BROKEN && weapon_id < ITEM_WEAPON_MALLET_BROKEN) {
        chara_no = CHARA_XIAO;
    }

    if (weapon_id >= ITEM_WEAPON_MALLET_BROKEN && weapon_id < ITEM_WEAPON_GOLD_RING_BROKEN) {
        chara_no = CHARA_GORO;
    }

    if (weapon_id >= ITEM_WEAPON_GOLD_RING_BROKEN && weapon_id < ITEM_WEAPON_FIGHTING_STICK_BROKEN) {
        chara_no = CHARA_RUBY;
    }

    if (weapon_id >= ITEM_WEAPON_FIGHTING_STICK_BROKEN && weapon_id < ITEM_WEAPON_MACHINE_GUN_BROKEN) {
        chara_no = CHARA_UNGAGA;
    }

    if (weapon_id >= ITEM_WEAPON_MACHINE_GUN_BROKEN && weapon_id < ITEM_WEAPON_END) {
        chara_no = CHARA_OSMOND;
    }

    return chara_no;
}

int CDngStatusData::CheckWeaponRot(int weapon_id) {
    const int chara_no = this->CheckWeaponUser(weapon_id);

    if (chara_no == -1 || weapon_id < ITEM_WEAPON_START) {
        return -1;
    }

    int weapon_count = 0;

    for (int slot = 0; slot < WEAPON_CAPACITY; slot++) {
        if (this->chara_weapons[chara_no][slot].item_no >= ITEM_WEAPON_START) {
            weapon_count++;
        }
    }

    return weapon_count;
}

void CDngStatusData::ClearDeamonShaft() {
    this->floor_reached[DEAMON_SHAFT] = -1;
}

// The inventory and consumable arrays hold three slots past their capacity, filled only while
// overflow_flag is set.
int CDngStatusData::GetItem(int item_id, int qty) {
    printf("GetITEM No === %d\n", item_id);

    if (IsDungeonItem(item_id)) {
        for (int slot = 0; slot < INVENTORY_SLOTS; slot++) {
            if (this->item_pack.item[slot] >= ITEM_DUNGEON_START) {
                continue;
            }

            if (item_id == ITEM_WORLDMAP) {
                this->special_flag_238 = true;
                return 0;
            }

            int held_count = static_cast<int>(
                std::ranges::count_if(this->item_pack.item, [](s16 item) { return item >= ITEM_DUNGEON_START; }));

            for (int quick_slot = 0; quick_slot < QUICK_SLOTS; quick_slot++) {
                if (this->item_pack.quick_item_slot[quick_slot] != -1) {
                    held_count += this->item_pack.quick_item_qty[quick_slot];
                }
            }

            if (held_count + 1 > this->item_pack.num) {
                this->overflow_flag = true;
            }

            const int have_copy = ItemDataToHaveCopy(item_id);

            if (this->overflow_flag != 0) {
                for (int extra = 0; extra < QUICK_SLOTS; extra++) {
                    const int extra_slot = this->item_pack.num + extra;

                    if (this->item_pack.item[extra_slot] == -1) {
                        this->item_pack.item[extra_slot] = static_cast<s16>(item_id);
                        this->item_pack.item_vol[extra_slot] = static_cast<s16>(have_copy);
                        return slot;
                    }
                }
            } else {
                this->item_pack.item[slot] = static_cast<s16>(item_id);
                this->item_pack.item_vol[slot] = static_cast<s16>(have_copy);
            }

            return slot;
        }
    }

    if (IsConsumable(item_id)) {
        for (int slot = 0; slot < CONSUMABLE_SLOTS; slot++) {
            if (this->consumable_items[slot].id >= ITEM_ATTACH_START) {
                continue;
            }

            this->consumable_items[slot].id = static_cast<s16>(item_id);
            SetAttachMentValue(item_id, slot, static_cast<short>(qty), nullptr);

            const auto held_count = std::ranges::count_if(
                this->consumable_items, [](const DNG_CONSUMABLE &consumable) { return IsConsumable(consumable.id); });

            if (held_count > CONSUMABLE_CAPACITY) {
                this->overflow_flag++;
            }

            return slot;
        }
    }

    if (item_id >= ITEM_WEAPON_START) {
        printf("get weapon!! %d\n", item_id);
        const int chara_no = static_cast<s8>(WeaponList[item_id - ITEM_WEAPON_START].owner);

        for (int slot = 0; slot < WEAPON_SLOTS; slot++) {
            if (this->chara_weapons[chara_no][slot].item_no >= ITEM_WEAPON_START) {
                continue;
            }

            WepDataListToHaveCopy(item_id, &this->chara_weapons[chara_no][slot]);

            int weapon_count = 0;

            for (int held_slot = 0; held_slot < WEAPON_CAPACITY; held_slot++) {
                if (this->chara_weapons[chara_no][held_slot].item_no >= ITEM_WEAPON_START) {
                    weapon_count++;
                }
            }

            if (weapon_count >= WEAPON_CAPACITY) {
                this->overflow_flag = true;
            }

            return slot;
        }
    }

    return -1;
}

int CDngStatusData::CheckActItemSlot(int item_id) {
    for (int slot = 0; slot < QUICK_SLOTS; slot++) {
        if (this->item_pack.quick_item_slot[slot] == item_id) {
            return slot;
        }
    }

    return -1;
}

int CDngStatusData::CheckDefaultWeapon(int chara_no) {
    const s32 default_weapon = defWeapon[chara_no];

    if (default_weapon == this->chara_weapons[chara_no][this->equipped_weapon_slot[chara_no]].item_no) {
        return 0;
    }

    return 1;
}

// A step that rounds to zero still moves the gauge one unit towards its target.
void CUserStatus::AddDrink(int chara_no, s16 amount, float ratio) {
    if (this->drink_step[chara_no] != 0) {
        this->water_now[chara_no] = static_cast<float>(this->drink_next[chara_no]);
        this->drink_step[chara_no] = 0;
    }

    if (0.0f == ratio) {
        this->water_now[chara_no] += static_cast<float>(amount);

        if (this->water_now[chara_no] >= this->water_max[chara_no]) {
            this->water_now[chara_no] = this->water_max[chara_no];
        }

        return;
    }

    this->drink_next[chara_no] = static_cast<s16>(this->water_now[chara_no] + static_cast<float>(amount));

    if (this->drink_next[chara_no] <= 0) {
        this->drink_next[chara_no] = 0;
    }

    if (static_cast<float>(this->drink_next[chara_no]) >= this->water_max[chara_no]) {
        this->drink_next[chara_no] = static_cast<s16>(this->water_max[chara_no]);
    }

    const float remaining = static_cast<float>(this->drink_next[chara_no]) - this->water_now[chara_no];
    this->drink_step[chara_no] = static_cast<s16>(ratio * (remaining / 100.0f));

    if (this->drink_step[chara_no] == 0) {
        this->drink_step[chara_no] = remaining < 0.0f ? -1 : 1;
    }
}

void CUserStatus::AddNowLife(int chara_no, s16 amount, float ratio) {
    if (this->life_step[chara_no] != 0) {
        this->hp[chara_no] = this->next_hp[chara_no];
        this->life_step[chara_no] = 0;
    }

    if (0.0f == ratio) {
        this->hp[chara_no] = static_cast<s16>(this->hp[chara_no] + amount);

        if (this->hp[chara_no] <= 0) {
            this->hp[chara_no] = 0;
        }

        if (this->hp[chara_no] >= this->max_hp[chara_no]) {
            this->hp[chara_no] = this->max_hp[chara_no];
        }

        return;
    }

    this->next_hp[chara_no] = static_cast<s16>(this->hp[chara_no] + amount);

    if (this->next_hp[chara_no] <= 0) {
        this->next_hp[chara_no] = 0;
    }

    if (this->next_hp[chara_no] >= this->max_hp[chara_no]) {
        this->next_hp[chara_no] = this->max_hp[chara_no];
    }

    const int remaining = this->next_hp[chara_no] - this->hp[chara_no];
    this->life_step[chara_no] = static_cast<s16>(ratio * (static_cast<float>(remaining) / 100.0f));

    if (this->life_step[chara_no] == 0) {
        this->life_step[chara_no] = remaining < 0 ? -1 : 1;
    }
}

int CUserStatus::CheckLife() {
    if (this->life_step[this->cur_chara] != 0 && this->next_hp[this->cur_chara] <= 0) {
        return 0;
    }

    if (this->hp[this->cur_chara] <= 0) {
        return 0;
    }

    return 1;
}

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
        return;
    }

    this->next_hp[chara_no] = value;

    const int remaining = this->next_hp[chara_no] - this->hp[chara_no];
    this->life_step[chara_no] = static_cast<s16>(ratio * (static_cast<float>(remaining) / 100.0f));

    if (this->life_step[chara_no] == 0) {
        this->life_step[chara_no] = remaining < 0 ? -1 : 1;
    }
}

void CUserStatus::Step(int mode) {
    if (this->step_disable != 0) {
        return;
    }

    float drain = 1.0f + 0.2f * static_cast<float>(this->cur_georama);
    drain = 0.003f * drain;

    if (this->water_drain_disable == 0 && mode == 0) {
        float &water = this->water_now[this->cur_chara];

        if (water <= 0.0f) {
            water = 0.0f;
        } else {
            if (this->res_limit_zone_current == RES_LIMIT_ZONE_THIRST) {
                drain = 5.0f * drain;
            }

            if ((NowWeaponHave->flags & WEAPONFLAG_QUENCH) != 0) {
                drain *= 0.8f;
            }

            if ((NowWeaponHave->flags & WEAPONFLAG_THIRST) != 0) {
                drain *= 2.0f;
            }

            water -= drain;

            if (water <= 0.0f) {
                water = 0.0f;
            }
        }

        // One point of life per 120 frames spent with an empty gauge.
        if (water <= 0.0f) {
            this->thirst_damage[this->cur_chara] += 0.008333334f;
        }

        if (this->thirst_damage[this->cur_chara] >= 1.0f) {
            this->thirst_damage[this->cur_chara] = 0.0f;
            this->AddNowLife(this->cur_chara, -1, 10.0f);
        }
    }

    for (int chara_no = 0; chara_no < 6; chara_no++) {
        if (this->drink_step[chara_no] != 0) {
            this->water_now[chara_no] += this->drink_step[chara_no];

            if (this->drink_step[chara_no] < 0 && this->water_now[chara_no] <= this->drink_next[chara_no]) {
                this->water_now[chara_no] = this->drink_next[chara_no];
                this->drink_step[chara_no] = 0;
            }

            if (this->drink_step[chara_no] > 0 && this->water_now[chara_no] >= this->drink_next[chara_no]) {
                this->water_now[chara_no] = this->drink_next[chara_no];
                this->drink_step[chara_no] = 0;
            }
        }
    }

    for (int chara_no = 0; chara_no < 6; chara_no++) {
        if (this->life_step[chara_no] != 0) {
            this->hp[chara_no] += this->life_step[chara_no];

            if (this->life_step[chara_no] < 0 && this->hp[chara_no] <= this->next_hp[chara_no]) {
                this->hp[chara_no] = this->next_hp[chara_no];
                this->life_step[chara_no] = 0;
            }

            if (this->life_step[chara_no] > 0 && this->hp[chara_no] >= this->next_hp[chara_no]) {
                this->hp[chara_no] = this->next_hp[chara_no];
                this->life_step[chara_no] = 0;
            }
        }
    }
}

void CUserStatus::Init() {
    std::ranges::fill(this->thirst_damage, 0.0f);

    this->water_drain_disable = 0;
    this->step_disable = 0;
    this->res_limit_zone_current = RES_LIMIT_ZONE_NONE;

    this->ClearEventFlag();

    std::ranges::fill(this->life_step, 0);
    std::ranges::fill(this->unk_8A8C, 0);
    std::ranges::fill(this->unk_8A74, 0);
    std::ranges::fill(this->drink_step, 0);
}

void CDngStatusData::SetDead() {
    this->money_signed = static_cast<s16>(static_cast<u32>(static_cast<u16>(this->money_signed)) >> 1);
}

void CDngStatusData::SetResLimmitZone() {
    const int zone = this->res_limit_zone_id[this->cur_georama][this->cur_floor];

    if (zone != RES_LIMIT_ZONE_NONE) {
        this->res_limit_zone_current = zone;
    }
}

void CDngStatusData::InitResLimmitZone() {
    // rand() / 2^31 is a uniform fraction in [0, 1).
    const auto assign_zones = [this](int georama_no, float floor_count, int zone_count) {
        for (int zone_no = 0; zone_no < zone_count; zone_no++) {
            const int floor_index = static_cast<int>((floor_count * static_cast<float>(rand())) / 2147483648.0f);
            const int roll = static_cast<int>((100.0f * static_cast<float>(rand())) / 2147483648.0f);

            this->res_limit_zone_id[georama_no][floor_index] =
                static_cast<char>(roll < 50 ? RES_LIMIT_ZONE_THIRST : RES_LIMIT_ZONE_NO_WEAPON_CHANGE);
        }
    };

    assign_zones(1, 15.0f, 3);
    assign_zones(2, 16.0f, 3);
    assign_zones(3, 16.0f, 3);
    assign_zones(4, 14.0f, 4);
    assign_zones(5, 23.0f, 4);
}

void CDngStatusData::Initialize() {
    constexpr std::array<s32, 6> start_hp = {70, 60, 100, 90, 110, 100};
    constexpr std::array<s32, 6> start_defense = {3, 1, 12, 23, 38, 46};

    this->cur_georama = -1;
    this->unk_01[0] = 0;
    this->cur_floor = -1;
    this->prev_floor = -1;
    this->cur_chara = CHARA_TOAN;
    this->party_size = 1;

    for (int chara_no = 0; chara_no < 6; chara_no++) {
        this->max_hp[chara_no] = static_cast<s16>(start_hp[chara_no]);
        this->hp[chara_no] = static_cast<s16>(start_hp[chara_no]);
        this->defense[chara_no] = start_defense[chara_no];
        this->ailments[chara_no] = 0;
        this->ailment_frames[chara_no] = 0;
        this->equipped_weapon_slot[chara_no] = -1;

        for (WEAPON_HAVE &weapon : this->chara_weapons[chara_no]) {
            weapon.item_no = -1;
        }

        this->water_max[chara_no] = 30.0f;
        this->water_now[chara_no] = 30.0f;
        this->skill_owned[chara_no] = 0;
    }

    this->equipped_weapon_slot[0] = 0;
    this->GetItem(ITEM_WEAPON_DAGGER, 0);
    this->special_flag_238 = false;
    this->minimap_status = 1;
    this->overflow_flag = false;

    std::ranges::fill(this->floor_reached, -1);

    for (auto &dungeon_kills : this->kills) {
        std::ranges::fill(dungeon_kills, 0);
    }

    for (auto &dungeon_zones : this->res_limit_zone_id) {
        std::ranges::fill(dungeon_zones, static_cast<char>(RES_LIMIT_ZONE_NONE));
    }

    this->InitResLimmitZone();

    for (auto &dungeon_grid : this->atra_grid) {
        for (auto &floor_slots : dungeon_grid) {
            std::ranges::fill(floor_slots, ATRA_SLOT_EMPTY);
        }
    }

    for (auto &dungeon_registry : this->atra_registry) {
        for (ATRA_SAVE &entry : dungeon_registry) {
            entry.id = -1;
        }
    }

    this->money_signed = 0;
    this->item_pack.num = 50;
    std::ranges::fill(this->item_pack.quick_item_slot, -1);
    std::ranges::fill(this->item_pack.item, -1);
    std::ranges::fill(this->item_pack.item_vol, 0);

    for (DNG_CONSUMABLE &consumable : this->consumable_items) {
        consumable.id = -1;
    }
}

void CDngStatusData::AddKills() {
    this->kills[this->cur_georama][this->cur_floor]++;
}

s16 CDngStatusData::ChkKills(int georama_no, int floor) {
    return this->kills[georama_no][floor];
}

int CDngStatusData::GetAtraNum(int georama_no, int floor) {
    if (georama_no == DEAMON_SHAFT) {
        return 0;
    }

    return static_cast<int>(std::ranges::count(this->atra_grid[georama_no][floor], ATRA_SLOT_COLLECTED));
}

int CDngStatusData::GetMaxAtraNum(int georama_no, int floor) {
    if (georama_no == DEAMON_SHAFT) {
        return 0;
    }

    return static_cast<int>(std::ranges::count_if(this->atra_grid[georama_no][floor],
                                                  [](s32 slot) { return slot != ATRA_SLOT_EMPTY; }));
}

int CDngStatusData::SetGetAtra(int georama_no, int floor, int atra_id) {
    if (georama_no == DEAMON_SHAFT) {
        return 0;
    }

    auto &floor_slots = this->atra_grid[georama_no][floor];

    auto *const free_slot = std::ranges::find(floor_slots, ATRA_SLOT_EMPTY);

    if (free_slot == std::end(floor_slots)) {
        return -1;
    }

    *free_slot = atra_id;
    return static_cast<int>(std::distance(std::begin(floor_slots), free_slot));
}

void CDngStatusData::SetCopyAtraList(int georama_no, int floor, int *out_list) {
    if (georama_no != DEAMON_SHAFT) {
        std::ranges::copy(this->atra_grid[georama_no][floor], out_list);
    }
}

void CDngStatusData::GetAtraData(int georama_no, int floor, int atra_id) {
    if (georama_no >= DEAMON_SHAFT) {
        return;
    }

    auto      &floor_slots = this->atra_grid[georama_no][floor];
    const auto collect = [&](s32 &slot) {
        slot = ATRA_SLOT_COLLECTED;

        ATRA_SAVE &entry = this->atra_registry[georama_no][atra_id];
        entry.refcount--;

        if (entry.refcount == 0) {
            entry.id = -1;
        }
    };

    for (s32 &slot : floor_slots) {
        if (slot == atra_id) {
            collect(slot);
            return;
        }
    }

    for (s32 &slot : floor_slots) {
        if (slot == ATRA_SLOT_ANY) {
            collect(slot);
            return;
        }
    }
}

} // namespace dc::game
