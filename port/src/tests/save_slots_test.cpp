// nlohmann/json (through the fishing framework) before the game headers, whose one-letter macros it
// cannot stand.
#include "fishing/framework.hpp"

#include <gtest/gtest.h>
#include <libpad.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/gameloop.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "fishing/host.hpp"
#include "gamepad.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "itembombeffect.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_save.hpp"
#include "platform/input.hpp"
#include "platform/paths.hpp"
#include "platform_fixture.hpp"
#include "save/extension.hpp"
#include "save/save_slots.hpp"
#include "save/save_state.hpp"
#include "savedata.hpp"
#include "stockitem.hpp"
#include "text/gametext.hpp"

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace dc::test {

using namespace dc::game;
using namespace dc::app;
using namespace dc::save;

// The save screens' list and CMemoryCardAccess on the save folders, driven as the save menu does:
// SetFuncNo, then Step until the operation finishes, on a save root in a temporary directory.

namespace fs = std::filesystem;

namespace {

constexpr int kImageSize = 0x136A7;
constexpr int kSaveDataSize = 0x131C0;
constexpr int kChecksumOffset = 0x131E0;
constexpr int kSaveMapNoOffset = 0x1C8;

struct alignas(64) SaveMenuBuffer {
    std::array<char, 0x30000> bytes;
};

std::unique_ptr<SaveMenuBuffer> g_menu_buffer = std::make_unique<SaveMenuBuffer>();
alignas(64) CSaveData g_save;
CMemoryCardAccess g_mc;

fs::path UseTempSaveRoot() {
    static unsigned sequence = 0;
    fs::path        root;
    do {
        root = fs::temp_directory_path() / ("dc_save_test_" + std::to_string(dc::test::ProcessId()) + "_" + std::to_string(sequence++));
    } while (!fs::create_directory(root));
    PathsSetSaveRoot(root);
    return root;
}

int RunOperation(CMemoryCardAccess &mc, int operation) {
    mc.SetFuncNo(operation);
    for (int step = 0; step < 100; ++step) {
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

void WriteFile(const fs::path &path, const std::vector<char> &data) {
    std::ofstream(path, std::ios::binary).write(data.data(), static_cast<std::streamsize>(data.size()));
}

s32 &MapNoOf(CSaveData &save) {
    return *reinterpret_cast<s32 *>(reinterpret_cast<char *>(&save) + kSaveMapNoOffset);
}

constexpr int kGameClear = 14;
constexpr int kLastFile = 17;

s32 &ConfigWord(int word) {
    return static_cast<s32 *>(g_save.GetConfigData())[word];
}

fs::path SlotOf(const fs::path &root, int file_no) {
    return root / "saves" / std::to_string(file_no + 1);
}

void WriteSave(const fs::path &root, int file_no, const std::vector<char> &data) {
    fs::create_directories(SlotOf(root, file_no));
    WriteFile(SlotOf(root, file_no) / "save.dat", data);
}

std::vector<char> ReadSave(const fs::path &root, int file_no) {
    return ReadFile(SlotOf(root, file_no) / "save.dat");
}

// A save from the New file board.
int SaveNew(int file_no) {
    SaveSlotNew = true;
    g_mc.file_no = file_no;
    int result = RunOperation(g_mc, MC_OPERATION_SAVE);
    SaveSlotNew = false;
    return result;
}

void PrepareSave() {
    g_save.Initialize();
    SaveData = &g_save;
    s16 *name = g_save.GetCharaName(0);
    for (int i = 0; i < 5; ++i) {
        name[i] = static_cast<s16>(static_cast<unsigned char>("Toan"[i]));
    }
    g_save.AddPlayTime(123456);
    g_save.QuestDungeon(0, 7);
    g_save.QuestDungeon(6, 3);
    MapNoOf(g_save) = 42;
    ASSERT_TRUE(g_mc.InitForMC() == 0);
    g_mc.SetBuff(g_menu_buffer->bytes.data());
}

struct alignas(64) PadDmaBuffer {
    std::array<unsigned char, 1024> bytes;
};

std::array<std::unique_ptr<PadDmaBuffer>, 2> g_pad_buffer = {
    std::make_unique<PadDmaBuffer>(), std::make_unique<PadDmaBuffer>()};

// As CGamePad::Init leaves the pads, past its wait on sceGsSyncV (platform_pad_test.cpp).
void OpenPads() {
    ASSERT_TRUE(scePadInit(0) == 1);
    ASSERT_TRUE(scePadPortOpen(0, 0, g_pad_buffer[0]->bytes.data()) == 1);
    ASSERT_TRUE(scePadPortOpen(1, 0, g_pad_buffer[1]->bytes.data()) == 1);
    InputPadState released;
    released.connected = true;
    InputSetOverride(0, &released);
    InputSetOverride(1, &released);
    for (int i = 0; i < 4; ++i) {
        GamePad.UpDate();
    }
}

// One frame with the button newly down.
void Press(std::uint16_t button) {
    InputPadState state;
    state.connected = true;
    InputSetOverride(0, &state);
    GamePad.UpDate();
    state.buttons = button;
    InputSetOverride(0, &state);
    GamePad.UpDate();
}

SAVEDATA_INFO Save(int file_no) {
    SAVEDATA_INFO info = {};
    info.state = 1;
    info.file_no = file_no + 1;
    return info;
}

} // namespace

TEST(SaveSlots, SaveDataIsPodSized) {
    ASSERT_TRUE(sizeof(CSaveData) == kSaveDataSize);
}

TEST(SaveSlots, FolderNamesAreBoardNumbers) {
    ASSERT_EQ(SaveSlotFileNo("1"), 0);
    ASSERT_EQ(SaveSlotFileNo("12"), 11);
    ASSERT_EQ(SaveSlotFileNo("123456789"), 123456788);
    for (std::string_view name : {"", "0", "01", "1.tmp", "-1", "1234567890", "darkcloud1", "state.json", "1a"}) {
        ASSERT_EQ(SaveSlotFileNo(name), -1) << name;
    }
}

TEST(SaveSlots, BoardsListSavesThenTheNewOne) {
    // Files 0, 1, 3 and 5 hold saves; 2 is unreadable; 4 is free.
    SaveSlotList list;
    list.files = {0, 1, 2, 3, 5};
    list.saves = {Save(0), Save(1), Save(3), Save(5)};

    ASSERT_EQ(SaveSlotFirstFree({}), 0);
    ASSERT_EQ(SaveSlotFirstFree(list.files), 4);
    ASSERT_EQ(SaveSlotRows(list, true), 5);
    ASSERT_EQ(SaveSlotRows(list, false), 4);

    ASSERT_EQ(SaveSlotRow(list, 3, true), 2);
    ASSERT_EQ(SaveSlotFileAt(list, 2), 3);
    ASSERT_EQ(SaveSlotFileAt(list, 4), 4);

    // A file without a board of its own, unreadable or gone, is the new save on the save screen
    // and the first save on the load screen.
    for (int file_no : {2, 4, 200}) {
        ASSERT_EQ(SaveSlotRow(list, file_no, true), 4);
        ASSERT_EQ(SaveSlotRow(list, file_no, false), 0);
    }

    SaveSlotList empty;
    ASSERT_EQ(SaveSlotRows(empty, false), 0);
    ASSERT_EQ(SaveSlotRow(empty, 3, true), 0);
    ASSERT_EQ(SaveSlotFileAt(empty, 0), 0);
}

TEST(SaveSlots, SaveListAndLoad) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();

    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_SEARCH_TYPE) == 1);
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_DIR) == 1);
    ASSERT_TRUE(g_mc.card[0].present == 1 && g_mc.card[0].formatted == 1 && g_mc.card[0].dir_exists == 1);
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    ASSERT_TRUE(SaveSlots.files.empty() && SaveSlots.saves.empty());
    ASSERT_FALSE(g_mc.CheckFileNo(13));

    ASSERT_TRUE(SaveNew(13) == 1);
    std::vector<char> image = ReadFile(root / "saves" / "14" / "save.dat");
    ASSERT_TRUE(image.size() == static_cast<std::size_t>(kImageSize));
    ASSERT_TRUE(std::memcmp(image.data(), g_mc.save_buffer, kImageSize) == 0);
    ASSERT_TRUE(std::string_view(image.data() + kSaveDataSize) == "darkcloudVer1.9");
    char total = 0;
    for (int i = 0; i < kSaveDataSize; ++i) {
        total = static_cast<char>(total + image[i]);
        if (i % 64 == 63) {
            ASSERT_TRUE(image[kChecksumOffset + i / 64] == total);
            total = 0;
        }
    }
    SaveState state;
    ASSERT_TRUE(SaveStateRead(root / "saves" / "state.json", state) && state.last_save == 14);
    ASSERT_TRUE(ConfigWord(kLastFile) == 13 && g_mc.CheckFileNo(13));
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_SAVE) == 1);

    // A save copied out of a card holds the same bytes.
    WriteSave(root, 2, image);
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    ASSERT_TRUE((SaveSlots.files == std::vector<int>{2, 13}));
    ASSERT_TRUE(SaveSlots.saves.size() == 2);
    const SAVEDATA_INFO &info = SaveSlots.saves[1];
    ASSERT_TRUE(info.state == 1 && info.file_no == 14);
    ASSERT_TRUE(info.map_no == 42);
    ASSERT_TRUE(info.play_time == 123456.0f);
    ASSERT_TRUE(info.quest_total == 10);
    ASSERT_TRUE(std::memcmp(info.name, g_save.GetCharaName(0), 10) == 0);

    std::vector<char> expected(image.begin(), image.begin() + kSaveDataSize);
    g_save.AddPlayTime(99);
    MapNoOf(g_save) = 7;
    g_mc.file_no = 2;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD) == 1);
    const std::ptrdiff_t config_offset = static_cast<char *>(g_save.GetConfigData()) - reinterpret_cast<char *>(&g_save);
    const s32            loaded_file = 2;
    std::memcpy(expected.data() + config_offset + kLastFile * sizeof(s32), &loaded_file, sizeof(loaded_file));
    ASSERT_TRUE(std::memcmp(&g_save, expected.data(), kSaveDataSize) == 0);
    ASSERT_TRUE(SaveStateRead(root / "saves" / "state.json", state) && state.last_save == 3);

    // A flipped byte fails the checksum and leaves the save in memory alone.
    image[100] ^= 0x40;
    WriteSave(root, 2, image);
    MapNoOf(g_save) = 9;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD) == -1);
    ASSERT_TRUE(MapNoOf(g_save) == 9);

    // Neither the broken save nor a short one is listed, but both keep their numbers.
    WriteSave(root, 0, std::vector<char>(100, 'x'));
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    ASSERT_TRUE(SaveSlots.saves.size() == 1 && SaveSlots.saves[0].file_no == 14);
    ASSERT_TRUE(SaveSlotFirstFree(SaveSlots.files) == 1);

    g_mc.file_no = 2;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_DELETE) == 1);
    ASSERT_FALSE(fs::exists(root / "saves" / "3"));

    fs::remove_all(root);
}

