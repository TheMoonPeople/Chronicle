#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "camera.hpp"
#include "camera_port.hpp"
#include "camera_zoom.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "debugfont.hpp"
#include "dialog_npc.hpp"
#include "dngstatusdata.hpp"
#include "draw2d_port.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "edit_in.hpp"
#include "editarea.hpp"
#include "editground.hpp"
#include "editground_port.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editmapscript.hpp"
#include "editmenu.hpp"
#include "editpartsinfo.hpp"
#include "effect.hpp"
#include "effectgroup.hpp"
#include "effectmacro.hpp"
#include "eparts_port.hpp"
#include "fishing.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "itemdata.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "nowload.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "objectframe.hpp"
#include "platform/arena.hpp"
#include "platform/input.hpp"
#include "rect.hpp"
#include "runeffect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "vutext.hpp"
#include "weaponlevelup.hpp"
#include "wind.hpp"

/* Town-script parsing, map construction and pre-event editor state. */

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

void BtSetMapJumpFloor(int floor);

namespace {
void EditMainDraw();
}

extern int           start_event_no;
extern CCameraFollow EventCamera;
extern int           start_system_event;
void                 EdSetFlag();
void                 EditExit();
void                 LoadScript();
void                 InitWorkBuffer();
int                  FadeOutToEvent(int event_no, int level);
void                 CheckKeyLock();
void                 PauseOffCheck();
int                  cat_end();
extern CTextureAnime TexAnime;

/* editloop's own functions, in the order the unit defines them; the prototypes
 * let each call ahead of its definition. */
void                CopyCMapParts(CMapParts *to, CMapParts *from, CDataAlloc2<1> *arena);
EPARTS_INFO_HEADER *LoadPTS(CMapParts *parts, unsigned int *archive, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime, EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points, CMapParts *shared);
void                LoadPTS(CMapParts *parts, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime, EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points);
u_int              *SearchPTS(u_int *archive, char *name);
int                 LoadEditMapData(EDIT_MAP_INFO *info, char *name, int kind);
void                LoadObjectParts();
void                LoadGroundData();
int                 LoadTexture();
int                 CheckEditToWalk(float *position);
int                 GotoInterior(char *name, int entrance, int direction, ED_EVENT_PARAM *param, int start_event);
void                MoveEditCursor();
void                VillagerCollision();
void                MoveChara();
void                MainEditMode();
void                OpenDoorMode();
void                TalkMode();
void                EventMode();
void                EditPartsObjectOnOff();
void                MainMode();
void                EditMode();
void                PlayAmbient(float volume);

void EBDraw();

namespace {
/**
 * The ground the player has built on one map, as the save holds it.
 */
struct ED_GRD_DATA {
    u8  unk_00[0x64];
    int complete_event_seen; /**< Zero until the map's own build event has been seen. */
};

/**
 * Starts a map event and copies an optional source camera into the event camera.
 */
int RunEvent(int event_no, CCamera *camera) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;

    if (start_event_no > 0 && simple_event == 0) {
        return 0;
    }

    if (camera != nullptr) {
        camera->GetPos(position);
        camera->GetRef(reference);
        EventCamera.SetPos(position);
        EventCamera.SetRef(reference);
        EventCamera.FollowOff();
        EventCamera.SetRoll(0.0f);
    }

    start_event_no = event_no;
    return 1;
}

/**
 * Starts a system event and copies an optional source camera into the event camera.
 */
void RunSystemEvent(int event_no, CCamera *camera) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;

    start_event_no = -1;

    if (start_system_event <= 0) {
        if (camera != nullptr) {
            camera->GetPos(position);
            camera->GetRef(reference);
            EventCamera.SetPos(position);
            EventCamera.SetRef(reference);
            EventCamera.FollowOff();
        }

        start_system_event = event_no;
    }
}

// Treant's clearing before (map 33) and after (map 13) the flood shares its five chests' flags.
bool TreantChestFlag(int flag_no) {
    return (MapNo == 33 || MapNo == 13) && flag_no >= 360 && flag_no <= 364;
}
} // namespace

int EdGetMapFlag(int flag_no) {
    if (flag_no <= 0) {
        return 0;
    }

    if (TreantChestFlag(flag_no)) {
        return SaveData->GetMapFlag(33, flag_no) || SaveData->GetMapFlag(13, flag_no);
    }

    return SaveData->GetMapFlag(MapNo, flag_no);
}

int EdSetMapFlag(int flag_no, int value) {
    if (flag_no <= 0) {
        return 0;
    }

    if (TreantChestFlag(flag_no)) {
        SaveData->SetMapFlag(33, flag_no, value);
        return SaveData->SetMapFlag(13, flag_no, value);
    }

    return SaveData->SetMapFlag(MapNo, flag_no, value);
}

/**
 * Runs one frame of the editor and reports what it is to do next.
 */
