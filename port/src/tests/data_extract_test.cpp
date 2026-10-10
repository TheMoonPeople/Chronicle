#include <gtest/gtest.h>

#include <cstring>
#include <numeric>
#include <tuple>

#include "data_fixture.hpp"

using namespace datafix;

namespace {

fs::path WriteStandardIso(const fs::path &dir, const Disc &disc) {
    fs::path iso = dir / "dark cloud.iso";
    WriteBytes(iso, MakeIso("DATA.DAT;1", disc.dat, "Data.Hd2;1", disc.hd2));
    return iso;
}

template <class F>
bool Throws(F f, std::string_view needle) {
    try {
        f();
    } catch (const dcdata::Error &error) {
        return std::string_view(error.what()).find(needle) != std::string_view::npos;
    }
    return false;
}

void CheckExtracted(const fs::path &out, const Disc &disc) {
    ASSERT_TRUE(ReadBytes(out / "dun/pack/maindat.pac") == disc.files[0].data);
    ASSERT_TRUE(ReadBytes(out / "img/title.img") == disc.files[1].data);
    ASSERT_TRUE(ReadBytes(out / "sound/bgm/b01.snd") == disc.files[2].data);
    ASSERT_TRUE(ReadBytes(out / "rmdat/rmdat1.pak") == disc.files[3].data);
    ASSERT_TRUE(ReadBytes(out / "meswin/systeme.bin") == disc.files[4].data);
    ASSERT_TRUE(fs::is_regular_file(out / "empty.bin") && fs::file_size(out / "empty.bin") == 0);
    ASSERT_TRUE(ReadBytes(out / "data.hd2") == disc.hd2);
    // Listed rather than looked up: macOS's file system ignores case.
    for (const fs::directory_entry &entry : fs::directory_iterator(out)) {
        ASSERT_TRUE(entry.path().filename() != "DUN");
    }
}

} // namespace

TEST(DataExtract, IsoReadsRootDirectory) {
    fs::path dir = TempDir("iso_root");
    Disc     disc = StandardDisc();
    fs::path iso = WriteStandardIso(dir, disc);

    {
        dcdata::Iso9660               volume(iso);
        std::vector<dcdata::IsoEntry> entries = volume.List(volume.Root());
        ASSERT_TRUE(entries.size() == 2);
        ASSERT_TRUE(entries[0].name == "DATA.DAT;1" && entries[0].extent == 20);
        ASSERT_TRUE(entries[0].size == disc.dat.size());
        ASSERT_TRUE(entries[1].name == "Data.Hd2;1" && entries[1].size == disc.hd2.size());
        ASSERT_TRUE(volume.Find(volume.Root(), "data.hd2").has_value());
        ASSERT_TRUE(!volume.Find(volume.Root(), "DATA.HED").has_value());
        ASSERT_TRUE(dcdata::Iso9660::StripVersion("SLES_123.45;1") == "SLES_123.45");
        ASSERT_TRUE(dcdata::Iso9660::StripVersion("NOEXT.;1") == "NOEXT");

        dcdata::Archive archive = dcdata::OpenArchive(iso);
        ASSERT_TRUE(archive.image);
        ASSERT_TRUE(archive.dat.offset == 20 * dcdata::kSector && archive.dat.size == disc.dat.size());
        ASSERT_TRUE(dcdata::ReadExtent(archive.hd2) == disc.hd2);
    }
    fs::remove_all(dir);
}

TEST(DataExtract, IndexParsesAsPal) {
    Disc                        disc = StandardDisc();
    std::vector<dcdata::Record> records = dcdata::ParseIndex(disc.hd2);
    ASSERT_TRUE(records.size() == 7);
    ASSERT_TRUE(records[0].name == "DUN/PACK/MAINDAT.PAC" && records[0].path == "dun/pack/maindat.pac");
    ASSERT_TRUE(records[0].size == 5000 && records[0].sector == 1 && records[0].sectors == 3);
    ASSERT_TRUE(records[2].name == "/sound/bgm/b01.SND" && records[2].path == "sound/bgm/b01.snd");
    ASSERT_TRUE(records[6].size == 0 && records[6].sectors == 0);
    ASSERT_TRUE(records[1].offset == records[1].sector * dcdata::kSector);
}