TEST(SaveSlots, AutosaveIsOneFileTheLoadScreenListsFirst) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    fs::path autosave = root / "saves" / "autosave" / "save.dat";

    ASSERT_TRUE(SaveSlotWriteAutosave());
    ASSERT_TRUE(ReadFile(autosave).size() == static_cast<std::size_t>(kImageSize));
    ASSERT_TRUE(std::memcmp(ReadFile(autosave).data(), g_mc.save_buffer, kImageSize) == 0);
    g_save.AddPlayTime(60);
    ASSERT_TRUE(SaveSlotWriteAutosave());
    ASSERT_TRUE(std::memcmp(ReadFile(autosave).data(), g_mc.save_buffer, kImageSize) != 0);
    ASSERT_TRUE(SaveSlotFiles().empty());

    ASSERT_TRUE(SaveNew(0) == 1);
    SaveMenu.mode = SAVE_MENU_MODE_LOAD;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    ASSERT_TRUE(SaveSlots.saves.size() == 2 && SaveSlots.saves[0].file_no == kSaveSlotAutosave + 1);
    ASSERT_TRUE(SaveSlotFileAt(SaveSlots, 0) == kSaveSlotAutosave && SaveSlotFileAt(SaveSlots, 1) == 0);
    SaveMenu.mode = SAVE_MENU_MODE_SAVE;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    ASSERT_TRUE(SaveSlots.saves.size() == 1 && SaveSlots.saves[0].file_no == 1);
    SaveMenu.mode = SAVE_MENU_MODE_LOAD;

    g_mc.file_no = kSaveSlotAutosave;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD) == 1);
    ASSERT_TRUE(ConfigWord(kLastFile) == 0);
    SaveState state;
    ASSERT_TRUE(SaveStateRead(root / "saves" / "state.json", state) && state.last_save == 1);

    fs::remove_all(root);
}

