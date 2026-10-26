#include "dngmessageman.hpp"

#include "dun/gameloop.hpp"
#include "userstatus.hpp"

void CDngMessageMan::LimmitZone() {
    int zone = UserStatus->res_limit_zone_current;

    if (zone >= 0 && zone < 6) {
        message = 0xB;
        insert_mes_1 = UserStatus->cur_chara + 0x32;
    }

    if (UserStatus->res_limit_zone_current == RES_LIMIT_ZONE_NO_WEAPON_CHANGE) {
        message = 0xD;
    }

    if (UserStatus->res_limit_zone_current == RES_LIMIT_ZONE_THIRST) {
        message = 0xE;
    }

    timer = 0xF0;
    steev_window = false;
}

void CDngMessageMan::SetStatus_Dry(float water_max, float water_before, float water_now) {
    float threshold = 0.1f * water_max;

    if (threshold < water_now && message == 0xAA) {
        message = -1;
    }

    if (0.0f < water_now && message == 0xAB) {
        message = -1;
    }

    if (threshold <= water_before + 0.5f && !(threshold <= water_now - 0.5f) && message == -1) {
        message = 0xAA;
        timer = 0xF0;
        steev_window = false;
    }

    if (water_now <= 0.0f && (message == -1 || message == 0xAB)) {
        message = 0xAB;
        timer = 0x9FFF6;
        steev_window = false;
    }
}

void CDngMessageMan::SetSteevMes(int first) {
    if (timer <= 0) {
        // The ten Steev lines are shown in turn, so the index rides on.
        message = first + steev_index;
        timer = 0xF0;
        steev_window = true;
        steev_index++;

        if (steev_index > 9) {
            steev_index = 0;
        }
    }
}
