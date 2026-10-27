#include "dungeoneventdata.hpp"

#include <print>

namespace dc::game {

void CDungeonEventData::Set(CDungeonEvent *source) {
    if (source == nullptr) {
        std::println("******** event NULL !!");

        for (;;) {
        }
    }

    event = source;
    hold = source->hold;
    switch_on = source->switch_on;
    enabled = 1;
    chara_done = -1;
}

int CDungeonEventData::CheckSwitch() {
    if (event == nullptr) {
        return 0;
    }

    return (enabled != 0 && switch_on != 0) ? 1 : 0;
}

void CDungeonEventData::Stop() {
    switch_on = 0;
}

void CDungeonEventData::Start() {
    switch_on = 1;
}

} // namespace dc::game
