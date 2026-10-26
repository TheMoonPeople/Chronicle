#include "dungeoneventdata.hpp"

#include <cstdio>

void CDungeonEventData::Set(CDungeonEvent *source) {
    if (source == NULL) {
        printf("******** event NULL !!\n");

        for (;;) {
        }
    }

    event = source;
    hold = source->hold;
    switch_on = source->switch_on;
    enabled = true;
    chara_done = -1;
}

int CDungeonEventData::CheckSwitch() {
    if (event == NULL) {
        return 0;
    }

    if (enabled != 0 && switch_on != 0) {
        return 1;
    }

    return 0;
}

void CDungeonEventData::Stop() {
    switch_on = false;
}

void CDungeonEventData::Start() {
    switch_on = true;
}
