#include "fishing.hpp"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>

#include <cstdlib>
#include <cstring>

#include "boxvu0.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "edit.hpp"
#include "editloop3.hpp"
#include "fish.hpp"
#include "framevu1.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "texture.hpp"

/**
 * Describes one kind of bait that can be put on the hook.
 */
struct EsaInfo {
    int   item_no; /**< Item the bait is made from. */
    float radius;  /**< Distance from which fish notice the bait. */
};

/**
 * The kinds of bait, indexed by esa_type.
 */
static EsaInfo esa_info[13] = {
    {ITEM_EVY,              50.0f},
    {ITEM_MIMI,             25.0f},
    {ITEM_PRICKLY,          25.0f},
    {ITEM_THROBBING_CHERRY, 25.0f},
    {ITEM_GOOEY_PEACH,      25.0f},
    {ITEM_BOMB_NUTS,        25.0f},
    {ITEM_POISONOUS_APPLE,  25.0f},
    {ITEM_MELLOW_BANANA,    25.0f},
    {ITEM_CARROT,           25.0f},
    {ITEM_POTATO_CAKE,      25.0f},
    {ITEM_MINON,            25.0f},
    {ITEM_BATTAN,           25.0f},
    {ITEM_PETITE_FISH,      40.0f},
};

int           draw_under_water = 1;
float         distp = 1.6666666f;
float         WaterLevel;
float         GroundLevel;
float         UkiGroundLevel;
float         HookGroundLevel;
float         LineGroundLevel;
CFrameVu1    *EsaFrame;
int           esa_type;
CFrame       *HookFrame;
CFrame       *UkiFrame;
int           fishing_texb;
int           fish_texb;
int           esa_texb;
CFish        *Fish;
CFish        *AngleFish;
CFish        *BattleFish;
int           FishNum;
CCPoly       *cpoly;
int           cpoly_num;
int           set_hook_pos;
int           set_uki_pos;
float         pull_hook;
CBoxVu0       fishing_rect;
CBoxVu0       fish_rect;
CFrameVu1     UkiFrameTop;
CCharacter    Rod;
sceVu0FVECTOR point[24];
sceVu0FVECTOR old_p[24];
sceVu0FVECTOR velo[24];
sceVu0FVECTOR hookp[3];
sceVu0FVECTOR hookop[3];
sceVu0FVECTOR hookv[3];
float         hook_dist[3];
sceVu0FVECTOR ukip[4];
sceVu0FVECTOR ukiop[4];
sceVu0FVECTOR ukiv[4];
float         uki_dist[6];
sceVu0FVECTOR rod_top;
sceVu0FVECTOR fishhook;
sceVu0FVECTOR uki;

static void GetHookPos(float *position);

void FishingLoad(CDataAlloc2<1> *alloc, int slot) {
    LoadFile("chara/fishing.pak", read_buffer, NULL);
    fishing_texb = slot;
    HookFrame = LoadMDSFile(GetPackFile(read_buffer, "hari.mds", NULL), alloc, 0, NULL, NULL);
    UkiFrame = LoadMDSFile(GetPackFile(read_buffer, "uki.mds", NULL), alloc, 0, NULL, NULL);
    CFrameAttr attr;
    attr.use_color = true;

    if (UkiFrame != NULL) {
        UkiFrame->SetAttr(attr, 1, 0x200);
    }

    cpoly = (CCPoly *) alloc->Alloc(0x1800);
    cpoly_num = 0;
    SndSPSeLoad(0x2F);
}

