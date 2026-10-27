#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/exitcodes.hpp"
#include "audio/hdbank.hpp"
#include "audio/sq.hpp"
#include "data/data_read.hpp"
#include "data_fixture.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "platform/audio.hpp"
#include "platform/data_archive.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"
#include "render/texture_registry.hpp"
#include "sound.hpp"

namespace dc::test {

using namespace dc::game;
using namespace dc::app;
using namespace dc::data;
using namespace dc::platform;
using namespace dc::render;
using namespace datafix;

namespace {

constexpr unsigned char kUntouched = 0xEE;

std::uint32_t IeeeCrc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t crc = 0xFFFFFFFF;
    for (const std::uint8_t byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) != 0 ? 0xEDB88320U : 0U);
        }
    }
    return ~crc;
}

struct StoredIndexFixture {
    Bytes       bytes;
    std::size_t central_offset = 0;
};

StoredIndexFixture MakeStoredIndexFixture(std::size_t entry_count, std::size_t comment_size) {
    Bytes             encoded;
    Bytes             central;
    const Bytes       extra{0xFE, 0xCA, 3, 0, 1, 2, 3};
    const std::string comment(comment_size, 'c');
    auto              append = [](Bytes &destination, const auto &source) {
        destination.insert(destination.end(), source.begin(), source.end());
    };
    for (std::size_t index = 0; index < entry_count; ++index) {
        const std::string   name = index == 0 ? "first.bin" : index == 1             ? "folder/"
                                                          : index + 1 == entry_count ? "last.bin"
                                                                                     : std::to_string(index) + ".bin";
        const Bytes         bytes = index == 0 ? Bytes{1, 2, 3} : index == 1             ? Bytes{}
                                                              : index + 1 == entry_count ? Bytes{9, 8, 7, 6}
                                                                                         : Bytes{static_cast<std::uint8_t>(index), 0x5A, 0xA5};
        const auto          offset = static_cast<std::uint32_t>(encoded.size());
        const auto          size = static_cast<std::uint32_t>(bytes.size());
        const std::uint32_t crc = IeeeCrc32(bytes);
        Bytes               local(30);
        Put32(local, 0, 0x04034B50);
        local[4] = 20;
        Put32(local, 14, crc);
        Put32(local, 18, size);
        Put32(local, 22, size);
        local[26] = static_cast<std::uint8_t>(name.size());
        append(encoded, local);
        append(encoded, name);
        append(encoded, bytes);
        Bytes header(46);
        Put32(header, 0, 0x02014B50);
        header[4] = 20;
        header[6] = 20;
        Put32(header, 16, crc);
        Put32(header, 20, size);
        Put32(header, 24, size);
        header[28] = static_cast<std::uint8_t>(name.size());
        header[30] = static_cast<std::uint8_t>(extra.size());
        header[32] = static_cast<std::uint8_t>(comment.size());
        header[33] = static_cast<std::uint8_t>(comment.size() >> 8);
        Put32(header, 42, offset);
        append(central, header);
        append(central, name);
        append(central, extra);
        append(central, comment);
    }
    const std::size_t central_offset = encoded.size();
    Bytes             end(22);
    Put32(end, 0, 0x06054B50);
    end[8] = static_cast<std::uint8_t>(entry_count);
    end[9] = static_cast<std::uint8_t>(entry_count >> 8);
    end[10] = end[8];
    end[11] = end[9];
    Put32(end, 12, static_cast<std::uint32_t>(central.size()));
    Put32(end, 16, static_cast<std::uint32_t>(central_offset));
    append(encoded, central);
    append(encoded, end);
    return {std::move(encoded), central_offset};
}

struct PathTestScope {
    fs::path              working_directory = fs::current_path();
    dc::test::EnvSnapshot data_environment = dc::test::SaveEnv("DC_DATA");
    dc::test::EnvSnapshot save_environment = dc::test::SaveEnv("DC_SAVE");

    PathTestScope() {
        PathsReset();
        PathsConfigure({.app_dir = "chronicle", .env_data = "DC_DATA", .env_save = "DC_SAVE"});
    }

    ~PathTestScope() {
        std::error_code error;
        fs::current_path(working_directory, error);
        dc::test::RestoreEnv(data_environment);
        dc::test::RestoreEnv(save_environment);
        PathsReset();
        PathsConfigure({.app_dir = "chronicle", .env_data = "DC_DATA", .env_save = "DC_SAVE"});
    }
};

struct alignas(64) BufferBlock {
    unsigned char bytes[64];
};

constexpr std::size_t          kBufferSize = 16384;
std::unique_ptr<BufferBlock[]> buffer_storage = std::make_unique<BufferBlock[]>(kBufferSize / 64);
unsigned char *const           buffer = reinterpret_cast<unsigned char *>(buffer_storage.get());

struct Mutable {
    std::string text;

    explicit Mutable(const char *source)
        : text(source) {}

    operator char *() {
        return text.data();
    }
};

// The standard disc mastered into an ISO, extracted with dcdata, and installed as the data root.
fs::path InstallStandardData(const char *tag) {
    fs::path dir = TempDir(tag);
    Disc     disc = StandardDisc();
    WriteBytes(dir / "disc.iso", MakeIso("DATA.DAT;1", disc.dat, "DATA.HD2;1", disc.hd2));
    dcdata::Extract(dcdata::OpenArchive(dir / "disc.iso"), dir / "data", nullptr);
    PathsSetDataRoot(dir / "data");
    PathsSetSaveRoot(dir / "save");
    InitCDFile();
    return dir;
}

u_long128 *Quads(void *data) {
    return static_cast<u_long128 *>(data);
}

void Clear() {
    std::memset(buffer, kUntouched, kBufferSize);
}

bool Equals(const void *data, const Bytes &expected) {
    return std::memcmp(data, expected.data(), expected.size()) == 0;
}

bool AllAre(const unsigned char *from, const unsigned char *to, unsigned char value) {
    return std::all_of(from, to, [value](unsigned char byte) { return byte == value; });
}

} // namespace

TEST(DataRead, Loadfile2ReadsExactSizeAndZeroFills) {
    fs::path dir = InstallStandardData("read_exact");
    Disc     disc = StandardDisc();
    int      size = -1;

    Clear();
    ASSERT_TRUE(LoadFile2(Mutable("dun/pack/maindat.pac"), buffer, &size, 0) == 1);
    ASSERT_TRUE(size == 5000);
    ASSERT_TRUE(Equals(buffer, disc.files[0].data));
    ASSERT_TRUE(AllAre(buffer + 5000, buffer + 3 * 2048, 0));
    ASSERT_TRUE(buffer[3 * 2048] == kUntouched);

    Clear();
    ASSERT_TRUE(LoadFile2(Mutable("img/title.img"), buffer, &size, 0) == 1);
    ASSERT_TRUE(size == 2048 && Equals(buffer, disc.files[1].data) && buffer[2048] == kUntouched);

    Clear();
    ASSERT_TRUE(LoadFile2(Mutable("sound/bgm/b01.snd"), buffer, &size, 0) == 1);
    ASSERT_TRUE(size == 1 && buffer[0] == disc.files[2].data[0] && AllAre(buffer + 1, buffer + 2048, 0));

    Clear();
    ASSERT_TRUE(LoadFile2(Mutable("empty.bin"), buffer, &size, 0) == 1);
    ASSERT_TRUE(size == 0 && buffer[0] == kUntouched);

    Clear();
    ASSERT_TRUE(LoadFile(Mutable("meswin/systeme.bin"), buffer, &size) == 1);
    ASSERT_TRUE(size == 2047 && Equals(buffer, disc.files[4].data) && buffer[2047] == 0);
    fs::remove_all(dir);
}

