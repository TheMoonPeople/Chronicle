#pragma once

#include <gtest/gtest.h>

// Synthetic disc fixtures for the data tests: a DATA.DAT/DATA.HD2 pair laid out as the disc's,
// packs in the game's 76-byte entry format, and a minimal ISO 9660 image holding the pair.

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "../../../tools/dcdata/dcdata.hpp"
#include "platform_fixture.hpp"

namespace datafix {

namespace fs = std::filesystem;

using Bytes = std::vector<unsigned char>;

struct File {
    std::string name;
    Bytes       data;
};

struct Disc {
    Bytes             dat;
    Bytes             hd2;
    std::vector<File> files;
};

// Resolved, as the working directory comes back: macOS's /var is a symlink.
inline fs::path TempDir(const char *tag) {
    fs::path dir = fs::canonical(fs::temp_directory_path()) / std::format("dc_{}_{}", tag, dc::test::ProcessId());
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

inline Bytes Pattern(std::size_t size, unsigned seed) {
    Bytes data(size);
    for (std::size_t i = 0; i < size; i++) {
        data[i] = static_cast<unsigned char>((i * 31 + seed * 17 + 1) % 251 + 1);
    }
    return data;
}

inline void Put32(Bytes &data, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; i++) {
        data[offset + i] = static_cast<unsigned char>(value >> (8 * i));
    }
}

inline void PutBoth32(Bytes &data, std::size_t offset, std::uint32_t value) {
    Put32(data, offset, value);
    for (int i = 0; i < 4; i++) {
        data[offset + 4 + i] = static_cast<unsigned char>(value >> (24 - 8 * i));
    }
}

inline void WriteBytes(const fs::path &path, const Bytes &data) {
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(data.data()), data.size());
}

inline Bytes ReadBytes(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(stream), {});
}

inline Bytes MakePack(const std::vector<File> &files) {
    std::size_t table = 76 * (files.size() + 1);
    Bytes       pack(table);
    for (std::size_t i = 0; i < files.size(); i++) {
        std::size_t entry = 76 * i;
        std::size_t data = (pack.size() + 15) & ~std::size_t{15};
        pack.resize(data);
        pack.insert(pack.end(), files[i].data.begin(), files[i].data.end());
        std::memcpy(&pack[entry], files[i].name.c_str(), files[i].name.size() + 1);
        Put32(pack, entry + 64, static_cast<std::uint32_t>(data - entry));
        Put32(pack, entry + 68, static_cast<std::uint32_t>(files[i].data.size()));
        Put32(pack, entry + 72, 76);
    }
    return pack;
}

// Each file starts on its own sector, one empty sector in, with the tail of its last sector
// filled with a non-zero byte so a reader that copies whole sectors is caught.
inline Disc MakeDisc(std::vector<File> files) {
    Disc disc;
    disc.files = std::move(files);
    // The retail index counts one all-zero record after the last file, whose name offset
    // lands on the NUL that ends the name block.
    std::size_t count = disc.files.size();
    disc.hd2.assign(32 * (count + 1), 0);
    disc.dat.assign(dcdata::kSector, 0xCD);
    for (std::size_t i = 0; i < count; i++) {
        const File   &file = disc.files[i];
        std::uint32_t sector = static_cast<std::uint32_t>(disc.dat.size() / dcdata::kSector);
        std::uint32_t name = static_cast<std::uint32_t>(disc.hd2.size());
        disc.hd2.insert(disc.hd2.end(), file.name.begin(), file.name.end());
        disc.hd2.push_back(0);
        disc.dat.insert(disc.dat.end(), file.data.begin(), file.data.end());
        disc.dat.resize(dcdata::SectorsFor(disc.dat.size()) * dcdata::kSector, 0xCD);
        Put32(disc.hd2, 32 * i, name);
        Put32(disc.hd2, 32 * i + 4, 0xDEADBEEF);
        Put32(disc.hd2, 32 * i + 16, sector * dcdata::kSector);
        Put32(disc.hd2, 32 * i + 20, static_cast<std::uint32_t>(file.data.size()));
        Put32(disc.hd2, 32 * i + 24, sector);
        Put32(disc.hd2, 32 * i + 28, static_cast<std::uint32_t>(dcdata::SectorsFor(file.data.size())));
    }
    Put32(disc.hd2, 32 * count, static_cast<std::uint32_t>(disc.hd2.size() - 1));
    return disc;
}