void FishingLoadFish(int spot, CDataAlloc2<1> *alloc, int slot) {
    int rare_interval;
    int loaded;

    draw_under_water = 1;

    if (spot == 3) {
        draw_under_water = 0;
    }

    Fish = new ((u_long128 *) alloc->Alloc(0xD8C)) CFish[6];
    fish_texb = slot;
    FishNum = 5;

    if (spot == 0) {
        FishNum = 4;
    }

    if (spot == 4) {
        FishNum = 4;
    }

    if (DebugMode) {
        FishNum = 5;
    }

    rare_interval = 30;

    switch (EdGetTime(SaveData->GetNowTime())) {
        case ED_TIME_DAY:
            rare_interval = 50;
            break;
        case ED_TIME_EVENING:
            rare_interval = 20;
            break;
        case ED_TIME_NIGHT:
            rare_interval = 35;
            break;
        case ED_TIME_MORNING:
            rare_interval = 25;
            break;
    }

    loaded = false;
    LoadFile("chara/f00s.chr", read_buffer, NULL);

    for (int i = 0; i < FishNum; i++) {
        int kind = -1;
        int rand_value = rand();

        switch (spot) {
            case 0:
                switch ((int) (3.99999f * rnd())) {
                    case 0:
                        kind = 1;
                        break;
                    case 1:
                        kind = 2;
                        break;
                    case 2:
                        kind = 6;
                        break;
                    case 3:
                        kind = 7;
                        break;
                }

                break;
            case 1: {
                int roll = 100.0f * rnd();

                if (roll < 35) {
                    kind = 1;
                } else if (roll < 70) {
                    kind = 4;
                } else if (roll < 80) {
                    kind = 9;
                } else if (roll < 100) {
                    kind = 10;
                }

                break;
            }
            case 2:
                if (rand_value % rare_interval == 0) {
                    if (rand() % 5 == 0) {
                        kind = 17;
                    } else {
                        kind = 5;
                    }
                } else {
                    rand_value %= 3;

                    if (rand_value == 0) {
                        kind = 2;
                    }

                    if (rand_value == 1) {
                        kind = 4;
                    }

                    if (rand_value == 2) {
                        kind = 6;
                    }
                }

                break;
            case 3: {
                int roll = rand_value % 100;

                if (roll < 20) {
                    kind = 0;
                } else if (roll < 40) {
                    kind = 3;
                } else if (roll < 60) {
                    kind = 11;
                } else if (roll < 80) {
                    kind = 12;
                } else {
                    kind = 13;
                }

                break;
            }
            case 4:
                if (rand_value % rare_interval == 0) {
                    if (rand() % 5 == 0) {
                        kind = 17;
                    } else {
                        kind = 5;
                    }
                } else {
                    int roll = rand_value % 100;

                    if (roll < 40) {
                        kind = 14;
                    } else if (roll < 70) {
                        kind = 15;
                    } else if (roll < 100) {
                        kind = 16;
                    }
                }

                break;
        }

        if (DebugMode) {
            static int fish_type[5][8] = {
                {1, 2, 6, 7, 1, 2, 6, 7},
                {1, 4, 9, 10, 1, 4, 9, 10},
                {2, 4, 6, 5, 17, 2, 4, 6},
                {0, 3, 11, 12, 13, 0, 3, 11},
                {14, 15, 16, 5, 17}
            };
            kind = fish_type[spot][i % 5];
        }

        LoadFish(&Fish[i], kind, slot, alloc, loaded);
        loaded = true;
    }
}

CFish::CFish() {
    Initialize();
}

void FishingLoadEsa(int item_no, CFrameVu1 *frame, int slot) {
    if (esa_type >= 0) {
        EdGetItem(esa_info[esa_type].item_no, 1, -1);
    }

    EsaFrame = frame;
    esa_texb = slot;
    esa_type = 0;

    for (int i = 0; i < 13; i++) {
        if (item_no == esa_info[i].item_no) {
            esa_type = i;
            return;
        }
    }
}

void FishingDeleteEsa() {
    EsaFrame = NULL;
    esa_type = -1;
}

int FishingGetEsaItemNo() {
    if (esa_type < 0) {
        return -1;
    }

    return esa_info[esa_type].item_no;
}

void FishingInit() {
    WaterLevel = 0.0f;
    GroundLevel = 0.0f;
    HookFrame = NULL;
    UkiFrame = NULL;
    EsaFrame = NULL;
    esa_type = -1;
    UkiFrameTop.Initialize();
    Rod.Initialize();
    esa_texb = -99;
    fish_texb = -99;
    fishing_texb = -99;
    Fish = NULL;
    FishNum = 0;
    AngleFish = NULL;
    BattleFish = NULL;
    cpoly = NULL;
    cpoly_num = 0;
}

