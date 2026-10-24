#include "menu_journal_chest.hpp"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "chests_data.hpp"
#include "gamepad.hpp"
#include "itemdata.hpp"
#include "localize.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "ui_framework.hpp"

#define PAD_GLYPH_UP "{up}"
#define PAD_GLYPH_DOWN "{down}"
#define PAD_GLYPH_CROSS "{cross}"
#define PAD_GLYPH_SQUARE "{square}"
#define PAD_GLYPH_CIRCLE "{circle}"

namespace JournalChest {

namespace {

constexpr int kLeftStartY = 156;
constexpr int kRowSpacing = 25;

int          s_selected_town = 0;
bool         s_filter_miracle_only = true;
UI::Cursor   s_cursor;
UI::Brackets s_brackets;

constexpr bool IsItemEden(uint16_t id) { return id == 180; }

constexpr bool IsItemGourd(uint16_t id) { return id == 182; }

constexpr bool IsItemPocket(uint16_t id) { return id == 199; }

constexpr bool IsItemDefense(uint16_t id) {
    return (id >= 177 && id <= 179) || (id >= 181 && id <= 184);
}

constexpr bool IsItemMiracle(uint16_t id) {
    return IsItemEden(id) || IsItemGourd(id) || IsItemDefense(id) || IsItemPocket(id);
}

struct TownStats {
    int collected_total = 0;
    int max_total = 0;
    int collected_miracle = 0;
    int max_miracle = 0;
    int collected_eden = 0;
    int max_eden = 0;
    int collected_gourd = 0;
    int max_gourd = 0;
    int collected_def = 0;
    int max_def = 0;
    int collected_pocket = 0;
    int max_pocket = 0;
    int collected_std = 0;
    int max_std = 0;

