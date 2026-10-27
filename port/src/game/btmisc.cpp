#include "common.h"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "btitem.hpp"
#include "btmisc.hpp"
#include "camera.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "hitvalue.hpp"
#include "itemdata.hpp"
#include "mds.hpp"
#include "menu_save.hpp"
#include "savedata.hpp"
#include "snd.hpp"

namespace dc::game {

namespace {

bool  battle_music_active;
int   battle_music_wait;
float battle_music_volume;

constexpr std::array<std::string_view, 6> kWeaponPrefixes = {"c01w", "c04w", "c06w", "c05w", "c10w", "c18w"};

// The floor number shown for each floor of the deepest dungeon.
constexpr std::array<int, 25> kYearFloorTable = {5, 18, 23, 38, 51, 66, 102, 109, 122, 140, 151, 162, 205,
                                                 208, 213, 225, 238, 249, 300, 310, 322, 340, 356, 382, 400};

void CopyToBuffer(std::string_view text, char *buffer) {
    std::ranges::copy(text, buffer);
    buffer[text.size()] = '\0';
}

std::string FormatWeaponIndex(int index) {
    if (index < 10) {
        return std::format("0{}", index);
    }

    return std::format("{:2}", index);
}

std::string makeWeaponName(int weapon_no) {
    constexpr std::array<int, 6> first_weapons = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN,
                                                  ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN,
                                                  ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};
    int                          chara_no = 0;

    if (weapon_no >= ITEM_WEAPON_DAGGER_BROKEN) {
        if (weapon_no >= ITEM_WEAPON_DAGGER_BROKEN && weapon_no < ITEM_WEAPON_WOODENSLINGSHOT_BROKEN) {
            chara_no = 0;
        }

        if (weapon_no >= ITEM_WEAPON_WOODENSLINGSHOT_BROKEN && weapon_no < ITEM_WEAPON_MALLET_BROKEN) {
            chara_no = 1;
        }

        if (weapon_no >= ITEM_WEAPON_MALLET_BROKEN && weapon_no < ITEM_WEAPON_GOLD_RING_BROKEN) {
            chara_no = 2;
        }

        if (weapon_no >= ITEM_WEAPON_GOLD_RING_BROKEN && weapon_no < ITEM_WEAPON_FIGHTING_STICK_BROKEN) {
            chara_no = 3;
        }

        if (weapon_no >= ITEM_WEAPON_FIGHTING_STICK_BROKEN && weapon_no < ITEM_WEAPON_MACHINE_GUN_BROKEN) {
            chara_no = 4;
        }

        if (weapon_no >= ITEM_WEAPON_MACHINE_GUN_BROKEN) {
            chara_no = 5;
        }
    }

    const int offset = weapon_no - first_weapons[chara_no];
    return std::format("dun/item/main_wep/{}{}", kWeaponPrefixes[chara_no], FormatWeaponIndex(offset));
}

} // namespace

CFrame *LoadMDSFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer) {
    int           size = 0;
    unsigned int *file = GetPackFile(pack, name, &size);

    if (file == nullptr) {
        printf("Model NotFound!!%s\n", name);
        exit__2(-1);
    }

    return LoadMDSFile(file, buffer, 0, nullptr, nullptr);
}

CFrame *LoadCollisionFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer) {
    int           size = 0;
    unsigned int *file = GetPackFile(pack, name, &size);

    if (file == nullptr) {
        printf("Model NotFound!!%s\n", name);
        exit__2(-1);
    }

    return LoadCollisionFile(file, buffer);
}

void setCameraPassData(CFrameVu1 *frame, CCamera *camera, char *position_name, char *reference_name) {
    sceVu0FMATRIX matrix;

    // A matrix's fourth row is where the frame stands.
    frame->SearchFrame(reference_name)->GetLWMatrix(matrix);
    camera->SetRef(matrix[3]);
    frame->SearchFrame(position_name)->GetLWMatrix(matrix);
    camera->SetPos(matrix[3]);
}

void getFramePos(CFrameVu1 *frame, char *name, float *position) {
    CFrame       *named_frame = frame->SearchFrame(name);
    sceVu0FVECTOR origin = {};

    named_frame->GetWorldPosition(position, origin);
}

