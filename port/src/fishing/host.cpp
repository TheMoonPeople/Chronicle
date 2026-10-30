#include "fishing/host.hpp"

// nlohmann/json before the game headers, whose one-letter macros it cannot stand.
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "editloop3.hpp"
#include "fish.hpp"
#include "fishing.hpp"
#include "fishing/framework.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "itemdata.hpp"
#include "data/data_read.hpp"
#include "platform/datapacks.hpp"
#include "platform/native_config.hpp"
#include "platform/paths.hpp"
#include "platform/vfs.hpp"
#include "save/extension.hpp"
#include "savedata.hpp"
#include "shop.hpp"

namespace dc::fishing {

using namespace dc::game;
using Json = nlohmann::json;
namespace native_config = platform::native_config;

namespace {

constexpr int kVanillaKinds = 18;
constexpr int kPrizeRows = 35;

// Retail's fish_info: each kind's model size, size range, points and liking for the thirteen baits in
// esa_info's order.
constexpr std::array<FishInfo, kVanillaKinds> kFishInfo = {
    {
     {17.0f, 10.0f, 20.0f, 20, 50, {1.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {19.5f, 8.0f, 16.0f, 10, 30, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f}},
     {25.5f, 8.0f, 19.0f, 8, 25, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.5f, 0.0f}},
     {35.4f, 12.0f, 24.0f, 30, 60, {0.5f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
     {28.0f, 8.0f, 16.0f, 10, 40, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 1.0f}},
     {21.0f, 10.0f, 16.0f, 200, 400, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {20.0f, 6.0f, 12.0f, 15, 40, {0.0f, 1.0f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f}},
     {20.0f, 6.0f, 10.0f, 20, 50, {0.0f, 0.5f, 1.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f}},
     {1.0f, 1.0f, 2.0f, 1, 2, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {21.0f, 8.0f, 16.0f, 100, 200, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
     {20.0f, 8.0f, 16.0f, 40, 80, {0.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 1.0f, 0.5f, 0.5f}},
     {20.0f, 6.0f, 12.0f, 20, 40, {1.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f}},
     {15.0f, 6.0f, 12.0f, 10, 30, {1.0f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f}},
     {21.0f, 10.0f, 20.0f, 35, 70, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f}},
     {21.0f, 8.0f, 16.0f, 15, 40, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 1.0f, 0.5f, 0.5f, 0.0f}},
     {20.0f, 8.0f, 20.0f, 20, 40, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f}},
     {20.0f, 8.0f, 14.0f, 20, 40, {0.0f, 0.0f, 0.0f, 0.2f, 0.2f, 0.2f, 0.0f, 0.2f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f}},
     {21.0f, 10.0f, 30.0f, 600, 1000, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f}},
     }
};

// Retail's GetFishFileName table.
constexpr std::array<const char *, kVanillaKinds> kFishModels = {
    "chara/f01a.chr", "chara/f02a.chr", "chara/f03a.chr", "chara/f04a.chr", "chara/f05a.chr", "chara/f06a.chr",
    "chara/f07a.chr", "chara/f08a.chr", "chara/f09a.chr", "chara/f10a.chr", "chara/f11a.chr", "chara/f12a.chr",
    "chara/f13a.chr", "chara/f14a.chr", "chara/f15a.chr", "chara/f16a.chr", "chara/f17a.chr", "chara/f18a.chr",
};

// The evaluation counted kinds 5 and 17 toward the Mardan Garayan.
constexpr bool IsVanillaMardanGarayan(int kind) {
    return kind == 5 || kind == 17;
}

std::string VanillaSpeciesId(int kind) {
    return std::format("vanilla:fish_{}", kind);
}

// The exchange's prizes as compiled, kept before a document replaces them.
const std::array<FISH_EXCHANGE_ITEM, kPrizeRows> &CompiledPrizes() {
    static const std::array<FISH_EXCHANGE_ITEM, kPrizeRows> prizes = std::to_array(exitemlst);
    return prizes;
}

// A float as the shortest decimal that reads back as it, so 35.4f is written 35.4.
Json Decimal(float value) {
    return std::stod(std::format("{}", value));
}

Json SpeciesDocument() {
    Json rows = Json::array();
    for (int kind = 0; kind < kVanillaKinds; kind++) {
        const FishInfo &info = kFishInfo[static_cast<std::size_t>(kind)];
        Json            liking = Json::array();
        for (std::size_t bait = 0; bait < esa_info.size(); bait++) {
            if (info.food_affinity[bait] > 0.0f) {
                liking.push_back({
                    {"bait",     item::VanillaItemId(esa_info[bait].item_no)},
                    {"affinity", Decimal(info.food_affinity[bait])          }
                });
            }
        }
        rows.push_back({
            {"fish_kind",      kind                                      },
            {"id",             VanillaSpeciesId(kind)                    },
            {"model",          kFishModels[static_cast<std::size_t>(kind)]},
            {"name_msg",       FishMsg[kind] + 30                        },
            {"model_size",     Decimal(info.model_size)                  },
            {"min_size",       Decimal(info.min_size)                    },
            {"max_size",       Decimal(info.max_size)                    },
            {"min_fp",         info.min_fp                               },
            {"max_fp",         info.max_fp                               },
            {"food_affinity",  std::move(liking)                         },
            {"mardan_garayan", IsVanillaMardanGarayan(kind)              },
        });
    }
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           "fish"                       },
        {"fish",           std::move(rows)              }
    };
}

