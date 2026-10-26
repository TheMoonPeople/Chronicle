#include "common.h"

#include <libvu0.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

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

/* Battle support: pack loading, item name paths, battle music, floor queries. */

/**
 * Whether the battle-music transition is active.
 */
int BtBattleMusic_Flag;

/**
 * The delay before the next battle-music transition.
 */
int BtBattleMusic_Wait;

/**
 * The current battle-music volume.
 */
float BtBattleMusic_Vol;

CFrame *LoadMDSFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer) {
    int           size;
    unsigned int *file = GetPackFile(pack, name, &size);

    if (file == NULL) {
        printf("Model NotFound!!%s\n", name);
        exit__2(-1);
    }

    return (CFrame *) LoadMDSFile(file, buffer, 0, NULL, NULL);
}

CFrame *LoadCollisionFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer) {
    int           size;
    unsigned int *file = GetPackFile(pack, name, &size);

    if (file == NULL) {
        printf("Model NotFound!!%s\n", name);
        exit__2(-1);
    }

    return (CFrame *) LoadCollisionFile(file, buffer);
}

/**
 * Puts the camera on the two named frames of a model's path.
 *
 * @mangled setCameraPassData__FP9CFrameVu1P7CCameraPcPc
 * @address 0x1B6E80
 * @size 0xA4
 */
void setCameraPassData(CFrameVu1 *frame, CCamera *camera, char *position_name, char *reference_name) {
    sceVu0FMATRIX matrix;

    // A matrix's fourth row is where the frame stands.
    frame->SearchFrame(reference_name)->GetLWMatrix(matrix);
    camera->SetRef(matrix[3]);
    frame->SearchFrame(position_name)->GetLWMatrix(matrix);
    camera->SetPos(matrix[3]);
}

/**
 * Gives the world position of one frame of a model.
 *
 * @mangled getFramePos__FP9CFrameVu1PcPf
 * @address 0x1B6F30
 * @size 0x50
 */
void getFramePos(CFrameVu1 *frame, char *name, float *position) {
    CFrame       *named_frame = frame->SearchFrame(name);
    sceVu0FVECTOR origin;

    // The position wanted is the frame's own origin.
    origin[0] = origin[1] = origin[2] = 0.0f;
    origin[3] = 0.0f;
    named_frame->GetWorldPosition(position, origin);
}

/**
 * Builds the resource name of one weapon.
 *
 * @mangled makeWeaponName__FPci
 * @address 0x1B6F80
 * @size 0x198
 */
void makeWeaponName(char *name, int weapon_no) {
    char *prefix[6] = {"c01w", "c04w", "c06w", "c05w", "c10w", "c18w"};
    int   first_weapon[6] = {ITEM_WEAPON_DAGGER_BROKEN, ITEM_WEAPON_WOODENSLINGSHOT_BROKEN, ITEM_WEAPON_MALLET_BROKEN, ITEM_WEAPON_GOLD_RING_BROKEN, ITEM_WEAPON_FIGHTING_STICK_BROKEN, ITEM_WEAPON_MACHINE_GUN_BROKEN};
    char  number[16];
    int   chara_no = 0;

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

    strcpy(name, "dun/item/main_wep/");
    strcat(name, prefix[chara_no]);
    int offset = weapon_no - first_weapon[chara_no];

    if (offset < 10) {
        sprintf(number, "0%d", offset);
    } else {
        sprintf(number, "%2d", offset);
    }

    strcat(name, number);
}

/**
 * Builds the model and texture paths of one item.
 *
 * @mangled BtGetItemNamePath__FPcPci
 * @address 0x1B7120
 * @size 0x124
 */

void BtGetItemNamePath(char *model_path, char *texture_path, int item_no) {
    item_no = TransWepNo(item_no);

    if (item_no >= ITEM_WEAPON_START) {
        makeWeaponName(model_path, item_no);
    } else {
        if (ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START] == NULL) {
            item_no = ITEM_REGULAR_WATER;
        }

        strcpy(model_path, "dun/item/main_data/");
        strcat(model_path, ITEM_NAME_TBL_NEW[item_no - ITEM_ATTACH_START]);
    }

    strcpy(texture_path, model_path);
    strcat(model_path, MdsExtension);
    strcat(texture_path, ".img");
    printf("mds = %s\n", model_path);
    printf("img = %s\n", texture_path);
}

/**
 * Model file name BtGetWeaponNamePath2 builds.
 */
char nameWepBuff_mds[64];

/**
 * Configuration file name BtGetWeaponNamePath2 builds.
 */
char nameWepBuff_img[64];

