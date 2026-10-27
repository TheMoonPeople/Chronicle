#include "item/framework.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>

namespace dc::item {

namespace {

/**
 * The registered items, each owned on its own so a Def a caller holds stays put while others register.
 */
struct Registry {
    std::map<int, std::unique_ptr<Def>>             by_number;
    std::map<std::string, const Def *, std::less<>> by_id;
};

Registry &Items() {
    static Registry registry;
    return registry;
}

bool ValidPart(std::string_view part) {
    return !part.empty() && std::ranges::all_of(part, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

} // namespace

bool ValidId(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon != std::string_view::npos && ValidPart(id.substr(0, colon)) && ValidPart(id.substr(colon + 1));
}

void Register(Def item) {
    Registry &registry = Items();
    if (!ValidId(item.id)) {
        throw std::logic_error(std::format("item ID \"{}\" is not namespace:name", item.id));
    }
    if (registry.by_id.contains(item.id)) {
        throw std::logic_error(std::format("item \"{}\" is registered twice", item.id));
    }
    if (const auto taken = registry.by_number.find(item.number); taken != registry.by_number.end()) {
        throw std::logic_error(
            std::format("item \"{}\" has number {}, which \"{}\" has", item.id, item.number, taken->second->id));
    }
    if (item.Has<Weapon>() && (item.Has<Consumable>() || item.Has<Attachment>())) {
        throw std::logic_error(std::format("item \"{}\" is a weapon and cannot also be consumed or attached", item.id));
    }
    const int number = item.number;
    auto      owned = std::make_unique<Def>(std::move(item));
    registry.by_id.emplace(owned->id, owned.get());
    registry.by_number.emplace(number, std::move(owned));
}

void Clear() {
    Registry &registry = Items();
    registry.by_id.clear();
    registry.by_number.clear();
}

const Def *Find(std::string_view id) {
    const Registry &registry = Items();
    const auto      found = registry.by_id.find(id);
    return found != registry.by_id.end() ? found->second : nullptr;
}

const Def *Find(int number) {
    const Registry &registry = Items();
    const auto      found = registry.by_number.find(number);
    return found != registry.by_number.end() ? found->second.get() : nullptr;
}

std::vector<const Def *> All() {
    std::vector<const Def *> items;
    for (const auto &[number, item] : Items().by_number) {
        items.push_back(item.get());
    }
    return items;
}

} // namespace dc::item
