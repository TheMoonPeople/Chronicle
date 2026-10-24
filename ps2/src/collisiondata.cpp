#include "collisiondata.hpp"

#include <cstdio>
#include <cstring>

#include "camerafollow.hpp"
#include "debugfont.hpp"
#include "dngmessageman.hpp"
#include "dun/gameloop.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "gamepad.hpp"
#include "hitmark.hpp"
#include "itemdata.hpp"
#ifdef PAL
#include "mainselect.hpp"
#endif
#include "mglib.hpp"
#include "rect.hpp"
#include "snd.hpp"
#ifdef PAL
#include "userstatus.hpp"
#endif

/**
 * The action code of each debug overlay line, ended by -1.
 */
int DebugInfoCode[15] = {DEBUG_INFO_MINIMAP_VIEW, DEBUG_INFO_COLLISION, DEBUG_INFO_BGM_PLAY, DEBUG_INFO_PARAMETER, DEBUG_INFO_VIEW_INFO, DEBUG_INFO_ULTRA_MAN, DEBUG_INFO_RELOAD_ENEMY, DEBUG_INFO_ITEM_PUT_ZONE, DEBUG_INFO_LIGHT_MODE, DEBUG_INFO_FLOOR_ATRA_GET, DEBUG_INFO_EVENT_TEST, DEBUG_INFO_SET_STATUS, DEBUG_INFO_SE_PLAY, DEBUG_INFO_SET_CHR_KEY, -1};

// clang-format off
/**
 * The format of each debug overlay line, ended by a null entry.
 */
char *DebugInfoMsg[15] = {
    DebugInfoMsgMiniMapView, DebugInfoMsgCollision,  DebugInfoMsgBgmPlay,     DebugInfoMsgParameter,
    DebugInfoMsgViewInfo,    DebugInfoMsgUltraMan,   DebugInfoMsgReloadEnemy, DebugInfoMsgItemPutZone,
    DebugInfoMsgLightMode,   DebugInfoMsgFloorAtraGet, DebugInfoMsgEventTest, DebugInfoMsgSetStatus,
    DebugInfoMsgSePlay,      DebugInfoMsgSetChrKey,  NULL,
};
// clang-format on

/**
 * The debug overlay line the cursor is on.
 */
int DebugInfoNowCursor;

/** Key items waiting to be dropped, one entry each, -1 where a slot is free. */
int gateKeyStack[32];

/**
 * The path NameExchg builds.
 */
char nameblock[64];

/**
 * Draws the dungeon debug overlay and its current menu selection.
 *
 * @mangled DebugInfomationDraw__Fv
 * @address 0x1B3780
 * @size 0xF70
 */
