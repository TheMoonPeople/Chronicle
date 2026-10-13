#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000

#include "savedata.hpp"

#include <libvu0.h>

#include "editatra.hpp"
#include "object.hpp"

#ifdef MOD_DEMON_SHAFT_GEORAMA
#include <cstdlib>
#include <cstring>

#include "dsgeorama.hpp"
#include "editpartsdata.hpp"
#include "mainselect.hpp"
#endif

void CObject::SetPosition(float *position) {
    sceVu0CopyVector(this->pos, position);
}

/**
 * Contains one entry of the Georama building definition table.
 *
 * Only the two fields that AtraPartsGet() reads are known.
 */
struct SV_ATRA_PARTS_DEF {
    s32 max_stock; /**< Most copies of the building the player can hold. */
    s32 stackable; /**< Greater than zero where each pickup adds five copies up to max_stock. */
};

/**
 * Returns the definition table entry of a Georama plot.
 *
 * @mangled GetEditAtraPartsData__Fii
 * @address 0x158E00
 * @size 0x74
 */

void *CSaveData::GetConfigData() {
    return this->config;
}

CMenuCursor *CSaveData::GetMenuCursor() {
    return &this->menu_cursor;
}

s16 *CSaveData::GetCharaName(int chara_no) {
    if (chara_no < 0 || chara_no >= 6) {
        return NULL;
    }

    return this->chara_name[chara_no];
}

void CSaveData::AddPlayTime(int time) {
    this->play_time += time;
}

int CSaveData::GetPlayTime() {
    return this->play_time;
}

void CSaveData::SetNowTime(float time) {
    this->now_time = time;
}

float CSaveData::GetNowTime() {
    return this->now_time;
}

void CSaveData::AddNowTime(float add) {
    float old_time = this->now_time;
    this->now_time += add;

    if (old_time < 8.5f) {
        if (this->now_time >= 8.5) {
            this->AddDay(1);
        }
    }

    if (this->now_time >= 12.0f) {
        this->now_time -= 12.0f;
    }

    if (this->now_time < 0.0f) {
        this->now_time += 12.0f;
    }
}

void CSaveData::SetDay(int day) {
    this->day = day;

    if (this->day > 9998) {
        this->day = 9998;
    }

    if (this->day < 0) {
        this->day = 0;
    }
}

int CSaveData::GetDay() {
    return this->day;
}

void CSaveData::AddDay(int add) {
    this->day += add;
    this->SetDay(this->day);
}

void CSaveData::AddFishingPoint(int add) {
    this->fishing_point += add;

    if (this->fishing_point > 0x270F) {
        this->fishing_point = 0x270F;
    }

    if (this->fishing_point < 0) {
        this->fishing_point = 0;
    }
}

void CSaveData::SetFishingPoint(int value) {
    this->fishing_point = value;

    if (this->fishing_point > 0x270F) {
        this->fishing_point = 0x270F;
    }

    if (this->fishing_point < 0) {
        this->fishing_point = 0;
    }
}

