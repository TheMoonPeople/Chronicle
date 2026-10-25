#pragma once

#include "common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// The languages the extracted disc carries, as LanguageCode numbers, from the languages.json dcdata
// writes for its release (dcdata::ReleaseOf); none when the data has no such file.
std::vector<s32> DataLanguages();

// The languages the game can offer: the disc's own. A localization pack, once the port has them,
// adds the languages it carries here.
std::vector<s32> SupportedLanguages();

// The size of a file of the game's data ("gedit/e01/chara/p13a.chr"), as the loaders find it; 0 for
// one the data does not have.
std::uintmax_t DataFileSize(std::string_view path);

// Whether the data has been indexed. Unlike the readers it never indexes, which stops the program
// when there is no data.
bool DataReadIndexed();

// Lists logical asset paths and reads a file before in-memory localization is applied.
std::vector<std::string> DataReadListFiles();
bool                     DataReadReadFile(std::string_view path, std::vector<std::uint8_t> &bytes);
bool                     DataReadReadResolvedFile(std::string_view source_path, std::vector<std::uint8_t> &bytes);

/**
 * Pins raw source bytes for main-thread texture metadata inspection.
 *
 * Archived assets are cached by default and immutable within an index epoch; edits require DataReadReset
 * or InitCDFile. DC_TEXTURE_SOURCE_CACHE=0 disables caching. Loose files stay fresh. Pins survive eviction
 * and index reset.
 * Returns nullptr when the exact source binding is missing, ambiguous or unreadable.
 */
std::shared_ptr<const std::vector<std::uint8_t>> DataReadResolvedMetadataSnapshot(std::string_view source_path);

struct DataTextureReadStats {
    std::uint64_t            archived_json = 0;
    std::uint64_t            archived_png = 0;
    std::uint64_t            loose_json = 0;
    std::uint64_t            loose_png = 0;
    std::vector<std::string> archived_sources;
    std::vector<std::string> loose_sources;
};

DataTextureReadStats DataReadTextureReadStats();
void                 DataReadSetTextureReadStatsEnabled(bool enabled);

struct DataReadSource {
    std::string                path;
    std::string                member;
    std::vector<std::uint64_t> member_offsets;
    std::vector<std::uint64_t> member_source_offsets;
    std::vector<std::uint64_t> member_sizes;
    std::uint64_t              offset = 0;
    std::uint64_t              range_size = 0;
    std::uint64_t              size = 0;
};

// Returns the exact indexed asset and pack-member range containing all requested bytes.
bool DataReadSourceRange(const void *bytes, std::size_t size, DataReadSource &source);
// Carries a pack member's retail identity across an explicit copy into an owned arena.
bool DataReadRegisterCopiedSource(const void *destination, std::size_t size, const void *owner,
                                  const DataReadSource &source);
// Drops copied-source ranges invalidated by an arena rewind or a new allocation.
void DataReadInvalidateCopiedSources(const void *owner, std::size_t allocation_quads);
/**
 * Drops source mappings owned by one storage buffer before that buffer is re-carved.
 *
 */
void DataReadForgetPackCapacities(const void *owner);

/**
 * Clears pack bounds for buffers invalidated when the game resets its data arenas.
 *
 */
void DataReadClearPackCapacities();
void DataReadReset();
