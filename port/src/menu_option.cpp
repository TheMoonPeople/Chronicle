#include "menu_option.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

#include "memcard.hpp"
#include "options/screen.hpp"
#include "savedata.hpp"
#include "sound.hpp"
#include "userstatus.hpp"

namespace {

// The save's configuration words (CSaveData::config) that hold the game's options.
constexpr int kWordClockOff = 2;
constexpr int kWordFastTime = 3;
constexpr int kWordFastMessages = 4;
constexpr int kWordMono = 5;
constexpr int kWordSoftFocusOff = 6;
constexpr int kWordVibrationOff = 7;
constexpr int kWordNamesOff = 8;
constexpr int kWordPlayerDamageOff = 9;
constexpr int kWordEnemyDamageOff = 10;
constexpr int kWordEnemyHpOff = 11;

// minimap_status that hides the dungeon map.
constexpr int kMapOff = 3;

// The smallest size the list offers from the display's own modes.
constexpr int kMinWidth = 800;
constexpr int kMinHeight = 600;

// 0x0 is the monitor's resolution.
constexpr OptionResolution kResolutions[] = {
    {0,    0   },
    {1024, 768 },
    {1280, 720 },
    {1280, 960 },
    {1366, 768 },
    {1440, 1080},
    {1600, 900 },
    {1920, 1080},
    {2560, 1440},
    {3840, 2160},
};

void WriteOptions(CSaveData &save, const ConfigGameOptions &options) {
    s32 *words = static_cast<s32 *>(save.GetConfigData());
    words[kWordClockOff] = !options.clock;
    words[kWordFastTime] = options.fast_time;
    words[kWordFastMessages] = options.fast_messages;
    words[kWordMono] = !options.stereo;
    words[kWordSoftFocusOff] = !options.soft_focus;
    words[kWordVibrationOff] = !options.vibration;
    words[kWordNamesOff] = !options.names;
    words[kWordPlayerDamageOff] = !options.player_damage;
    words[kWordEnemyDamageOff] = !options.enemy_damage;
    words[kWordEnemyHpOff] = !options.enemy_hp;
    reinterpret_cast<CUserStatus *>(save.GetDngStatus())->minimap_status = options.map == 0 ? kMapOff : options.map - 1;
    save.GetMenuCursor()->reset_pos = !options.save_cursor_position;
}

} // namespace

std::vector<OptionResolution> OptionResolutionList(std::span<const DisplayModeSize> modes, int configured_width,
                                                   int configured_height, bool display_known, int display_width,
                                                   int display_height) {
    std::vector<OptionResolution> sizes(std::begin(kResolutions), std::end(kResolutions));
    for (const DisplayModeSize &mode : modes) {
        if (mode.width >= kMinWidth && mode.height >= kMinHeight) {
            sizes.push_back({mode.width, mode.height});
        }
    }
    bool configured = configured_width > 0 && configured_height > 0;
    if (configured) {
        sizes.push_back({configured_width, configured_height});
    }
    // Desktop stays first; the rest run from the smallest area, narrower first.
    std::sort(sizes.begin() + 1, sizes.end(), [](const OptionResolution &a, const OptionResolution &b) {
        return std::pair(a.width * a.height, a.width) < std::pair(b.width * b.height, b.width);
    });
    std::vector<OptionResolution> listed;
    for (const OptionResolution &size : sizes) {
        bool fits = !display_known || (size.width <= display_width && size.height <= display_height) ||
                    (configured && size.width == configured_width && size.height == configured_height);
        bool again = !listed.empty() && listed.back().width == size.width && listed.back().height == size.height;
        if (fits && !again) {
            listed.push_back(size);
        }
    }
    return listed;
}

bool MenuOptionOpen() {
    return options::g_screen.open;
}

void GameOptionsApply(const ConfigGameOptions &options) {
    if (SaveData == nullptr) {
        return;
    }
    WriteOptions(*SaveData, options);
    CMenuCursor *cursor = SaveData->GetMenuCursor();
    if (cursor->reset_pos != 0) {
        cursor->InitPos();
    }
    // Takes config.json's stereo option whatever it is passed.
    CSnd.SetStereoMode(1);
}

void GameOptionsChanged(const Config &before, const Config &after) {
    if (after.options != before.options) {
        GameOptionsApply(after.options);
    }
}

void GameOptionsClear(CSaveData &save) {
    // These bytes remain reserved in the fixed save layout, but carry no player preference.
    s32 *words = static_cast<s32 *>(save.GetConfigData());
    std::fill(words + kWordClockOff, words + kWordEnemyHpOff + 1, 0);
    words[15] = 0;
    words[16] = 0;
    reinterpret_cast<CUserStatus *>(save.GetDngStatus())->minimap_status = 0;
    save.GetMenuCursor()->reset_pos = 0;
}
