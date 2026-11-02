#include "georama/tables.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <deque>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "dungeonparts.hpp"
#include "editpartsdata.hpp"
#include "platform/native_config.hpp"
#include "platform/paths.hpp"

namespace dc::georama {

using namespace dc::game;
using namespace dc::platform;

namespace {

using Json = native_config::Json;

constexpr int kTowns = 6;
constexpr int kParts = 24; /**< Parts a town's table holds; its 25th record is left empty. */
constexpr int kElementSlots = 6;
constexpr int kModels = 4;
constexpr int kElements = 100;
constexpr int kFirstElementAtla = 40; /**< An Atla below this is a part, from it an element. */
constexpr int kAtlaRows = 100;        /**< Rows of a dungeon's Atla the save holds (atra_registry). */
constexpr int kAtlaPerFloor = 8;      /**< Atla a floor holds (atra_grid). */
constexpr int kAtlaFloors = 40;       /**< Floors the save holds Atla for. */

// The towns as EditPartsData numbers them, and the dungeon that hands out each one's Atla.
constexpr std::array<std::string_view, kTowns> kTownNames = {"norune", "matataki", "queens", "muska_lacka", "yellow_drops", "dark_heaven_castle"};
constexpr std::array<std::string_view, kTowns> kDungeonNames = {"divine_beast_cave", "wise_owl_forest", "shipwreck",
                                                                "sun_and_moon_temple", "moon_sea", "gallery_of_time"};

// The tables as compiled, kept before a document replaces them.
struct Compiled {
    std::array<EDIT_PARTS_DATA, kTowns>                          parts;
    std::array<std::array<EDIT_ELEMENT_ATRA, kElements>, kTowns> elements;
    std::array<std::vector<ATRA_APPEAR>, kTowns>                 atla;
};

const Compiled &CompiledTables() {
    static const Compiled compiled = [] {
        Compiled tables;
        std::copy(std::begin(EditPartsData), std::end(EditPartsData), tables.parts.begin());
        for (int town = 0; town < kTowns; town++) {
            std::copy(std::begin(EditElementData[town]), std::end(EditElementData[town]), tables.elements[town].begin());
            for (const ATRA_APPEAR *row = AtraAppearData[town]; row->id != -1; row++) {
                tables.atla[town].push_back(*row);
            }
        }
        return tables;
    }();
    return compiled;
}

// Where the tables the game reads are kept once a document fills them: the model names a document's parts
// give, and each dungeon's Atla, ended by -1 as the game reads them.
std::deque<std::string>                      g_names;
std::array<std::vector<ATRA_APPEAR>, kTowns> g_atla;

int Index(std::string_view name, const std::array<std::string_view, kTowns> &names, const char *what) {
    const auto found = std::ranges::find(names, name);
    if (found == names.end()) {
        throw std::runtime_error(std::format("{} is not a {}", name, what));
    }
    return static_cast<int>(found - names.begin());
}

std::string String(const Json &object, const char *key) {
    const Json &value = native_config::At(object, key);
    if (!value.is_string()) {
        throw std::runtime_error(std::format("{} is not a string", key));
    }
    return value.get<std::string>();
}

int Optional(const Json &object, const char *key, int otherwise) {
    return object.contains(key) ? native_config::Int(object, key) : otherwise;
}

void Range(int value, int low, int high, const char *what) {
    if (value < low || value > high) {
        throw std::runtime_error(std::format("{} {} is outside {} to {}", what, value, low, high));
    }
}

// ---- parts.json ----

bool PartUsed(const EDIT_PARTS_ATRA &part) {
    return part.max != 0 || part.tex_no >= 0 || part.msg_no >= 0;
}

// The game reads every model name, an empty one as "", as the compiled table gives them.
char g_no_model[] = "";

EDIT_PARTS_ATRA EmptyPart() {
    EDIT_PARTS_ATRA part{.max = 0, .stackable = 0, .tex_no = -1, .msg_no = -1, .kind = 0, .elements = {}};
    for (EDIT_CHIP_ATTACH_DATA &element : part.elements) {
        element = {
            .id = -1, .required_element = -1, .names = {g_no_model, g_no_model, g_no_model, g_no_model},
                    .npc_no = -1
        };
    }
    return part;
}

Json PartsDocument() {
    const Compiled &compiled = CompiledTables();
    Json            towns = Json::array();
    for (int town = 0; town < kTowns; town++) {
        Json parts = Json::array();
        for (int number = 0; number < kParts; number++) {
            const EDIT_PARTS_ATRA &part = compiled.parts[town].parts[number];
            if (!PartUsed(part)) {
                continue;
            }
            // The element slots up to the last one in use, each one's model names up to its last.
            Json elements = Json::array();
            int  slots = kElementSlots;
            while (slots > 0 && part.elements[slots - 1].id == -1) {
                slots--;
            }
            for (int slot = 0; slot < slots; slot++) {
                const EDIT_CHIP_ATTACH_DATA &element = part.elements[slot];
                Json                         models = Json::array();
                int                          names = kModels;
                while (names > 0 && (element.names[names - 1] == nullptr || *element.names[names - 1] == '\0')) {
                    names--;
                }
                for (int name = 0; name < names; name++) {
                    models.push_back(element.names[name] != nullptr ? element.names[name] : "");
                }
                elements.push_back({
                    {"id",       element.id              },
                    {"requires", element.required_element},
                    {"models",   models                  },
                    {"villager", element.npc_no          },
                });
            }
            parts.push_back({
                {"part",         number        },
                {"max",          part.max      },
                {"stackable",    part.stackable},
                {"picture",      part.tex_no   },
                {"name_message", part.msg_no   },
                {"kind",         part.kind     },
                {"elements",     elements      },
            });
        }
        Json elements = Json::array();
        for (int number = 0; number < kElements; number++) {
            const EDIT_ELEMENT_ATRA &element = compiled.elements[town][number];
            if (element.tex_no < 0 && element.msg_no < 0) {
                continue;
            }
            elements.push_back({
                {"element",      number        },
                {"picture",      element.tex_no},
                {"name_message", element.msg_no},
                {"unk_08",       element.unk_08},
            });
        }
        towns.push_back({
            {"town",     kTownNames[town]},
            {"parts",    parts           },
            {"elements", elements        },
        });
    }
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           "georama_parts"              },
        {"towns",          towns                        },
    };
}

