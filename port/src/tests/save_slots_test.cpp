#include <gtest/gtest.h>
#include <libpad.h>

#include <climits>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "platform/input.hpp"
#include "gameloop.hpp"
#include "gamepad.hpp"
#include "memcard.hpp"
#include "memorycardaccess.hpp"
#include "menu_save.hpp"
#include "platform/paths.hpp"
#include "platform/save_state.hpp"
#include "platform_fixture.hpp"
#include "save_slots.hpp"
#include "savedata.hpp"

#ifndef _WIN32
#include <sys/stat.h>
#endif

// The save screens' list and CMemoryCardAccess on the save folders, driven as the save menu does:
// SetFuncNo, then Step until the operation finishes, on a save root in a temporary directory.

namespace fs = std::filesystem;

extern int (*SaveMenuFunc[26])();

namespace {

constexpr int kImageSize = 0x136A7;
constexpr int kSaveDataSize = 0x131C0;
constexpr int kChecksumOffset = 0x131E0;
constexpr int kSaveMapNoOffset = 0x1C8;

alignas(64) char g_menu_buffer[0x30000];
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
        name[i] = static_cast<s16>("Toan"[i]);
    }
    g_save.AddPlayTime(123456);
    g_save.QuestDungeon(0, 7);
    g_save.QuestDungeon(6, 3);
    MapNoOf(g_save) = 42;
    ASSERT_TRUE(g_mc.InitForMC() == 0);
    g_mc.SetBuff(g_menu_buffer);
}

unsigned char g_pad_buffer[2][1024];

// As CGamePad::Init leaves the pads, past its wait on sceGsSyncV (platform_pad_test.cpp).
void OpenPads() {
    ASSERT_TRUE(scePadInit(0) == 1);
    ASSERT_TRUE(scePadPortOpen(0, 0, g_pad_buffer[0]) == 1);
    ASSERT_TRUE(scePadPortOpen(1, 0, g_pad_buffer[1]) == 1);
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
    for (const char *name : {"", "0", "01", "1.tmp", "-1", "1234567890", "darkcloud1", "state.json", "1a"}) {
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
    ASSERT_TRUE(std::strcmp(image.data() + kSaveDataSize, "darkcloudVer1.9") == 0);
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
    reinterpret_cast<s32 *>(expected.data() + (static_cast<char *>(g_save.GetConfigData()) - reinterpret_cast<char *>(&g_save)))[kLastFile] = 2;
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
    for (const char *value : {"-1", "1000000000", "18446744073709551615", "1.5", "\"2\""}) {
        text = std::string(R"({"game_clear":true,"options":[1,2],"last_save":)") + value + "}";
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
    g_mc.SetBuff(g_menu_buffer);
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
    g_mc.SetBuff(g_menu_buffer);
    ASSERT_EQ(SaveNew(0), 1);
    g_mc.file_no = 0;

    for (const char *text : {"", "malformed", "[]", "{}", R"({"game_clear":1})"}) {
        for (int clear : {0, 1}) {
            fs::remove(SaveStatePath());
            if (*text) {
                std::string content = text;
                WriteFile(SaveStatePath(), {content.begin(), content.end()});
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
    std::strcpy(other.data() + kSaveDataSize, "darkcloudVer1.0");
    WriteSave(root, 4, other);
    std::vector<char> damaged = image;
    *reinterpret_cast<s32 *>(damaged.data() + kSaveMapNoOffset) = 0x7fffffff;
    WriteSave(root, 0, damaged);
    fs::create_directories(SlotOf(root, 2));
    WriteFile(SlotOf(root, 9), {'x'});
    MapNoOf(g_save) = 0x7fffffff;
    g_save.QuestDungeon(0, -8);
    g_save.QuestDungeon(6, INT_MIN);
    g_mc.SetBuff(g_menu_buffer);
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
