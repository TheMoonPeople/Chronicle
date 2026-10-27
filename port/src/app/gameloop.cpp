#include "app/gameloop.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <print>
#include <string_view>
#include <vector>

#include "app/exitcodes.hpp"
#include "app/presence.hpp"
#include "battle_globals.hpp"
#include "battlemenu.hpp"
#include "btsysscript.hpp"
#include "data/data_read.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "editloop.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "langset.hpp"
#include "localize/localize.hpp"
#include "main.hpp"
#include "mainselect.hpp"
#include "memorycardaccess.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "nowload.hpp"
#include "options/menu_option.hpp"
#include "platform/clock.hpp"
#include "platform/config.hpp"
#include "platform/input.hpp"
#include "platform/paths.hpp"
#include "quest/host.hpp"
#include "render/draw2d.hpp"
#include "render/gs_state.hpp"
#include "save/save_slots.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "text/gametext.hpp"
#include "texture.hpp"
#include "title/opening.hpp"
#include "title/rushmovi.hpp"
#include "title/title.hpp"

int  EditInit(void *param);
int  EditLoop();
bool TakeBattleMenuLoadRequest();
bool TakeBattleMenuTitleRequest();

extern s32 mode;
extern s32 gameTask;
extern int viewMode;
extern s32 mc_mode;
extern s32 NextMapNo;

