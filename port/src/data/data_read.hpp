#pragma once

#include "common.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dc::data {

/**
 * The languages the extracted disc carries, as LanguageCode numbers, from the languages.json dcdata
 * writes for its release (dcdata::ReleaseOf); none when the data has no such file.
 */
std::vector<s32> DataLanguages();

/**
 * The languages the game can offer: the disc's own. A localization pack, once the port has them,
 * adds the languages it carries here.
 */
std::vector<s32> SupportedLanguages();

/**
 * The size of a file of the game's data ("gedit/e01/chara/p13a.chr"), as the loaders find it; 0 for
 * one the data does not have.
 */
std::uintmax_t DataFileSize(std::string_view path);

/**
 * Lists the logical paths of every file in the indexed game data.
 */
std::vector<std::string> DataReadListFiles();

/**
 * Reads a logical data file into bytes before in-memory localization is applied; false (and bytes
 * cleared) when the file is missing or cannot be read.
 */
bool DataReadReadFile(std::string_view path, std::vector<std::uint8_t> &bytes);

/**
 * Reads the one indexed file whose source path matches source_path; false (and bytes cleared) when
 * none or more than one matches, or it cannot be read.
 */
bool DataReadReadResolvedFile(std::string_view source_path, std::vector<std::uint8_t> &bytes);

/**
 * Counts of texture files read from the archive and from loose files, while tracking is enabled.
 */
struct DataTextureReadStats {
    std::uint64_t            archived_json = 0; /**< Texture .json files read from the archive. */
    std::uint64_t            archived_png = 0;  /**< Texture .png files read from the archive. */
    std::uint64_t            loose_json = 0;    /**< Texture .json files read from loose files. */
    std::uint64_t            loose_png = 0;     /**< Texture .png files read from loose files. */
    std::vector<std::string> archived_sources;  /**< The first few archive sources textures came from. */
    std::vector<std::string> loose_sources;     /**< The first few loose sources textures came from. */
};

/**
 * Returns a snapshot of the texture read counts gathered since tracking was last enabled.
 */
DataTextureReadStats DataReadTextureReadStats();

/**
 * Resets the texture read counts and turns their tracking on or off.
 */
void DataReadSetTextureReadStatsEnabled(bool enabled);

/**
 * Where a range of loaded bytes came from: the indexed asset and, for a pack, its member.
 */
struct DataReadSource {
    std::string                path;                  /**< Logical path of the indexed asset. */
    std::string                member;                /**< Nested pack member path, empty for a whole file. */
    std::vector<std::uint64_t> member_offsets;        /**< Offset of each nested member within its parent. */
    std::vector<std::uint64_t> member_source_offsets; /**< Offset of each nested member within the source file. */
    std::vector<std::uint64_t> member_sizes;          /**< Size of each nested member. */
    std::uint64_t              offset = 0;            /**< Offset of the requested bytes in the source file. */
    std::uint64_t              range_size = 0;        /**< Size of the whole tracked range. */
    std::uint64_t              size = 0;              /**< Size of the requested bytes. */
};

/**
 * Finds the exact indexed asset and pack-member range containing all requested bytes; false when
 * none or more than one equally small range holds them.
 */
bool DataReadSourceRange(const void *bytes, std::size_t size, DataReadSource &source);

/**
 * Carries a pack member's retail identity across an explicit copy into an owned arena, so
 * DataReadSourceRange finds the copy too.
 */
bool DataReadRegisterCopiedSource(const void *destination, std::size_t size, const void *owner,
                                  const DataReadSource &source);

/**
 * Drops copied-source ranges invalidated by an arena rewind or by a new allocation of
 * allocation_quads quadwords at owner's current end.
 */
void DataReadInvalidateCopiedSources(const void *owner, std::size_t allocation_quads);

/**
 * Drops source mappings owned by one storage buffer before that buffer is re-carved.
 */
void DataReadForgetPackCapacities(const void *owner);

/**
 * Clears pack bounds for buffers invalidated when the game resets its data arenas.
 */
void DataReadClearPackCapacities();

/**
 * Forgets the data index, the archive and every tracked range, so the next read indexes afresh.
 */
void DataReadReset();

/**
 * Indexes the game data (loose files under the data root, or data.zip beside it) and checks it
 * against data.hd2; aborts when there is no usable data.
 */
void DataReadIndex();

/**
 * Reads a whole data file into buffer as the drive did, zeroing the rest of its last sector, and
 * tracks the buffer as a pack; false, with nothing read, when the data has no such file. A device
 * prefix before a colon in the path is ignored.
 */
bool DataReadLoadFile(std::string_view path, void *buffer, int *size);

/**
 * The readable extent of a loaded pack or pack member; false when pack is not one.
 */
bool DataReadPackCapacity(const void *pack, std::size_t &capacity);

/**
 * Tracks a pack member's bytes as a nested range of the loaded pack: entry_offset is the member's
 * offset in the pack, entry_size its size there, and size the size of the bytes at data (which a
 * localized message file may change). duplicate_name tags the member with its index.
 */
void DataReadTrackPackMember(const void *pack, const void *data, std::size_t size, std::size_t entry_offset,
                             std::string_view entry_name, std::size_t entry_size, int index, bool duplicate_name);

} // namespace dc::data

// DC_TRANSITIONAL_BEGIN
#ifndef DC_NO_TRANSITIONAL_USING
using namespace dc::data;
#endif
// DC_TRANSITIONAL_END
