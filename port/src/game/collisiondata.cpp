#include "collisiondata.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <print>
#include <string>

#include "camerafollow.hpp"
#include "debugfont.hpp"
#include "dngmessageman.hpp"
#include "dun/gameloop.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "gamepad.hpp"
#include "hitmark.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#include "userstatus.hpp"

namespace dc::game {

namespace {

/** The action code of each debug overlay line, ended by -1. */
constexpr std::array<int, 15> DebugInfoCode = {
    DEBUG_INFO_MINIMAP_VIEW,
    DEBUG_INFO_COLLISION,
    DEBUG_INFO_BGM_PLAY,
    DEBUG_INFO_PARAMETER,
    DEBUG_INFO_VIEW_INFO,
    DEBUG_INFO_ULTRA_MAN,
    DEBUG_INFO_RELOAD_ENEMY,
    DEBUG_INFO_ITEM_PUT_ZONE,
    DEBUG_INFO_LIGHT_MODE,
    DEBUG_INFO_FLOOR_ATRA_GET,
    DEBUG_INFO_EVENT_TEST,
    DEBUG_INFO_SET_STATUS,
    DEBUG_INFO_SE_PLAY,
    DEBUG_INFO_SET_CHR_KEY,
    -1,
};

/** The printf format of each debug overlay line, ended by a null entry. */
const std::array<const char *, 15> DebugInfoMsg = {
    DebugInfoMsgMiniMapView,
    DebugInfoMsgCollision,
    DebugInfoMsgBgmPlay,
    DebugInfoMsgParameter,
    DebugInfoMsgViewInfo,
    DebugInfoMsgUltraMan,
    DebugInfoMsgReloadEnemy,
    DebugInfoMsgItemPutZone,
    DebugInfoMsgLightMode,
    DebugInfoMsgFloorAtraGet,
    DebugInfoMsgEventTest,
    DebugInfoMsgSetStatus,
    DebugInfoMsgSePlay,
    DebugInfoMsgSetChrKey,
    nullptr,
};

constexpr std::array<const char *, 2> on_off_names = {"OFF", "ON"};
constexpr std::array<const char *, 2> main_sub_names = {"MAIN", "SUB"};
constexpr std::array<const char *, 6> condition_names = {"RESET", "STONE", "BIN2", "POISON", "CURSE", "NEBA2"};
constexpr std::array<const char *, 3> power_names = {"HUMAN   ", "SUPERMAN", "ULTRAMAN"};

/** The debug overlay line the cursor is on. */
int DebugInfoNowCursor;

/** Key items waiting to be dropped, one entry each, -1 where a slot is free. */
std::array<int, 32> gateKeyStack;

/** The path NameExchg builds. */
std::string nameblock;

/** Where each language's caption centres each dungeon's floor number. */
constexpr std::array<int, 7> center_us = {0xCE, 0x125, 0x101, 0x100, 0x87, 0x57, 0x101};
constexpr std::array<int, 7> center_fr = {0xCD, 0x113, 0xF8, 0xF8, 0xB2, 0x59, 0xFF};
constexpr std::array<int, 7> center_gr = {0xFE, 0x103, 0x13D, 0xFD, 0x78, 0x38, 0xFF};
constexpr std::array<int, 7> center_it = {0xCC, 0x129, 0xF3, 0xFF, 0xFA, 0x75, 0xF9};
constexpr std::array<int, 7> center_sp = {0xCE, 0x12C, 0xF2, 0xFA, 0x7C, 0xBA, 0xFA};

/** Each language's floor-number centres, indexed by LanguageCode. */
constexpr std::array<const std::array<int, 7> *, 7> center_ptr = {&center_sp, &center_us, &center_us, &center_fr, &center_gr, &center_it, &center_sp};

/**
 * Appends printf-formatted text to the debug overlay's buffer. Without
 * arguments the text is appended as it is, so it must hold no conversions.
 */
template <typename... Args>
void AppendDebugText(const char *format, Args... args) {
    const std::size_t capacity = std::size(DbgMsg.text) - static_cast<std::size_t>(DbgMsg.length);

    if constexpr (sizeof...(Args) == 0) {
        DbgMsg.length += std::snprintf(&DbgMsg.text[DbgMsg.length], capacity, "%s", format);
    } else {
        DbgMsg.length += std::snprintf(&DbgMsg.text[DbgMsg.length], capacity, format, args...);
    }
}

/**
 * Flips a debug switch between off (0) and on (1).
 */
void ToggleDebugSwitch(s32 &value) { value = value == 0 ? 1 : 0; }

/**
 * Closes the debug overlay and returns the pad to normal input.
 */
void CloseDebugOverlay() {
    DebugStatus[0] = 0;
    GamePad.AutoRepeatOff();
    GamePad.MenuModeOff();
}

/**
 * Resets every runtime event record of the event manager to an idle, unheld state.
 */
void ClearEventData(CDungeonEventMan *event_man) {
    for (CDungeonEventData &data : event_man->event) {
        data.event = nullptr;
        data.switch_on = false;
        data.enabled = false;
        data.hold = 0;
        data.chara_done = -1;
    }
}

/**
 * Marks the floor's event places, treasure boxes, atla balls and room links unused and resets their counts.
 */
void ClearMapEvent(CDungeonMap *map) {
    for (DUNGEON_EVENT &event : map->events) {
        event.kind = DNG_EVENT_NONE;
        event.reset_flag = 0;
    }

    for (TREASURE_BOX &box : map->boxes) {
        box.used = 0;
        box.lid_angle = 0;
        box.trap_no = 0;
    }

    map->box_num = 0;

    for (ATRA_BOLL &atra : map->atra) {
        atra.used = 0;
    }

    map->atra_num = 0;

    for (ROOM_LINK_RESULT &link : map->room_link) {
        link.used = 0;
    }
}

/**
 * Fills the debug overlay with the player's and the camera's positions.
 */
void DrawViewInformation() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR rot;
    sceVu0FVECTOR cam_pos;
    sceVu0FVECTOR cam_ref;