int EditLoop() {
    goto_return_menu = 0;

    // Pausing is a walking action: an event must not open the pause menu, which reads state only
    // valid outside an event, over itself.
    if (GameMode != ED_MODE_EVENT && EdPadDown(0x800, 4) != 0) {
        goto_return_menu = 1;
    }

    if (MapNo < TOWN_COUNT && loop_counter == 60 && GameMode == ED_MODE_WALK) {
        EdWalkToEditMes(0);
    }

    if (GameMode != ED_MODE_EVENT) {
        if (EdCheckViewMode() != 0) {
            MGSetRenderInfo(800.0f, 4.0f, 0x20000 - 2);
            MGScisioringForce(1);
        } else {
            MGSetRenderInfo(800.0f, 5.0f, 0x20000 - 2);
            MGScisioringForce(0);
        }
    }

    EdExchangeInfo.player = Chara;
    EdExchangeInfo.ground = pEditGround;
    EdExchangeInfo.camera = static_cast<CCameraFollow *>(NowCamera);
    EdExchangeInfo.event_marker = TreasureCursor;
    EdExchangeInfo.system_effect = SystemEffect;
    EdSetFlag();
    EdEventInfo.current_time = NowTime;
    CEditGround *ground = pEditGround;

    ground->focus_parts_id = -1;

    if ((static_cast<int *>(SaveData->GetConfigData()))[4] != 0) {
        EditMes1.text_rate_set = 0.6f;
    } else {
        EditMes1.text_rate_set = 0.3f;
    }

    int mode = GameMode;

    if (mode == ED_MODE_MAP_JUMP) {
        EdInteriorFlag = 1;

        int interior_result = EditInLoop();

        if (interior_result != 0) {
            if (interior_result == 0x63) {
                EditExit();
                return 1;
            }

            sceGsSyncV(0);
            sceGsSyncV(0);
            sceGsSyncV(0);
            sceGsSyncV(0);
            simple_event = 0;
            LoadScript();

            MGFillBox(CRect_i_(0, 0, 0x2800, SCREEN_HALF_HEIGHT * 16), 0, 0, 0, 0x80);
            MGEndFrame();
            TexManager.DeleteTextureBlock(0xF);
            EdNPCBuffer.used = 0;
            EdVillagerBuffer.used = 0;
            InitWorkBuffer();
            EdSelectVillager(EdVillagerInfo, NowTime, EditMapInfo);
            EdInitVilager(EdVillagerInfo, EdExchangeInfo.ground, nullptr);
            EdInitVilagerPosition(EdVillager, EdVillagerInfo, pEditGround, nullptr);
            MGBeginFrame();

            MGFillBox(CRect_i_(0, 0, 0x2800, SCREEN_HALF_HEIGHT * 16), 0, 0, 0, 0x80);

            ED_EVENT_PARAM entry{};

            if (EdSearchEvent(&entry, EdInteriorName, EdInteriorJumpID, NowTime) != 0) {
                sceVu0CopyVector(fix_chara_pos, entry.position);
                sceVu0CopyVector(fix_chara_rot, entry.rotation);
                sceVu0CopyVector(fix_camera_pos, entry.camera_pos);
            }

            Chara->SetPosition(fix_chara_pos);

            float turn = fix_chara_rot[1];

            turn += PI_SHORT;

            if (!(turn <= PI_SHORT)) {
                turn -= TWO_PI_SHORT;
            }

            Chara->SetRotation(fix_chara_rot[0], turn, fix_chara_rot[2]);
            MainCamera.FollowOff();
            MainCamera.SetPos(fix_camera_pos);
            MainCamera.SetRef(fix_chara_pos[0], 14.0f + fix_chara_pos[1], fix_chara_pos[2]);
            MainCamera.Step(-1);

            sceVu0FVECTOR direction;

            MainCamera.GetDir(direction);

            float angle = MainCamera.GetAngleH();

            MainCamera.FollowOn();
            MainCamera.SetAngleSoon(angle);
            Chara->ClothStep(-1);
            sceGsSyncV(0);
            sceGsSyncV(0);
            EdInitSoundSrc();
            PlayAmbient(NowTime);
            EdSetAmbientVol(1.0f);

            if (EdBeforeInBgmNo >= 0) {
                SndBgmLoad(EdBeforeInBgmNo);
            }

            sceGsSyncV(0);
            sceGsSyncV(0);
            EdFadeIn(0x40, 0.0f, 0.0f, 0.0f);
            GameMode = ED_MODE_WALK;

            if (EdInteriorDoorSound >= 0) {
                SndSetCamera(static_cast<CCamera *>(&MainCamera));
                EdDoorCloseSe(EdInteriorDoorSound, fix_chara_pos);
            }

            if (MapNo < TOWN_COUNT) {
                EdWalkToEditMes(60);
            }

            if (EdEventInfo.outside_map_no >= 0) {
                RunEvent(EdEventInfo.outside_map_no, nullptr);
            }
        }

        return 0;
    }

    EdInteriorFlag = 0;

    if (interior_test != 0) {
        static int             select = 0;
        std::array<char *, 64> menu{};

        for (std::size_t i = 0; i < menu.size(); i++) {
            menu[i] = interior_name[i];
        }

        int count = 0;

        while (menu[count][0] != '\0') {
            count++;
        }

        if (count == 0) {
            return 0;
        }

        GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 20, 5);

        if (GamePad.Down(PAD_DOWN) != 0) {
            select++;
        }

        if (GamePad.Down(PAD_UP) != 0) {
            select--;
        }

        select = std::max(select, 0);

        if (select >= count) {
            select = count - 1;
        }

        const int top = select >= 14 ? select - 13 : 0;

        constexpr std::array<std::string_view, 2> mark = {" ", ">"};
        constexpr int                             text_capacity = sizeof(DebugFont__3.text);

        DebugFont__3.length = 0;

        for (int i = top; i < std::min(top + 14, count); i++) {
            const int remaining = text_capacity - DebugFont__3.length;
            if (remaining <= 1) {
                break;
            }

            const auto result = std::format_to_n(&DebugFont__3.text[DebugFont__3.length], remaining - 1, "{} {}\n",
                                                 mark[i == select ? 1 : 0], std::string_view(menu[i]));
            if (result.size >= remaining) {
                DebugFont__3.text[text_capacity - 1] = '\0';
                DebugFont__3.length = text_capacity - 1;
                break;
            }

            *result.out = '\0';
            DebugFont__3.length += static_cast<int>(result.size);
        }

        TexManager.ReloadTexture(GetVif1Packet(), 0x1F);
        DebugFont__3.Draw();

        if (GamePad.Down(PAD_CIRCLE) != 0) {
            const std::string_view name = menu[select];
            if (name.size() >= std::size(interior_map_name)) {
                return 0;
            }

            float period_time = NowTime / 3.0f;

            EdMapJump(static_cast<int>(period_time), menu[select]);
            GameMode = ED_MODE_MAP_JUMP;

            while (ReadBGSync() != 0) {
            }

            std::ranges::copy(name, interior_map_name);
            interior_map_name[name.size()] = '\0';
            EditInInit(NowTime, interior_map_name);
        }

        if (GamePad.On(PAD_SELECT) != 0 && GamePad.On(PAD_START) != 0) {
            return 1;
        }

        return 0;
    }

    switch (mode) {
        case ED_MODE_FISHING:
            draw_clock = 0;
            TownMouseBegin(&MainCamera, pEditGround, MapNo, GameMode);
            MainMode();
            MainEditMode();

            if (EdCheckViewMode() == 0) {
                NowCamera = static_cast<CCamera *>(&MainCamera);
            } else {
                NowCamera = static_cast<CCamera *>(&ViewCamera);
            }

            if (EdCheckViewMode() == 0) {
                NowCamera->Step(1);
            }

            break;
        case ED_MODE_WALK:
            if (loop_counter > 0) {
                TownMouseBegin(&MainCamera, pEditGround, MapNo, GameMode);
                if (EdCheckViewMode() == 0) {
                    TownZoomBegin(&MainCamera, pEditGround, MapNo);
                }
                MainMode();
            }

            MainEditMode();

            if (EdCheckViewMode() == 0) {
                NowCamera = static_cast<CCamera *>(&MainCamera);
            } else {
                NowCamera = static_cast<CCamera *>(&ViewCamera);
            }

            if (EdCheckViewMode() == 0) {
                NowCamera->Step(1);
            }

            if (goto_cmp_event != 0) {
                goto_return_menu = 0;

                if (EdFadeOutCheck() != 0) {
                    if (goto_cmp_event < 0) {
                        EdExitLoop();
                    } else {
                        RunEvent(goto_cmp_event, nullptr);
                        EdFadeIn(60, 0.0f, 0.0f, 0.0f);
                    }

                    goto_cmp_event = 0;
                }
            }

            break;
        case ED_MODE_EVENT:
            NowCamera = static_cast<CCamera *>(&EventCamera);
            MainEditMode();
            EventMode();
            VillagerCollision();
            EdEventNPCStep();
            NowCamera->Step(1);
            draw_npc_cursor = 0;
            draw_clock = 0;
            goto_return_menu = 0;
            break;
        case ED_MODE_TALK:
            NowCamera = static_cast<CCamera *>(&TalkCamera);
            TalkMode();
            MainEditMode();
            NowCamera->Step(1);
            draw_npc_cursor = 0;
            draw_clock = 0;
            break;
        case ED_MODE_GEORAMA:
            GamePad.SetAutoRepeat(PAD_DPAD, 20, 5);
            NowCamera = static_cast<CCamera *>(&EditCamera);
            MainEditMode();
            EditMode();
            NowCamera->Step(1);
            pEditGround->EditAreaClip(NowCamera, -1.0f);

            if (goto_cmp_event != 0) {
                goto_return_menu = 0;

                if (EdFadeOutCheck() != 0) {
                    RunEvent(goto_cmp_event, nullptr);
                    EdFadeIn(60, 0.0f, 0.0f, 0.0f);
                    goto_cmp_event = 0;
                }
            }

            break;
        case ED_MODE_DOOR_OPEN:
            if (door_open_cnt == 0x9C) {
                EdMapJump(EdGetTime(NowTime), interior_map_name);
            }

            if (door_open_cnt < 0x9C) {
                ReadBG();
            }

            if (door_open != 0) {
                NowCamera = static_cast<CCamera *>(&MainCamera);
            }

            OpenDoorMode();
            MainEditMode();
            door_open_cnt--;
            NowCamera->Step(1);

            if (door_open_cnt == 100 && EdInteriorDoorSound >= 0) {
                sceVu0FVECTOR chara_pos;

                Chara->GetPosition(chara_pos);
                EdDoorOpenSe(EdInteriorDoorSound, chara_pos);
            }

            if (door_open_cnt < 0x96 && EdFadeOutCheck() != 0) {
                door_open_cnt = -1;
            }

            if (door_open_cnt < 0) {
                door_open_cnt = 0;
                GameMode = ED_MODE_MAP_JUMP;
                EdStopSoundSrc();
                EdSetAmbientVol(0.3f);
                SndStep();
                EdBeforeInBgmNo = SndGetBgmNo();

                if (old_main_mode == 5) {
                    EdBeforeInBgmNo = 0;
                }

                bgm_vol >>= 1;

                if (bgm_vol <= 0) {
                    bgm_vol = 1;
                }

                if (EdEventInfo.map_jump_bgm_stop != 0) {
                    SndBgmFadeOutStop();
                }

                SndStep();

                MGFillBox(CRect_i_(0, 0, 0x2800, SCREEN_HALF_HEIGHT * 16), 0, 0, 0, 0x80);
                MGEndFrame();

                for (int i = 0; i < 10; i++) {
                    TexManager.DeleteTextureBlock(i + 0x36);
                }

                TexManager.CleanUpBuffer();

                while (ReadBGSync() != 0) {
                }

                sceGsSyncV(0);
                EdSelectVillager(EdVillagerInfo, NowTime, EditMapInfo);
                EditInInit(NowTime, interior_map_name);
                MGBeginFrame();

                MGFillBox(CRect_i_(0, 0, 0x2800, SCREEN_HALF_HEIGHT * 16), 0, 0, 0, 0x80);
                ItemVolumeStep.CheckItemVolume();
                return 0;
            }

            break;
        case ED_MODE_GEORAMA_MENU:
            if (EditMenuLoop() != 0) {
                GameMode = ED_MODE_GEORAMA;
                NowSelectParts = -1;

                switch (EditMenuStatus.mode) {
                    case EDIT_MENU_MODE_PLACE:
                        NowSelectParts = EditMenuStatus.parts;
                        break;
                    case EDIT_MENU_MODE_MOVE:
                        NowSelectParts = -1;
                        break;
                    case EDIT_MENU_MODE_EVENT:
                        FadeOutToEvent(EditMenuStatus.event_no + 0xC8, 0);
                        break;
                    default:
                        break;
                }

                EdExitMenu();
                return 0;
            }

            break;
        case ED_MODE_MENU:
            TexManager.ReloadTexture(GetVif1Packet(), 0x10);
            MenuMapJumpMode = -1;
            EditPartsObjectOnOff();

            if (EdMenuMode() != 0) {
                GameMode = ED_MODE_WALK;
                EdExitMenu();

                if (MenuMapJumpMode >= 0) {
                    MapJump(MenuMapJumpMode, -1);
                    EdEventInfo.integer_arguments[0] = MenuMapJumpMode;
                    RunSystemEvent(2, NowCamera);
                    MenuMapJumpMode = -1;
                }

                EdInitVillagerOnOff(EdVillager, EdVillagerInfo, pEditGround);
                return 0;
            }

            break;
        case ED_MODE_TIME_CHANGE:
            if (chg_time_cnt < 0x8C) {
                ReadBG();
            }

            for (int i = 0; i < 10; i++) {
                if (EdVillager[i].CheckDraw() != 0) {
                    EdVillager[i].Step();
                } else {
                    EdVillager[i].initialized = false;
                }
            }

            if (chg_time_cnt == 0x8C) {
                EdVillagerBuffer.used = 0;
                StartReadBG();
                EdSelectVillager(EdVillagerInfo, NowTime, EditMapInfo);
                EdInitVilager(EdVillagerInfo, pEditGround, EdNPCReadBuffer);
            }

            NowCamera = static_cast<CCamera *>(&MainCamera);
            Chara->GetVelocity()->y = 0.0f;

            Chara->motion_no = 0;
            Chara->motion_flags = 0;
            Chara->motion_speed = -1.0f;
            Chara->Step();
            Chara->ClothStep(0);
            Chara->ShadowStep();
            MainEditMode();
            chg_time_cnt--;
            NowCamera->Step(1);

            if (chg_time_cnt < 0) {
                while (ReadBGSync() != 0) {
                }

                for (int i = 0x36; i < 0x40; i++) {
                    TexManager.DeleteTextureBlock(i);
                }

                TexManager.CleanUpBuffer();

                for (int i = 0; i < 10; i++) {
                    BG_READ_INFO *file = GetReadBGFile(i);

                    if (file != nullptr) {
                        auto *pack = reinterpret_cast<u_int *>(file->buffer);

                        EdLoadVillager(pack, EdVillager[i].resource_name, &EdVillager[i], &EdVillagerBuffer);
                    }
                }

                EdInitVilagerPosition(EdVillager, EdVillagerInfo, pEditGround, nullptr);
                chg_time_cnt = 0;
                GameMode = ED_MODE_WALK;
                EdFadeIn(0x40, 0.0f, 0.0f, 0.0f);
            }

            break;
        case ED_MODE_GEORAMA_MENU_INIT:
        case ED_MODE_MENU_INIT:
            draw_clock = 0;
            MGFlipWaitVSync(1);
            pEditGround->EditAreaClip(NowCamera, -1.0f);
            break;
        case ED_MODE_UNK_0:
        case ED_MODE_RETURN_MENU_WALK:
        case ED_MODE_RETURN_MENU_GEORAMA:
        case ED_MODE_MAP_JUMP:
        case ED_MODE_UNK_D:
        case ED_MODE_UNK_F:
        default:
            break;
    }

    TownZoomEnd();
    TownMouseApply(NowCamera, pEditGround, MapNo, GameMode);

    sceVu0FMATRIX view;
    sceVu0FVECTOR eye;

    NowCamera->GetCameraMatrix(view);
    NowCamera->GetPos(eye);
    MGSetViewMatrix(view, eye);

    if (GameMode != ED_MODE_GEORAMA && GameMode != ED_MODE_GEORAMA_MENU_INIT && GameMode != ED_MODE_RETURN_MENU_GEORAMA) {
        if (move_count > 0) {
            pEditGround->EditAreaClip(NowCamera, -1.0f);
        } else {
            pEditGround->EditAreaClip(NowCamera, 20000.0f);
        }
    }

    switch (GameMode) {
        case ED_MODE_GEORAMA:
            depth_of_field = 0;
            edit_mode_lighting = 1;
            draw_sky = 0;
            edit_mode_draw = 1;
            edit_mode_grd_draw = 1;
            EditMainDraw();
            break;
        case ED_MODE_UNK_0:
        case ED_MODE_WALK:
        case ED_MODE_TALK:
        case ED_MODE_EVENT:
        case ED_MODE_FISHING:
            depth_of_field = 1;
            edit_mode_lighting = 0;
            draw_sky = 1;
            edit_mode_draw = 0;

            if (move_count <= 10) {
                edit_mode_grd_draw = 0;
            }
            /* fallthrough */
        case ED_MODE_TIME_CHANGE:
        case ED_MODE_GEORAMA_MENU_INIT:
        case ED_MODE_MENU_INIT:
        case ED_MODE_RETURN_MENU_WALK:
        case ED_MODE_RETURN_MENU_GEORAMA:
        case ED_MODE_DOOR_OPEN:
            EditMainDraw();
            break;
        case ED_MODE_GEORAMA_MENU:
        case ED_MODE_MENU:
            EditNameMes.MakeMesWin(-1);
            EdFadeInOut();
            break;
        default:
            break;
    }

    CheckKeyLock();

    sceVu0FVECTOR focus_pos;

    if (GameMode == ED_MODE_WALK) {
        Chara->GetPosition(focus_pos);
    } else {
        sceVu0CopyVector(focus_pos, ECursorFrame->position);
    }

    if ((EdDebugMoveFlag > 0 || MapNo < TOWN_COUNT) && EdPadDown(0x100, 2) != 0 && EdCheckViewMode() == 0 && change_time_event == 0) {
        sceVu0FVECTOR at;

        if (GameMode == ED_MODE_WALK) {
            move_count = 30;
            GameMode = ED_MODE_GEORAMA;
            EditCamera.SetAngleSoon(MainCamera.GetAngle());
            MainCamera.GetPos(at);
            EditCamera.SetPos(at);
            MainCamera.GetRef(at);
            EditCamera.SetRef(at);
            Chara->GetPosition(at);
            NowSelectAngle = 0;
            OldSelectAngle = 0;
            NowCursorRotY = 0.0f;
            Chara->GetPosition(at);
            ECursorFrame->SetPosition(at);
            sceVu0CopyVector(NowPartsCursorPos, ECursorFrame->position);
            sceVu0CopyVector(NowCursorPos, ECursorFrame->position);
            sceVu0CopyVector(NextCursorPos, ECursorFrame->position);
            NowFocusParts = nullptr;
            OldFocusParts = nullptr;
            PartsNameNum = -1;
            DrawPartsNameCount = 30;
            EditMenuStatus.mode = EDIT_MENU_MODE_CLOSED;
            EditMenuStatus.parts = -1;
            NowSelectParts = -1;
        } else if (GameMode == ED_MODE_GEORAMA && pEditGround->CheckEffect() == 0) {
            move_count = 30;
            GamePad.AutoRepeatOff();

            if (CheckEditToWalk(at) != 0) {
                GameMode = ED_MODE_WALK;
                Chara->SetPosition(at);
                MainCamera.SetAngleSoon(EditCamera.GetAngle());
                EditCamera.GetPos(at);
                MainCamera.SetPos(at);
                EditCamera.GetRef(at);
                MainCamera.SetRef(at);

                int   requested_count = 0;
                auto *ground_data = reinterpret_cast<ED_GRD_DATA *>(SaveData->GetGrdData(MapNo));

                if (ground_data != nullptr && ground_data->complete_event_seen == 0) {
                    for (int i = 0; i < 24; i++) {
                        if (EditPartsInfo.request[i] != 0) {
                            requested_count++;
                        }
                    }

                    if (requested_count == EditPartsInfo.parts_max) {
                        FadeOutToEvent(0x83, 9);
                    }
                }

                if (MapNo == TOWN_MATATAKI && SaveData->GetGameFlag(0x14) == 0 && EditPartsInfo.request[16] != 0) {
                    FadeOutToEvent(0xB, 0xA);
                }
            }

            EdInitVilagerPosition(EdVillager, EdVillagerInfo, pEditGround, &at);
        }
    }

    if (move_count > 0) {
        EditCamera.SetSpeed(10.0f);
    } else {
        EditCamera.SetSpeed(8.0f);
    }

    move_count = std::max(move_count, 0);

    move_count--;

    if (GameMode == ED_MODE_GEORAMA_MENU_INIT || GameMode == ED_MODE_MENU_INIT) {
        int next_mode = EdInitModeFinish(NowCamera, TexManager.GetTexture("frame_image", -1));

        if (next_mode != 0) {
            GameMode = next_mode;
        }
    }

    EdSaveFrameImageTask();

    if (GameMode == ED_MODE_GEORAMA) {
        EdStopSoundSrc();

        if ((EdPadDown(0x10, 2) != 0 || EdPadDown(0x20, 2) != 0) && EdInitMenu(ED_MENU_EDIT) != 0) {
            NowSelectParts = -1;
            GameMode = ED_MODE_GEORAMA_MENU_INIT;
            SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
        }
    }

    if (GameMode == ED_MODE_WALK && (EdPadDown(0x10, 1) != 0 || goto_menu != 0 || (SystemMesCheck() == 0 && EdCheckItemOver() != 0))) {
        int menu_no = ED_MENU_BATTLE;

        if (goto_menu != 0) {
            menu_no = goto_menu;
            goto_menu = 0;
        }

        if (EdInitMenu(menu_no) != 0) {
            if (EdCheckItemOver() != 0) {
                if (loop_counter > 10) {
                    SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
                }
            } else {
                SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
            }

            GameMode = ED_MODE_MENU_INIT;
        }
    }

    static int debug_flag = 0;

    // Debug builds draw the editor debug overlay and open the debug menu from the pad.
    if (DebugMode != 0) {
        if (GamePad.Down(PAD_L3) != 0) {
            debug_flag = !debug_flag;
        }

        EdDDebug(debug_flag);

        if (debug_menu_mode != 0) {
            GamePad.KeyLock(0);
            EdDebugMenu();

            if (GamePad.Down(PAD_R3 | PAD_CIRCLE) != 0 || EdDebugRunEventNo > 0) {
                debug_menu_mode = 0;
                InputSetMenuNavigation(false);
                GamePad.AutoRepeatOff();
                RunEvent(EdDebugRunEventNo, NowCamera);
            } else {
                GamePad.KeyLock(1);
            }
        } else {
            if (GameMode != ED_MODE_EVENT && GamePad.Down(PAD_R3) != 0) {
                GamePad.SetAutoRepeat(PAD_DPAD, 0x19, 3);
                GamePad.SetAutoRepeat(PAD_L1 | PAD_R1, 0x19, 3);
                debug_menu_mode = 1;
                InputSetMenuNavigation(true);
                debug_flag = 0;
            }

            if (GameMode == ED_MODE_GEORAMA) {
                sceVu0FVECTOR cursor_pos;
                sceVu0CopyVector(cursor_pos, ECursorFrame->position);
                EdDPrintVector("pos = ", cursor_pos);
                int         area = pEditGround->GetAreaCode(cursor_pos[0], cursor_pos[1], cursor_pos[2]);
                std::string text = "area = " + std::to_string(area) + "\n";
                EdDPrint(text.data());

                if (area >= 0) {
                    CVector3_i_ grid;

                    pEditGround->areas[area]->GetPos(&grid, cursor_pos[0], cursor_pos[1], cursor_pos[2]);
                    text = "pos(grid) = (" + std::to_string(grid.x) + "," + std::to_string(grid.z) + ")\n";
                    EdDPrint(text.data());
                    text = "id = " + std::to_string(pEditGround->areas[area]->GetPartsID(grid.x, grid.z)) + "\n";
                    EdDPrint(text.data());
                }
            } else {
                EdDPrintChara(static_cast<CMainChara *>(Chara));
            }

            EdDPrintCamera(NowCamera);
            EdDDrawFont();
        }
    }

    if (GameMode != ED_MODE_GEORAMA_MENU && GameMode != ED_MODE_GEORAMA && loop_counter > 10) {
        sceVu0FVECTOR eye_pos;
        sceVu0FVECTOR eye_dir;
        CBoxVu0       box;
        constexpr int nearby_capacity = 64;
        auto          nearby = std::make_unique<CMapParts *[]>(nearby_capacity);

        NowCamera->GetPos(eye_pos);
        NowCamera->GetDir(eye_dir);

        float reach = 550;

        box.min[0] = eye_pos[0] - reach;
        box.min[1] = eye_pos[1] - reach;
        box.min[2] = eye_pos[2] - reach;
        box.min[3] = 1.0f;
        box.max[0] = reach + eye_pos[0];
        box.max[1] = reach + eye_pos[1];
        box.max[2] = reach + eye_pos[2];
        box.max[3] = 1.0f;

        int near_count = std::clamp(pEditGround->GetNearParts(nearby.get(), nearby_capacity, &box, nullptr), 0,
                                    nearby_capacity);

        for (int i = 0; i < near_count; i++) {
            if (nearby[i]->subtype == MAP_PARTS_SUBTYPE_RIVER && pEditGround->suppress_water != 0) {
                nearby[i] = nullptr;
            }
        }

        if (sound_off_cnt <= 0) {
            EdSetSoundSrcVol(NowTime, nearby.get(), near_count, eye_pos, eye_dir);
        }
    }

    if (change_time_event != 0 && move_count <= 0 && RunEvent(0x84, NowCamera) != 0) {
        change_time_event = 0;
    }

    static int event_next = 0;

    if (GamePad.Down2(PAD_SQUARE) != 0) {
        event_next = 4;
        EdEventAllClear();
        simple_event = 0;
    }

    event_next--;

    event_next = std::max(event_next, 0);

    if (event_next == 1) {
        start_event_no = 0x96;
    }

    if (GameMode != ED_MODE_EVENT) {
        auto *player = static_cast<CMainChara *>(Chara);

        if (player->move_info.landed != 0 && loop_counter > 10 && player->move_info.ground_poly.attr.ground_kind > 0) {
            RunEvent(player->move_info.ground_poly.attr.ground_kind, NowCamera);
            player->move_info.ground_poly.attr.ground_kind = 0;
        }
    }

    // An event starting clears the time-of-day text left from walking, so it is not drawn over a
    // cutscene; the text an event starts itself (the time-change event's _DRAW_DAY) is drawn.
    if (EdDebugEventEnable != 0 && (start_event_no > 0 || start_system_event > 0)) {
        sceVu0FVECTOR camera_pos;
        sceVu0FVECTOR camera_ref;

        NowCamera->GetPos(camera_pos);
        NowCamera->GetRef(camera_ref);
        EventCamera.FollowOff();
        EventCamera.SetPos(camera_pos);
        EventCamera.SetRef(camera_ref);
        EventCamera.Step(-1);

        if (start_system_event > 0) {
            if (EdEventInit(start_system_event, &EdWorkBuffer, EdSystemEventData) != 0) {
                EdInitMesParam();
                EdInitDrawDay();
                GameMode = ED_MODE_EVENT;
            }
        } else {
            if (EdEventInit(start_event_no, &EdWorkBuffer, EdEventData) != 0) {
                EdInitMesParam();
                EdInitDrawDay();
                GameMode = ED_MODE_EVENT;
                ItemVolumeStep.CheckItemVolume();
            }

            if (EdEventInfo.return_code == ED_EVENT_RETURN_TALK) {
                EdInitMesParam();
                GameMode = ED_MODE_TALK;
                simple_event = 0;
                ItemVolumeStep.CheckItemVolume();
            }
        }

        start_event_no = -1;
        start_system_event = -1;
    } else {
        simple_event = 0;
    }

    PauseOffCheck();
    static int old_mode;

    if ((GameMode == ED_MODE_RETURN_MENU_WALK || GameMode == ED_MODE_RETURN_MENU_GEORAMA) &&
        EdPadDown(0x800, 0xFFFF) != 0) {
        GameMode = old_mode;
        EdSePlay(static_cast<ED_SOUND_ID>(2), -1);
        PlayTimeCountFlag(1);
    } else if (goto_return_menu != 0) {
        if (GameMode == ED_MODE_WALK || GameMode == ED_MODE_FISHING) {
            old_mode = GameMode;
            oldGameMode = GameMode;
            GameMode = ED_MODE_RETURN_MENU_WALK;
            EdSePlay(static_cast<ED_SOUND_ID>(1), -1);
            PlayTimeCountFlag(0);
        } else if (GameMode == ED_MODE_GEORAMA) {
            old_mode = GameMode;
            GameMode = ED_MODE_RETURN_MENU_GEORAMA;
            EdSePlay(static_cast<ED_SOUND_ID>(1), -1);
            PlayTimeCountFlag(0);
        }
    }

    goto_return_menu = 0;
    static int end_code = 0;

    if (goto_dungeon != 0 && end_counter == 0) {
        main_select_menu_no = NowEditMap;
        MapJump(NowEditMap + 0xC8, -1);
        end_counter = 1;
        end_code = 1;
    }

    key_counter = 0;

    // Debug builds leave the editor through a fade on a two-button chord.
    if (DebugMode != 0 && GamePad.On(PAD_SELECT) != 0 && GamePad.On(PAD_START) != 0 && end_counter == 0) {
        end_counter = 100;
        EdFadeOut(0x40, 0.0f, 0.0f, 0.0f);
        end_code = 1;
    }

    if (exit_loop != 0) {
        EditExit();
        return 1;
    }

    if (end_counter == 1) {
        EditExit();
        return end_code;
    }

    end_counter--;

    end_counter = std::max(end_counter, 0);

    SndStep();
    loop_counter++;
    key_counter++;
    sound_off_cnt--;

    sound_off_cnt = std::max(sound_off_cnt, 0);

    DebugFont__3.length = 0;

    if (GamePad.On(PAD_CIRCLE) != 0) {
        cat_end();
    }

    return 0;
}