TEST(DataRead, ReadsStoredArchiveWithoutExtractedDirectory) {
    fs::path   dir = InstallStandardData("read_archive");
    const Disc disc = StandardDisc();
    dcdata::PackDirectory(dir / "data", dir / "data.zip");
    ASSERT_TRUE(DataArchivePresent(dir / "data"));
    fs::remove_all(dir / "data");
    InitCDFile();

    int size = -1;
    Clear();
    ASSERT_TRUE(LoadFile2(Mutable("dun/pack/maindat.pac"), buffer, &size, 0) == 1);
    ASSERT_TRUE(size == 5000 && Equals(buffer, disc.files[0].data));
    Clear();
    ASSERT_TRUE(LoadFile2(Mutable("DUN\\PACK\\MAINDAT.PAC"), buffer, &size, 0) == 1);
    ASSERT_TRUE(size == 5000 && Equals(buffer, disc.files[0].data));
    std::vector<std::uint8_t> indexed_file;
    ASSERT_TRUE(DataReadReadFile("img/title.img", indexed_file));
    ASSERT_TRUE(indexed_file == disc.files[1].data);
    DataReadReset();
    fs::remove_all(dir);
}

TEST(DataRead, ArchiveLookupEscapesShiftJisBytesOnEveryPlatform) {
    const fs::path    dir = TempDir("read_archive_shift_jis");
    const std::string shift_jis_path = std::string("dun/") + "\x82\xa0" + ".bin";
    const Bytes       contents = Pattern(17, 21);
    const Disc        disc = MakeDisc({
        {shift_jis_path, contents}
    });
    const fs::path    source = dir / "source";
    WriteBytes(source / "data.hd2", disc.hd2);
    WriteBytes(source / "dun" / "%82%a0.bin", contents);
    dcdata::PackDirectory(source, dir / "data.zip");
    PathsSetDataRoot(dir / "data");
    PathsSetSaveRoot(dir / "save");

    testing::internal::CaptureStderr();
    InitCDFile();
    const std::string init_diagnostics = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(init_diagnostics.empty()) << init_diagnostics;

    int size = -1;
    Clear();
    ASSERT_EQ(LoadFile2(Mutable(shift_jis_path.c_str()), buffer, &size, 0), 1);
    EXPECT_EQ(size, static_cast<int>(contents.size()));
    EXPECT_TRUE(Equals(buffer, contents));

    DataReadReset();
    fs::remove_all(dir);
}

TEST(DataRead, LooseDirectoryKeepsItsNativeShiftJisPathKeys) {
    const fs::path    dir = TempDir("read_loose_shift_jis");
    const std::string path = std::string("loose/") + "\x82\xa0" + ".bin";
    const Bytes       contents = Pattern(23, 22);
    const Disc        disc = MakeDisc({
        {path, contents}
    });
    WriteBytes(dir / "disc.iso", MakeIso("DATA.DAT;1", disc.dat, "DATA.HD2;1", disc.hd2));
    dcdata::Extract(dcdata::OpenArchive(dir / "disc.iso"), dir / "data", nullptr);
    PathsSetDataRoot(dir / "data");
    PathsSetSaveRoot(dir / "save");
    InitCDFile();

    int size = -1;
    Clear();
    ASSERT_EQ(LoadFile2(Mutable(path.c_str()), buffer, &size, 0), 1);
    EXPECT_EQ(size, static_cast<int>(contents.size()));
    EXPECT_TRUE(Equals(buffer, contents));

    DataReadReset();
    fs::remove_all(dir);
}

TEST(DataRead, ArchiveIntegrityUsesBaseAssetsBeforeNormalizedOverrides) {
    const fs::path dir = TempDir("read_archive_overlay_integrity");
    const Bytes    base = {1, 2, 3, 4};
    const Bytes    overlay = {5, 6};
    const Bytes    base_chr = {7, 8, 9};
    Bytes          overlay_chr(72);
    std::memcpy(overlay_chr.data() + 4, "TIM2", 4);
    overlay_chr[10] = 1;
    overlay_chr[20] = 52;
    overlay_chr[28] = 4;
    overlay_chr[32] = 48;
    const Disc     disc = MakeDisc({
        {"asset.bin", base}
    });
    const fs::path source = dir / "source";
    WriteBytes(source / "data.hd2", disc.hd2);
    WriteBytes(source / "asset.bin", base);
    WriteBytes(source / "normalized" / "asset.bin", overlay);
    WriteBytes(source / "hero.chr", base_chr);
    WriteBytes(source / "normalized" / "hero.chr", overlay_chr);
    dcdata::PackDirectory(source, dir / "data.zip");
    PathsSetDataRoot(dir / "data");
    PathsSetSaveRoot(dir / "save");

    testing::internal::CaptureStderr();
    InitCDFile();
    const std::string init_diagnostics = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(init_diagnostics.empty()) << init_diagnostics;

    int size = -1;
    Clear();
    ASSERT_EQ(LoadFile2(Mutable("asset.bin"), buffer, &size, 0), 1);
    EXPECT_EQ(size, static_cast<int>(overlay.size()));
    EXPECT_TRUE(Equals(buffer, overlay));
    DataReadSource source_info;
    ASSERT_TRUE(DataReadSourceRange(buffer, overlay.size(), source_info));
    EXPECT_EQ(source_info.path, "normalized/asset.bin");
    EXPECT_TRUE(source_info.member.empty());

    Clear();
    ASSERT_EQ(LoadFile2(Mutable("hero.chr"), buffer, &size, 0), 1);
    ASSERT_TRUE(DataReadSourceRange(buffer, overlay_chr.size(), source_info));
    EXPECT_EQ(source_info.path, "normalized/hero.chr");
    Bytes resolved_chr;
    ASSERT_TRUE(DataReadReadResolvedFile(source_info.path, resolved_chr));
    EXPECT_EQ(resolved_chr, overlay_chr);
    ASSERT_TRUE(DataReadSourceRange(static_cast<const std::uint8_t *>(buffer) + 68, 4, source_info));
    EXPECT_EQ(source_info.path, "normalized/hero.chr");
    EXPECT_EQ(PortTextureDocumentFromSource(source_info), "textures/normalized/hero-embedded-offset-4.json");

    DataReadReset();
    fs::remove_all(dir);
}

TEST(DataRead, CountsBakedTextureReadsFromArchiveWithSourcePaths) {
    const fs::path dir = TempDir("read_texture_archive_stats");
    const fs::path source = dir / "source";
    WriteBytes(source / "textures" / "fixture.json", Bytes{'{', '}'});
    WriteBytes(source / "textures" / "fixture.png", Bytes{1, 2, 3});
    dcdata::PackDirectory(source, dir / "data.zip");
    PathsSetDataRoot(dir / "data");

    DataReadSetTextureReadStatsEnabled(true);
    InitCDFile();
    Bytes document;
    Bytes image;
    ASSERT_TRUE(DataReadReadFile("textures/fixture.json", document));
    ASSERT_TRUE(DataReadReadFile("textures/fixture.png", image));
    const DataTextureReadStats stats = DataReadTextureReadStats();
    EXPECT_EQ(stats.archived_json, 1);
    EXPECT_EQ(stats.archived_png, 1);
    EXPECT_EQ(stats.loose_json, 0);
    EXPECT_EQ(stats.loose_png, 0);
    EXPECT_EQ(stats.archived_sources, (std::vector<std::string>{"textures/fixture.json", "textures/fixture.png"}));

    DataReadSetTextureReadStatsEnabled(false);
    DataReadReset();
    fs::remove_all(dir);
}