void DebugInfomationDraw() {
    if (DebugStatus[0] != 1) {
        if (DebugStatus[4] != 0) {
            sceVu0FVECTOR      pos;
            sceVu0FVECTOR      rot;
            sceVu0FVECTOR      cam_pos;
            sceVu0FVECTOR      cam_ref;
            float              angle;
            float              distance;
            float              height;
            int                tile_x;
            float              local_x;
            int                tile_z;
            float              local_z;
            float              cam_local_x;
            float              cam_local_y;
            float              cam_local_z;
            float              ref_local_x;
            float              ref_local_y;
            float              local_y;
            float              ref_local_z;
            CDungeonEventData *event_data;
            int                event_count;

            DbgMsg.length = sprintf(DbgMsg.text, " ---Infomation---\n");
            sceVu0CopyVector(pos, CharaMain.pos);
            CharaMain.GetRotation(rot);
            NowCamera__3->GetPos(cam_pos);
            NowCamera__3->GetRef(cam_ref);
            angle = NowCamera__3->GetAngle();
            distance = NowCamera__3->GetDistance();
            height = NowCamera__3->GetHeight();
            tile_x = (int) ((pos[0] - 80.0f) / 160.0f) * 160;
            local_x = pos[0] - tile_x - 160.0f;
            local_y = pos[1];
            tile_z = (int) ((pos[2] - 80.0f) / 160.0f) * 160;
            local_z = pos[2] - tile_z - 160.0f;
            cam_local_x = cam_pos[0] - tile_x - 160.0f;
            cam_local_y = cam_pos[1];
            cam_local_z = cam_pos[2] - tile_z - 160.0f;
            ref_local_x = cam_ref[0] - tile_x - 160.0f;
            ref_local_y = cam_ref[1];
            ref_local_z = cam_ref[2] - tile_z - 160.0f;
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "gpos %.2f/ %.2f/ %.2f\n", pos[0], pos[1], pos[2]);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "lpos %.2f/ %.2f/ %.2f\n", local_x, local_y, local_z);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "rot %.2f/ %.2f/ %.2f\n", rot[0], rot[1], rot[2]);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "CAM\n");
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "cpos %.2f/ %.2f/ %.2f\n", cam_pos[0], cam_pos[1], cam_pos[2]);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "cref %.2f/ %.2f/ %.2f\n", cam_ref[0], cam_ref[1], cam_ref[2]);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "lcpos %.2f/ %.2f/ %.2f\n", cam_local_x, cam_local_y, cam_local_z);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "lcref %.2f/ %.2f/ %.2f\n", ref_local_x, ref_local_y, ref_local_z);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "Angle = %.3f\n", angle);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "Dist = %.3f\n", distance);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "Height = %.3f\n", height);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "RANDOM MAP CODE = %d\n", NowDngMap->map_seed);
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "WeaponList Ver 2000/%d\n", VERSION_VOL);
            event_data = NowEventMan->SearchDataSlotPos(pos);
            event_count = 0;

            if (event_data != NULL) {
                event_count = 1;
            }

            event_count = NowEventMan->GetDataNum();
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "EventID = %d\n", event_count);
#ifdef PAL
            MGFillBox(CRect_i_(0x200, 0x280, 0x1000, 0x780), 8, 8, 8, 0x60);
#else
            MGFillBox(CRect_i_(0x200, 0x280, 0x1000, 0x700), 8, 8, 8, 0x60);
#endif
            DbgMsg.Draw();
        }

        return;
    }

    DbgMsg.length = sprintf(DbgMsg.text, "");
    char *on_off_names[2] = {"OFF", "ON"};
    char *main_sub_names[2] = {"MAIN", "SUB"};
    char *condition_names[6] = {"RESET", "STONE", "BIN2", "POISON", "CURSE", "NEBA2"};
    char *power_names[3] = {"HUMAN   ", "SUPERMAN", "ULTRAMAN"};
    char  bgm_text[32];

    for (int i = 0; DebugInfoMsg[i] != NULL; i++) {
        if (DebugInfoNowCursor == i) {
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], ">>");
        } else {
            DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], "  ");
        }

        switch (DebugInfoCode[i]) {
            case DEBUG_INFO_NONE:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i]);
                break;
            case DEBUG_INFO_UNK_28:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], DebugStatus[8]);
                break;
            case DEBUG_INFO_BGM_PLAY:
                if (DebugStatus[9] == -1) {
                    DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], "OFF");
                } else {
                    sprintf(bgm_text, "%d", DebugStatus[9] + 1);
                    DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], bgm_text);
                }

                break;
            case DEBUG_INFO_PARAMETER:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], on_off_names[DebugStatus[10]]);
                break;
            case DEBUG_INFO_COLLISION:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], on_off_names[DebugStatus[5]]);
                break;
            case DEBUG_INFO_VIEW_INFO:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], on_off_names[DebugStatus[4]]);
                break;
            case DEBUG_INFO_ITEM_PUT_ZONE:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], on_off_names[DebugStatus[6]]);
                break;
            case DEBUG_INFO_MINIMAP_VIEW:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], on_off_names[DebugStatus[3]]);
                break;
            case DEBUG_INFO_ULTRA_MAN:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], power_names[DebugStatus[20]]);
                break;
            case DEBUG_INFO_RELOAD_ENEMY:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], DebugStatus[14], DebugStatus[15]);
                break;
            case DEBUG_INFO_FLOOR_ATRA_GET:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i]);
                break;
            case DEBUG_INFO_LIGHT_MODE:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], main_sub_names[DebugStatus[16]]);
                break;
            case DEBUG_INFO_EVENT_TEST:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], DebugStatus[11]);
                break;
            case DEBUG_INFO_SET_STATUS:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], condition_names[DebugStatus[17]]);
                break;
            case DEBUG_INFO_SE_PLAY:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], DebugStatus[18]);
                break;
            case DEBUG_INFO_SET_CHR_KEY:
                DbgMsg.length += sprintf(&DbgMsg.text[DbgMsg.length], DebugInfoMsg[i], DebugStatus[19]);
                break;
        }
    }