void FishingExit() {
    if (esa_type >= 0) {
        EdGetItem(esa_info[esa_type].item_no, 1, -1);
    }

    for (int i = 0; i < 6; i++) {
        Fish[i].Initialize();
    }

    FishingInit();
}

void FishingSetWaterLevel(float water_level, float ground_level) {
    WaterLevel = water_level;
    GroundLevel = ground_level;
    FishingSetGroundLevel(ground_level, ground_level);
}

void FishingSetGroundLevel(float uki_height, float hook_height) {
    UkiGroundLevel = uki_height;
    HookGroundLevel = hook_height;
}

float FishingGetWaterLevel() {
    return WaterLevel;
}

void FishingSetCPoly(CCPoly *polys, int count) {
    for (int i = 0; i < count; i++) {
        memcpy(&cpoly[i], &polys[i], sizeof(CCPoly));
    }

    cpoly_num = count;
}

void FishingSetRect(CBoxVu0 bounds) {
    memcpy(&fishing_rect, &bounds, sizeof(CBoxVu0));
}

int FishingPickUpPoly(CCPoly *polys) {
    sceVu0FVECTOR corner[8];
    sceVu0FVECTOR bound[2];

    sceVu0CopyVector(bound[0], fishing_rect.min);
    sceVu0CopyVector(bound[1], fishing_rect.max);

    for (int i = 0; i < 8; i++) {
        corner[i][3] = 1.0f;
        corner[i][0] = bound[(i & 1) != 0][0];
        corner[i][1] = bound[(i & 2) != 0][1];
        corner[i][2] = bound[(i & 4) != 0][2];
    }

    int face[8][3] = {
        {0, 1, 2},
        {1, 3, 2},
        {1, 5, 3},
        {5, 7, 3},
        {5, 4, 7},
        {4, 6, 7},
        {4, 0, 6},
        {0, 2, 6},
    };

    for (int i = 0; i < 8; i++) {
        CCPoly *poly = &polys[i];
        memset(poly, 0, sizeof(CCPoly));

        for (int j = 0; j < 3; j++) {
            sceVu0CopyVector(poly->vertex[j], corner[face[i][j]]);
        }

        PlaneNormal(poly->normal, poly->vertex[0], poly->vertex[1], poly->vertex[2]);
    }

    return 8;
}

void FishingInitFish(CBoxVu0 bounds) {
    sceVu0FVECTOR position;

    if (Fish == NULL) {
        return;
    }

    memcpy(&fish_rect, &bounds, sizeof(CBoxVu0));

    for (int i = 0; i < FishNum; i++) {
        position[0] = 0.5f * (bounds.max[0] + bounds.min[0]);
        position[1] = WaterLevel - 12.0f;
        position[2] = 0.5f * (bounds.max[2] + bounds.min[2]);
        Fish[i].SetPosition(position);
        Fish[i].SetCPoly(cpoly, cpoly_num);
    }
}

int FishingFishStatus(int *fish_no) {
    int status;

    if (Fish == NULL) {
        return -1;
    }

    status = -1;

    for (int i = 0; i < 6; i++) {
        if (fish_no != NULL) {
            *fish_no = i;
        }

        CFish *fish;

        if (status == -1) {
            fish = &Fish[i];

            if (fish->action == FISH_ACTION_EAT_FOOD) {
                status = FISH_ACTION_EAT_FOOD;
            }
        }

        fish = &Fish[i];

        if (fish->action == FISH_ACTION_BITE_HOOK) {
            return FISH_ACTION_BITE_HOOK;
        }

        if (fish->action == FISH_ACTION_BATTLE) {
            return FISH_ACTION_BATTLE;
        }
    }

    return status;
}

void FishingBattleFish(int fish_no) {
    BattleFish = NULL;

    if (fish_no >= 0 && fish_no < 6) {
        CFish *fish = &Fish[fish_no];

        if (fish->action == FISH_ACTION_BITE_HOOK) {
            BattleFish = fish;
            fish->SetBattleMode();
        }
    }
}

int FishingFishKind(int fish_no) {
    // The range check can never fail: it needs both bounds exceeded at once.
    if (fish_no < 0 && fish_no >= 6) {
        return -1;
    }

    return Fish[fish_no].fish_kind;
}

