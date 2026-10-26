#pragma once

#include "common.h"

struct EPARTS_FUNC_DATA;

/**
 * @file
 * Declares what the game remembers about every part an edited map can hold.
 */

class CSaveData;

/**
 * Describes the shape of one editable map part, as the part definition file
 * on the disc writes it.
 */
struct EPARTS_INFO_HEADER {
    s32               header_size; /**< Byte offset from the header to the packed cell map. */
    s32               data_size;   /**< Total number of bytes occupied by the packed record. */
    s32               width;       /**< Cells that the part covers from west to east. */
    s32               height;      /**< Cells that the part covers from north to south. */
    s32               kind;        /**< Parts classification copied from the source definition. */
    u8                unk_14[0x10];
    float             position[3];     /**< Where the part stands, relative to its plot. */
    float             rotation[3];     /**< Which way it faces. */
    u8               *cell;            /**< One byte per grid cell, row by row, that says what the cell is. */
    s32               element_id[6];   /**< Identifiers for the part's optional visual elements. */
    char             *element_name[6]; /**< Packed names of the part's optional visual elements. */
    EPARTS_FUNC_DATA *func;            /**< Records that say what stands on the part. */
    s32               func_count;      /**< Number of those records. */
};

/**
 * The archive one part is built from: the chunk each of its models stands in,
 * as an offset from the archive and a length.
 */
struct EPARTS_ARCHIVE {
    s32 unk_00;
    s32 info_offset; /**< Offset of the part definition from the archive. */
    u8  unk_08[0x40];
    s32 lod0_offset; /**< Offset of the level-of-detail 0 model from the archive. */
    s32 lod1_offset; /**< Offset of the level-of-detail 1 model from the archive. */
    s32 lod2_offset; /**< Offset of the level-of-detail 2 model from the archive. */
    s32 lod3_offset; /**< Offset of the level-of-detail 3 model from the archive. */
    s32 lod0_size;   /**< Length of the level-of-detail 0 model, or zero where there is none. */
    s32 lod1_size;   /**< Length of the level-of-detail 1 model, or zero where there is none. */
    s32 lod2_size;   /**< Length of the level-of-detail 2 model, or zero where there is none. */
    s32 lod3_size;   /**< Length of the level-of-detail 3 model, or zero where there is none. */
    u8  unk_68[0x10];
    s32 collision_offset; /**< Offset of the collision model from the archive. */
    s32 collision_size;   /**< Length of the collision model, or zero where there is none. */
    u8  unk_80[0x10];
    s32 shadow_offset; /**< Offset of the shadow model from the archive. */
    s32 shadow_size;   /**< Length of the shadow model, or zero where there is none. */
    u8  unk_98[0x10];
    s32 shade_offset; /**< Offset of the shade model from the archive. */
    s32 shade_size;   /**< Length of the shade model, or zero where there is none. */
    u8  unk_b0[0x10];
    s32 camera_offset; /**< Offset of the camera-collision model from the archive. */
    s32 camera_size;   /**< Length of the camera-collision model, or zero where there is none. */
    u8  unk_c8[0x10];
    s32 extra_offset; /**< Offset of the extra model from the archive. */
    s32 extra_size;   /**< Length of the extra model, or zero where there is none. */
};

STATIC_ASSERT(sizeof(EPARTS_ARCHIVE) == 0xE0);

/**
 * Describes one optional visual element attached to an editable part.
 */
struct EDITPARTS_ELEMENT {
    s32   id;               /**< Element identifier, or a negative value when the slot is unused. */
    s32   required_element; /**< Element slot of the same part that must be enabled first, or -1 for none. */
    s32   enabled;          /**< Whether the element's ordinary object names are visible. */
    char *names[4];         /**< Optional object names controlled by this element. */
    s32   npc_no;           /**< Villager the element belongs to, or -1 for none. */
};

STATIC_ASSERT(sizeof(EDITPARTS_ELEMENT) == 0x20);

/**
 * Holds the unpacked grid and visual-element names read from a parts definition.
 */
struct INIT_PARTSINFO {
    s32  width;                 /**< Cells that the source part covers from west to east. */
    s32  height;                /**< Cells that the source part covers from north to south. */
    u8   cell[32][2];           /**< Source grid cells, whose low bytes form the packed grid. */
    s32  kind;                  /**< Parts classification copied into the packed header. */
    s32  element_id[6];         /**< Identifiers for the optional visual elements. */
    char element_name[6][0x20]; /**< Names belonging to the optional visual elements. */
};