namespace {
// How far from the player retail draws a villager (KeepNearVillagers, editloop3.cpp); the port
// keeps those within video.detail_distance too.
constexpr float kRetailVillagerReach = 150.0f;

/**
 * Draws, before the depth of field, what the port draws past retail's distances, so that the
 * softening takes it in with the scenery: the villagers further than retail's reach from the player,
 * with their shadows, and the town parts' shadows that video.shadow_distance moves into the near
 * band. Retail draws the characters and the shadows after the depth of field, and they keep that
 * place. far_marks receives the draw marks of the villagers drawn here, which the characters' own
 * pass then leaves out; returns whether it marked any.
 */
bool EdDrawBeyondRetailReach(int *far_marks) {
    bool drawn = false;

    for (int i = 0; i < 10; i++) {
        far_marks[i] = 0;
    }

    switch (GameMode) {
        case ED_MODE_RETURN_MENU_WALK:
        case ED_MODE_MENU_INIT:
        case ED_MODE_WALK:
        case ED_MODE_FISHING:
        case ED_MODE_TALK:
        case ED_MODE_TIME_CHANGE: {
            sceVu0FVECTOR player_position;
            sceVu0FVECTOR villager_position;

            Chara->GetPosition(player_position);

            for (int i = 0; i < 10; i++) {
                EdVillager[i].GetPosition(villager_position);

                if (EdVillager[i].initialized != 0 && DistVector(player_position, villager_position) >= kRetailVillagerReach) {
                    far_marks[i] = 3;
                    drawn = true;
                }
            }

            if (drawn) {
                int shadow_on = GameMode != ED_MODE_TALK && EdSystemMesCheck() == 0;
                EdDrawCharacter(Chara, 0, 10, EdVillager, far_marks, shadow_on, nullptr);
            }
            break;
        }
        default:
            break;
    }

    if (EdDrawOffMapShadow == 0 && edit_mode_draw == 0 && EditMapInfo->shadow_level > 0) {
        float saved_light[4][4];
        float shadow_light[4][4];
        float light_colour[4][4];

        MGGetPLight(saved_light, light_colour);
        sceVu0CopyMatrix(shadow_light, saved_light);
        EdLimitShadowLight(shadow_light, 3.0f);
        MGSetPLight(shadow_light, light_colour);
        TexManager.ReloadTexture(Vif1Packet, 0x16);
        MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
        EdGroundDrawShadowBeyondNear(pEditGround, EditMapInfo->shadow_near);
        MGEndDrawShadow(EditMapInfo->shadow_mode);
        MGSetPLight(saved_light, light_colour);
    }

    return drawn;
}

/**
 * Draws the editor's world for one frame (retail's MainDraw).
 */
void EditMainDraw() {
    auto far_marks = std::make_unique<int[]>(10);
    bool far_drawn = false;

    if (EdDrawOffFlag == 0) {
        if (EdPauseFlag != 0 || GameMode == ED_MODE_RETURN_MENU_WALK || GameMode == ED_MODE_RETURN_MENU_GEORAMA) {
            CTextureAnime::stop_anime = 1;
        } else {
            CTextureAnime::stop_anime = 0;

            if (EdThunderEffectFlag != 0) {
                EdThunderEffect(MapNo, pEditGround);
            }
        }

        EdDrawOffMap = EdEventInfo.suppress_background;
        EdDrawOffMapShadow = EdEventInfo.suppress_shadows;

        if (EdDrawOffMap == 0) {
            if (draw_sky != 0) {
                EdDrawSky(NowTime, SkyFrame, SunFrame, SkyBackFrame, NowCamera, EditMapInfo->sky_follow);
            }

            TexManager.ReloadTexture(Vif1Packet, 1);
            TexAnime.TexAnime(1);
            pEditGround->DrawBaseGround();

            // Georama mode draws the parts at walking's levels of detail, where retail drew them all at
            // their coarsest.
            constexpr int far_lod = 0;
            constexpr int near_lod = 2;
            constexpr int mid_lod = 0;

            pEditGround->Draw(NowTime, 1, far_lod, near_lod, mid_lod, near_lod);

            if (edit_mode_draw != 0) {
                sceVu0FVECTOR at;
                sceVu0FVECTOR colour = {0.0f, 0.0f, 0.0f, 0.0f};

                sceVu0CopyVector(at, ECursorFrame->position);
                colour[1] = NowCursorRotY;
                at[1] = pEditGround->GetAlt(at[0], at[1], at[2]);
                pEditGround->DrawPartsCursor(NowSelectParts, at, NowPartsCursorPos, NowSelectAngle, colour, 1);
            }

            TexManager.ReloadTexture(Vif1Packet, 2);
            TexAnime.TexAnime(2);
            pEditGround->Draw(NowTime, 2, far_lod, near_lod, 0, 3);

            if (edit_mode_draw != 0) {
                sceVu0FVECTOR at2;
                sceVu0FVECTOR colour2 = {0.0f, 0.0f, 0.0f, 0.0f};

                sceVu0CopyVector(at2, ECursorFrame->position);
                colour2[1] = NowCursorRotY;
                at2[1] = pEditGround->GetAlt(at2[0], at2[1], at2[2]);
                pEditGround->DrawPartsCursor(NowSelectParts, at2, NowPartsCursorPos, NowSelectAngle, colour2, 2);
            }

            if (FishingDrawCheck() != 0) {
                FishingDrawFish();
                FishLineDraw(0);
            }

            TexManager.ReloadTexture(Vif1Packet, 0x15);
            TexAnime.TexAnime(0x15);
            pEditGround->DrawWater(0x15);

            switch (GameMode) {
                case ED_MODE_WALK:
                case ED_MODE_FISHING:
                case ED_MODE_GEORAMA:
                case ED_MODE_TALK:
                case ED_MODE_EVENT:
                case ED_MODE_TIME_CHANGE:
                case ED_MODE_UNK_0:
                case ED_MODE_RETURN_MENU_WALK:
                case ED_MODE_MENU_INIT:
                case ED_MODE_GEORAMA_MENU_INIT:
                case ED_MODE_RETURN_MENU_GEORAMA: {
                    sceGsTex0 frame;
                    CRect_i_  screen;
                    sceGsTex0 water;
                    float     ref[4];
                    sceGsZbuf zbuf;

                    MGGetFBuffTex(&frame);
                    screen.x = 0;
                    screen.y = 0;
                    screen.width = 0x280;
                    screen.height = SCREEN_HALF_HEIGHT;
                    water = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("water_buff", -1)->tex0);
                    MGMoveImage(&frame, screen, &water, 0, 0, 0);
                    MainCamera.GetRef(ref);
                    zbuf = mgZBuffer;
                    zbuf.bits.zmsk = 1;
                    MGSetGsZBUF(&zbuf);
                    pEditGround->DrawWaterSurface(NowCamera);
                    MGSetGsZBUF(&mgZBuffer);
                    break;
                }
                default:
                    break;
            }

            pEditGround->DrawRipple(0x15);

            if (depth_of_field != 0) {
                int dof_level = 2;

                far_drawn = EdDrawBeyondRetailReach(far_marks.get());

                if ((static_cast<int *>(SaveData->GetConfigData()))[6] != 0) {
                    dof_level = 1;
                }

                EdDrawDOF(dof_level);
            }
        }

        if (FishingDrawCheck() != 0) {
            FishLineDraw(1);
        }

        if (GameMode == ED_MODE_EVENT) {
            EdEventBackSpriteDraw();
        }

        PolyCount = 0;

        if (GameMode == ED_MODE_EVENT) {
            EdDrawItem();
        }

        switch (GameMode) {
            case ED_MODE_RETURN_MENU_WALK:
            case ED_MODE_MENU_INIT:
                CCharacter::MotionStopFlag = 1;
                Chara->Step();
                Chara->ShadowStep();

                for (int i = 0; i < 10; i++) {
                    CNPCharacter *villager = &EdVillager[i];

                    if (villager->initialized != 0 && villager->draw_enabled != 0) {
                        int saved_sequence = villager->sequence_enabled;

                        villager->sequence_enabled = 0;
                        villager->Step();
                        villager->ShadowStep();
                        villager->sequence_enabled = saved_sequence;
                    }
                }

                CCharacter::MotionStopFlag = 0;
                /* fallthrough */
            case ED_MODE_WALK:
            case ED_MODE_FISHING:
            case ED_MODE_TALK:
            case ED_MODE_EVENT:
            case ED_MODE_TIME_CHANGE:
            case ED_MODE_DOOR_OPEN: {
                ED_EVENT_INFO *event = nullptr;
                int            shadow_on = 1;
                int            detail = 3;
                auto           marks_store = std::make_unique<int[]>(10);
                int           *marks = marks_store.get();
                float          chara_pos[4];
                CBoxVu0        box;
                constexpr int  near_parts_capacity = 0x40;
                auto           near_parts = std::make_unique<CMapParts *[]>(near_parts_capacity);

                if (GameMode == ED_MODE_TALK || EdSystemMesCheck() != 0) {
                    shadow_on = 0;
                }

                if (EdCheckViewMode() != 0) {
                    detail = 0;
                }

                if (GameMode == ED_MODE_DOOR_OPEN) {
                    marks = nullptr;
                } else {
                    for (int i = 0; i < 10; i++) {
                        marks_store[i] = far_drawn && far_marks[i] != 0 ? 0 : 3;
                    }
                }

                if (GameMode == ED_MODE_EVENT) {
                    event = &EdEventInfo;
                }

                Chara->GetPosition(chara_pos);
                chara_pos[1] += 10.0f;
                box.min[0] = chara_pos[0] - 100.0f;
                box.min[1] = chara_pos[1] - 100.0f;
                box.min[2] = chara_pos[2] - 100.0f;
                box.min[3] = 1.0f;
                box.max[0] = 100.0f + chara_pos[0];
                box.max[1] = 100.0f + chara_pos[1];
                box.max[2] = 100.0f + chara_pos[2];
                box.max[3] = 1.0f;

                int found = std::clamp(pEditGround->GetNearParts(near_parts.get(), near_parts_capacity, &box, nullptr), 0,
                                       near_parts_capacity);

                Chara->ClearPointLight();

                for (int i = 0; i < found; i++) {
                    CMapParts *parts = near_parts[i];
                    CFrame    *frame = parts->frame[0];

                    if (frame != nullptr) {
                        float transform[4];

                        parts->GetPosition(transform);
                        frame->SetPosition(transform);
                        near_parts[i]->GetRotation(transform);
                        frame->SetRotation(transform[0], transform[1], transform[2]);

                        for (int j = 0; j < 0x18; j++) {
                            EDIT_EFFECT_INFO *effect = near_parts[i]->effect[j];

                            if (near_parts[i]->effect_on[j] != 0 && effect != nullptr && (effect->kind == EDIT_EFFECT_FIRE_LARGE || effect->kind == EDIT_EFFECT_FIRE_MEDIUM || effect->kind == EDIT_EFFECT_FIRE_SMALL) && effect->frame != nullptr && CheckEditEffect(effect, NowTime) != 0) {
                                float light_pos[4];

                                effect->frame->GetWorldPosition(light_pos, effect->offset);

                                float distance = DistVector(chara_pos, light_pos);

                                if (distance <= 40.0f) {
                                    float brightness = 80.0f + static_cast<float>(rand() % 48);

                                    light_pos[3] = distance;
                                    Chara->SetPointLight(light_pos, 5.0f, 40.0f, brightness, brightness, brightness, 128.0f);
                                }
                            }
                        }
                    }
                }

                EdDrawCharacter(Chara, detail, 0xA, EdVillager, marks, shadow_on, event);
                break;
            }
            default:
                break;
        }

        if (EdDrawOffMapShadow == 0 && EdDrawOffMap == 0) {
            float saved_light[4][4];
            float shadow_light[4][4];
            float light_colour[4][4];

            MGGetPLight(saved_light, light_colour);
            sceVu0CopyMatrix(shadow_light, saved_light);
            EdLimitShadowLight(shadow_light, 3.0f);
            MGSetPLight(shadow_light, light_colour);
            TexManager.ReloadTexture(Vif1Packet, 0x16);

            float shadow_range[4] = {-100.0f, EditMapInfo->shadow_near, EditMapInfo->shadow_far, 0.0f};

            if (edit_mode_draw != 0) {
                MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
                pEditGround->DrawShadow(1, 0.0f, 100000.0f);
                MGEndDrawShadow(0x20);
            } else {
                if (EditMapInfo->shadow_level > 0) {
                    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
                    pEditGround->DrawShadow(0, shadow_range[0], shadow_range[1]);
                    MGEndDrawShadow(EditMapInfo->shadow_mode);
                }

                if (EditMapInfo->shadow_level >= 2) {
                    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
                    pEditGround->DrawShadow(0, shadow_range[1], shadow_range[2]);
                    MGEndDrawShadow(EditMapInfo->shadow_mode_2);
                }
            }

            MGSetPLight(saved_light, light_colour);
        }

        for (int i = 0; i < 4; i++) {
            TexManager.ReloadTexture(Vif1Packet, i + 0x1B);
            MotionParts[i].TextureAnime(i + 0x1B);
            MotionParts[i].Draw();
        }

        if (EdDrawOffMap == 0) {
            TexManager.ReloadTexture(Vif1Packet, 0x18);
            RunEffect.Lighting(1);
            RunEffect.Draw();

            switch (GameMode) {
                case ED_MODE_UNK_0:
                case ED_MODE_WALK:
                case ED_MODE_TALK:
                case ED_MODE_TIME_CHANGE:
                case ED_MODE_GEORAMA:
                case ED_MODE_GEORAMA_MENU_INIT:
                case ED_MODE_MENU_INIT:
                case ED_MODE_DOOR_OPEN:
                case ED_MODE_EVENT:
                case ED_MODE_FISHING:
                    if (EdPauseFlag == 0) {
                        float wind[4];

                        EditEffectStep();
                        EdWind.GetWind(wind);
                        EffectMacroStep(wind);
                        RunEffect.Step();
                        EdEffectGroup.Step(1);

                        if (NowEditMap == TOWN_MATATAKI) {
                            sceVu0FVECTOR spray = {800.0f, -20.0f, 1300.0f, 1.0f};
                            sceVu0FVECTOR size = {10.0f, 7.0f, 10.0f, 1.0f};

                            for (int i = 0; i < 5; i++) {
                                EffectWaterSpray(&EdEffectGroup, spray, size, 0xA, i);
                                spray[0] += 20.0f;
                            }
                        }
                    }
                    /* fallthrough */
                case ED_MODE_RETURN_MENU_WALK:
                case ED_MODE_RETURN_MENU_GEORAMA:
                    EditEffectStep2();
                    pEditGround->DrawEffect(static_cast<CCameraFollow *>(NowCamera), NowTime, &EdEffectGroup);
                    EdEffectGroup.Draw();
                    break;
                default:
                    break;
            }

            if (edit_mode_draw == 0) {
                EdDrawLensFlare(NowTime, SunFrame);
            }
        }

        if (GameMode == ED_MODE_EVENT) {
            EdEventSpriteDraw();
            EBDraw();

            if (EdEventInfo.screen_filter != 0) {
                float    fade[4];
                CRect_i_ screen;

                sceVu0CopyVector(fade, EdEventInfo.screen_filter_color);
                screen.x = 0;
                screen.y = 0;
                screen.width = 0x2800;
                screen.height = SCREEN_HALF_HEIGHT * 16;
                MGFillBox(screen, static_cast<int>(fade[0]) & 0xFF, static_cast<int>(fade[1]) & 0xFF, static_cast<int>(fade[2]) & 0xFF, static_cast<int>(fade[3]) & 0xFF);
            }
        }

        if (GameMode != ED_MODE_TALK) {
            DrawSysGra();
        }

        if (GameMode == ED_MODE_GEORAMA) {
            ParamDraw();
            TexManager.ReloadTexture(Vif1Packet, EditMes1.tex_block);
            EditNameMes.Step();
            EditNameMes.DrawMesWin();
        }

        int mes_mode = GameMode;

        if (mes_mode == ED_MODE_TALK || mes_mode == ED_MODE_FISHING || mes_mode == ED_MODE_EVENT || mes_mode == ED_MODE_GEORAMA) {
            TexManager.ReloadTexture(Vif1Packet, EditMes1.tex_block);
            EditMes1.DrawMesWin();

            if (GameMode == ED_MODE_EVENT) {
                EditEventMes1.DrawMesWin();
                EditSystemMes.DrawMesWin();
            }
        }

        if (GameMode == ED_MODE_WALK) {
            MonsterNameDraw();
        } else {
            MonsterNameMake(-1);
        }

        EdSystemMesStep();
        EdSystemMesDraw();

        if (GameMode == ED_MODE_GEORAMA && (EditMenuStatus.mode == EDIT_MENU_MODE_PLACE || EditMenuStatus.mode == EDIT_MENU_MODE_REPLACE)) {
            EDITPARTS_INFO *info = EditPartsInfo.GetPartsInfo(NowSelectParts);

            if (info != nullptr) {
                int      stock = info->stock;
                int      percent = (stock - info->placed) * 100 / stock;
                CRect_i_ back;
                CRect_i_ bar;
                CRect_i_ fill;

                back.x = 0x1F20;
                back.y = 0xBF8;
                back.width = 0x680;
                back.height = 0x70;
                MGFillBox(back, 0x14, 0x14, 0x14, 0x80);
                bar.x = 0x1F40;
                bar.y = 0xC08;
                bar.width = 0x640;
                bar.height = 0x50;
                MGFillBox(bar, 0x64, 0x64, 0x64, 0x80);
                fill.x = 0x1F40;
                fill.y = 0xC08;
                fill.width = percent * 0x10;
                fill.height = 0x50;
                MGFillBox(fill, 0xB4, 0x64, 0x28, 0x80);
                DrawAtraBuildNum(info, 0x1F4, 0x181, 0x80);
            }
        }

        EdFadeInOut();

        if (clear_screen != 0) {
            CRect_i_ screen;

            screen.x = 0;
            screen.y = 0;
            screen.width = 0x2800;
            screen.height = SCREEN_HALF_HEIGHT * 16;
            MGFillBox(screen, 0, 0, 0, 0x80);
        }

        clear_screen = 0;
    }
}
} // namespace

