#include "menu_misc.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <print>
#include <string>
#include <string_view>
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

namespace {

// Each weapon's .chr address, kept here rather than in the int-sized MenuWeaponModelData table.
std::array<u_int *, 42> weapon_model_files;

u_int **GetMenuWeaponModelData(int index) {
    return &weapon_model_files[index];
}

void InitMenuWeaponModelData() {
    memset(MenuWeaponModelData, 0, sizeof(MenuWeaponModelData));
    weapon_model_files.fill(nullptr);
}

int *GetMenuWeaponModelInfo(int index) {
    return MenuWeaponModelInfo[index];
}

std::pair<std::string, std::string> WeaponNames(int chara, int weapon_index) {
    constexpr std::array<std::string_view, 6> prefixes = {"c01w", "c04w", "c06w", "c05w", "c10w", "c18w"};
    if (chara < 0 || std::cmp_greater_equal(chara, prefixes.size()) || weapon_index < 0) {
        return {};
    }
    std::string number = std::to_string(weapon_index);
    if (weapon_index < 10) {
        number.insert(0, "0");
    }
    const std::string base = std::string(prefixes[chara]) + number;
    return {base + ".chr", base + ".cfg"};
}

void SetWepEffectMenuReadBuf(u_long128 *buffer) {
    WepEffectMenuReadBuf = buffer;
}

} // namespace

int EnterWeaponModel(int chara, int texture_block, [[maybe_unused]] int weapon_slot) {
    if (chara < 0 || chara >= 6) {
        return 1;
    }
    BG_READ_INFO                             *pack = GetReadBGFile(0);
    BG_READ_INFO                             *shadow = GetReadBGFile(1);
    BG_READ_INFO                             *effect = GetReadBGFile(2);
    const int                                 weapon_max = std::clamp<int>(MenuCharaWeaponMax[chara], 0, static_cast<int>(std::size(weapon_model_files)));
    constexpr std::array<std::string_view, 6> names = {"c01", "c04", "c06", "c05", "c10", "c18"};
    std::vector<int>                          order(static_cast<std::size_t>(weapon_max));

    switch (GetNowTestNo()) {
        case 0:
            break;
        case 1: {
            auto texture = std::make_unique<LOADTEXTURE_INFO2[]>(3);
            texture[0] = {.name = const_cast<char *>("#frame_menuwep_dmy#640#" SCREEN_HEIGHT_STR "#4"), .block_no = 0, .mipmap = 0};
            texture[1] = {.name = nullptr, .block_no = 0, .mipmap = 0};
            texture[2] = {.name = nullptr, .block_no = 0, .mipmap = 0};
            texture[0].block_no = MenuShadowReadBlock;
            texture[1].block_no = MenuShadowReadBlock;
            texture[1].name = reinterpret_cast<char *>(shadow->buffer);
            TexManager.DeleteTextureBlock(MenuShadowReadBlock);
            TexManager.CleanUpTextureList();
            TexManager.LoadTextureBlockEX(-1, texture.get());
            WepIcon = TexManager.GetTexture(const_cast<char *>("wepicon"), MenuShadowReadBlock);

            for (int i = 0; i < weapon_max; i++) {
                order[i] = i;
            }

            InitMenuWeaponModelData();

            for (int i = 0; i < weapon_max; i++) {
                const int   weapon_no = order[i];
                std::string chr{names[chara]};

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

            u_long128 *build = nullptr;

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
        default:
            break;
    }

    return 1;
}

void WeaponModelBuildFunc(int chara, int texture_block) {
    if (chara < 0 || chara >= 6 || BtlMenuStatusPt == nullptr) {
        return;
    }
    std::println("weapon model build func start");
    InitMenuWeaponModelReference();
    std::array<LOADTEXTURE_INFO2, 2> textures = {
        LOADTEXTURE_INFO2{.name = const_cast<char *>("#frame_menuwep#640#" SCREEN_HEIGHT_STR "#4"), .block_no = texture_block, .mipmap = 0},
        LOADTEXTURE_INFO2{.name = nullptr,                                                          .block_no = 0,             .mipmap = 0},
    };
    TexManager.DeleteTextureBlock(texture_block);
    TexManager.CleanUpTextureList();
    TexManager.LoadTextureBlockEX(-1, textures.data());
    printf("modelbuildbuffer = %p\n", MenuWeaponModelBuildBuffer);
    MenuExCashBuffer.base = reinterpret_cast<u_char *>(MenuWeaponModelBuildBuffer);
    MenuExCashBuffer.limit = 0xEC00;
    MenuExCashBuffer.used = 0;
    WEAPON_HAVE *weapons = static_cast<CUserStatus *>(BtlMenuStatusPt)->chara_weapons[chara];
    const int    default_no = GetDefaultWeaponNo(chara);
    int          next = 2;
    u_int      **data = GetMenuWeaponModelData(0);
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

        bool found = false;

        for (int j = 0; j < 10; j++) {
            int *info = GetMenuWeaponModelInfo(j);

            if (std::cmp_equal(info[1], kind)) {
                found = true;
                SetMenuWeaponModelReference(i, info[0], info[1]);
                break;
            }
        }

        if (!found) {
            u_int **pack = GetMenuWeaponModelData(kind);

            if (*pack == nullptr) {
                std::println("{} pack data is NULL", kind);
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
    std::println("WeaponBuffer Size     = {}", MenuExCashBuffer.limit);
    printf("WeaponBuffer address  = %p\n", MenuExCashBuffer.base + static_cast<std::ptrdiff_t>(MenuExCashBuffer.used) * 16);
    printf("WepMenuEffectReadBuf = %p\n", WepMenuEffectReadBuf);
}

int DngWeaponEquipModelBuild(int chara, int texture_block, [[maybe_unused]] u_long128 *read_buffer) {
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
        if (kind < 0 || std::cmp_greater_equal(kind, weapon_model_files.size())) {
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
