#include "menu_misc.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "battlemenu.hpp"
#include "bt_shot_effect.hpp"
#include "btactstatus.hpp"
#include "btmisc.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "editloop.hpp"
#include "frame.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_dungeon.hpp"
#include "menu_inventory.hpp"
#include "menu_manual.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "shot_effect.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "userstatus.hpp"
#include "weapon_buildup.hpp"
#include "weaponeffect.hpp"
#include "weaponlevelup.hpp"

// Retail's EnterWeaponModel keeps the address of each weapon's .chr in MenuWeaponModelData, an int
// table, and WeaponModelBuildFunc and DngWeaponEquipModelBuild read its entries back as u_int *,
// eight bytes at a four-byte stride. The three keep the addresses in a table of pointers instead.

// The bodies are retail's, spelled as MWCC took them.
#pragma clang diagnostic ignored "-Wwritable-strings"
#pragma clang diagnostic ignored "-Wchar-subscripts"

namespace {

u_int *weapon_model_files[42];

u_int **GetMenuWeaponModelData(int index) {
    return &weapon_model_files[index];
}

void InitMenuWeaponModelData() {
    memset(MenuWeaponModelData, 0, sizeof(MenuWeaponModelData));
    memset(weapon_model_files, 0, sizeof(weapon_model_files));
}

int *GetMenuWeaponModelInfo(int index) {
    return MenuWeaponModelInfo[index];
}

void SetWepEffectMenuReadBuf(u_long128 *buffer) {
    WepEffectMenuReadBuf = buffer;
}

} // namespace

PC_OVERRIDE int EnterWeaponModel(int chara, int texture_block, int weapon_slot) {
    BG_READ_INFO *pack = GetReadBGFile(0);
    BG_READ_INFO *shadow = GetReadBGFile(1);
    BG_READ_INFO *effect = GetReadBGFile(2);
    // Built and indexed but never read.
    char *shadows[6] = {"kagetoan", "kagesyao", "kagegoro", "kageruby", "kageunga", "kageozu"};
    [[maybe_unused]] char *shadow_name = shadows[chara];
    s16   weapon_max = MenuCharaWeaponMax[chara];
    char *names[6] = {"c01", "c04", "c06", "c05", "c10", "c18"};
    char  order[42];
    char  image[32];
    char  model[32];
    char  chr[32];

    switch (GetNowTestNo()) {
        case 0:
            break;
        case 1: {
            LOADTEXTURE_INFO2 texture[3] = {
                {"#frame_menuwep_dmy#640#" SCREEN_HEIGHT_STR "#4", 0, 0},
                {NULL,                                             0, 0},
                {NULL,                                             0, 0},
            };
            texture[0].block_no = MenuShadowReadBlock;
            texture[1].block_no = MenuShadowReadBlock;
            texture[1].name = (char *) shadow->buffer;
            TexManager.DeleteTextureBlock(MenuShadowReadBlock);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, texture);
            WepIcon = TexManager.GetTexture("wepicon", MenuShadowReadBlock);

            for (int i = 0; i < weapon_max; i++) {
                order[i] = i;
            }

            InitMenuWeaponModelData();

            for (int i = 0; i < weapon_max; i++) {
                char *name = names[chara];
                strcpy(image, name);
                strcpy(model, name);
                strcpy(chr, name);
                int weapon_no = order[i];

                if (0 <= weapon_no && weapon_no <= 9) {
                    strcat(chr, "w0%d");
                    sprintf(chr, chr, weapon_no);
                    strcat(image, "w0%d");
                    sprintf(image, image, weapon_no);
                    strcat(model, "w0%d");
                    sprintf(model, model, weapon_no);
                } else if (weapon_no >= 10) {
                    strcat(chr, "w%d");
                    sprintf(chr, chr, weapon_no);
                    strcat(image, "w%d");
                    sprintf(image, image, weapon_no);
                    strcat(model, "w%d");
                    sprintf(model, model, weapon_no);
                } else {
                    strcat(chr, "w01");
                    strcat(image, "w01");
                    strcat(model, "w01d");
                }

                strcat(chr, ".chr");
                strcat(image, ".img");
                strcat(model, ".mds");
                u_int *file = GetPackFile((u_int *) pack->buffer, chr, NULL);

                if (file != NULL) {
                    *GetMenuWeaponModelData(i) = file;
                }
            }

            u_long128 *build;

            if (effect != NULL) {
                SetWepEffectMenuReadBuf(effect->buffer);
                build = effect->buffer + 0x2D01;
            } else {
                build = shadow->buffer + (shadow->size >> 4) + 1;
            }

            MenuWeaponModelBuildBuffer = build;
            WeaponModelBuildFunc(chara, texture_block);
            break;
        }
    }

    return 1;
}