    DbgMsg.length = std::snprintf(DbgMsg.text, std::size(DbgMsg.text), " ---Infomation---\n");
    sceVu0CopyVector(pos, CharaMain.pos);
    CharaMain.GetRotation(rot);
    NowCamera__3->GetPos(cam_pos);
    NowCamera__3->GetRef(cam_ref);
    const float angle = NowCamera__3->GetAngle();
    const float distance = NowCamera__3->GetDistance();
    const float height = NowCamera__3->GetHeight();

    // Positions relative to the 160-unit floor tile the player stands on.
    const int   tile_x = static_cast<int>((pos[0] - 80.0f) / 160.0f) * 160;
    const float local_x = pos[0] - tile_x - 160.0f;
    const float local_y = pos[1];
    const int   tile_z = static_cast<int>((pos[2] - 80.0f) / 160.0f) * 160;
    const float local_z = pos[2] - tile_z - 160.0f;
    const float cam_local_x = cam_pos[0] - tile_x - 160.0f;
    const float cam_local_y = cam_pos[1];
    const float cam_local_z = cam_pos[2] - tile_z - 160.0f;
    const float ref_local_x = cam_ref[0] - tile_x - 160.0f;
    const float ref_local_y = cam_ref[1];
    const float ref_local_z = cam_ref[2] - tile_z - 160.0f;

