#include "ui/journal_chest.hpp"

#include <array>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "gamepad.hpp"
#include "itemdata.hpp"
#include "localize/localize.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "ui/chests_data.hpp"
#include "ui/framework.hpp"

namespace dc::ui::journal_chest {

using namespace dc::game;
using namespace dc::localize;

namespace {

constexpr int kLeftStartY = 156;
constexpr int kRowSpacing = 25;

constexpr const char *kControlsText = "{up} / {down} Select   {cross} / {square} Filter   {circle} Return";

constexpr std::array<int, 6> kDefenseTreatItems = {ITEM_FLUFFY_DOUGHNUT, ITEM_FISH_CANDY, ITEM_GRASS_CAKE,
                                                   ITEM_WITCH_PARFAIT, ITEM_SCORPION_JERKY, ITEM_CARROT_COOKIE};

int          s_selected_town = 0;
bool         s_filter_miracle_only = true;
ui::Cursor   s_cursor;
ui::Brackets s_brackets;
int          s_item_anim_tick = 0;

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
    constexpr int icon_size = 20;
    constexpr int label_x = right_pane_x + 32;
    constexpr int count_x = 460;

    ui::TrackerWindow::DrawPaper(64, 91, 512, 262, 0x80);

    ui::TrackerWindow::DrawStoneBanner(
        ui::Rect(150, 74, 340, 28),
        0x80,
        0x80,
        LocalizeText("tracker.title", "JOURNAL: CHEST TRACKER"));

    ui::Divider::DrawVertical(divider_x, 132, 338, 0x80);
    ui::Divider::DrawHorizontal(84, divider_x - 10, 285, 0x80);
    ui::Divider::DrawHorizontal(right_pane_x, 542, 285, 0x80);

    MenuTextureReload(0x1A);

    const std::string mode_label = s_filter_miracle_only
                                       ? LocalizeText("tracker.filter.stat_boosts_only", "< STAT BOOSTS ONLY >")
                                       : LocalizeText("tracker.filter.all_chests", "< ALL TOWN CHESTS >");
    const u32         mode_color = s_filter_miracle_only ? ui::ColorYellow : ui::ColorWhite;
    ui::Label(mode_label, mode_color, ui::Alignment::Center, true, body_scale).Draw(320, 108, 0x80);

    ui::Label(LocalizeText("tracker.settlements", "SETTLEMENTS"),
              ui::ColorYellow, ui::Alignment::Left, true, header_scale)
        .Draw(left_pane_x, 132, 0x80);

    for (int i = 0; i < TRACKER_TOWN_COUNT; i++) {
        const int  row_y = kLeftStartY + (i * kRowSpacing);
        const bool selected = (i == s_selected_town);
        const u32  color = selected ? ui::ColorYellow : ui::ColorWhite;
        const int  row_alpha = selected ? 0x80 : 0x58;

        const std::string town_name = LocalizeText("tracker.town." + std::to_string(i), TRACKER_TOWNS[i].name);
        ui::Label(town_name, color, ui::Alignment::Left, true, body_scale).Draw(left_pane_x, row_y, row_alpha);
    }

    const TownStats world_stats = GetGlobalStats();
    const int       world_collected = s_filter_miracle_only ? world_stats.collected_miracle : world_stats.collected_total;
    const int       world_max = s_filter_miracle_only ? world_stats.max_miracle : world_stats.max_total;

    ui::Label(LocalizeText("tracker.world_total", "World Total"),
              ui::ColorGold, ui::Alignment::Left, true, header_scale)
        .Draw(left_pane_x, 287, 0x80);
    ui::DrawCounter(left_pane_x, 316, world_collected, world_max, body_scale);

    const std::string cur_town_name = LocalizeText("tracker.town." + std::to_string(s_selected_town),
                                                   TRACKER_TOWNS[s_selected_town].name);
    const std::string right_title = LocalizeFormat(LocalizeText("tracker.breakdown", "%1 Breakdown"), {cur_town_name});
    ui::Label(right_title, ui::ColorYellow, ui::Alignment::Left, true, header_scale).Draw(right_pane_x, 132, 0x80);

    const TownStats cur_stats = GetTownStats(s_selected_town);

    const int cur_defense_item = kDefenseTreatItems[(s_item_anim_tick / 60) % kDefenseTreatItems.size()];
    s_item_anim_tick = (s_item_anim_tick + 1) % 36000;

    struct MetricRow {
        std::string label;
        int         item_no;
        int         collected;
        int         max_val;
    };