Json Weights(std::initializer_list<std::pair<int, int>> fish) {
    Json list = Json::array();
    for (const auto &[kind, weight] : fish) {
        list.push_back({
            {"fish",   VanillaSpeciesId(kind)},
            {"weight", weight                }
        });
    }
    return list;
}

// CFish's odds and EdMoveChara's bait losses, one in five for Cross on a nibble and three in ten for a
// fish that lets go; the hook held half a second to a second, the port's own (retail's 21 to 50 ticks
// left too little time to react).
Json RetailBite() {
    return {
        {"stage",             "retail"                  },
        {"hook_ticks",        Json::array({30, 60})     },
        {"lose_bait_early",   0.2                       },
        {"lose_bait_escaped", 0.3                       }
    };
}

// EdMoveChara's battle: 120 ticks while the fish's model loads.
Json RetailFight() {
    return {
        {"stage", "retail"},
        {"ticks", 120     }
    };
}

Json RetailOutcome() {
    return {
        {"stage", "retail"}
    };
}

// FishingLoadFish's switch: the rare fish is kind 17 one time in five, else kind 5, one fish in 50 by
// day, 20 in the evening, 35 at night and 25 in the morning.
Json PlacesDocument() {
    const Json rare = {
        {"interval", {{"day", 50}, {"evening", 20}, {"night", 35}, {"morning", 25}}},
        {"fish",     Weights({{17, 1}, {5, 4}})                                     }
    };
    const auto place = [](int spot, std::string_view id, int count, bool draw, Json spawn) {
        spawn["stage"] = "retail";
        return Json{
            {"spot",             spot            },
            {"id",               id              },
            {"fish_count",       count           },
            {"draw_under_water", draw            },
            {"spawn",            std::move(spawn)},
            {"bite",             RetailBite()    },
            {"fight",            RetailFight()   },
            {"outcome",          RetailOutcome() }
        };
    };
    Json places = Json::array();
    places.push_back(place(0, "vanilla:norune", 4, true, {{"fish", Weights({{1, 1}, {2, 1}, {6, 1}, {7, 1}})}}));
    places.push_back(place(1, "vanilla:matataki_1", 5, true, {{"fish", Weights({{1, 35}, {4, 35}, {9, 10}, {10, 20}})}}));
    places.push_back(place(2, "vanilla:matataki_2", 5, true, {{"rare", rare}, {"fish", Weights({{2, 1}, {4, 1}, {6, 1}})}}));
    places.push_back(place(3, "vanilla:queens", 5, false, {{"fish", Weights({{0, 1}, {3, 1}, {11, 1}, {12, 1}, {13, 1}})}}));
    places.push_back(place(4, "vanilla:muska_lacka", 4, true, {{"rare", rare}, {"fish", Weights({{14, 40}, {15, 30}, {16, 30}})}}));
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           "fishing_places"             },
        {"places",         std::move(places)            }
    };
}