// The georama view's camera, turned by the mouse beside the right stick.
void MoveCamera(CCameraFollow *camera) {
    float horizontal = MouseLookTurn(camera, 0.03f, GamePad.GetRXf());
    camera->AddHeight(-GamePad.GetRYf());

    if (EdDebugCameraFlag == 0 && !(camera->GetHeight() <= 30.0f)) {
        camera->SetHeight(30.0f);
    }

    camera->AddAngle(0.03f * -horizontal);

    if (horizontal == 0.0f) {
        if (GamePad.On(PAD_R1) != 0) {
            camera->AddAngle(-DEG_TO_RAD);
        }

        if (GamePad.On(PAD_L1) != 0) {
            camera->AddAngle(DEG_TO_RAD);
        }
    }
}

// The pause sprite is skipped when an event left the texture block without "pause" loaded.
void DrawSysGra() {
    if (EdDebugParamDrawOff == 0) {
        TexManager.ReloadTexture(Vif1Packet, 20);
        EdDrawSysCursor(EditMapInfo->work.events.points, 256);

        if (FishingDrawCheck() == 0) {
            EdDrawClock(0, 0);
        }

        DrawDay();

        CTexture *pause = TexManager.GetTexture("pause", -1);

        if (pause != nullptr &&
            (GameMode == ED_MODE_RETURN_MENU_WALK || GameMode == ED_MODE_RETURN_MENU_GEORAMA || EdPauseFlag != 0)) {
            CRect_i_ fade;
            fade.x = 0;
            fade.y = 0;
            fade.width = 0x2800;
            fade.height = (SCREEN_HALF_HEIGHT << 4);
            MGFillBox(fade, 0, 0, 0, 0x40);
            setbilinear(0);

            CRect_i_ screen;
            CRect_i_ texel;
            texel.x = 0;
            texel.y = 0;
            texel.width = 0x80;
            texel.height = 0x28;
            screen.x = 0x100;
            screen.y = 0xdc;
            screen.width = 0x80;
            screen.height = 0x28;
            set2DSprite(GetVif1Packet(), pause, screen, texel, 0x80);
        }
    }
}