TEST(SaveSlots, StateKeepsTheLastSaveAndTheClearFlag) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    fs::path path = root / "saves" / "state.json";

    ASSERT_TRUE(SaveNew(200) == 1);
    ConfigWord(kGameClear) = 1;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_SAVE_CONFIG) == 1);
    SaveState state;
    ASSERT_TRUE(SaveStateRead(path, state) && state.last_save == 201 && state.game_clear);
    ConfigWord(kLastFile) = 0;
    ConfigWord(kGameClear) = 0;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1);
    ASSERT_TRUE(ConfigWord(kLastFile) == 200 && ConfigWord(kGameClear) == 1);
    ASSERT_FALSE(fs::exists(root / "sysconfig.bin") || fs::exists(root / "mc0"));

    // Unknown keys are ignored; without the file, or with one that is not JSON, the game keeps
    // its own.
    std::string text = R"({"last_save": 6, "x": 1})";
    WriteFile(path, {text.begin(), text.end()});
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1);
    ASSERT_TRUE(ConfigWord(kLastFile) == 5 && ConfigWord(kGameClear) == 1);
    // State cannot import options, and invalid numbers cannot wrap into a valid last slot.
    ConfigWord(0) = 19;
    for (std::string_view value : {"-1", "1000000000", "18446744073709551615", "1.5", "\"2\""}) {
        text = std::string(R"({"game_clear":true,"options":[1,2],"last_save":)").append(value).append("}");
        WriteFile(path, {text.begin(), text.end()});
        ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1);
        ASSERT_TRUE(ConfigWord(kLastFile) == 5 && ConfigWord(0) == 19);
    }
    text = R"({"last_save":0,"game_clear":false})";
    WriteFile(path, {text.begin(), text.end()});
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1);
    ASSERT_TRUE(ConfigWord(kLastFile) == -1 && ConfigWord(kGameClear) == 0);
    text = R"({"last_save":999999999,"game_clear":true})";
    WriteFile(path, {text.begin(), text.end()});
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1);
    ASSERT_TRUE(ConfigWord(kLastFile) == 999999998 && ConfigWord(kGameClear) == 1);
    for (bool present : {true, false}) {
        WriteFile(path, {'x'});
        if (!present) {
            fs::remove(path);
        }
        ConfigWord(kLastFile) = 8;
        ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1);
        ASSERT_TRUE(ConfigWord(kLastFile) == 8);
    }
    // Old card/flat state and saves are not migrated or consulted.
    WriteFile(root / "sysconfig.bin", std::vector<char>(68, 1));
    WriteFile(root / "darkcloud0", ReadSave(root, 200));
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1 && ConfigWord(kLastFile) == 8);
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    ASSERT_TRUE((SaveSlots.files == std::vector<int>{200}));
    // A valid JSON prefix in an oversized state file must not be accepted as the whole file.
    text = R"({"last_save":1})";
    text.resize(0x10001, ' ');
    WriteFile(path, {text.begin(), text.end()});
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG) == 1 && ConfigWord(kLastFile) == 8);

    fs::remove_all(root);
}

