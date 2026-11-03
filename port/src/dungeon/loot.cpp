// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include "dungeon/loot.hpp"

#include <nlohmann/json.hpp>

#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <set>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include "dungeonparts.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "platform/native_config.hpp"
#include "platform/paths.hpp"

namespace dc::dungeon {

using namespace dc::game;
using Json = nlohmann::json;
namespace native_config = platform::native_config;

namespace {

constexpr const char *kPath = "dungeon/loot.json";
constexpr const char *kPatchPath = "dungeon/loot.patch.json";
constexpr const char *kKind = "dungeon_loot";
constexpr const char *kPatchKind = "dungeon_loot_patch";
constexpr int         kDungeons = 7;
// Retail's lists: the first dungeon's floors have four rows, every other three, each ended by floor -1.
constexpr int kMostRows = 4;
constexpr int kRateCount = 385;

std::array<DungeonLoot, kDungeons> g_loot;

// The tables as compiled: dungeonparts.cpp's lists, seven dungeons and then their back floors.
std::array<DungeonLoot, kDungeons> Compiled() {
    std::array<DungeonLoot, kDungeons> loot;
    for (int dungeon = 0; dungeon < kDungeons; dungeon++) {
        DungeonLoot  &out = loot[dungeon];
        std::set<int> drawn;
        for (int back = 0; back < 2; back++) {
            const ITEM_PUT_SET *rows = ItemPutListPtr[dungeon + (back * kDungeons)];
            for (int row = 0; row < kMostRows && rows[row].floor != -1; row++) {
                ChestList list{.floor = rows[row].floor};
                list.items.assign(rows[row].item, rows[row].item + rows[row].item_count);
                drawn.insert(list.items.begin(), list.items.end());
                out.chests[back].push_back(std::move(list));
            }
            const PIERO_ITEM_SET *clown = PieroItemListPtr[dungeon + (back * kDungeons)];
            for (int offer = 0; offer < 2; offer++) {
                out.clown[back].lower[offer].assign(clown[0].item[offer], clown[0].item[offer] + clown[0].count[offer]);
                out.clown[back].upper[offer].assign(clown[1].item[offer], clown[1].item[offer] + clown[1].count[offer]);
                drawn.insert(out.clown[back].lower[offer].begin(), out.clown[back].lower[offer].end());
                drawn.insert(out.clown[back].upper[offer].begin(), out.clown[back].upper[offer].end());
            }
        }
        // Only an item a list holds is ever drawn, so the rest of retail's 385 rates are left out.
        for (int item : drawn) {
            if (item > 0 && item <= kRateCount && ItemSetRateTbl[dungeon][item - 1] != 0) {
                out.turned_down[item] = ItemSetRateTbl[dungeon][item - 1];
            }
        }
    }
    return loot;
}

Json Ids(const std::vector<int> &items) {
    Json ids = Json::array();
    for (int item : items) {
        const item::Def *def = item::Find(item);
        ids.push_back(def != nullptr ? def->id : item::VanillaItemId(item));
    }
    return ids;
}

Json FloorName(int floor) {
    return floor == kLowerFloors ? Json("lower") : floor == kUpperFloors ? Json("upper")
                                                                         : Json(floor);
}

Json ChestsJson(const std::vector<ChestList> &lists) {
    Json out = Json::array();
    for (const ChestList &list : lists) {
        out.push_back({
            {"floor", FloorName(list.floor)},
            {"items", Ids(list.items)      }
        });
    }
    return out;
}

Json ClownJson(const ClownLists &clown) {
    return {
        {"lower", {Ids(clown.lower[0]), Ids(clown.lower[1])}},
        {"upper", {Ids(clown.upper[0]), Ids(clown.upper[1])}}
    };
}

Json Document(const std::array<DungeonLoot, kDungeons> &loot) {
    Json dungeons = Json::array();
    for (int dungeon = 0; dungeon < kDungeons; dungeon++) {
        const DungeonLoot &one = loot[dungeon];
        Json               turned_down = Json::object();
        for (const auto &[item, chance] : one.turned_down) {
            const item::Def *def = item::Find(item);
            turned_down[def != nullptr ? def->id : item::VanillaItemId(item)] = chance;
        }
        dungeons.push_back({
            {"dungeon",           dungeon                  },
            {"turned_down",       std::move(turned_down)   },
            {"chests",            ChestsJson(one.chests[0])},
            {"back_floor_chests", ChestsJson(one.chests[1])},
            {"clown",             ClownJson(one.clown[0])  },
            {"back_floor_clown",  ClownJson(one.clown[1])  }
        });
    }
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           kKind                        },
        {"dungeons",       std::move(dungeons)          }
    };
}