TEST(DataArchive, Slicing8MatchesIndependentIeeeOracle) {
    ASSERT_EQ(dc::test::SetEnv("DC_ARCHIVE_CRC_SLICE8", "1", 1), 0);
    const Bytes known{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    ASSERT_EQ(IeeeCrc32({}), 0U);
    ASSERT_EQ(IeeeCrc32(known), 0xCBF43926U);

    std::vector<Bytes> cases{known};
    for (const std::size_t length : {0U, 1U, 7U, 8U, 9U, 15U, 16U, 17U, 31U, 257U, 4096U, 4099U}) {
        Bytes bytes(length);
        for (std::size_t index = 0; index < length; ++index) {
            bytes[index] = static_cast<std::uint8_t>((index * 73 + index / 11) & 0xFF);
        }
        cases.push_back(std::move(bytes));
    }

    const fs::path             dir = TempDir("archive_slicing8_oracle");
    constexpr std::string_view name = "file.bin";
    for (std::size_t index = 0; index < cases.size(); ++index) {
        SCOPED_TRACE(index);
        const Bytes   &bytes = cases[index];
        const fs::path fixture = dir / std::to_string(index);
        WriteBytes(fixture / "source" / name, bytes);
        const fs::path archive_path = fixture / "data.zip";
        dcdata::PackDirectory(fixture / "source", archive_path);
        Bytes             encoded = ReadBytes(archive_path);
        const std::size_t data_offset = 30 + name.size();
        const std::size_t central_offset = data_offset + bytes.size();
        ASSERT_GE(encoded.size(), central_offset + 46);
        const std::uint32_t crc = IeeeCrc32(bytes);
        Put32(encoded, 14, crc);
        Put32(encoded, central_offset + 16, crc);
        WriteBytes(archive_path, encoded);
        {
            DataArchive archive(archive_path);
            for (std::size_t offset = 0; offset < 16; ++offset) {
                SCOPED_TRACE(offset);
                Bytes output(bytes.size() + 32, kUntouched);
                auto  payload = std::span(output).subspan(offset, bytes.size());
                ASSERT_TRUE(archive.Read(0, payload));
                ASSERT_TRUE(std::equal(payload.begin(), payload.end(), bytes.begin(), bytes.end()));
                ASSERT_TRUE(AllAre(output.data(), output.data() + offset, kUntouched));
                ASSERT_TRUE(AllAre(output.data() + offset + bytes.size(), output.data() + output.size(), kUntouched));
            }
        }
        if (bytes.empty()) {
            Put32(encoded, 14, crc ^ 1U);
            Put32(encoded, central_offset + 16, crc ^ 1U);
        } else {
            encoded[data_offset + bytes.size() / 2] ^= 0x80;
        }
        WriteBytes(archive_path, encoded);
        {
            DataArchive archive(archive_path);
            Bytes       output(bytes.size());
            ASSERT_FALSE(archive.Read(0, output));
        }
    }
    fs::remove_all(dir);
}

TEST(DataArchive, SequentialIndexReadsCentralExtrasCommentsAndDirectories) {
    ASSERT_EQ(dc::test::SetEnv("DC_ARCHIVE_SEQUENTIAL_INDEX", "1", 1), 0);
    auto              fixture = MakeStoredIndexFixture(3, 21);
    Bytes            &encoded = fixture.bytes;
    const std::size_t central_offset = fixture.central_offset;
    const Bytes       first{1, 2, 3};
    const Bytes       last{9, 8, 7, 6};
    const fs::path    dir = TempDir("archive_sequential_index");
    const fs::path    archive_path = dir / "data.zip";
    WriteBytes(archive_path, encoded);
    {
        DataArchive archive(archive_path);
        ASSERT_EQ(archive.Count(), 3U);
        ASSERT_EQ(archive.GetEntry(1).name, "folder/");
        Bytes output(first.size());
        ASSERT_TRUE(archive.Read(0, output));
        ASSERT_EQ(output, first);
        output.resize(last.size());
        ASSERT_TRUE(archive.Read(2, output));
        ASSERT_EQ(output, last);
        ASSERT_FALSE(archive.Read(1, {}));
        output.resize(first.size());
        ASSERT_TRUE(archive.Read(0, output));
        ASSERT_EQ(output, first);
    }
    encoded[central_offset + 46 + std::string_view("first.bin").size() + 2] = 0xFF;
    WriteBytes(archive_path, encoded);
    EXPECT_THROW({ DataArchive archive(archive_path); }, std::runtime_error);
    fs::remove_all(dir);
}

TEST(DataArchive, BulkIndexReadsCentralExtrasCommentsAndRejectsShortRecords) {
    ASSERT_EQ(dc::test::SetEnv("DC_ARCHIVE_BULK_INDEX", "1", 1), 0);
    const auto     fixture = MakeStoredIndexFixture(3, 21);
    const fs::path dir = TempDir("archive_bulk_index");
    const fs::path archive_path = dir / "data.zip";
    WriteBytes(archive_path, fixture.bytes);
    {
        DataArchive archive(archive_path);
        ASSERT_EQ(archive.Count(), 3U);
        ASSERT_EQ(archive.GetEntry(1).name, "folder/");
        Bytes output(4);
        ASSERT_TRUE(archive.Read(2, output));
        ASSERT_EQ(output, (Bytes{9, 8, 7, 6}));
        output.resize(3);
        ASSERT_TRUE(archive.Read(0, output));
        ASSERT_EQ(output, (Bytes{1, 2, 3}));
        ASSERT_FALSE(archive.Read(1, {}));
    }
    Bytes invalid = fixture.bytes;
    invalid[fixture.central_offset + 46 + std::string_view("first.bin").size() + 2] = 0xFF;
    WriteBytes(archive_path, invalid);
    EXPECT_THROW({ DataArchive archive(archive_path); }, std::runtime_error);
    for (const std::size_t central_bytes : {20U, 47U}) {
        SCOPED_TRACE(central_bytes);
        Bytes shortened(fixture.bytes.begin(), fixture.bytes.begin() + fixture.central_offset + central_bytes);
        Bytes end(fixture.bytes.end() - 22, fixture.bytes.end());
        Put32(end, 12, static_cast<std::uint32_t>(central_bytes));
        shortened.insert(shortened.end(), end.begin(), end.end());
        WriteBytes(archive_path, shortened);
        EXPECT_THROW({ DataArchive archive(archive_path); }, std::runtime_error);
    }
    fs::remove_all(dir);
}

TEST(DataArchive, BulkIndexFallsBackForLargeValidDirectory) {
    ASSERT_EQ(dc::test::SetEnv("DC_ARCHIVE_BULK_INDEX", "1", 1), 0);
    const auto fixture = MakeStoredIndexFixture(257, 65535);
    ASSERT_GT(fixture.bytes.size() - 22 - fixture.central_offset, 16U * 1024 * 1024);
    const fs::path dir = TempDir("archive_bulk_index_large");
    const fs::path archive_path = dir / "data.zip";
    WriteBytes(archive_path, fixture.bytes);
    {
        DataArchive archive(archive_path);
        ASSERT_EQ(archive.Count(), 257U);
        Bytes output(4);
        ASSERT_TRUE(archive.Read(256, output));
        ASSERT_EQ(output, (Bytes{9, 8, 7, 6}));
    }
    fs::remove_all(dir);
}

TEST(DataArchive, RejectsPayloadWithWrongCrc) {
    const fs::path dir = TempDir("archive_crc");
    WriteBytes(dir / "source" / "file.bin", Bytes{1, 2, 3, 4});
    const fs::path archive_path = dir / "data.zip";
    dcdata::PackDirectory(dir / "source", archive_path);
    {
        std::fstream archive(archive_path, std::ios::binary | std::ios::in | std::ios::out);
        archive.seekg(30 + std::string("file.bin").size());
        char byte = 0;
        archive.read(&byte, 1);
        archive.seekp(30 + std::string("file.bin").size());
        byte ^= 0xFF;
        archive.write(&byte, 1);
    }
    {
        DataArchive               archive(archive_path);
        std::vector<std::uint8_t> output(4);
        ASSERT_FALSE(archive.Read(0, output));
    }
    fs::remove_all(dir);
}

TEST(DataRead, LanguagesFromTheExtraction) {
    fs::path dir = InstallStandardData("read_languages");
    ASSERT_TRUE((DataLanguages() == std::vector<s32>{2, 3, 4, 5, 6}));

    std::string text = R"({"release": "[x]", "languages": [1, 9]})";
    WriteBytes(dir / "data/languages.json", Bytes(text.begin(), text.end()));
    InitCDFile();
    ASSERT_TRUE((DataLanguages() == std::vector<s32>{1})); // 9 is no language, [x] no list
    ASSERT_TRUE((SupportedLanguages() == std::vector<s32>{1}));

    fs::remove(dir / "data/languages.json");
    InitCDFile();
    ASSERT_TRUE(DataLanguages().empty());
    fs::remove_all(dir);
}

TEST(DataRead, Loadfile2FoldsCaseAndDevices) {
    fs::path dir = InstallStandardData("read_names");
    Disc     disc = StandardDisc();
    int      size = -1;

    constexpr std::array<const char *, 5> names = {"DUN/PACK/MainDat.PAC", R"(cdrom0:\DUN\PACK\MAINDAT.PAC)",
                                                   "host:dun/pack/maindat.pac",
                                                   "sim:/dun/pack/maindat.pac", R"(\dun\pack\maindat.pac)"};
    for (const char *name : names) {
        Clear();
        size = -1;
        ASSERT_TRUE(LoadFile2(Mutable(name), buffer, &size, 0) == 1);
        ASSERT_TRUE(size == 5000 && Equals(buffer, disc.files[0].data));
    }

    Clear();
    size = 123;
    ASSERT_TRUE(LoadFile2(Mutable("dun/pack/missing.pac"), buffer, &size, 0) == 0);
    ASSERT_TRUE(size == 0 && buffer[0] == kUntouched);
    ASSERT_TRUE(LoadFile2(Mutable("dun/pack/missing.pac"), buffer, nullptr, 0) == 0);
    ASSERT_TRUE(LoadFile2(Mutable("img/title.img"), buffer, nullptr, 0) == 1);
    fs::remove_all(dir);
}

TEST(DataRead, LoadfileMissingAsserts) {
    fs::path dir = InstallStandardData("read_assert");
    int      size = -1;
    KZ_ASSERT_EXIT([&] { LoadFile(Mutable("nothing/here.bin"), buffer, &size); }, kExitGameAssert);
    fs::remove_all(dir);
}

TEST(DataRead, InitcdfileRequiresData) {
    fs::path dir = TempDir("read_nodata");
    PathsSetDataRoot(dir / "data");
    KZ_ASSERT_ABORT([] { InitCDFile(); });
    fs::create_directories(dir / "data/empty_dir");
    KZ_ASSERT_ABORT([] { InitCDFile(); });
    WriteBytes(dir / "data/a.bin", Pattern(3, 0));
    KZ_ASSERT_EXIT([] { InitCDFile(); }, 0);
    fs::remove_all(dir);
}

TEST(DataRead, LoadfilebgCompletesSynchronously) {
    fs::path dir = InstallStandardData("read_bg");
    Disc     disc = StandardDisc();
    int      size = -1;

    InitReadBG();
    ASSERT_TRUE(ReadBGSync() == 0);
    ASSERT_TRUE(GetReadBGFile(0) == nullptr);

    Clear();
    auto  second_storage = std::make_unique<BufferBlock[]>(4096 / 64);
    auto *second = reinterpret_cast<unsigned char *>(second_storage.get());
    ASSERT_TRUE(LoadFileBG(Mutable("DUN/PACK/MAINDAT.PAC"), Quads(buffer), &size) == 1);
    ASSERT_TRUE(size == 5000);
    ASSERT_TRUE(LoadFileBG(Mutable("nope.bin"), Quads(second), &size) == 0);
    ASSERT_TRUE(size == 0);
    ASSERT_TRUE(LoadFileBG(Mutable("meswin/systeme.bin"), Quads(second), &size) == 1);
    ASSERT_TRUE(size == 2047);
    ReadBG();
    ASSERT_TRUE(ReadBGSync() == 0);

    BG_READ_INFO *first = GetReadBGFile(0);
    ASSERT_TRUE(first && first->busy && first->id != 0 && first->done);
    ASSERT_TRUE(first->buffer == Quads(buffer) && first->size == 5000 && first->sectors == 3);
    ASSERT_TRUE(std::string_view(first->name) == "DUN/PACK/MAINDAT.PAC");
    ASSERT_TRUE(Equals(buffer, disc.files[0].data) && AllAre(buffer + 5000, buffer + 6144, 0));
    BG_READ_INFO *next = GetReadBGFile(1);
    ASSERT_TRUE(next && next->buffer == Quads(second) && next->size == 2047);
    ASSERT_TRUE(Equals(second, disc.files[4].data) && second[2047] == 0);
    ASSERT_TRUE(GetReadBGFile(2) == nullptr && GetReadBGFile(-1) == nullptr && GetReadBGFile(32) == nullptr);

    for (int i = 2; i < 32; i++) {
        ASSERT_TRUE(LoadFileBG(Mutable("sound/bgm/b01.snd"), Quads(second), nullptr) == 1);
    }
    ASSERT_TRUE(GetReadBGFile(31) != nullptr);
    ASSERT_TRUE(LoadFileBG(Mutable("sound/bgm/b01.snd"), Quads(second), &size) == 0);
    ASSERT_TRUE(size == 0);

    // A slot handed out but not yet complete is what keeps retail's spin loops waiting.
    GetReadBGFile(5)->done = 0;
    ASSERT_TRUE(ReadBGSync() == 1);
    BreakReadBG();
    ASSERT_TRUE(GetReadBGFile(0) == nullptr && GetReadBGFile(5) == nullptr);
    ASSERT_TRUE(ReadBGSync() == 0);
    StartReadBG();
    ASSERT_TRUE(LoadFileBG(Mutable("img/title.img"), Quads(buffer), &size) == 1);
    ASSERT_TRUE(GetReadBGFile(0) && GetReadBGFile(0)->size == 2048);
    fs::remove_all(dir);
}

TEST(DataRead, PackLookupOnHost) {
    fs::path dir = InstallStandardData("read_pack");
    File     pack = PackFile();
    int      size = -1;

    Clear();
    auto *pack_data = reinterpret_cast<u_int *>(buffer);
    ASSERT_TRUE(LoadPackFile(Mutable("RMDAT/RMDAT1.PAK"), pack_data, &size) == 1);
    ASSERT_TRUE(size == static_cast<int>(pack.data.size()));

    u_int *model = GetPackFile(pack_data, Mutable("chara/MODEL.mds"), &size);
    ASSERT_TRUE(model && size == 300 && Equals(model, Pattern(300, 2)));
    ASSERT_TRUE(GetPackFile(Mutable("info.CFG"), &size) && size == 37);
    ASSERT_TRUE(GetPackFile(pack_data, Mutable("absent.bin"), &size) == nullptr);

    char  *name = nullptr;
    u_int *tex = GetPackFile(pack_data, 2, &name, &size);
    ASSERT_TRUE(tex && std::string_view(name) == "tex.img" && size == 64 && Equals(tex, Pattern(64, 3)));
    ASSERT_TRUE(GetPackFile(pack_data, 4, &name, &size) == nullptr);

    constexpr int max_files = 8;
    auto          files = std::make_unique<u_int *[]>(max_files);
    auto          sizes = std::make_unique<int[]>(max_files);
    auto          names = std::make_unique<char *[]>(max_files);
    ASSERT_TRUE(GetPackFileExt(pack_data, Mutable("img"), files.get(), max_files, sizes.get(), names.get()) == 2);
    ASSERT_TRUE(files[0] == tex && sizes[1] == 5 && std::string_view(names[1]) == "face.IMG");
    ASSERT_TRUE(GetPackFileExt(pack_data, Mutable("IMG"), files.get(), 1, nullptr, nullptr) == 1);

    // The table is inside the sector-padded read, but the member's own range must still fit it.
    auto               *raw_pack = reinterpret_cast<unsigned char *>(pack_data);
    const std::uint32_t valid_offset = static_cast<std::uint32_t>(raw_pack[76 + 64]) |
                                       (static_cast<std::uint32_t>(raw_pack[76 + 65]) << 8) |
                                       (static_cast<std::uint32_t>(raw_pack[76 + 66]) << 16) |
                                       (static_cast<std::uint32_t>(raw_pack[76 + 67]) << 24);
    raw_pack[76 + 64] = 0xFF;
    raw_pack[76 + 65] = 0xFF;
    raw_pack[76 + 66] = 0xFF;
    raw_pack[76 + 67] = 0x7F;
    ASSERT_EQ(GetPackFile(pack_data, Mutable("model.mds"), &size), nullptr);
    ASSERT_EQ(size, 0);
    for (int byte = 0; byte < 4; byte++) {
        raw_pack[76 + 64 + byte] = static_cast<unsigned char>(valid_offset >> (byte * 8));
    }

    ASSERT_TRUE(LoadPackFile(Mutable("rmdat/none.pak"), pack_data, &size) == 0);
    ASSERT_TRUE(GetPackFile(Mutable("info.cfg"), &size) == nullptr);
    InitializeDataBuffer();
    ASSERT_EQ(GetPackFile(pack_data, Mutable("info.cfg"), &size), nullptr);
    ASSERT_EQ(size, 0);
    fs::remove_all(dir);
}

TEST(DataRead, NestedPackLookupUsesEachMembersExactCapacity) {
    fs::path dir = InstallStandardData("read_nested_pack");
    Bytes    info = Pattern(37, 21);
    Bytes    model = Pattern(300, 22);
    Bytes    leaf = MakePack({
        {"info.cfg",   info },
        {"trap01.mds", model}
    });
    Bytes    middle = MakePack({
        {"trap01.chr", leaf},
        {"sibling.bin", Pattern(1024, 23)}
    });
    Bytes    outer = MakePack({
        {"maindat.pac", middle}
    });
    WriteBytes(dir / "data/nested.pac", outer);
    InitCDFile();

    Clear();
    int   size = 0;
    auto *outer_pack = reinterpret_cast<u_int *>(buffer);
    ASSERT_EQ(LoadFile2(Mutable("nested.pac"), buffer, &size, 0), 1);
    ASSERT_EQ(size, static_cast<int>(outer.size()));
    DataReadSource source;
    ASSERT_TRUE(DataReadSourceRange(buffer, outer.size(), source));
    EXPECT_EQ(source.path, "nested.pac");
    EXPECT_TRUE(source.member.empty());
    EXPECT_EQ(source.offset, 0);
    EXPECT_EQ(source.size, outer.size());

    auto files = std::make_unique<u_int *[]>(1);
    auto sizes = std::make_unique<int[]>(1);
    auto names = std::make_unique<char *[]>(1);
    ASSERT_EQ(GetPackFileExt(outer_pack, Mutable("pac"), files.get(), 1, sizes.get(), names.get()), 1);
    auto *middle_pack = files[0];
    ASSERT_EQ(std::string_view(names[0]), "maindat.pac");
    ASSERT_EQ(sizes[0], static_cast<int>(middle.size()));

    auto *leaf_pack = GetPackFile(middle_pack, Mutable("trap01.chr"), &size);
    ASSERT_NE(leaf_pack, nullptr);
    const int leaf_size = size;
    ASSERT_EQ(leaf_size, static_cast<int>(leaf.size()));
    auto *info_file = GetPackFile(leaf_pack, Mutable("info.cfg"), &size);
    ASSERT_NE(info_file, nullptr);
    ASSERT_EQ(size, static_cast<int>(info.size()));
    ASSERT_TRUE(Equals(info_file, info));

    char *name = nullptr;
    auto *model_file = GetPackFile(leaf_pack, 1, &name, &size);
    ASSERT_NE(model_file, nullptr);
    ASSERT_EQ(std::string_view(name), "trap01.mds");
    ASSERT_EQ(size, static_cast<int>(model.size()));
    ASSERT_TRUE(Equals(model_file, model));
    ASSERT_TRUE(DataReadSourceRange(model_file, model.size(), source));
    EXPECT_EQ(source.path, "nested.pac");
    EXPECT_EQ(source.member, "maindat.pac/trap01.chr/trap01.mds");
    const std::uint64_t model_source_offset = source.offset;
    EXPECT_LE(source.offset + source.size, outer.size());
    EXPECT_TRUE(DataReadSourceRange(model_file, 1, source));
    EXPECT_FALSE(DataReadSourceRange(model_file, model.size() + 1, source));
    ASSERT_TRUE(DataReadSourceRange(model_file, model.size(), source));

    CDataAlloc2<1>            copied_arena;
    std::array<u_long128, 32> copied_storage{};
    copied_arena.base = reinterpret_cast<u_char *>(copied_storage.data());
    copied_arena.buffer = copied_arena.base;
    copied_arena.used = 0;
    copied_arena.limit = static_cast<int>(copied_storage.size());
    auto *copied_member = copied_arena.Alloc((static_cast<int>(model.size()) >> 4) + 1);
    std::memcpy(copied_member, model_file, model.size());
    ASSERT_TRUE(DataReadRegisterCopiedSource(copied_member, model.size(), &copied_arena, source));
    ASSERT_TRUE(DataReadRegisterCopiedSource(copied_member, model.size(), &copied_arena, source));
    CDataAlloc2<1> unrelated_arena;
    DataReadForgetPackCapacities(&unrelated_arena);
    ASSERT_TRUE(DataReadSourceRange(copied_member, model.size(), source));
    EXPECT_EQ(source.path, "nested.pac");
    ASSERT_TRUE(DataReadSourceRange(copied_member + 1, model.size() - 1, source));
    EXPECT_EQ(source.offset, model_source_offset + 1);
    EXPECT_FALSE(DataReadSourceRange(copied_member + model.size() - 4, 8, source));

    // A malformed child entry may fit in its parent's remaining bytes, but cannot extend beyond
    // the exact size of the child archive returned above.
    auto      *leaf_bytes = reinterpret_cast<unsigned char *>(leaf_pack);
    const auto too_far = static_cast<std::uint32_t>(leaf_size + 16);
    for (int byte = 0; byte < 4; ++byte) {
        leaf_bytes[72 + byte] = static_cast<unsigned char>(too_far >> (byte * 8));
    }
    ASSERT_EQ(GetPackFile(leaf_pack, Mutable("info.cfg"), &size), nullptr);
    ASSERT_EQ(size, 0);

    // Reusing the root buffer invalidates every nested view obtained from its previous contents.
    ASSERT_EQ(LoadFile2(Mutable("img/title.img"), buffer, &size, 0), 1);
    ASSERT_EQ(GetPackFile(middle_pack, Mutable("trap01.chr"), &size), nullptr);
    ASSERT_EQ(size, 0);
    ASSERT_TRUE(DataReadSourceRange(model_file, model.size(), source));
    EXPECT_EQ(source.path, "img/title.img");
    EXPECT_TRUE(source.member.empty());
    ASSERT_TRUE(DataReadSourceRange(copied_member, model.size(), source));
    EXPECT_EQ(source.path, "nested.pac");
    EXPECT_EQ(source.member, "maindat.pac/trap01.chr/trap01.mds");

    copied_arena.Reset();
    EXPECT_FALSE(DataReadSourceRange(copied_member, 1, source));
    copied_member = copied_arena.Alloc(1);
    EXPECT_FALSE(DataReadSourceRange(copied_member, 1, source));
    fs::remove_all(dir);
}

TEST(DataRead, ReadOverAnotherBuffersMemberReplacesItsSource) {
    fs::path dir = InstallStandardData("read_over_member");
    Bytes    head = Pattern(64, 30);
    Bytes    lead = Pattern(128, 31);
    Bytes    weapon = Pattern(300, 32);
    // Larger than the member it covers, so a stale member record would be the tighter match.
    Bytes portrait = Pattern(600, 33);
    WriteBytes(dir / "data/menu.pac", MakePack({
                                          {"head.bin",   head  },
                                          {"lead.bin",   lead  },
                                          {"weapon.chr", weapon}
    }));
    WriteBytes(dir / "data/portrait.bin", portrait);
    InitCDFile();

    Clear();
    int   size = 0;
    auto *pack = reinterpret_cast<u_int *>(buffer);
    ASSERT_EQ(LoadFile2(Mutable("menu.pac"), buffer, &size, 0), 1);
    auto *head_file = reinterpret_cast<unsigned char *>(GetPackFile(pack, Mutable("head.bin"), &size));
    auto *lead_file = reinterpret_cast<unsigned char *>(GetPackFile(pack, Mutable("lead.bin"), &size));
    auto *weapon_file = reinterpret_cast<unsigned char *>(GetPackFile(pack, Mutable("weapon.chr"), &size));
    ASSERT_NE(head_file, nullptr);
    ASSERT_NE(lead_file, nullptr);
    ASSERT_NE(weapon_file, nullptr);
    ASSERT_LE(head_file + head.size(), lead_file);
    ASSERT_LE(lead_file + lead.size(), weapon_file);
    DataReadSource source;
    ASSERT_TRUE(DataReadSourceRange(weapon_file, weapon.size(), source));
    EXPECT_EQ(source.member, "weapon.chr");

    // A read into another buffer, starting inside one member and covering the next: those bytes
    // now come from that file, as the shop's portrait does over the weapon menu's pack.
    unsigned char *portrait_file = lead_file + 64;
    ASSERT_LE(weapon_file + weapon.size(), portrait_file + portrait.size());
    ASSERT_EQ(LoadFile2(Mutable("portrait.bin"), portrait_file, &size, 0), 1);
    ASSERT_TRUE(DataReadSourceRange(weapon_file, weapon.size(), source));
    EXPECT_EQ(source.path, "portrait.bin");
    EXPECT_TRUE(source.member.empty());
    EXPECT_EQ(source.offset, static_cast<std::size_t>(weapon_file - portrait_file));

    // The pack keeps the bytes in front of the read, and only those.
    ASSERT_TRUE(DataReadSourceRange(lead_file, 64, source));
    EXPECT_EQ(source.member, "lead.bin");
    EXPECT_FALSE(DataReadSourceRange(lead_file, lead.size(), source));
    EXPECT_NE(GetPackFile(pack, Mutable("head.bin"), &size), nullptr);
    EXPECT_EQ(GetPackFile(pack, Mutable("lead.bin"), &size), nullptr);
    EXPECT_EQ(GetPackFile(pack, Mutable("weapon.chr"), &size), nullptr);
    fs::remove_all(dir);
}

TEST(DataRead, CopyIntoOneArenaReplacesAnotherArenasCopyBeneathIt) {
    fs::path dir = InstallStandardData("read_copy_over_copy");
    Bytes    weapon = Pattern(96, 40);
    Bytes    atla = Pattern(400, 41);
    WriteBytes(dir / "data/menu.pac", MakePack({
                                          {"weapon.img", weapon},
                                          {"atla.img",   atla  }
    }));
    InitCDFile();

    Clear();
    int   size = 0;
    auto *pack = reinterpret_cast<u_int *>(buffer);
    ASSERT_EQ(LoadFile2(Mutable("menu.pac"), buffer, &size, 0), 1);
    auto          *weapon_file = GetPackFile(pack, Mutable("weapon.img"), &size);
    auto          *atla_file = GetPackFile(pack, Mutable("atla.img"), &size);
    DataReadSource weapon_source;
    DataReadSource atla_source;
    ASSERT_TRUE(DataReadSourceRange(weapon_file, weapon.size(), weapon_source));
    ASSERT_TRUE(DataReadSourceRange(atla_file, atla.size(), atla_source));

    // Two arenas over the same memory, as the weapon menu's and the Atla pickup's both lie in read_buffer.
    std::array<u_long128, 64> shared{};
    CDataAlloc2<1>            menu_arena;
    CDataAlloc2<1>            pickup_arena;
    for (CDataAlloc2<1> *arena : {&menu_arena, &pickup_arena}) {
        arena->base = reinterpret_cast<u_char *>(shared.data());
        arena->buffer = arena->base;
        arena->used = 0;
        arena->limit = static_cast<int>(shared.size());
    }
    menu_arena.Alloc(4);
    auto *weapon_copy = menu_arena.Alloc((static_cast<int>(weapon.size()) >> 4) + 1);
    std::memcpy(weapon_copy, weapon_file, weapon.size());
    ASSERT_TRUE(DataReadRegisterCopiedSource(weapon_copy, weapon.size(), &menu_arena, weapon_source));

    // The pickup's larger copy starts in front of the menu's and covers it.
    auto *atla_copy = pickup_arena.Alloc((static_cast<int>(atla.size()) >> 4) + 1);
    ASSERT_LT(atla_copy, weapon_copy);
    std::memcpy(atla_copy, atla_file, atla.size());
    ASSERT_TRUE(DataReadRegisterCopiedSource(atla_copy, atla.size(), &pickup_arena, atla_source));

    DataReadSource source;
    ASSERT_TRUE(DataReadSourceRange(weapon_copy, 16, source));
    EXPECT_EQ(source.member, "atla.img");
    EXPECT_EQ(source.offset, atla_source.offset + static_cast<std::size_t>(weapon_copy - atla_copy));
    fs::remove_all(dir);
}

TEST(DataRead, SoundLoadsPackMembersAboveFourGiBWithoutPointerNarrowing) {
    fs::path   dir = InstallStandardData("sound_pointer_width");
    Bytes      listing = {'s', 'e', 'q', '0', '0', 'a', '.', 's', 'q', '\n'};
    Bytes      sequence(0x1a);
    const auto put = [&](std::size_t at, std::uint32_t value, std::size_t bytes) {
        for (std::size_t i = 0; i < bytes; ++i) {
            sequence[at + i] = static_cast<unsigned char>(value >> (8 * i));
        }
    };
    put(0, audio::kChunkCreator, 4);
    put(4, audio::kChunkMidi, 4);
    put(8, 0x24, 4);
    put(0x10, 0x14, 4);
    put(0x14, 6, 4);
    put(0x18, 480, 2);
    sequence.insert(sequence.end(), {0, 0x90, 60, 0x64, 0x80, 60, 0, 0xFF, 0x2F, 0});
    Bytes pack = MakePack({
        {"sound.txt", listing },
        {"seq00a.sq", sequence}
    });
    WriteBytes(dir / "data/sound/test.pak", pack);
    InitCDFile();

    std::vector<u_long128> buffer((dcdata::SectorsFor(pack.size()) * dcdata::kSector) / sizeof(u_long128));
    int                    size = 0;
    ASSERT_EQ(LoadFile2(Mutable("sound/test.pak"), buffer.data(), &size, 0), 1);
    auto *member = GetPackFile(reinterpret_cast<u_int *>(buffer.data()), Mutable("seq00a.sq"), &size);
    ASSERT_NE(member, nullptr);
    ASSERT_GT(reinterpret_cast<std::uintptr_t>(member), std::numeric_limits<std::uint32_t>::max());
    CSnd.Init(0, 0, 0, 0);

    struct StopAudio {
        ~StopAudio() { AudioOutputStop(); }
    } stop_audio;

    ASSERT_EQ(CSnd.LoadSoundFileFromPack(Mutable("sound.txt"), reinterpret_cast<u_int *>(buffer.data())), 0);
    ASSERT_NE(CSnd.GetMidiState()->port[0].sequence_address[0], nullptr);
    fs::remove_all(dir);
}

TEST(DataRead, WritefileGoesToSaveHost0) {
    fs::path dir = InstallStandardData("write");
    Bytes    data = Pattern(100, 7);

    ASSERT_TRUE(WriteFile(Mutable("host0:y:/ps2/dc_data/gdata0.edt"), data.data(), 100) == 1);
    ASSERT_TRUE(ReadBytes(dir / "save/host0/ps2/dc_data/gdata0.edt") == data);
    ASSERT_TRUE(WriteFile(Mutable("host0:debug.txt"), data.data(), 10) == 1);
    ASSERT_TRUE(fs::file_size(dir / "save/host0/debug.txt") == 10);
    ASSERT_TRUE(WriteFile(Mutable("host0:../escape.txt"), data.data(), 10) == 0);
    ASSERT_TRUE(!fs::exists(dir / "save/escape.txt"));
    fs::remove_all(dir);
}

TEST(DataRead, PathsFromArguments) {
    PathTestScope paths;
    fs::path      dir = TempDir("paths_args");
    std::string   data = (dir / "d").string();
    std::string   save = "--save=" + (dir / "s").string();
    const char   *argv[] = {"darkcloud", "--data", data.c_str(), "--frames",
                            "3", save.c_str(), "--database", nullptr};
    int           argc = PathsConsumeArgs(7, argv);
    ASSERT_TRUE(argc == 4);
    ASSERT_TRUE(std::string_view(argv[0]) == "darkcloud" && std::string_view(argv[1]) == "--frames");
    ASSERT_TRUE(std::string_view(argv[2]) == "3" && std::string_view(argv[3]) == "--database" && argv[4] == nullptr);
    ASSERT_TRUE(PathsDataRoot() == dir / "d");
    ASSERT_TRUE(PathsSaveRoot() == dir / "s" && fs::is_directory(dir / "s"));
    fs::remove_all(dir);
}

TEST(DataRead, PathsFromEnvironment) {
    PathTestScope paths;
    fs::path      dir = TempDir("paths_env");
    dc::test::SetEnv("DC_DATA", (dir / "env_data"), 1);
    dc::test::SetEnv("DC_SAVE", (dir / "env_save"), 1);
    ASSERT_TRUE(PathsDataRoot() == dir / "env_data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "env_save" && fs::is_directory(dir / "env_save"));
    PathsSetDataRoot(dir / "flag");
    ASSERT_TRUE(PathsDataRoot() == dir / "flag");
    fs::remove_all(dir);
}

TEST(DataRead, PathsDefaultToWorkingDirectory) {
    PathTestScope paths;
    fs::path      dir = TempDir("paths_cwd");
    dc::test::UnsetEnv("DC_DATA");
    dc::test::UnsetEnv("DC_SAVE");
    fs::create_directories(dir / "data");
    fs::current_path(dir);
    ASSERT_TRUE(PathsDataRoot() == dir / "data");
    ASSERT_TRUE(PathsSaveRoot() == dir / "save" && fs::is_directory(dir / "save"));
    fs::current_path(fs::temp_directory_path());
    fs::remove_all(dir);
}

namespace {

struct MetadataDataScope {
    fs::path directory;

    explicit MetadataDataScope(const char *tag, bool cache = true) : directory(TempDir(tag)) {
        DataReadReset();
        dc::test::SetEnv("DC_TEXTURE_SOURCE_CACHE", cache ? "1" : "0", 1);
        PathsSetDataRoot(directory / "data");
        PathsSetSaveRoot(directory / "save");
    }

    ~MetadataDataScope() {
        DataReadReset();
        std::error_code ignored;
        fs::remove_all(directory, ignored);
    }

    void Pack() { dcdata::PackDirectory(directory / "source", directory / "data.zip"); }
};

} // namespace

TEST(DataRead, MetadataSnapshotsReuseOnlyArchivedBindings) {
    MetadataDataScope scope("metadata_archive");
    const Bytes       expected = Pattern(1024, 81);
    WriteBytes(scope.directory / "source/folder/asset.bin", expected);
    scope.Pack();
    const auto first = DataReadResolvedMetadataSnapshot("folder/asset.bin");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(*first, expected);
    const auto second = DataReadResolvedMetadataSnapshot("/FOLDER\\ASSET.BIN");
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(first.get(), second.get());
    Bytes fresh(expected.size(), 0);
    ASSERT_TRUE(DataReadReadResolvedFile("folder/asset.bin", fresh));
    EXPECT_EQ(fresh, expected);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("folder/asset.bin").get(), first.get());
}

TEST(DataRead, MetadataSnapshotsStayFreshWhenDisabled) {
    MetadataDataScope scope("metadata_disabled", false);
    const Bytes       expected = Pattern(257, 82);
    WriteBytes(scope.directory / "source/asset.bin", expected);
    scope.Pack();
    const auto first = DataReadResolvedMetadataSnapshot("asset.bin");
    const auto second = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first.get(), second.get());
    EXPECT_EQ(*first, expected);
    EXPECT_EQ(*second, expected);
}

