#pragma once

#include <cstddef>
#include <cstdint>

#include "dataalloc.hpp"
#include "editpartsinfo.hpp"
#include "objanime.hpp"

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
