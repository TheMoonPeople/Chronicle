#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "memorycardaccess.hpp"
#include "menu_option.hpp"
#include "savedata.hpp"
#include "userstatus.hpp"

namespace {

alignas(64) CSaveData g_options_save;
alignas(64) char g_save_buffer[0x30000];

ConfigGameOptions AllChanged() {
    ConfigGameOptions options;
    options.save_cursor_position = false;
    options.vibration = false;
    options.fast_messages = true;
    options.stereo = false;
    options.clock = false;
    options.fast_time = true;
    options.map = 0;
    options.enemy_damage = false;
    options.player_damage = false;
    options.enemy_hp = false;
    options.names = false;
    options.soft_focus = false;
    return options;
}

} // namespace

// config.json's options reach the words the game reads, over whatever a loaded save brought; a save
// about to be written has reserved zero fields instead. State in the same block stays.
TEST(MenuOption, OptionsLiveInConfigNotInSaves) {
    g_options_save.Initialize();
    SaveData = &g_options_save;
    s32 *words = static_cast<s32 *>(g_options_save.GetConfigData());
    words[14] = 1;
    words[17] = 5;
    g_options_save.GetMenuCursor()->pos[0] = 3;

    GameOptionsApply(AllChanged());
    for (int word = 2; word <= 11; ++word) {
        ASSERT_TRUE(words[word] == 1) << word;
    }
    ASSERT_TRUE(reinterpret_cast<CUserStatus *>(g_options_save.GetDngStatus())->minimap_status == 3);
    ASSERT_TRUE(g_options_save.GetMenuCursor()->reset_pos == 1);
    ASSERT_TRUE(g_options_save.GetMenuCursor()->pos[0] == 0);

    CMemoryCardAccess card{};
    card.SetBuff(g_save_buffer);
    s32 *serialized = static_cast<s32 *>(card.save_buffer->GetConfigData());
    for (int word = 2; word <= 11; ++word) {
        ASSERT_EQ(serialized[word], 0) << word;
        ASSERT_EQ(words[word], 1) << word;
    }
    ASSERT_EQ(reinterpret_cast<CUserStatus *>(card.save_buffer->GetDngStatus())->minimap_status, 0);
    ASSERT_EQ(card.save_buffer->GetMenuCursor()->reset_pos, 0);
    ASSERT_EQ(serialized[15], 0);
    ASSERT_EQ(serialized[16], 0);
    ASSERT_TRUE(serialized[14] == 1 && serialized[17] == 5);
    ASSERT_EQ(reinterpret_cast<CUserStatus *>(g_options_save.GetDngStatus())->minimap_status, 3);
    ASSERT_EQ(g_options_save.GetMenuCursor()->reset_pos, 1);

    SV_CONFIG_SYS saved;
    card.save_buffer->ConvertConfig(&saved);
    ASSERT_TRUE(saved.values[5] == 0 && saved.values[15] == 0 && saved.values[16] == 0 && saved.values[14] == 1);
    // A load's neutral fields must be replaced by JSON before the game uses them again.
    g_options_save.InvertConfig(&saved);
    GameOptionsApply(AllChanged());
    ASSERT_EQ(words[5], 1);
    ASSERT_EQ(reinterpret_cast<CUserStatus *>(g_options_save.GetDngStatus())->minimap_status, 3);
    SaveData = nullptr;
}

namespace {

std::vector<DisplayModeSize> Modes() {
    return {
        {1920, 1200},
        {640,  480 },
        {1920, 1080},
        {2560, 1080},
        {1920, 1080},
        {800,  600 },
    };
}

bool Has(const std::vector<OptionResolution> &list, int width, int height) {
    return std::any_of(list.begin(), list.end(),
                       [&](const OptionResolution &size) { return size.width == width && size.height == height; });
}

} // namespace