// A town's tables as a document gives them; the model names are pointed at once they are stored to last.
using ModelNames = std::array<std::array<std::array<std::string, kModels>, kElementSlots>, kParts>;

struct TownParts {
    int                                      town = 0;
    EDIT_PARTS_DATA                          parts{};
    std::array<EDIT_ELEMENT_ATRA, kElements> elements{};
    ModelNames                               models;
};

std::vector<TownParts> ParseParts(const Json &document) {
    std::vector<TownParts>   towns;
    std::array<bool, kTowns> seen{};
    for (const Json &entry : native_config::Array(document, "towns")) {
        TownParts &town = towns.emplace_back();
        town.town = Index(String(entry, "town"), kTownNames, "town");
        if (std::exchange(seen[town.town], true)) {
            throw std::runtime_error(std::format("{} is given twice", kTownNames[town.town]));
        }
        // The 25th record stays as compiled, zeros the game never reads as a part.
        town.parts = CompiledTables().parts[town.town];
        std::fill_n(town.parts.parts, kParts, EmptyPart());
        std::ranges::fill(town.elements, EDIT_ELEMENT_ATRA{.tex_no = -1, .msg_no = -1, .unk_08 = 0});
        for (const Json &row : native_config::Array(entry, "parts")) {
            const int number = native_config::Int(row, "part");
            Range(number, 0, kParts - 1, "part");
            EDIT_PARTS_ATRA &part = town.parts.parts[number];
            if (PartUsed(part)) {
                throw std::runtime_error(std::format("part {} of {} is given twice", number, kTownNames[town.town]));
            }
            part.max = native_config::Int(row, "max");
            Range(part.max, 0, 0x7FFF, "max");
            part.stackable = native_config::Int(row, "stackable");
            part.tex_no = native_config::Int(row, "picture");
            part.msg_no = native_config::Int(row, "name_message");
            part.kind = native_config::Int(row, "kind");
            const Json &elements = row.contains("elements") ? native_config::Array(row, "elements") : Json::array();
            if (elements.size() > kElementSlots) {
                throw std::runtime_error(std::format("part {} has more than {} elements", number, kElementSlots));
            }
            for (std::size_t slot = 0; slot < elements.size(); slot++) {
                EDIT_CHIP_ATTACH_DATA &element = part.elements[slot];
                element.id = native_config::Int(elements[slot], "id");
                element.required_element = Optional(elements[slot], "requires", -1);
                Range(element.required_element, -1, kElementSlots - 1, "requires");
                element.npc_no = Optional(elements[slot], "villager", -1);
                const Json &models = elements[slot].contains("models") ? native_config::Array(elements[slot], "models") : Json::array();
                if (models.size() > kModels) {
                    throw std::runtime_error(std::format("part {} has an element with more than {} models", number, kModels));
                }
                for (std::size_t name = 0; name < models.size(); name++) {
                    if (!models[name].is_string()) {
                        throw std::runtime_error("a model is not a string");
                    }
                    town.models[number][slot][name] = models[name].get<std::string>();
                }
            }
        }
        for (const Json &row : native_config::Array(entry, "elements")) {
            const int number = native_config::Int(row, "element");
            Range(number, 0, kElements - 1, "element");
            town.elements[number] = {
                .tex_no = native_config::Int(row, "picture"),
                .msg_no = native_config::Int(row, "name_message"),
                .unk_08 = Optional(row, "unk_08", 0),
            };
        }
    }
    return towns;
}