Json PrizesDocument() {
    Json rows = Json::array();
    for (const FISH_EXCHANGE_ITEM &prize : CompiledPrizes()) {
        rows.push_back({
            {"item",  item::VanillaItemId(prize.item_no)},
            {"price", prize.price                       }
        });
    }
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           "fish_prizes"                },
        {"prizes",         std::move(rows)              }
    };
}

/**
 * A document's rows, checked whole, and the fields it holds that this version does not read.
 */
template <class T>
struct Parsed {
    T                     value;
    std::set<std::string> unknown;
};

void NoteUnknown(const Json &row, std::initializer_list<std::string_view> known, std::set<std::string> &unknown) {
    for (const auto &[key, value] : row.items()) {
        if (std::ranges::find(known, key) == known.end()) {
            unknown.insert(key);
        }
    }
}

float Positive(const Json &row, const char *key, std::string_view where) {
    const float value = native_config::Float(native_config::At(row, key));
    if (!(value > 0.0f)) {
        throw std::runtime_error(std::format("{} {} must be above 0", where, key));
    }
    return value;
}

bool Bool(const Json &row, const char *key, std::string_view where) {
    const Json &value = native_config::At(row, key);
    if (!value.is_boolean()) {
        throw std::runtime_error(std::format("{} {} must be true or false", where, key));
    }
    return value.get<bool>();
}

// A path LoadFile can take, under the data: lower case, no way out of it.
bool ValidModel(std::string_view path) {
    return !path.empty() && !path.contains("..") && path.front() != '/' &&
           std::ranges::all_of(path, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '/' || c == '.'; });
}

// What a species row and a custom species' document share: the model, sizes, points and likings.
void ParseFish(Species &species, const Json &row, std::string_view where) {
    // What fight stages read about the species; tension its pull.
    if (const auto fight = row.find("fight"); fight != row.end() && !fight->is_null()) {
        if (!fight->is_object()) {
            throw std::runtime_error(std::format("{} fight must hold what fights read, such as pull", where));
        }
        if (fight->contains("pull")) {
            const float pull = native_config::Float((*fight)["pull"]);
            if (!(pull > 0.0f) || pull > 10.0f) {
                throw std::runtime_error(std::format("{} fight pull must be above 0 and at most 10", where));
            }
            species.pull = pull;
        }
    }
    if (const auto item = row.find("item"); item != row.end() && !item->is_null()) {
        if (!item->is_string() || item::Find(item->get_ref<const std::string &>()) == nullptr) {
            throw std::runtime_error(std::format("{} item must be the ID of an item, or null", where));
        }
        species.item = item->get<std::string>();
    }
    species.model = native_config::At(row, "model").get<std::string>();
    if (!ValidModel(species.model)) {
        throw std::runtime_error(std::format("{} model \"{}\" is not a path in the data", where, species.model));
    }
    species.model_size = Positive(row, "model_size", where);
    species.min_size = Positive(row, "min_size", where);
    species.max_size = Positive(row, "max_size", where);
    if (species.max_size <= species.min_size) {
        throw std::runtime_error(std::format("{} max_size must be above min_size", where));
    }
    species.min_fp = native_config::Int(row, "min_fp");
    species.max_fp = native_config::Int(row, "max_fp");
    if (species.min_fp < 0 || species.max_fp < 0) {
        throw std::runtime_error(std::format("{} points cannot be negative", where));
    }
    for (const Json &liking : native_config::Array(row, "food_affinity")) {
        const std::string bait = native_config::At(liking, "bait").get<std::string>();
        const item::Def  *def = item::Find(bait);
        if (def == nullptr || !def->Has<item::Bait>()) {
            throw std::runtime_error(std::format("{} likes {}, which is not a bait", where, bait));
        }
        // 2 makes a fish take the bait at every check: retail takes it at half its liking.
        const float affinity = native_config::Float(native_config::At(liking, "affinity"));
        if (affinity < 0.0f || affinity > 2.0f) {
            throw std::runtime_error(std::format("{} affinity for {} must be from 0 to 2", where, bait));
        }
        if (!species.food_affinity.emplace(bait, affinity).second) {
            throw std::runtime_error(std::format("{} names {} twice", where, bait));
        }
    }
}