void CSaveData::SetFishingRank(int fish_id, float score) {
    float lowest_score = -1.0f;
    int   lowest_index = -1;
    int   i;

    for (i = 0; i < 64; i++) {
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

    for (i = 0; i < 63; i++) {
        for (int k = i + 1; k < 64; k++) {
            if (this->fish_data[i].rank < this->fish_data[k].rank) {
                SV_FISH_DATA swapped = this->fish_data[i];
                this->fish_data[i] = this->fish_data[k];
                this->fish_data[k] = swapped;
            }
        }
    }
}

SV_FISH_DATA *CSaveData::GetFishingRank(int rank_index) {
    if (rank_index < 0 || rank_index >= 64) {
        return NULL;
    }

    if (this->fish_data[rank_index].rank <= 0.0f || this->fish_data[rank_index].fish_id < 0) {
        return NULL;
    }

    return &this->fish_data[rank_index];
}

SV_GEORAMA_DATA *CSaveData::GetGrdData(int georama_no) {
#ifdef MOD_DEMON_SHAFT_GEORAMA
    georama_no = DsgSaveIndex(georama_no);

    if (georama_no == DSG_GEORAMA_NO) {
        return &DsgSave.georama;
    }

#endif
    if (georama_no < 0 || georama_no >= 6) {
        return NULL;
    }

    return &this->georama[georama_no];
}

SV_EDIT_PARTS_INFO *CSaveData::GetEditPartsInfo(int georama_no, int plot_no) {
#ifdef MOD_DEMON_SHAFT_GEORAMA
    georama_no = DsgSaveIndex(georama_no);

    if (georama_no == DSG_GEORAMA_NO) {
        return plot_no < 0 || plot_no >= 24 ? NULL : &DsgSave.georama.parts_info[plot_no];
    }

#endif
    if (georama_no < 0 || georama_no >= 6) {
        return NULL;
    }

    if (plot_no < 0 || plot_no >= 24) {
        return NULL;
    }

    return &this->georama[georama_no].parts_info[plot_no];
}

SV_GRD_PART *CSaveData::GetParts(int georama_no, int *out_count) {
    SV_GRD_PART *parts;
    int          count;

#ifdef MOD_DEMON_SHAFT_GEORAMA
    georama_no = DsgSaveIndex(georama_no);

    if (georama_no == DSG_GEORAMA_NO) {
        parts = DsgSave.georama.placed_parts;
    } else
#endif
    {
        if (georama_no < 0 || georama_no >= 6) {
            return NULL;
        }

        parts = this->georama[georama_no].placed_parts;
    }

    for (count = 0; count < 130; count++) {
        if (parts[count].part_id < 0) {
            break;
        }
    }

    *out_count = count;
    return parts;
}

SV_GRD_NPC *CSaveData::GetGrdNPCData(int map_no, int npc_no) {
    static s32 sub_map[] = {14, 23, 11, 19, 42, 38, -1};
    int        i;

    if (map_no >= 11) {
        for (i = 0;; i++) {
            if (sub_map[i] < 0) {
                return NULL;
            }

            if (map_no == sub_map[i]) {
                return (SV_GRD_NPC *) (this->special_npc + i * 12);
            }
        }
    }

#ifdef MOD_DEMON_SHAFT_GEORAMA
    if (DsgSaveIndex(map_no) == DSG_GEORAMA_NO) {
        return npc_no < 0 || npc_no >= 20 ? NULL : &DsgSave.georama.npc[npc_no];
    }

#endif
    if (map_no < 0 || map_no >= 6) {
        return NULL;
    }

    if (npc_no < 0 || npc_no >= 20) {
        return NULL;
    }

    return &this->georama[map_no].npc[npc_no];
}

s16 *CSaveData::GetElemData(int georama_no) {
#ifdef MOD_DEMON_SHAFT_GEORAMA
    georama_no = DsgSaveIndex(georama_no);

    if (georama_no == DSG_GEORAMA_NO) {
        return DsgSave.elem_data;
    }

#endif
    if (georama_no < 0 || georama_no >= 6) {
        return NULL;
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
    int bit_idx;

    if (flag_no < 0) {
        return 0;
    }

    if (flag_no >> 5 >= 256) {
        return 0;
    }

    bit_idx = flag_no & 0x1F;

    if (flag_no < 0 && bit_idx != 0) {
        bit_idx -= 32;
    }

    u32 mask = 1 << bit_idx;
    return (this->game_flags[flag_no >> 5] & mask) != 0;
}

int CSaveData::SetGameFlag(int flag_no, int value) {
    int bit_idx;
    int word_idx;
    u32 mask;

    if (flag_no < 0) {
        return 0;
    }

    word_idx = flag_no >> 5;

    if (word_idx >= 256) {
        return 0;
    }

    bit_idx = flag_no & 0x1F;

    if (flag_no < 0 && bit_idx != 0) {
        bit_idx -= 32;
    }

    mask = 1 << bit_idx;
    this->game_flags[word_idx] &= ~mask;

    if (value) {
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
    int word_idx;
    int bit_idx;
    u32 mask;

    if (map_no < 0 || map_no >= 0x50) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    bit_idx = flag_no & 0x1F;

    if (flag_no < 0 && bit_idx != 0) {
        bit_idx -= 32;
    }

    mask = 1 << bit_idx;
    return (this->map_flags[map_no][word_idx] & mask) != 0;
}

int CSaveData::SetMapFlag(int map_no, int flag_no, int value) {
    int word_idx;
    int bit_idx;
    u32 mask;

    if (map_no < 0 || map_no >= 0x50) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    bit_idx = flag_no & 0x1F;

    if (flag_no < 0 && bit_idx != 0) {
        bit_idx -= 32;
    }

    mask = 1 << bit_idx;
    this->map_flags[map_no][word_idx] &= ~mask;

    if (value) {
        this->map_flags[map_no][word_idx] |= mask;
    }

    return value;
}

int CSaveData::GetMapInitFlag(int map_no, int flag_no) {
    int word_idx;
    int bit_idx;
    u32 mask;

    if (map_no < 0 || map_no >= 0x50) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    bit_idx = flag_no & 0x1F;

    if (flag_no < 0 && bit_idx != 0) {
        bit_idx -= 32;
    }

    mask = 1 << bit_idx;
    return (this->map_init_flags[map_no][word_idx] & mask) != 0;
}

int CSaveData::SetMapInitFlag(int map_no, int flag_no, int value) {
    int word_idx;
    int bit_idx;
    u32 mask;

    if (map_no < 0 || map_no >= 0x50) {
        return 0;
    }

    if (flag_no < 0) {
        return 0;
    }

    word_idx = flag_no >> 5;

    if (word_idx >= 16) {
        return 0;
    }

    bit_idx = flag_no & 0x1F;

    if (flag_no < 0 && bit_idx != 0) {
        bit_idx -= 32;
    }

    mask = 1 << bit_idx;
    this->map_init_flags[map_no][word_idx] &= ~mask;

    if (value) {
        this->map_init_flags[map_no][word_idx] |= mask;
    }

    return value;
}

int CSaveData::AtraPartsGet(int georama_no, int plot_no) {
    SV_EDIT_PARTS_INFO *info;
    SV_ATRA_PARTS_DEF  *atra;

    info = this->GetEditPartsInfo(georama_no, plot_no);

    if (info == NULL) {
        return 0;
    }

    atra = (SV_ATRA_PARTS_DEF *) GetEditAtraPartsData(georama_no, plot_no);

    if (atra == NULL) {
        return 0;
    }

    if (atra->stackable > 0) {
        if (info->obtained == 0) {
            info->stock = 5;
        } else {
            info->stock += 5;

            if (info->stock > atra->max_stock) {
                info->stock = (s16) atra->max_stock;
            }
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
    s16 *chips;
    s16 *slot;
    int  i;

    chips = this->GetElemData(georama_no);

    if (chip_id < 0) {
        return 0;
    }

    if (chips == NULL) {
        return 0;
    }

    slot = chips;

    for (i = 0; i < 128; i++, slot++) {
        if (*slot < 0) {
            *slot = chip_id;
            return 1;
        }
    }

    return 0;
}

int CSaveData::VisitMap(int map_no, int add) {
    if (map_no < 0 || map_no >= 0x50) {
        return 0;
    }

    this->visit_map[map_no] += add;

    if (this->visit_map[map_no] > 0x270F) {
        this->visit_map[map_no] = 0x270F;
    }

    return this->visit_map[map_no];
}

int CSaveData::QuestDungeon(int dungeon_no, int add) {
    if (dungeon_no == 6) {
        this->quest_dungeon_total += add;

        if (this->quest_dungeon_total > 0x270F) {
            this->quest_dungeon_total = 0x270F;
        }

        return this->quest_dungeon_total;
    }

    if (dungeon_no < 0 || dungeon_no >= 6) {
        return 0;
    }

    this->quest_dungeon[dungeon_no] += add;

    if (this->quest_dungeon[dungeon_no] > 0x270F) {
        this->quest_dungeon[dungeon_no] = 0x270F;
    }

    return this->quest_dungeon[dungeon_no];
}

void CSaveData::ConvertConfig(SV_CONFIG_SYS *out) {
    char *src;
    char *dst;
    int   copy;
    int   value_no;

    dst = (char *) out;

    this->config[16] = this->menu_cursor.reset_pos;
    this->config[15] = this->dng_status.minimap_status;

    src = (char *) this->config;
    memset(dst, 0, sizeof(SV_CONFIG_SYS));

    for (copy = 0; copy < 3; copy++) {
        for (value_no = 0; value_no < 18; value_no++) {
            *(dst + copy * 18 + value_no) = *(src + value_no * 4);
        }
    }
}

int CSaveData::InvertConfig(SV_CONFIG_SYS *in) {
    int i;
    int j;

    for (j = 0; j < 18; j++) {
        if (in->values[j] != in->values_copy1[j]) {
            return 0;
        }

        if (in->values[j] != in->values_copy2[j]) {
            return 0;
        }
    }

    for (i = 0; i < 18; i++) {
        this->config[i] = in->values[i];
    }

    s32 saved_reset_pos = this->config[16];
    this->menu_cursor.reset_pos = (s16) saved_reset_pos;

    if (this->menu_cursor.reset_pos != 0) {
        this->menu_cursor.InitPos();
    }

    this->dng_status.minimap_status = this->config[15];

    return 1;
}

void CMenuCursor::InitPos() {
    int i;
    int j;

    for (i = 0; i < 10; i++) {
        this->pos[i] = 0;
    }

    for (j = 0; j < 10; j++) {
        this->mode[j] = -1;
    }

    this->mode[0] = 4;
    this->mode[2] = 6;
    this->mode[3] = 0;
    this->chara_no = 0;
}

void CSaveData::Initialize() {
    int i;
    int j;
    int k;

    memset(this, 0, sizeof(CSaveData));
    this->unk_1CC = -1;
    this->now_time = 0.0f;

    for (j = 0; j < 6; j++) {
        for (k = 0; k < 128; k++) {
            this->elem_data[j][k] = -1;
        }
    }

    SV_GEORAMA_DATA *town = this->georama;

    for (i = 0; i < 6; i++, town++) {
        memset(town, 0, sizeof(SV_GEORAMA_DATA));

        for (j = 0; j < 24; j++) {
            SV_EDIT_PARTS_INFO *info = &town->parts_info[j];

            for (k = 0; k < 6; k++) {
                info->element_enabled[k] = 0;
            }
        }

        for (j = 0; j < 130; j++) {
            town->placed_parts[j].part_id = -1;
        }
    }

    this->dng_status.Initialize();
    this->stock_item.Initialize();
#ifdef MOD_DEMON_SHAFT_GEORAMA
    DsgInitialize();
#endif

    memset(this, 0, sizeof(this->config));
    memset(this->game_flags, 0, sizeof(this->game_flags));
    memset(this->map_flags, 0, 4);
}

CSaveData::CSaveData() {
    this->menu_cursor.reset_pos = 0;
    this->menu_cursor.InitPos();
    this->Initialize();
}

#ifdef MOD_DEMON_SHAFT_GEORAMA
// The Demon Shaft georama mod (dsgeorama.hpp, docs/DEMON_SHAFT_GEORAMA.md).

#define DSG_SAVE_MAGIC "DSGEO01"

SV_DSG_SAVE DsgSave;
int         DsgTownActive;
int         DsgTownRequest;

static s32 DsgCheckSum(const SV_DSG_SAVE *save) {
    const u8 *data = (const u8 *) save;
    s32       total = 0;
    int       size = (int) ((const u8 *) &save->check_sum - data);

    for (int i = 0; i < size; i++) {
        total += data[i];
    }

    return total;
}

void DsgInitialize() {
    memset(&DsgSave, 0, sizeof(DsgSave));
    strcpy(DsgSave.magic, DSG_SAVE_MAGIC);

    for (int i = 0; i < DSG_ATLA_MAX; i++) {
        DsgSave.atla[i].id = -1;
        DsgSave.atla[i].floor = -1;
        DsgSave.atla[i].refcount = 0;
    }

    for (int j = 0; j < 130; j++) {
        DsgSave.georama.placed_parts[j].part_id = -1;
    }

    for (int k = 0; k < 128; k++) {
        DsgSave.elem_data[k] = -1;
    }

    DsgTownActive = 0;
    DsgTownRequest = 0;
}

int DsgSaveIndex(int georama_no) {
    // MapNo keeps the redirection to the town itself: the donor's dungeon,
    // reached with the flag still set, has its own atla.
    if (georama_no == DSG_DONOR_TOWN && DsgTownActive != 0 && MapNo == DSG_DONOR_TOWN) {
        return DSG_GEORAMA_NO;
    }

    return georama_no;
}

int DsgAssetIndex(int georama_no) {
    if (georama_no == DSG_GEORAMA_NO) {
        return DSG_DONOR_TOWN;
    }

    return georama_no;
}

void DsgEnterTown(int map_no) {
    DsgTownActive = map_no == DSG_DONOR_TOWN && DsgTownRequest != 0;
    DsgTownRequest = 0;
}

/**
 * Fills the Demon Shaft's atla records from the donor town's table the first
 * time they are wanted: every part and chip it hands out, but no villager,
 * each as many times as the donor's dungeon puts it down.
 */
static void DsgAtlaListMake() {
    if (DsgSave.atla_ready != 0) {
        return;
    }

    ATRA_APPEAR *appear = AtraAppearData[DSG_DONOR_TOWN];
    int          count = 0;

    for (int i = 0; appear[i].id != -1 && count < DSG_ATLA_MAX; i++) {
        if (appear[i].id >= DSG_ATLA_FIRST_VILLAGER) {
            continue;
        }

        DsgSave.atla[count].id = appear[i].id;
        DsgSave.atla[count].floor = -1;
        DsgSave.atla[count].refcount = appear[i].count;
        count++;
    }

    DsgSave.atla_ready = 1;
}

static int DsgRand(int range) {
    int value = (int) (((float) range * (float) rand()) / 2147483648.0f);

    if (value < 0 || value >= range) {
        value = 0;
    }

    return value;
}

int DsgAtlaFloorChoose(int floor, int *atra_no) {
    int candidates[DSG_ATLA_MAX];
    int candidate_num = 0;
    int count = 0;
    int want;
    int roll;

    for (int i = 0; i < 8; i++) {
        atra_no[i] = -1;
    }

    DsgAtlaListMake();

    for (int j = 0; j < DSG_ATLA_MAX; j++) {
        if (DsgSave.atla[j].id >= 0 && DsgSave.atla[j].refcount > 0) {
            candidates[candidate_num++] = j;
        }
    }

    if (candidate_num == 0) {
        return 0;
    }

    // About one floor in three has an atla, and a few of those two.
    roll = DsgRand(100);
    want = 0;

    if (roll < 35) {
        want = 1;
    }

    if (roll < 8) {
        want = 2;
    }

    while (count < want && candidate_num > 0) {
        int pick = DsgRand(candidate_num);

        atra_no[count++] = candidates[pick];
        candidates[pick] = candidates[--candidate_num];
    }

    return count;
}

int DsgAtlaId(int atra_no) {
    if (atra_no < 0 || atra_no >= DSG_ATLA_MAX) {
        return -1;
    }

    return DsgSave.atla[atra_no].id;
}

void DsgAtlaCollected(int atra_no) {
    if (atra_no < 0 || atra_no >= DSG_ATLA_MAX || DsgSave.atla[atra_no].refcount <= 0) {
        return;
    }

    DsgSave.atla[atra_no].refcount--;
}

void DsgWriteSave(char *dst) {
    DsgSave.in_town = DsgTownActive != 0 && MapNo == DSG_DONOR_TOWN;
    strcpy(DsgSave.magic, DSG_SAVE_MAGIC);
    DsgSave.check_sum = DsgCheckSum(&DsgSave);
    memcpy(dst, &DsgSave, sizeof(DsgSave));
}

void DsgReadSave(const char *src, int size) {
    DsgInitialize();

    if (src == NULL || size < DSG_SAVE_SIZE) {
        return;
    }

    const SV_DSG_SAVE *save = (const SV_DSG_SAVE *) src;

    if (memcmp(save->magic, DSG_SAVE_MAGIC, sizeof(save->magic)) != 0 || save->check_sum != DsgCheckSum(save)) {
        return;
    }

    memcpy(&DsgSave, save, sizeof(DsgSave));
    // A save made in the Demon Shaft's georama goes back to it.
    DsgTownRequest = DsgSave.in_town;
}
#endif