TEST(DataExtract, IndexRejectsBadRecords) {
    Disc disc = StandardDisc();

    Bytes short_index(disc.hd2.begin(), disc.hd2.begin() + 31);
    ASSERT_TRUE(Throws([&] { dcdata::ParseIndex(short_index); }, "too short"));

    Bytes overcount = disc.hd2;
    Put32(overcount, 0, static_cast<std::uint32_t>(overcount.size() + 32));
    ASSERT_TRUE(Throws([&] { dcdata::ParseIndex(overcount); }, "claims"));

    Bytes far_name = disc.hd2;
    Put32(far_name, 32, static_cast<std::uint32_t>(far_name.size()));
    ASSERT_TRUE(Throws([&] { dcdata::ParseIndex(far_name); }, "record 1: name offset"));

    Bytes unterminated = disc.hd2;
    unterminated.pop_back();
    ASSERT_TRUE(Throws([&] { dcdata::ParseIndex(unterminated); }, "not terminated"));

    Disc escaping = MakeDisc({
        {"dun\\..\\..\\etc\\passwd", Pattern(4, 0)}
    });
    ASSERT_TRUE(Throws([&] { dcdata::ParseIndex(escaping.hd2); }, "unusable path"));

    Bytes negative = disc.hd2;
    Put32(negative, 32 + 20, 0xFFFFFFF0);
    ASSERT_TRUE(Throws([&] { dcdata::ParseIndex(negative); }, "negative"));
}

TEST(DataExtract, FromIso) {
    fs::path dir = TempDir("extract_iso");
    Disc     disc = StandardDisc();
    fs::path iso = WriteStandardIso(dir, disc);
    fs::path out = dir / "data";

    dcdata::Summary summary = dcdata::Extract(dcdata::OpenArchive(iso), out, nullptr);
    ASSERT_TRUE(summary.records == 7);
    ASSERT_TRUE(summary.duplicates == 1);
    ASSERT_TRUE(summary.written == kStandardReachable && summary.kept == 0);
    ASSERT_TRUE(summary.warnings == 0);
    CheckExtracted(out, disc);
    ASSERT_TRUE(dcdata::Mismatched(dcdata::ParseIndex(disc.hd2), out).empty());
    // A release the table lacks, without NTSC's layout: PAL's five languages.
    std::string languages = "{\"release\": \"unknown release, PAL layout\", \"languages\": [2, 3, 4, 5, 6]}\n";
    EXPECT_EQ(ReadBytes(out / "languages.json"), Bytes(languages.begin(), languages.end()));
    fs::remove_all(dir);
}

TEST(DataExtract, IsIdempotent) {
    fs::path dir = TempDir("extract_again");
    Disc     disc = StandardDisc();
    fs::path iso = WriteStandardIso(dir, disc);
    fs::path out = dir / "data";

    dcdata::Extract(dcdata::OpenArchive(iso), out, nullptr);
    dcdata::Summary again = dcdata::Extract(dcdata::OpenArchive(iso), out, nullptr);
    ASSERT_TRUE(again.written == 0 && again.kept == kStandardReachable);

    fs::resize_file(out / "img/title.img", 10);
    ASSERT_TRUE(dcdata::Mismatched(dcdata::ParseIndex(disc.hd2), out).size() == 1);
    dcdata::Summary repaired = dcdata::Extract(dcdata::OpenArchive(iso), out, nullptr);
    ASSERT_TRUE(repaired.written == 1 && repaired.kept == kStandardReachable - 1);
    CheckExtracted(out, disc);
    fs::remove_all(dir);
}