TEST(SaveSlots, NewGameKeepsTheWholeLastFile) {
    PrepareSave();
    for (s32 file_no : {127, 128, 200, 256, 999999998}) {
        ConfigWord(kLastFile) = file_no;
        ConfigWord(kGameClear) = 1;
        MapNoOf(g_save) = 42;
        ResetSaveKeepingConfig(g_save);
        ASSERT_NE(MapNoOf(g_save), 42);
        ASSERT_EQ(ConfigWord(kLastFile), file_no);
        ASSERT_EQ(ConfigWord(kGameClear), 1);
    }
}

TEST(SaveSlots, LoadingAnOlderSaveKeepsEndingState) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    ConfigWord(kGameClear) = 0;
    g_mc.SetBuff(g_menu_buffer->bytes.data());
    ASSERT_EQ(SaveNew(2), 1);
    std::vector<char> image = ReadSave(root, 2);

    // The real ending operation writes only state, leaving the pre-ending image unchanged.
    ConfigWord(kGameClear) = 1;
    ConfigWord(kLastFile) = 8;
    SaveMenu.mode = SAVE_MENU_MODE_ENDING;
    SaveMenu.access_kind = SAVE_ACCESS_SAVE;
    SaveMenu.key_no = SAVE_KEY_SAVE_ENDING;
    SaveMenuFunc[SaveMenu.key_no]();
    ASSERT_EQ(SaveMenu.key_no, SAVE_KEY_END_SAVE_ENDING);
    SaveState state;
    ASSERT_TRUE(SaveStateRead(SaveStatePath(), state));
    ASSERT_TRUE(state.game_clear);
    ASSERT_EQ(state.last_save, 9);

    g_mc.file_no = 2;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    ASSERT_EQ(ConfigWord(kGameClear), 0);
    ASSERT_EQ(ReadSave(root, 2), image);
    ASSERT_TRUE(SaveStateRead(SaveStatePath(), state));
    ASSERT_TRUE(state.game_clear);
    ASSERT_EQ(state.last_save, 3);

    // Fresh title-equivalent state must still unlock the cleared game after this load.
    ConfigWord(kGameClear) = 0;
    ConfigWord(kLastFile) = -1;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD_CONFIG), 1);
    ASSERT_EQ(ConfigWord(kGameClear), 1);
    ASSERT_EQ(ConfigWord(kLastFile), 2);

    // Explicit no-clear state remains authoritative too, even in a cleared session.
    ConfigWord(kGameClear) = 0;
    SaveMenu.key_no = SAVE_KEY_SAVE_ENDING;
    SaveMenuFunc[SaveMenu.key_no]();
    ASSERT_EQ(SaveMenu.key_no, SAVE_KEY_END_SAVE_ENDING);
    ConfigWord(kGameClear) = 1;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    ASSERT_TRUE(SaveStateRead(SaveStatePath(), state));
    ASSERT_FALSE(state.game_clear);

    fs::remove_all(root);
}

TEST(SaveSlots, LoadCursorFallsBackToPreLoadSessionState) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    ConfigWord(kGameClear) = 0;
    g_mc.SetBuff(g_menu_buffer->bytes.data());
    ASSERT_EQ(SaveNew(0), 1);
    g_mc.file_no = 0;

    for (std::string_view text : {"", "malformed", "[]", "{}", R"({"game_clear":1})"}) {
        for (int clear : {0, 1}) {
            fs::remove(SaveStatePath());
            if (!text.empty()) {
                WriteFile(SaveStatePath(), {text.begin(), text.end()});
            }
            ConfigWord(kGameClear) = clear;
            ConfigWord(kLastFile) = 8;
            ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1) << text;
            ASSERT_EQ(ConfigWord(kGameClear), 0);
            SaveState state;
            ASSERT_TRUE(SaveStateRead(SaveStatePath(), state));
            ASSERT_EQ(state.game_clear, clear != 0) << text;
            ASSERT_EQ(state.last_save, 1);
        }
    }

    fs::remove_all(root);
}

TEST(SaveSlots, UnusableFilesStayAndKeepTheirNumbers) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();

    ASSERT_TRUE(SaveNew(1) == 1);
    std::vector<char> image = ReadSave(root, 1);

    // Another version's save, one whose map word is damaged, a folder with no save, a file where a
    // folder belongs, and a whole save whose map lies past the boards' table of names and whose
    // quest counts are negative.
    std::vector<char> other = image;
    std::ranges::copy(std::to_array("darkcloudVer1.0"), other.begin() + kSaveDataSize);
    WriteSave(root, 4, other);
    std::vector<char> damaged = image;
    const s32         damaged_map_no = 0x7fffffff;
    std::memcpy(damaged.data() + kSaveMapNoOffset, &damaged_map_no, sizeof(damaged_map_no));
    WriteSave(root, 0, damaged);
    fs::create_directories(SlotOf(root, 2));
    WriteFile(SlotOf(root, 9), {'x'});
    MapNoOf(g_save) = 0x7fffffff;
    g_save.QuestDungeon(0, -8);
    g_save.QuestDungeon(6, INT_MIN);
    g_mc.SetBuff(g_menu_buffer->bytes.data());
    ASSERT_TRUE(SaveNew(5) == 1);

    fs::create_directories(SlotOf(root, 7));
    std::vector<int> files = {0, 1, 2, 4, 5, 7, 9};