TEST(DataRead, MetadataLooseReadsObserveEditsAndRetryErrors) {
    MetadataDataScope scope("metadata_loose");
    const auto        path = scope.directory / "data/asset.bin";
    const Bytes       first_bytes = Pattern(257, 83);
    const Bytes       changed_bytes = Pattern(first_bytes.size(), 84);
    WriteBytes(path, first_bytes);
    const auto first = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(first, nullptr);
    WriteBytes(path, changed_bytes);
    const auto changed = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(changed, nullptr);
    EXPECT_NE(first.get(), changed.get());
    EXPECT_EQ(*first, first_bytes);
    EXPECT_EQ(*changed, changed_bytes);
    WriteBytes(path, Bytes{1});
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("asset.bin"), nullptr);
    WriteBytes(path, first_bytes);
    const auto recovered = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(recovered, nullptr);
    EXPECT_EQ(*recovered, first_bytes);
}

TEST(DataRead, MetadataSnapshotsPreserveNormalizedAndAmbiguousSources) {
    MetadataDataScope scope("metadata_normalized");
    const Bytes       base = Pattern(32, 85);
    const Bytes       overlay = Pattern(32, 86);
    WriteBytes(scope.directory / "source/hero.chr", base);
    WriteBytes(scope.directory / "source/normalized/hero.chr", overlay);
    scope.Pack();
    const auto archived = DataReadResolvedMetadataSnapshot("normalized/hero.chr");
    ASSERT_NE(archived, nullptr);
    EXPECT_EQ(*archived, overlay);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("hero.chr"), nullptr);
    DataReadReset();
    fs::remove(scope.directory / "data.zip");
    WriteBytes(scope.directory / "data/hero.chr", base);
    WriteBytes(scope.directory / "data/normalized/hero.chr", overlay);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("normalized/hero.chr"), nullptr);
    Bytes fresh{1};
    EXPECT_FALSE(DataReadReadResolvedFile("normalized/hero.chr", fresh));
    EXPECT_TRUE(fresh.empty());
    EXPECT_EQ(*archived, overlay);
}

