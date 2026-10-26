#pragma argument_flag 0
#pragma argument_flag_ones 33, 34, 39, 59, 225, 254, 266, 268
#include "mainitemmodel.hpp"

#include <cstdio>
#pragma argument_flag_ones 58
#include <libvu0.h>

#include <cstring>

#include "character.hpp"
#include "collisiondata.hpp"
#include "dataalloc.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "itembombeffect.hpp"
#include "itemdata.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "shot_effect.hpp"
#include "snd.hpp"
#include "texture.hpp"

int CMainItemModel::GetFreeCashNo() {
    for (int i = 0; i < 6; i++) {
        if (cash[i] == NULL) {
            return i;
        }
    }

    return -1;
}

int CMainItemModel::GetFreeModelNo() {
    for (int i = 0; i < 16; i++) {
        if (model[i] == ITEM_MODEL_FREE) {
            return i;
        }
    }

    return -1;
}

void CMainItemModel::DeleteModel(int model_no) {
    cash_lock[model_cash[model_no]]--;

    if (cash_lock[model_cash[model_no]] <= 0) {
        cash[model_cash[model_no]] = NULL;
        cash_lock[model_cash[model_no]] = 0;
        printf(MainItemRemoveMessage);
    }

    model[model_no] = ITEM_MODEL_FREE;
    model_cash[model_no] = -1;
}

int CMainItemModel::SetHandModel(int source_no) {
    int hand_no = GetFreeModelNo();

    if (hand_no == -1) {
        return -1;
    }

    model[hand_no] = ITEM_MODEL_HAND;
    float zero = 0.0f;
    frame[hand_no].SetPosition(zero, zero, zero);
    float angle = HALF_PI;
    frame[hand_no].SetRotation(angle, 0.0f, 0.0f);
    model_cash[hand_no] = model_cash[source_no];
    cash_lock[model_cash[source_no]]++;
    printf(MainItemHandMessage, hand_no, cash_lock[model_cash[source_no]]);
    return hand_no;
}

void CMainItemModel::AllReleasItem() {
    for (int i = 0; i < 16; i++) {
        switch (model[i]) {
            case ITEM_MODEL_HAND:
                DeleteModel(i);
                break;
        }
    }
}

/**
 * Creates a thrown model at a position with a copied heading vector.
 */
int CMainItemModel::SetThrowModel(int source_no, float *position, float *heading) {
    int slot = GetFreeModelNo();
    model[slot] = ITEM_MODEL_THROWN;
    sceVu0CopyVector(velocity[slot], heading);
    frame[slot].SetPosition(position);
    frame[slot].SetRotation(HALF_PI, 0.0f, 0.0f);
    throw_time[slot] = 0;
    model_cash[slot] = model_cash[source_no];
    cash_lock[model_cash[source_no]]++;
    return slot;
}

void CMainItemModel::Draw() {
    sceVu0FVECTOR position;
    sceVu0FVECTOR rotation;
    CFrame       *hand = CharaMain.frame->SearchFrame("item");
    int           i;
    s32          *cash_no;
    CFrame       *placement;

    for (i = 0; i < 16; i++) {
        switch (model[i]) {
            case ITEM_MODEL_HAND:
                cash_no = &model_cash[i];
                TexManager.ReloadTexture(Vif1Packet, *cash_no + 0x38);
                placement = &frame[i];
                sceVu0CopyVector(position, placement->position);
                placement->GetRotation(rotation);
                ((CFrame *) cash[*cash_no])->SetPosition(position);
                ((CFrame *) cash[*cash_no])->SetRotation(rotation[0], rotation[1], rotation[2]);
                ((CFrame *) cash[*cash_no])->SetReference(hand);
                MGDraw((CFrame *) cash[*cash_no]);
                break;
            case ITEM_MODEL_THROWN:
                cash_no = &model_cash[i];
                TexManager.ReloadTexture(Vif1Packet, *cash_no + 0x38);
                placement = &frame[i];
                sceVu0CopyVector(position, placement->position);
                placement->GetRotation(rotation);
                placement->SetPosition(position);
                ((CFrame *) cash[*cash_no])->SetPosition(position);
                ((CFrame *) cash[*cash_no])->SetRotation(rotation[0], rotation[1], rotation[2]);
                ((CFrame *) cash[*cash_no])->DeleteReference();
                MGDraw((CFrame *) cash[*cash_no]);
                break;
            case ITEM_MODEL_UNK_3:
                cash_no = &model_cash[i];
                TexManager.ReloadTexture(Vif1Packet, *cash_no + 0x38);
                ((CFrame *) cash[*cash_no])->DeleteReference();
                placement = &frame[i];
                sceVu0CopyVector(position, placement->position);
                placement->GetRotation(rotation);
                ((CFrame *) cash[*cash_no])->SetPosition(position);
                ((CFrame *) cash[*cash_no])->SetRotation(rotation[0], rotation[1], rotation[2]);
                MGDraw((CFrame *) cash[*cash_no]);
                break;
            case ITEM_MODEL_FREE:
            case ITEM_MODEL_LOADED:
                break;
        }
    }
}