#ifdef PAL
    MGFillBox(CRect_i_(0x200, 0x280, 0x1000, 0x780), 8, 8, 8, 0x60);
#else
    MGFillBox(CRect_i_(0x200, 0x280, 0x1000, 0x700), 8, 8, 8, 0x60);
#endif
    DbgMsg.Draw();
}

/**
 * Clears the debug overlay's state.
 *
 * @mangled DebugInfomationInit__Fv
 * @address 0x1B46F0
 * @size 0xC8
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
 * Resets every runtime event record of the event manager to an idle, unheld state.
 */
static inline void ClearEventData(CDungeonEventMan *event_man) {
    int i;

    for (i = 0; i < 96; i++) {
        CDungeonEventData *data = &event_man->event[i];

        data->event = NULL;
        data->switch_on = false;
        data->enabled = false;
        data->hold = 0;
        data->chara_done = -1;
    }
}

/**
 * Marks the floor's event places, treasure boxes, atla balls and room links unused and resets their counts.
 */
static inline void ClearMapEvent(CDungeonMap *map) {
    int i;

    for (i = 0; i < 48; i++) {
        map->events[i].kind = DNG_EVENT_NONE;
        map->events[i].reset_flag = 0;
    }

    for (i = 0; i < 24; i++) {
        map->boxes[i].used = 0;
        map->boxes[i].lid_angle = 0;
        map->boxes[i].trap_no = 0;
    }

    map->box_num = 0;

    for (i = 0; i < 8; i++) {
        map->atra[i].used = 0;
    }

    map->atra_num = 0;

    for (i = 0; i < 4; i++) {
        map->room_link[i].used = 0;
    }
}

/**
 * Moves through the debug overlay's pages with the pad.
 *
 * @mangled DebugInfomationIF__Fv
 * @address 0x1B47C0
 * @size 0xE78
 */