// ---- atla.json ----

Json AtlaDocument() {
    const Compiled &compiled = CompiledTables();
    Json            dungeons = Json::array();
    for (int dungeon = 0; dungeon < kTowns; dungeon++) {
        Json atla = Json::array();
        for (const ATRA_APPEAR &row : compiled.atla[dungeon]) {
            Json entry = row.id < kFirstElementAtla ? Json{
                                                          {"part", row.id}
            }
                                                    : Json{{"element", row.id - kFirstElementAtla}};
            entry["floor"] = row.floor == -1 ? Json("first_half") : row.floor == -2 ? Json("second_half")
                                                                                    : Json(row.floor);
            entry["count"] = row.count;
            atla.push_back(entry);
        }
        dungeons.push_back({
            {"dungeon", kDungeonNames[dungeon]},
            {"atla",    atla                  },
        });
    }
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           "georama_atla"               },
        {"dungeons",       dungeons                     },
    };
}

// Whether BtAtraListMake can place a dungeon's random Atla: every one of a half's on a floor of that half
// that takes Atla and has room beside the fixed ones, or it would look for room forever.
void CheckRoom(int dungeon, const std::vector<ATRA_APPEAR> &rows) {
    const int                    center = AtraCenterFloor(dungeon);
    const int                    floors = AtraFloorCount(dungeon);
    std::array<int, kAtlaFloors> fixed{};
    std::array<int, 2>           wanted{};
    for (const ATRA_APPEAR &row : rows) {
        if (row.floor > 0) {
            fixed[row.floor - 1]++;
        } else {
            wanted[row.floor == -1 ? 0 : 1] += row.count;
        }
    }
    // The halves' floors as BtAtraListMake picks them, counted from 0.
    const std::array<std::array<int, 2>, 2> halves = {
        {{0, center - 1}, {center, floors - 1}}
    };
    for (int half = 0; half < 2; half++) {
        int room = 0;
        for (int floor = halves[half][0]; floor < halves[half][1]; floor++) {
            if (AtraFloorOpen(dungeon, floor + 1)) {
                room += std::max(0, kAtlaPerFloor - fixed[floor]);
            }
        }
        if (wanted[half] > room) {
            throw std::runtime_error(std::format("{} asks for {} Atla on its {} half, which has room for {}", kDungeonNames[dungeon],
                                                 wanted[half], half == 0 ? "first" : "second", room));
        }
    }
}

struct DungeonAtla {
    int                      dungeon = 0;
    std::vector<ATRA_APPEAR> rows;
};

