#include "common.h"

#include <cstring>

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
    char full_path[256];
    int  ground_count = 0;

    std::strcpy(pathName, "");
    LOADTEXTURE_INFO2 ground_images[5] = {
        {"", 0, 0},
        {"", 0, 0},
        {"", 0, 0},
        {"", 0, 0},
        {"", 0, 0}
    };
    LOADTEXTURE_INFO2 fire_images[3] = {
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4", 14, 0},
        {"",                                          14, 0},
        {"",                                          0,  0}
    };
    LOADTEXTURE_INFO2 minimap_images[2] = {
        {"", 31, 0},
        {"", 0,  0}
    };
    int size;

    for (int i = 0; i < 4; i++) {
        TexManager.DeleteTextureBlock(i + 64);
    }
    TexManager.CleanUpTextureList();
    TexManager.CleanUpBuffer();

    for (int i = 0; i < argLevel; i++) {
        if (TEIGI_SET_PATH[0] == static_cast<int>(argValBuff[i][0])) {
            std::strcpy(pathName, argStrBuff[i]);
        }

        if (TEIGI_GRD_IMG[0] == static_cast<int>(argValBuff[i][0])) {
            std::strcpy(full_path, pathName);
            std::strcat(full_path, argStrBuff[i]);
            std::strcpy(argStrBuff[i], full_path);

            u_char        *destination = arena->used * 16 + arena->base;
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
            std::strcpy(full_path, pathName);
            std::strcat(full_path, argStrBuff[i]);
            std::printf("%s\n", argStrBuff[i]);
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
            std::strcpy(full_path, pathName);
            std::strcat(full_path, argStrBuff[i]);
            minimap_images[0].name = reinterpret_cast<char *>(GetPackFile(pack, full_path, &size));
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

/* ComItemInfo holds items 81 to 376. Retail indexes it with no check, and PresetSmallItemNo_Get
   asks about weapons up to 379, which reads the words after the table; the port answers nothing
   for an item the table does not hold. */
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

    if (item_no >= 296) {
        return nullptr;
    }

    return &ComItemInfo[item_no];
}

/* The retail accessors trust the kind and index in ComItemInfo. Keep both checks
   here so an invalid ID or a corrupt table entry cannot address a neighbouring
   item table (or masquerade as an attachment). */
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
    if (info == nullptr || info->kind != ITEMKIND_ATTACH || info->index < 0 || info->index >= 50) {
        return nullptr;
    }

    return &AttachList[info->index];
}

ITEM_DATA *GetItemData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_ITEM || info->index < 0 || info->index >= 175) {
        return nullptr;
    }

    return &ITEM_LIST[info->index];
}

WEAPON_DATA *GetWeaponData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == nullptr || info->kind != ITEMKIND_WEAPON || info->index < 0 || info->index >= 120) {
        return nullptr;
    }

    return &WeaponList[info->index];
}