inline std::uint32_t SectorCount(const Bytes &data) {
    return static_cast<std::uint32_t>(dcdata::SectorsFor(data.size()));
}

inline Bytes DirectoryRecord(std::string_view name, std::uint32_t extent, std::size_t size, bool directory) {
    Bytes record(33 + name.size() + (name.size() % 2 == 0 ? 1 : 0));
    record[0] = static_cast<unsigned char>(record.size());
    PutBoth32(record, 2, extent);
    PutBoth32(record, 10, static_cast<std::uint32_t>(size));
    record[25] = directory ? 2 : 0;
    record[28] = 1;
    record[31] = 1;
    record[32] = static_cast<unsigned char>(name.size());
    std::memcpy(&record[33], name.data(), name.size());
    return record;
}

// Sectors 16 and 17 are the primary descriptor and the terminator, the root directory takes 18
// and 19 with the second file's record pushed into 19 so the zero tail of 18 must be skipped,
// and the files follow from 20.
inline Bytes MakeIso(const std::string &first_name, const Bytes &first, const std::string &second_name,
                     const Bytes &second) {
    constexpr std::uint32_t root_extent = 18;
    std::uint32_t           first_extent = 20;
    std::uint32_t           second_extent = first_extent + SectorCount(first);
    std::uint32_t           total = second_extent + SectorCount(second);

    Bytes                 image(total * dcdata::kSector, 0);
    constexpr std::size_t pvd = 16 * dcdata::kSector;
    constexpr std::size_t terminator = 17 * dcdata::kSector;
    std::string_view      self("\0", 1);

    image[pvd] = 1;
    std::memcpy(&image[pvd + 1], "CD001", 5);
    image[pvd + 6] = 1;
    PutBoth32(image, pvd + 80, total);
    image[pvd + 129] = 0x08;
    Bytes root = DirectoryRecord(self, root_extent, 2 * dcdata::kSector, true);
    std::memcpy(&image[pvd + 156], root.data(), root.size());
    image[terminator] = 255;
    std::memcpy(&image[terminator + 1], "CD001", 5);
    image[terminator + 6] = 1;

    std::size_t offset = root_extent * dcdata::kSector;
    for (const Bytes &record : {root, DirectoryRecord("\1", root_extent, 2 * dcdata::kSector, true),
                                DirectoryRecord(first_name, first_extent, first.size(), false)}) {
        std::memcpy(&image[offset], record.data(), record.size());
        offset += record.size();
    }
    Bytes last = DirectoryRecord(second_name, second_extent, second.size(), false);
    std::memcpy(&image[(root_extent + 1) * dcdata::kSector], last.data(), last.size());
    std::memcpy(&image[first_extent * dcdata::kSector], first.data(), first.size());
    std::memcpy(&image[second_extent * dcdata::kSector], second.data(), second.size());
    return image;
}

inline File PackFile() {
    return {
        "rmdat\\rmdat1.pak", MakePack({{"info.cfg", Pattern(37, 1)},
                                       {"MODEL.MDS", Pattern(300, 2)},
                                       {"tex.img", Pattern(64, 3)},
                                       {"face.IMG", Pattern(5, 4)}}
         )
    };
}

// The game's own paths, as DATA.HD2 spells them: backslashes, mixed case, one with a leading
// separator, a later duplicate of the first, sizes on both sides of a sector, and an empty file.
inline Disc StandardDisc() {
    return MakeDisc({
        {"DUN\\PACK\\MAINDAT.PAC", Pattern(5000, 10)},
        {"img\\Title.img", Pattern(2048, 11)},
        {"\\sound\\bgm\\b01.SND", Pattern(1, 12)},
        PackFile(),
        {"meswin\\systeme.bin", Pattern(2047, 13)},
        {"dun\\pack\\maindat.pac", Pattern(100, 14)},
        {"empty.bin", {}},
    });
}

inline constexpr std::size_t kStandardReachable = 6;

} // namespace datafix