namespace {

// Words of CSaveData::config that main() reads and writes directly.
constexpr int kConfigVibrationOff = 7;
constexpr int kConfigScreenX = 12;
constexpr int kConfigScreenY = 13;
constexpr int kConfigGameClear = 14;
constexpr int kConfigLastFile = 17;

// CSaveData::map_no is private; retail's main() writes it by offset.
constexpr int kSaveMapNoOffset = 0x1C8;

constexpr int kWarmUpTicks = 60;

// The TEXA vudata.cpp's My_DrawEnv loads at the start of every mode, through My_dma_start0.
constexpr sceGsTexa kDrawEnvTexa = {.TA0 = 0x80, .pad8 = 0, .AEM = 1, .pad16 = 0, .TA1 = 0x80, .pad40 = 0};

void SetMainSelectParam(std::string_view text) {
    std::ranges::copy(text, std::begin(main_select_param));
    main_select_param[text.size()] = '\0';
}

bool g_menu_chord_held = false;
bool g_debug_r3_held = false;

std::vector<int> g_give;

bool Holds(CDngStatusData &status, int item) {
    if (item >= ITEM_WEAPON_START) {
        for (const auto &weapons : status.chara_weapons) {
            for (const WEAPON_HAVE &weapon : weapons) {
                if (weapon.item_no == item) {
                    return true;
                }
            }
        }
        return false;
    }
    return status.SearchItemIndexNo(item) >= 0;
}

void GiveOnArrival() {
    static int last_mode = -1;
    bool       arrived = mode != last_mode && (mode == GAME_MODE_EDIT || mode == GAME_MODE_DUNGEON);
    last_mode = mode;
    if (!arrived || g_give.empty() || SaveData == nullptr) {
        return;
    }
    CDngStatusData &status = *SaveData->GetDngStatus();
    for (int item : g_give) {
        if (!Holds(status, item)) {
            status.GetItem(item, 0);
        }
    }
}

struct Jump {
    bool set = false;
    int  mode = GAME_MODE_MENU;
    int  map = 0;
};

Jump g_jump;
bool g_fast_load;
bool g_autosave;
int  g_developer_return_mode = GAME_MODE_TITLE;
int  g_developer_return_map = 800;
int  g_developer_return_local_map = 0;
int  g_developer_return_menu_no = 0;
bool g_developer_return_pending = false;

std::int64_t g_frame_budget = -1;
std::int64_t g_frames;
bool         g_stop;

// Everything else reaches the save through SaveData.
alignas(64) CSaveData g_save_data;

s32 *ConfigWords() {
    return static_cast<s32 *>(SaveData->GetConfigData());
}

void ModeInit(int &title_ran, int &exist_data, bool &skip_title) {
    if (mode == GAME_MODE_MENU && !g_developer_return_pending) {
        g_developer_return_mode = GAME_MODE_TITLE;
    }
    if (mode != GAME_MODE_MENU && mode != GAME_MODE_LOADER) {
        g_developer_return_pending = false;
    }
    InputSetMenuNavigation(mode == GAME_MODE_MENU || mode == GAME_MODE_LOADER);
    InputSetDeveloperMenu(mode == GAME_MODE_MENU);
    switch (mode) {
        case GAME_MODE_LOADER:
            LoaderInit();
            break;
        case GAME_MODE_LANGUAGE:
            LangsetInit();
            break;
        case GAME_MODE_TITLE:
            LoadSystemMessage();
            GlobalNameInit();
            SndInitialize(4, 30, 4, 5);
            if (!title_ran) {
                title_ran = 1;
                exist_data = InitExistData();
                if (ConfigWords()[kConfigGameClear] != 0) {
                    GameClearFlag = 1;
                }
            }
            TitleInit(exist_data);
            break;
        case GAME_MODE_RUSH_MOVIE:
            SndInitialize(4, 30, 4, 5);
            RushInit();
            if (GamePad.On(PAD_START) != 0) {
                MapJump(800, -1);
                skip_title = true;
            }
            break;
        case GAME_MODE_EDIT:
            EditInit(nullptr);
            break;
        case GAME_MODE_MENU:
            MenuInit();
            break;
        case GAME_MODE_SAVE:
            InitSave();
            break;
        case GAME_MODE_MEMORY_CHECK:
            MemCheckInit();
            break;
        case GAME_MODE_TRIAL_END:
            TrialEndInit();
            break;
        case GAME_MODE_OPENING:
            SndInitialize(4, 30, 4, 5);
            OpeningInit();
            break;
        case GAME_MODE_UNUSED_6:
        case GAME_MODE_UNUSED_8:
        case GAME_MODE_UNUSED_12:
            break;
        default:
            GameInit();
            break;
    }
}

void StartNewGame() {
    auto names = std::make_unique<std::array<char, 64>[]>(6);
    for (int i = 0; i < 6; ++i) {
        std::memcpy(names[i].data(), g_save_data.GetCharaName(i), names[i].size());
    }
    ResetSaveKeepingConfig(g_save_data);
    GameOptionsApply(ConfigGet().options);
    for (int i = 0; i < 6; ++i) {
        std::memcpy(g_save_data.GetCharaName(i), names[i].data(), names[i].size());
    }
    QuestHostNewGame();
    TrialStart();
}

// One frame of the current mode; a non-zero result ends it.
int ModeLoop(bool &skip_title) {
    switch (mode) {
        case GAME_MODE_LANGUAGE:
            return LangsetLoop();
        case GAME_MODE_TITLE:
            return TitleLoop();
        case GAME_MODE_RUSH_MOVIE:
            if (skip_title) {
                skip_title = false;
                return 1;
            }
            return RushLoop();
        case GAME_MODE_EDIT:
            return EditLoop();
        case GAME_MODE_MENU:
            if (GamePad.GetPadDown() & PAD_CIRCLE) {
                return -1;
            }
            return MenuLoop();
        case GAME_MODE_SAVE:
            return LoopSave();
        case GAME_MODE_UNUSED_12:
            return 0;
        case GAME_MODE_MEMORY_CHECK:
            return MemCheckLoop();
        case GAME_MODE_TRIAL_END:
            return TrialEndLoop();
        case GAME_MODE_OPENING:
            return OpeningLoop();
        case GAME_MODE_LOADER:
            return LoaderLoop();
        case GAME_MODE_UNUSED_6:
        case GAME_MODE_UNUSED_8:
            return 0;
        default:
            return GameLoop();
    }
}

// What My_dma_start0 sent to the GS before and after a mode's Init. The rest of My_DrawEnv is
// state the renderer fixes or takes from elsewhere (COLCLAMP, PRMODECONT, FOGCOL from
// mgRenderInfo) or that the first frame overwrites (SCISSOR, ALPHA, TEX0, TEX1).
void LoadDrawEnv() {
    sceGsTexa texa = kDrawEnvTexa;
    MGSetGsTEXA(&texa);
}

// What the developer menu's choice (and the dungeon loader's) leaves behind.
void ApplyJump() {
    OldMapNo = -1;
    NextMapNo = -1;
    GamePad.AutoRepeatOff();
    switch (g_jump.mode) {
        case GAME_MODE_EDIT:
            main_select_menu_no = g_jump.map;
            MapNo = g_jump.map;
            if (g_jump.map != 99) {
                LocalMapNo = g_jump.map;
            }
            mode = GAME_MODE_EDIT;
            break;
        case GAME_MODE_DUNGEON:
            selectMapNo = g_jump.map;
            main_select_menu_no = g_jump.map;
            MapJump(g_jump.map + 200, -1);
            MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
            GameFollowMapJump();
            break;
        case GAME_MODE_TITLE:
            MapJump(800, -1);
            GameFollowMapJump();
            break;
        case GAME_MODE_RUSH_MOVIE:
            MapJump(801, -1);
            GameFollowMapJump();
            break;
        case GAME_MODE_OPENING:
            MapJump(400, -1);
            GameFollowMapJump();
            break;
        default:
            mode = GAME_MODE_MENU;
            break;
    }
    NextMapNo = -1;
}

// Quit from the pause menu: the town or dungeon is left where it stands, as the developer menu leaves
// it (GameEnterDeveloperMenu), and the title screen loads as it does at start-up.
void ReturnToTitle() {
    while (ReadBGSync() != 0) {
        ClockSyncV();
    }
    SndExit();
    MGScisioringForce(0);
    GamePad.KeyLock(0);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    StartEventNo = -1;
    MapJump(800, -1);
}

bool FrameBoundaryStop() {
    return g_stop || (g_frame_budget >= 0 && g_frames >= g_frame_budget);
}

} // namespace