Parsed<std::vector<Species>> ParseSpecies(const Json &document) {
    Parsed<std::vector<Species>> parsed;
    std::map<int, Species>       by_kind;
    for (const Json &row : native_config::Array(document, "fish")) {
        const int         kind = native_config::Int(row, "fish_kind");
        const std::string where = std::format("fish {}", kind);
        if (kind < 0 || kind >= kVanillaKinds) {
            throw std::runtime_error(std::format("{} is not a kind this table holds", where));
        }
        Species species{.id = native_config::At(row, "id").get<std::string>(), .kind = kind};
        if (species.id != VanillaSpeciesId(kind)) {
            throw std::runtime_error(std::format("{} is {}, not {}", where, VanillaSpeciesId(kind), species.id));
        }
        species.name_msg = native_config::Int(row, "name_msg");
        if (species.name_msg < 0 || species.name_msg > 0x7FFF) {
            throw std::runtime_error(std::format("{} name_msg {} is not a message number", where, species.name_msg));
        }
        ParseFish(species, row, where);
        species.mardan_garayan = Bool(row, "mardan_garayan", where);
        NoteUnknown(row, {"fish_kind", "id", "model", "name_msg", "model_size", "min_size", "max_size", "min_fp", "max_fp", "food_affinity", "mardan_garayan", "item", "fight"},
                    parsed.unknown);
        if (!by_kind.emplace(kind, std::move(species)).second) {
            throw std::runtime_error(std::format("{} is listed twice", where));
        }
    }
    for (int kind = 0; kind < kVanillaKinds; kind++) {
        if (!by_kind.contains(kind)) {
            throw std::runtime_error(std::format("fish {} is missing", kind));
        }
    }
    for (auto &[kind, species] : by_kind) {
        parsed.value.push_back(std::move(species));
    }
    return parsed;
}

void MakeStages(Place &place, const Json &row, std::string_view where) {
    try {
        place.spawn = MakeSpawnStage(native_config::At(row, "spawn"));
        place.bite = MakeBiteStage(native_config::At(row, "bite"));
        place.fight = MakeFightStage(native_config::At(row, "fight"));
        place.outcome = MakeOutcomeStage(native_config::At(row, "outcome"));
    } catch (const std::exception &error) {
        throw std::runtime_error(std::format("{} {}", where, error.what()));
    }
}

// A custom species, from its own document, gameplay/fishing/species/<namespace>.<name>.json.
Species ParseCustomSpecies(std::string_view file, const Json &document) {
    const Json &id = native_config::At(document, "id");
    if (!id.is_string() || !item::ValidId(id.get_ref<const std::string &>())) {
        throw std::runtime_error("id must be namespace:name in lower-case letters, digits and underscores");
    }
    Species species{.id = id.get<std::string>(), .kind = -1};
    if (species.id.starts_with("vanilla:")) {
        throw std::runtime_error("the vanilla namespace is retail's fish, in fish.json");
    }
    std::string expected = species.id;
    std::ranges::replace(expected, ':', '.');
    if (vfs::FoldKey(file) != vfs::FoldKey(expected + ".json")) {
        throw std::runtime_error(std::format("a species with id {} belongs in {}.json", species.id, expected));
    }
    ParseFish(species, document, species.id);
    species.mardan_garayan = document.contains("mardan_garayan") && Bool(document, "mardan_garayan", species.id);
    for (const auto &[language, entry] : native_config::At(document, "text").items()) {
        const Json &name = native_config::At(entry, "name");
        if (!name.is_string() || name.get_ref<const std::string &>().empty()) {
            throw std::runtime_error(std::format("text {}: name must be a string", language));
        }
        species.names[language] = name.get<std::string>();
    }
    if (species.names.empty()) {
        throw std::runtime_error("text needs a name in at least one language");
    }
    std::set<std::string> unknown;
    NoteUnknown(document, {"schema_version", "kind", "id", "model", "model_size", "min_size", "max_size", "min_fp", "max_fp", "food_affinity", "mardan_garayan", "text", "item", "fight"},
                unknown);
    if (!unknown.empty()) {
        std::string names;
        for (const std::string &name : unknown) {
            names += names.empty() ? name : ", " + name;
        }
        native_config::Report(std::format("gameplay/fishing/species/{}", file), "fields this version does not read: " + names);
    }
    return species;
}