void BtGetWeaponNamePath2(char *chr_name, char *cfg_name, int chara, int weapon_index) {
    char *prefix[6] = {"c01w", "c04w", "c06w", "c05w", "c10w", "c18w"};
    char  number[32];
    char *base = prefix[chara];

    strcpy(nameWepBuff_mds, base);
    strcpy(nameWepBuff_img, base);

    if (weapon_index < 10) {
        sprintf(number, "0%d", weapon_index);
    } else {
        sprintf(number, "%2d", weapon_index);
    }

    strcat(nameWepBuff_mds, number);
    strcat(nameWepBuff_mds, ".chr");
    strcat(nameWepBuff_img, number);
    strcat(nameWepBuff_img, ".cfg");
    strcpy(chr_name, nameWepBuff_mds);
    strcpy(cfg_name, nameWepBuff_img);
}

void BtGetWeaponNamePath3(char *chr_name, char *cfg_name, int weapon_no) {
    WEAPON_DATA *weapon_data;
    int          chara_no;

    if (weapon_no < ITEM_WEAPON_START) {
        weapon_data = NULL;
    } else {
        weapon_data = GetWeaponData(weapon_no);

        if (weapon_data != NULL) {
            chara_no = (s8) weapon_data->owner;
            weapon_no -= defWeapon__2[chara_no];
            printf("offset %d\n", weapon_no);
            BtGetWeaponNamePath2(chr_name, cfg_name, chara_no, weapon_no);
        }
    }
}

/**
 * Records in the save file that an Atla has been collected.
 *
 * @mangled getAtraToSaveData__FiiP9CSaveDataii
 * @address 0x1B7470
 * @size 0xBC
 */
void getAtraToSaveData(int atra, int atra_no, CSaveData *save, int dungeon, int floor) {
    printf("GET ATRA [%d] !!\n", atra);

    if (atra < 0x28) {
        save->AtraPartsGet(dungeon, atra);
    } else {
        save->AtraChipGet(dungeon, atra - 0x28);
    }

    ((CDngStatusData *) UserStatus)->GetAtraData(dungeon, floor, atra_no);
}

/**
 * Gives how much of an attachment one item yields.
 *
 * @mangled createAttachVolume__Fii
 * @address 0x1B7530
 * @size 0xE8
 */
int createAttachVolume(int item_no, int dungeon) {
    int volume;

    if (item_no < ITEM_ATTACH_STAT_START || item_no > ITEM_ATTACH_MAGICAL_POWER) {
        return 0;
    }

    int roll = (int) ((50.0f * (float) rand()) / 2.1474836e9f);
    roll += (int) ((50.0f * (float) rand()) / 2.1474836e9f);
    volume = 1;

    if (roll < 31) {
        volume = 2;
    }

    if (roll < 16) {
        volume = 3;
    }

    return volume;
}

void BtBattleMusic_Init() {
    BtBattleMusic_Flag = 0;
    BtBattleMusic_Wait = 0;
    BtBattleMusic_Vol = 0.0f;
}

void BtBattleMusic_Stop() {
    if (BtBattleMusic_Flag != 0) {
        SndSetBgmVolf(1.0f);
        SndAmbientSetVolf(0.0f);
        BtBattleMusic_Init();
    }
}

/**
 * Crossfades between the field and battle music as the party nears a monster.
 *
 * @mangled BtBattleMusic_Excg__FfPfPf
 * @address 0x1B7690
 * @size 0x12C
 */
void BtBattleMusic_Excg(float distance, float *field_volume, float *battle_volume) {
    if (BtBattleMusic_Flag != 0) {
        if (distance >= 110.0f) {
            if (BtBattleMusic_Wait == 0) {
                BtBattleMusic_Vol -= 1.0f / 30.0f;

                if (BtBattleMusic_Vol < 0.1f) {
                    BtBattleMusic_Flag = 0;
                    BtBattleMusic_Vol = 0.0f;
                }
            } else {
                BtBattleMusic_Wait--;
            }
        } else if (BtBattleMusic_Vol < 0.9f) {
            BtBattleMusic_Vol += 1.0f / 30.0f;
        }
    }

    if (BtBattleMusic_Flag == 0 && distance <= 100.0f) {
        BtBattleMusic_Flag = 1;
    }

    if (BtBattleMusic_Vol >= 0.9f) {
        BtBattleMusic_Vol = 0.9f;
    }

    if (BtBattleMusic_Vol <= 0.0f) {
        BtBattleMusic_Vol = 0.0f;
    }

    *field_volume = 1.0f - BtBattleMusic_Vol;
    *battle_volume = BtBattleMusic_Vol;
}

/**
 * The floor number shown for each floor of the deepest dungeon.
 */
int yearFloorTbl[25] = {5, 18, 23, 38, 51, 66, 102, 109, 122, 140, 151, 162, 205,
                        208, 213, 225, 238, 249, 300, 310, 322, 340, 356, 382, 400};

int BtGetFloorLevel(int floor) {
    if (floor >= 0 && floor < 25) {
        return yearFloorTbl[floor];
    }

    return 0;
}
