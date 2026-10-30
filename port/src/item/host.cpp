#include "item/host.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <format>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "btitem.hpp"
#include "data/data_read.hpp"
#include "dngstatusdata.hpp"
#include "fishing.hpp"
#include "item/framework.hpp"
#include "itemdata.hpp"
#include "localize/localize.hpp"
#include "platform/datapacks.hpp"
#include "platform/native_config.hpp"
#include "platform/paths.hpp"
#include "savedata.hpp"
#include "shop.hpp"
#include "stockitem.hpp"
#include "text/gametext.hpp"

namespace dc::item {

using namespace dc::game;
using namespace dc::platform;

namespace {

using Json = native_config::Json;

enum class Representation : uint8_t {
    Signed,
    Unsigned,
    Float,
};

/**
 * One field of a table's rows, by its retail name.
 */
struct Field {
    const char    *name;
    std::size_t    offset;
    std::size_t    size;  /**< Bytes of one element. */
    std::size_t    count; /**< Elements of an array; 1 for a single value. */
    Representation representation;
    bool           fixed; /**< Decides where a save's copies sit, so a document cannot change it. */
};

template <class Member>
constexpr Field MakeField(const char *name, std::size_t offset, bool fixed) {
    using Element = std::remove_all_extents_t<Member>;
    // char's signedness differs between platforms; the documents read it as unsigned everywhere.
    const Representation representation = std::is_floating_point_v<Element>                             ? Representation::Float
                                          : std::is_signed_v<Element> && !std::is_same_v<Element, char> ? Representation::Signed
                                                                                                        : Representation::Unsigned;
    return {name, offset, sizeof(Element), sizeof(Member) / sizeof(Element), representation, fixed};
}

#define ITEM_FIELD(T, member) MakeField<decltype(T::member)>(#member, offsetof(T, member), false)
#define ITEM_FIXED(T, member) MakeField<decltype(T::member)>(#member, offsetof(T, member), true)

constexpr std::array kCommonFields = {
    ITEM_FIXED(COM_ITEM_INFO, kind),
    ITEM_FIELD(COM_ITEM_INFO, unk_01),
    ITEM_FIXED(COM_ITEM_INFO, index),
    ITEM_FIELD(COM_ITEM_INFO, icon_index),
    ITEM_FIELD(COM_ITEM_INFO, msg),
};

constexpr std::array kItemFields = {
    ITEM_FIELD(ITEM_DATA, sort_key),
    ITEM_FIELD(ITEM_DATA, unk_02),
    ITEM_FIELD(ITEM_DATA, use_flags),
    ITEM_FIELD(ITEM_DATA, kind_flags),
    ITEM_FIELD(ITEM_DATA, vol),
    ITEM_FIELD(ITEM_DATA, vol_range),
    ITEM_FIELD(ITEM_DATA, unk_0E),
    ITEM_FIELD(ITEM_DATA, stack_kind),
};

// An attachment's item_no is its row's.
constexpr std::array kAttachFields = {
    ITEM_FIELD(ATTACH_DATA, sphere_weapon_no),
    ITEM_FIELD(ATTACH_DATA, sphere_flags),
    ITEM_FIELD(ATTACH_DATA, sphere_level),
    ITEM_FIELD(ATTACH_DATA, attack),
    ITEM_FIELD(ATTACH_DATA, endurance),
    ITEM_FIELD(ATTACH_DATA, speed),
    ITEM_FIELD(ATTACH_DATA, magic),
    ITEM_FIELD(ATTACH_DATA, elem),
    ITEM_FIELD(ATTACH_DATA, vs_monster),
    ITEM_FIELD(ATTACH_DATA, unk_1F),
};

constexpr std::array kWeaponFields = {
    ITEM_FIELD(WEAPON_DATA, durability),
    ITEM_FIELD(WEAPON_DATA, attack),
    ITEM_FIELD(WEAPON_DATA, endurance),
    ITEM_FIELD(WEAPON_DATA, speed),
    ITEM_FIELD(WEAPON_DATA, magic),
    ITEM_FIXED(WEAPON_DATA, owner),
    ITEM_FIELD(WEAPON_DATA, hole),
    ITEM_FIXED(WEAPON_DATA, hole_num),
    ITEM_FIELD(WEAPON_DATA, elem),
    ITEM_FIELD(WEAPON_DATA, vs_monster),
    ITEM_FIELD(WEAPON_DATA, exp_base),
    ITEM_FIELD(WEAPON_DATA, exp_per_level),
    ITEM_FIELD(WEAPON_DATA, unk_34),
    ITEM_FIELD(WEAPON_DATA, unk_36),
    ITEM_FIELD(WEAPON_DATA, flags),
    ITEM_FIELD(WEAPON_DATA, unk_3A),
    ITEM_FIELD(WEAPON_DATA, buildup_mask0),
    ITEM_FIELD(WEAPON_DATA, buildup_mask1),
    ITEM_FIELD(WEAPON_DATA, attack_max),
    ITEM_FIELD(WEAPON_DATA, magic_max),
    ITEM_FIELD(WEAPON_DATA, chain_pos),
    ITEM_FIELD(WEAPON_DATA, unk_4A),
};

// A bait's item stays as compiled while the picker's list of baits is code.
constexpr std::array kBaitFields = {ITEM_FIELD(EsaInfo, radius)};

constexpr std::array kPriceFields = {ITEM_FIELD(ITEM_MONEY_ENTRY, buy_price), ITEM_FIELD(ITEM_MONEY_ENTRY, sell_price)};

#undef ITEM_FIELD
#undef ITEM_FIXED

/**
 * The tables as compiled, copied before anything is loaded into them.
 */
struct Compiled {
    std::array<COM_ITEM_INFO, std::size(ComItemInfo)>      common;
    std::array<const char *, std::size(ITEM_NAME_TBL_NEW)> models;
    std::array<ITEM_DATA, std::size(ITEM_LIST)>            items;
    std::array<ATTACH_DATA, std::size(AttachList)>         attachments;
    std::array<WEAPON_DATA, std::size(WeaponList)>         weapons;
    decltype(esa_info)                                     bait;
    std::array<ITEM_MONEY_ENTRY, std::size(PriceList)>     prices;
};

const Compiled &Original() {
    static const Compiled compiled = [] {
        Compiled copy{};
        std::ranges::copy(ComItemInfo, copy.common.begin());
        std::ranges::copy(ITEM_NAME_TBL_NEW, copy.models.begin());
        std::ranges::copy(ITEM_LIST, copy.items.begin());
        std::ranges::copy(AttachList, copy.attachments.begin());
        std::ranges::copy(WeaponList, copy.weapons.begin());
        copy.bait = esa_info;
        std::ranges::copy(PriceList, copy.prices.begin());
        return copy;
    }();
    return compiled;
}

/**
 * An item and the index of its row in a table.
 */
struct Row {
    int         item;
    std::size_t index;
};

/**
 * A table: its document's path under gameplay/, kind and key for its rows, and the retail rows they fill.
 */
struct Table {
    std::string_view       path;
    std::string_view       kind;
    const char            *rows;
    std::span<const Field> fields;
    std::size_t            row_size;
    unsigned char         *live;
    const unsigned char   *original;
    std::vector<Row>       items;
    bool                   models = false; /**< item_common: rows also name the item's model. */