    AppendDebugText("gpos %.2f/ %.2f/ %.2f\n", pos[0], pos[1], pos[2]);
    AppendDebugText("lpos %.2f/ %.2f/ %.2f\n", local_x, local_y, local_z);
    AppendDebugText("rot %.2f/ %.2f/ %.2f\n", rot[0], rot[1], rot[2]);
    AppendDebugText("CAM\n");
    AppendDebugText("cpos %.2f/ %.2f/ %.2f\n", cam_pos[0], cam_pos[1], cam_pos[2]);
    AppendDebugText("cref %.2f/ %.2f/ %.2f\n", cam_ref[0], cam_ref[1], cam_ref[2]);
    AppendDebugText("lcpos %.2f/ %.2f/ %.2f\n", cam_local_x, cam_local_y, cam_local_z);
    AppendDebugText("lcref %.2f/ %.2f/ %.2f\n", ref_local_x, ref_local_y, ref_local_z);
    AppendDebugText("Angle = %.3f\n", angle);
    AppendDebugText("Dist = %.3f\n", distance);
    AppendDebugText("Height = %.3f\n", height);
    AppendDebugText("RANDOM MAP CODE = %d\n", NowDngMap->map_seed);
    AppendDebugText("WeaponList Ver 2000/%d\n", VERSION_VOL);
    NowEventMan->SearchDataSlotPos(pos);
    AppendDebugText("EventID = %d\n", NowEventMan->GetDataNum());
    MGFillBox(CRect_i_(0x200, 0x280, 0x1000, 0x780), 8, 8, 8, 0x60);
    DbgMsg.Draw();
}

/**
 * Applies the pad's left, right, L1 and R1 presses to the line the cursor is on.
 */
void AdjustDebugValue() {
    const auto line_count = static_cast<int>(std::ranges::find(DebugInfoCode, -1) - DebugInfoCode.begin());

    if (GamePad.Down(PAD_DOWN)) {
        if (DebugInfoNowCursor == line_count - 1) {
            DebugInfoNowCursor = 0;
        } else {
            DebugInfoNowCursor++;
        }
    }

    if (GamePad.Down(PAD_UP)) {
        if (DebugInfoNowCursor == 0) {
            DebugInfoNowCursor = line_count - 1;
        } else {
            DebugInfoNowCursor--;
        }
    }

    if (GamePad.Down(PAD_RIGHT)) {
        switch (DebugInfoCode[DebugInfoNowCursor]) {
            case DEBUG_INFO_MINIMAP_VIEW:
                ToggleDebugSwitch(DebugStatus[3]);
                break;
            case DEBUG_INFO_COLLISION:
                ToggleDebugSwitch(DebugStatus[5]);
                break;
            case DEBUG_INFO_ITEM_PUT_ZONE:
                ToggleDebugSwitch(DebugStatus[6]);
                break;
            case DEBUG_INFO_VIEW_INFO:
                ToggleDebugSwitch(DebugStatus[4]);
                break;
            case DEBUG_INFO_PARAMETER:
                ToggleDebugSwitch(DebugStatus[10]);
                break;
            case DEBUG_INFO_ULTRA_MAN:
                if (DebugStatus[20] == 2) {
                    DebugStatus[20] = 0;
                } else {
                    DebugStatus[20]++;
                }

                break;
            case DEBUG_INFO_RELOAD_ENEMY:
                if (DebugStatus[14] < 4) {
                    DebugStatus[14]++;
                } else {
                    DebugStatus[14] = 0;
                }

                break;
            case DEBUG_INFO_BGM_PLAY:
                DebugStatus[9]++;

                if (DebugStatus[9] != -1) {
                    SndBgmLoad(DebugStatus[9] * 10 + 100);
                    SndBgmPlay(0);
                } else {
                    SndBgmStop();
                }

                break;
            case DEBUG_INFO_EVENT_TEST:
                DebugStatus[11]++;
                break;
            case DEBUG_INFO_LIGHT_MODE:
                ToggleDebugSwitch(DebugStatus[16]);
                break;
            case DEBUG_INFO_SET_STATUS:
                if (DebugStatus[17] == 5) {
                    DebugStatus[17] = 0;
                } else {
                    DebugStatus[17]++;
                }

                break;
            case DEBUG_INFO_SE_PLAY:
                DebugStatus[18]++;
                break;
            case DEBUG_INFO_SET_CHR_KEY:
                if (DebugStatus[19] >= 5) {
                    DebugStatus[19] = 0;
                } else {
                    DebugStatus[19]++;
                }

                break;
            default:
                break;
        }
    }

    if (GamePad.Down(PAD_LEFT)) {
        switch (DebugInfoCode[DebugInfoNowCursor]) {
            case DEBUG_INFO_MINIMAP_VIEW:
                ToggleDebugSwitch(DebugStatus[3]);
                break;
            case DEBUG_INFO_COLLISION:
                ToggleDebugSwitch(DebugStatus[5]);
                break;
            case DEBUG_INFO_ITEM_PUT_ZONE:
                ToggleDebugSwitch(DebugStatus[6]);
                break;
            case DEBUG_INFO_VIEW_INFO:
                ToggleDebugSwitch(DebugStatus[4]);
                break;
            case DEBUG_INFO_PARAMETER:
                ToggleDebugSwitch(DebugStatus[10]);
                break;
            case DEBUG_INFO_ULTRA_MAN:
                if (DebugStatus[20] == 0) {
                    DebugStatus[20] = 2;
                } else {
                    DebugStatus[20]--;
                }

                break;
            case DEBUG_INFO_RELOAD_ENEMY:
                if (DebugStatus[14] > 0) {
                    DebugStatus[14]--;
                } else {
                    DebugStatus[14] = 4;
                }

                break;
            case DEBUG_INFO_BGM_PLAY:
                if (DebugStatus[9] > -1) {
                    DebugStatus[9]--;
                }

                if (DebugStatus[9] != -1) {
                    SndBgmLoad(DebugStatus[9] * 10 + 100);
                    SndBgmPlay(0);
                } else {
                    SndBgmStop();
                }

                break;
            case DEBUG_INFO_EVENT_TEST:
                if (DebugStatus[11] > 0) {
                    DebugStatus[11]--;
                }

                break;
            case DEBUG_INFO_LIGHT_MODE:
                ToggleDebugSwitch(DebugStatus[16]);
                break;
            case DEBUG_INFO_SET_STATUS:
                if (DebugStatus[17] == 0) {
                    DebugStatus[17] = 5;
                } else {
                    DebugStatus[17]--;
                }

                break;
            case DEBUG_INFO_SE_PLAY:
                DebugStatus[18]--;
                break;
            case DEBUG_INFO_SET_CHR_KEY:
                if (DebugStatus[19] <= 0) {
                    DebugStatus[19] = 5;
                } else {
                    DebugStatus[19]--;
                }

                break;
            default:
                break;
        }
    }

    if (GamePad.Down(PAD_L1)) {
        switch (DebugInfoCode[DebugInfoNowCursor]) {
            case DEBUG_INFO_EVENT_TEST:
                if (DebugStatus[11] > 0) {
                    DebugStatus[11] -= 10;
                }

                DebugStatus[11] = std::max(DebugStatus[11], 0);
                break;
            case DEBUG_INFO_RELOAD_ENEMY:
                if (DebugStatus[15] > 0) {
                    DebugStatus[15]--;
                } else {
                    DebugStatus[15] = 16;
                }

                break;
            case DEBUG_INFO_SE_PLAY:
                if (DebugStatus[18] > 0) {
                    DebugStatus[18] -= 10;
                }

                DebugStatus[18] = std::max(DebugStatus[18], 0);
                break;
            default:
                break;
        }
    }

    if (GamePad.Down(PAD_R1)) {
        switch (DebugInfoCode[DebugInfoNowCursor]) {
            case DEBUG_INFO_EVENT_TEST:
                DebugStatus[11] += 10;
                break;
            case DEBUG_INFO_RELOAD_ENEMY:
                if (DebugStatus[15] < 16) {
                    DebugStatus[15]++;
                } else {
                    DebugStatus[15] = 0;
                }

                break;
            case DEBUG_INFO_SE_PLAY:
                DebugStatus[18] += 10;
                break;
            default:
                break;
        }
    }
}

} // namespace