// Reads documents' item IDs, noting the ones no loaded item has; those are left out.
struct Reader {
    std::set<std::string> missing;
    std::set<std::string> unknown;

    int Item(const Json &id) {
        if (!id.is_string() || !item::ValidId(id.get_ref<const std::string &>())) {
            throw std::runtime_error(std::format("{} is not an item ID", id.dump()));
        }
        const item::Def *def = item::Find(id.get_ref<const std::string &>());
        if (def == nullptr) {
            missing.insert(id.get<std::string>());
            return -1;
        }
        return def->number;
    }

    std::vector<int> Items(const Json &ids, std::string_view where) {
        if (!ids.is_array()) {
            throw std::runtime_error(std::format("{} must be a list of item IDs", where));
        }
        std::vector<int> items;
        for (const Json &id : ids) {
            if (const int item = Item(id); item >= 0) {
                items.push_back(item);
            }
        }
        return items;
    }

    std::vector<ChestList> Chests(const Json &lists, std::string_view where) {
        if (!lists.is_array()) {
            throw std::runtime_error(std::format("{} must be a list of box lists", where));
        }
        std::vector<ChestList> out;
        for (const Json &list : lists) {
            const Json &floor = native_config::At(list, "floor");
            ChestList   chest;
            if (floor == "lower") {
                chest.floor = kLowerFloors;
            } else if (floor == "upper") {
                chest.floor = kUpperFloors;
            } else if (floor.is_number_integer() && floor.get<int>() >= 1 && floor.get<int>() < kUpperFloors) {
                chest.floor = floor.get<int>();
            } else {
                throw std::runtime_error(std::format("{} floor {} is not \"lower\", \"upper\" or a floor from 1", where, floor.dump()));
            }
            chest.items = Items(native_config::At(list, "items"), where);
            if (chest.items.size() > 128) {
                throw std::runtime_error(std::format("{} holds more than 128 items", where));
            }
            out.push_back(std::move(chest));
        }
        if (out.size() > kMostRows) {
            throw std::runtime_error(std::format("{} has more than {} lists", where, kMostRows));
        }
        return out;
    }

    ClownLists Clown(const Json &clown, std::string_view where) {
        ClownLists out;
        for (const auto &[half, lists] : {
                 std::pair{"lower", &out.lower},
                 std::pair{"upper", &out.upper}
        }) {
            const Json &both = native_config::At(clown, half);
            if (!both.is_array() || both.size() != 2) {
                throw std::runtime_error(std::format("{} {} must be the two offers' lists", where, half));
            }
            for (int offer = 0; offer < 2; offer++) {
                (*lists)[offer] = Items(both[offer], where);
                if ((*lists)[offer].empty() || (*lists)[offer].size() > 64) {
                    throw std::runtime_error(std::format("{} {} offer {} must hold from 1 to 64 items", where, half, offer + 1));
                }
            }
        }
        return out;
    }

    void TurnedDown(const Json &chances, std::map<int, int> &out, std::string_view where) {
        if (!chances.is_object()) {
            throw std::runtime_error(std::format("{} turned_down must map item IDs to chances", where));
        }
        for (const auto &[id, chance] : chances.items()) {
            if (!chance.is_number_integer() || chance.get<int>() < 0 || chance.get<int>() > 100) {
                throw std::runtime_error(std::format("{} turned_down {} must be from 0 to 100", where, id));
            }
            if (const int item = Item(Json(id)); item >= 0) {
                out[item] = chance.get<int>();
            }
        }
    }