void FishingBattleToAngleFish(u_int *pack, CDataAlloc2<1> *alloc) {
    AngleFish = BattleFish;
    BattleFish = NULL;

    if (AngleFish != NULL) {
        if (pack != NULL) {
            CCharacter *model = &AngleFish->angle_model;
            model->LoadPackData2(pack, "info.cfg", alloc, fish_texb + 1, alloc, 0);
        }

        AngleFish->SetAngleMode();
        CFish *fish = AngleFish;
        fish->use_angle_model = true;
    }
}

char fishing_err_format[] __attribute__((section(".rodata"))) = "err %d\n";

CFish *FishingGetBattleFish() {
    return BattleFish;
}

void FishingAngleFish(int fish_no) {
    AngleFish = NULL;

    if (fish_no >= 0 && fish_no < 6) {
        CFish *fish = &Fish[fish_no];

        if (fish->action == FISH_ACTION_BITE_HOOK) {
            AngleFish = fish;
            fish->SetAngleMode();
            fish = &Fish[fish_no];
            fish->use_angle_model = true;
        }
    }
}

int FishingGetAngleFishSize(int *size, int *fp) {
    if (AngleFish == NULL) {
        return -1;
    }

    float length = AngleFish->size;
    *size = length * 10.0f;
    *fp = AngleFish->GetFP();
    return AngleFish->fish_kind;
}

void FishingInitFishStatus() {
    if (Fish != NULL) {
        for (int i = 0; i < 6; i++) {
            CFish *fish = &Fish[i];
            fish->action = FISH_ACTION_NONE;
            fish->move_mode = FISH_MOVE_UNSET;
            fish->use_angle_model = false;
        }
    }
}

void FishingDeleteAngleFish() {
    if (AngleFish != NULL) {
        AngleFish->fish_kind = -1;
    }
}

void FishingStepFish() {
    sceVu0FVECTOR hook;

    GetHookPos(hook);
    hook[1] -= 3.0f;

    if (Fish == NULL) {
        return;
    }

    if (AngleFish != NULL) {
        AngleFish->Step();
        return;
    }

    for (int i = 0; i < 6; i++) {
        if (esa_type < 0 || EsaFrame == NULL) {
            float radius = 0.0f;
            Fish[i].SetFoodPos(-1, hook, radius);
        } else {
            float radius = esa_info[esa_type].radius;

            // Debug builds let every fish notice the bait from anywhere.
            if (DebugMode) {
                radius = 100000000;
            }

            Fish[i].SetFoodPos(esa_type, hook, radius);
        }

        Fish[i].Step();
    }
}

void FishingDrawFish() {
    if (Fish == NULL) {
        return;
    }

    if (AngleFish != NULL) {
        TexManager.ReloadTexture(GetVif1Packet(), fish_texb + 1);
        AngleFish->SetReference(HookFrame);
        sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 0.0f};
        sceVu0FVECTOR rotation = {-1.57f, 1.57f, 0.0f, 0.0f};
        AngleFish->SetPosition(position);
        AngleFish->SetRotation(rotation);
        AngleFish->Draw();
        AngleFish->DeleteReference();
    } else {
        TexManager.ReloadTexture(GetVif1Packet(), fish_texb);

        if (draw_under_water) {
            for (int i = 0; i < 6; i++) {
                Fish[i].Draw();
            }
        }
    }
}

/**
 * Gives where the hook is.
 *
 * @mangled GetHookPos__FPf
 * @address 0x1A9BC0
 * @size 0x28
 */
static void GetHookPos(float *position) {
    sceVu0CopyVector(position, hookp[0]);
}

/**
 * The pairs of hook points held apart by a fixed length.
 */
static int hook_link[3][2] = {
    {0, 1},
    {0, 2},
    {1, 2}
};

/**
 * The pairs of float points held apart by a fixed length.
 */
static int uki_link[6][2] = {
    {0, 1},
    {0, 2},
    {0, 3},
    {1, 2},
    {2, 3},
    {3, 1}
};