int DebugInfomationIF() {
    int line_count;

    if (GamePad.Down(PAD_R3)) {
        DebugStatus[0] = 0;
        GamePad.AutoRepeatOff();
        GamePad.MenuModeOff();
        return DEBUG_INFO_CLOSE;
    }

    if (GamePad.Down(PAD_TRIANGLE)) {
        switch (DebugInfoCode[DebugInfoNowCursor]) {
            case DEBUG_INFO_EVENT_TEST:
                DebugStatus[0] = 0;
                DebugStatus[12] = 1;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                return DEBUG_INFO_UNK_28;
        }
    }

    if (GamePad.Down(PAD_CIRCLE)) {
        switch (DebugInfoCode[DebugInfoNowCursor]) {
            case DEBUG_INFO_UNK_28:
                DebugStatus[0] = 0;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                return DEBUG_INFO_UNK_28;
            case DEBUG_INFO_UNK_3C:
                DebugStatus[0] = 0;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                return DEBUG_INFO_UNK_3C;
            case DEBUG_INFO_RELOAD_ENEMY:
                DebugStatus[0] = 0;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                return DEBUG_INFO_RELOAD_ENEMY;
            case DEBUG_INFO_FLOOR_ATRA_GET:
                DebugStatus[0] = 0;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                return DEBUG_INFO_FLOOR_ATRA_GET;
            case DEBUG_INFO_EVENT_TEST:
                DebugStatus[0] = 0;
                DebugStatus[12] = 0;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                return DEBUG_INFO_EVENT_TEST;
            case DEBUG_INFO_SET_STATUS:
                DebugStatus[0] = 0;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                return DEBUG_INFO_SET_STATUS;
            case DEBUG_INFO_SE_PLAY:
                SndSePlay(DebugStatus[18], -1, 0);
                break;
            case DEBUG_INFO_SET_CHR_KEY:
                DebugStatus[0] = 0;
                GamePad.AutoRepeatOff();
                GamePad.MenuModeOff();
                ClearEventData(NowEventMan);
                printf("trap num = %d\n", NowDngMap->SetCharaDoor(DebugStatus[19]));
                ClearMapEvent(NowDngMap);
                {
                    int mode = NowDngMap->map_type;

                    NowEventMan->SetupEvent(NowDngMap, mode);
                }
                return DEBUG_INFO_SET_CHR_KEY;
        }
    }

    for (line_count = 0; DebugInfoCode[line_count] != -1; line_count++) {
    }

    switch (DebugStatus[2]) {
        case 0:
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
                        if (DebugStatus[3] == 0) {
                            DebugStatus[3] = 1;
                        } else {
                            DebugStatus[3] = 0;
                        }

                        break;
                    case DEBUG_INFO_COLLISION:
                        if (DebugStatus[5] == 0) {
                            DebugStatus[5] = 1;
                        } else {
                            DebugStatus[5] = 0;
                        }

                        break;
                    case DEBUG_INFO_ITEM_PUT_ZONE:
                        if (DebugStatus[6] == 0) {
                            DebugStatus[6] = 1;
                        } else {
                            DebugStatus[6] = 0;
                        }

                        break;
                    case DEBUG_INFO_VIEW_INFO:
                        if (DebugStatus[4] == 0) {
                            DebugStatus[4] = 1;
                        } else {
                            DebugStatus[4] = 0;
                        }

                        break;
                    case DEBUG_INFO_PARAMETER:
                        if (DebugStatus[10] == 0) {
                            DebugStatus[10] = 1;
                        } else {
                            DebugStatus[10] = 0;
                        }

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
                        if (DebugStatus[16] == 0) {
                            DebugStatus[16] = 1;
                        } else {
                            DebugStatus[16] = 0;
                        }

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
                }
            }

            if (GamePad.Down(PAD_LEFT)) {
                switch (DebugInfoCode[DebugInfoNowCursor]) {
                    case DEBUG_INFO_MINIMAP_VIEW:
                        if (DebugStatus[3] == 0) {
                            DebugStatus[3] = 1;
                        } else {
                            DebugStatus[3] = 0;
                        }

                        break;
                    case DEBUG_INFO_COLLISION:
                        if (DebugStatus[5] == 0) {
                            DebugStatus[5] = 1;
                        } else {
                            DebugStatus[5] = 0;
                        }

                        break;
                    case DEBUG_INFO_ITEM_PUT_ZONE:
                        if (DebugStatus[6] == 0) {
                            DebugStatus[6] = 1;
                        } else {
                            DebugStatus[6] = 0;
                        }

                        break;
                    case DEBUG_INFO_VIEW_INFO:
                        if (DebugStatus[4] == 0) {
                            DebugStatus[4] = 1;
                        } else {
                            DebugStatus[4] = 0;
                        }

                        break;
                    case DEBUG_INFO_PARAMETER:
                        if (DebugStatus[10] == 0) {
                            DebugStatus[10] = 1;
                        } else {
                            DebugStatus[10] = 0;
                        }

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
                        if (DebugStatus[16] == 0) {
                            DebugStatus[16] = 1;
                        } else {
                            DebugStatus[16] = 0;
                        }

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
                }
            }

            if (GamePad.Down(PAD_L1)) {
                switch (DebugInfoCode[DebugInfoNowCursor]) {
                    case DEBUG_INFO_EVENT_TEST:
                        if (DebugStatus[11] > 0) {
                            DebugStatus[11] -= 10;
                        }

                        if (DebugStatus[11] < 0) {
                            DebugStatus[11] = 0;
                        }

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

                        if (DebugStatus[18] < 0) {
                            DebugStatus[18] = 0;
                        }

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
                }
            }

            break;
    }

    return DEBUG_INFO_NONE;
}