void BtGetItemNamePath(char *model_path, char *texture_path, int item_no) {
    item_no = TransWepNo(item_no);

    std::string model;

    if (item_no >= ITEM_WEAPON_START) {
        model = makeWeaponName(item_no);
    } else {
        if (ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START] == nullptr) {
            item_no = ITEM_REGULAR_WATER;
        }

        model = std::format("dun/item/main_data/{}", ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START]);
    }

    const std::string texture = model + ".img";
    model += ".mds";
    CopyToBuffer(model, model_path);
    CopyToBuffer(texture, texture_path);
    printf("mds = %s\n", model_path);
    printf("img = %s\n", texture_path);
}

void BtGetWeaponNamePath2(char *chr_name, char *cfg_name, int chara, int weapon_index) {
    const std::string_view prefix = kWeaponPrefixes[chara];
    const std::string      number = FormatWeaponIndex(weapon_index);

    CopyToBuffer(std::format("{}{}.chr", prefix, number), chr_name);
    CopyToBuffer(std::format("{}{}.cfg", prefix, number), cfg_name);
}

void BtGetWeaponNamePath3(char *chr_name, char *cfg_name, int weapon_no) {
    if (weapon_no < ITEM_WEAPON_START) {
        return;
    }

    const WEAPON_DATA *weapon_data = GetWeaponData(weapon_no);

    if (weapon_data != nullptr) {
        const int chara_no = static_cast<s8>(weapon_data->owner);
        weapon_no -= defWeapon__2[chara_no];
        printf("offset %d\n", weapon_no);
        BtGetWeaponNamePath2(chr_name, cfg_name, chara_no, weapon_no);
    }
}

void getAtraToSaveData(int atra, int atra_no, CSaveData *save, int dungeon, int floor) {
    printf("GET ATRA [%d] !!\n", atra);

    if (atra < 0x28) {
        save->AtraPartsGet(dungeon, atra);
    } else {
        save->AtraChipGet(dungeon, atra - 0x28);
    }

    static_cast<CDngStatusData *>(UserStatus)->GetAtraData(dungeon, floor, atra_no);
}

int createAttachVolume(int item_no, [[maybe_unused]] int dungeon) {
    if (item_no < ITEM_ATTACH_STAT_START || item_no > ITEM_ATTACH_MAGICAL_POWER) {
        return 0;
    }

    int roll = static_cast<int>((50.0f * static_cast<float>(rand())) / 2.1474836e9f);
    roll += static_cast<int>((50.0f * static_cast<float>(rand())) / 2.1474836e9f);
    int volume = 1;

    if (roll < 31) {
        volume = 2;
    }

    if (roll < 16) {
        volume = 3;
    }

    return volume;
}

void BtBattleMusic_Init() {
    battle_music_active = false;
    battle_music_wait = 0;
    battle_music_volume = 0.0f;
}

void BtBattleMusic_Stop() {
    if (battle_music_active) {
        SndSetBgmVolf(1.0f);
        SndAmbientSetVolf(0.0f);
        BtBattleMusic_Init();
    }
}

void BtBattleMusic_Excg(float distance, float *field_volume, float *battle_volume) {
    if (battle_music_active) {
        if (distance >= 110.0f) {
            if (battle_music_wait == 0) {
                battle_music_volume -= 1.0f / 30.0f;

                if (battle_music_volume < 0.1f) {
                    battle_music_active = false;
                    battle_music_volume = 0.0f;
                }
            } else {
                battle_music_wait--;
            }
        } else if (battle_music_volume < 0.9f) {
            battle_music_volume += 1.0f / 30.0f;
        }
    }

    if (!battle_music_active && distance <= 100.0f) {
        battle_music_active = true;
    }

    battle_music_volume = std::min(battle_music_volume, 0.9f);

    if (battle_music_volume <= 0.0f) {
        battle_music_volume = 0.0f;
    }

    *field_volume = 1.0f - battle_music_volume;
    *battle_volume = battle_music_volume;
}

int BtGetFloorLevel(int floor) {
    if (floor >= 0 && std::cmp_less(floor, kYearFloorTable.size())) {
        return kYearFloorTable[floor];
    }

    return 0;
}

} // namespace dc::game
