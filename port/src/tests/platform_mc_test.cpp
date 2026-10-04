#include <gtest/gtest.h>
#include <libmc.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "memorycardaccess.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"
#include "savedata.hpp"

// Drives CMemoryCardAccess as the save menu does: SetFuncNo, then Step once
// per frame until the operation finishes, on a save root in a temporary
// directory.

namespace fs = std::filesystem;

namespace {

constexpr int         kImageSize = 0x136A7;
constexpr int         kSaveDataSize = 0x131C0;
constexpr int         kChecksumOffset = 0x131E0;
constexpr int         kSaveMapNoOffset = 0x1C8;
constexpr const char *kSaveDir = "BESCES-50295dkcloud";

// SetBuff and the game's int casts need the buffer below 2 GiB: static
// storage in the non-PIE executable is.
alignas(64) char g_menu_buffer[0x30000];
alignas(64) CSaveData g_save;
CMemoryCardAccess g_mc;
CMemoryCardAccess g_boot;
char              g_icons[3][100];
char              g_icon_names[3][16] = {"dkicon.ico", "dkicon_c.ico", "dkicon_d.ico"};

fs::path UseTempSaveRoot() {
    fs::path root = fs::temp_directory_path() / ("dc_mc_test_" + std::to_string(dc::test::ProcessId()));
    fs::remove_all(root);
    fs::create_directories(root);
    dc::test::SetEnv("DC_SAVE", root, 1);
    return root;
}

int RunOperation(CMemoryCardAccess &mc, int operation) {
    mc.SetFuncNo(operation);
    for (int frame = 0; frame < 1000; ++frame) {
        int result = mc.Step();
        if (result != 0) {
            return result;
        }
    }
    ADD_FAILURE() << "operation never finished";
    return 0;
}

std::vector<char> ReadFile(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

s32 &MapNoOf(CSaveData &save) {
    return *reinterpret_cast<s32 *>(reinterpret_cast<char *>(&save) + kSaveMapNoOffset);
}

void PrepareSave() {
    g_save.Initialize();
    SaveData = &g_save;
    s16 *name = g_save.GetCharaName(0);
    for (int i = 0; i < 5; ++i) {
        name[i] = static_cast<s16>("Toan"[i]);
    }
    g_save.AddPlayTime(123456);
    g_save.QuestDungeon(0, 7);
    g_save.QuestDungeon(6, 3);
    MapNoOf(g_save) = 42;
}

void PrepareAccess(CMemoryCardAccess &mc) {
    ASSERT_TRUE(mc.InitForMC() == 0);
    MC_ICON_DATA icon = {
        {g_icon_names[0], g_icons[0], sizeof(g_icons[0])},
        {g_icon_names[1], g_icons[1], sizeof(g_icons[1])},
        {g_icon_names[2], g_icons[2], sizeof(g_icons[2])},
    };
    for (int i = 0; i < 3; ++i) {
        std::memset(g_icons[i], 'a' + i, sizeof(g_icons[i]));
    }
    mc.SetBuff(g_menu_buffer);
    mc.SetIconData(&icon);
    mc.MakeMcIconSysInfo();
}

} // namespace

TEST(PlatformMc, SaveDataIsPodSized) {
    ASSERT_TRUE(sizeof(CSaveData) == kSaveDataSize);
}

TEST(PlatformMc, SyncReportsCommandCodes) {
    UseTempSaveRoot();
    int cmd = -1;
    int result = -1;
    ASSERT_TRUE(sceMcInit() == sceMcIniSucceed);
    ASSERT_TRUE(sceMcSync(MC_NOWAIT, &cmd, &result) == -1);

    int type = 0, free_size = 0, formatted = 0;
    ASSERT_TRUE(sceMcGetInfo(0, 0, &type, &free_size, &formatted) == 0);
    ASSERT_TRUE(sceMcSync(MC_NOWAIT, &cmd, &result) == 1);
    ASSERT_TRUE(cmd == 1 && result == sceMcResSucceed);
    ASSERT_TRUE(type == sceMcTypePS2 && formatted == 1 && free_size >= 0x190);
    ASSERT_TRUE(sceMcSync(MC_NOWAIT, &cmd, &result) == -1);

    ASSERT_TRUE(sceMcGetInfo(1, 0, &type, &free_size, &formatted) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1);
    ASSERT_TRUE(cmd == 1 && result <= -10 && type == sceMcTypeNoCard);

    ASSERT_TRUE(sceMcChdir(0, 0, (char *) "/nothing", nullptr) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1);
    ASSERT_TRUE(cmd == 0xC && result == sceMcResNoEntry);

    ASSERT_TRUE(sceMcMkdir(0, 0, (char *) "dir") == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0xB && result == 0);
    ASSERT_TRUE(sceMcMkdir(0, 0, (char *) "dir") == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0xB && result < 0);

    char current[0x40] = "garbage";
    ASSERT_TRUE(sceMcChdir(0, 0, (char *) "dir/", current) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0xC && result == 0);
    ASSERT_TRUE(std::strcmp(current, "/") == 0);

    ASSERT_TRUE(sceMcOpen(0, 0, (char *) "missing", 1) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 2 && result == sceMcResNoEntry);
    ASSERT_TRUE(sceMcOpen(0, 0, (char *) "file", 0x203) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 2 && result >= 0);
    int  fd = result;
    char data[5] = "abcd";
    ASSERT_TRUE(sceMcWrite(fd, data, 4) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 6 && result == 4);
    ASSERT_TRUE(sceMcFlush(fd) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0xA && result == 0);
    ASSERT_TRUE(sceMcClose(fd) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 3 && result == 0);

    ASSERT_TRUE(sceMcOpen(0, 0, (char *) "/dir/file", 1) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && result >= 0);
    fd = result;
    char back[8] = {};
    ASSERT_TRUE(sceMcRead(fd, back, 8) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 5 && result == 4);
    ASSERT_TRUE(std::memcmp(back, "abcd", 4) == 0);
    ASSERT_TRUE(sceMcWrite(fd, data, 4) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 6 && result == sceMcResDeniedPermit);
    ASSERT_TRUE(sceMcClose(fd) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1);

    unsigned char table[4][0x40] = {};
    ASSERT_TRUE(sceMcGetDir(0, 0, (char *) "*", 0, 4, table) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0xD && result == 3);
    ASSERT_TRUE(std::strcmp(reinterpret_cast<char *>(table[0] + 0x20), ".") == 0);
    ASSERT_TRUE(std::strcmp(reinterpret_cast<char *>(table[1] + 0x20), "..") == 0);
    ASSERT_TRUE(std::strcmp(reinterpret_cast<char *>(table[2] + 0x20), "file") == 0);
    std::uint32_t size;
    std::memcpy(&size, table[2] + 0x10, 4);
    ASSERT_TRUE(size == 4);
    std::uint16_t attributes;
    std::memcpy(&attributes, table[2] + 0x14, 2);
    ASSERT_TRUE((attributes & 0x8010) == 0x8010);
    std::uint16_t year;
    std::memcpy(&year, table[2] + 0x0E, 2);
    ASSERT_TRUE(year >= 2000);

    ASSERT_TRUE(sceMcDelete(0, 0, (char *) "file") == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0xF && result == 0);
    ASSERT_TRUE(sceMcDelete(0, 0, (char *) "file") == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0xF && result == sceMcResNoEntry);

    ASSERT_TRUE(sceMcUnformat(0, 0) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0x11 && result == 0);
    ASSERT_TRUE(sceMcGetInfo(0, 0, &type, &free_size, &formatted) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && result == sceMcResNoFormat && formatted == 0);
    ASSERT_TRUE(sceMcFormat(0, 0) == 0);
    ASSERT_TRUE(sceMcSync(MC_WAIT, &cmd, &result) == 1 && cmd == 0x10 && result == 0);
}

TEST(PlatformMc, SaveLoadCycle) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    PrepareAccess(g_mc);

    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_SEARCH_TYPE) == 1);
    ASSERT_TRUE(g_mc.card[0].present == 1);
    ASSERT_TRUE(g_mc.card[0].type == sceMcTypePS2);
    ASSERT_TRUE(g_mc.card[0].formatted == 1);

    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_DIR) == 1);
    ASSERT_TRUE(g_mc.card[0].dir_exists == 0);

    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_MAKE_DIR) == 1);
    fs::path dir = root / "mc0" / kSaveDir;
    ASSERT_TRUE(fs::file_size(dir / kSaveDir) == 0x40);
    ASSERT_TRUE(ReadFile(dir / kSaveDir)[0x11] == 1);
    std::vector<char> icon_sys = ReadFile(dir / "icon.sys");
    ASSERT_TRUE(icon_sys.size() == sizeof(sceMcIconSys));
    ASSERT_TRUE(std::memcmp(icon_sys.data(), "PS2D", 4) == 0);
    ASSERT_TRUE(ReadFile(dir / "dkicon_c.ico") == std::vector<char>(100, 'b'));

    g_mc.file_no = 3;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_SAVE) == 1);
    std::vector<char> image = ReadFile(dir / "darkcloud3");
    ASSERT_TRUE(image.size() == static_cast<std::size_t>(kImageSize));
    ASSERT_TRUE(std::memcmp(image.data(), g_mc.save_buffer, kImageSize) == 0);
    ASSERT_TRUE(std::strcmp(image.data() + kSaveDataSize, "darkcloudVer1.9") == 0);
    char total = 0;
    for (int i = 0; i < kSaveDataSize; ++i) {
        total = static_cast<char>(total + image[i]);
        if (i % 64 == 63) {
            ASSERT_TRUE(image[kChecksumOffset + i / 64] == total);
            total = 0;
        }
    }
    ASSERT_TRUE(ReadFile(dir / kSaveDir)[17] == 3);

    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    for (int file = 0; file < MC_SAVE_FILE_MAX; ++file) {
        ASSERT_TRUE(g_mc.file_info[file].state == (file == 3));
    }
    const SAVEDATA_INFO &info = g_mc.file_info[3];
    ASSERT_TRUE(info.file_no == 4);
    ASSERT_TRUE(info.map_no == 42);
    ASSERT_TRUE(info.play_time == 123456.0f);
    ASSERT_TRUE(info.quest_total == 10);
    ASSERT_TRUE(std::memcmp(info.name, g_save.GetCharaName(0), 10) == 0);

    std::vector<char> expected(reinterpret_cast<char *>(g_mc.save_buffer), reinterpret_cast<char *>(g_mc.save_buffer) + kSaveDataSize);
    g_save.AddPlayTime(99);
    MapNoOf(g_save) = 7;
    g_mc.file_no = 3;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD) == 1);
    reinterpret_cast<s32 *>(expected.data() + (static_cast<char *>(g_save.GetConfigData()) - reinterpret_cast<char *>(&g_save)))[17] = 3;
    ASSERT_TRUE(std::memcmp(&g_save, expected.data(), kSaveDataSize) == 0);
    ASSERT_TRUE(MapNoOf(g_save) == 42);

    // A flipped byte fails the checksum and leaves the save in memory alone.
    image[100] ^= 0x40;
    std::ofstream(dir / "darkcloud3", std::ios::binary).write(image.data(), image.size());
    MapNoOf(g_save) = 9;
    g_mc.file_no = 3;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD) == -1);
    ASSERT_TRUE(MapNoOf(g_save) == 9);

    // What InitExistData does at boot, on a fresh library state.
    CMemoryCardAccess &boot = g_boot;
    PrepareAccess(boot);
    ASSERT_TRUE(RunOperation(boot, MC_OPERATION_SEARCH_TYPE) == 1);
    ASSERT_TRUE(RunOperation(boot, MC_OPERATION_GET_DIR) == 1);
    ASSERT_TRUE(boot.card[0].dir_exists == 1);
    ASSERT_TRUE(boot.card[0].dir_entries == 1);
    ASSERT_TRUE(std::strcmp(SaveFileInfo[0].name, "darkcloud3") == 0);
    ASSERT_TRUE(GetOpenAttribute((char *) "darkcloud3") == 1);
    ASSERT_TRUE(RunOperation(boot, MC_OPERATION_LOAD_CONFIG) == 1);

    boot.file_no = 3;
    ASSERT_TRUE(RunOperation(boot, MC_OPERATION_DELETE) == 1);
    ASSERT_TRUE(!fs::exists(dir / "darkcloud3"));

    boot.port = 1;
    ASSERT_TRUE(RunOperation(boot, MC_OPERATION_SEARCH_TYPE) == 1);
    ASSERT_TRUE(boot.card[1].type == sceMcTypeNoCard);

    fs::remove_all(root);
}