char MainItemRemoveMessage[] __attribute__((section(".rodata"))) = "remove !!\n";
char MainItemHandMessage[] __attribute__((section(".rodata"))) = "code = %d, lock = %d\n";
int  ItemThrowStep(float *position, float *velocity);

void CMainItemModel::Step() {
    int           i;
    int           step_result;
    CFrame       *placement;
    int           item_no;
    sceVu0FVECTOR position;
    sceVu0FVECTOR up = {0.0f, 1.0f, 0.0f, 0.0f};

    for (i = 0; i < 16; i++) {
        switch (model[i]) {
            case ITEM_MODEL_HAND:
                break;
            case ITEM_MODEL_THROWN: {
                placement = &frame[i];
                sceVu0CopyVector(position, placement->position);
                step_result = ItemThrowStep(position, velocity[i]);
                placement->SetPosition(position);

                if (step_result == 2) {
                    throw_time[i] = 45;
                }

                throw_time[i]++;

                if (throw_time[i] < 45) {
                    break;
                }

                throw_time[i] = 0;
                item_no = cash_item[model_cash[i]];

                switch (item_no) {
                    case ITEM_STONE:
                        NowColData->Set(position, 8, 5, 8.0f, 1.0f, 2, 2, 0, 0);
                        DeleteModel(i);
                        break;
                    case ITEM_GOOEY_PEACH:
                        NowColData->Set(position, 8, 5, 8.0f, 1.0f, 2, 2, 0x800, 0);
                        DeleteModel(i);
                        break;
                    case ITEM_THROBBING_CHERRY:
                        NowColData->Set(position, 2, 5, 8.0f, 1.0f, 2, 2, 0x100, 0);
                        DeleteModel(i);
                        break;
                    case ITEM_POISONOUS_APPLE:
                        NowColData->Set(position, 2, 5, 8.0f, 1.0f, 2, 2, 0x200, 0);
                        DeleteModel(i);
                        break;
                    case ITEM_BOMB:
                    default:
                        SetBombEffect(position, 3, (selectMapNo + 1) * 30, 1.0f);
                        DeleteModel(i);
                        break;
                    case ITEM_HOLY_WATER:
                        SndSePlay(SE_HOLY_GEM, -1, 0);
                        SndSePlay(SE_EXPLOSION, -1, 0);
                        MasekiEffect[4].Set(position, up, -1, -1, 0, NULL, -1);
                        MasekiEffect[4].SetDmg((int) (30.0f * (float) (selectMapNo + 1)));
                        MasekiEffect[4].SetEnemyAttr(1);
                        MasekiEffect[4].SetWait(2);
                        DeleteModel(i);
                        break;
                    case ITEM_FIRE_GEM:
                    case ITEM_ICE_GEM:
                    case ITEM_THUNDER_GEM:
                    case ITEM_WIND_GEM:
                    case ITEM_HOLY_GEM: {
                        SndSePlay(item_no - 0x3C, -1, 0);
                        SndSePlay(SE_EXPLOSION, -1, 0);
                        CSHOT_EFFECT *effect = &MasekiEffect[item_no - ITEM_FIRE_GEM];
                        effect->Set(position, up, -1, -1, 0, NULL, -1);
                        effect->SetDmg((int) (30.0f * (float) (selectMapNo + 1)));
                        effect->SetLifeTime(10);
                        effect->SetWait(5);
                        DeleteModel(i);
                        break;
                    }
                }

                break;
            }
            case ITEM_MODEL_UNK_3:
            case ITEM_MODEL_FREE:
            case ITEM_MODEL_LOADED:
                break;
        }
    }
}

void CMainItemModel::Initialize() {
    for (int i = 0; i < 6; i++) {
        cash[i] = NULL;
        cash_lock[i] = 0;
        throw_time[i] = 0;
    }

    for (int i = 0; i < 16; i++) {
        model[i] = ITEM_MODEL_FREE;
        frame[i].SetPosition(0.0f, 0.0f, 0.0f);
        frame[i].SetRotation(PI, 0.0f, 0.0f);
    }
}

int CActiveItemPack::CheckStatusType() {
    int type;

    if (now <= 0) {
        return ACTIVE_ITEM_NONE;
    }

    int item_no = item[now];
    type = -1;

    if (item_no == -1) {
        return ACTIVE_ITEM_NONE;
    }

    switch (item_no) {
        case ITEM_REGULAR_WATER:
        case ITEM_TASTY_WATER:
        case ITEM_PREMIUM_WATER:
        case ITEM_STAMINA_DRINK:
        case ITEM_ANTIDOTE_DRINK:
            type = ACTIVE_ITEM_DRINK;
            break;
        case ITEM_BREAD:
        case ITEM_PREMIUM_CHICKEN:
        case ITEM_SOAP:
        case ITEM_MIGHTY_HEALING:
        case ITEM_CHEESE:
        case ITEM_MELLOW_BANANA:
            type = ACTIVE_ITEM_EAT;
            break;
    }

    if (type != -1) {
        return type;
    }

    if (ITEM_LIST[item_no - ITEM_ATTACH_START].kind_flags & 2) {
        return ACTIVE_ITEM_ACTION;
    }

    if (item_no == ITEM_DRAN_S_FEATHER) {
        return ACTIVE_ITEM_FEATHER;
    }

    return ACTIVE_ITEM_NONE;
}