void ResetSaveKeepingConfig(CSaveData &save) {
    SV_CONFIG_SYS config{};
    // The configuration's copy holds one byte per word; the last file's number may not fit one.
    const s32 last_file = static_cast<s32 *>(save.GetConfigData())[kConfigLastFile];
    save.ConvertConfig(&config);
    std::memset(static_cast<void *>(&save), 0, sizeof(CSaveData));
    save.Initialize();
    save.InvertConfig(&config);
    static_cast<s32 *>(save.GetConfigData())[kConfigLastFile] = last_file;
}

void GameFollowMapJump() {
    if (NextMapNo < 0) {
        return;
    }
    OldMapNo = MapNo;
    if (NextMapNo < 200) {
        mode = GAME_MODE_EDIT;
        MapNo = NextMapNo;
    } else if (NextMapNo < 300) {
        mode = GAME_MODE_DUNGEON;
        MapNo = NextMapNo;
        LocalMapNo = NextMapNo - 200;
        main_select_menu_no = LocalMapNo;
    } else if (NextMapNo == 400) {
        mode = GAME_MODE_OPENING;
        MapNo = NextMapNo;
        LocalMapNo = 0;
        main_select_menu_no = 0;
    } else if (NextMapNo >= 800) {
        main_select_menu_no = 0;
        SetMainSelectParam("title");
        if (NextMapNo == 800) {
            mode = GAME_MODE_TITLE;
        }
        if (NextMapNo == 801) {
            mode = GAME_MODE_RUSH_MOVIE;
        }
        MapNo = NextMapNo;
        LocalMapNo = 0;
    }
    if (NextMapNo == 1000) {
        MapNo = -1;
        LocalMapNo = 0;
        mode = GAME_MODE_SAVE;
        mc_mode = SAVE_MENU_MODE_ENDING;
    }
}