#ifdef _WIN32
TEST(PlatformMc, WindowsPathsCannotEscapeCardRoot) {
    fs::path root = UseTempSaveRoot();
    PathsSetSaveRoot(root / "save");
    ASSERT_EQ(sceMcInit(), sceMcIniSucceed);
    fs::create_directories(root / "outside");
    std::ofstream(root / "sentinel.bin", std::ios::binary) << "sentinel";
    const std::vector<char>  sentinel = ReadFile(root / "sentinel.bin");
    std::vector<std::string> prefixes = {
        "..\\..\\",
        root.string() + "\\",
        "\\" + root.relative_path().string() + "\\",
        root.root_name().string() + root.relative_path().string() + "\\",
        "\\\\?\\" + root.string() + "\\",
    };
    auto rejected = [](int issued) {
        ASSERT_EQ(issued, 0);
        int result = 0;
        ASSERT_EQ(sceMcSync(MC_WAIT, nullptr, &result), 1);
        ASSERT_EQ(result, sceMcResNoEntry);
    };
    for (const std::string &prefix : prefixes) {
        SCOPED_TRACE(prefix);
        std::string file = prefix + "sentinel.bin";
        for (int flags : {1, 2, 0x203}) {
            rejected(sceMcOpen(0, 0, file.data(), flags));
        }
        rejected(sceMcDelete(0, 0, file.data()));
        std::string fresh = prefix + "new.bin";
        rejected(sceMcOpen(0, 0, fresh.data(), 0x203));
        std::string dir = prefix + "outside";
        rejected(sceMcChdir(0, 0, dir.data(), nullptr));
        std::string   listing = dir + "/*";
        unsigned char table[4][0x40] = {};
        rejected(sceMcGetDir(0, 0, listing.data(), 0, 4, table));
        dir = prefix + "new-directory";
        rejected(sceMcMkdir(0, 0, dir.data()));
        ASSERT_EQ(ReadFile(root / "sentinel.bin"), sentinel);
        ASSERT_FALSE(fs::exists(root / "new.bin"));
        ASSERT_FALSE(fs::exists(root / "new-directory"));
    }
    for (std::string name : {"sentinel.bin.", "sentinel.bin "}) {
        rejected(sceMcOpen(0, 0, name.data(), 0x203));
        rejected(sceMcDelete(0, 0, name.data()));
        rejected(sceMcMkdir(0, 0, name.data()));
    }
    for (std::string name : {"NUL", "nul", "Con", "AUX.txt", "prn.tar.gz", "COM1", "lpt9.bin", "CONIN$", "conout$"}) {
        SCOPED_TRACE(name);
        for (int flags : {1, 2, 0x203}) {
            rejected(sceMcOpen(0, 0, name.data(), flags));
        }
        rejected(sceMcDelete(0, 0, name.data()));
        rejected(sceMcMkdir(0, 0, name.data()));
    }
    ASSERT_TRUE(fs::is_directory(root / "save/mc0"));
    ASSERT_TRUE(fs::is_empty(root / "save/mc0"));
    fs::remove_all(root);
}
#endif