#ifdef PAL
/**
 * Where the English caption centres each dungeon's floor number.
 */
int center_us[7] = {0xCE, 0x125, 0x101, 0x100, 0x87, 0x57, 0x101};

/**
 * Where the French caption centres each dungeon's floor number.
 */
int center_fr[7] = {0xCD, 0x113, 0xF8, 0xF8, 0xB2, 0x59, 0xFF};

/**
 * Where the German caption centres each dungeon's floor number.
 */
int center_gr[7] = {0xFE, 0x103, 0x13D, 0xFD, 0x78, 0x38, 0xFF};

/**
 * Where the Italian caption centres each dungeon's floor number.
 */
int center_it[7] = {0xCC, 0x129, 0xF3, 0xFF, 0xFA, 0x75, 0xF9};

/**
 * Where the Spanish caption centres each dungeon's floor number.
 */
int center_sp[7] = {0xCE, 0x12C, 0xF2, 0xFA, 0x7C, 0xBA, 0xFA};

/**
 * Each language's floor-number centres.
 */
int *center_ptr[7] = {center_sp, center_us, center_us, center_fr, center_gr, center_it, center_sp};

void StartMessageDraw(CTexture *texture, int dungeon, int floor, int ura, int alpha) {
    int *centers = center_ptr[LanguageCode];
    int  x = centers[dungeon] + 0x82;
    int  digit_x;

    set2DSprite(Vif1Packet, texture, CRect_i_(0x82, 0xAA, 0x17C, 0x32), CRect_i_(0, 0, 0x17C, 0x32), alpha);

    floor++;

    if (dungeon == DUNGEON_GALLERY_OF_TIME) {
        floor = BtGetFloorLevel(floor - 1);
    }

    if (floor < 10) {
        x -= 0x13;
        digit_x = floor % 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), alpha);
    }

    if (floor >= 10 && floor < 100) {
        x -= 0x26;
        digit_x = floor / 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), alpha);
        digit_x = floor % 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x26, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), alpha);
    }

    if (floor >= 100) {
        int digit = floor / 100;

        digit_x = digit * 0x26;
        floor -= digit * 100;
        set2DSprite(Vif1Packet, texture, CRect_i_(x - 0x39, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), alpha);
        digit_x = floor / 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x - 0x13, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), alpha);
        digit_x = floor % 10 * 0x26;
        set2DSprite(Vif1Packet, texture, CRect_i_(x + 0x13, 0xAA, 0x26, 0x32), CRect_i_(digit_x, 0x32, 0x26, 0x32), alpha);
    }

    if (ura != 0) {
        set2DSprite(Vif1Packet, texture, CRect_i_(0x10E, 0xE6, 0x64, 0x32), CRect_i_(0x114, 0x7C, 0x64, 0x32), alpha);
        return;
    }

    if (UserStatus->res_limit_zone_current >= 0) {
        set2DSprite(Vif1Packet, texture, CRect_i_(0xC8, 0xE6, 0xF0, 0x32), CRect_i_(0, 0x7C, 0xF0, 0x32), alpha);
    }
}
#endif

/**
 * Empties the list of key items waiting to be dropped.
 *
 * @mangled ClearGateKeyStack__Fv
 * @address 0x1B5640
 * @size 0x3C
 */
void ClearGateKeyStack() {
    for (int i = 0; i < 32; i++) {
        gateKeyStack[i] = -1;
    }
}

int SetGateKeyStack(int item) {
    if (item == -1) {
        return 0;
    }

    // One of a key item is enough, so a second is refused rather than stacked.
    for (int i = 0; i < 32; i++) {
        if (item == gateKeyStack[i]) {
            return 0;
        }
    }

    for (int i = 0; i < 32; i++) {
        if (gateKeyStack[i] == -1) {
            gateKeyStack[i] = item;
            return 1;
        }
    }

    return 0;
}

/**
 * Builds a resource path by putting one of the fixed prefixes before a name.
 *
 * @mangled NameExchg__FPci
 * @address 0x1B5740
 * @size 0x60
 */