#ifndef _WIN32
    // A FIFO is not read, which would wait for a writer.
    ASSERT_TRUE(mkfifo((SlotOf(root, 7) / "save.dat").c_str(), 0600) == 0);
#endif

    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO) == 1);
    ASSERT_TRUE(SaveSlots.files == files);
    ASSERT_TRUE(SaveSlots.saves.size() == 2);
    ASSERT_TRUE(SaveSlots.saves[0].file_no == 2 && SaveSlots.saves[1].file_no == 6);
    ASSERT_TRUE(SaveSlots.saves[1].map_no == 0 && SaveSlots.saves[1].quest_total == 0);
    ASSERT_TRUE(SaveSlotFirstFree(SaveSlots.files) == 3);
    ASSERT_TRUE(ReadSave(root, 4) == other && ReadSave(root, 0) == damaged);
    g_mc.file_no = 0;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD) == -1);

    // A new save never replaces what took its number after the list was read.
    SaveSlotNew = true;
    ASSERT_FALSE(g_mc.CheckFileNo(1));
    SaveSlotNew = false;
    ASSERT_TRUE(SaveNew(1) == 1);
    ASSERT_TRUE(g_mc.file_no == 3 && ConfigWord(kLastFile) == 3);
    ASSERT_TRUE(ReadSave(root, 1) == image);
    ASSERT_TRUE(SaveNew(2) == 1);
    ASSERT_TRUE(g_mc.file_no == 6 && fs::is_empty(SlotOf(root, 2)));
    ASSERT_TRUE(SaveNew(9) == 1);
    ASSERT_TRUE(g_mc.file_no == 8 && ReadFile(SlotOf(root, 9)) == std::vector<char>{'x'});
    for (const fs::directory_entry &entry : fs::recursive_directory_iterator(root)) {
        ASSERT_TRUE(entry.path().extension() != ".tmp") << entry.path();
    }

    g_mc.file_no = 10;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_DELETE) == 1);

    fs::remove_all(root);
}

TEST(SaveSlots, FailedPublicationKeepsOccupiedFolders) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    ASSERT_TRUE(SaveNew(0) == 1);
    std::vector<char> image = ReadSave(root, 0);
    fs::create_directory(SlotOf(root, 0) / "extra");
    fs::create_directories(root / "saves" / "state.json.blocked");
    // Move only this test's state out of the way and replace it with a directory.
    fs::rename(root / "saves" / "state.json", root / "saves" / "state.json.blocked" / "original");
    fs::create_directory(root / "saves" / "state.json");
    ASSERT_TRUE(SaveNew(0) == -1);
    ASSERT_TRUE(ReadSave(root, 0) == image && fs::is_directory(SlotOf(root, 0) / "extra"));
    ASSERT_TRUE(fs::is_regular_file(SlotOf(root, 1) / "save.dat"));
    g_mc.file_no = 0;
    MapNoOf(g_save) = 7;
    ASSERT_TRUE(RunOperation(g_mc, MC_OPERATION_LOAD) == 1);
    ASSERT_EQ(MapNoOf(g_save), 42);
    ASSERT_EQ(ConfigWord(kLastFile), 0);
    ASSERT_TRUE(fs::is_directory(root / "saves" / "state.json"));
    fs::remove_all(root);
}

TEST(SaveSlots, EndingSaveFailureCanBeLeft) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    fs::create_directories(root / "saves" / "state.json" / "taken");
    OpenPads();

    McAccess.SetFuncNo(MC_OPERATION_IDLE);
    SaveMenu.mode = SAVE_MENU_MODE_ENDING;
    SaveMenu.access_kind = SAVE_ACCESS_SAVE;
    for (int pass = 0; pass < 2; ++pass) {
        SaveMenu.key_no = SAVE_KEY_SAVE_ENDING;
        SaveMenuFunc[SaveMenu.key_no]();
        ASSERT_TRUE(SaveMenu.key_no == SAVE_KEY_ALERT && SaveMenu.alert_no == SAVE_ALERT_SAVE_FAILED);
        ASSERT_TRUE(GetSaveMenuMsgNo() == 266);
        Press(pass == 0 ? PAD_CROSS : PAD_CIRCLE);
        SaveMenuFunc[SaveMenu.key_no]();
        ASSERT_TRUE(SaveMenu.key_no == (pass == 0 ? SAVE_KEY_AFTER_ENDING : SAVE_KEY_FADE_OUT));
    }

    fs::remove_all(root);
}

// Custom items without the document layer: registered by hand, so a missing definition is one
// unregistered here. Orphans take numbers from kCustomFirst, as no custom document is loaded.
constexpr int kRod = item::kCustomFirst + 4;
constexpr int kBass = item::kCustomFirst + 5;

void RegisterCustomItems() {
    item::Clear();
    item::Register(item::Def{.id = "chronicle:rod", .number = kRod});
    item::Register(item::Def{.id = "chronicle:bass", .number = kBass, .state = {{"size", 31.5}}, .state_version = 2});
    item::RegisterSaveChunk();
    item::ClearCopies();
}

std::vector<char> BuildImage() {
    g_mc.SetBuff(g_menu_buffer->bytes.data());
    const char *image = reinterpret_cast<const char *>(g_mc.save_buffer);
    return {image, image + kImageSize};
}

ITEM_PACK &PackOf(CSaveData &save) {
    return save.GetDngStatus()->item_pack;
}