/**
 * Records what one editable map part is and how far the player has taken it.
 */
struct EDITPARTS_INFO {
    s32                 parts_no;         /**< Number that names the part. */
    s32                 completion_flags; /**< Bit zero records whether the completion event has run. */
    s32                 obtained;         /**< Nonzero once the player has picked up the part. */
    s32                 placed;           /**< Copies of the part standing on the map. */
    s32                 tex_no;           /**< Cell of the part-picture sheet that the part's picture draws from. */
    s32                 kind;             /**< What the plot holds: 2 where a story building stands on it. */
    s32                 stock;            /**< Copies of the part the player may place. */
    s32                 width;            /**< Cells that the part covers from west to east. */
    s32                 height;           /**< Cells that the part covers from north to south. */
    EDITPARTS_ELEMENT   elements[6];      /**< Visibility definitions for optional model elements. */
    EPARTS_INFO_HEADER *header;           /**< Shape of the part. */
};

STATIC_ASSERT(sizeof(EDITPARTS_INFO) == 0xE8);

class CEditPartsInfo {
public:
    s32            parts_max;   /**< Parts the map's own catalogue holds. */
    s32            request[24]; /**< Villager request each part belongs to. */
    EDITPARTS_INFO parts[24];   /**< Every part the map can hold. */
    s32            unk_1624;

    /**
     * Returns the record of one editable map part, or NULL.
     *
     * @mangled GetPartsInfo__14CEditPartsInfoFi
     * @address 0x199FD0
     * @size 0x44
     */
    EDITPARTS_INFO *GetPartsInfo(int index);

    /**
     * Reports whether one georama part has every element it wants.
     *
     * @mangled CheckComplete__14CEditPartsInfoFi
     * @address 0x19A020
     * @size 0x80
     */
    int CheckComplete(int index);

    /**
     * Sets whether one editable part has completed its associated event.
     *
     * @mangled SetCompEvent__14CEditPartsInfoFii
     * @address 0x19A0A0
     * @size 0x60
     */
    void SetCompEvent(int index, int completed);

    /**
     * Reports whether one editable part has completed its associated event.
     *
     * @mangled GetCompEvent__14CEditPartsInfoFi
     * @address 0x19A100
     * @size 0x40
     */
    int GetCompEvent(int index);

    /**
     * Gives the villager request one georama part belongs to.
     *
     * @mangled GetRequest__14CEditPartsInfoFi
     * @address 0x19A140
     * @size 0x38
     */
    int GetRequest(int index);

    /**
     * Counts the georama parts after one that still have work left.
     *
     * @mangled GetNextPartsNum__14CEditPartsInfoFi
     * @address 0x19A180
     * @size 0x88
     */
    int GetNextPartsNum(int index);

    /**
     * Returns the next valid editable part after an index, or -1.
     *
     * @mangled GetNextParts__14CEditPartsInfoFi
     * @address 0x19A210
     * @size 0x70
     */
    int GetNextParts(int index);

    /**
     * Empties every georama part definition.
     *
     * @mangled Clear__14CEditPartsInfoFv
     * @address 0x19A280
     * @size 0x40
     */
    void Clear();

    /**
     * Writes the georama part definitions into the save file.
     *
     * @mangled Save__14CEditPartsInfoFiP9CSaveData
     * @address 0x19A2C0
     * @size 0x128
     */
    void Save(int georama_no, CSaveData *save_data);

    /**
     * Reads the georama part definitions back out of the save file.
     *
     * @mangled Load__14CEditPartsInfoFiP9CSaveDatai
     * @address 0x19A3F0
     * @size 0x178
     */
    void Load(int georama_no, CSaveData *save_data, int load_requests);

    /**
     * Clears the georama part definitions of one map.
     *
     * @mangled Initialize__14CEditPartsInfoFi
     * @address 0x19A570
     * @size 0x1C8
     */
    void Initialize(int georama_no);

    /**
     * Fills one georama part definition from its packed header.
     *
     * @mangled Initialize__14CEditPartsInfoFiP18EPARTS_INFO_HEADER
     * @address 0x19A740
     * @size 0x64
     */
    void Initialize(int index, EPARTS_INFO_HEADER *header);

private:
};

/** Shared editable-map part catalogue. */
extern CEditPartsInfo EditPartsInfo;

STATIC_ASSERT(sizeof(CEditPartsInfo) == 0x1628);