/**
 * Draws the dungeon debug overlay and its current menu selection.
 */
void DebugInfomationDraw() {
    if (DebugStatus[0] != 1) {
        if (DebugStatus[4] != 0) {
            DrawViewInformation();
        }

        return;
    }

    DbgMsg.length = 0;
    DbgMsg.text[0] = '\0';

    for (int line = 0; DebugInfoMsg[line] != nullptr; line++) {
        const char *format = DebugInfoMsg[line];

        AppendDebugText(DebugInfoNowCursor == line ? ">>" : "  ");

        switch (DebugInfoCode[line]) {
            case DEBUG_INFO_NONE:
                AppendDebugText(format);
                break;
            case DEBUG_INFO_UNK_28:
                AppendDebugText(format, DebugStatus[8]);
                break;
            case DEBUG_INFO_BGM_PLAY:
                if (DebugStatus[9] == -1) {
                    AppendDebugText(format, "OFF");
                } else {
                    const std::string bgm_text = std::to_string(DebugStatus[9] + 1);
                    AppendDebugText(format, bgm_text.c_str());
                }

                break;
            case DEBUG_INFO_PARAMETER:
                AppendDebugText(format, on_off_names[DebugStatus[10]]);
                break;
            case DEBUG_INFO_COLLISION:
                AppendDebugText(format, on_off_names[DebugStatus[5]]);
                break;
            case DEBUG_INFO_VIEW_INFO:
                AppendDebugText(format, on_off_names[DebugStatus[4]]);
                break;
            case DEBUG_INFO_ITEM_PUT_ZONE:
                AppendDebugText(format, on_off_names[DebugStatus[6]]);
                break;
            case DEBUG_INFO_MINIMAP_VIEW:
                AppendDebugText(format, on_off_names[DebugStatus[3]]);
                break;
            case DEBUG_INFO_ULTRA_MAN:
                AppendDebugText(format, power_names[DebugStatus[20]]);
                break;
            case DEBUG_INFO_RELOAD_ENEMY:
                AppendDebugText(format, DebugStatus[14], DebugStatus[15]);
                break;
            case DEBUG_INFO_FLOOR_ATRA_GET:
                AppendDebugText(format);
                break;
            case DEBUG_INFO_LIGHT_MODE:
                AppendDebugText(format, main_sub_names[DebugStatus[16]]);
                break;
            case DEBUG_INFO_EVENT_TEST:
                AppendDebugText(format, DebugStatus[11]);
                break;
            case DEBUG_INFO_SET_STATUS:
                AppendDebugText(format, condition_names[DebugStatus[17]]);
                break;
            case DEBUG_INFO_SE_PLAY:
                AppendDebugText(format, DebugStatus[18]);
                break;
            case DEBUG_INFO_SET_CHR_KEY:
                AppendDebugText(format, DebugStatus[19]);
                break;
            default:
                break;
        }
    }

    MGFillBox(CRect_i_(0x200, 0x280, 0x1000, 0x780), 8, 8, 8, 0x60);
    DbgMsg.Draw();
}

