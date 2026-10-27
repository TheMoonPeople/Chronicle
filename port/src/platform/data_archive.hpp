#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace dc::platform {

/**
 * Reads a conventional ZIP archive of game data.
 *
 * Stored and deflated entries are supported; malformed archives fail when opened or read.
 */
class DataArchive {
public:
    /**
     * One file name and its uncompressed size from the archive.
     */
    struct Entry {
        std::string   name;     /**< Slash-separated path, in archive case. */
        std::uint64_t size = 0; /**< Uncompressed bytes. */
    };

    /**
     * Opens the archive and validates its central directory; local headers are checked when read.
     */
    explicit DataArchive(const std::filesystem::path &path);

    /**
     * Returns the number of files listed by the archive.
     */
    std::size_t Count() const;

    /**
     * Returns the name and uncompressed size for an archive entry.
     */
    const Entry &GetEntry(std::size_t index) const;

    /**
     * Reads one entry and verifies its uncompressed size and CRC-32.
     */
    bool Read(std::size_t index, std::span<std::uint8_t> output) const;

private:
    /**
     * ZIP compression methods the archive accepts.
     */
    enum class Method : std::uint16_t {
        Stored = 0,
        Deflated = 8
    };

    struct Record {
        Entry                 entry;
        std::uint32_t         crc = 0;
        std::uint32_t         compressed_size = 0;
        std::uint32_t         local_header_offset = 0;
        Method                method = Method::Stored;
        std::uint16_t         flags = 0;
        mutable std::uint64_t data_offset = 0;
        mutable bool          local_header_checked = false;
    };

    std::filesystem::path       path_;
    mutable std::vector<Record> records_;
    mutable std::mutex          stream_mutex_;
    mutable std::ifstream       stream_;
    std::uint64_t               central_directory_offset_ = 0;
};

/**
 * Reports whether the data root has a sibling data.zip archive.
 */
bool DataArchivePresent(const std::filesystem::path &data_root);

} // namespace dc::platform
