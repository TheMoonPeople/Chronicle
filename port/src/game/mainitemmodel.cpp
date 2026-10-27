#include "mainitemmodel.hpp"

#include <libvu0.h>

#include <cstring>
#include <iterator>
#include <print>

#include "btitem.hpp"
#include "character.hpp"
#include "collisiondata.hpp"
#include "data/data_read.hpp"
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

namespace dc::game {

using namespace dc::data;

namespace {

/** Frames a thrown model flies before it lands. */
constexpr int kThrowFrames = 45;

/** Texture block of the first model cache slot. */
constexpr int kCashTextureBlock = 0x38;

/** Allocation granule of a cache area, in bytes. */
constexpr int kCashAllocUnit = 16;

CFrame &CashFrame(u_int *cash_model) {
    return *reinterpret_cast<CFrame *>(cash_model);
}

} // namespace

int CMainItemModel::GetFreeCashNo() {
    for (int slot = 0; slot < std::ssize(cash); slot++) {
        if (cash[slot] == nullptr) {
            return slot;
        }
    }

    return -1;
}

int CMainItemModel::GetFreeModelNo() {
    for (int slot = 0; slot < std::ssize(model); slot++) {
        if (model[slot] == ITEM_MODEL_FREE) {
            return slot;
        }
    }

    return -1;
}

int CMainItemModel::SetCashModel(int item_no, unsigned int *model_data, unsigned int *texture_data,
                                 int texture_size) {
    const int slot = GetFreeCashNo();
    if (slot == -1) {
        return -1;
    }

    const auto      texture_bytes = static_cast<std::size_t>(texture_size);
    DataReadSource  source;
    const bool      has_source = texture_data != nullptr && texture_size > 0 &&
                                 DataReadSourceRange(texture_data, texture_bytes, source);
    CDataAlloc2<1> &area = BtItemCashArea[slot];
    area.Reset();
    u_char *texture_copy = &area.base[static_cast<std::ptrdiff_t>(area.used) * kCashAllocUnit];
    area.Alloc((texture_size >> 4) + 1);
    std::memcpy(texture_copy, texture_data, texture_bytes);
    if (has_source) {
        DataReadRegisterCopiedSource(texture_copy, texture_bytes, &area, source);
    }
    TexManager.DeleteTextureBlock(slot + kCashTextureBlock);
    TexManager.CleanUpBuffer();
    LOADTEXTURE_INFO2 textures[2] = {};
    textures[0].name = reinterpret_cast<char *>(texture_copy);
    textures[0].block_no = slot + kCashTextureBlock;
    TexManager.LoadTextureBlockEX(slot + kCashTextureBlock, textures);
    cash[slot] = reinterpret_cast<u_int *>(LoadMDSFile(model_data, &area, 0, nullptr, nullptr));
    cash_lock[slot] = 1;
    cash_item[slot] = item_no;
    const int model_no = GetFreeModelNo();
    model[model_no] = ITEM_MODEL_LOADED;
    model_cash[model_no] = slot;
    return model_no;
}

void CMainItemModel::DeleteModel(int model_no) {
    cash_lock[model_cash[model_no]]--;

    if (cash_lock[model_cash[model_no]] <= 0) {
        cash[model_cash[model_no]] = nullptr;
        cash_lock[model_cash[model_no]] = 0;
        std::println("remove !!");
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
    frame[hand_no].SetPosition(0.0f, 0.0f, 0.0f);
    frame[hand_no].SetRotation(HALF_PI, 0.0f, 0.0f);
    model_cash[hand_no] = model_cash[source_no];
    cash_lock[model_cash[source_no]]++;
    std::println("code = {}, lock = {}", hand_no, cash_lock[model_cash[source_no]]);
    return hand_no;
}

void CMainItemModel::AllReleasItem() {
    for (int slot = 0; slot < std::ssize(model); slot++) {
        if (model[slot] == ITEM_MODEL_HAND) {
            DeleteModel(slot);
        }
    }
}

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

    for (int slot = 0; slot < std::ssize(model); slot++) {
        switch (model[slot]) {
            case ITEM_MODEL_HAND: {
                int cash_no = model_cash[slot];
                TexManager.ReloadTexture(Vif1Packet, cash_no + kCashTextureBlock);
                CFrame &placement = frame[slot];
                sceVu0CopyVector(position, placement.position);
                placement.GetRotation(rotation);
                CFrame &item_frame = CashFrame(cash[cash_no]);
                item_frame.SetPosition(position);
                item_frame.SetRotation(rotation[0], rotation[1], rotation[2]);
                item_frame.SetReference(hand);
                MGDraw(&item_frame);
                break;
            }
            case ITEM_MODEL_THROWN: {
                int cash_no = model_cash[slot];
                TexManager.ReloadTexture(Vif1Packet, cash_no + kCashTextureBlock);
                CFrame &placement = frame[slot];
                sceVu0CopyVector(position, placement.position);
                placement.GetRotation(rotation);
                placement.SetPosition(position);
                CFrame &item_frame = CashFrame(cash[cash_no]);
                item_frame.SetPosition(position);
                item_frame.SetRotation(rotation[0], rotation[1], rotation[2]);
                item_frame.DeleteReference();
                MGDraw(&item_frame);
                break;
            }
            case ITEM_MODEL_UNK_3: {
                int cash_no = model_cash[slot];
                TexManager.ReloadTexture(Vif1Packet, cash_no + kCashTextureBlock);
                CFrame &item_frame = CashFrame(cash[cash_no]);
                item_frame.DeleteReference();
                CFrame &placement = frame[slot];
                sceVu0CopyVector(position, placement.position);
                placement.GetRotation(rotation);
                item_frame.SetPosition(position);
                item_frame.SetRotation(rotation[0], rotation[1], rotation[2]);
                MGDraw(&item_frame);
                break;
            }
            default:
                break;
        }
    }
}

void CMainItemModel::Step() {
    sceVu0FVECTOR position;
    sceVu0FVECTOR up = {0.0f, 1.0f, 0.0f, 0.0f};

    for (int slot = 0; slot < std::ssize(model); slot++) {
        if (model[slot] != ITEM_MODEL_THROWN) {
            continue;
        }

        CFrame &placement = frame[slot];
        sceVu0CopyVector(position, placement.position);
        int step_result = ItemThrowStep(position, velocity[slot]);
        placement.SetPosition(position);

        // 2 means the model reached a monster effect and lands this frame.
        if (step_result == 2) {
            throw_time[slot] = kThrowFrames;
        }

        throw_time[slot]++;

        if (throw_time[slot] < kThrowFrames) {
            continue;
        }

        throw_time[slot] = 0;
        int item_no = cash_item[model_cash[slot]];

        switch (item_no) {
            case ITEM_STONE:
                NowColData->Set(position, 8, 5, 8.0f, 1.0f, 2, 2, 0, 0);
                DeleteModel(slot);
                break;
            case ITEM_GOOEY_PEACH:
                NowColData->Set(position, 8, 5, 8.0f, 1.0f, 2, 2, 0x800, 0);
                DeleteModel(slot);
                break;
            case ITEM_THROBBING_CHERRY:
                NowColData->Set(position, 2, 5, 8.0f, 1.0f, 2, 2, 0x100, 0);
                DeleteModel(slot);
                break;
            case ITEM_POISONOUS_APPLE:
                NowColData->Set(position, 2, 5, 8.0f, 1.0f, 2, 2, 0x200, 0);
                DeleteModel(slot);
                break;
            case ITEM_BOMB:
            default:
                SetBombEffect(position, 3, (selectMapNo + 1) * 30, 1.0f);
                DeleteModel(slot);
                break;
            case ITEM_HOLY_WATER:
                SndSePlay(SE_HOLY_GEM, -1, 0);
                SndSePlay(SE_EXPLOSION, -1, 0);
                MasekiEffect[4].Set(position, up, -1, -1, 0, nullptr, -1);
                MasekiEffect[4].SetDmg(static_cast<int>(30.0f * static_cast<float>(selectMapNo + 1)));
                MasekiEffect[4].SetEnemyAttr(1);
                MasekiEffect[4].SetWait(2);
                DeleteModel(slot);
                break;
            case ITEM_FIRE_GEM:
            case ITEM_ICE_GEM:
            case ITEM_THUNDER_GEM:
            case ITEM_WIND_GEM:
            case ITEM_HOLY_GEM: {
                int gem_no = item_no - ITEM_FIRE_GEM;
                SndSePlay(SE_FIRE_GEM + gem_no, -1, 0);
                SndSePlay(SE_EXPLOSION, -1, 0);
                CSHOT_EFFECT &effect = MasekiEffect[gem_no];
                effect.Set(position, up, -1, -1, 0, nullptr, -1);
                effect.SetDmg(static_cast<int>(30.0f * static_cast<float>(selectMapNo + 1)));
                effect.SetLifeTime(10);
                effect.SetWait(5);
                DeleteModel(slot);
                break;
            }
        }
    }
}

void CMainItemModel::Initialize() {
    for (int slot = 0; slot < std::ssize(cash); slot++) {
        cash[slot] = nullptr;
        cash_lock[slot] = 0;
        throw_time[slot] = 0;
    }

    for (int slot = 0; slot < std::ssize(model); slot++) {
        model[slot] = ITEM_MODEL_FREE;
        frame[slot].SetPosition(0.0f, 0.0f, 0.0f);
        frame[slot].SetRotation(PI, 0.0f, 0.0f);
    }
}

int CActiveItemPack::CheckStatusType() {
    if (now <= 0) {
        return ACTIVE_ITEM_NONE;
    }

    int item_no = item[now];

    if (item_no == -1) {
        return ACTIVE_ITEM_NONE;
    }

    switch (item_no) {
        case ITEM_REGULAR_WATER:
        case ITEM_TASTY_WATER:
        case ITEM_PREMIUM_WATER:
        case ITEM_STAMINA_DRINK:
        case ITEM_ANTIDOTE_DRINK:
            return ACTIVE_ITEM_DRINK;
        case ITEM_BREAD:
        case ITEM_PREMIUM_CHICKEN:
        case ITEM_SOAP:
        case ITEM_MIGHTY_HEALING:
        case ITEM_CHEESE:
        case ITEM_MELLOW_BANANA:
            return ACTIVE_ITEM_EAT;
        default:
            break;
    }

    if ((ITEM_LIST[item_no - ITEM_ATTACH_START].kind_flags & 2) != 0) {
        return ACTIVE_ITEM_ACTION;
    }

    if (item_no == ITEM_DRAN_S_FEATHER) {
        return ACTIVE_ITEM_FEATHER;
    }

    return ACTIVE_ITEM_NONE;
}

} // namespace dc::game