TEST(SaveSlots, ACustomFishsRecordGoesToChronicleDatAndOutlivesItsDefinition) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    item::Load();
    fishing::Load();
    fishing::RegisterSpecies(fishing::Species{.id = "chronicle:moon_trout", .kind = fishing::kCustomKindFirst});
    const fishing::Species *trout = fishing::FindSpecies("chronicle:moon_trout");
    const std::vector<char> retail = BuildImage();
    fishing::RecordCatch({.species = trout, .size = 150, .bait = "vanilla:carrot"});
    fishing::RecordCatch({.species = trout, .size = 182, .bait = "vanilla:minon"});
    fishing::RecordCatch({.species = trout, .size = 120, .bait = "vanilla:carrot"});

    ASSERT_EQ(BuildImage(), retail); // fish_data never sees a custom kind
    ASSERT_EQ(SaveNew(0), 1);
    ASSERT_TRUE(fs::exists(SlotOf(root, 0) / "chronicle.dat"));

    // Loaded with the species' pack gone, the record waits for it and is saved again as it was.
    save::ExtensionNewGame();
    EXPECT_EQ(fishing::RecordOf("chronicle:moon_trout"), nullptr);
    fishing::Load();
    EXPECT_EQ(fishing::FindSpecies("chronicle:moon_trout"), nullptr);
    g_mc.file_no = 0;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_SAVE), 1);
    save::ExtensionNewGame();
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    const fishing::SpeciesRecord *record = fishing::RecordOf("chronicle:moon_trout");
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->caught, 3);
    EXPECT_EQ(record->largest, 182);
    EXPECT_EQ(record->baits, (std::vector<std::string>{"vanilla:carrot", "vanilla:minon"}));
    save::ExtensionNewGame();
    fs::remove_all(root);
}

TEST(SaveSlots, TheChosenRodIsUsedWhileCarriedAndKeptWithTheSave) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    item::Load();
    item::Register(item::Def{.id = "chronicle:sea_rod", .number = kRod}.With(item::Rod{.spring = 19.0f}));
    fishing::Load();
    PackOf(g_save).item[0] = ITEM_FISHING_ROD;
    fishing::ChooseRod("chronicle:sea_rod");
    EXPECT_EQ(fishing::RodInUse().spring, 10.0f); // not carried: the Fishing Rod
    PackOf(g_save).item[1] = kRod;
    EXPECT_EQ(fishing::RodsCarried(), (std::vector<int>{ITEM_FISHING_ROD, kRod}));
    EXPECT_EQ(fishing::RodInUse().spring, 19.0f);

    BuildImage();
    ASSERT_EQ(SaveNew(0), 1);
    save::ExtensionNewGame();
    EXPECT_TRUE(fishing::ChosenRod().empty());
    g_mc.file_no = 0;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    EXPECT_EQ(fishing::ChosenRod(), "chronicle:sea_rod");
    save::ExtensionNewGame();
    item::Load();
    fs::remove_all(root);
}

TEST(SaveSlots, CustomItemsGoToChronicleDatAndSaveDatStaysRetails) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    RegisterCustomItems();
    ITEM_PACK &pack = PackOf(g_save);
    pack.item[0] = 150;
    pack.item_vol[0] = 4;

    // What retail writes for this save without the custom items.
    alignas(64) static CSaveData vanilla;
    vanilla = g_save;
    SaveData = &vanilla;
    const std::vector<char> retail = BuildImage();
    SaveData = &g_save;

    pack.item[1] = kRod;
    pack.item_vol[1] = 3;
    const int handle = item::NewCopy(*item::Find("chronicle:bass"));
    item::FindCopy(handle)->state["size"] = 44.0;
    pack.item[2] = static_cast<s16>(handle);
    pack.quick_item_slot[0] = kRod;
    pack.quick_item_qty[0] = 2;
    g_save.GetDngStatus()->active_item_vol[0] = 5;
    g_save.GetStockItem()->dungeon_items[4] = kRod;
    g_save.GetStockItem()->dungeon_item_vols[4] = 1;
    const std::uint64_t instance = item::FindCopy(handle)->instance;

    ASSERT_EQ(BuildImage(), retail);
    ASSERT_EQ(SaveNew(0), 1);
    ASSERT_EQ(ReadSave(root, 0), retail);
    ASSERT_TRUE(fs::exists(SlotOf(root, 0) / "chronicle.dat"));
    // The live game keeps its items.
    ASSERT_EQ(pack.item[1], kRod);

    std::ranges::fill(pack.item, -1);
    pack.quick_item_slot[0] = -1;
    item::ClearCopies();
    g_mc.file_no = 0;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    EXPECT_EQ(pack.item[0], 150);
    EXPECT_EQ(pack.item[1], kRod);
    EXPECT_EQ(pack.item_vol[1], 3);
    EXPECT_EQ(pack.quick_item_slot[0], kRod);
    EXPECT_EQ(pack.quick_item_qty[0], 2);
    EXPECT_EQ(g_save.GetDngStatus()->active_item_vol[0], 5);
    EXPECT_EQ(g_save.GetStockItem()->dungeon_items[4], kRod);
    EXPECT_EQ(g_save.GetStockItem()->dungeon_item_vols[4], 1);
    const item::Copy *copy = item::FindCopy(pack.item[2]);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->id, "chronicle:bass");
    EXPECT_EQ(copy->instance, instance);
    EXPECT_EQ(copy->state.at("size"), 44.0);
    EXPECT_EQ(copy->state_version, 2);
    EXPECT_EQ(item::Definition(pack.item[2]), kBass);
    EXPECT_EQ(item::NextInstance(), instance + 1);

    // Deleting the save takes its chronicle.dat with it.
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_DELETE), 1);
    ASSERT_FALSE(fs::exists(SlotOf(root, 0)));
    fs::remove_all(root);
}

