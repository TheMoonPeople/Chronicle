#include "menu_misc.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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

std::pair<std::string, std::string> WeaponNames(int chara, int weapon_index) {
    constexpr const char *prefixes[] = {"c01w", "c04w", "c06w", "c05w", "c10w", "c18w"};
    if (chara < 0 || chara >= static_cast<int>(std::size(prefixes)) || weapon_index < 0) {
        return {};
    }
    std::string number = std::to_string(weapon_index);
    if (weapon_index < 10) {
        number.insert(0, "0");
    } else if (weapon_index < 100) {
        number.insert(0, " ");
    }
    const std::string base = std::string(prefixes[chara]) + number;
    return {base + ".chr", base + ".cfg"};
}

void SetWepEffectMenuReadBuf(u_long128 *buffer) {
    WepEffectMenuReadBuf = buffer;
}

} // namespace

PORT_OVERRIDE int EnterWeaponModel(int chara, int texture_block, int weapon_slot) {
    if (chara < 0 || chara >= 6) {
        return 1;
    }
    BG_READ_INFO *pack = GetReadBGFile(0);
    BG_READ_INFO *shadow = GetReadBGFile(1);
    BG_READ_INFO *effect = GetReadBGFile(2);
    // Built and indexed but never read.
    char                  *shadows[6] = {"kagetoan", "kagesyao", "kagegoro", "kageruby", "kageunga", "kageozu"};
    [[maybe_unused]] char *shadow_name = shadows[chara];
    const int              weapon_max = std::clamp<int>(MenuCharaWeaponMax[chara], 0, static_cast<int>(std::size(weapon_model_files)));
    char                  *names[6] = {"c01", "c04", "c06", "c05", "c10", "c18"};
    std::vector<int>       order(static_cast<std::size_t>(weapon_max));

    switch (GetNowTestNo()) {
        case 0:
            break;
        case 1: {
            auto texture = std::make_unique<LOADTEXTURE_INFO2[]>(3);
            texture[0] = {"#frame_menuwep_dmy#640#" SCREEN_HEIGHT_STR "#4", 0, 0};
            texture[1] = {nullptr, 0, 0};
            texture[2] = {nullptr, 0, 0};
            texture[0].block_no = MenuShadowReadBlock;
            texture[1].block_no = MenuShadowReadBlock;
            texture[1].name = reinterpret_cast<char *>(shadow->buffer);
            TexManager.DeleteTextureBlock(MenuShadowReadBlock);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, texture.get());
            WepIcon = TexManager.GetTexture("wepicon", MenuShadowReadBlock);

            for (int i = 0; i < weapon_max; i++) {
                order[i] = i;
            }

            InitMenuWeaponModelData();

            for (int i = 0; i < weapon_max; i++) {
                const char *name = names[chara];
                int         weapon_no = order[i];
                std::string chr = name;

                if (0 <= weapon_no && weapon_no <= 9) {
                    chr += "w0" + std::to_string(weapon_no);
                } else if (weapon_no >= 10) {
                    chr += "w" + std::to_string(weapon_no);
                } else {
                    chr += "w01";
                }

                chr += ".chr";
                u_int *file = GetPackFile(reinterpret_cast<u_int *>(pack->buffer), chr.data(), nullptr);

                if (file != nullptr) {
                    *GetMenuWeaponModelData(i) = file;
                }
            }

            u_long128 *build;

            if (effect != nullptr) {
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

PORT_OVERRIDE void WeaponModelBuildFunc(int chara, int texture_block) {
    if (chara < 0 || chara >= 6 || BtlMenuStatusPt == nullptr) {
        return;
    }
    printf("weapon model build func start\n");
    InitMenuWeaponModelReference();
    LOADTEXTURE_INFO2 textures[] = {
        {const_cast<char *>("#frame_menuwep#640#" SCREEN_HEIGHT_STR "#4"), texture_block, 0},
        {nullptr,                                                          0,             0},
    };
    TexManager.DeleteTextureBlock(texture_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, textures);
    printf("modelbuildbuffer = %p\n", MenuWeaponModelBuildBuffer);
    MenuExCashBuffer.base = reinterpret_cast<u_char *>(MenuWeaponModelBuildBuffer);
    MenuExCashBuffer.limit = 0xEC00;
    MenuExCashBuffer.used = 0;
    int          default_no;
    WEAPON_HAVE *weapons = static_cast<CUserStatus *>(BtlMenuStatusPt)->chara_weapons[chara];
    default_no = GetDefaultWeaponNo(chara);
    int     next = 2;
    u_int **data = GetMenuWeaponModelData(0);
    auto [name, cfg] = WeaponNames(chara, 0);
    DngWeaponFrm[0].LoadPackData3(*data, cfg.data(), &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);
    data = GetMenuWeaponModelData(1);
    std::tie(name, cfg) = WeaponNames(chara, 1);
    DngWeaponFrm[1].LoadPackData3(*data, cfg.data(), &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);

    for (int i = 0; i < 10; i++) {
        WEAPON_HAVE *weapon = &weapons[i];

        if (weapon == nullptr) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

        int item_no = weapon->item_no;

        if (item_no < ITEM_WEAPON_START) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

        unsigned int kind = item_no - default_no;

        if (item_no < default_no || kind >= std::size(weapon_model_files)) {
            SetMenuWeaponModelReference(i, -2, -1);
            continue;
        }

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

            if (*pack == nullptr) {
                printf("%d pack data is NULL\n", kind);
            } else {
                std::tie(name, cfg) = WeaponNames(chara, static_cast<int>(kind));
                DngWeaponFrm[next].LoadPackData3(*pack, cfg.data(), &MenuExCashBuffer, texture_block, &MenuExCashBuffer, 1, 0);
                SetMenuWeaponModelReference(i, next, kind);
                next++;
            }
        }
    }

    WepMenuEffectReadBuf = reinterpret_cast<CWeaponLevelUp *>(MenuWeaponModelBuildBuffer + 0xEC01);
    WepMenuEffectReadBuf = reinterpret_cast<CWeaponLevelUp *>(MenuCalcBufAlignment(reinterpret_cast<u_long128 *>(WepMenuEffectReadBuf)));
    printf("read buffer           = %p\n", read_buffer);
    printf("model build buffer    = %p\n", MenuWeaponModelBuildBuffer);
    printf("WeaponBuffer Size     = %d\n", MenuExCashBuffer.limit);
    printf("WeaponBuffer address  = %p\n", MenuExCashBuffer.base + MenuExCashBuffer.used * 16);
    printf("WepMenuEffectReadBuf = %p\n", WepMenuEffectReadBuf);
}

PORT_OVERRIDE int DngWeaponEquipModelBuild(int chara, int texture_block, u_long128 *read_buffer) {
    if (chara < 0 || chara >= 6) {
        return 0;
    }
    TexManager.DeleteTextureBlock(texture_block);
    u_int **first = GetMenuWeaponModelData(0);
    u_int **second = GetMenuWeaponModelData(1);
    int     kind = 0;

    if (UserStatus != nullptr) {
        const int slot = UserStatus->equipped_weapon_slot[chara];
        if (slot < 0 || slot >= 10) {
            return 0;
        }
        kind = UserStatus->chara_weapons[chara][slot].item_no;
        kind -= GetDefaultWeaponNo(chara);
        if (kind < 0 || kind >= static_cast<int>(std::size(weapon_model_files))) {
            return 0;
        }
    }

    u_int **equipped = GetMenuWeaponModelData(kind);

    if (equipped == nullptr) {
        equipped = second;
    } else if (*equipped == nullptr) {
        *equipped = *second;
    }

    LoadWeapon2(*first, *second, *equipped, chara, 1);
    MenuWeaponEffectSet(1);
    return 1;
}