/**
 * Clears the debug overlay's state.
 */
void DebugInfomationInit() {
    DebugStatus[0] = 0;
    DebugStatus[1] = 0;
    DebugStatus[2] = 0;
    DebugStatus[3] = 0;
    DebugStatus[5] = 1;
    DebugStatus[6] = 0;
    DebugStatus[4] = 0;
    DebugStatus[7] = 0;
    DebugStatus[8] = 150;
    DebugStatus[9] = -1;
    DebugStatus[10] = 0;
    DebugStatus[11] = 100;
    DebugStatus[12] = 0;
    DebugStatus[13] = 16;
    DebugStatus[14] = 0;
    DebugStatus[15] = 16;
    DebugStatus[16] = 0;
    DebugStatus[17] = 0;
    DebugStatus[18] = 400;
    DebugStatus[19] = 1;
    DebugStatus[20] = 0;
}

/**
 * Moves through the debug overlay's pages with the pad.
 */
int DebugInfomationIF() {
    if (GamePad.Down(PAD_R3)) {
        CloseDebugOverlay();
        return DEBUG_INFO_CLOSE;
    }

    if (GamePad.Down(PAD_TRIANGLE) && DebugInfoCode[DebugInfoNowCursor] == DEBUG_INFO_EVENT_TEST) {
        DebugStatus[12] = 1;
        CloseDebugOverlay();
        return DEBUG_INFO_UNK_28;
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        switch (DebugInfoCode[DebugInfoNowCursor]) {
            case DEBUG_INFO_UNK_28:
                CloseDebugOverlay();
                return DEBUG_INFO_UNK_28;
            case DEBUG_INFO_UNK_3C:
                CloseDebugOverlay();
                return DEBUG_INFO_UNK_3C;
            case DEBUG_INFO_RELOAD_ENEMY:
                CloseDebugOverlay();
                return DEBUG_INFO_RELOAD_ENEMY;
            case DEBUG_INFO_FLOOR_ATRA_GET:
                CloseDebugOverlay();
                return DEBUG_INFO_FLOOR_ATRA_GET;
            case DEBUG_INFO_EVENT_TEST:
                DebugStatus[12] = 0;
                CloseDebugOverlay();
                return DEBUG_INFO_EVENT_TEST;
            case DEBUG_INFO_SET_STATUS:
                CloseDebugOverlay();
                return DEBUG_INFO_SET_STATUS;
            case DEBUG_INFO_SE_PLAY:
                SndSePlay(DebugStatus[18], -1, 0);
                break;
            case DEBUG_INFO_SET_CHR_KEY:
                CloseDebugOverlay();
                ClearEventData(NowEventMan);
                std::println("trap num = {}", NowDngMap->SetCharaDoor(DebugStatus[19]));
                ClearMapEvent(NowDngMap);
                NowEventMan->SetupEvent(NowDngMap, NowDngMap->map_type);
                return DEBUG_INFO_SET_CHR_KEY;
            default:
                break;
        }
    }

    if (DebugStatus[2] == 0) {
        AdjustDebugValue();
    }

    return DEBUG_INFO_NONE;
}