void FishLineInit(float *position) {
    sceVu0FVECTOR p;
    int           i;

    set_uki_pos = 0;
    set_hook_pos = 0;
    pull_hook = 0.0f;
    sceVu0FVECTOR zero = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0CopyVector(rod_top, position);
    sceVu0CopyVector(p, position);
    p[3] = 1.0f;

    for (i = 0; i < 24; i++) {
        sceVu0CopyVector(point[i], p);
        sceVu0CopyVector(old_p[i], p);
        sceVu0CopyVector(velo[i], zero);
        p[1] -= distp;
    }

    for (i = 0; i < 3; i++) {
        sceVu0CopyVector(hookp[i], point[23]);
    }

    for (i = 1; i < 3; i++) {
        hookp[i][1] -= 2.0f;
    }

    hookp[1][0] -= 1.0f;
    hookp[2][0] += 1.0f;

    for (i = 0; i < 3; i++) {
        sceVu0CopyVector(hookop[i], hookp[i]);
        sceVu0CopyVector(hookv[i], zero);
    }

    for (i = 0; i < 3; i++) {
        hook_dist[i] = DistVector(hookp[hook_link[i][0]], hookp[hook_link[i][1]]);
    }

    for (i = 0; i < 4; i++) {
        sceVu0CopyVector(ukip[i], point[18]);
    }

    for (i = 1; i < 4; i++) {
        ukip[i][1] -= 0.866f;
    }

    ukip[1][2] += 1.005f;
    ukip[2][0] += 0.866f;
    ukip[2][2] -= 0.5f;
    ukip[3][0] -= 0.866f;
    ukip[3][2] -= 0.5f;

    for (i = 0; i < 4; i++) {
        sceVu0CopyVector(ukiop[i], ukip[i]);
        sceVu0CopyVector(ukiv[i], zero);
    }

    for (i = 0; i < 6; i++) {
        uki_dist[i] = DistVector(ukip[uki_link[i][0]], ukip[uki_link[i][1]]);
    }
}

void FishLineSetUki(float *position, float rate) {
    sceVu0FVECTOR offset;

    if (rate < 0.0f) {
        set_uki_pos = 0;
        return;
    }

    sceVu0SubVector(offset, position, ukip[0]);
    sceVu0ScaleVector(offset, offset, rate);
    sceVu0AddVector(uki, ukip[0], offset);
    set_uki_pos = 1;
}

void FishLineSetHook(float *position, float rate) {
    sceVu0FVECTOR offset;

    if (rate < 0.0f) {
        set_hook_pos = 0;
        return;
    }

    sceVu0SubVector(offset, position, hookp[0]);
    sceVu0ScaleVector(offset, offset, rate);
    sceVu0AddVector(fishhook, hookp[0], offset);
    set_hook_pos = 1;
}

void FishPullHook(float tension) {
    pull_hook = tension;
}

void FishLineGetUki(float *position) {
    sceVu0CopyVector(position, point[18]);
}

void FishLineGetHook(float *position) {
    sceVu0CopyVector(position, point[23]);
}

int FishingCheckUkiHook() {
    sceVu0FVECTOR uki_pos;
    sceVu0FVECTOR hook_pos;
    float         water;

    FishLineGetUki(uki_pos);
    FishLineGetHook(hook_pos);

    for (int i = 0; i < 3; i++) {
        if (fishing_rect.max[i] < uki_pos[i]) {
            return 1;
        }

        if (fishing_rect.max[i] < hook_pos[i]) {
            return 1;
        }

        if (fishing_rect.min[i] > uki_pos[i]) {
            return 1;
        }

        if (fishing_rect.min[i] > hook_pos[i]) {
            return 1;
        }
    }

    water = 5.0f + FishingGetWaterLevel();

    if (hook_pos[1] > water) {
        return 1;
    }

    if (uki_pos[1] > water) {
        return 1;
    }

    return 0;
}