TEST(DataExtract, NormalizesNtscAssets) {
    fs::path dir = TempDir("normalize_ntsc");
    Bytes   start = Pattern(32, 7);
    Bytes   event(24, 0);
    Put32(event, 20, 24);
    event.insert(event.end(), {1, 2, 3, 4});
    Bytes image(16 + 2 * 48 + 8, 0);
    std::memcpy(image.data(), "IM2", 3);
    Put32(image, 4, 2);
    std::memcpy(image.data() + 16, "other", 5);
    Put32(image, 16 + 32, 16 + 2 * 48);
    std::memcpy(image.data() + 64, "wepstatus", 9);
    Put32(image, 64 + 32, 16 + 2 * 48 + 4);
    std::iota(image.begin() + 16 + 2 * 48, image.end(), 1);
    Disc disc = MakeDisc({
        {"gedit/system/esys.pak", MakePack({{"cursor.img", Pattern(8, 1)}})},
        {"meswin/mes_tex.pak", Pattern(16, 2)},
        {"rmdat/rmdat1.pak", MakePack({{"start.img", start}})},
        {"dun/img/us/dname00.img", Pattern(16, 3)},
        {"dun/script/d01/event.stb", event},
        {"commenu/a_eng/dungeon/dunmenu5.pak", MakePack({{"btlmenu.img", image}})}
    });
    fs::path out = dir / "data";
    dcdata::Archive archive = dcdata::OpenArchive(WriteStandardIso(dir, disc));
    dcdata::Extract(archive, out, nullptr);

    EXPECT_EQ(ReadBytes(out / "meswin/mes_tex_2.pak"), disc.files[1].data);
    EXPECT_EQ(ReadBytes(out / "dun/img/us_e/dname00.img"), disc.files[3].data);
    auto members = dcdata::ReadPack(ReadBytes(out / "normalized/rmdat/rmdat1.pak"));
    ASSERT_EQ(members.size(), 5);
    EXPECT_EQ(members[1].name, "start_f.img");
    EXPECT_EQ(members[1].data, start);
    EXPECT_EQ(ReadBytes(out / "dun/script/d01/d01_1.mes"), (Bytes{1, 2, 3, 4}));
    std::string languages = "{\"release\": \"unknown release, NTSC layout\", \"languages\": [1]}\n";
    EXPECT_EQ(ReadBytes(out / "languages.json"), Bytes(languages.begin(), languages.end()));
    EXPECT_EQ(ReadBytes(out / "dun/script/d01/d01_2.mes"), (Bytes{1, 2, 3, 4}));
    auto battle = dcdata::ReadPack(ReadBytes(out / "normalized/commenu/a_eng/dungeon/dunmenu5.pak"));
    ASSERT_EQ(battle.size(), 2);
    EXPECT_EQ(battle[1].name, "btlmenu2.img");
    EXPECT_EQ(dcdata::Le32(battle[1].data.data() + 4), 1);
    EXPECT_EQ(battle[1].data.size(), 68);
    EXPECT_EQ(Bytes(battle[1].data.begin() + 64, battle[1].data.end()), Bytes(image.begin() + 116, image.end()));
    EXPECT_EQ(dcdata::Le32(battle[0].data.data() + 4), 1);
    EXPECT_EQ(std::memcmp(battle[0].data.data() + 16, "other", 6), 0);
    EXPECT_EQ(dcdata::Le32(battle[0].data.data() + 16 + 32), 64);
    EXPECT_EQ(Bytes(battle[0].data.begin() + 64, battle[0].data.end()), Bytes(image.begin() + 112, image.begin() + 116));
    EXPECT_TRUE(dcdata::Mismatched(dcdata::ParseIndex(disc.hd2), out).empty());

    dcdata::Summary again = dcdata::Extract(archive, out, nullptr);
    EXPECT_EQ(again.written, 0);
    EXPECT_EQ(again.kept, 6);
    fs::remove_all(dir);
}