    void Accumulate(const TownStats &other) {
        collected_total += other.collected_total;
        max_total += other.max_total;
        collected_miracle += other.collected_miracle;
        max_miracle += other.max_miracle;
        collected_eden += other.collected_eden;
        max_eden += other.max_eden;
        collected_gourd += other.collected_gourd;
        max_gourd += other.max_gourd;
        collected_def += other.collected_def;
        max_def += other.max_def;
        collected_pocket += other.collected_pocket;
        max_pocket += other.max_pocket;
        collected_std += other.collected_std;
        max_std += other.max_std;
    }
};

TownStats GetTownStats(int town_index) {
    TownStats stats{};
    if (SaveData == nullptr || town_index < 0 || town_index >= TRACKER_TOWN_COUNT) {
        return stats;
    }

    const auto &town = TRACKER_TOWNS[town_index];
    for (int i = 0; i < town.total_chests; ++i) {
        const auto &chest = town.chests[i];
        const bool  opened = (SaveData->GetMapFlag(town.town_id, chest.flag_no) != 0);

        stats.max_total++;
        if (opened) {
            stats.collected_total++;
        }

        if (IsItemEden(chest.item_id)) {
            stats.max_eden++;
            if (opened) {
                stats.collected_eden++;
            }
        } else if (IsItemGourd(chest.item_id)) {
            stats.max_gourd++;
            if (opened) {
                stats.collected_gourd++;
            }
        } else if (IsItemDefense(chest.item_id)) {
            stats.max_def++;
            if (opened) {
                stats.collected_def++;
            }
        } else if (IsItemPocket(chest.item_id)) {
            stats.max_pocket++;
            if (opened) {
                stats.collected_pocket++;
            }
        } else {
            stats.max_std++;
            if (opened) {
                stats.collected_std++;
            }
        }

        if (IsItemMiracle(chest.item_id)) {
            stats.max_miracle++;
            if (opened) {
                stats.collected_miracle++;
            }
        }
    }
    return stats;
}

TownStats GetGlobalStats() {
    TownStats global{};
    for (int i = 0; i < TRACKER_TOWN_COUNT; ++i) {
        global.Accumulate(GetTownStats(i));
    }
    return global;
}

} // namespace

void Open() {
    s_selected_town = 0;
    s_cursor.SnapTo(48.0f, static_cast<float>(kLeftStartY - 2));
}

void HandlePointer(float x, float y) {
    if (x >= 84 && x < 248 && y >= kLeftStartY - 4 &&
        y < kLeftStartY - 4 + TRACKER_TOWN_COUNT * kRowSpacing) {
        const int town = static_cast<int>((y - (kLeftStartY - 4)) / kRowSpacing);
        if (town != s_selected_town) {
            s_selected_town = town;
            ComMenuSePlay(MENU_SOUND_CURSOR);
        }
    } else if (x >= 180 && x < 460 && y >= 100 && y < 128) {
        s_filter_miracle_only = !s_filter_miracle_only;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    }
}

bool HandleKey() {
    if (GamePad.Down(PAD_CIRCLE)) {
        ComMenuSePlay(MENU_SOUND_REFUSE);
        return true;
    }
    if (GamePad.Down(PAD_UP)) {
        if (--s_selected_town < 0) {
            s_selected_town = TRACKER_TOWN_COUNT - 1;
        }
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if (GamePad.Down(PAD_DOWN)) {
        if (++s_selected_town >= TRACKER_TOWN_COUNT) {
            s_selected_town = 0;
        }
        ComMenuSePlay(MENU_SOUND_CURSOR);
    }
    if (GamePad.Down(PAD_CROSS | PAD_SQUARE)) {
        s_filter_miracle_only = !s_filter_miracle_only;
        ComMenuSePlay(MENU_SOUND_CONFIRM);
    }
    return false;
}

void Draw() {
    constexpr float header_scale = 0.92f;
    constexpr float body_scale = 0.84f;

    constexpr int left_pane_x = 92;
    constexpr int divider_x = 258;
    constexpr int right_pane_x = 274;
    constexpr int icon_size = 20;              // 20px icon with 25px row stride leaves a clean 5px vertical gap
    constexpr int label_x = right_pane_x + 32; // 20px icon + 12px horizontal breathing room
    constexpr int count_x = 460;

    // 1. Weathered Paper Menu Window
    UI::TrackerWindow::DrawPaper(64, 91, 512, 262, 0x80);

    // 2. Parchment Scroll Title Banner crowning the top edge
    UI::TrackerWindow::DrawStoneBanner(
        UI::Rect(150, 74, 340, 28),
        0x80,
        0x80,
        LocalizeText("tracker.title", "JOURNAL: CHEST TRACKER"));

    // 3. 4px Recessed Groove Dividers
    UI::Divider::DrawVertical(divider_x, 132, 338, 0x80);
    UI::Divider::DrawHorizontal(84, divider_x - 10, 285, 0x80);
    UI::Divider::DrawHorizontal(right_pane_x, 542, 285, 0x80);

    MenuTextureReload(0x1A);

    // 4. Filter Mode Indicator
    const std::string mode_label = s_filter_miracle_only
                                       ? LocalizeText("tracker.filter.stat_boosts_only", "< STAT BOOSTS ONLY >")
                                       : LocalizeText("tracker.filter.all_chests", "< ALL TOWN CHESTS >");
    const u32         mode_color = s_filter_miracle_only ? UI::ColorYellow : UI::ColorWhite;
    UI::Label(mode_label, mode_color, UI::Alignment::Center, true, body_scale).Draw(320, 108, 0x80);

    // 5. Left Pane: Settlements List
    UI::Label(LocalizeText("tracker.settlements", "SETTLEMENTS"),
              UI::ColorYellow, UI::Alignment::Left, true, header_scale)
        .Draw(left_pane_x, 132, 0x80);

    for (int i = 0; i < TRACKER_TOWN_COUNT; i++) {
        const int  row_y = kLeftStartY + (i * kRowSpacing);
        const bool selected = (i == s_selected_town);
        const u32  color = selected ? UI::ColorYellow : UI::ColorWhite;
        const int  row_alpha = selected ? 0x80 : 0x58;

        std::string town_name = LocalizeText("tracker.town." + std::to_string(i), TRACKER_TOWNS[i].name);
        UI::Label(town_name, color, UI::Alignment::Left, true, body_scale).Draw(left_pane_x, row_y, row_alpha);
    }

    // 6. Left Pane: World Total
    //    "World Total" raised to Y = 287 (aligned with "Area Total"), leaving [ 19 / 84 ] at Y = 316
    const TownStats g_stats = GetGlobalStats();
    const int       g_collected = s_filter_miracle_only ? g_stats.collected_miracle : g_stats.collected_total;
    const int       g_max = s_filter_miracle_only ? g_stats.max_miracle : g_stats.max_total;

    UI::Label(LocalizeText("tracker.world_total", "World Total"),
              UI::ColorGold, UI::Alignment::Left, true, header_scale)
        .Draw(left_pane_x, 287, 0x80);
    UI::DrawCounter(left_pane_x, 316, g_collected, g_max, body_scale);

    // 7. Right Pane: Selected Settlement Breakdown
    std::string cur_town_name = LocalizeText("tracker.town." + std::to_string(s_selected_town),
                                             TRACKER_TOWNS[s_selected_town].name);
    std::string right_title = LocalizeFormat(LocalizeText("tracker.breakdown", "%1 Breakdown"), {cur_town_name});
    UI::Label(right_title, UI::ColorYellow, UI::Alignment::Left, true, header_scale).Draw(right_pane_x, 132, 0x80);

    const TownStats cur_stats = GetTownStats(s_selected_town);

    static int    s_item_anim_tick = 0;
    constexpr int defense_treat_items[6] = {
        ITEM_FLUFFY_DOUGHNUT, ITEM_FISH_CANDY, ITEM_GRASS_CAKE,
        ITEM_WITCH_PARFAIT, ITEM_SCORPION_JERKY, ITEM_CARROT_COOKIE};
    const int cur_defense_item = defense_treat_items[(s_item_anim_tick / 60) % 6];
    s_item_anim_tick = (s_item_anim_tick + 1) % 36000;

    struct MetricRow {
        std::string label;
        int         item_no;
        int         collected;
        int         max_val;
    };

    std::vector<MetricRow> breakdown = {
        {LocalizeText("tracker.item.eden",    "Fruit of Eden"),  ITEM_FRUIT_OF_EDEN, cur_stats.collected_eden,   cur_stats.max_eden  },
        {LocalizeText("tracker.item.gourd",   "Gourds"),         ITEM_GOURD,         cur_stats.collected_gourd,  cur_stats.max_gourd },
        {LocalizeText("tracker.item.defense", "Defense Treats"), cur_defense_item,   cur_stats.collected_def,    cur_stats.max_def   },
        {LocalizeText("tracker.item.pocket",  "Pockets"),        ITEM_POCKET,        cur_stats.collected_pocket, cur_stats.max_pocket}
    };

    if (!s_filter_miracle_only) {
        breakdown.push_back({LocalizeText("tracker.item.standard", "Standard Items"), ITEM_TREASURE_KEY, cur_stats.collected_std, cur_stats.max_std});
    }

    for (size_t r = 0; r < breakdown.size(); ++r) {
        const int  y = kLeftStartY + (static_cast<int>(r) * kRowSpacing);
        const bool icon_drawn = UI::ItemIcon::Draw(breakdown[r].item_no, right_pane_x + 2, y - 1, icon_size, 0x80);

        MenuTextureReload(0x1A);
        const int text_x = icon_drawn ? label_x : right_pane_x;
        UI::Label(breakdown[r].label, UI::ColorWhite, UI::Alignment::Left, true, body_scale).Draw(text_x, y, 0x80);
        UI::DrawCounter(count_x, y, breakdown[r].collected, breakdown[r].max_val, body_scale);
    }

    // 8. Right Pane: Area Total & Recessed Shade-Mask Progress Bar
    //    Both "Area Total" and its [ 19 / 20 ] counter aligned at Y = 287
    const int town_cur_collected = s_filter_miracle_only ? cur_stats.collected_miracle : cur_stats.collected_total;
    const int town_cur_max = s_filter_miracle_only ? cur_stats.max_miracle : cur_stats.max_total;

    UI::Label(LocalizeText("tracker.area_total", "Area Total"),
              UI::ColorYellow, UI::Alignment::Left, true, header_scale)
        .Draw(right_pane_x, 287, 0x80);
    UI::DrawCounter(count_x, 287, town_cur_collected, town_cur_max, body_scale);

    const float progress_ratio = (town_cur_max > 0)
                                     ? static_cast<float>(town_cur_collected) / static_cast<float>(town_cur_max)
                                     : 0.0f;
    const int   pct = (town_cur_max > 0) ? (town_cur_collected * 100) / town_cur_max : 0;
    std::string pct_buf = LocalizeFormat(LocalizeText("tracker.progress", "Progress: %1%"), {std::to_string(pct)});

    UI::ProgressBarStyle bar_style{};
    bar_style.text_scale = 0.76f;
    UI::ProgressBar::Draw(UI::Rect(right_pane_x, 314, 268, 22), progress_ratio, pct_buf, bar_style, 0x80);

    // 9. Bottom Controls Bar & Selection Cursor
    MenuTextureReload(0x1A);
    std::string controls_str = LocalizeText(
        "tracker.controls",
        PAD_GLYPH_UP " / " PAD_GLYPH_DOWN " Select   " PAD_GLYPH_CROSS " / " PAD_GLYPH_SQUARE " Filter   " PAD_GLYPH_CIRCLE " Return");
    UI::Label(controls_str, UI::ColorWhite, UI::Alignment::Center).Draw(320, 418, 0x80);

    const int sel_y = kLeftStartY + (s_selected_town * kRowSpacing);
    s_brackets.Draw(UI::Rect(left_pane_x - 4, sel_y - 1, 152, 22), 0x80);

    s_cursor.SetTarget(48.0f, static_cast<float>(sel_y - 2));
    s_cursor.Draw(0x80);
}

void AppendStrings(std::vector<std::pair<std::string, std::string>> &out) {
    const std::pair<const char *, const char *> tracker_entries[] = {
        {"tracker.title",                   "JOURNAL: CHEST TRACKER"                                                                                                     },
        {"tracker.filter.stat_boosts_only", "< STAT BOOSTS ONLY >"                                                                                                       },
        {"tracker.filter.all_chests",       "< ALL TOWN CHESTS >"                                                                                                        },
        {"tracker.settlements",             "SETTLEMENTS"                                                                                                                },
        {"tracker.world_total",             "World Total"                                                                                                                },
        {"tracker.breakdown",               "%1 Breakdown"                                                                                                               },
        {"tracker.item.eden",               "Fruit of Eden"                                                                                                              },
        {"tracker.item.gourd",              "Gourds"                                                                                                                     },
        {"tracker.item.defense",            "Defense Treats"                                                                                                             },
        {"tracker.item.pocket",             "Pockets"                                                                                                                    },
        {"tracker.item.standard",           "Standard Items"                                                                                                             },
        {"tracker.progress",                "Progress: %1%"                                                                                                              },
        {"tracker.area_total",              "Area Total"                                                                                                                 },
        {"tracker.controls",                PAD_GLYPH_UP " / " PAD_GLYPH_DOWN " Select   " PAD_GLYPH_CROSS " / " PAD_GLYPH_SQUARE " Filter   " PAD_GLYPH_CIRCLE " Return"},
    };
    for (const auto &[key, val] : tracker_entries) {
        out.emplace_back(key, val);
    }
    for (size_t i = 0; i < std::size(TRACKER_TOWNS); i++) {
        out.emplace_back("tracker.town." + std::to_string(i), TRACKER_TOWNS[i].name);
    }
}

} // namespace JournalChest