TEST(SaveSlots, ASaveWithoutCustomItemsWritesNoChronicleDat) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    RegisterCustomItems();
    PackOf(g_save).item[1] = kRod;
    BuildImage();
    ASSERT_EQ(SaveNew(0), 1);
    ASSERT_TRUE(fs::exists(SlotOf(root, 0) / "chronicle.dat"));

    // Saving over it once the item is gone removes the file.
    PackOf(g_save).item[1] = -1;
    BuildImage();
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_SAVE), 1);
    ASSERT_FALSE(fs::exists(SlotOf(root, 0) / "chronicle.dat"));
    ASSERT_TRUE(SaveSlotWriteAutosave());
    ASSERT_FALSE(fs::exists(root / "saves" / "autosave" / "chronicle.dat"));
    fs::remove_all(root);
}

TEST(SaveSlots, ChronicleDatOnlyGoesWithItsOwnSaveDat) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    RegisterCustomItems();
    PackOf(g_save).item[1] = kRod;
    BuildImage();
    ASSERT_EQ(SaveNew(0), 1);

    // save.dat alone, as copied out of a card or from an older backup, loads without the items...
    WriteSave(root, 1, ReadSave(root, 0));
    g_mc.file_no = 1;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    EXPECT_EQ(PackOf(g_save).item[1], -1);

    // ...and so does one beside another save's chronicle.dat.
    g_save.AddPlayTime(1);
    BuildImage();
    ASSERT_EQ(SaveNew(2), 1);
    fs::copy_file(SlotOf(root, 0) / "chronicle.dat", SlotOf(root, 2) / "chronicle.dat", fs::copy_options::overwrite_existing);
    g_mc.file_no = 2;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    EXPECT_EQ(PackOf(g_save).item[1], -1);

    g_mc.file_no = 0;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    EXPECT_EQ(PackOf(g_save).item[1], kRod);
    fs::remove_all(root);
}

TEST(SaveSlots, AnItemWhoseDefinitionIsGoneWaitsAsAnOrphan) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    RegisterCustomItems();
    ITEM_PACK &pack = PackOf(g_save);
    pack.item[1] = kRod;
    pack.item[2] = static_cast<s16>(item::NewCopy(*item::Find("chronicle:bass")));
    BuildImage();
    ASSERT_EQ(SaveNew(0), 1);
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO), 1);
    EXPECT_TRUE(SaveSlots.missing.empty());

    item::Unregister("chronicle:rod");
    item::Unregister("chronicle:bass");
    item::RebindCopies();

    // The board marks the save before it loads.
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO), 1);
    EXPECT_EQ(SaveSlots.missing, std::vector<int>{1});
    g_mc.file_no = 0;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    const int rod = pack.item[1];
    EXPECT_TRUE(item::IsOrphan(rod));
    EXPECT_EQ(item::KindOf(rod), item::Kind::Item);
    EXPECT_TRUE(item::IsOrphan(pack.item[2]));
    EXPECT_EQ(item::FindCopy(pack.item[2])->state.at("size"), 31.5);
    EXPECT_EQ(text::GameTextDecode(item::CustomMessage(rod + 100, true)), "Unknown item");

    // It does nothing from a quick slot, where a custom item would be used.
    UserStatus = g_save.GetDngStatus();
    pack.quick_item_slot[0] = static_cast<s16>(rod);
    EXPECT_EQ(checkItemUsed(0), 0);
    pack.quick_item_slot[0] = kRod;
    EXPECT_EQ(checkItemUsed(0), 1);
    pack.quick_item_slot[0] = -1;

    // Saved again without their definitions, they keep their IDs and state...
    BuildImage();
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_SAVE), 1);
    // ...still marked while they are orphans...
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO), 1);
    EXPECT_EQ(SaveSlots.missing, std::vector<int>{1});

    // ...and they are themselves again once the definitions return.
    item::DropOrphans();
    item::Register(item::Def{.id = "chronicle:rod", .number = kRod});
    item::Register(item::Def{.id = "chronicle:bass", .number = kBass, .state = {{"size", 31.5}}, .state_version = 2});
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_GET_ALL_SAVE_FILE_INFO), 1);
    EXPECT_TRUE(SaveSlots.missing.empty());
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    EXPECT_EQ(pack.item[1], kRod);
    EXPECT_EQ(item::Definition(pack.item[2]), kBass);
    EXPECT_EQ(item::FindCopy(pack.item[2])->state.at("size"), 31.5);
    fs::remove_all(root);
}