// An IM2 bank of 16 by 16 eight-bit pictures, each with its own palette and pixel fill.
static Bytes MakeIconBank(const std::vector<std::tuple<std::string, std::uint32_t, unsigned char>> &pictures) {
    constexpr std::size_t picture = 16 + 48 + 256 + 1024;
    Bytes                 bank(16 + pictures.size() * 48, 0);
    std::memcpy(bank.data(), "IM2", 3);
    Put32(bank, 4, static_cast<std::uint32_t>(pictures.size()));
    for (std::size_t i = 0; i < pictures.size(); i++) {
        const auto &[name, colour, pixel] = pictures[i];
        std::memcpy(bank.data() + 16 + i * 48, name.c_str(), name.size());
        Put32(bank, 16 + i * 48 + 32, static_cast<std::uint32_t>(bank.size()));
        Bytes tim(picture, 0);
        std::memcpy(tim.data(), "TIM2", 4);
        Put32(tim, 16, 0x30030); // the wrong total the game's images carry
        Put32(tim, 16 + 4, 1024);
        Put32(tim, 16 + 8, 256);
        tim[16 + 12] = 0x30;
        tim[16 + 15] = 0x01;
        tim[16 + 19] = 5;
        tim[16 + 20] = 16;
        tim[16 + 22] = 16;
        std::fill(tim.begin() + 64, tim.begin() + 64 + 256, pixel);
        for (int c = 0; c < 256; c++) {
            Put32(tim, 64 + 256 + c * 4, colour + c);
        }
        bank.insert(bank.end(), tim.begin(), tim.end());
    }
    return bank;
}

TEST(DataExtract, NormalizesIconSheets) {
    fs::path dir = TempDir("normalize_icons");
    Bytes    american = MakeIconBank({
        {"quickchara", 0x300, 3},
        {"wepicon",    0x200, 4}
    });
    Bytes    english = MakeIconBank({
        {"wepicon", 0x100, 5}
    });
    Disc     disc = MakeDisc({
        {"commenu/a_eng/itempack.img",           MakeIconBank({{"itempack", 0x100, 1}})                             },
        {"commenu/a_eng/quickchr.pac",           MakePack({{"qchr.mes", Pattern(8, 2)}, {"quickchr.img", american}})},
        {"commenu/a_eng/itemlst.img",            MakeIconBank({{"wepicon", 0x200, 4}, {"itemicon", 0x200, 7}})      },
        {"commenu/a_eng/dunenter/dunenter2.pak", MakePack({{"dunenter.img", english}})                              },
        {"commenu/a_eng/kgetoan2.img",           MakeIconBank({{"wepicon", 0x200, 4}})                              },
        {"commenu/a_eng/_charatex.img",          MakeIconBank({{"wepicon", 0x200, 4}})                              },
        {"commenu/a_eng/manual/m10.pac",         Pattern(80, 9)                                                     },
        {"commenu/a_fre/itempack.img",           MakeIconBank({{"itempack", 0x200, 6}})                             },
        {"commenu/a_fre/quickchr.pac",           MakePack({{"quickchr.img", american}})                             },
        {"commenu/a_fre/dunenter/dunenter2.pak", MakePack({{"dunenter.img", english}})                              },
    });
    fs::path out = dir / "data";
    dcdata::Extract(dcdata::OpenArchive(WriteStandardIso(dir, disc)), out, nullptr);

    // English: the American sheets take the English one; the rest of each file is as it was.
    dcdata::Im2Picture donor = dcdata::FindIm2Picture(english, "wepicon");
    auto               holds_donor = [&](const Bytes &bank) {
        dcdata::Im2Picture icons = dcdata::FindIm2Picture(bank, "wepicon");
        return icons.size == donor.size &&
               std::equal(english.begin() + donor.offset, english.begin() + donor.offset + donor.size, bank.begin() + icons.offset);
    };
    auto quick = dcdata::ReadPack(ReadBytes(out / "normalized/commenu/a_eng/quickchr.pac"));
    ASSERT_EQ(quick.size(), 2);
    EXPECT_EQ(quick[0].data, Pattern(8, 2));
    EXPECT_TRUE(holds_donor(quick[1].data));
    dcdata::Im2Picture portraits = dcdata::FindIm2Picture(quick[1].data, "quickchara");
    ASSERT_NE(portraits.size, 0);
    EXPECT_TRUE(std::equal(american.begin() + portraits.offset, american.begin() + portraits.offset + portraits.size,
                           quick[1].data.begin() + portraits.offset));
    EXPECT_TRUE(holds_donor(ReadBytes(out / "normalized/commenu/a_eng/itemlst.img")));
    // The main menu's sheet too, but not a leftover the game never loads, and a .pac without a pack.
    EXPECT_TRUE(holds_donor(ReadBytes(out / "normalized/commenu/a_eng/kgetoan2.img")));
    EXPECT_FALSE(fs::exists(out / "normalized/commenu/a_eng/_charatex.img"));
    EXPECT_FALSE(fs::exists(out / "normalized/commenu/a_eng/manual/m10.pac"));
    // French: its sheet already shares itempack's palette.
    EXPECT_FALSE(fs::exists(out / "normalized/commenu/a_fre/quickchr.pac"));
    fs::remove_all(dir);
}