PC_OVERRIDE void WeaponModelBuildFunc(int chara, int texture_block) {
    printf("weapon model build func start\n");
    InitMenuWeaponModelReference();
    LOADTEXTURE_INFO2 textures[] = {
        {(char *) "#frame_menuwep#640#" SCREEN_HEIGHT_STR "#4", texture_block, 0},
        {NULL,                                                  0,             0},
    };
    char name[32];
    char cfg[32];
    TexManager.DeleteTextureBlock(texture_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, textures);
    printf("modelbuildbuffer = %p\n", MenuWeaponModelBuildBuffer);
    MenuExCashBuffer.base = (u_char *) MenuWeaponModelBuildBuffer;
    MenuExCashBuffer.limit = 0xEC00;
    MenuExCashBuffer.used = 0;
    int          default_no;
    WEAPON_HAVE *weapons = ((CUserStatus *) BtlMenuStatusPt)->chara_weapons[chara];
    default_no = GetDefaultWeaponNo(chara);
    int     next = 2;
    u_int **data = GetMenuWeaponModelData(0);
    BtGetWeaponNamePath2(name, cfg, chara, 0);
    DngWeaponFrm[0].LoadPackData3(*data, cfg, &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);
    data = GetMenuWeaponModelData(1);
    BtGetWeaponNamePath2(name, cfg, chara, 1);
    DngWeaponFrm[1].LoadPackData3(*data, cfg, &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);

    for (int i = 0; i < 10; i++) {
        WEAPON_HAVE *weapon = &weapons[i];

        if (weapon == NULL) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

        int item_no = weapon->item_no;

        if (item_no < ITEM_WEAPON_START) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

        unsigned int kind = item_no - default_no;

        if (kind < 2U) {
            SetMenuWeaponModelReference(i, kind, kind);
            continue;
        }

        int found = 0;

        for (int j = 0; j < 10; j++) {
            int *info = GetMenuWeaponModelInfo(j);

            if (info[1] == kind) {
                found = 1;
                SetMenuWeaponModelReference(i, info[0], info[1]);
                break;
            }
        }

        if (found == 0) {
            u_int **pack = GetMenuWeaponModelData(kind);

            if (*pack == NULL) {
                printf("%d pack data is NULL\n", kind);
            } else {
                BtGetWeaponNamePath2(name, cfg, chara, kind);
                DngWeaponFrm[next].LoadPackData3(*pack, cfg, &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);
                SetMenuWeaponModelReference(i, next, kind);
                next++;
            }
        }
    }

    WepMenuEffectReadBuf = (CWeaponLevelUp *) (MenuWeaponModelBuildBuffer + 0xEC01);
    WepMenuEffectReadBuf = (CWeaponLevelUp *) MenuCalcBufAlignment((u_long128 *) WepMenuEffectReadBuf);
    printf("read buffer           = %p\n", read_buffer);
    printf("model build buffer    = %p\n", MenuWeaponModelBuildBuffer);
    printf("WeaponBuffer Size     = %d\n", (int) MenuExCashBuffer.limit);
    printf("WeaponBuffer address  = %p\n", MenuExCashBuffer.base + MenuExCashBuffer.used * 16);
    printf("WepMenuEffectReadBuf = %p\n", WepMenuEffectReadBuf);
}

PC_OVERRIDE int DngWeaponEquipModelBuild(int chara, int texture_block, u_long128 *read_buffer) {
    TexManager.DeleteTextureBlock(texture_block);
    u_int **first = GetMenuWeaponModelData(0);
    u_int **second = GetMenuWeaponModelData(1);
    int     kind = 0;

    if (UserStatus != NULL) {
        kind = UserStatus->chara_weapons[chara][UserStatus->equipped_weapon_slot[chara]].item_no;
        kind -= GetDefaultWeaponNo(chara);
    }

    u_int **equipped = GetMenuWeaponModelData(kind);

    if (equipped == NULL) {
        equipped = second;
    } else if (*equipped == NULL) {
        *equipped = *second;
    }

    LoadWeapon2(*first, *second, *equipped, chara, 1);
    MenuWeaponEffectSet(1);
    return 1;
}
