#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <optional>
#include <print>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "dngstatusdata.hpp"
#include "item/framework.hpp"
#include "item/host.hpp"
#include "save/extension.hpp"
#include "savedata.hpp"
#include "stockitem.hpp"

namespace dc::item {

using namespace dc::game;

namespace {

constexpr std::uint32_t kVersion = 1;

/**
 * Where in the save a custom item sits.
 */
enum class Place : std::uint8_t {
    Pack,  /**< item_pack.item */
    Quick, /**< item_pack.quick_item_slot */
    Stock, /**< CStockItem::dungeon_items */
};

/**
 * A custom item taken out of the save, and where it goes back.
 */
struct Record {
    std::string   id;
    Place         place = Place::Pack;
    int           slot = 0;
    std::int32_t  vol = 0;      /**< What is left of it: item_vol, active_item_vol, dungeon_item_vols. */
    std::int32_t  qty = 1;      /**< How many a quick slot holds. */
    std::uint64_t instance = 0; /**< 0 for an item without state of its own. */
    int           state_version = 0;
    State         state;
};

std::vector<std::uint8_t> Encode(std::uint64_t next_instance, const std::vector<Record> &records) {
    save::ChunkWriter out;
    out.Put(next_instance);
    out.Put(static_cast<std::uint32_t>(records.size()));
    for (const Record &record : records) {
        out.PutText(record.id);
        out.Put(static_cast<std::uint8_t>(record.place));
        out.Put(static_cast<std::uint16_t>(record.slot));
        out.Put(record.vol);
        out.Put(record.qty);
        out.Put(record.instance);
        if (record.instance != 0) {
            out.Put(static_cast<std::int32_t>(record.state_version));
            out.Put(static_cast<std::uint16_t>(record.state.size()));
            for (const auto &[name, value] : record.state) {
                out.PutText(name);
                out.Put(std::bit_cast<std::uint64_t>(value));
            }
        }
    }
    return out.Take();
}

std::optional<std::vector<Record>> Decode(std::span<const std::uint8_t> bytes, std::uint64_t &next_instance) {
    save::ChunkReader reader(bytes);
    std::uint32_t count = 0;
    if (!reader.Get(next_instance) || !reader.Get(count)) {
        return std::nullopt;
    }
    std::vector<Record> records;
    for (std::uint32_t index = 0; index < count; index++) {
        Record        record;
        std::uint8_t  place = 0;
        std::uint16_t slot = 0;
        if (!reader.GetText(record.id) || !reader.Get(place) || place > static_cast<std::uint8_t>(Place::Stock) ||
            !reader.Get(slot) || !reader.Get(record.vol) || !reader.Get(record.qty) || !reader.Get(record.instance)) {
            return std::nullopt;
        }
        record.place = static_cast<Place>(place);
        record.slot = slot;
        if (record.instance != 0) {
            std::int32_t  version = 0;
            std::uint16_t names = 0;
            if (!reader.Get(version) || !reader.Get(names)) {
                return std::nullopt;
            }
            record.state_version = version;
            for (std::uint16_t name = 0; name < names; name++) {
                std::string   key;
                std::uint64_t value = 0;
                if (!reader.GetText(key) || !reader.Get(value)) {
                    return std::nullopt;
                }
                record.state.insert_or_assign(std::move(key), std::bit_cast<double>(value));
            }
        }
        records.push_back(std::move(record));
    }
    if (!reader.Done()) {
        return std::nullopt;
    }
    return records;
}

// Leaves in image only what retail has numbers for, the empty slot it would hold in place of each
// custom item, and returns what was taken.
std::optional<save::ExtensionChunk> Take(CSaveData &image) {
    ReclaimHeldCopies();
    std::vector<Record> records;
    std::set<int>       seen;

    // A record for number, or false for a number retail has.
    const auto take = [&](int number, Place place, int slot, std::int32_t vol, std::int32_t qty) {
        if (number < kCustomFirst) {
            return false;
        }
        Record record{.place = place, .slot = slot, .vol = vol, .qty = qty};
        if (const Copy *copy = FindCopy(number)) {
            record.id = copy->id;
            // Retail can copy a whole slot (#158): the second place a handle turns up keeps a copy of
            // its own from now on.
            record.instance = seen.insert(number).second ? copy->instance : TakeInstance();
            record.state_version = copy->state_version;
            record.state = copy->state;
        } else if (const Def *item = Find(number)) {
            record.id = item->id;
        } else {
            std::println(stderr, "items: item number {} has no item; left out of the save", number);
            return true;
        }
        records.push_back(std::move(record));
        return true;
    };

    ITEM_PACK &pack = image.GetDngStatus()->item_pack;
    for (int slot = 0; slot < static_cast<int>(std::size(pack.item)); slot++) {
        if (take(pack.item[slot], Place::Pack, slot, pack.item_vol[slot], 1)) {
            pack.item[slot] = -1;
            pack.item_vol[slot] = 0;
        }
    }
    std::int32_t *quick_vol = image.GetDngStatus()->active_item_vol;
    for (int slot = 0; slot < static_cast<int>(std::size(pack.quick_item_slot)); slot++) {
        if (take(pack.quick_item_slot[slot], Place::Quick, slot, quick_vol[slot], pack.quick_item_qty[slot])) {
            pack.quick_item_slot[slot] = -1;
            pack.quick_item_qty[slot] = 0;
            quick_vol[slot] = 0;
        }
    }
    CStockItem &stock = *image.GetStockItem();
    for (int slot = 0; slot < static_cast<int>(std::size(stock.dungeon_items)); slot++) {
        if (take(stock.dungeon_items[slot], Place::Stock, slot, stock.dungeon_item_vols[slot], 1)) {
            stock.dungeon_items[slot] = 0;
            stock.dungeon_item_vols[slot] = 0;
        }
    }
    if (records.empty()) {
        return std::nullopt;
    }
    save::ExtensionChunk chunk;
    chunk.tag = {'I', 'T', 'E', 'M'};
    chunk.version = kVersion;
    chunk.payload = Encode(NextInstance(), records);
    return chunk;
}

// Puts number back where record says, or in the first free slot of the same kind when that one has
// been taken since; false when there is no room.
bool PutBack(const Record &record, int number) {
    ITEM_PACK    &pack = SaveData->GetDngStatus()->item_pack;
    std::int32_t *quick_vol = SaveData->GetDngStatus()->active_item_vol;
    CStockItem   &stock = *SaveData->GetStockItem();

    const auto into = [&](std::span<s16> items, std::span<s16> vols, std::size_t limit) {
        std::size_t slot = static_cast<std::size_t>(record.slot);
        if (slot >= items.size() || items[slot] >= ITEM_DUNGEON_START) {
            slot = 0;
            while (slot < limit && items[slot] >= ITEM_DUNGEON_START) {
                slot++;
            }
            if (slot >= limit) {
                return false;
            }
        }
        items[slot] = static_cast<s16>(number);
        vols[slot] = static_cast<s16>(record.vol);
        return true;
    };

    switch (record.place) {
        case Place::Quick:
            if (record.slot < static_cast<int>(std::size(pack.quick_item_slot)) && pack.quick_item_slot[record.slot] == -1) {
                pack.quick_item_slot[record.slot] = static_cast<s16>(number);
                pack.quick_item_qty[record.slot] = static_cast<s16>(record.qty);
                quick_vol[record.slot] = record.vol;
                return true;
            }
            [[fallthrough]];
        case Place::Pack:
            return into(pack.item, pack.item_vol, static_cast<std::size_t>(std::max<int>(pack.num, 0)));
        case Place::Stock:
            return into(stock.dungeon_items, stock.dungeon_item_vols, std::size(stock.dungeon_items));
    }
    return false;
}

void Restore(const save::ExtensionChunk *chunk) {
    DropOrphans();
    ClearCopies();
    if (chunk == nullptr) {
        return;
    }
    std::uint64_t                      next_instance = 1;
    std::optional<std::vector<Record>> records;
    if (chunk->version == kVersion) {
        records = Decode(chunk->payload, next_instance);
    }
    if (!records) {
        std::println(stderr, "items: the save's custom items cannot be read (version {}); they are left out", chunk->version);
        return;
    }
    ClearCopies(next_instance);
    for (const Record &record : *records) {
        const Def *item = Find(record.id);
        int        number = item != nullptr ? item->number : OrphanNumber(record.id);
        if (number >= 0 && record.instance != 0) {
            number = RestoreCopy(record.id, record.instance, record.state, record.state_version);
        }
        if (number < 0) {
            std::println(stderr, "items: no number left for the save's {}; it is left out", record.id);
            continue;
        }
        if (item == nullptr) {
            std::println(stderr, "items: the save's {} has no definition; it waits for one as an unknown item", record.id);
        }
        if (!PutBack(record, number)) {
            std::println(stderr, "items: no room for the save's {}; it is left out", record.id);
        }
    }
}

// Whether the chunk names an item no loaded pack defines: one that would load as an orphan.
bool Missing(const save::ExtensionChunk &chunk) {
    std::uint64_t next_instance = 0;
    if (chunk.version != kVersion) {
        return false;
    }
    const std::optional<std::vector<Record>> records = Decode(chunk.payload, next_instance);
    return records && std::ranges::any_of(*records, [](const Record &record) {
        const Def *item = Find(record.id);
        return item == nullptr || IsOrphan(item->number);
    });
}

} // namespace

void RegisterSaveChunk() {
    save::ExtensionRegister("ITEM", Take, Restore, Missing);
}

} // namespace dc::item