// Kept in the window's top right corner.
void EdDrawClock(int x, int y) {
    if (draw_clock != 0 && EditMapInfo->time_stop == 0) {
        gfx::UiAnchorScope anchor(gfx::UiAnchor::Side(1, -1));

        TexManager.ReloadTexture(Vif1Packet, 20);
        setbilinear(0);

        CRect_i_ clock_screen;
        CRect_i_ clock_texel;
        clock_texel.x = 0;
        clock_texel.y = 0;
        clock_texel.width = 88;
        clock_texel.height = 80;
        clock_screen.x = x + 520;
        clock_screen.y = y + 14;
        clock_screen.width = 88;
        clock_screen.height = 80;
        set2DSprite(GetVif1Packet(), TexManager.GetTexture("dayclock", -1), clock_screen, clock_texel, 128);

        float scale = 2.0f * (NowTime / 12.0f);
        float pi = PI_SHORT;
        float angle = pi * scale;
        angle = SIXTH_PI_SHORT + (pi - angle);
        CRect_i_ hand_screen;
        CRect_i_ hand_texel;
        hand_texel.x = 88;
        hand_texel.y = 0;
        hand_texel.width = 22;
        hand_texel.height = 40;
        hand_screen.x = x + 564;
        hand_screen.y = y + 54;
        hand_screen.width = 22;
        hand_screen.height = 40;
        set2DSpriteRot(GetVif1Packet(), TexManager.GetTexture("dayclock", -1), hand_screen, hand_texel, 12, 40, angle, 128);
    }
}