TEST(DataExtract, KnowsReleasesByTheirIndex) {
    Bytes index = Pattern(64, 3);
    ASSERT_EQ(dcdata::IdentifyRelease(index), nullptr);
    for (const dcdata::Release &release : dcdata::KnownReleases()) {
        ASSERT_FALSE(release.languages.empty());
        for (const dcdata::Release &other : dcdata::KnownReleases()) {
            ASSERT_TRUE(&release == &other || release.index_fnv != other.index_fnv);
        }
    }
    // FNV-1a's published value for "a".
    const unsigned char a[] = {'a'};
    ASSERT_EQ(dcdata::Fnv1a64(a), 0xaf63dc4c8601ec8cULL);
}

TEST(DataExtract, FromDirectory) {
    fs::path dir = TempDir("extract_dir");
    Disc     disc = StandardDisc();
    WriteBytes(dir / "iso/DATA.DAT", disc.dat);
    WriteBytes(dir / "iso/data.HD2", disc.hd2);
    WriteBytes(dir / "iso/SYSTEM.CNF", Pattern(10, 0));

    dcdata::Archive archive = dcdata::OpenArchive(dir / "iso");
    ASSERT_TRUE(!archive.image);
    dcdata::Summary summary = dcdata::Extract(archive, dir / "data", nullptr);
    ASSERT_TRUE(summary.written == kStandardReachable);
    CheckExtracted(dir / "data", disc);

    ASSERT_TRUE(Throws([&] { dcdata::OpenArchive(dir / "data"); }, "holds no DATA.DAT"));
    fs::remove_all(dir);
}

TEST(DataExtract, RejectsTruncation) {
    fs::path dir = TempDir("extract_cut");
    Disc     disc = StandardDisc();

    Bytes short_dat(disc.dat.begin(), disc.dat.end() - 2 * dcdata::kSector);
    WriteBytes(dir / "cut/DATA.DAT", short_dat);
    WriteBytes(dir / "cut/DATA.HD2", disc.hd2);
    ASSERT_TRUE(Throws([&] { dcdata::Extract(dcdata::OpenArchive(dir / "cut"), dir / "data", nullptr); },
                       "DATA.DAT is truncated"));
    ASSERT_TRUE(!fs::exists(dir / "data"));

    Bytes image = MakeIso("DATA.DAT;1", disc.dat, "DATA.HD2;1", disc.hd2);
    image.resize(image.size() - dcdata::kSector);
    WriteBytes(dir / "cut.iso", image);
    ASSERT_TRUE(Throws([&] { dcdata::OpenArchive(dir / "cut.iso"); }, "truncated"));

    WriteBytes(dir / "tiny.iso", Bytes(100, 0));
    ASSERT_TRUE(Throws([&] { dcdata::OpenArchive(dir / "tiny.iso"); }, "no ISO 9660 primary volume descriptor"));

    WriteBytes(dir / "junk.iso", Bytes(40 * dcdata::kSector, 0x55));
    ASSERT_TRUE(Throws([&] { dcdata::OpenArchive(dir / "junk.iso"); }, "not an ISO 9660 image"));
    fs::remove_all(dir);
}

