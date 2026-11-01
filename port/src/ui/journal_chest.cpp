#include "ui/journal_chest.hpp"

#include <array>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "itemdata.hpp"
#include "localize/localize.hpp"
#include "savedata.hpp"
#include "ui/chests_data.hpp"
#include "ui/framework.hpp"

namespace dc::ui::journal_chest {

using namespace dc::game;
using namespace dc::localize;

namespace {

constexpr const char *kControlsText = "{up} / {down} Select   {cross} / {square} Filter   {circle} Return";

constexpr std::array<int, 6> kDefenseTreatItems = {ITEM_FLUFFY_DOUGHNUT, ITEM_FISH_CANDY, ITEM_GRASS_CAKE,
                                                   ITEM_WITCH_PARFAIT, ITEM_SCORPION_JERKY, ITEM_CARROT_COOKIE};

ui::Tracker s_tracker;
int         s_item_anim_tick = 0;

constexpr bool IsItemEden(uint16_t id) { return id == ITEM_FRUIT_OF_EDEN; }

constexpr bool IsItemGourd(uint16_t id) { return id == ITEM_GOURD; }

constexpr bool IsItemPocket(uint16_t id) { return id == ITEM_POCKET; }

// The allies' defense treats, from the Fluffy Doughnut to the Carrot Cookie.
constexpr bool IsItemDefense(uint16_t id) {
    return id >= ITEM_FLUFFY_DOUGHNUT && id <= ITEM_CARROT_COOKIE;
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

// The rows of a town's breakdown: each stat boost, and the standard items when not filtered.
std::vector<ui::TrackerRow> TownRows(int town, bool filtered) {
    const TownStats stats = GetTownStats(town);

    const int defense_item = kDefenseTreatItems[(s_item_anim_tick / 60) % kDefenseTreatItems.size()];
    s_item_anim_tick = (s_item_anim_tick + 1) % 36000;

    std::vector<ui::TrackerRow> rows = {
        {.label = LocalizeText("tracker.item.eden",    "Fruit of Eden"),
         .item_no = ITEM_FRUIT_OF_EDEN,
         .tally = {.done = stats.collected_eden, .total = stats.max_eden}    },
        {.label = LocalizeText("tracker.item.gourd",   "Gourds"),
         .item_no = ITEM_GOURD,
         .tally = {.done = stats.collected_gourd, .total = stats.max_gourd}  },
        {.label = LocalizeText("tracker.item.defense", "Defense Treats"),
         .item_no = defense_item,
         .tally = {.done = stats.collected_def, .total = stats.max_def}      },
        {.label = LocalizeText("tracker.item.pocket",  "Pockets"),
         .item_no = ITEM_POCKET,
         .tally = {.done = stats.collected_pocket, .total = stats.max_pocket}},
    };

    if (!filtered) {
        rows.push_back({
            .label = LocalizeText("tracker.item.standard", "Standard Items"),
            .item_no = ITEM_TREASURE_KEY,
            .tally = {.done = stats.collected_std, .total = stats.max_std}
        });
    }

    return rows;
}

ui::TrackerTally TownTally(int town, bool filtered) {
    const TownStats stats = GetTownStats(town);
    return filtered ? ui::TrackerTally{.done = stats.collected_miracle, .total = stats.max_miracle}
                    : ui::TrackerTally{.done = stats.collected_total, .total = stats.max_total};
}

std::string TownName(int town) {
    return LocalizeText("tracker.town." + std::to_string(town), TRACKER_TOWNS[town].name);
}

} // namespace

void Open() {
    s_tracker.Open({.title = LocalizeText("tracker.title", "JOURNAL: CHEST TRACKER"),
                    .groups_heading = LocalizeText("tracker.settlements", "SETTLEMENTS"),
                    .all_label = LocalizeText("tracker.world_total", "World Total"),
                    .group_label = LocalizeText("tracker.area_total", "Area Total"),
                    .breakdown = LocalizeText("tracker.breakdown", "%1 Breakdown"),
                    .progress = LocalizeText("tracker.progress", "Progress: %1%"),
                    .controls = LocalizeText("tracker.controls", kControlsText),
                    .filter_on = LocalizeText("tracker.filter.stat_boosts_only", "< STAT BOOSTS ONLY >"),
                    .filter_off = LocalizeText("tracker.filter.all_chests", "< ALL TOWN CHESTS >"),
                    .group_count = [] { return TRACKER_TOWN_COUNT; },
                    .group_name = TownName,
                    .rows = TownRows,
                    .tally = TownTally});
}

void HandlePointer(float x, float y) {
    s_tracker.HandlePointer(x, y);
}

bool HandleKey() {
    return s_tracker.HandleKey();
}

void Draw() {
    s_tracker.Draw();
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