int LoadTexture() {
    if (EditMapInfo == nullptr) {
        return 0;
    }

    constexpr std::size_t image_capacity = std::extent_v<decltype(EDIT_MAP_INFO::images)>;
    std::size_t           image_count = 0;
    while (image_count < image_capacity && EditMapInfo->images[image_count].name[0] != '\0') {
        image_count++;
    }

    auto *buffer = reinterpret_cast<u_long128 *>(DataBuffer__2.base + DataBuffer__2.used * 16);

    TexManager.Initialize(0x3FE0);
    TexManager.SetBuffer(buffer, 0x4E200);

    LOADTEXTURE_INFO2 mes_blocks[8] = {
        {"#mes_frame_buff#640#" SCREEN_HEIGHT_STR "#4", 0x1A, 0},
        {"#fukidashibase#640#224#4",                    0x1A, 0},
        {"#fontbase#512#256#1",                         0x1A, 0},
        {"meswin/gaiji.img",                            0x1A, 0},
        {"meswin/fuki256.img",                          0x1A, 0},
        {"meswin/syst04.img",                           0x1A, 0},
    };
    std::string mes_path = "meswin/mes_tex";
    if (LanguageCode > LANG_JAPANESE) {
        mes_path += "_" + std::to_string(LanguageCode);
    }
    mes_path += ".pak";

    LoadFile(mes_path.data(), read_buffer, nullptr);
    wait_now_loading_vsync();
    mes_blocks[3].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "gaiji.img", nullptr));
    mes_blocks[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "fuki256.img", nullptr));
    mes_blocks[5].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "syst04.img", nullptr));
    TexManager.LoadTextureBlock(-1, mes_blocks);

    int common_size = 0;

    LoadFile("gedit/system/esys_cmn.pak", read_buffer, &common_size);
    wait_now_loading_vsync();

    // The language's system image goes in the 1 KiB-aligned space after the common pack; it holds
    // the language's own pause and day-of-adventure pictures, which the common pack does not.
    u_int      *system_image = read_buffer + ((common_size >> 6) + 1) * 64;
    std::string system_path = "gedit/system/sys";
    if (LanguageCode > LANG_JAPANESE) {
        system_path += "_" + std::to_string(LanguageCode);
    }
    system_path += ".img";

    LoadFile(system_path.data(), system_image, nullptr);
    wait_now_loading_vsync();
    TexManager.EnterIMGFile(reinterpret_cast<u_char *>(GetPackFile(read_buffer, "cursor.img", nullptr)), -1, 0, 0);
    TexManager.EnterIMGFile(reinterpret_cast<u_char *>(GetPackFile(read_buffer, "e01t02.img", nullptr)), -1, 0, 0);

    LOADTEXTURE_INFO2 system_blocks[] = {
        {"#water_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",  0x15, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 0x16, 0},
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0x18, 0},
        {"#font_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",   0x1F, 0},
        {"img/system.img",                                0x14, 0},
        {"",                                              0x14, 0},
        {"s_eff.img",                                     0x14, 0},
        {"img/ankfont.img",                               0x1F, 0},
        {"#frame_image#640#" SCREEN_HEIGHT_STR "#4",      0x13, 0},
    };

    auto blocks = std::make_unique<LOADTEXTURE_INFO2[]>(std::max(image_count + 1, std::size(system_blocks)));
    std::ranges::copy(system_blocks, blocks.get());
    blocks[4].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "sys_cmn.img", nullptr));
    blocks[5].name = reinterpret_cast<char *>(system_image);
    blocks[6].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "s_eff.img", nullptr));
    blocks[7].name = reinterpret_cast<char *>(GetPackFile(read_buffer, "ankfont.img", nullptr));
    TexManager.LoadTextureBlock(-1, blocks.get());
    wait_now_loading_vsync();

    CFrameAttr cursor_attr;

    cursor_attr.use_color = true;
    CharaCursor0 = LoadMDSFile(GetPackFile(read_buffer, "cursor01.mds", nullptr), &EtcDataBuffer, 0, nullptr, nullptr);
    CharaCursor0->SetAttr(cursor_attr, 1, 0x200);
    CharaCursor1 = LoadMDSFile(GetPackFile(read_buffer, "cursor02.mds", nullptr), &EtcDataBuffer, 0, nullptr, nullptr);
    CharaCursor1->SetAttr(cursor_attr, 1, 0x200);

    CharaCursor1->SetScale(2.5f, 2.5f, 2.5f);
    CharaCursor2 = LoadMDSFile(GetPackFile(read_buffer, "bic.mds", nullptr), &EtcDataBuffer, 0, nullptr, nullptr);
    CharaCursor2->SetAttr(cursor_attr, 1, 0x200);
    TreasureCursor = LoadMDSFile(GetPackFile(read_buffer, "ibox_0.mds", nullptr), &EtcDataBuffer, 0, nullptr, nullptr);
    TreasureCursorOpen = LoadMDSFile(GetPackFile(read_buffer, "ibox_1.mds", nullptr), &EtcDataBuffer, 0, nullptr, nullptr);

    CFrameAttr box_attr;

    box_attr.clip_enable = false;
    box_attr.fog_enable = true;

    if (TreasureCursor != nullptr) {
        TreasureCursor->SetAttr(box_attr, 1, 0x44);
    }

    if (TreasureCursorOpen != nullptr) {
        TreasureCursorOpen->SetAttr(box_attr, 1, 0x44);
    }

    SystemEffect[0].texture = TexManager.GetTexture("s_ef01", -1);

    std::string stay_path = std::string(EditDataDir) + "img.pak";
    auto        map_blocks = std::make_unique<LOADTEXTURE_INFO[]>(image_count + 1);
    CRect_i_    effect_rect;

    effect_rect.x = 0;
    effect_rect.y = 0;
    effect_rect.width = 0x20;
    effect_rect.height = 0x20;
    SystemEffect[0].texel = effect_rect;
    SystemEffect[0].alpha_blend = true;
    SystemEffect[0].disable_z_write = true;

    auto *menu_data = reinterpret_cast<u_int *>(EdNPCBuffer.base + EdNPCBuffer.used * 16);

    LoadFileMenuData("stayframe.img", menu_data);
    wait_now_loading_vsync();
    TexManager.EnterFixTextureZ(reinterpret_cast<u_char *>(menu_data));
    StayTexture = TexManager.GetTexture("stayframe", -1);

    if (LoadFile2(stay_path.data(), menu_data, nullptr, 0) != 0) {
        wait_now_loading_vsync();

        for (std::size_t image = 0; image < image_count; image++) {
            blocks[image].name = reinterpret_cast<char *>(GetPackFile(menu_data, EditMapInfo->images[image].name, nullptr));
            blocks[image].block_no = EditMapInfo->images[image].type;
            blocks[image].mipmap = EditMapInfo->images[image].number;
        }

        blocks[image_count].name = nullptr;
        blocks[image_count].block_no = 0;
        blocks[image_count].mipmap = false;
        TexManager.LoadTextureBlock(-1, blocks.get());
        TexAnime.Initialize(TexAnimeData, 0x40);

        int   anime_size = 0;
        char *anime_cfg = reinterpret_cast<char *>(GetPackFile(menu_data, "texanime.cfg", &anime_size));

        if (anime_cfg != nullptr) {
            for (int i = 0; i < 64; i++) {
                TexAnimeData[i].Initialize();
            }

            TexAnime.LoadCFGFile(anime_cfg, anime_size);
        }
    } else {
        TexAnime.Initialize(nullptr, 0);

        for (std::size_t image = 0; image < image_count; image++) {
            map_blocks[image].name = EditMapInfo->images[image].name;
            map_blocks[image].block_no = EditMapInfo->images[image].type;
            map_blocks[image].mipmap = EditMapInfo->images[image].number;
        }

        map_blocks[image_count].name = nullptr;
        map_blocks[image_count].block_no = 0;
        map_blocks[image_count].mipmap = false;
        TexManager.LoadTextureBlock(-1, map_blocks.get(), read_buffer);
    }

    TexManager.buffer_size = TexManager.buffer_used;
    DataBuffer__2.Alloc(static_cast<int>(TexManager.buffer + TexManager.buffer_size - buffer) + 16);
    DataBuffer__2.Align64();
    return 0;
}

// EditInit carves the town's objects out of EtcDataBuffer in quadword counts sized from the host
// classes, all of which are larger than the PS2 ones.
namespace {
template <class T>
int Quadwords(int count) {
    return static_cast<int>((sizeof(T) * count + 15) / 16);
}

// Formats into a fixed name buffer, truncating and terminating as snprintf does.
template <std::size_t N, class... Args>
void FormatInto(char (&buffer)[N], std::format_string<Args...> format, Args &&...args) {
    *std::format_to_n(buffer, N - 1, format, std::forward<Args>(args)...).out = '\0';
}
} // namespace

