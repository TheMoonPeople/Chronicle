#include "common.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <print>
#include <string>
#include <string_view>
#include <utility>

#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataread_port.hpp"
#include "dun/gameloop.hpp"
#include "itemdata.hpp"
#include "texture.hpp"
#include "textureanime.hpp"

char  argStrBuff[640][36];
float argValBuff[640][36];
int   argLevel;
int   TEIGI_GRD_IMG[] = {0, 2, 0, 1};
int   TEIGI_FIRE_IMG[] = {1, 1, 0};

extern COM_ITEM_INFO ComItemInfo[296];
extern ATTACH_DATA   AttachList[50];
extern ITEM_DATA     ITEM_LIST[175];
extern WEAPON_DATA   WeaponList[120];

void TEIGIImgLoad(u_int *pack, CDataAlloc2<1> *arena) {
    int ground_count = 0;

    pathName[0] = '\0';
    LOADTEXTURE_INFO2 ground_images[5] = {
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0},
        {.name = "", .block_no = 0, .mipmap = 0}
    };
    LOADTEXTURE_INFO2 fire_images[3] = {
        {.name = "#blender#640#" HALF_BUFFER_HEIGHT_STR "#4", .block_no = 14, .mipmap = 0},
        {.name = "",                                          .block_no = 14, .mipmap = 0},
        {.name = "",                                          .block_no = 0,  .mipmap = 0}
    };
    LOADTEXTURE_INFO2 minimap_images[2] = {
        {.name = "", .block_no = 31, .mipmap = 0},
        {.name = "", .block_no = 0,  .mipmap = 0}
    };
    int size = 0;

    for (int i = 0; i < 4; i++) {
        TexManager.DeleteTextureBlock(i + 64);
    }
    TexManager.CleanUpTextureList();
    TexManager.CleanUpBuffer();

    for (int i = 0; i < argLevel; i++) {
        if (TEIGI_SET_PATH[0] == static_cast<int>(argValBuff[i][0])) {
            const std::string_view path_arg = argStrBuff[i];
            std::ranges::copy(path_arg, pathName);
            pathName[path_arg.size()] = '\0';
        }

        if (TEIGI_GRD_IMG[0] == static_cast<int>(argValBuff[i][0])) {
            const std::string full_path = std::string(pathName) + argStrBuff[i];
            std::memcpy(argStrBuff[i], full_path.c_str(), full_path.size() + 1);

            u_char        *destination = &arena->base[static_cast<std::ptrdiff_t>(arena->used) * 16];
            u_int         *image = GetPackFile(pack, argStrBuff[i], &size);
            DataReadSource source;
            const bool     has_source = image != nullptr && size > 0 &&
                                        DataReadSourceRange(image, static_cast<std::size_t>(size), source);
            arena->Alloc((((size >> 6) + 1) << 6) >> 4);
            std::memcpy(destination, image, static_cast<std::size_t>(size));
            if (has_source) {
                DataReadRegisterCopiedSource(destination, static_cast<std::size_t>(size), arena, source);
            }
            ground_images[ground_count].name = reinterpret_cast<char *>(destination);
            ground_images[ground_count].block_no = 3;
            ground_images[ground_count].mipmap = static_cast<int>(argValBuff[i][2]);
            ground_count++;
            ground_images[ground_count].name = nullptr;

            TexManager.DeleteTextureBlock(3);
            TexManager.CleanUpTextureList();
            TexManager.CleanUpBuffer();
            TexManager.LoadTextureBlockEX(-1, ground_images);

            BtTexAnime.Initialize(BtTexAnimeData, 96);
            char *config = reinterpret_cast<char *>(GetPackFile(pack, const_cast<char *>("texanime.cfg"), &size));
            if (config != nullptr) {
                for (int j = 0; j < 96; j++) {
                    BtTexAnimeData[j].Initialize();
                }
                BtTexAnime.LoadCFGFile(config, size);
            }
        }

        if (TEIGI_FIRE_IMG[0] == static_cast<int>(argValBuff[i][0])) {
            std::println("{}", std::string_view(argStrBuff[i]));
            fire_images[1].name = reinterpret_cast<char *>(GetPackFile(pack, const_cast<char *>("fire.img"), &size));
            if (fire_images[1].name == nullptr) {
                exit__2(-1);
            }
            TexManager.DeleteTextureBlock(14);
            TexManager.CleanUpTextureList();
            TexManager.CleanUpBuffer();
            TexManager.LoadTextureBlock(-1, fire_images);
        }

        if (TEIGI_MINIMAP_IMG[0] == static_cast<int>(argValBuff[i][0])) {
            std::string full_path = std::string(pathName) + argStrBuff[i];
            minimap_images[0].name = reinterpret_cast<char *>(GetPackFile(pack, full_path.data(), &size));
            if (minimap_images[0].name == nullptr) {
                exit__2(-1);
            }
            TexManager.DeleteTextureBlock(31);
            TexManager.CleanUpTextureList();
            TexManager.CleanUpBuffer();
            TexManager.LoadTextureBlock(-1, minimap_images);
        }
    }
}

// ComItemInfo holds items 81 to 376; PresetSmallItemNo_Get asks about weapons up to 379, so
// anything outside the table yields nullptr.
COM_ITEM_INFO *GetCommonItemInfo(int item_no) {
    if (item_no <= 0) {
        return nullptr;
    }

    // Items before slot 81 function as an alias for weapons in the info table.
    if (0 < item_no && item_no < ITEM_ATTACH_START) {
        item_no += ITEM_WEAPON_START - 1 - ITEM_ATTACH_START;
    } else {
        item_no -= ITEM_ATTACH_START;
    }

    if (std::cmp_greater_equal(item_no, std::size(ComItemInfo))) {
        return nullptr;
    }

    return &ComItemInfo[item_no];
}

// The kind and index checks stop an invalid ID or corrupt table entry from addressing a
// neighbouring item table.
int GetItemTypeInfo(int item_no, s8 *kind) {
    if (kind == nullptr) {
        return -1;
    }

    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr) {
        *kind = -1;
        return -1;
    }

    *kind = info->kind;
    return info->index;
}

ATTACH_DATA *GetAttachData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_ATTACH || info->index < 0 || std::cmp_greater_equal(info->index, std::size(AttachList))) {
        return nullptr;
    }

    return &AttachList[info->index];
}

ITEM_DATA *GetItemData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_ITEM || info->index < 0 || std::cmp_greater_equal(info->index, std::size(ITEM_LIST))) {
        return nullptr;
    }

    return &ITEM_LIST[info->index];
}

WEAPON_DATA *GetWeaponData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_WEAPON || info->index < 0 || std::cmp_greater_equal(info->index, std::size(WeaponList))) {
        return nullptr;
    }

    return &WeaponList[info->index];
}