void FishLineStep(float *rod_position, float *unused) {
    sceVu0FVECTOR hook_from;
    sceVu0FVECTOR hook_to;
    sceVu0FVECTOR uki_from;
    sceVu0FVECTOR uki_to;
    sceVu0FVECTOR hit;
    sceVu0FVECTOR normal;
    sceVu0FVECTOR diff;
    sceVu0FVECTOR move0;
    sceVu0FVECTOR move1;
    float         water;
    float         weight;
    int           i;
    int           iteration;

    if (cpoly != NULL) {
        sceVu0CopyVector(hook_from, point[23]);
        sceVu0CopyVector(hook_to, point[23]);
        hook_from[1] += 40.0f;
        hook_to[1] -= 40.0f;
        int hit_no = CheckHit(cpoly, cpoly_num, hook_from, hook_to, hit, 1, 0);

        if (hit_no >= 0) {
            sceVu0Normalize(normal, cpoly[hit_no].normal);
            float slope = normal[1] < 0.0f ? -normal[1] : normal[1];

            if (slope > 0.2f) {
                HookGroundLevel = 0.2f + hit[1];
            }
        }

        sceVu0CopyVector(uki_from, point[18]);
        sceVu0CopyVector(uki_to, point[18]);
        uki_from[1] += 40.0f;
        uki_to[1] -= 40.0f;
        hit_no = CheckHit(cpoly, cpoly_num, uki_from, uki_to, hit, 1, 0);

        if (hit_no >= 0) {
            sceVu0Normalize(normal, cpoly[hit_no].normal);
            float slope = normal[1] < 0.0f ? -normal[1] : normal[1];

            if (slope > 0.2f) {
                UkiGroundLevel = 1.0f + hit[1];
            }
        }
    }

    if (UkiGroundLevel < HookGroundLevel) {
        LineGroundLevel = UkiGroundLevel;
    } else {
        LineGroundLevel = HookGroundLevel;
    }

    water = WaterLevel;

    sceVu0CopyVector(rod_top, rod_position);
    rod_top[3] = 1.0f;
    fishhook[3] = 1.0f;

    for (i = 0; i < 24; i++) {
        velo[i][1] += -0.1f;
        velo[i][3] = 0.0f;
        sceVu0AddVector(point[i], point[i], velo[i]);
    }

    weight = pull_hook;
    point[23][1] += weight;

    for (i = 0; i < 3; i++) {
        hookv[i][1] += -0.1f + weight;
        hookv[i][3] = 0.0f;
        sceVu0AddVector(hookp[i], hookp[i], hookv[i]);
    }

    for (i = 0; i < 4; i++) {
        ukiv[i][1] += -0.1f + weight;
        ukiv[i][3] = 0.0f;
        sceVu0AddVector(ukip[i], ukip[i], ukiv[i]);
    }

    for (iteration = 0; iteration < 16; iteration++) {
        sceVu0CopyVector(point[0], rod_top);
        sceVu0CopyVector(old_p[0], rod_top);

        for (i = 1; i < 24; i++) {
            weight = 0.5f;

            if (i == 23) {
                weight = 0.9f;
            }

            float  rest = 1.0f - weight;
            float *near = point[i - 1];
            float *far = point[i];
            sceVu0SubVector(diff, near, far);
            float length = DistVector(diff);
            float stretch = length - distp;
            float scale = rest * stretch / length;
            weight = weight * stretch / length;
            sceVu0ScaleVector(move0, diff, scale);
            sceVu0ScaleVector(move1, diff, weight);
            sceVu0SubVector(near, near, move0);
            sceVu0AddVector(far, far, move1);
        }

        sceVu0CopyVector(hookp[0], point[23]);

        for (i = 0; i < 3; i++) {
            sceVu0SubVector(diff, hookp[hook_link[i][0]], hookp[hook_link[i][1]]);
            float length = DistVector(diff);
            sceVu0ScaleVector(move0, diff, 0.5f * (length - hook_dist[i]) / length);
            sceVu0SubVector(hookp[hook_link[i][0]], hookp[hook_link[i][0]], move0);
            sceVu0AddVector(hookp[hook_link[i][1]], hookp[hook_link[i][1]], move0);
        }

        sceVu0SubVector(diff, point[18], ukip[0]);
        float length = DistVector(diff);
        weight = 0.2f;
        float scale = length * (1.0f - weight) / length;
        weight = length * weight / length;
        sceVu0ScaleVector(move0, diff, scale);
        sceVu0ScaleVector(move1, diff, weight);
        sceVu0SubVector(point[18], point[18], move0);
        sceVu0AddVector(ukip[0], ukip[0], move1);

        for (i = 0; i < 6; i++) {
            sceVu0SubVector(diff, ukip[uki_link[i][0]], ukip[uki_link[i][1]]);
            float length = DistVector(diff);
            sceVu0ScaleVector(move0, diff, 0.5f * (length - uki_dist[i]) / length);
            sceVu0SubVector(ukip[uki_link[i][0]], ukip[uki_link[i][0]], move0);
            sceVu0AddVector(ukip[uki_link[i][1]], ukip[uki_link[i][1]], move0);
        }

        if (set_hook_pos) {
            sceVu0CopyVector(point[23], fishhook);
            sceVu0CopyVector(hookp[0], fishhook);
        }

        if (set_uki_pos) {
            sceVu0CopyVector(point[18], uki);
            sceVu0CopyVector(ukip[0], uki);
        }
    }

    for (i = 0; i < 24; i++) {
        sceVu0SubVector(velo[i], point[i], old_p[i]);

        if (i != 23) {
            sceVu0ScaleVector(velo[i], velo[i], 0.96f);
        } else {
            sceVu0ScaleVector(velo[i], velo[i], 0.999f);
        }

        if (point[i][1] < LineGroundLevel) {
            point[i][1] = LineGroundLevel;
            velo[i][0] *= 0.4f;
            velo[i][1] *= -0.4f;
            velo[i][2] *= 0.4f;
        }

        if (point[i][1] < water) {
            sceVu0ScaleVector(velo[i], velo[i], 0.4f);

            if (i < 19) {
                velo[i][1] += 0.1f;
            }
        }

        sceVu0CopyVector(old_p[i], point[i]);
    }

    for (i = 0; i < 3; i++) {
        sceVu0SubVector(hookv[i], hookp[i], hookop[i]);

        if (hookp[i][1] < HookGroundLevel) {
            hookp[i][1] = HookGroundLevel;
            hookv[i][0] *= 0.8f;
            hookv[i][1] *= -0.6f;
            hookv[i][2] *= 0.8f;
        }

        sceVu0CopyVector(hookop[i], hookp[i]);
        sceVu0ScaleVector(hookv[i], hookv[i], 0.99f);
    }

    for (i = 0; i < 4; i++) {
        sceVu0SubVector(ukiv[i], ukip[i], ukiop[i]);

        if (ukip[i][1] < UkiGroundLevel) {
            ukip[i][1] = UkiGroundLevel;
            ukiv[i][0] *= 0.6f;
            ukiv[i][1] *= -0.8f;
            ukiv[i][2] *= 0.6f;
        }

        if (i != 0 && ukip[i][1] < water && ukip[0][1] < water) {
            sceVu0ScaleVector(ukiv[i], ukiv[i], 0.6f);
            ukiv[i][1] += 0.3f;
        }

        if (ukip[0][1] < water && !(ukip[i][1] < water)) {
            float height = ukip[0][1] - ukip[i][1];
            float depth = ukip[0][1] - ukip[i][1];
            height = depth < 0.0f ? -depth : height;
            ukiv[i][0] *= 0.6f;
            ukiv[i][2] *= 0.6f;

            if ((depth < 0.0f ? -depth : depth) > 0.001f) {
                ukiv[i][1] += 0.3f * (water - ukip[0][1]) / height;
            }
        }

        if (ukip[i][1] < water && !(ukip[0][1] < water)) {
            float height = ukip[i][1] - ukip[0][1];
            height = height < 0.0f ? -height : height;
            float depth = ukip[0][1] - ukip[i][1];

            if ((depth < 0.0f ? -depth : depth) > 0.001f) {
                ukiv[i][1] += 0.3f * (water - ukip[i][1]) / height;
            }
        }

        sceVu0CopyVector(ukiop[i], ukip[i]);
        sceVu0ScaleVector(ukiv[i], ukiv[i], 0.999f);
    }

    LineGroundLevel = HookGroundLevel = UkiGroundLevel = GroundLevel;
    set_uki_pos = 0;
    set_hook_pos = 0;
    pull_hook = 0.0f;
}

void FishLineDraw(int above_water);