// An itempack.img: one 256 by 192 eight-bit picture named itempack, every texel `fill` but those of
// the floor plate's first row, `plate`, and colour i of the palette at the CLUT slot of value i.
static Bytes MakeItempack(const std::vector<std::uint32_t> &colours, unsigned char fill, unsigned char plate) {
    constexpr std::size_t pixels = 256 * 192;
    Bytes                 tim(16 + 48 + pixels + 1024, 0);
    std::memcpy(tim.data(), "TIM2", 4);
    Put32(tim, 16 + 4, 1024);
    Put32(tim, 16 + 8, pixels);
    tim[16 + 0x0C] = 0x30;
    tim[16 + 0x0F] = 0x01;
    tim[16 + 0x13] = 5;
    tim[16 + 0x15] = 1;
    tim[16 + 0x16] = 192;
    std::fill(tim.begin() + 64, tim.begin() + 64 + pixels, fill);
    for (int x = 0x9A; x < 0x9A + 0x66; x++) {
        tim[64 + dcdata::Tim2T8Texel(x, 1, 256)] = plate;
    }
    for (int i = 0; i < 256; i++) {
        Put32(tim, 64 + pixels + dcdata::Tim2T8Slot(i) * 4, colours[i]);
    }
    Bytes bank(16 + 48, 0);
    std::memcpy(bank.data(), "IM2", 3);
    Put32(bank, 4, 1);
    std::memcpy(bank.data() + 16, "itempack", 8);
    Put32(bank, 16 + 32, 16 + 48);
    bank.insert(bank.end(), tim.begin(), tim.end());
    return bank;
}

// The July 12 PAL prototype's blank floor plates take American English's, in each sheet's palette.
TEST(DataExtract, FillsBlankFloorPlates) {
    fs::path dir = TempDir("floor_plates");
    std::vector<std::uint32_t> american(256), british(256);
    for (int i = 0; i < 256; i++) {
        american[i] = 0x80000000u | (i * 0x010101u);         // grey i, opaque
        british[i] = 0x80000000u | ((255 - i) * 0x010101u); // the same greys, reversed
    }
    american[0] = 0x00000000u; // transparent
    british[7] = 0x00123456u;  // transparent, elsewhere
    WriteBytes(dir / "commenu/a_usa/itempack.img", MakeItempack(american, 0, 40));
    WriteBytes(dir / "commenu/a_eng/itempack.img", MakeItempack(british, 200, 200));
    dcdata::FillFloorPlates(dir);

    Bytes          sheet = ReadBytes(dir / "normalized/commenu/a_eng/itempack.img");
    const unsigned char *pixels = sheet.data() + 16 + 48 + 64;
    auto           value = [&](int x, int y) { return pixels[dcdata::Tim2T8Texel(x, y, 256)]; };
    EXPECT_EQ(value(0x9A, 1), 255 - 40);    // grey 40, at its place in the British palette
    EXPECT_EQ(value(0x9A + 0x65, 1), 255 - 40);
    EXPECT_EQ(value(0x9A, 2), 7);           // the American plate's transparent texels
    EXPECT_EQ(value(0xDA, 0x2B + 0x28), 7); // and the back floor's plate
    EXPECT_EQ(value(0x99, 1), 200);         // outside the plates: the sheet's own
    EXPECT_EQ(value(0x9A, 0x2A), 200);
    EXPECT_EQ(ReadBytes(dir / "commenu/a_eng/itempack.img"), MakeItempack(british, 200, 200));
    EXPECT_FALSE(fs::exists(dir / "normalized/commenu/a_usa/itempack.img"));
    fs::remove_all(dir);
}
