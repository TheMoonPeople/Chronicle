#include "item/framework.hpp"

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>

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

/**
 * The copies by handle, and the instance the next new copy gets.
 */
struct Copies {
    std::map<int, Copy> by_handle;
    std::uint64_t       next_instance = 1;
};

Copies &TheCopies() {
    static Copies copies;
    return copies;
}

int FreeHandle() {
    const Copies &copies = TheCopies();
    int           handle = kHandleFirst;
    for (const int taken : std::views::keys(copies.by_handle)) {
        if (taken != handle) {
            break;
        }
        handle++;
    }
    return handle < kHandleEnd ? handle : -1;
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

void Unregister(std::string_view id) {
    Registry  &registry = Items();
    const auto found = registry.by_id.find(id);
    if (found == registry.by_id.end()) {
        return;
    }
    const int number = found->second->number;
    registry.by_id.erase(found);
    registry.by_number.erase(number);
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

int NewCopy(const Def &item) {
    if (item.state.empty()) {
        return -1;
    }
    const int handle = FreeHandle();
    if (handle < 0) {
        return -1;
    }
    Copies &copies = TheCopies();
    copies.by_handle.emplace(handle, Copy{.id = item.id,
                                          .number = item.number,
                                          .instance = copies.next_instance++,
                                          .state = item.state,
                                          .state_version = item.state_version});
    return handle;
}

int RestoreCopy(std::string_view id, std::uint64_t instance, State state, int state_version) {
    const int handle = FreeHandle();
    if (handle < 0) {
        return -1;
    }
    const Def *item = Find(id);
    TheCopies().by_handle.emplace(handle, Copy{.id = std::string(id),
                                               .number = item != nullptr ? item->number : -1,
                                               .instance = instance,
                                               .state = std::move(state),
                                               .state_version = state_version});
    return handle;
}

Copy *FindCopy(int handle) {
    auto      &copies = TheCopies().by_handle;
    const auto found = copies.find(handle);
    return found != copies.end() ? &found->second : nullptr;
}

int Definition(int number) {
    if (number < kHandleFirst || number >= kHandleEnd) {
        return number;
    }
    const Copy *copy = FindCopy(number);
    return copy != nullptr ? copy->number : -1;
}

void ReclaimCopies(const std::function<bool(int handle)> &held) {
    std::erase_if(TheCopies().by_handle, [&](const auto &entry) { return !held(entry.first); });
}

int CopyCount() {
    return static_cast<int>(TheCopies().by_handle.size());
}

void RebindCopies() {
    for (Copy &copy : std::views::values(TheCopies().by_handle)) {
        const Def *item = Find(copy.id);
        copy.number = item != nullptr ? item->number : -1;
    }
}

void ClearCopies(std::uint64_t next) {
    Copies &copies = TheCopies();
    copies.by_handle.clear();
    copies.next_instance = next;
}

std::uint64_t NextInstance() {
    return TheCopies().next_instance;
}

std::uint64_t TakeInstance() {
    return TheCopies().next_instance++;
}

} // namespace dc::item