    std::vector<MetricRow> breakdown = {
        {.label = LocalizeText("tracker.item.eden",    "Fruit of Eden"),
         .item_no = ITEM_FRUIT_OF_EDEN,
         .collected = cur_stats.collected_eden,
         .max_val = cur_stats.max_eden  },
        {.label = LocalizeText("tracker.item.gourd",   "Gourds"),
         .item_no = ITEM_GOURD,
         .collected = cur_stats.collected_gourd,
         .max_val = cur_stats.max_gourd },
        {.label = LocalizeText("tracker.item.defense", "Defense Treats"),
         .item_no = cur_defense_item,
         .collected = cur_stats.collected_def,
         .max_val = cur_stats.max_def   },
        {.label = LocalizeText("tracker.item.pocket",  "Pockets"),
         .item_no = ITEM_POCKET,
         .collected = cur_stats.collected_pocket,
         .max_val = cur_stats.max_pocket},
    };

    if (!s_filter_miracle_only) {
        breakdown.push_back({.label = LocalizeText("tracker.item.standard", "Standard Items"),
                             .item_no = ITEM_TREASURE_KEY,
                             .collected = cur_stats.collected_std,
                             .max_val = cur_stats.max_std});
    }

    int row_y = kLeftStartY;
    for (const MetricRow &row : breakdown) {
        const bool icon_drawn = ui::ItemIcon::Draw(row.item_no, right_pane_x + 2, row_y - 1, icon_size, 0x80);

        MenuTextureReload(0x1A);
        const int text_x = icon_drawn ? label_x : right_pane_x;
        ui::Label(row.label, ui::ColorWhite, ui::Alignment::Left, true, body_scale).Draw(text_x, row_y, 0x80);
        ui::DrawCounter(count_x, row_y, row.collected, row.max_val, body_scale);
        row_y += kRowSpacing;
    }

    const int town_cur_collected = s_filter_miracle_only ? cur_stats.collected_miracle : cur_stats.collected_total;
    const int town_cur_max = s_filter_miracle_only ? cur_stats.max_miracle : cur_stats.max_total;

    ui::Label(LocalizeText("tracker.area_total", "Area Total"),
              ui::ColorYellow, ui::Alignment::Left, true, header_scale)
        .Draw(right_pane_x, 287, 0x80);
    ui::DrawCounter(count_x, 287, town_cur_collected, town_cur_max, body_scale);

    const float       progress_ratio = (town_cur_max > 0)
                                           ? static_cast<float>(town_cur_collected) / static_cast<float>(town_cur_max)
                                           : 0.0f;
    const int         percent = (town_cur_max > 0) ? (town_cur_collected * 100) / town_cur_max : 0;
    const std::string progress_text =
        LocalizeFormat(LocalizeText("tracker.progress", "Progress: %1%"), {std::to_string(percent)});

    ui::ProgressBarStyle bar_style{};
    bar_style.text_scale = 0.76f;
    ui::ProgressBar::Draw(ui::Rect(right_pane_x, 314, 268, 22), progress_ratio, progress_text, bar_style, 0x80);

    MenuTextureReload(0x1A);
    const std::string controls_text = LocalizeText("tracker.controls", kControlsText);
    ui::Label(controls_text, ui::ColorWhite, ui::Alignment::Center).Draw(320, 418, 0x80);

    const int sel_y = kLeftStartY + (s_selected_town * kRowSpacing);
    s_brackets.Draw(ui::Rect(left_pane_x - 4, sel_y - 1, 152, 22), 0x80);

    s_cursor.SetTarget(48.0f, static_cast<float>(sel_y - 2));
    s_cursor.Draw(0x80);
}

void AppendStrings(std::vector<std::pair<std::string, std::string>> &out) {
    const std::array<std::pair<const char *, const char *>, 14> tracker_entries = {
        {
         {"tracker.title", "JOURNAL: CHEST TRACKER"},
         {"tracker.filter.stat_boosts_only", "< STAT BOOSTS ONLY >"},
         {"tracker.filter.all_chests", "< ALL TOWN CHESTS >"},
         {"tracker.settlements", "SETTLEMENTS"},
         {"tracker.world_total", "World Total"},
         {"tracker.breakdown", "%1 Breakdown"},
         {"tracker.item.eden", "Fruit of Eden"},
         {"tracker.item.gourd", "Gourds"},
         {"tracker.item.defense", "Defense Treats"},
         {"tracker.item.pocket", "Pockets"},
         {"tracker.item.standard", "Standard Items"},
         {"tracker.progress", "Progress: %1%"},
         {"tracker.area_total", "Area Total"},
         {"tracker.controls", kControlsText},
         }
    };
    for (const auto &[key, val] : tracker_entries) {
        out.emplace_back(key, val);
    }
    for (size_t i = 0; i < std::size(TRACKER_TOWNS); ++i) {
        out.emplace_back("tracker.town." + std::to_string(i), TRACKER_TOWNS[i].name);
    }
}

} // namespace dc::ui::journal_chest