void GameApplyLoopResult(int loop_mode, int result) {
    switch (loop_mode) {
        case GAME_MODE_LANGUAGE:
            // There is no memory card check to run, so the attract movie comes next.
            if (result != 0) {
                MapNo = 801;
                mode = GAME_MODE_RUSH_MOVIE;
            }
            break;
        case GAME_MODE_TITLE:
            if (result == 1 || result == 2) {
                main_select_menu_no = result == 1 ? 0 : 1;
                SetMainSelectParam("e01");
                mode = GAME_MODE_EDIT;
            }
            if (result == 3) {
                mode = GAME_MODE_DUNGEON;
                main_select_menu_no = 0;
            }
            if (result == 5) {
                mode = GAME_MODE_OPENING;
            }
            if (result == 1) {
                MapJump(400, -1);
                StartNewGame();
            }
            if (result == 4) {
                MapJump(801, -1);
            }
            if (result == 2) {
                SndInitialize(4, 30, 4, 5);
            }
            break;
        case GAME_MODE_RUSH_MOVIE:
            if (result != 0) {
                MapJump(800, -1);
            }
            break;
        case GAME_MODE_EDIT:
            // Sequential, as retail tests them: 1 and 2 are overwritten by MENU.
            if (result == 1) {
                mode = GAME_MODE_TITLE;
            }
            if (result == 2) {
                mode = GAME_MODE_EDIT;
            }
            if (result != 0) {
                mode = GAME_MODE_MENU;
            }
            if (result == 3) {
                mode = GAME_MODE_DUNGEON;
            }
            break;
        case GAME_MODE_SAVE:
        case GAME_MODE_UNUSED_12:
            if (result != 0) {
                mode = GAME_MODE_MENU;
            }
            break;
        case GAME_MODE_MEMORY_CHECK:
            if (result != 0) {
                MapNo = 801;
                mode = GAME_MODE_RUSH_MOVIE;
            }
            break;
        case GAME_MODE_TRIAL_END:
            if (result != 0) {
                MapNo = 800;
                mode = GAME_MODE_TITLE;
            }
            break;
        case GAME_MODE_OPENING:
            if (result != 0) {
                mode = GAME_MODE_TITLE;
                MapJump(0, -1);
            }
            break;
        case GAME_MODE_LOADER:
            if (result != 0) {
                mode = result == 2 ? GAME_MODE_MENU : GAME_MODE_DUNGEON;
            }
            break;
        case GAME_MODE_MENU:
            if (result == -1) {
                mode = g_developer_return_mode;
                if (g_developer_return_pending) {
                    MapNo = g_developer_return_map;
                    LocalMapNo = g_developer_return_local_map;
                    main_select_menu_no = g_developer_return_menu_no;
                } else {
                    MapJump(800, -1);
                }
            }
            break;
        case GAME_MODE_UNUSED_6:
        case GAME_MODE_UNUSED_8:
            break;
        default:
            if (result != 0) {
                mode = GAME_MODE_MENU;
            }
            break;
    }
}

bool GameSetGive(const char *list) {
    std::vector<int> items;
    std::string_view rest = list;
    for (bool more = true; more;) {
        std::string_view token = rest.substr(0, rest.find(','));
        more = token.size() < rest.size();
        rest.remove_prefix(std::min(rest.size(), token.size() + 1));
        int item = 0;
        auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), item);
        if (error != std::errc{} || end != token.data() + token.size() || item < 1 || item >= ITEM_WEAPON_END) {
            return false;
        }
        items.push_back(item);
    }
    g_give = std::move(items);
    return !g_give.empty();
}

bool GameSetJump(const char *spec) {
    std::string_view text = spec;
    std::string_view name = text.substr(0, text.find(':'));
    std::string_view number = name.size() < text.size() ? text.substr(name.size() + 1) : std::string_view();
    Jump             jump;
    jump.set = true;
    if (!number.empty()) {
        auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), jump.map);
        if (error != std::errc{} || end != number.data() + number.size() || jump.map < 0) {
            return false;
        }
    }
    bool numbered = name == "edit" || name == "dungeon";
    if (!numbered && !number.empty()) {
        return false;
    }
    if (name == "edit") {
        jump.mode = GAME_MODE_EDIT;
    } else if (name == "dungeon" && jump.map <= 6) {
        jump.mode = GAME_MODE_DUNGEON;
    } else if (name == "title") {
        jump.mode = GAME_MODE_TITLE;
    } else if (name == "rush") {
        jump.mode = GAME_MODE_RUSH_MOVIE;
    } else if (name == "opening") {
        jump.mode = GAME_MODE_OPENING;
    } else if (name == "menu") {
        jump.mode = GAME_MODE_MENU;
    } else {
        return false;
    }
    g_jump = jump;
    return true;
}

// Retail PAL tests pad 2's four shoulder buttons with R3's press edge after every frame; here they are
// pad 1's, as the host latched it, so the chord can enable debug mode after an ordinary boot.
void GameCheckDebugToggle() {
    constexpr std::uint16_t shoulders = kInputL1 | kInputR1 | kInputL2 | kInputR2;
    std::uint16_t           buttons = InputGetPad(0).buttons;
    bool                    held = (buttons & kInputR3) != 0;
    bool                    pressed = held && !g_debug_r3_held;
    g_debug_r3_held = held;
    if (!pressed || (buttons & shoulders) != shoulders) {
        return;
    }
    DebugMode = !DebugMode;
    std::println(stderr, "debug mode {}", DebugMode ? "on" : "off");
}