// The row offers the display's own modes beside the curated sizes, each once, smallest first after
// Desktop, and nothing the display cannot show.
TEST(MenuOption, ResolutionListMergesDisplayModes) {
    std::vector<DisplayModeSize>  modes = Modes();
    std::vector<OptionResolution> list = OptionResolutionList(modes, 0, 0, true, 1920, 1200);
    ASSERT_FALSE(list.empty());
    ASSERT_EQ(list.front().width, 0);
    ASSERT_EQ(list.front().height, 0);
    ASSERT_TRUE(Has(list, 1920, 1200));
    ASSERT_TRUE(Has(list, 800, 600));
    ASSERT_TRUE(Has(list, 1366, 768));
    ASSERT_FALSE(Has(list, 640, 480)) << "below the smallest size offered";
    ASSERT_FALSE(Has(list, 2560, 1080)) << "wider than the display";
    ASSERT_FALSE(Has(list, 2560, 1440));
    ASSERT_EQ(std::count_if(list.begin(), list.end(), [](const OptionResolution &size) { return size.width == 1920 && size.height == 1080; }), 1);
    for (size_t i = 2; i < list.size(); ++i) {
        ASSERT_LE(list[i - 1].width * list[i - 1].height, list[i].width * list[i].height);
    }
}

// A size config.json holds stays in the list when the display would not list it, and a display
// that cannot be asked leaves the curated sizes whole.
TEST(MenuOption, ResolutionListKeepsConfiguredSize) {
    std::vector<OptionResolution> list = OptionResolutionList({}, 1234, 777, true, 1920, 1080);
    ASSERT_TRUE(Has(list, 1234, 777));
    ASSERT_TRUE(Has(list, 1920, 1080));
    ASSERT_FALSE(Has(list, 2560, 1440));

    list = OptionResolutionList({}, 0, 0, false, 0, 0);
    ASSERT_TRUE(Has(list, 3840, 2160));
    ASSERT_EQ(list.front().width, 0);
}

TEST(MenuOption, ZoomResetPresetsPreserveOtherBindingsAndRestoreDefault) {
    Config config;
    config.key_bindings.push_back({
        "cross", {"Space", "Z"}
    });
    ASSERT_EQ(OptionZoomResetChoice(config), 0);
    ASSERT_EQ(OptionZoomResetText(config), "Middle Mouse");
    ASSERT_EQ(OptionZoomResetCount(config), 5);
    OptionSetZoomReset(config, 2);
    ASSERT_EQ(OptionZoomResetChoice(config), 2);
    ASSERT_EQ(OptionZoomResetText(config), "Mouse5");
    ASSERT_EQ(config.key_bindings.front(), (ConfigKeyBinding{
                                               "cross", {"Space", "Z"}
    }));
    Config restored = ConfigParse(ConfigSerialize(config));
    ASSERT_EQ(OptionZoomResetText(restored), "Mouse5");
    OptionSetZoomReset(config, 4);
    ASSERT_EQ(OptionZoomResetText(config), "Disabled");
    OptionRestoreZoomReset(config, Config{});
    ASSERT_EQ(OptionZoomResetText(config), "Middle Mouse");
    ASSERT_EQ(config.key_bindings.size(), 1u);
    ASSERT_EQ(config.key_bindings.front().action, "cross");
}

TEST(MenuOption, CustomZoomResetBindingIsShownAndKeptUntilChanged) {
    Config config;
    config.key_bindings = {
        {"zoom_reset", {"F12", "Mouse4"}},
        {"r3",         {"B"}            }
    };
    ASSERT_EQ(OptionZoomResetChoice(config), 5);
    ASSERT_EQ(OptionZoomResetCount(config), 6);
    ASSERT_EQ(OptionZoomResetText(config), "F12, Mouse4");
    Config before = config;
    OptionSetZoomReset(config, 5);
    ASSERT_EQ(config, before);
    OptionSetZoomReset(config, 3);
    ASSERT_EQ(OptionZoomResetText(config), "Home");
    ASSERT_EQ(config.key_bindings[1], before.key_bindings[1]);
    OptionRestoreZoomReset(config, before);
    ASSERT_EQ(OptionZoomResetText(config), "F12, Mouse4");
    ASSERT_EQ(config.key_bindings.front(), before.key_bindings[1]);
}