TEST(SaveSlots, AHandleFoundTwiceInASaveBecomesTwoCopies) {
    fs::path root = UseTempSaveRoot();
    PrepareSave();
    RegisterCustomItems();
    const int handle = item::NewCopy(*item::Find("chronicle:bass"));
    PackOf(g_save).item[0] = static_cast<s16>(handle);
    PackOf(g_save).item[5] = static_cast<s16>(handle);
    BuildImage();
    ASSERT_EQ(SaveNew(0), 1);
    g_mc.file_no = 0;
    ASSERT_EQ(RunOperation(g_mc, MC_OPERATION_LOAD), 1);
    const item::Copy *first = item::FindCopy(PackOf(g_save).item[0]);
    const item::Copy *second = item::FindCopy(PackOf(g_save).item[5]);
    ASSERT_TRUE(first != nullptr && second != nullptr && first != second);
    EXPECT_NE(first->instance, second->instance);
    EXPECT_EQ(first->state, second->state);
    fs::remove_all(root);
}

TEST(SaveSlots, ADamagedChunkIsSetAsideAlone) {
    save::ExtensionChunk items{.tag = {'I', 'T', 'E', 'M'}, .version = 1, .payload = {1, 2, 3}};
    save::ExtensionChunk other{.tag = {'F', 'I', 'S', 'H'}, .version = 3, .payload = {4, 5}};
    std::vector<std::uint8_t> bytes = save::ExtensionEncode({{7, {items, other}}, {9, {other}}});
    std::optional<save::ExtensionSets> sets = save::ExtensionDecode(bytes);
    ASSERT_TRUE(sets && sets->size() == 2 && (*sets)[0].first == 7 && (*sets)[1].second.size() == 1);
    EXPECT_EQ((*sets)[0].second[1].payload, other.payload);
    EXPECT_EQ((*sets)[0].second[1].version, 3U);

    // The first chunk's payload starts after the header (12), the set's (12) and its own (16).
    bytes[12 + 12 + 16] ^= 0xFF;
    sets = save::ExtensionDecode(bytes);
    ASSERT_TRUE(sets);
    EXPECT_FALSE((*sets)[0].second[0].intact);
    EXPECT_TRUE((*sets)[0].second[1].intact);

    bytes.pop_back();
    EXPECT_FALSE(save::ExtensionDecode(bytes));
}

TEST(SaveFixtureSeed, CopiesIntoNewAndEmptyRootsWithoutChangingSource) {
    const fs::path root = UseTempSaveRoot();
    const fs::path source = root / "fixture";
    fs::create_directories(source / "saves" / "1");
    const std::vector<char> bytes{'s', '\0', 'v', static_cast<char>(0xff)};
    WriteFile(source / "saves" / "1" / "save.dat", bytes);
    WriteFile(source / "saves" / "state.json", {'{', '}'});
    std::string error;
    for (const char *name : {"new", "empty"}) {
        const fs::path destination = root / name;
        if (std::string_view(name) == "empty") {
            fs::create_directory(destination);
        }
        ASSERT_TRUE(SaveStateSeed(source, destination, error)) << error;
        EXPECT_EQ(ReadFile(destination / "saves" / "1" / "save.dat"), bytes);
        EXPECT_EQ(ReadFile(destination / "saves" / "state.json"), ReadFile(source / "saves" / "state.json"));
        EXPECT_FALSE(fs::exists(destination / "config.json"));
    }
    EXPECT_EQ(ReadFile(source / "saves" / "1" / "save.dat"), bytes);
    EXPECT_EQ(std::distance(fs::directory_iterator(source), fs::directory_iterator()), 1);
    fs::remove_all(root);
}

TEST(SaveFixtureSeed, RefusesNonemptyDestinationWithoutOverwritingOrAddingFiles) {
    const fs::path root = UseTempSaveRoot();
    const fs::path source = root / "fixture";
    const fs::path destination = root / "existing";
    fs::create_directory(source);
    fs::create_directory(destination);
    WriteFile(source / "save.dat", {'n', 'e', 'w'});
    WriteFile(source / "state.json", {'{', '}'});
    const std::vector<char> original{'u', 's', 'e', 'r'};
    WriteFile(destination / "save.dat", original);
    std::string error;
    EXPECT_FALSE(SaveStateSeed(source, destination, error));
    EXPECT_NE(error.find("new or empty"), std::string::npos);
    EXPECT_EQ(ReadFile(destination / "save.dat"), original);
    EXPECT_FALSE(fs::exists(destination / "state.json"));
    EXPECT_EQ(ReadFile(source / "save.dat"), (std::vector<char>{'n', 'e', 'w'}));
    fs::remove_all(root);
}

TEST(SaveFixtureSeed, RejectsOverlappingRootsBeforeCreatingDestination) {
    const fs::path root = UseTempSaveRoot();
    const fs::path source = root / "fixture";
    fs::create_directory(source);
    WriteFile(source / "save.dat", {'x'});
    std::string error;
    EXPECT_FALSE(SaveStateSeed(source, source, error));
    EXPECT_FALSE(SaveStateSeed(source, source / "nested", error));
    EXPECT_FALSE(fs::exists(source / "nested"));
    EXPECT_FALSE(SaveStateSeed(source, root, error));
    EXPECT_EQ(ReadFile(source / "save.dat"), (std::vector<char>{'x'}));
    fs::remove_all(root);
}

TEST(SaveFixtureSeed, MissingSourceDoesNotCreateDestination) {
    const fs::path root = UseTempSaveRoot();
    std::string    error;
    EXPECT_FALSE(SaveStateSeed(root / "missing", root / "destination", error));
    EXPECT_FALSE(fs::exists(root / "destination"));
    fs::remove_all(root);
}

} // namespace dc::test
