#include <algorithm>
#include <functional>
#include <utility>
#include <variant>
#include <vector>

#include "fishing/framework.hpp"

namespace dc::fishing {

namespace {

struct Listener {
    int                                                                                 id;
    std::variant<std::function<void(const Catch &)>, std::function<void(const Lost &)>> call;
};

std::vector<Listener> &Listeners() {
    static std::vector<Listener> listeners;
    return listeners;
}

int g_next_id = 1;

template <class Event>
void Tell(const Event &event) {
    // A copy: a listener may subscribe or unsubscribe as it is told.
    const std::vector<Listener> listeners = Listeners();
    for (const Listener &listener : listeners) {
        if (const auto *call = std::get_if<std::function<void(const Event &)>>(&listener.call)) {
            (*call)(event);
        }
    }
}

} // namespace

int OnCatch(std::function<void(const Catch &)> listener) {
    Listeners().push_back({g_next_id, std::move(listener)});
    return g_next_id++;
}

int OnLost(std::function<void(const Lost &)> listener) {
    Listeners().push_back({g_next_id, std::move(listener)});
    return g_next_id++;
}

void Unsubscribe(int id) {
    std::erase_if(Listeners(), [id](const Listener &listener) { return listener.id == id; });
}

void Publish(const Catch &fish) {
    Tell(fish);
}

void Publish(const Lost &fish) {
    Tell(fish);
}

} // namespace dc::fishing