// The custom species from the packs and the data, each named file once, in ID order.
std::vector<Species> LoadCustomSpecies() {
    std::map<std::string, std::string> files;
    for (const std::string &name : platform::datapacks::Members("gameplay/fishing/species")) {
        files.try_emplace(vfs::FoldKey(name), name);
    }
    if (data::DataReadIndexed()) {
        constexpr std::string_view kFolder = "gameplay/fishing/species/";
        for (const std::string &path : data::DataReadListFiles()) {
            if (path.size() > kFolder.size() && vfs::FoldKey(path.substr(0, kFolder.size())) == vfs::FoldKey(kFolder) &&
                path.find('/', kFolder.size()) == std::string::npos) {
                files.try_emplace(vfs::FoldKey(path.substr(kFolder.size())), path.substr(kFolder.size()));
            }
        }
    }
    std::map<std::string, Species> by_id;
    for (const std::string &name : std::views::values(files)) {
        native_config::Read(std::format("fishing/species/{}", name), "fish_species", [&](const Json &document) {
            Species           species = ParseCustomSpecies(name, document);
            const std::string id = species.id;
            by_id.insert_or_assign(id, std::move(species));
        });
    }
    std::vector<Species> species;
    for (auto &[id, one] : by_id) {
        species.push_back(std::move(one));
    }
    return species;
}

Parsed<std::vector<Place>> ParsePlaces(const Json &document) {
    const Json                 compiled = PlacesDocument();
    std::map<int, Place>       by_spot;
    Parsed<std::vector<Place>> parsed;
    for (const Json &row : native_config::Array(document, "places")) {
        const int         spot = native_config::Int(row, "spot");
        const std::string where = std::format("place {}", spot);
        const auto        vanilla = std::ranges::find_if(compiled["places"], [spot](const Json &place) { return place["spot"] == spot; });
        if (vanilla == compiled["places"].end()) {
            throw std::runtime_error(std::format("{} is not a spot the town scripts open", where));
        }
        Place place{.id = native_config::At(row, "id").get<std::string>(), .spot = spot};
        if (place.id != (*vanilla)["id"].get<std::string>()) {
            throw std::runtime_error(std::format("{} is {}, not {}", where, (*vanilla)["id"].get<std::string>(), place.id));
        }
        place.fish_count = native_config::Int(row, "fish_count");
        if (place.fish_count < 1 || place.fish_count > kMaxFish) {
            throw std::runtime_error(std::format("{} fish_count must be from 1 to {}", where, kMaxFish));
        }
        place.draw_under_water = Bool(row, "draw_under_water", where);
        MakeStages(place, row, where);
        NoteUnknown(row, {"spot", "id", "fish_count", "draw_under_water", "spawn", "bite", "fight", "outcome"}, parsed.unknown);
        if (!by_spot.emplace(spot, std::move(place)).second) {
            throw std::runtime_error(std::format("{} is listed twice", where));
        }
    }
    for (const Json &vanilla : compiled["places"]) {
        if (!by_spot.contains(vanilla["spot"].get<int>())) {
            throw std::runtime_error(std::format("place {} is missing", vanilla["spot"].get<int>()));
        }
    }
    for (auto &[spot, place] : by_spot) {
        parsed.value.push_back(std::move(place));
    }
    return parsed;
}

Parsed<std::array<FISH_EXCHANGE_ITEM, kPrizeRows>> ParsePrizes(const Json &document) {
    Parsed<std::array<FISH_EXCHANGE_ITEM, kPrizeRows>> parsed;
    const Json                                        &rows = native_config::Array(document, "prizes");
    // The exchange screen draws a fixed list.
    if (rows.size() != kPrizeRows) {
        throw std::runtime_error(std::format("the exchange holds {} prizes, not {}", kPrizeRows, rows.size()));
    }
    for (std::size_t index = 0; index < rows.size(); index++) {
        const Json       &row = rows[index];
        const std::string id = native_config::At(row, "item").get<std::string>();
        const item::Def  *def = item::Find(id);
        if (def == nullptr) {
            throw std::runtime_error(std::format("prize {} is {}, which no item is", index, id));
        }
        const int price = native_config::Int(row, "price");
        if (price < 0 || price > 0x7FFF) {
            throw std::runtime_error(std::format("prize {} price must be from 0 to 32767", index));
        }
        parsed.value[index] = {.item_no = static_cast<s16>(def->number), .price = static_cast<s16>(price)};
        NoteUnknown(row, {"item", "price"}, parsed.unknown);
    }
    return parsed;
}