char *NameExchg(char *name, int language) {
#ifdef PAL
    strcpy(nameblock, LanguageStr[LanguageCode]);
#else
    strcpy(nameblock, LanguageStr[language][1]);
#endif
    strcat(nameblock, name);
    return nameblock;
}

int CCollisionData::Set(float *position, int damage, int life, float radius, float scale_rate, int target_mask, int kind, int flags, int attribute) {
    for (int i = 0; i < 96; i++) {
        if (active[i] == 0) {
            active[i] = 1;
            sceVu0CopyVector(hit[i].pos, position);
            hit[i].base_point[0] = hit[i].base_point[1] = hit[i].base_point[2] = 0.0f;
            hit[i].base_point[3] = 1.0f;
            hit[i].velocity[0] = hit[i].velocity[1] = hit[i].velocity[2] = 0.0f;
            hit[i].velocity[0] = 1.0f;
            hit[i].damage = damage;
            hit[i].scale_rate = scale_rate;
            hit[i].life = life;
            hit[i].radius = radius;
            hit[i].target_mask = target_mask;
            hit[i].kind = kind;
            hit[i].flags = flags;
            hit[i].attribute = attribute;
            hit[i].owner = -1;
            hit[i].monster_no = -1;
            hit[i].attack_no = -1;
            hit[i].vs_monster = NULL;
            hit[i].weapon_flags = 1;
            hit[i].target_kind = -1;
            hit[i].knockback_origin[0] = hit[i].knockback_origin[1] = hit[i].knockback_origin[2] = 0.0f;
            hit[i].knockback_origin[3] = 1.0f;
            hit[i].knockback_speed = 0.0f;
            hit[i].knockback_decay = 0.0f;
            hit[i].knockback_mode = 0;
            hit[i].phase = hit[i].ready_phase = 0;
            now_hit = i;
            return i;
        }
    }
    // With every record in use nothing is set and no index is returned.
    PORT_RETURN(-1);
}

/**
 * Reports which recorded hit reaches the player.
 *
 * @mangled CheckHitUser__14CCollisionDataFPfif
 * @address 0x1B5920
 * @size 0x1BC
 */
int CCollisionData::CheckHitUser(float *position, int mask, float height) {
    sceVu0FVECTOR user_position;
    sceVu0FVECTOR hit_position;
    sceVu0CopyVector(user_position, position);
    user_position[1] = 0.0f;
    float user_top;
    float user_bottom = position[1];
    user_top = user_bottom + height;

    for (int i = 0; i < 96; i++) {
        if (active[i] == 0 || (mask & hit[i].target_mask) == 0 || hit[i].phase != hit[i].ready_phase) {
            continue;
        }

        sceVu0CopyVector(hit_position, hit[i].pos);
        hit_position[1] = 0.0f;

        if (DistVector(user_position, hit_position) > hit[i].radius) {
            continue;
        }

        int   miss = 1;
        float hit_top = hit[i].pos[1] + hit[i].radius;
        float hit_bottom = hit[i].pos[1] - hit[i].radius;

        if (!(hit_top < user_top) && hit_bottom < user_top) {
            miss = 0;
        }

        if (!(hit_top < user_bottom) && hit_bottom < user_bottom) {
            miss = 0;
        }

        if (hit_top <= user_top && !(hit_bottom <= user_bottom)) {
            miss = 0;
        }

        if (!(hit_top < user_top) && hit_bottom < user_bottom) {
            miss = 0;
        }

        if (miss == 0) {
            return i;
        }
    }

    return -1;
}

/**
 * Records the push a hit gives whatever it struck.
 *
 * @mangled SetKickBack__14CCollisionDataFPfffi
 * @address 0x1B5AE0
 * @size 0xA4
 */
void CCollisionData::SetKickBack(float *origin, float speed, float decay, int mode) {
    hit[now_hit].knockback_origin[0] = origin[0];
    hit[now_hit].knockback_origin[1] = origin[1];
    hit[now_hit].knockback_origin[2] = origin[2];
    hit[now_hit].knockback_speed = speed;
    hit[now_hit].knockback_decay = decay;
    hit[now_hit].knockback_mode = mode;
}