TEST(DataRead, MetadataSnapshotEpochsPreservePinsAndRefreshReplacedArchives) {
    MetadataDataScope scope("metadata_epoch");
    const Bytes       original = Pattern(100, 87);
    const Bytes       replacement = Pattern(original.size(), 88);
    WriteBytes(scope.directory / "source/asset.bin", original);
    scope.Pack();
    const auto old = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(old, nullptr);
    DataReadReset();
    fs::remove(scope.directory / "data.zip");
    WriteBytes(scope.directory / "source/asset.bin", replacement);
    scope.Pack();
    const auto replaced = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(replaced, nullptr);
    EXPECT_NE(old.get(), replaced.get());
    EXPECT_EQ(*old, original);
    EXPECT_EQ(*replaced, replacement);
    InitCDFile();
    const auto reinitialized = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(reinitialized, nullptr);
    EXPECT_NE(reinitialized.get(), replaced.get());
    EXPECT_EQ(*replaced, replacement);
    EXPECT_EQ(*reinitialized, replacement);
    DataReadReset();
    const auto other_root = scope.directory / "other";
    WriteBytes(other_root / "source/asset.bin", original);
    dcdata::PackDirectory(other_root / "source", other_root / "data.zip");
    PathsSetDataRoot(other_root / "data");
    const auto rebound = DataReadResolvedMetadataSnapshot("asset.bin");
    ASSERT_NE(rebound, nullptr);
    EXPECT_EQ(*rebound, original);
    EXPECT_EQ(*reinitialized, replacement);
}