/**
 * The first usable document at a path, or the compiled one; reports fields it does not read.
 */
template <class T>
T Read(std::string_view path, std::string_view kind, Parsed<T> (*parse)(const Json &), const Json &compiled) {
    std::optional<Parsed<T>> parsed;
    native_config::Read(path, kind, [&](const Json &document) { parsed = parse(document); });
    if (!parsed) {
        return parse(compiled).value;
    }
    if (!parsed->unknown.empty()) {
        std::string names;
        for (const std::string &name : parsed->unknown) {
            names += names.empty() ? name : ", " + name;
        }
        native_config::Report(std::format("gameplay/{}", path), "fields this version does not read: " + names);
    }
    return std::move(parsed->value);
}

std::map<int, FishInfo>                            g_info;
std::map<std::string, SpeciesRecord, std::less<>> g_records;
std::string                                       g_rod; // the rod chosen from the fishing menu

// Version 2 puts the chosen rod before the records.
constexpr std::uint32_t kRecordsVersion = 2;

std::optional<save::ExtensionChunk> RecordsChunk(CSaveData & /*image*/) {
    if (g_records.empty() && g_rod.empty()) {
        return std::nullopt;
    }
    save::ChunkWriter out;
    out.PutText(g_rod);
    out.Put(static_cast<std::uint32_t>(g_records.size()));
    for (const auto &[id, record] : g_records) {
        out.PutText(id);
        out.Put(static_cast<std::uint32_t>(record.caught));
        out.Put(static_cast<std::int32_t>(record.largest));
        out.Put(static_cast<std::uint16_t>(record.baits.size()));
        for (const std::string &bait : record.baits) {
            out.PutText(bait);
        }
    }
    save::ExtensionChunk chunk;
    chunk.tag = {'F', 'I', 'S', 'H'};
    chunk.version = kRecordsVersion;
    chunk.payload = out.Take();
    return chunk;
}

std::optional<std::map<std::string, SpeciesRecord, std::less<>>> DecodeRecords(std::span<const std::uint8_t> bytes, std::uint32_t version,
                                                                                 std::string &rod) {
    save::ChunkReader                                 reader(bytes);
    std::map<std::string, SpeciesRecord, std::less<>> records;
    std::uint32_t                                     count = 0;
    if ((version >= 2 && !reader.GetText(rod)) || !reader.Get(count)) {
        return std::nullopt;
    }
    for (std::uint32_t index = 0; index < count; index++) {
        std::string   id;
        std::uint32_t caught = 0;
        std::int32_t  largest = 0;
        std::uint16_t baits = 0;
        if (!reader.GetText(id) || !reader.Get(caught) || !reader.Get(largest) || !reader.Get(baits)) {
            return std::nullopt;
        }
        SpeciesRecord record{.caught = static_cast<int>(caught), .largest = largest};
        for (std::uint16_t bait = 0; bait < baits; bait++) {
            if (!reader.GetText(record.baits.emplace_back())) {
                return std::nullopt;
            }
        }
        records.insert_or_assign(std::move(id), std::move(record));
    }
    if (!reader.Done()) {
        return std::nullopt;
    }
    return records;
}

void RestoreRecords(const save::ExtensionChunk *chunk) {
    g_records.clear();
    g_rod.clear();
    if (chunk == nullptr) {
        return;
    }
    std::optional<std::map<std::string, SpeciesRecord, std::less<>>> records;
    if (chunk->version == 1 || chunk->version == kRecordsVersion) {
        records = DecodeRecords(chunk->payload, chunk->version, g_rod);
    }
    if (!records) {
        std::println(stderr, "fishing: the save's records of custom fish cannot be read (version {}); they are left out", chunk->version);
        g_rod.clear();
        return;
    }
    g_records = std::move(*records);
}
std::vector<BaitInfo>    g_baits;
const Place             *g_current = nullptr;
Place                    g_fallback; // retail's bite, fight and outcome for a spot no place is

