#include "common.h"

#include <cstring>

#include "itemdata.hpp"

extern COM_ITEM_INFO ComItemInfo[296];
extern ATTACH_DATA AttachList[50];
extern ITEM_DATA ITEM_LIST[175];
extern WEAPON_DATA WeaponList[120];

/* ComItemInfo holds items 81 to 376. Retail indexes it with no check, and PresetSmallItemNo_Get
   asks about weapons up to 379, which reads the words after the table; the port answers nothing
   for an item the table does not hold. */
PC_OVERRIDE COM_ITEM_INFO *GetCommonItemInfo(int item_no) {
    if (item_no <= 0) {
        return 0;
    }

    // Items before slot 81 function as an alias for weapons in the info table.
    if (0 < item_no && item_no < ITEM_ATTACH_START) {
        item_no += ITEM_WEAPON_START - 1 - ITEM_ATTACH_START;
    } else {
        item_no -= ITEM_ATTACH_START;
    }

    if (item_no >= 296) {
        return 0;
    }

    return &ComItemInfo[item_no];
}

/* The retail accessors trust the kind and index in ComItemInfo. Keep both checks
   here so an invalid ID or a corrupt table entry cannot address a neighbouring
   item table (or masquerade as an attachment). */
PC_OVERRIDE int GetItemTypeInfo(int item_no, s8 *kind) {
    if (kind == 0) {
        return -1;
    }

    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == 0) {
        *kind = -1;
        return -1;
    }

    *kind = info->kind;
    return info->index;
}

PC_OVERRIDE ATTACH_DATA *GetAttachData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == 0 || info->kind != ITEMKIND_ATTACH || info->index < 0 || info->index >= 50) {
        return 0;
    }

    return &AttachList[info->index];
}

PC_OVERRIDE ITEM_DATA *GetItemData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == 0 || info->kind != ITEMKIND_ITEM || info->index < 0 || info->index >= 175) {
        return 0;
    }

    return &ITEM_LIST[info->index];
}

PC_OVERRIDE WEAPON_DATA *GetWeaponData(int item_no) {
    COM_ITEM_INFO *info = GetCommonItemInfo(item_no);
    if (info == 0 || info->kind != ITEMKIND_WEAPON || info->index < 0 || info->index >= 120) {
        return 0;
    }

    return &WeaponList[info->index];
}

PC_OVERRIDE void AttachDataListToHaveCopy(int attachment_no, ATTACH_LIST *attachment) {
    if (attachment == 0) {
        return;
    }

    ATTACH_DATA *data = GetAttachData(attachment_no);
    if (data != 0) {
        memcpy(attachment, data, sizeof(*attachment));
    }
}