// The pad as the host latched it, so a mode that has locked the game's pad (an event, a fade) still
// leaves. Only the press of the chord counts: held into the next mode, it does not leave that too.
bool GameDeveloperMenuRequested() {
    constexpr std::uint16_t chord = kInputSelect | kInputStart;
    bool                    held = (InputGetPad(0).buttons & chord) == chord;
    bool                    bound = InputHostPressed(InputHostAction::DeveloperMenu);
    bool                    pressed = (held && !g_menu_chord_held) || bound;
    g_menu_chord_held = held;
    return pressed && DebugMode != 0 && mode != GAME_MODE_MENU;
}

// A mode's own exit stops its sound, unlocks the pad and clears its scissor; this does it for a mode
// cut off mid-frame, and drops the map jump or event it had pending.
void GameEnterDeveloperMenu() {
    g_developer_return_mode = mode;
    g_developer_return_map = MapNo;
    g_developer_return_local_map = LocalMapNo;
    g_developer_return_menu_no = main_select_menu_no;
    g_developer_return_pending = true;
    while (ReadBGSync() != 0) {
        ClockSyncV();
    }
    SndExit();
    MGScisioringForce(0);
    GamePad.KeyLock(0);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    NextMapNo = -1;
    StartEventNo = -1;
    mode = GAME_MODE_MENU;
    InputSetMenuNavigation(true);
    InputSetDeveloperMenu(true);
    std::println(stderr, "debug mode: start + select, the developer menu");
}

void GameSetAutosave(bool on) {
    g_autosave = on;
}

void GameSetFastLoad(bool fast) {
    g_fast_load = fast;
}

bool GameFastLoad() {
    return g_fast_load;
}

void GameSetFrameBudget(std::int64_t frames) {
    g_frame_budget = frames;
}

std::int64_t GameFrameCount() {
    return g_frames;
}

void GameRequestStop() {
    g_stop = true;
}

bool GameStopRequested() {
    return g_stop;
}

