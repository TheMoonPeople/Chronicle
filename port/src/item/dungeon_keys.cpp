#include "item/dungeon_keys.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <set>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "platform/native_config.hpp"
#include "platform/paths.hpp"

namespace dc::item {

using Json = nlohmann::json;
namespace native_config = platform::native_config;

namespace {

constexpr const char *kPath = "items/dungeon_keys.json";
constexpr const char *kKind = "dungeon_keys";
constexpr int         kDungeons = 7;

// What one dungeon's floors take, by item ID.
struct Row {
    int                      dungeon = 0;
    std::vector<std::string> locked_door;
    std::vector<std::string> floor_exit;
    std::vector<std::string> back_floor;
};

// The items retail's dungeon scripts take (dun/script/dNN/event.stb): each locked room door's key, each
// floor exit's item (the Wise Owl Forest's floors pick one of three, CDungeonMap::gate_key_no), and the
// back floor entrance's item.
const std::vector<Row> &Compiled() {
    static const std::vector<Row> rows = {
        {0, {"vanilla:bone_key"},      {"vanilla:dran_s_crest"},                                                {"vanilla:tram_oil"}                            },
        {1, {"vanilla:moustache_key"}, {"vanilla:shiny_stone", "vanilla:red_berry", "vanilla:pointy_chestnut"}, {"vanilla:sun_dew"}                             },
        {2, {"vanilla:shipcabin_key"}, {"vanilla:hook"},                                                        {"vanilla:flapping_fish", "vanilla:rotten_fish"}},
        {3, {"vanilla:stone_key"},     {"vanilla:king_s_slate"},                                                {"vanilla:secret_path_key"}                     },
        {4, {"vanilla:handle"},        {"vanilla:gun_powder"},                                                  {"vanilla:bravery_launch"}                      },
        {5, {"vanilla:pitchdark_key"}, {"vanilla:clock_hands"},                                                 {"vanilla:flapping_duster"}                     },
        {6, {"vanilla:silver_key"},    {"vanilla:black_knight_crest"},                                          {"vanilla:crystal_eyeball"}                     },
    };
    return rows;
}

constexpr std::array<std::pair<const char *, DungeonKey::Opens>, 3> kLists = {
    {{"locked_door", DungeonKey::Opens::LockedDoor},
     {"floor_exit", DungeonKey::Opens::FloorExit},
     {"back_floor", DungeonKey::Opens::BackFloor}}
};

std::vector<std::string> &List(Row &row, DungeonKey::Opens opens) {
    switch (opens) {
        case DungeonKey::Opens::LockedDoor:
            return row.locked_door;
        case DungeonKey::Opens::FloorExit:
            return row.floor_exit;
        default:
            return row.back_floor;
    }
}

Json Document(const std::vector<Row> &rows) {
    Json dungeons = Json::array();
    for (Row row : rows) {
        Json entry = {
            {"dungeon", row.dungeon}
        };
        for (const auto &[name, opens] : kLists) {
            entry[name] = List(row, opens);
        }
        dungeons.push_back(std::move(entry));
    }
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           kKind                        },
        {"dungeons",       std::move(dungeons)          }
    };
}

// Every dungeon once, each item once in the whole document; a list left out is empty.
std::map<std::string, DungeonKey, std::less<>> Parse(const Json &document, std::set<std::string> &unknown) {
    std::map<std::string, DungeonKey, std::less<>> keys;
    std::set<int>                                  dungeons;
    for (const Json &entry : native_config::Array(document, "dungeons")) {
        const int dungeon = native_config::Int(entry, "dungeon");
        if (dungeon < 0 || dungeon >= kDungeons) {
            throw std::runtime_error(std::format("dungeon {} is not one from 0 to {}", dungeon, kDungeons - 1));
        }
        if (!dungeons.insert(dungeon).second) {
            throw std::runtime_error(std::format("dungeon {} is listed twice", dungeon));
        }
        for (const auto &[name, opens] : kLists) {
            const auto found = entry.find(name);
            if (found == entry.end()) {
                continue;
            }
            if (!found->is_array()) {
                throw std::runtime_error(std::format("dungeon {} {} must be a list of item IDs", dungeon, name));
            }
            for (const Json &id : *found) {
                if (!id.is_string() || !ValidId(id.get_ref<const std::string &>())) {
                    throw std::runtime_error(std::format("dungeon {} {} holds {}, which is not an item ID", dungeon, name, id.dump()));
                }
                if (!keys.emplace(id.get<std::string>(), DungeonKey{.opens = opens, .dungeon = dungeon}).second) {
                    throw std::runtime_error(std::format("{} is listed twice", id.get<std::string>()));
                }
            }
        }
        for (const auto &[key, value] : entry.items()) {
            if (key != "dungeon" && key != "locked_door" && key != "floor_exit" && key != "back_floor") {
                unknown.insert(key);
            }
        }
    }
    return keys;
}

} // namespace

std::map<std::string, DungeonKey, std::less<>> LoadDungeonKeys() {
    std::optional<std::map<std::string, DungeonKey, std::less<>>> keys;
    std::set<std::string>                                         unknown;
    native_config::Read(kPath, kKind, [&](const Json &document) {
        unknown.clear();
        keys = Parse(document, unknown);
    });
    if (!keys) {
        keys = Parse(Document(Compiled()), unknown);
    }
    if (!unknown.empty()) {
        std::string names;
        for (const std::string &name : unknown) {
            names += names.empty() ? name : ", " + name;
        }
        native_config::Report(std::format("gameplay/{}", kPath), "fields this version does not read: " + names);
    }
    return std::move(*keys);
}

bool ExportDungeonKeys(const std::filesystem::path &directory) {
    const std::filesystem::path file = directory / "gameplay" / kPath;
    std::error_code             error;
    std::filesystem::create_directories(file.parent_path(), error);
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << Document(Compiled()).dump(2) << '\n';
    if (error || !stream) {
        std::println(stderr, "cannot write {}", PathsDisplay(file));
        return false;
    }
    return true;
}

} // namespace dc::item