void StartMessageDraw(CTexture *texture, int dungeon, int floor, int ura, int alpha) {
    const std::array<int, 7> &centers = *center_ptr[LanguageCode];
    int                       x = centers[dungeon] + 0x82;

    set2DSprite(Vif1Packet, texture, CRect_i_(0x82, 0xAA, 0x17C, 0x32), CRect_i_(0, 0, 0x17C, 0x32), alpha);

    floor++;

    if (dungeon == DUNGEON_GALLERY_OF_TIME) {
        floor = BtGetFloorLevel(floor - 1);
    }

    if (floor < 10) {
        x -= 0x13;
        const int ones_x = floor % 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x, 0xAA, 0x26, 0x32), CRect_i_(ones_x, 0x32, 0x26, 0x32), alpha);
    }

    if (floor >= 10 && floor < 100) {
        x -= 0x26;
        const int tens_x = floor / 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x, 0xAA, 0x26, 0x32), CRect_i_(tens_x, 0x32, 0x26, 0x32), alpha);
        const int ones_x = floor % 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x26, 0xAA, 0x26, 0x32), CRect_i_(ones_x, 0x32, 0x26, 0x32), alpha);
    }

    if (floor >= 100) {
        const int hundreds = floor / 100;
        const int hundreds_x = hundreds * 0x26;

        floor -= hundreds * 100;
        set2DSprite(Vif1Packet, texture, CRect_i_(x - 0x39, 0xAA, 0x26, 0x32), CRect_i_(hundreds_x, 0x32, 0x26, 0x32), alpha);
        const int tens_x = floor / 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x - 0x13, 0xAA, 0x26, 0x32), CRect_i_(tens_x, 0x32, 0x26, 0x32), alpha);
        const int ones_x = floor % 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x13, 0xAA, 0x26, 0x32), CRect_i_(ones_x, 0x32, 0x26, 0x32), alpha);
    }

    if (ura != 0) {
        set2DSprite(Vif1Packet, texture, CRect_i_(0x10E, 0xE6, 0x64, 0x32), CRect_i_(0x114, 0x7C, 0x64, 0x32), alpha);
        return;
    }

    if (UserStatus->res_limit_zone_current >= 0) {
        set2DSprite(Vif1Packet, texture, CRect_i_(0xC8, 0xE6, 0xF0, 0x32), CRect_i_(0, 0x7C, 0xF0, 0x32), alpha);
    }
}

