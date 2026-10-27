#include "savedata.hpp"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <utility>

#include "editatra.hpp"
#include "object.hpp"

namespace dc::game {

void CObject::SetPosition(float *position) {
    sceVu0CopyVector(this->pos, position);
}

namespace {

// Maps that keep their NPC slots in CSaveData::special_npc, in slot order.
constexpr std::array<s32, 6> SUB_MAPS = {14, 23, 11, 19, 42, 38};

constexpr u32 FlagMask(int flag_no) {
    return 1U << (flag_no & 0x1F);
}

} // namespace

void *CSaveData::GetConfigData() {
    return this->config;
}

CMenuCursor *CSaveData::GetMenuCursor() {
    return &this->menu_cursor;
}

s16 *CSaveData::GetCharaName(int chara_no) {
    if (chara_no < 0 || chara_no >= 6) {
        return nullptr;
    }

    return this->chara_name[chara_no];
}

void CSaveData::AddPlayTime(int time) {
    this->play_time += time;
}

int CSaveData::GetPlayTime() {
    return this->play_time;
}

int CSaveData::GetMapNo() {
    return this->map_no;
}

void CSaveData::SetMapNo(int map_no) {
    this->map_no = map_no;
}

void CSaveData::SetNowTime(float time) {
    this->now_time = time;
}

float CSaveData::GetNowTime() {
    return this->now_time;
}

void CSaveData::AddNowTime(float add) {
    const float old_time = this->now_time;
    this->now_time += add;

    if (old_time < 8.5f && this->now_time >= 8.5) {
        this->AddDay(1);
    }

    if (this->now_time >= 12.0f) {
        this->now_time -= 12.0f;
    }

    if (this->now_time < 0.0f) {
        this->now_time += 12.0f;
    }
}

void CSaveData::SetDay(int day) {
    this->day = std::clamp(day, 0, 9998);
}

int CSaveData::GetDay() {
    return this->day;
}

void CSaveData::AddDay(int add) {
    this->day += add;
    this->SetDay(this->day);
}

void CSaveData::AddFishingPoint(int add) {
    this->fishing_point = std::clamp(this->fishing_point + add, 0, 9999);
}

void CSaveData::SetFishingPoint(int value) {
    this->fishing_point = std::clamp(value, 0, 9999);
}

void CSaveData::SetFishingRank(int fish_id, float score) {
    float lowest_score = -1.0f;
    int   lowest_index = -1;
    int   i = 0;

    for (; i < 64; i++) {
        if (this->fish_data[i].rank <= 0.0f) {
            this->fish_data[i].rank = score;
            this->fish_data[i].fish_id = fish_id;
            break;
        }

        if (lowest_score < 0.0f || lowest_score > this->fish_data[i].rank) {
            lowest_index = i;
            lowest_score = this->fish_data[i].rank;
        }
    }

    if (i == 64 && lowest_index >= 0) {
        this->fish_data[lowest_index].rank = score;
        this->fish_data[lowest_index].fish_id = fish_id;
    }

    for (int j = 0; j < 63; j++) {
        for (int k = j + 1; k < 64; k++) {
            if (this->fish_data[j].rank < this->fish_data[k].rank) {
                std::swap(this->fish_data[j], this->fish_data[k]);
            }
        }
    }
}

SV_FISH_DATA *CSaveData::GetFishingRank(int rank_index) {
    if (rank_index < 0 || rank_index >= 64) {
        return nullptr;
    }

    if (this->fish_data[rank_index].rank <= 0.0f || this->fish_data[rank_index].fish_id < 0) {
        return nullptr;
    }

    return &this->fish_data[rank_index];
}

SV_GEORAMA_DATA *CSaveData::GetGrdData(int georama_no) {
    if (georama_no < 0 || georama_no >= 6) {
        return nullptr;
    }

    return &this->georama[georama_no];
}

SV_EDIT_PARTS_INFO *CSaveData::GetEditPartsInfo(int georama_no, int plot_no) {
    if (georama_no < 0 || georama_no >= 6) {
        return nullptr;
    }

    if (plot_no < 0 || plot_no >= 24) {
        return nullptr;
    }

    return &this->georama[georama_no].parts_info[plot_no];
}

SV_GRD_PART *CSaveData::GetParts(int georama_no, int *out_count) {
    if (georama_no < 0 || georama_no >= 6) {
        return nullptr;
    }

    SV_GRD_PART *parts = this->georama[georama_no].placed_parts;

    int count = 0;
    for (; count < 130; count++) {
        if (parts[count].part_id < 0) {
            break;
        }
    }

    *out_count = count;
    return parts;
}

SV_GRD_NPC *CSaveData::GetGrdNPCData(int map_no, int npc_no) {
    if (map_no >= 11) {
        for (std::size_t slot = 0; slot < SUB_MAPS.size(); slot++) {
            if (map_no == SUB_MAPS[slot]) {
                return reinterpret_cast<SV_GRD_NPC *>(&this->special_npc[slot * sizeof(SV_GRD_NPC)]);
            }
        }

        return nullptr;
    }

    if (map_no < 0 || map_no >= 6) {
        return nullptr;
    }

    if (npc_no < 0 || npc_no >= 20) {
        return nullptr;
    }

    return &this->georama[map_no].npc[npc_no];
}

s16 *CSaveData::GetElemData(int georama_no) {
    if (georama_no < 0 || georama_no >= 6) {
        return nullptr;
    }

    return this->elem_data[georama_no];
}

CDngStatusData *CSaveData::GetDngStatus() {
    return &this->dng_status;
}

CStockItem *CSaveData::GetStockItem() {
    return &this->stock_item;
}

int CSaveData::GetGameFlag(int flag_no) {
    if (flag_no < 0) {
        return 0;
    }

    const int word_idx = flag_no >> 5;

    if (word_idx >= 256) {
        return 0;
    }

    return static_cast<int>((this->game_flags[word_idx] & FlagMask(flag_no)) != 0);
}

int CSaveData::SetGameFlag(int flag_no, int value) {
    if (flag_no < 0) {
        return 0;
    }

    const int word_idx = flag_no >> 5;

    if (word_idx >= 256) {
        return 0;
    }

    const u32 mask = FlagMask(flag_no);
    this->game_flags[word_idx] &= ~mask;

    if (value != 0) {
        this->game_flags[word_idx] |= mask;
    }

    return value;
}

int CSaveData::GetGameIntFlag(int flag_no) {
    if (flag_no >= 32) {
        return 0;
    }

    return this->game_int_flag[flag_no];
}

int CSaveData::SetGameIntFlag(int flag_no, int value) {
    if (flag_no >= 32) {
        return 0;
    }

    this->game_int_flag[flag_no] = value;
    return value;
}

int CSaveData::GetMapFlag(int map_no, int flag_no) {
    if (map_no < 0 || map_no >= 80) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    const int word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    return static_cast<int>((this->map_flags[map_no][word_idx] & FlagMask(flag_no)) != 0);
}

int CSaveData::SetMapFlag(int map_no, int flag_no, int value) {
    if (map_no < 0 || map_no >= 80) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    const int word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    const u32 mask = FlagMask(flag_no);
    this->map_flags[map_no][word_idx] &= ~mask;

    if (value != 0) {
        this->map_flags[map_no][word_idx] |= mask;
    }

    return value;
}

int CSaveData::GetMapInitFlag(int map_no, int flag_no) {
    if (map_no < 0 || map_no >= 80) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    const int word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    return static_cast<int>((this->map_init_flags[map_no][word_idx] & FlagMask(flag_no)) != 0);
}

int CSaveData::SetMapInitFlag(int map_no, int flag_no, int value) {
    if (map_no < 0 || map_no >= 80) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    const int word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    const u32 mask = FlagMask(flag_no);
    this->map_init_flags[map_no][word_idx] &= ~mask;

    if (value != 0) {
        this->map_init_flags[map_no][word_idx] |= mask;
    }

    return value;
}

int CSaveData::AtraPartsGet(int georama_no, int plot_no) {
    SV_EDIT_PARTS_INFO *info = this->GetEditPartsInfo(georama_no, plot_no);

    if (info == nullptr) {
        return 0;
    }

    const EDIT_PARTS_ATRA *atra = GetEditAtraPartsData(georama_no, plot_no);

    if (atra == nullptr) {
        return 0;
    }

    if (atra->stackable > 0) {
        if (info->obtained == 0) {
            info->stock = 5;
        } else {
            info->stock += 5;

            info->stock = static_cast<s16>(std::min<s32>(info->stock, atra->max));
        }

        info->obtained = 1;
    } else {
        if (info->obtained != 0) {
            return 0;
        }

        info->obtained = 1;
    }

    return 1;
}

int CSaveData::AtraChipGet(int georama_no, int chip_id) {
    s16 *chips = this->GetElemData(georama_no);

    if (chip_id < 0) {
        return 0;
    }

    if (chips == nullptr) {
        return 0;
    }

    for (s16 &slot : std::span(chips, 128)) {
        if (slot < 0) {
            slot = static_cast<s16>(chip_id);
            return 1;
        }
    }

    return 0;
}

int CSaveData::VisitMap(int map_no, int add) {
    if (map_no < 0 || map_no >= 80) {
        return 0;
    }

    this->visit_map[map_no] += add;

    this->visit_map[map_no] = std::min<s16>(this->visit_map[map_no], 9999);

    return this->visit_map[map_no];
}

int CSaveData::QuestDungeon(int dungeon_no, int add) {
    if (dungeon_no == 6) {
        this->quest_dungeon_total += add;

        this->quest_dungeon_total = std::min(this->quest_dungeon_total, 9999);

        return this->quest_dungeon_total;
    }

    if (dungeon_no < 0 || dungeon_no >= 6) {
        return 0;
    }

    this->quest_dungeon[dungeon_no] += add;

    this->quest_dungeon[dungeon_no] = std::min<s16>(this->quest_dungeon[dungeon_no], 9999);

    return this->quest_dungeon[dungeon_no];
}

void CSaveData::ConvertConfig(SV_CONFIG_SYS *out) {
    this->config[16] = this->menu_cursor.reset_pos;
    this->config[15] = this->dng_status.minimap_status;

    *out = {};

    // Each value keeps only its low byte on the card.
    for (char *copy : {out->values, out->values_copy1, out->values_copy2}) {
        for (int value_no = 0; value_no < 18; value_no++) {
            copy[value_no] = static_cast<char>(this->config[value_no]);
        }
    }
}

int CSaveData::InvertConfig(SV_CONFIG_SYS *in) {
    for (int value_no = 0; value_no < 18; value_no++) {
        if (in->values[value_no] != in->values_copy1[value_no]) {
            return 0;
        }

        if (in->values[value_no] != in->values_copy2[value_no]) {
            return 0;
        }
    }

    for (int value_no = 0; value_no < 18; value_no++) {
        this->config[value_no] = in->values[value_no];
    }

    this->menu_cursor.reset_pos = static_cast<s16>(this->config[16]);

    if (this->menu_cursor.reset_pos != 0) {
        this->menu_cursor.InitPos();
    }

    this->dng_status.minimap_status = this->config[15];

    return 1;
}

void CMenuCursor::InitPos() {
    std::ranges::fill(this->pos, 0);
    std::ranges::fill(this->mode, -1);

    this->mode[0] = 4;
    this->mode[2] = 6;
    this->mode[3] = 0;
    this->chara_no = 0;
}

void CSaveData::Initialize() {
    std::memset(static_cast<void *>(this), 0, sizeof(CSaveData));
    this->unk_1CC = -1;
    this->now_time = 0.0f;

    for (auto &town_chips : this->elem_data) {
        std::ranges::fill(town_chips, -1);
    }

    for (SV_GEORAMA_DATA &town : this->georama) {
        for (SV_GRD_PART &part : town.placed_parts) {
            part.part_id = -1;
        }
    }

    this->dng_status.Initialize();
    this->stock_item.Initialize();

    std::ranges::fill(this->config, 0);
    std::ranges::fill(this->game_flags, 0U);
    this->map_flags[0][0] = 0;
}

CSaveData::CSaveData() {
    this->menu_cursor.reset_pos = 0;
    this->menu_cursor.InitPos();
    this->Initialize();
}

} // namespace dc::game
