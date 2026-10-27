#pragma once

/**
 * @file
 * Declares what the game remembers about every part an edited map can hold.
 */

#include "common.h"

#include <cstddef>
#include <cstdint>

#include "dataalloc.hpp"
#include "objanime.hpp"

namespace dc::game {

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

/**
 * Catalogue of every editable part one georama map can hold, together with
 * the villager requests and the player's progress on each part.
 */
class CEditPartsInfo {
public:
    s32            parts_max;   /**< Parts the map's own catalogue holds. */
    s32            request[24]; /**< Villager request each part belongs to. */
    EDITPARTS_INFO parts[24];   /**< Every part the map can hold. */
    s32            unk_1624;

    /**
     * Returns the record of one editable map part, or nullptr.
     */
    EDITPARTS_INFO *GetPartsInfo(int index);

    /**
     * Reports whether one georama part has every element it wants.
     */
    int CheckComplete(int index);

    /**
     * Sets whether one editable part has completed its associated event.
     */
    void SetCompEvent(int index, int completed);

    /**
     * Reports whether one editable part has completed its associated event.
     */
    int GetCompEvent(int index);

    /**
     * Gives the villager request one georama part belongs to.
     */
    int GetRequest(int index);

    /**
     * Counts the georama parts after one that still have work left.
     */
    int GetNextPartsNum(int index);

    /**
     * Returns the next valid editable part after an index, or -1.
     */
    int GetNextParts(int index);

    /**
     * Empties every georama part definition.
     */
    void Clear();

    /**
     * Writes the georama part definitions into the save file.
     */
    void Save(int georama_no, CSaveData *save_data);

    /**
     * Reads the georama part definitions back out of the save file.
     */
    void Load(int georama_no, CSaveData *save_data, int load_requests);

    /**
     * Clears the georama part definitions of one map.
     */
    void Initialize(int georama_no);

    /**
     * Fills one georama part definition from its packed header.
     */
    void Initialize(int index, EPARTS_INFO_HEADER *header);
};

/** Shared editable-map part catalogue. */
extern CEditPartsInfo EditPartsInfo;

STATIC_ASSERT(sizeof(CEditPartsInfo) == 0x1628);

// Port additions.

/**
 * The header of a .pts part definition as the disc lays it out: EPARTS_INFO_HEADER with its
 * pointers stored as 4-byte offsets from the header. Every reader of a .pts goes through this.
 */
struct EPartsDiscHeader {
    std::int32_t  header_size;
    std::int32_t  data_size;
    std::int32_t  width;
    std::int32_t  height;
    std::int32_t  kind;
    std::uint8_t  unk_14[0x10];
    float         position[3];
    float         rotation[3];
    std::uint32_t cell; /**< Offset of the cell map from the header. */
    std::int32_t  element_id[6];
    std::uint32_t element_name[6]; /**< Offsets of the element names, or 0 for none. */
    std::uint32_t func;            /**< Offset of the function table from the header. */
    std::int32_t  func_count;
};

static_assert(sizeof(EPartsDiscHeader) == 0x78);
static_assert(offsetof(EPartsDiscHeader, cell) == 0x3C);
static_assert(offsetof(EPartsDiscHeader, func) == 0x70);

/**
 * One 0xC0-byte function record of a .pts definition as the disc lays it out. The host
 * EPARTS_FUNC_DATA holds an 8-byte pointer, so the host table's stride differs.
 */
struct EPartsDiscFunc {
    std::uint8_t  unk_00[0x10];
    std::int32_t  kind;
    std::uint32_t parts; /**< Disc word of the CMapParts pointer. */
    float         start_time;
    float         end_time;
    std::int32_t  link_id;
    std::int32_t  completion_flag;
    std::uint8_t  unk_28[0x8];
    char          frame_name[0x10];
    float         position[4];
    float         rotation[4];
    float         parameters[4];
    float         values[4];
    float         matrix[4][4];
};

static_assert(sizeof(EPartsDiscFunc) == 0xC0);
static_assert(offsetof(EPartsDiscFunc, position) == 0x40);

/**
 * Copies out the header of the .pts definition at `record`.
 */
EPartsDiscHeader EPartsReadHeader(const void *record);

/**
 * Copies function record `index` of the definition at `record` into the host record `out`;
 * `parts` keeps the disc word, zero-extended.
 */
void EPartsReadFunc(const void *record, int index, EPARTS_FUNC_DATA *out);

/**
 * Builds the relocated copy of the definition at host layout, carved from `arena`: the host header,
 * the record's bytes (which the cell map and element names point into) and the function table.
 */
EPARTS_INFO_HEADER *EPartsLoad(const void *record, CDataAlloc2<1> *arena);

} // namespace dc::game