    [[nodiscard]] unsigned char *LiveRow(const Row &row) const { return live + row_size * row.index; }

    [[nodiscard]] const unsigned char *CompiledRow(const Row &row) const { return original + row_size * row.index; }
};

template <class T>
Table MakeTable(std::string_view path, std::string_view kind, const char *rows, std::span<const Field> fields,
                std::span<T> live, std::span<const T> original, std::vector<Row> items) {
    return {.path = path,
            .kind = kind,
            .rows = rows,
            .fields = fields,
            .row_size = sizeof(T),
            .live = reinterpret_cast<unsigned char *>(live.data()),
            .original = reinterpret_cast<const unsigned char *>(original.data()),
            .items = std::move(items)};
}

std::size_t CommonIndex(int item) {
    return static_cast<std::size_t>(item - ITEM_ATTACH_START);
}

// Every item number in ComItemInfo, each at its own row.
std::vector<Row> EveryItem() {
    std::vector<Row> items;
    for (std::size_t index = 0; index < std::size(ComItemInfo); index++) {
        items.push_back({.item = ITEM_ATTACH_START + static_cast<int>(index), .index = index});
    }
    return items;
}

// The items whose rows lie in a table of this kind; items 131 and 256 index one past the end of theirs.
std::vector<Row> ItemsOfKind(int kind, std::size_t length) {
    std::vector<Row> items;
    for (const Row &row : EveryItem()) {
        const COM_ITEM_INFO &info = Original().common[row.index];
        if (info.kind == kind && info.index >= 0 && std::cmp_less(info.index, length)) {
            items.push_back({.item = row.item, .index = static_cast<std::size_t>(info.index)});
        }
    }
    return items;
}

std::vector<Row> Baits() {
    std::vector<Row> items;
    for (std::size_t index = 0; index < Original().bait.size(); index++) {
        items.push_back({.item = Original().bait[index].item_no, .index = index});
    }
    return items;
}

const std::vector<Table> &Tables() {
    static const std::vector<Table> tables = [] {
        const Compiled    &original = Original();
        std::vector<Table> list;
        list.push_back(MakeTable<COM_ITEM_INFO>("items/item_common.json", "item_common", "records", kCommonFields,
                                                ComItemInfo, original.common, EveryItem()));
        list.back().models = true;
        list.push_back(MakeTable<ITEM_DATA>("items/items.json", "items", "items", kItemFields, ITEM_LIST, original.items,
                                            ItemsOfKind(ITEMKIND_ITEM, std::size(ITEM_LIST))));
        list.push_back(MakeTable<ATTACH_DATA>("items/attachments.json", "attachments", "attachments", kAttachFields,
                                              AttachList, original.attachments,
                                              ItemsOfKind(ITEMKIND_ATTACH, std::size(AttachList))));
        list.push_back(MakeTable<WEAPON_DATA>("items/weapons.json", "weapons", "weapons", kWeaponFields, WeaponList,
                                              original.weapons, ItemsOfKind(ITEMKIND_WEAPON, std::size(WeaponList))));
        list.push_back(MakeTable<EsaInfo>("items/bait.json", "bait", "bait", kBaitFields, esa_info, original.bait, Baits()));
        list.push_back(MakeTable<ITEM_MONEY_ENTRY>("items/prices.json", "prices", "prices", kPriceFields, PriceList,
                                                   original.prices, EveryItem()));
        return list;
    }();
    return tables;
}

bool HasModel(const Row &row) {
    return row.index < std::size(ITEM_NAME_TBL_NEW);
}

template <class T>
T LoadAs(const unsigned char *at) {
    T value;
    std::memcpy(&value, at, sizeof value);
    return value;
}

template <class T>
void StoreAs(unsigned char *at, T value) {
    std::memcpy(at, &value, sizeof value);
}

Json Value(const unsigned char *at, const Field &field) {
    if (field.representation == Representation::Float) {
        return LoadAs<float>(at);
    }
    const bool is_signed = field.representation == Representation::Signed;
    switch (field.size) {
        case 1:
            return is_signed ? Json(LoadAs<std::int8_t>(at)) : Json(LoadAs<std::uint8_t>(at));
        case 2:
            return is_signed ? Json(LoadAs<std::int16_t>(at)) : Json(LoadAs<std::uint16_t>(at));
        default:
            return is_signed ? Json(LoadAs<std::int32_t>(at)) : Json(LoadAs<std::uint32_t>(at));
    }
}

// Writes a document's value into a field, refusing one the field cannot hold.
void Put(unsigned char *at, const Field &field, const Json &value, std::string_view where) {
    if (field.representation == Representation::Float) {
        StoreAs(at, native_config::Float(value));
        return;
    }
    if (!value.is_number_integer()) {
        throw std::runtime_error(std::format("{} is not an integer", where));
    }
    const auto      bits = static_cast<int>(field.size * 8);
    const bool      is_signed = field.representation == Representation::Signed;
    const long long low = is_signed ? -(1LL << (bits - 1)) : 0;
    const long long high = is_signed ? (1LL << (bits - 1)) - 1 : (1LL << bits) - 1;
    const bool      fits = value.is_number_unsigned() ? std::cmp_less_equal(value.get<std::uint64_t>(), high)
                                                      : value.get<long long>() >= low && value.get<long long>() <= high;
    if (!fits) {
        throw std::runtime_error(std::format("{} {} is outside {} to {}", where, value.dump(), low, high));
    }
    const auto number = value.get<long long>();
    switch (field.size) {
        case 1:
            return StoreAs(at, static_cast<std::uint8_t>(number));
        case 2:
            return StoreAs(at, static_cast<std::uint16_t>(number));
        default:
            return StoreAs(at, static_cast<std::uint32_t>(number));
    }
}

// A model name BtGetItemNamePath can put in a path.
bool ValidModel(std::string_view name) {
    return std::ranges::all_of(name, [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

Json Document(const Table &table) {
    Json rows = Json::array();
    for (const Row &item : table.items) {
        Json row = {
            {"item_no", item.item}
        };
        const unsigned char *bytes = table.CompiledRow(item);
        for (const Field &field : table.fields) {
            if (field.count == 1) {
                row[field.name] = Value(bytes + field.offset, field);
                continue;
            }
            Json values = Json::array();
            for (std::size_t i = 0; i < field.count; i++) {
                values.push_back(Value(bytes + field.offset + i * field.size, field));
            }
            row[field.name] = std::move(values);
        }
        if (table.models && HasModel(item)) {
            const char *model = Original().models[item.index];
            row["model"] = model != nullptr ? Json(model) : Json(nullptr);
        }
        rows.push_back(std::move(row));
    }
    return {
        {"schema_version", native_config::kSchemaVersion},
        {"kind",           table.kind                   },
        {table.rows,       std::move(rows)              }
    };
}

/**
 * A table's rows as a document gives them, checked whole: each row's bytes in the table's order, and
 * for item_common the model names.
 */
struct Parsed {
    std::vector<std::vector<unsigned char>> rows;
    std::vector<std::optional<std::string>> models;
    std::set<std::string>                   unknown;
};

Parsed Parse(const Table &table, const Json &document) {
    std::map<int, const Json *> by_item;
    for (const Json &row : native_config::Array(document, table.rows)) {
        const int item = native_config::Int(row, "item_no");
        if (std::ranges::none_of(table.items, [item](const Row &known) { return known.item == item; })) {
            throw std::runtime_error(std::format("item {} is not one this table holds", item));
        }
        if (!by_item.emplace(item, &row).second) {
            throw std::runtime_error(std::format("item {} is listed twice", item));
        }
    }
    Parsed parsed;
    for (const Row &item : table.items) {
        const auto found = by_item.find(item.item);
        if (found == by_item.end()) {
            throw std::runtime_error(std::format("item {} is missing", item.item));
        }
        const Json                &row = *found->second;
        const unsigned char       *original = table.CompiledRow(item);
        std::vector<unsigned char> bytes(original, original + table.row_size);
        for (const Field &field : table.fields) {
            const Json       &value = native_config::At(row, field.name);
            const std::string where = std::format("item {} {}", item.item, field.name);
            if (field.count == 1) {
                Put(bytes.data() + field.offset, field, value, where);
            } else {
                if (!value.is_array() || value.size() != field.count) {
                    throw std::runtime_error(std::format("{} needs {} values", where, field.count));
                }
                for (std::size_t i = 0; i < field.count; i++) {
                    Put(bytes.data() + field.offset + i * field.size, field, value[i], where);
                }
            }
            if (field.fixed && std::memcmp(bytes.data() + field.offset, original + field.offset, field.size * field.count) != 0) {
                throw std::runtime_error(std::format("{} is fixed by the save's shape and cannot change", where));
            }
        }
        for (const auto &[key, value] : row.items()) {
            const bool known = key == "item_no" || (table.models && key == "model") ||
                               std::ranges::any_of(table.fields, [&key](const Field &field) { return key == field.name; });
            if (!known) {
                parsed.unknown.insert(key);
            }
        }
        if (table.models && HasModel(item)) {
            const Json &model = native_config::At(row, "model");
            if (model.is_null()) {
                parsed.models.emplace_back();
            } else if (!model.is_string() || !ValidModel(model.get_ref<const std::string &>())) {
                throw std::runtime_error(std::format("item {} model must be lower-case letters, digits and underscores", item.item));
            } else {
                parsed.models.emplace_back(model.get<std::string>());
            }
        }
        parsed.rows.push_back(std::move(bytes));
    }
    return parsed;
}

// Points the model table at a document's names: retail's own string where the name is unchanged.
void LoadModels(const std::vector<std::optional<std::string>> &models) {
    static std::deque<std::string> names; // the strings a changed name points at, kept for the run
    for (std::size_t i = 0; i < models.size(); i++) {
        const char *original = Original().models[i];
        if (!models[i]) {
            ITEM_NAME_TBL_NEW[i] = nullptr;
        } else if (original != nullptr && *models[i] == original) {
            ITEM_NAME_TBL_NEW[i] = original;
        } else {
            ITEM_NAME_TBL_NEW[i] = names.emplace_back(*models[i]).c_str();
        }
    }
}

void Fill(const Table &table, const Parsed *parsed) {
    for (std::size_t i = 0; i < table.items.size(); i++) {
        const Row &item = table.items[i];
        std::memcpy(table.LiveRow(item), parsed != nullptr ? parsed->rows[i].data() : table.CompiledRow(item), table.row_size);
    }
    if (!table.models) {
        return;
    }
    if (parsed != nullptr) {
        LoadModels(parsed->models);
    } else {
        std::ranges::copy(Original().models, ITEM_NAME_TBL_NEW);
    }
}

// The vanilla items' IDs by number; a number the decomp has not named is vanilla:item_<number>.
std::string VanillaId(int item) {
    struct Entry {
        int              number;
        std::string_view name;
    };

    static constexpr std::array kIds = std::to_array<Entry>({
#include "item/item_ids.inc"
    });
    const auto found = std::ranges::find(kIds, item, &Entry::number);
    return found != kIds.end() ? std::format("vanilla:{}", found->name) : std::format("vanilla:item_{}", item);
}

// The traits a bag item's row gives it.
void ItemTraits(Def &def, const ITEM_DATA &data) {
    if ((data.kind_flags & ITEMKINDF_CONSUMABLE) != 0) {
        def.With(Consumable{.heals_hp = (data.use_flags & ITEMUSE_HEAL_HP) != 0,
                            .restores_water = (data.use_flags & ITEMUSE_DRINK) != 0,
                            .grants_status = (data.use_flags & ITEMUSE_STATUS) != 0,
                            .volume = data.vol,
                            .volume_range = data.vol_range});
    }
    if ((data.kind_flags & ITEMKINDF_THROWABLE) != 0) {
        def.With(Throwable{});
    }
    if (data.stack_kind == ITEM_STACK_MULTI || data.stack_kind == ITEM_STACK_SINGLE) {
        def.With(QuickSlot{.stack = data.stack_kind == ITEM_STACK_MULTI ? Stack::Multi : Stack::Single});
    }
    if (data.sort_key == ITEM_SORT_FISH) {
        def.With(Fish{});
    } else if (data.sort_key == ITEM_SORT_KEY_ITEM) {
        def.With(KeyItem{});
    } else if (data.sort_key == ITEM_SORT_QUEST) {
        def.With(QuestItem{});
    }
}

// A vanilla item as the tables hold it now.
Def VanillaDef(int item) {
    const COM_ITEM_INFO    &info = ComItemInfo[CommonIndex(item)];
    const ITEM_MONEY_ENTRY &price = PriceList[CommonIndex(item)];
    Def                     def{.id = VanillaId(item), .number = item};
    def.With(Sellable{.buy = price.buy_price, .sell = price.sell_price});
    if (info.kind == ITEMKIND_ITEM && info.index >= 0 && std::cmp_less(info.index, std::size(ITEM_LIST))) {
        ItemTraits(def, ITEM_LIST[info.index]);
    } else if (info.kind == ITEMKIND_ATTACH && info.index >= 0 && std::cmp_less(info.index, std::size(AttachList))) {
        const ATTACH_DATA &data = AttachList[info.index];
        Attachment         attachment{.attack = data.attack, .endurance = data.endurance, .speed = data.speed, .magic = data.magic};
        std::ranges::copy(data.elem, attachment.elements.begin());
        std::ranges::copy(data.vs_monster, attachment.vs_monster.begin());
        def.With(attachment);
    } else if (info.kind == ITEMKIND_WEAPON && info.index >= 0 && std::cmp_less(info.index, std::size(WeaponList))) {
        const WEAPON_DATA &data = WeaponList[info.index];
        def.With(Weapon{.owner = data.owner,
                        .durability = data.durability,
                        .attack = data.attack,
                        .endurance = data.endurance,
                        .speed = data.speed,
                        .magic = data.magic,
                        .attack_max = data.attack_max,
                        .magic_max = data.magic_max,
                        .holes = data.hole_num});
    }
    if (const auto bait = std::ranges::find(esa_info, item, &EsaInfo::item_no); bait != esa_info.end()) {
        def.With(Bait{.radius = bait->radius});
    }
    // Retail's rod is a rod with the stats that change nothing.
    if (item == ITEM_FISHING_ROD) {
        def.With(Rod{});
    }
    return def;
}

/**
 * A custom bag item, from its own document, gameplay/items/custom/<namespace>.<name>.json.
 */
struct Custom {
    std::string                     id;
    COM_ITEM_INFO                   common{};
    ITEM_DATA                       data{};
    ITEM_MONEY_ENTRY                price{};
    std::string                     model; /**< Empty for none. */
    std::map<std::string, ItemText> text;  /**< By language file, en_gb and so on. */
    std::optional<Bait>             bait;
    std::optional<Rod>              rod;
    State                           state; /**< A new copy's state; empty for none. */
    int                             state_version = 0;
    bool                            orphan = false; /**< Stands for a loaded save's item with no definition. */
};

/**
 * The custom items, in number order from kCustomFirst, then the orphans.
 */
std::vector<Custom> customs;

const Custom *CustomAt(int number) {
    number = Definition(number);
    if (number < kCustomFirst || number - kCustomFirst >= static_cast<int>(customs.size())) {
        return nullptr;
    }
    return &customs[static_cast<std::size_t>(number - kCustomFirst)];
}

constexpr const char *kOrphanName = "item.orphan.name";
constexpr const char *kOrphanNameText = "Unknown item";
constexpr const char *kOrphanDescription = "item.orphan.description";
constexpr const char *kOrphanDescriptionText = "From a mod that isn't loaded.";

/**
 * CustomMessage's codes by language file, item number and whether it is a name.
 */
std::map<std::tuple<std::string, int, bool>, std::vector<s16>> messages;

// A field of a custom item's row, 0 when the document leaves it out.
void PutOptional(unsigned char *row, const Field &field, const Json &document, std::string_view where) {
    const auto found = document.find(field.name);
    if (found != document.end()) {
        Put(row + field.offset, field, *found, std::format("{} {}", where, field.name));
    }
}

// Names a custom item document's fields this version does not read, which would otherwise be
// ignored without a word (a field renamed since the document was written).
void ReportUnknown(std::string_view file, const Json &document) {
    std::set<std::string> unknown;
    for (const auto &[key, value] : document.items()) {
        const bool known = key == "schema_version" || key == "kind" || key == "id" || key == "icon" || key == "model" ||
                           key == "text" || key == "bait" || key == "rod" || key == "state" || key == "state_version" ||
                           std::ranges::any_of(kItemFields, [&key](const Field &field) { return key == field.name; }) ||
                           std::ranges::any_of(kPriceFields, [&key](const Field &field) { return key == field.name; });
        if (!known) {
            unknown.insert(key);
        }
    }
    if (const auto bait = document.find("bait"); bait != document.end() && bait->is_object()) {
        for (const auto &[key, value] : bait->items()) {
            if (key != "radius" && key != "lure" && key != "default_affinity") {
                unknown.insert("bait." + key);
            }
        }
    }
    if (const auto rod = document.find("rod"); rod != document.end() && rod->is_object()) {
        for (const auto &[key, value] : rod->items()) {
            if (key != "heft" && key != "control" && key != "spring" && key != "balance" && key != "lure") {
                unknown.insert("rod." + key);
            }
        }
    }
    if (!unknown.empty()) {
        std::string names;
        for (const std::string &name : unknown) {
            names += names.empty() ? name : ", " + name;
        }
        native_config::Report(std::format("gameplay/items/custom/{}", file), "fields this version does not read: " + names);
    }
}

Custom ParseCustom(std::string_view file, const Json &document) {
    Custom      custom;
    const Json &id = native_config::At(document, "id");
    if (!id.is_string() || !ValidId(id.get_ref<const std::string &>())) {
        throw std::runtime_error("id must be namespace:name in lower-case letters, digits and underscores");
    }
    custom.id = id.get<std::string>();
    if (custom.id.starts_with("vanilla:")) {
        throw std::runtime_error("the vanilla namespace is retail's items");
    }
    std::string expected = custom.id;
    std::ranges::replace(expected, ':', '.');
    if (vfs::FoldKey(file) != vfs::FoldKey(expected + ".json")) {
        throw std::runtime_error(std::format("an item with id {} belongs in {}.json", custom.id, expected));
    }
    for (const Field &field : kItemFields) {
        PutOptional(reinterpret_cast<unsigned char *>(&custom.data), field, document, custom.id);
    }
    for (const Field &field : kPriceFields) {
        PutOptional(reinterpret_cast<unsigned char *>(&custom.price), field, document, custom.id);
    }
    custom.common.kind = ITEMKIND_ITEM;
    custom.common.index = -1;
    custom.common.icon_index = -1;
    if (const auto icon = document.find("icon"); icon != document.end()) {
        const int value = native_config::Int(document, "icon");
        if (value < -1 || value > std::numeric_limits<s16>::max()) {
            throw std::runtime_error("icon must be a retail icon number, or -1 for none");
        }
        custom.common.icon_index = static_cast<s16>(value);
    }
    if (const auto model = document.find("model"); model != document.end() && !model->is_null()) {
        if (!model->is_string() || model->get_ref<const std::string &>().empty() || !ValidModel(model->get_ref<const std::string &>())) {
            throw std::runtime_error("model must be lower-case letters, digits and underscores");
        }
        custom.model = model->get<std::string>();
    }
    const Json &text = native_config::At(document, "text");
    if (!text.is_object() || text.empty()) {
        throw std::runtime_error("text must give the item's name and description in at least one language");
    }
    for (const auto &[language, entry] : text.items()) {
        const Json &name = native_config::At(entry, "name");
        const Json &description = native_config::At(entry, "description");
        if (!name.is_string() || !description.is_string()) {
            throw std::runtime_error(std::format("text {}: name and description must be strings", language));
        }
        custom.text[language] = {.name = name.get<std::string>(), .description = description.get<std::string>()};
    }
    if (const auto bait = document.find("bait"); bait != document.end()) {
        custom.bait = Bait{.radius = native_config::Float(native_config::At(*bait, "radius"))};
        if (!(custom.bait->radius > 0.0f)) {
            throw std::runtime_error("bait radius must be above 0");
        }
        if (bait->contains("lure")) {
            if (!(*bait)["lure"].is_boolean()) {
                throw std::runtime_error("bait lure must be true or false");
            }
            custom.bait->lure = (*bait)["lure"].get<bool>();
        }
        if (bait->contains("default_affinity")) {
            // A fish takes the bait at half its liking a check, so 2 is the most that means anything.
            custom.bait->default_affinity = native_config::Float(native_config::At(*bait, "default_affinity"));
            if (custom.bait->default_affinity < 0.0f || custom.bait->default_affinity > 2.0f) {
                throw std::runtime_error("bait default_affinity must be from 0 to 2");
            }
        }
    }
    if (const auto rod = document.find("rod"); rod != document.end()) {
        if (!rod->is_object()) {
            throw std::runtime_error("rod must hold its stats: heft, control, spring, balance, lure");
        }
        Rod stats;
        for (const auto &[key, field] : std::initializer_list<std::pair<const char *, float Rod::*>>{
                 {"heft", &Rod::heft}, {"control", &Rod::control}, {"spring", &Rod::spring}, {"balance", &Rod::balance}, {"lure", &Rod::lure}}) {
            if (rod->contains(key)) {
                stats.*field = native_config::Float((*rod)[key]);
                if (stats.*field < 0.0f || stats.*field > 100.0f) {
                    throw std::runtime_error(std::format("rod {} must be from 0 to 100", key));
                }
            }
        }
        custom.rod = stats;
    }
    if (const auto state = document.find("state"); state != document.end()) {
        if (!state->is_object()) {
            throw std::runtime_error("state must map each name a copy keeps to the number it starts with");
        }
        for (const auto &[name, value] : state->items()) {
            if (!value.is_number()) {
                throw std::runtime_error(std::format("state {} must be a number", name));
            }
            custom.state[name] = value.get<double>();
        }
        custom.state_version = native_config::Int(document, "state_version");
        if (custom.state_version < 0) {
            throw std::runtime_error("state_version must be 0 or more");
        }
    }
    ReportUnknown(file, document);
    return custom;
}

// Reads the custom items from the packs and the data, each named file once, giving them numbers in ID order.
void LoadCustom() {
    customs.clear();
    std::map<std::string, std::string> files;
    for (const std::string &name : datapacks::Members("gameplay/items/custom")) {
        files.try_emplace(vfs::FoldKey(name), name);
    }
    if (data::DataReadIndexed()) {
        constexpr std::string_view kFolder = "gameplay/items/custom/";
        for (const std::string &path : data::DataReadListFiles()) {
            if (path.size() <= kFolder.size() || vfs::FoldKey(path.substr(0, kFolder.size())) != vfs::FoldKey(kFolder)) {
                continue;
            }
            const std::string name = path.substr(kFolder.size());
            if (name.find('/') == std::string::npos) {
                files.try_emplace(vfs::FoldKey(name), name);
            }
        }
    }
    std::map<std::string, Custom> by_id;
    for (const std::string &name : std::views::values(files)) {
        native_config::Read(std::format("items/custom/{}", name), "item", [&](const Json &document) {
            Custom            custom = ParseCustom(name, document);
            const std::string custom_id = custom.id;
            by_id.insert_or_assign(custom_id, std::move(custom));
        });
    }
    for (auto &[id, custom] : by_id) {
        if (kCustomFirst + static_cast<int>(customs.size()) >= kCustomEnd) {
            native_config::Report("gameplay/items/custom", std::format("more custom items than there are numbers; {} and after left out", id));
            break;
        }
        custom.common.msg = static_cast<s16>(kCustomFirst + static_cast<int>(customs.size()));
        customs.push_back(std::move(custom));
    }
}

Def CustomDef(int number, const Custom &custom) {
    Def def{.id = custom.id, .number = number, .state = custom.state, .state_version = custom.state_version};
    def.With(Sellable{.buy = custom.price.buy_price, .sell = custom.price.sell_price});
    ItemTraits(def, custom.data);
    if (custom.bait) {
        def.With(*custom.bait);
    }
    if (custom.rod) {
        def.With(*custom.rod);
    }
    return def;
}

} // namespace

void Load() {
    for (const Table &table : Tables()) {
        std::optional<Parsed> parsed;
        native_config::Read(table.path, table.kind, [&](const Json &document) { parsed = Parse(table, document); });
        Fill(table, parsed ? &*parsed : nullptr);
        if (parsed && !parsed->unknown.empty()) {
            std::string names;
            for (const std::string &name : parsed->unknown) {
                names += names.empty() ? name : ", " + name;
            }
            native_config::Report(std::format("gameplay/{}", table.path), "fields this version does not read: " + names);
        }
    }
    LoadCustom();
    messages.clear();
    Clear();
    for (const Row &row : EveryItem()) {
        Register(VanillaDef(row.item));
    }
    for (std::size_t index = 0; index < customs.size(); index++) {
        Register(CustomDef(kCustomFirst + static_cast<int>(index), customs[index]));
    }
    RebindCopies();
    RegisterSaveChunk();
}

int OrphanNumber(std::string_view id) {
    for (const Custom &custom : customs) {
        if (custom.orphan && custom.id == id) {
            return custom.common.msg;
        }
    }
    const int number = kCustomFirst + static_cast<int>(customs.size());
    if (number >= kCustomEnd || !ValidId(id) || Find(id) != nullptr) {
        return -1;
    }
    Custom orphan{.id = std::string(id), .orphan = true};
    orphan.common.kind = ITEMKIND_ITEM;
    orphan.common.index = -1;
    orphan.common.icon_index = -1;
    orphan.common.msg = static_cast<s16>(number);
    customs.push_back(std::move(orphan));
    // Registered without traits: nothing reads it as bait, sellable or anything else.
    Register(Def{.id = std::string(id), .number = number});
    return number;
}

std::string VanillaItemId(int number) {
    return VanillaId(number);
}

bool IsOrphan(int number) {
    const Custom *custom = CustomAt(number);
    return custom != nullptr && custom->orphan;
}

void DropOrphans() {
    while (!customs.empty() && customs.back().orphan) {
        Unregister(customs.back().id);
        customs.pop_back();
    }
    RebindCopies();
}

COM_ITEM_INFO *CustomCommonInfo(int number) {
    const Custom *custom = CustomAt(number);
    return custom != nullptr ? const_cast<COM_ITEM_INFO *>(&custom->common) : nullptr;
}

ITEM_DATA *CustomItemData(int number) {
    const Custom *custom = CustomAt(number);
    return custom != nullptr ? const_cast<ITEM_DATA *>(&custom->data) : nullptr;
}

const ITEM_MONEY_ENTRY *CustomPrice(int number) {
    const Custom *custom = CustomAt(number);
    return custom != nullptr ? &custom->price : nullptr;
}

const char *CustomModel(int number) {
    const Custom *custom = CustomAt(number);
    return custom != nullptr && !custom->model.empty() ? custom->model.c_str() : nullptr;
}

Kind KindOf(int number) {
    number = Definition(number);
    if (number >= kCustomFirst) {
        return CustomAt(number) != nullptr ? Kind::Item : Kind::None;
    }
    if (number >= ITEM_WEAPON_START) {
        return Kind::Weapon;
    }
    if (number >= ITEM_DUNGEON_START) {
        return Kind::Item;
    }
    return number >= ITEM_ATTACH_START ? Kind::Attachment : Kind::None;
}

void ReclaimHeldCopies() {
    if (SaveData == nullptr) {
        return;
    }
    std::set<int>    held;
    const ITEM_PACK &pack = SaveData->GetDngStatus()->item_pack;
    held.insert(std::begin(pack.item), std::end(pack.item));
    held.insert(std::begin(pack.quick_item_slot), std::end(pack.quick_item_slot));
    const CStockItem &stock = *SaveData->GetStockItem();
    held.insert(std::begin(stock.dungeon_items), std::end(stock.dungeon_items));
    ReclaimCopies([&](int handle) { return held.contains(handle); });
}

const char *ModelName(int number) {
    if (number >= kCustomFirst) {
        return CustomModel(number);
    }
    if (number < ITEM_ATTACH_START || number - ITEM_ATTACH_START >= static_cast<int>(std::size(ITEM_NAME_TBL_NEW))) {
        return nullptr;
    }
    return ITEM_NAME_TBL_NEW[number - ITEM_ATTACH_START];
}

const ItemText *CustomText(int number, std::string_view language) {
    const Custom *custom = CustomAt(number);
    if (custom == nullptr) {
        return nullptr;
    }
    if (custom->orphan) {
        // By language, as CustomMessage caches what this returns.
        static std::map<std::string, ItemText, std::less<>> orphan;
        auto [text, made] = orphan.try_emplace(std::string(language));
        if (made) {
            text->second = {.name = localize::LocalizeText(kOrphanName, kOrphanNameText),
                            .description = localize::LocalizeText(kOrphanDescription, kOrphanDescriptionText)};
        }
        return &text->second;
    }
    for (const std::string_view wanted : {language, std::string_view("en_gb"), std::string_view("en_us")}) {
        if (const auto found = custom->text.find(std::string(wanted)); found != custom->text.end()) {
            return &found->second;
        }
    }
    return &custom->text.begin()->second;
}

short *CustomMessage(int id, bool system) {
    const std::string language = localize::LocalizeLanguageFile();
    const int         number = Definition(id - (system ? 100 : 500));
    const ItemText   *text = CustomText(number, language);
    if (text == nullptr) {
        return nullptr;
    }
    // By the item, not the id: a handle's id names another item once the handle is reused.
    auto [codes, made] = messages.try_emplace(std::tuple(language, number, system));
    if (made) {
        text::GameTextEncode(system ? text->name : text->description, codes->second);
        // The game reads whole quadwords off a message's end, as it does off a file's.
        codes->second.resize((codes->second.size() + 7) & ~std::size_t{7}, game::MES_CODE_END);
    }
    return codes->second.data();
}

std::vector<std::pair<std::string, std::string>> Strings() {
    return {
        {kOrphanName,        kOrphanNameText       },
        {kOrphanDescription, kOrphanDescriptionText},
    };
}

bool Export(const std::filesystem::path &directory) {
    for (const Table &table : Tables()) {
        const std::filesystem::path file = directory / "gameplay" / table.path;
        std::error_code             error;
        std::filesystem::create_directories(file.parent_path(), error);
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << Document(table).dump(2) << '\n';
        if (error || !stream) {
            std::println(stderr, "cannot write {}", PathsDisplay(file));
            return false;
        }
    }
    return true;
}

} // namespace dc::item