TimeOfDay TimeFrom(int time_of_day) {
    switch (time_of_day) {
        case ED_TIME_EVENING:
            return TimeOfDay::Evening;
        case ED_TIME_NIGHT:
            return TimeOfDay::Night;
        case ED_TIME_MORNING:
            return TimeOfDay::Morning;
        default:
            return TimeOfDay::Day;
    }
}

} // namespace

void Load() {
    CompiledPrizes();
    RegisterBuiltinStages();
    RegisterQuestions();
    Clear();
    g_info.clear();
    g_current = nullptr;
    const Json retail = PlacesDocument()["places"][0];
    g_fallback = {.id = "vanilla:none",
                  .spot = -1,
                  .bite = MakeBiteStage(retail["bite"]),
                  .fight = MakeFightStage(retail["fight"]),
                  .outcome = MakeOutcomeStage(retail["outcome"])};
    g_baits.clear();
    const auto add_bait = [](const item::Def &def) {
        const item::Bait &bait = *def.Get<item::Bait>();
        g_baits.push_back({.item = def.number, .id = def.id, .radius = bait.radius, .lure = bait.lure, .default_affinity = bait.default_affinity});
    };
    for (const EsaInfo &bait : esa_info) {
        if (const item::Def *def = item::Find(bait.item_no); def != nullptr && def->Has<item::Bait>()) {
            add_bait(*def);
        }
    }
    for (const item::Def *def : item::With<item::Bait>()) {
        if (def->number >= item::kCustomFirst) {
            add_bait(*def);
        }
    }
    std::vector<Species> all = Read<std::vector<Species>>("fishing/fish.json", "fish", ParseSpecies, SpeciesDocument());
    int                  next_kind = kCustomKindFirst;
    for (Species &custom : LoadCustomSpecies()) {
        custom.kind = next_kind++;
        all.push_back(std::move(custom));
    }
    for (Species &species : all) {
        FishInfo info{.model_size = species.model_size,
                      .min_size = species.min_size,
                      .max_size = species.max_size,
                      .min_fp = species.min_fp,
                      .max_fp = species.max_fp};
        for (std::size_t bait = 0; bait < esa_info.size(); bait++) {
            info.food_affinity[bait] = species.Affinity(item::VanillaItemId(esa_info[bait].item_no));
        }
        g_info[species.kind] = info;
        RegisterSpecies(std::move(species));
    }
    // Places name species, so they come after.
    for (Place &place : Read<std::vector<Place>>("fishing/places.json", "fishing_places", ParsePlaces, PlacesDocument())) {
        RegisterPlace(std::move(place));
    }
    std::ranges::copy(Read<std::array<FISH_EXCHANGE_ITEM, kPrizeRows>>("fishing/prizes.json", "fish_prizes", ParsePrizes, PrizesDocument()),
                      exitemlst);
    save::ExtensionRegister("FISH", RecordsChunk, RestoreRecords);
}

bool Export(const std::filesystem::path &directory) {
    const std::array<std::pair<const char *, Json>, 3> documents = {
        {{"fish.json", SpeciesDocument()}, {"places.json", PlacesDocument()}, {"prizes.json", PrizesDocument()}}
    };
    for (const auto &[name, document] : documents) {
        const std::filesystem::path file = directory / "gameplay/fishing" / name;
        std::error_code             error;
        std::filesystem::create_directories(file.parent_path(), error);
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << document.dump(2) << '\n';
        if (error || !stream) {
            std::println(stderr, "cannot write {}", PathsDisplay(file));
            return false;
        }
    }
    return true;
}

const FishInfo *Info(int kind) {
    const auto found = g_info.find(kind);
    return found != g_info.end() ? &found->second : nullptr;
}

char *ModelPath(int kind) {
    const Species *species = SpeciesOfKind(kind);
    // LoadFile takes a mutable path but only reads it.
    return species != nullptr ? const_cast<char *>(species->model.c_str()) : nullptr;
}

int NameMessage(int kind) {
    const Species *species = SpeciesOfKind(kind);
    return species != nullptr ? species->name_msg : 0;
}

bool CountsAsMardanGarayan(int kind) {
    const Species *species = SpeciesOfKind(kind);
    return species != nullptr && species->mardan_garayan;
}

