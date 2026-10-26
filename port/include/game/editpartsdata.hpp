#pragma once

#include "common.h"

/**
 * @file
 * Declares the Georama editor's part and element attribute tables and the Atla each dungeon hands out.
 */

/**
 * Stores one map-editor element attribute record.
 */
struct EDIT_ELEMENT_ATRA {
    s32 tex_no; /**< Cell of the element sheet that the element's icon draws from, seven to a row. */
    s32 msg_no; /**< Message of the element's name, counted from its georama's first element message. */
    s32 unk_08;
};

/**
 * Describes one optional model element in a part attribute record.
 */
struct EDIT_CHIP_ATTACH_DATA {
    s32   id;               /**< Element identifier copied into the runtime part record. */
    s32   required_element; /**< Element slot of the same part that must be enabled first, or -1 for none. */
    char *names[4];         /**< Optional model names controlled by the element. */
    s32   npc_no;           /**< Villager the element belongs to, or -1 for none. */
};

/**
 * Stores the acquisition limits for one editable part.
 */
struct EDIT_PARTS_ATRA {
    s32                   max;         /**< Most copies of the part the player can hold. */
    s32                   stackable;   /**< Greater than zero where each pickup adds five copies up to max. */
    s32                   tex_no;      /**< Cell of the part-picture sheet that the part's picture draws from. */
    s32                   msg_no;      /**< Message of the part's name, counted from its georama's first part message. */
    s32                   kind;        /**< Parts classification copied into the runtime record. */
    EDIT_CHIP_ATTACH_DATA elements[6]; /**< Optional model-element definitions. */
};

/**
 * Stores all editable part attributes for one character group.
 */
struct EDIT_PARTS_DATA {
    EDIT_PARTS_ATRA parts[25]; /**< Attribute records addressed by part number; the last is left empty. */
};

/**
 * Places one Atla that a dungeon hands out.
 */
struct ATRA_APPEAR {
    int id;    /**< Atla; -1 ends the table. */
    int floor; /**< Floor it lies on counted from one, or -1 or -2 for any upper or lower floor. */
    int count; /**< How many floors it is put on when the floor is not fixed. */
};

STATIC_ASSERT(sizeof(EDIT_ELEMENT_ATRA) == 0xC);
STATIC_ASSERT(sizeof(EDIT_CHIP_ATTACH_DATA) == 0x1C);
STATIC_ASSERT(sizeof(EDIT_PARTS_ATRA) == 0xBC);
STATIC_ASSERT(sizeof(EDIT_PARTS_DATA) == 0x125C);
STATIC_ASSERT(sizeof(ATRA_APPEAR) == 0xC);

/** The part attribute records of each georama. */
extern EDIT_PARTS_DATA EditPartsData[6];

/** The element attribute records of each georama. */
extern EDIT_ELEMENT_ATRA EditElementData[6][100];

/** The Atla the first dungeon hands out, ended by an entry whose id is -1. */
extern ATRA_APPEAR AtraAppearData0[79];

/** The Atla the second dungeon hands out, ended by an entry whose id is -1. */
extern ATRA_APPEAR AtraAppearData1[88];

/** The Atla the third dungeon hands out, ended by an entry whose id is -1. */
extern ATRA_APPEAR AtraAppearData2[72];

/** The Atla the fourth dungeon hands out, ended by an entry whose id is -1. */
extern ATRA_APPEAR AtraAppearData3[66];

/** The Atla the fifth dungeon hands out, ended by an entry whose id is -1. */
extern ATRA_APPEAR AtraAppearData4[57];

/** The Atla the sixth dungeon hands out, ended by an entry whose id is -1. */
extern ATRA_APPEAR AtraAppearData5[63];

/** The Atla table of each dungeon. */
extern ATRA_APPEAR *AtraAppearData[6];

/**
 * One external character, in the table the Georama editor's parts data ends
 * with. The window indexes it with the character's negative code, so the
 * entries sit before the address the code names.
 */
struct EDIT_GAIJI {
    s16 width; /**< How wide the character draws, in font cells. */
    u8  unk_02[0x1E];
};

STATIC_ASSERT(sizeof(EDIT_GAIJI) == 0x20);

/**
 * Table of external characters inside EditPartsData, which the linker script names.
 */
extern EDIT_GAIJI EditGaijiTbl[];