TEST(DataRead, MetadataSnapshotsEvictTheLeastRecentOfFourFiles) {
    MetadataDataScope scope("metadata_lru");
    for (int file = 0; file < 5; ++file) {
        WriteBytes(scope.directory / "source" / (std::to_string(file) + ".bin"), Pattern(64, file));
    }
    scope.Pack();
    const auto first = DataReadResolvedMetadataSnapshot("0.bin");
    const auto second = DataReadResolvedMetadataSnapshot("1.bin");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(DataReadResolvedMetadataSnapshot("2.bin"), nullptr);
    ASSERT_NE(DataReadResolvedMetadataSnapshot("3.bin"), nullptr);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("1.bin").get(), second.get());
    ASSERT_NE(DataReadResolvedMetadataSnapshot("4.bin"), nullptr);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("1.bin").get(), second.get());
    const auto reloaded = DataReadResolvedMetadataSnapshot("0.bin");
    ASSERT_NE(reloaded, nullptr);
    EXPECT_NE(first.get(), reloaded.get());
    EXPECT_EQ(*first, Pattern(64, 0));
    EXPECT_EQ(*reloaded, *first);
}

TEST(DataRead, MetadataSnapshotBudgetEvictsAndOversizeReadsStayFresh) {
    MetadataDataScope     scope("metadata_budget");
    constexpr std::size_t six_mib = 6 * 1024 * 1024;
    for (int file = 0; file < 3; ++file) {
        WriteBytes(scope.directory / "source" / (std::to_string(file) + ".bin"), Pattern(six_mib, file));
    }
    WriteBytes(scope.directory / "source/oversize.bin", Pattern(16 * 1024 * 1024 + 1, 9));
    scope.Pack();
    auto first = DataReadResolvedMetadataSnapshot("0.bin");
    ASSERT_NE(first, nullptr);
    const std::weak_ptr<const std::vector<std::uint8_t>> evicted = first;
    first.reset();
    const auto second = DataReadResolvedMetadataSnapshot("1.bin");
    ASSERT_NE(second, nullptr);
    const auto third = DataReadResolvedMetadataSnapshot("2.bin");
    ASSERT_NE(third, nullptr);
    EXPECT_TRUE(evicted.expired());
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("1.bin").get(), second.get());
    const auto oversize_first = DataReadResolvedMetadataSnapshot("oversize.bin");
    const auto oversize_second = DataReadResolvedMetadataSnapshot("oversize.bin");
    ASSERT_NE(oversize_first, nullptr);
    ASSERT_NE(oversize_second, nullptr);
    EXPECT_NE(oversize_first.get(), oversize_second.get());
    EXPECT_EQ(*oversize_first, *oversize_second);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("1.bin").get(), second.get());
}