int EditInit(void * /*param*/) {
    std::string map_path;
    std::string save_path;
    int         size = 0;
    int         mes_size = 0;

    oldGameMode = ED_MODE_NONE;
    NowTime = 0.0f;
    SaveData->VisitMap(MapNo, 1);
    PlayTimeCountFlag(1);
    MGFlipWaitVSync(0);
    EdSetKeyMode(0xFFFF);
    GamePad.MenuModeOff();
    GamePad.AutoRepeatOff();
    int map_no = MapNo;
    NowEditMap = map_no;
    interior_test = 0;

    if (map_no < 9) {
        FormatInto(EditMapName, "e0{}", map_no + 1);
    }

    int map_id = MapNo;

    if (map_id > 10) {
        int interior = map_id - 10;

        if (interior < 10) {
            FormatInto(EditMapName, "s0{}", interior);
        } else {
            FormatInto(EditMapName, "s{}", interior);
        }
    }

    if (NowEditMap == 99) {
        EditDataDir[0] = '\0';
        interior_test = 1;
    } else {
        FormatInto(EditDataDir, "gedit/{}/", std::string_view(EditMapName));
    }

    bgm_play_flag = 0;
    bgm_play_start = 0;
    InitializeDataBuffer();
    SetDataBuffer(&VisualData, 100);
    SetDataBuffer(&EtcDataBuffer, 40000);
#ifdef PAL
    SetDataBuffer(&EdScriptBuffer, 20000);
#else
    SetDataBuffer(&EdScriptBuffer, 16000);
#endif
    SetDataBuffer(&EPartsInfoBuff, 8000);
    SetDataBuffer(&CharaBuffer, 115000);
    SetDataBuffer(&TextureData, 10);
    SetDataBuffer(&MotionData, 7900);
    SetDataBuffer(&EdMesBuffer, 13000);
    SetDataBuffer(&DataBuffer__2, 1216000);
    EdNPCBuffer.base = DataBuffer__2.base + DataBuffer__2.used * 16 + 0xCF8500;
    EdNPCBuffer.limit = 326000;
    EdNPCBuffer.used = 0;
    SetPacketReadBuffer(30000, 140000);
    EPartsInfoBuff.used = 0;
    EditArea = new (reinterpret_cast<u_long128 *>(EtcDataBuffer.Alloc(Quadwords<CEditArea>(4)))) CEditArea[4];
    pEditGround = new (reinterpret_cast<u_long128 *>(EtcDataBuffer.Alloc(Quadwords<CEditGround>(1)))) CEditGround;
    ObjParts = new (reinterpret_cast<u_long128 *>(EtcDataBuffer.Alloc(Quadwords<CMapParts>(24)))) CMapParts[24];
    MotionParts = new (reinterpret_cast<u_long128 *>(EtcDataBuffer.Alloc(Quadwords<CCharacter>(4)))) CCharacter[4];
    RiverParts = new (reinterpret_cast<u_long128 *>(EtcDataBuffer.Alloc(Quadwords<CMapParts>(16)))) CMapParts[16];
    RoadParts = new (reinterpret_cast<u_long128 *>(EtcDataBuffer.Alloc(Quadwords<CMapParts>(6)))) CMapParts[6];
    float near_clip = 10.0f;
    MGSetRenderInfo(800.0f, near_clip, 65535.0f);
    map_path = std::string(EditDataDir) + "mapinfo.cfb";
    EditMapInfo = reinterpret_cast<EDIT_MAP_INFO *>(EtcDataBuffer.Alloc(Quadwords<EDIT_MAP_INFO>(1)));
    EdInInfo = reinterpret_cast<EDIT_IN_INFO *>(EtcDataBuffer.Alloc(Quadwords<EDIT_IN_INFO>(1)));

    if (interior_test == 0) {
        LoadEditMapData(EditMapInfo, map_path.data(), MapNo);
    } else {
        LoadEditMapData(EditMapInfo, "gedit/interior/mapinfo.cfg", MapNo);
        LoadFile("gedit/interior/interior.cfg", read_buffer, &size);
        const u8 *text = reinterpret_cast<u8 *>(read_buffer);
        int       read_pos = 0;
        int       column = 0;
        int       row = 0;

        while (true) {
            const u8 c = text[read_pos++];

            if (read_pos > size) {
                break;
            }

            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' || c == '_') {
                interior_name[row][column] = static_cast<char>(c);
                column++;
                continue;
            }

            if (column > 1) {
                interior_name[row][column] = '\0';
                column = 0;
                row++;
            }

            if (row >= 63) {
                break;
            }
        }

        interior_name[row][0] = '\0';
    }

    SndSetReadBuffer(read_buffer);

    if (StartEventNo < 0) {
        int bgm = EditMapInfo->bgm_no;

        if (bgm >= 0) {
            if (old_main_mode == 5) {
                SndBgmLoad(0);
            } else {
                SndBgmLoad(bgm);
            }

            if (EditMapInfo->time_stop == 0) {
                EdSetBgmVol(SaveData->GetNowTime());
            } else {
                SndBgmPlay(0);
            }

            SndStep();
        }
    }

    int sound_set = EditMapInfo->sound_set_no;

    if (sound_set < 0) {
        if (SndGetNowSetNo() < 0) {
            SndSoundLoad(0);
        }
    } else {
        SndSoundLoad(sound_set);
    }

    EdInitSoundSrc();
    EdEventInfo.world_coord_enable = false;
    EdEventInfo.main_character = Chara;
    EdEventInfo.main_texture_animation = CharaTexAnimeData;
    EdEventInfo.main_texture_animation_count = 128;
    EdEventInfo.npcs = EdVillager;
    EdEventInfo.npc_count = 10;
    EdEventInfo.messages[0] = &EditMes1;
    EdEventInfo.villagers = EditMapInfo->villagers;
    CEditGround *ground = pEditGround;
    EdEventInfo.edit_ground = ground;
    EdEventInfo.fixed_parts_count = 64;
    EdEventInfo.fixed_parts = ground->fixed_parts;
    EdEventInfo.edit_parts_count = 24;
    EdEventInfo.edit_parts = ObjParts;
    EdEventInfo.player_texture_block = 8;
    EdEventInfo.npc_texture_block = 54;
    EdEventInfo.messages[0] = &EditMes1;
    EdEventInfo.messages[1] = &EditEventMes1;
    EdEventInfo.messages[4] = &EditSystemMes;
    EdInitEventParam();
    EdEventData = nullptr;
    simple_event = 0;
    LoadScript();
    now_fog = EditMapInfo->fog[0];
    MainCamera.SetFollow(0.0f, 0.0f, 0.0f);
    MainCamera.Step(10);
    MainCamera.SetSpeed(7.0f);
    EditCamera.SetFollow(0.0f, 0.0f, 0.0f);
    EditCamera.SetSpeed(8.0f);
    EditCamera.snap_range = 1.0f;
    EditCamera.Step(10);
    MainCamera.SetHeight(-10.0f);
    MainCamera.FollowOff();
    LoadTexture();

    if (interior_test == 0) {
        LoadGroundData();
    }

    EdSetDOF(reinterpret_cast<DEPTH_OF_FIELD_INFO *>(&EditMapInfo->dof_start_time));
    camera_dist_mode = 1;
    NowCamera = &MainCamera;
    EditMenuStatus.mode = EDIT_MENU_MODE_CLOSED;
    EditMenuStatus.parts = -1;
    DebugFont__3.texture_name = "font_buff";
    DebugFont__3.x = 16;
    DebugFont__3.y = 16;
    DebugFont__3.width = 280;
    DebugFont__3.height = SCREEN_HALF_HEIGHT;
    DebugFont__3.alpha = 64;
    EdDSetFont(&DebugFont__3);

    if (interior_test == 0) {
        if (old_main_mode != 7 || main_select_padrup != 0) {
            EditLoad();
        } else {
            save_path = std::string(EditDataDir) + "gdata0.edt";
            int loaded = LoadFile2(save_path.data(), read_buffer, nullptr, 0);

            for (int i = 0; i < 22; i++) {
                EditPartsInfo.parts[i].obtained = 1;
                EDITPARTS_INFO *info = EditPartsInfo.GetPartsInfo(i);

                for (int j = 0; j < 6; j++) {
                    info->elements[j].enabled = true;
                }
            }

            for (int i = 0; i < 36; i++) {
                EditElementInfo[i].element_no = i % 15 + 1;
            }

            short *element_data = SaveData->GetElemData(0);

            if (loaded != 0) {
                pEditGround->Load(reinterpret_cast<char *>(read_buffer));
            }

            pEditGround->MakePartsBox();

            for (int i = 0; i < 128; i++) {
                element_data[i] = -1;
            }
        }

        EditPartsObjectOnOff();
    }

    pEditGround->RemakeGrid();
    GameMode = ED_MODE_UNK_0;
    EdMoveCharaInit();
    end_counter = 0;
    door_open_cnt = 0;
    EdStepTimeFlag = 1;
    std::uintptr_t free_start = reinterpret_cast<std::uintptr_t>(DataBuffer__2.base) + DataBuffer__2.used * 16;
    int            free_quads = DataBuffer__2.limit - DataBuffer__2.used;

    if (free_quads < 326000) {
        ArenaOverflow("edit NPC buffer", DataBuffer__2.used, DataBuffer__2.limit);
    }

    const int alignment_quads = static_cast<int>(((0x40 - (free_start & 0x3F)) & 0x3F) / 16);

    if (free_quads <= alignment_quads + 4) {
        ArenaOverflow("edit NPC buffer alignment", free_quads, alignment_quads + 4);
    }

    free_start += static_cast<std::uintptr_t>(alignment_quads) * 16;
    free_quads -= alignment_quads;
    EdNPCBuffer.base = reinterpret_cast<u_char *>(free_start);
    EdNPCBuffer.limit = free_quads - 4;
    EdNPCBuffer.used = 0;
    EdVillagerBuffer.base = reinterpret_cast<u_char *>(free_start);
    EdVillagerBuffer.limit = free_quads - 4;
    EdVillagerBuffer.used = 0;
    EdNPCReadBuffer = reinterpret_cast<u_long128 *>(EdNPCBuffer.base + EdNPCBuffer.used * 16 + (free_quads >> 1) * 16);
    const auto read_address = reinterpret_cast<std::uintptr_t>(EdNPCReadBuffer);
    EdNPCReadBuffer = reinterpret_cast<u_long128 *>(read_address + ((0x40 - (read_address & 0x3F)) & 0x3F));

    EdCreateVillagerTable(EditMapInfo);
    EdInitVillagerControl();
    EdInitVillagerTable(NowTime, EditMapInfo);
    EdLoadMainChara("chara/c01d.chr", "info.cfg", &CharaBuffer);
    sceVu0FVECTOR start_position = {0.0f, 0.0f, 0.0f, 1.0f};
    sceVu0FVECTOR start_rotation = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR fade;
    GameMode = ED_MODE_WALK;
    EdGetFadeColor(fade);
    EdFadeIn(128, static_cast<float>(static_cast<int>(fade[0])), static_cast<float>(static_cast<int>(fade[1])), static_cast<float>(static_cast<int>(fade[2])));
    Chara->SetPosition(start_position);
    Chara->SetRotation(start_rotation);
    float yaw = start_rotation[1] - PI_SHORT;

    if (yaw < PI_SHORT) {
        yaw -= TWO_PI;
    }

    MainCamera.SetAngleSoon(yaw);
    MainCamera.SetFollow(start_position[0], 14.0f + start_position[1], start_position[2]);
    MainCamera.Step(-1);
    EffectTable__3 = reinterpret_cast<CEffect *>(EtcDataBuffer.Alloc(Quadwords<CEffect>(128)));
    EdEffectGroup.Initialize(EffectTable__3, 128);
    EdEffectGroup.Clear();
    EdInitMesParam();
    EditMes1.tex_block = 26;
    EditEventMes1.tex_block = 26;
    EditMes1.tex_buff = MesWinTexBuff_01;
    EditEventMes1.tex_buff = MesWinTexBuff_02;
    auto             *talk_mes = reinterpret_cast<short *>(EdMesBuffer.base + EdMesBuffer.used * 16);
    std::string       mes_path;
    std::string       sys_path;
    const std::string language = LanguageCode > LANG_JAPANESE ? "_" + std::to_string(LanguageCode) : "";
    sys_path = std::string(EditDataDir) + "fconv.bin";
    mes_path = std::string(EditDataDir) + EditMapName + "talk" + language + ".mes";

    const bool talk_loaded = LoadFile2(mes_path.data(), talk_mes, &mes_size, 0) != 0;

    if (!talk_loaded && LoadFile2(sys_path.data(), talk_mes, &mes_size, 0) == 0) {
        LoadFile("gedit/e01/fconv.bin", talk_mes, &mes_size);
    }

    EdMesBuffer.Alloc((mes_size >> 4) + 1);
    EditMes1.SetBuff(talk_mes);
    DialogNpcSetMessageFile(talk_loaded ? mes_path.data() : "");
    EdMesBuffer.Align64();
    auto *system_mes = reinterpret_cast<short *>(EdMesBuffer.base + EdMesBuffer.used * 16);
#ifdef PAL
    mes_path = std::string("gedit/system/editsys") + language + ".mes";
    LoadFile(mes_path.data(), system_mes, &mes_size);
#else
    LoadFile("gedit/system/editsys.bin", system_mes, &mes_size);
#endif
    EdMesBuffer.Alloc((mes_size >> 4) + 1);
    EditSystemMes.tex_buff = MesWinTexBuff_01;
    EditSystemMes.tex_block = 26;
    EditSystemMes.SetBuff(system_mes);
    static ClsMes name_mes;
    name_mes.fukidashi = false;
    name_mes.stay_frame = true;
    MonsterNameInit(&name_mes, system_mes, MesWinTexBuff_11);
    EditNameMes.tex_block = 26;
    EditNameMes.tex_buff = MesWinTexBuff_11;
    EditNameMes.SetBuff(system_mes);
    EdMesBuffer.Align64();
    auto       *window_mes = reinterpret_cast<short *>(EdMesBuffer.base + EdMesBuffer.used * 16);
    std::string window_path = "meswin/system14";
    int         language_no = LanguageCode;

    if (language_no > 0) {
        window_path += "_" + std::to_string(language_no);
    }
    window_path += ".mes";

    if (LoadFile2(window_path.data(), window_mes, &mes_size, 0) == 0) {
        LoadFile("meswin/system14e.bin", window_mes, &mes_size);
    }

    EdMesBuffer.Alloc((mes_size >> 4) + 1);
    EditMes1.SetBuff_system(window_mes);
    EditEventMes1.SetBuff_system(window_mes);
    EditNameMes.SetBuff_system(window_mes);
    EditSystemMes.SetBuff_system(window_mes);
    CommonMenuMes1.SetBuff_system(SystemMes);
    CommonMenuMes2.SetBuff_system(SystemMes);
    CommonMenuMes3.SetBuff_system(SystemMes);
    EdClearSystemMes();
    EdInitOpenItemBox(TreasureCursorOpen, TreasureCursor);
    exit_loop = 0;
    loop_counter = 0;
    key_counter = 0;
    sound_off_cnt = 1;
    clear_screen = 0;
    goto_dungeon = 0;
    key_lock = false;
    goto_menu = 0;
    draw_npc_cursor = 0;
    edit_mode_draw = 0;
    edit_mode_grd_draw = 0;
    depth_of_field = 1;
    edit_mode_lighting = 0;
    draw_sky = 1;
    goto_cmp_event = 0;
    goto_cmp_event_level = -1;
    change_time_event = 0;
    start_event_no = -1;
    start_system_event = -1;
    move_count = 0;
    EdBeforeInBgmNo = -1;
    EdDrawOffFlag = 0;
    EdDrawOffMap = 0;
    EdDrawOffMapShadow = 0;
    EdThunderEffectFlag = 1;
    EdPauseFlag = 0;
    debug_menu_mode = 0;
    EdSaveFrameImageInit();
    EdInitMenu(ED_MENU_RESET);
    BtSetMapJumpFloor(-1);
    EBInitialize();
    EdInitThunderEffect();
    EdInitDrawDay();
    InitWorkBuffer();
    int event_no = StartEventNo;

    if (event_no < 0) {
        RunEvent(128, nullptr);
    } else {
        RunEvent(event_no, nullptr);
    }

    StartEventNo = -1;
    SaveData->GetDngStatus()->cur_chara = 0;
    static int debug_flag_set = 0;

    if (debug_flag_set == 0) {
        EdDebugCameraFlag = 0;
        EdDebugMoveFlag = 0;
    }

    debug_flag_set = 1;
    ItemVolumeStep.CheckItemVolume();
    return 0;
}