std::vector<DungeonAtla> ParseAtla(const Json &document) {
    std::vector<DungeonAtla> dungeons;
    std::array<bool, kTowns> seen{};
    for (const Json &entry : native_config::Array(document, "dungeons")) {
        DungeonAtla &dungeon = dungeons.emplace_back();
        dungeon.dungeon = Index(String(entry, "dungeon"), kDungeonNames, "dungeon");
        if (std::exchange(seen[dungeon.dungeon], true)) {
            throw std::runtime_error(std::format("{} is given twice", kDungeonNames[dungeon.dungeon]));
        }
        const Json &atla = native_config::Array(entry, "atla");
        if (atla.size() > kAtlaRows) {
            throw std::runtime_error(std::format("{} has {} Atla, more than the save holds ({})", kDungeonNames[dungeon.dungeon], atla.size(), kAtlaRows));
        }
        for (const Json &row : atla) {
            ATRA_APPEAR appear{};
            if (row.contains("part") == row.contains("element")) {
                throw std::runtime_error("an Atla names a part or an element, not both or neither");
            }
            if (row.contains("part")) {
                appear.id = native_config::Int(row, "part");
                Range(appear.id, 0, kFirstElementAtla - 1, "part");
            } else {
                const int element = native_config::Int(row, "element");
                Range(element, 0, kElements - kFirstElementAtla - 1, "element");
                appear.id = kFirstElementAtla + element;
            }
            const Json &floor = native_config::At(row, "floor");
            if (floor == "first_half") {
                appear.floor = -1;
            } else if (floor == "second_half") {
                appear.floor = -2;
            } else {
                appear.floor = native_config::Int(row, "floor");
                Range(appear.floor, 1, AtraFloorCount(dungeon.dungeon), "floor");
            }
            appear.count = Optional(row, "count", 1);
            Range(appear.count, 1, kAtlaRows, "count");
            dungeon.rows.push_back(appear);
        }
        CheckRoom(dungeon.dungeon, dungeon.rows);
    }
    return dungeons;
}

bool Write(const std::filesystem::path &file, const Json &document) {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream << document.dump(2) << '\n';
    if (error || !stream) {
        std::println(stderr, "cannot write {}", PathsDisplay(file));
        return false;
    }
    return true;
}

} // namespace

void Load() {
    const Compiled &compiled = CompiledTables();
    std::ranges::copy(compiled.parts, std::begin(EditPartsData));
    for (int town = 0; town < kTowns; town++) {
        std::ranges::copy(compiled.elements[town], std::begin(EditElementData[town]));
        g_atla[town] = compiled.atla[town];
    }
    g_names.clear();

    native_config::Read("georama/parts.json", "georama_parts", [&](const Json &document) {
        for (TownParts &town : ParseParts(document)) {
            // A deque keeps its strings where they are as it grows, so the table can point into them.
            for (int part = 0; part < kParts; part++) {
                for (int slot = 0; slot < kElementSlots; slot++) {
                    for (int name = 0; name < kModels; name++) {
                        if (!town.models[part][slot][name].empty()) {
                            town.parts.parts[part].elements[slot].names[name] = g_names.emplace_back(std::move(town.models[part][slot][name])).data();
                        }
                    }
                }
            }
            EditPartsData[town.town] = town.parts;
            std::ranges::copy(town.elements, std::begin(EditElementData[town.town]));
        }
    });
    native_config::Read("georama/atla.json", "georama_atla", [&](const Json &document) {
        for (DungeonAtla &dungeon : ParseAtla(document)) {
            g_atla[dungeon.dungeon] = std::move(dungeon.rows);
        }
    });

    for (int dungeon = 0; dungeon < kTowns; dungeon++) {
        g_atla[dungeon].push_back({.id = -1, .floor = 0, .count = 0});
        AtraAppearData[dungeon] = g_atla[dungeon].data();
    }
}

bool Export(const std::filesystem::path &directory) {
    return Write(directory / "gameplay/georama/parts.json", PartsDocument()) && Write(directory / "gameplay/georama/atla.json", AtlaDocument());
}

} // namespace dc::georama