/**
 * Empties the list of key items waiting to be dropped.
 */
void ClearGateKeyStack() { std::ranges::fill(gateKeyStack, -1); }

int SetGateKeyStack(int item) {
    if (item == -1) {
        return 0;
    }

    // One of a key item is enough, so a second is refused rather than stacked.
    if (std::ranges::contains(gateKeyStack, item)) {
        return 0;
    }

    for (int &slot : gateKeyStack) {
        if (slot == -1) {
            slot = item;
            return 1;
        }
    }

    return 0;
}

char *NameExchg(char *name, [[maybe_unused]] int language) {
    nameblock = LanguageStr[LanguageCode];
    nameblock += name;
    return nameblock.data();
}

int CCollisionData::Set(float *position, int damage, int life, float radius, float scale_rate, int target_mask, int kind, int flags, int attribute) {
    for (int i = 0; i < 96; i++) {
        if (active[i] != 0) {
            continue;
        }

        COLLISION_HIT &record = hit[i];

        active[i] = 1;
        sceVu0CopyVector(record.pos, position);
        record.base_point[0] = record.base_point[1] = record.base_point[2] = 0.0f;
        record.base_point[3] = 1.0f;
        record.velocity[0] = record.velocity[1] = record.velocity[2] = 0.0f;
        record.velocity[0] = 1.0f;
        record.damage = damage;
        record.scale_rate = scale_rate;
        record.life = life;
        record.radius = radius;
        record.target_mask = target_mask;
        record.kind = kind;
        record.flags = flags;
        record.attribute = attribute;
        record.owner = -1;
        record.monster_no = -1;
        record.attack_no = -1;
        record.vs_monster = nullptr;
        record.weapon_flags = 1;
        record.target_kind = -1;
        record.knockback_origin[0] = record.knockback_origin[1] = record.knockback_origin[2] = 0.0f;
        record.knockback_origin[3] = 1.0f;
        record.knockback_speed = 0.0f;
        record.knockback_decay = 0.0f;
        record.knockback_mode = 0;
        record.phase = record.ready_phase = 0;
        now_hit = i;
        return i;
    }

    return -1;
}

int CCollisionData::CheckHitUser(float *position, int mask, float height) {
    sceVu0FVECTOR user_position;
    sceVu0FVECTOR hit_position;

    sceVu0CopyVector(user_position, position);
    user_position[1] = 0.0f;

    const float user_bottom = position[1];
    const float user_top = user_bottom + height;

    for (int i = 0; i < 96; i++) {
        if (active[i] == 0 || (mask & hit[i].target_mask) == 0 || hit[i].phase != hit[i].ready_phase) {
            continue;
        }

        sceVu0CopyVector(hit_position, hit[i].pos);
        hit_position[1] = 0.0f;

        if (DistVector(user_position, hit_position) > hit[i].radius) {
            continue;
        }

        const float hit_top = hit[i].pos[1] + hit[i].radius;
        const float hit_bottom = hit[i].pos[1] - hit[i].radius;
        bool        overlaps = false;

        // The negated comparisons are deliberate: they decide how NaN heights resolve.
        if (!(hit_top < user_top) && hit_bottom < user_top) {
            overlaps = true;
        }

        if (!(hit_top < user_bottom) && hit_bottom < user_bottom) {
            overlaps = true;
        }

        if (hit_top <= user_top && !(hit_bottom <= user_bottom)) {
            overlaps = true;
        }

        if (!(hit_top < user_top) && hit_bottom < user_bottom) {
            overlaps = true;
        }

        if (overlaps) {
            return i;
        }
    }

    return -1;
}

void CCollisionData::SetKickBack(float *origin, float speed, float decay, int mode) {
    COLLISION_HIT &record = hit[now_hit];

    record.knockback_origin[0] = origin[0];
    record.knockback_origin[1] = origin[1];
    record.knockback_origin[2] = origin[2];
    record.knockback_speed = speed;
    record.knockback_decay = decay;
    record.knockback_mode = mode;
}

} // namespace dc::game