// The work arena's start is rounded up to 64 bytes on the whole host pointer.
void InitWorkBuffer() {
    int     quads = EdNPCBuffer.used;
    u_char *free_start = EdNPCBuffer.base + quads * 16;
    quads = EdNPCBuffer.limit - quads;
    EdVillagerBuffer.base = free_start;
    EdVillagerBuffer.limit = quads;
    EdVillagerBuffer.used = 0;
    free_start = reinterpret_cast<u_char *>(((reinterpret_cast<std::uintptr_t>(free_start) >> 6) + 1) << 6);
    EdWorkBuffer.base = free_start;
    EdWorkBuffer.limit = quads - 4;
    EdWorkBuffer.used = 0;
    free_start = reinterpret_cast<u_char *>(read_buffer);
    EdMenuBuffer.base = free_start - 0x180000;
    EdMenuBuffer.limit = 0x3A2E0;
    EdMenuBuffer.used = 0;
}

namespace {

CFrame *GetCameraFrame(CMapParts *parts) {
    if (parts->camera_frame == nullptr) {
        return nullptr;
    }

    parts->camera_frame->SetPosition(parts->pos[0], parts->pos[1], parts->pos[2]);
    parts->camera_frame->SetRotation(parts->rotation.x, parts->rotation.y, parts->rotation.z);
    return parts->camera_frame;
}

// The flooded Treant clearing's extra chests, from forest progress 9: the first three chests' items again.
struct ExtraChest {
    int   item;
    float position[3];
    float yaw;
    int   flag;
};

constexpr ExtraChest kTreantChests[] = {
    {ITEM_GOURD,               {-20.0f, 0.0f, 195.0f}, 1.937f,                       365},
    {ITEM_FRUIT_OF_EDEN,       {125.0f, 0.1f, 95.0f},  -1.862f + 3.14159265f / 6.0f, 366},
    {ITEM_ATTACH_BEAST_BUSTER, {60.0f, 0.6f, 165.0f},  -1.862f + 3.14159265f / 2.0f, 367},
};

// Adds the flooded clearing's extra chests beside the part holding its Gourd chest (flag 360).
int AddTreantChests(CMapParts *parts, EPARTS_FUNC_DATA *functions, int function_count, ED_EVENT_POINT *points) {
    if (MapNo != 13) {
        return 0;
    }

    const EPARTS_FUNC_DATA *gourd = nullptr;

    for (int i = 0; i < function_count; i++) {
        if (functions[i].kind == EPARTS_FUNC_ITEM_BOX && functions[i].completion_flag == 360) {
            gourd = &functions[i];
            break;
        }
    }

    if (gourd == nullptr) {
        return 0;
    }

    constexpr std::size_t count = std::size(kTreantChests);
    auto                  extra = std::make_unique<EPARTS_FUNC_DATA[]>(count);

    for (std::size_t i = 0; i < count; i++) {
        const ExtraChest &chest = kTreantChests[i];
        extra[i] = *gourd;
        extra[i].completion_flag = chest.flag;
        extra[i].position[0] = chest.position[0];
        extra[i].position[1] = chest.position[1];
        extra[i].position[2] = chest.position[2];
        extra[i].rotation[1] = chest.yaw;
        extra[i].values[0] = static_cast<float>(chest.item);
        extra[i].values[2] = 9.0f;

        // Initialised closed on the map's first load, as LoadPTS does for the part's own flags.
        if (SaveData->GetMapInitFlag(MapNo, chest.flag) == 0) {
            SaveData->SetMapInitFlag(MapNo, chest.flag, 1);
            SaveData->SetMapFlag(MapNo, chest.flag, !static_cast<char>(extra[i].unk_28[0]));
        }
    }

    return EdInitEventPoint(parts, nullptr, extra.get(), static_cast<int>(count), points, 0x100);
}

} // namespace

// The .pts definition is decoded into host records (eparts_port.hpp) rather than relocated in place.
EPARTS_INFO_HEADER *LoadPTS(CMapParts *parts, unsigned int *archive, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime,
                            EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points,
                            CMapParts *shared) {
    auto *record = reinterpret_cast<EPARTS_ARCHIVE *>(archive);

    const char         *definition = reinterpret_cast<char *>(record) + record->info_offset;
    EPartsDiscHeader    source = EPartsReadHeader(definition);
    EPARTS_INFO_HEADER *header = EPartsLoad(definition, &EPartsInfoBuff);

    parts->func_count = header->func_count;
    // Nothing reads func_data back; it keeps the bits retail stored.
    parts->func_data = static_cast<s32>(reinterpret_cast<std::uintptr_t>(header->func));

    if (shared != nullptr) {
        CopyCMapParts(parts, shared, &DataBuffer__2);
    } else {
        char *names[9] = {nullptr};

        if (record->lod0_size > 0) {
            names[0] = reinterpret_cast<char *>(record) + record->lod0_offset;
        } else {
            return header;
        }

        if (record->lod1_size > 0) {
            names[1] = reinterpret_cast<char *>(record) + record->lod1_offset;
        }

        if (record->lod2_size > 0) {
            names[2] = reinterpret_cast<char *>(record) + record->lod2_offset;
        }

        if (record->lod3_size > 0) {
            names[3] = reinterpret_cast<char *>(record) + record->lod3_offset;
        }

        if (record->shadow_size > 0) {
            names[4] = reinterpret_cast<char *>(record) + record->shadow_offset;
        }

        if (record->collision_size > 0) {
            names[5] = reinterpret_cast<char *>(record) + record->collision_offset;
        }

        if (record->shade_size > 0) {
            names[6] = reinterpret_cast<char *>(record) + record->shade_offset;
        }

        if (record->extra_size > 0) {
            names[7] = reinterpret_cast<char *>(record) + record->extra_offset;
        }

        if (record->camera_size > 0) {
            names[8] = reinterpret_cast<char *>(record) + record->camera_offset;
        }

        LoadMapObject(parts, reinterpret_cast<u_int **>(names), &DataBuffer__2);
    }

    CFrame *frames[9] = {};
    CBoxVu0 bound;
    CBoxVu0 part_bound;

    for (int k = 0; k < 4; k++) {
        frames[k] = parts->frame[k];
    }

    frames[4] = parts->shadow_frame;
    frames[5] = parts->GetCollisionFrame();
    frames[6] = parts->shade_frame;
    frames[7] = parts->ripple_frame;
    frames[8] = GetCameraFrame(parts);

    if (frames[0] == nullptr) {
        return nullptr;
    }

    frames[0]->GetBoundBox(&bound, 1);
    CFrame *frame = parts->GetCollisionFrame();

    if (frame != nullptr) {
        frame->GetBoundBox(&part_bound, 1);
        VectorMaxMin(bound.max, bound.min, bound.max, bound.min, part_bound.max, part_bound.min);
    }

    frame = GetCameraFrame(parts);

    if (frame != nullptr) {
        frame->GetBoundBox(&part_bound, 1);
        VectorMaxMin(bound.max, bound.min, bound.max, bound.min, part_bound.max, part_bound.min);
    }

    parts->bound = bound;

    for (int k = 0; k < 8; k++) {
        int anime_no = info->anime[k];

        if (anime_no > 0) {
            InitObjAnime(frames, &anime[anime_no]);
        }
    }

    EPARTS_FUNC_DATA *funcs = header->func;

    EditMapInfo->event_count += EdInitEventPoint(parts, info->events, funcs, header->func_count, points, 0x100);
    EditMapInfo->event_count += AddTreantChests(parts, funcs, header->func_count, points);

    for (int i = 0; i < header->func_count; i++) {
        EPARTS_FUNC_DATA *func = &header->func[i];

        if (func->completion_flag > 0 && SaveData->GetMapInitFlag(MapNo, func->completion_flag) == 0) {
            SaveData->SetMapInitFlag(MapNo, func->completion_flag, 1);
            SaveData->SetMapFlag(MapNo, func->completion_flag, !static_cast<char>(func->unk_28[0]));
        }

        EnterPartsEffect(parts, func, effects, 0x40);

        if (EditMapInfo->obj_anime_count < 128 &&
            InitObjAnime(frames, 9, func, &anime[EditMapInfo->obj_anime_count]) != 0) {
            EditMapInfo->obj_anime_count++;
        }

        if (func->kind == EPARTS_FUNC_OBJ_TIMER) {
            if (EditMapInfo->object_timer_count >= 128) {
                continue;
            }

            EDIT_OBJECT_TIMER *timer = &timers[EditMapInfo->object_timer_count++];
            std::copy_n(func->frame_name, std::size(timer->name) - 1, timer->name);
            timer->name[std::size(timer->name) - 1] = '\0';
            timer->start_time = ConvertTime(func->start_time);
            timer->end_time = ConvertTime(func->end_time);
            timer->object = parts;
        }

        if (func->kind == EPARTS_FUNC_VILLAGER) {
            if (pEditGround->people_count >= 128) {
                continue;
            }

            pEditGround->people[pEditGround->people_count++] = func;
            func->parts = parts;
        }
    }

    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 1.0f};

    position[0] = source.position[0];
    position[1] = source.position[1];
    position[2] = source.position[2];
    parts->SetPosition(position);
    parts->SetRotation(source.rotation[0], source.rotation[1], source.rotation[2]);
    return header;
}

// The editor's own rotation in 12.4 units: offsets turned with the axes swapped at angle 0, the far
// edges a sixteenth short, y halved for the field after turning. Always point-sampled.
void set2DSpriteRot(sceVif1Packet * /*packet*/, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel,
                    int pivot_x, int pivot_y, float angle, unsigned char alpha) {
    if (texture == nullptr) {
        return;
    }

    float x[4];
    float y[4];

    x[0] = x[2] = static_cast<float>(pivot_x * -16);
    x[1] = x[3] = static_cast<float>(((screen.width - pivot_x) << 4) - 1);
    y[0] = y[1] = static_cast<float>(pivot_y * -16);
    y[2] = y[3] = static_cast<float>(((screen.height - pivot_y) << 4) - 1);

    for (int i = 0; i < 4; i++) {
        const float turned_x = y[i] * std::cos(angle) + x[i] * std::sin(angle);
        const float turned_y = x[i] * std::cos(angle) - y[i] * std::sin(angle);

        x[i] = static_cast<float>(screen.x) + static_cast<float>(static_cast<int>(turned_x)) / 16.0f;
        y[i] = static_cast<float>(screen.y) + static_cast<float>(static_cast<int>(0.5f * turned_y)) / 8.0f;
    }

    const auto u0 = static_cast<float>(texel.x);
    const auto v0 = static_cast<float>(texel.y);
    const auto v1 = static_cast<float>(texel.y + texel.height);

    // The second corner's u spans the screen width, not the texel width, as in retail.
    std::array<gfx::Vertex2D, 4> strip = {
        draw2d::Vertex(x[0], y[0], 0.0f, u0, v0, 0x80, 0x80, 0x80, alpha),
        draw2d::Vertex(x[1], y[1], 0.0f, static_cast<float>(texel.x + screen.width), v0, 0x80, 0x80, 0x80,
                       alpha),
        draw2d::Vertex(x[2], y[2], 0.0f, u0, v1, 0x80, 0x80, 0x80, alpha),
        draw2d::Vertex(x[3], y[3], 0.0f, static_cast<float>(texel.x + texel.width), v1, 0x80, 0x80, 0x80,
                       alpha),
    };
    draw2d::DrawTextured(gfx::Primitive::TriangleStrip, strip, texture->tex0, 0x41, draw2d::SpriteState());
    draw2d::RestoreTestZbuf();
}