    // A dungeon's row: every field for a full document, those it names for a patch.
    void Row(const Json &row, DungeonLoot &loot, bool every) {
        const int         dungeon = native_config::Int(row, "dungeon");
        const std::string where = std::format("dungeon {}", dungeon);
        const auto        field = [&](const char *name) -> const Json        *{
            if (const auto found = row.find(name); found != row.end()) {
                return &*found;
            }
            if (every) {
                throw std::runtime_error(std::format("{} is missing {}", where, name));
            }
            return nullptr;
        };
        if (const Json *chances = field("turned_down")) {
            if (every) {
                loot.turned_down.clear();
            }
            TurnedDown(*chances, loot.turned_down, where);
        }
        if (const Json *lists = field("chests")) {
            loot.chests[0] = Chests(*lists, where);
        }
        if (const Json *lists = field("back_floor_chests")) {
            loot.chests[1] = Chests(*lists, where);
        }
        if (const Json *clown = field("clown")) {
            loot.clown[0] = Clown(*clown, where);
        }
        if (const Json *clown = field("back_floor_clown")) {
            loot.clown[1] = Clown(*clown, where);
        }
        for (const auto &[key, value] : row.items()) {
            if (key != "dungeon" && key != "turned_down" && key != "chests" && key != "back_floor_chests" && key != "clown" &&
                key != "back_floor_clown") {
                unknown.insert(key);
            }
        }
    }

    // The rows of a document by dungeon, each once.
    static std::map<int, const Json *> Rows(const Json &document) {
        std::map<int, const Json *> rows;
        for (const Json &row : native_config::Array(document, "dungeons")) {
            const int dungeon = native_config::Int(row, "dungeon");
            if (dungeon < 0 || dungeon >= kDungeons) {
                throw std::runtime_error(std::format("dungeon {} is not one from 0 to {}", dungeon, kDungeons - 1));
            }
            if (!rows.emplace(dungeon, &row).second) {
                throw std::runtime_error(std::format("dungeon {} is listed twice", dungeon));
            }
        }
        return rows;
    }
};

void Report(const char *path, const Reader &reader) {
    for (const std::string &id : reader.missing) {
        native_config::Report(std::format("gameplay/{}", path), std::format("no loaded item is {}; left out", id));
    }
    if (!reader.unknown.empty()) {
        std::string names;
        for (const std::string &name : reader.unknown) {
            names += names.empty() ? name : ", " + name;
        }
        native_config::Report(std::format("gameplay/{}", path), "fields this version does not read: " + names);
    }
}

} // namespace

void LoadLoot() {
    std::optional<std::array<DungeonLoot, kDungeons>> loot;
    Reader                                            reader;
    native_config::Read(kPath, kKind, [&](const Json &document) {
        Reader                             attempt;
        std::array<DungeonLoot, kDungeons> parsed;
        const auto                         rows = Reader::Rows(document);
        for (int dungeon = 0; dungeon < kDungeons; dungeon++) {
            const auto found = rows.find(dungeon);
            if (found == rows.end()) {
                throw std::runtime_error(std::format("dungeon {} is missing", dungeon));
            }
            attempt.Row(*found->second, parsed[dungeon], true);
        }
        loot = std::move(parsed);
        reader = std::move(attempt);
    });
    g_loot = loot ? std::move(*loot) : Compiled();
    Report(kPath, reader);

    // Every pack's patch over that, the lowest priority's first; one is applied whole or not at all.
    native_config::ReadEach(kPatchPath, kPatchKind, [&](const Json &document) {
        Reader     attempt;
        auto       patched = g_loot;
        const auto rows = Reader::Rows(document);
        for (const auto &[dungeon, row] : rows) {
            attempt.Row(*row, patched[dungeon], false);
        }
        g_loot = std::move(patched);
        Report(kPatchPath, attempt);
    });
}

const DungeonLoot &Loot(int dungeon) {
    return g_loot[dungeon >= 0 && dungeon < kDungeons ? dungeon : 0];
}

int TurnedDown(int dungeon, int item) {
    const DungeonLoot &loot = Loot(dungeon);
    const auto         found = loot.turned_down.find(item);
    return found != loot.turned_down.end() ? found->second : 0;
}

bool ExportLoot(const std::filesystem::path &directory) {
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

} // namespace dc::dungeon