TEST(DataRead, MetadataSnapshotsRejectCorruptionAndRecoverAfterReset) {
    MetadataDataScope     scope("metadata_corrupt");
    auto                  fixture = MakeStoredIndexFixture(3, 0);
    constexpr std::size_t first_payload = 30 + sizeof("first.bin") - 1;
    fixture.bytes[first_payload] ^= 1;
    WriteBytes(scope.directory / "data.zip", fixture.bytes);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("first.bin"), nullptr);
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("first.bin"), nullptr);
    DataReadReset();
    fixture.bytes[first_payload] ^= 1;
    WriteBytes(scope.directory / "data.zip", fixture.bytes);
    const auto recovered = DataReadResolvedMetadataSnapshot("first.bin");
    ASSERT_NE(recovered, nullptr);
    EXPECT_EQ(*recovered, (Bytes{1, 2, 3}));
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("first.bin").get(), recovered.get());
}

TEST(DataRead, MetadataSnapshotsPreserveImgOrdinalsInsidePackMembers) {
    MetadataDataScope scope("metadata_img_ordinals");
    Bytes             img(248);
    std::memcpy(img.data(), "IMG\0", 4);
    Put32(img, 4, 2);
    std::memcpy(img.data() + 16, "DUP", 3);
    std::memcpy(img.data() + 64, "DUP", 3);
    Put32(img, 16 + 32, 112);
    Put32(img, 64 + 32, 180);
    Put32(img, 112 + 16 + 12, 48);
    Put32(img, 180 + 16 + 12, 48);
    Bytes bad_img = img;
    Put32(bad_img, 64 + 32, 112);
    const Bytes pack = MakePack({
        {"dup.img", img    },
        {"bad.img", bad_img}
    });
    WriteBytes(scope.directory / "source/img.pak", pack);
    scope.Pack();
    int size = 0;
    ASSERT_EQ(LoadFile2(Mutable("img.pak"), buffer, &size, 0), 1);
    auto *member = GetPackFile(reinterpret_cast<u_int *>(buffer), Mutable("dup.img"), &size);
    ASSERT_NE(member, nullptr);
    const auto parent = DataReadResolvedMetadataSnapshot("img.pak");
    ASSERT_NE(parent, nullptr);
    for (const auto [offset, ordinal] : std::array<std::pair<std::size_t, unsigned>, 2>{
             {{176, 0}, {244, 1}}
    }) {
        DataReadSource source;
        ASSERT_TRUE(DataReadSourceRange(reinterpret_cast<const std::uint8_t *>(member) + offset, 4, source));
        EXPECT_EQ(PortTextureEntryIndexFromSource(source), ordinal);
    }
    auto *ambiguous = GetPackFile(reinterpret_cast<u_int *>(buffer), Mutable("bad.img"), &size);
    ASSERT_NE(ambiguous, nullptr);
    DataReadSource invalid;
    ASSERT_TRUE(DataReadSourceRange(reinterpret_cast<const std::uint8_t *>(ambiguous) + 176, 4, invalid));
    EXPECT_FALSE(PortTextureEntryIndexFromSource(invalid).has_value());
    EXPECT_EQ(DataReadResolvedMetadataSnapshot("img.pak").get(), parent.get());
}

} // namespace dc::test