int RunGame([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
    DebugMode = ConfigGet().debug_mode ? 1 : 0;
    g_developer_return_pending = false;
    g_developer_return_mode = GAME_MODE_TITLE;
    mode = GAME_MODE_MENU;
    main_select_menu_no = 0;
    SetMainSelectParam("e01");
    InitCDFile();
    // With one language there is nothing to choose: the game starts in it, past the language
    // select, as retail NTSC does. With several the select offers retail's five.
    std::vector<s32> languages = SupportedLanguages();
    if (languages.size() == 1) {
        LanguageCode = languages[0];
    } else if (languages.empty()) {
        std::println(stderr, "the data has no languages.json; run dcdata extract again");
    }
    MGInit();
    InitMemoryFile();
    BufferAllClear();
    InitReadBG();

    ClockSyncV();
    GamePad.Init();

    SaveData = &g_save_data;
    NextMapNo = -1;
    MapNo = -1;
    OldMapNo = -1;
    StartEventNo = -1;
    GameClearFlag = 0;
    std::memset(static_cast<void *>(&g_save_data), 0, sizeof(CSaveData));
    g_save_data.Initialize();
    GlobalNameInit();

    if (g_jump.set) {
        DebugMode = 1;
    }
    for (int tick = 0; tick < kWarmUpTicks && !g_jump.set; ++tick) {
        ClockSyncV();
        GamePad.UpDate();
    }
    if (DebugMode && !g_jump.set) {
        std::println(stderr, "debug mode on");
    }
    if (!g_jump.set) {
        MapNo = -1;
        mode = GAME_MODE_LANGUAGE;
        GamePad.KeyLock2(1);
        if (languages.size() == 1) {
            GameApplyLoopResult(GAME_MODE_LANGUAGE, 1);
        } else if (const int language = ConfigGet().language;
                   std::ranges::find(languages, language) != languages.end()) {
            LanguageCode = language;
            MapNo = 801;
            mode = GAME_MODE_RUSH_MOVIE;
        }
    }
    if (g_jump.set) {
        // A jump skips the language screen, so the language chosen in config.json is the one it starts in.
        if (const int language = ConfigGet().language;
            std::ranges::find(languages, language) != languages.end()) {
            LanguageCode = language;
        }
        ApplyJump();
    }

    int  title_ran = 0;
    int  exist_data = 0;
    bool data_loaded = false;
    bool skip_title = false;
    for (;;) {
        // A language chosen on the Options screen takes hold here, where the next area is about to load.
        LocalizeApplyPendingLanguage();
        if (mode != GAME_MODE_UNUSED_12 && !data_loaded) {
            initialize_data();
            data_loaded = true;
        }
        InitReadBG();
        SndInit();

        if (!DebugMode && mode == GAME_MODE_MENU) {
            MapNo = 801;
            mode = GAME_MODE_RUSH_MOVIE;
        }

        LoadOverlay(mode);
        MGSetRenderInfo(800.0f, 10.0f, 65535.0f);
        switch (mode) {
            case GAME_MODE_LOADER:
            case GAME_MODE_MENU:
            case GAME_MODE_MEMORY_CHECK:
            case GAME_MODE_LANGUAGE:
                MapNo = -1;
                OldMapNo = -1;
                break;
            default:
                break;
        }
        LoadDrawEnv();

        if (mode != GAME_MODE_UNUSED_12 && StartEventNo < 0) {
            init_now_loading(MapNo);
        }
        GamePad.StopVibration();
        if (DebugMode) {
            LoadSystemMessage();
        }

        // A save loaded since the last mode brought its own options.
        GameOptionsApply(ConfigGet().options);
        bool event_start = StartEventNo >= 0;
        ModeInit(title_ran, exist_data, skip_title);

        NextMapNo = -1;
        // check_now_loading pumps the clock while the loading screen runs.
        while (check_now_loading() == 0) {
        }

        MGInitVSyncCallBack(PlayTimeCount);
        LoadDrawEnv();
        ClockSyncV();
        MGPortCutInterpolation();
        *reinterpret_cast<s32 *>(reinterpret_cast<char *>(SaveData) + kSaveMapNoOffset) = MapNo;
        if (g_autosave && mode == GAME_MODE_EDIT && !event_start && !SaveSlotWriteAutosave()) {
            std::println(stderr, "autosave: could not write {}", PathsDisplay(SaveSlotPath(kSaveSlotAutosave)));
        }

        int result = 0;
        do {
            if (FrameBoundaryStop()) {
                return kExitOk;
            }
            MGAdjustScreen(ConfigWords()[kConfigScreenX], ConfigWords()[kConfigScreenY]);
            ConfigWords()[kConfigGameClear] = GameClearFlag;

            MGBeginFrame();
            SetEnv(Vif1Packet);
            PolyCount = 0;
            old_main_mode = mode;

            result = ModeLoop(skip_title);
            GameApplyLoopResult(old_main_mode, result);
            if (result == 0 && LocalizeLanguagePending() && !MenuOptionOpen() &&
                (mode == GAME_MODE_EDIT || mode == GAME_MODE_TITLE)) {
                // The language was changed in this town or title and the Options screen has closed: load
                // the same map again, now in the new language. A dungeon floor is not restarted for it;
                // the language takes hold when the next area loads.
                NextMapNo = MapNo;
                StartEventNo = -1;
                result = 1;
            }

            if (result == 0 && TakeBattleMenuLoadRequest()) {
                const s32 loaded_map = *reinterpret_cast<const s32 *>(
                    reinterpret_cast<const char *>(SaveData) + kSaveMapNoOffset);
                MapNo = (loaded_map >= 0 && loaded_map < 200) ? loaded_map : 0;
                LocalMapNo = MapNo;
                OldMapNo = -1;
                NextMapNo = -1;
                StartEventNo = -1;
                GameApplyLoopResult(GAME_MODE_TITLE, 2);
                break;
            }

            if (result == 0 && TakeBattleMenuTitleRequest()) {
                ReturnToTitle();
                break;
            }

            GiveOnArrival();
            InputSetLookOnLeftStick((mode == GAME_MODE_DUNGEON && gameTask == GAME_TASK_EYE_CAMERA) ||
                                    (mode == GAME_MODE_EDIT && viewMode != 0 && EdInteriorFlag == 0));
            GamePad.UpDate();
            GamePad.VibrationEnable(ConfigWords()[kConfigVibrationOff] == 0);
            GamePad.Step();
            MGEndFrame();
            ++g_frames;
            PresenceTick();

            GameCheckDebugToggle();
            if (GameDeveloperMenuRequested()) {
                GameEnterDeveloperMenu();
                break;
            }
        } while (result == 0);

        MGBeginFrame();
        MGEndFrame();

        GameFollowMapJump();
        if (CheckTrialEnd() != 0) {
            mode = GAME_MODE_TRIAL_END;
            MapNo = -1;
            LocalMapNo = -1;
        }

        // Reads complete as they are issued; pumping the clock keeps a stray busy slot from
        // hanging here.
        while (ReadBGSync() != 0) {
            ClockSyncV();
        }
    }
}