Spawning SpawnAt(int spot, int time_of_day) {
    const Place *place = PlaceAtSpot(spot);
    g_current = place;
    if (place == nullptr) {
        return {.kinds = std::vector<int>(5, -1)};
    }
    const SpawnContext context{.time = TimeFrom(time_of_day), .random = [] {
                                   return static_cast<double>(std::rand()) / (static_cast<double>(RAND_MAX) + 1.0);
                               }};
    Spawning spawning{.draw_under_water = place->draw_under_water};
    for (const Species *species : place->spawn->Spawn(*place, context)) {
        spawning.kinds.push_back(species != nullptr ? species->kind : -1);
    }
    return spawning;
}

const SpeciesRecord *RecordOf(std::string_view species) {
    const auto found = g_records.find(species);
    return found != g_records.end() ? &found->second : nullptr;
}

const std::map<std::string, SpeciesRecord, std::less<>> &AllRecords() {
    return g_records;
}

void RecordCatch(const Catch &fish) {
    if (fish.species == nullptr) {
        return;
    }
    SpeciesRecord &record = g_records[fish.species->id];
    record.caught++;
    record.largest = std::max(record.largest, fish.size);
    if (!fish.bait.empty() && std::ranges::find(record.baits, fish.bait) == record.baits.end()) {
        record.baits.emplace_back(fish.bait);
    }
}

const std::string &SpeciesName(const Species &species, std::string_view language) {
    static const std::string none;
    for (const std::string_view wanted : {language, std::string_view("en_gb"), std::string_view("en_us")}) {
        if (const auto found = species.names.find(std::string(wanted)); found != species.names.end()) {
            return found->second;
        }
    }
    return species.names.empty() ? none : species.names.begin()->second;
}

std::vector<int> RodsCarried() {
    std::vector<int> rods;
    if (SaveData == nullptr) {
        return rods;
    }
    const ITEM_PACK &pack = SaveData->GetDngStatus()->item_pack;
    const auto       add = [&rods](int number) {
        if (IsRod(number)) {
            const int rod = item::Definition(number);
            if (std::ranges::find(rods, rod) == rods.end()) {
                rods.push_back(rod);
            }
        }
    };
    std::ranges::for_each(pack.item, add);
    std::ranges::for_each(pack.quick_item_slot, add);
    return rods;
}

void ChooseRod(std::string id) {
    g_rod = std::move(id);
}

bool IsRod(int number) {
    const item::Def *def = number > 0 ? item::Find(item::Definition(number)) : nullptr;
    return def != nullptr && def->Has<item::Rod>();
}

void ChooseRodNumber(int number) {
    if (!IsRod(number)) {
        return;
    }
    const item::Def *rod = item::Find(item::Definition(number));
    ChooseRod(rod->number == ITEM_FISHING_ROD ? std::string() : rod->id);
}

const std::string &ChosenRod() {
    return g_rod;
}

item::Rod RodInUse() {
    const item::Def *chosen = g_rod.empty() ? nullptr : item::Find(g_rod);
    if (chosen != nullptr && chosen->Has<item::Rod>() && std::ranges::contains(RodsCarried(), chosen->number)) {
        return *chosen->Get<item::Rod>();
    }
    const item::Def *rod = item::Find(ITEM_FISHING_ROD);
    return rod != nullptr && rod->Has<item::Rod>() ? *rod->Get<item::Rod>() : item::Rod{};
}

TimeOfDay TimeOfDayNow() {
    return TimeFrom(EdGetTime(SaveData->GetNowTime()));
}

const Place &CurrentPlace() {
    return g_current != nullptr ? *g_current : g_fallback;
}

const BaitInfo *BaitAt(int index) {
    return index >= 0 && std::cmp_less(index, g_baits.size()) ? &g_baits[static_cast<std::size_t>(index)] : nullptr;
}

int BaitIndex(int item) {
    const int  number = item::Definition(item);
    const auto found = std::ranges::find(g_baits, number, &BaitInfo::item);
    return found != g_baits.end() ? static_cast<int>(found - g_baits.begin()) : -1;
}

std::vector<int> BaitItems() {
    std::vector<int> items;
    for (const BaitInfo &bait : g_baits) {
        items.push_back(bait.item);
    }
    return items;
}

} // namespace dc::fishing
