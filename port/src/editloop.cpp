#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
#include "dngstatusdata.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "edit_in.hpp"
#include "editarea.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editmapscript.hpp"
#include "editpartsinfo.hpp"
#include "effect.hpp"
#include "effectgroup.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gfx/gfx.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "menu_misc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "objectframe.hpp"
#include "rect.hpp"
#include "runeffect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"

/* Retail editloop.cpp: town-script parsing, map construction and pre-event editor state. */

void BtSetMapJumpFloor(int floor);

#include "battlemenu.hpp"
#include "editmenu.hpp"
#include "effectmacro.hpp"
#include "fishing.hpp"
#include "gameutil.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menuetc.hpp"
#include "nowload.hpp"
#include "vutext.hpp"
#include "weaponlevelup.hpp"
#include "wind.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's EditLoop without its VU1 program upload, which aborts in the port. EditLoop calls the
// static RunEvent and RunSystemEvent, so they come along. It also calls editloop's MainDraw, which
// is kept here as EditMainDraw.
static void EditMainDraw();

extern int start_event_no;
extern CCameraFollow EventCamera;
extern int          start_system_event;
void EdSetFlag();
void EditExit();
void LoadScript();
void InitWorkBuffer();
int FadeOutToEvent(int event_no, int level);
void CheckKeyLock();
void PauseOffCheck();
int cat_end();
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

/* The ground the player has built on one map, as the save holds it. */
struct ED_GRD_DATA {
    u8  unk_00[0x64];
    int complete_event_seen; /**< Zero until the map's own build event has been seen. */
};

void EBDraw();

/**
 * Starts a map event and copies an optional source camera into the event camera.
 */
static int RunEvent(int event_no, CCamera *camera) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;

    if (start_event_no > 0 && simple_event == 0) {
        return 0;
    }

    if (camera != NULL) {
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
static void RunSystemEvent(int event_no, CCamera *camera) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;

    start_event_no = -1;

    if (start_system_event <= 0) {
        if (camera != NULL) {
            camera->GetPos(position);
            camera->GetRef(reference);
            EventCamera.SetPos(position);
            EventCamera.SetRef(reference);
            EventCamera.FollowOff();
        }

        start_system_event = event_no;
    }
}

/**
 * Runs one frame of the editor and reports what it is to do next.
 *
 * @mangled EditLoop__Fv
 * @address 0x1797E0
 * @size 0x1FE8
 */
PC_OVERRIDE int EditLoop() {
    goto_return_menu = 0;

    if (EdPadDown(0x800, 4) != 0) {
        goto_return_menu = 1;
    }

    if (MapNo < TOWN_COUNT && loop_counter == 60 && GameMode == ED_MODE_WALK) {
        EdWalkToEditMes(0);
    }

    [[maybe_unused]] static int end_count = -1;

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
    EdExchangeInfo.camera = (CCameraFollow *) NowCamera;
    EdExchangeInfo.event_marker = TreasureCursor;
    EdExchangeInfo.system_effect = SystemEffect;
    EdSetFlag();
    EdEventInfo.current_time = NowTime;
    CEditGround *ground = pEditGround;

    ground->focus_parts_id = -1;

    if (((int *) SaveData->GetConfigData())[4] != 0) {
        EditMes1.text_rate_set = 0.6f;
    } else {
        EditMes1.text_rate_set = 0.3f;
    }

    int mode = GameMode;

    if (mode == 0xC) {
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
            EdInitVilager(EdVillagerInfo, EdExchangeInfo.ground, NULL);
            EdInitVilagerPosition(EdVillager, EdVillagerInfo, pEditGround, NULL);
            MGBeginFrame();

            MGFillBox(CRect_i_(0, 0, 0x2800, SCREEN_HALF_HEIGHT * 16), 0, 0, 0, 0x80);

            ED_EVENT_PARAM entry;

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
                SndSetCamera((CCamera *) &MainCamera);
                EdDoorCloseSe(EdInteriorDoorSound, fix_chara_pos);
            }

            if (MapNo < TOWN_COUNT) {
                EdWalkToEditMes(60);
            }

            if (EdEventInfo.outside_map_no >= 0) {
                RunEvent(EdEventInfo.outside_map_no, NULL);
            }
        }

        return 0;
    }

    EdInteriorFlag = 0;

    if (interior_test != 0) {
        static int   top = 0;
        static int   select = 0;
        [[maybe_unused]] static int   cur = 0;
        static char *menu[64];

        for (int i = 0; i < 64; i++) {
            menu[i] = interior_name[i];
        }

        int count;

        for (count = 0; menu[count][0] != '\0'; count++) {
        }

        GamePad.SetAutoRepeat(PAD_UP | PAD_DOWN, 20, 5);

        if (GamePad.Down(PAD_DOWN) != 0) {
            select++;
        }

        if (GamePad.Down(PAD_UP) != 0) {
            select--;
        }

        if (select < 0) {
            select = 0;
        }

        if (select >= count) {
            select = count - 1;
        }

        top = 0;

        if (select >= 14) {
            top = select - 13;
        }

        char *mark[2] = {" ", ">"};

        DebugFont__3.length = 0;

        for (int i = top; i < top + 14; i++) {
            DebugFont__3.length += sprintf(&DebugFont__3.text[DebugFont__3.length], "%s %s\n", mark[i == select], menu[i]);
        }

        TexManager.ReloadTexture(GetVif1Packet(), 0x1F);
        DebugFont__3.Draw();

        if (GamePad.Down(PAD_CIRCLE) != 0) {
            float period_time = NowTime / 3.0f;

            EdMapJump((int) period_time, menu[select]);
            GameMode = ED_MODE_MAP_JUMP;

            while (ReadBGSync() != 0) {
            }

            strcpy(interior_map_name, menu[select]);
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
                NowCamera = (CCamera *) &MainCamera;
            } else {
                NowCamera = (CCamera *) &ViewCamera;
            }

            if (EdCheckViewMode() == 0) {
                NowCamera->Step(1);
            }

            break;
        case ED_MODE_WALK:
            if (loop_counter > 0) {
                TownMouseBegin(&MainCamera, pEditGround, MapNo, GameMode);
                if (EdCheckViewMode() == 0) TownZoomBegin(&MainCamera, pEditGround, MapNo);
                MainMode();
            }

            MainEditMode();

            if (EdCheckViewMode() == 0) {
                NowCamera = (CCamera *) &MainCamera;
            } else {
                NowCamera = (CCamera *) &ViewCamera;
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
                        RunEvent(goto_cmp_event, NULL);
                        EdFadeIn(60, 0.0f, 0.0f, 0.0f);
                    }

                    goto_cmp_event = 0;
                }
            }

            break;
        case ED_MODE_EVENT:
            NowCamera = (CCamera *) &EventCamera;
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
            NowCamera = (CCamera *) &TalkCamera;
            TalkMode();
            MainEditMode();
            NowCamera->Step(1);
            draw_npc_cursor = 0;
            draw_clock = 0;
            break;
        case ED_MODE_GEORAMA:
            GamePad.SetAutoRepeat(PAD_DPAD, 20, 5);
            NowCamera = (CCamera *) &EditCamera;
            MainEditMode();
            EditMode();
            NowCamera->Step(1);
            pEditGround->EditAreaClip(NowCamera, -1.0f);

            if (goto_cmp_event != 0) {
                goto_return_menu = 0;

                if (EdFadeOutCheck() != 0) {
                    RunEvent(goto_cmp_event, NULL);
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
                NowCamera = (CCamera *) &MainCamera;
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

            NowCamera = (CCamera *) &MainCamera;
            Chara->GetVelocity()->y = 0.0f;

            CCharacter *chara;
            chara = Chara;

            chara->motion_no = 0;
            chara->motion_flags = 0;
            chara->motion_speed = -1.0f;
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

                    if (file != NULL) {
                        u_int *pack = (u_int *) file->buffer;

                        EdLoadVillager(pack, EdVillager[i].resource_name, &EdVillager[i], &EdVillagerBuffer);
                    }
                }

                EdInitVilagerPosition(EdVillager, EdVillagerInfo, pEditGround, NULL);
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
            NowFocusParts = NULL;
            OldFocusParts = NULL;
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

                int          requested_count = 0;
                ED_GRD_DATA *ground_data = (ED_GRD_DATA *) SaveData->GetGrdData(MapNo);

                if (ground_data != NULL && ground_data->complete_event_seen == 0) {
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

    if (move_count < 0) {
        move_count = 0;
    }

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

            if (GamePad.Down(PAD_R3) != 0 || EdDebugRunEventNo > 0) {
                debug_menu_mode = 0;
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
                debug_flag = 0;
            }

            if (GameMode == ED_MODE_GEORAMA) {
                sceVu0FVECTOR cursor_pos;
                char          text[128];

                sceVu0CopyVector(cursor_pos, ECursorFrame->position);
                EdDPrintVector("pos = ", cursor_pos);
                int area = pEditGround->GetAreaCode(cursor_pos[0], cursor_pos[1], cursor_pos[2]);
                sprintf(text, "area = %d\n", area);
                EdDPrint(text);

                if (area >= 0) {
                    CVector3_i_ grid;

                    pEditGround->areas[area]->GetPos(&grid, cursor_pos[0], cursor_pos[1], cursor_pos[2]);
                    sprintf(text, "pos(grid) = (%d,%d)\n", grid.x, grid.z);
                    EdDPrint(text);
                    sprintf(text, "id = %d\n", pEditGround->areas[area]->GetPartsID(grid.x, grid.z));
                    EdDPrint(text);
                }
            } else {
                EdDPrintChara((CMainChara *) Chara);
            }

            EdDPrintCamera(NowCamera);
            EdDDrawFont();
        }
    }

    if (GameMode != ED_MODE_GEORAMA_MENU && GameMode != ED_MODE_GEORAMA && loop_counter > 10) {
        sceVu0FVECTOR eye_pos;
        sceVu0FVECTOR eye_dir;
        CBoxVu0       box;
        CMapParts    *nearby[128];

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

        int near_count = pEditGround->GetNearParts(nearby, 64, &box, NULL);

        for (int i = 0; i < near_count; i++) {
            if (nearby[i]->subtype == MAP_PARTS_SUBTYPE_RIVER && pEditGround->suppress_water != 0) {
                nearby[i] = NULL;
            }
        }

        if (sound_off_cnt <= 0) {
            EdSetSoundSrcVol(NowTime, nearby, near_count, eye_pos, eye_dir);
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

    if (event_next < 0) {
        event_next = 0;
    }

    if (event_next == 1) {
        start_event_no = 0x96;
    }

    if (GameMode != ED_MODE_EVENT) {
        if (((CMainChara *) Chara)->move_info.landed != 0 && loop_counter > 10) {
            if (((CMainChara *) Chara)->move_info.ground_poly.attr.ground_kind > 0) {
                RunEvent(((CMainChara *) Chara)->move_info.ground_poly.attr.ground_kind, NowCamera);
                ((CMainChara *) Chara)->move_info.ground_poly.attr.ground_kind = 0;
            }
        }
    }

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
            if (EdEventInit(start_system_event, &EdWorkBuffer, (char *) EdSystemEventData) != 0) {
                EdInitMesParam();
                GameMode = ED_MODE_EVENT;
            }
        } else {
            if (EdEventInit(start_event_no, &EdWorkBuffer, (char *) EdEventData) != 0) {
                EdInitMesParam();
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

    if ((unsigned int) (GameMode - 9) < 2U && EdPadDown(0x800, 0xFFFF) != 0) {
        GameMode = old_mode;
        EdSePlay((ED_SOUND_ID) 2, -1);
        PlayTimeCountFlag(1);
    } else if (goto_return_menu != 0) {
        if (GameMode == ED_MODE_WALK || GameMode == ED_MODE_FISHING) {
            old_mode = GameMode;
            oldGameMode = GameMode;
            GameMode = ED_MODE_RETURN_MENU_WALK;
            EdSePlay((ED_SOUND_ID) 1, -1);
            PlayTimeCountFlag(0);
        } else if (GameMode == ED_MODE_GEORAMA) {
            old_mode = GameMode;
            GameMode = ED_MODE_RETURN_MENU_GEORAMA;
            EdSePlay((ED_SOUND_ID) 1, -1);
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

    if (end_counter < 0) {
        end_counter = 0;
    }

    SndStep();
    loop_counter++;
    key_counter++;
    sound_off_cnt--;

    if (sound_off_cnt < 0) {
        sound_off_cnt = 0;
    }

    DebugFont__3.length = 0;

    if (GamePad.On(PAD_CIRCLE) != 0) {
        cat_end();
    }

    return 0;
}

/**
 * Draws the editor's world for one frame.
 *
 * @mangled MainDraw__Fv
 * @address 0x17B7D0
 * @size 0x11D8
 */
static void EditMainDraw() {
    ED_EVENT_INFO *event;
    int            shadow_on;
    int            detail;
    int           *marks;
    int            i;

    if (EdDrawOffFlag == 0) {
        if (EdPauseFlag != 0 || (unsigned int) (GameMode - 9) < 2U) {
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

            float far_lod;
            float near_lod = 2.0f;
            far_lod = 0.0f;
            float mid_lod = 0.0f;

            if (edit_mode_draw != 0) {
                near_lod = 3.0f;
                far_lod = 3.0f;
                mid_lod = 3.0f;
            }

            pEditGround->Draw(NowTime, 1, (int) far_lod, (int) near_lod, (int) mid_lod, (int) near_lod);

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
            pEditGround->Draw(NowTime, 2, (int) far_lod, (int) near_lod, 0, 3);

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
                    water = *(sceGsTex0 *) &TexManager.GetTexture("water_buff", -1)->tex0;
                    MGMoveImage(&frame, screen, &water, 0, 0, 0);
                    MainCamera.GetRef(ref);
                    zbuf = mgZBuffer;
                    zbuf.bits.zmsk = 1;
                    MGSetGsZBUF(&zbuf);
                    pEditGround->DrawWaterSurface(NowCamera);
                    MGSetGsZBUF(&mgZBuffer);
                    break;
                }
            }

            pEditGround->DrawRipple(0x15);


            if (depth_of_field != 0) {
                int dof_level = 2;

                if (((int *) SaveData->GetConfigData())[6] != 0) {
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

                for (i = 0; i < 10; i++) {
                    CNPCharacter *villager = &EdVillager[i];
                    unsigned char drawable = villager->initialized != 0;

                    if (drawable != 0) {
                        drawable = villager->draw_enabled != 0;
                    }

                    if (drawable != 0) {
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
                event = NULL;
                shadow_on = 1;
                detail = 3;
                int marks_store[10];
                marks = marks_store;
                float      chara_pos[4];
                CBoxVu0    box;
                CMapParts *near_parts[0x40];

                if (GameMode == ED_MODE_TALK || EdSystemMesCheck() != 0) {
                    shadow_on = 0;
                }

                if (EdCheckViewMode() != 0) {
                    detail = 0;
                }

                if (GameMode == ED_MODE_DOOR_OPEN) {
                    marks = NULL;
                } else {
                    for (i = 0; i < 10; i++) {
                        marks_store[i] = 3;
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

                int found = pEditGround->GetNearParts(near_parts, 0x40, &box, NULL);

                Chara->ClearPointLight();

                for (i = 0; i < found; i++) {
                    CMapParts *parts = near_parts[i];
                    CFrame    *frame = (CFrame *) parts->frame[0];

                    if (frame != NULL) {
                        float transform[4];

                        parts->GetPosition(transform);
                        frame->SetPosition(transform);
                        near_parts[i]->GetRotation(transform);
                        frame->SetRotation(transform[0], transform[1], transform[2]);

                        for (int j = 0; j < 0x18; j++) {
                            EDIT_EFFECT_INFO *effect = near_parts[i]->effect[j];

                            if (near_parts[i]->effect_on[j] != 0 && effect != NULL && (effect->kind == EDIT_EFFECT_FIRE_LARGE || effect->kind == EDIT_EFFECT_FIRE_MEDIUM || effect->kind == EDIT_EFFECT_FIRE_SMALL) && effect->frame != NULL && CheckEditEffect(effect, NowTime) != 0) {
                                float light_pos[4];

                                effect->frame->GetWorldPosition(light_pos, effect->offset);

                                float distance = DistVector(chara_pos, light_pos);

                                if (distance <= 40.0f) {
                                    float brightness = 80.0f + (float) (rand() % 48);

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
                MGBeginDrawShadow(*(sceGsTex0 *) &TexManager.GetTexture("shadow_buff", -1)->tex0);
                pEditGround->DrawShadow(1, 0.0f, 100000.0f);
                MGEndDrawShadow(0x20);
            } else {
                if (EditMapInfo->shadow_level > 0) {
                    MGBeginDrawShadow(*(sceGsTex0 *) &TexManager.GetTexture("shadow_buff", -1)->tex0);
                    pEditGround->DrawShadow(0, shadow_range[0], shadow_range[1]);
                    MGEndDrawShadow(EditMapInfo->shadow_mode);
                }

                if (EditMapInfo->shadow_level >= 2) {
                    MGBeginDrawShadow(*(sceGsTex0 *) &TexManager.GetTexture("shadow_buff", -1)->tex0);
                    pEditGround->DrawShadow(0, shadow_range[1], shadow_range[2]);
                    MGEndDrawShadow(EditMapInfo->shadow_mode_2);
                }
            }

            MGSetPLight(saved_light, light_colour);
        }

        for (i = 0; i < 4; i++) {
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
                    CEditGround *ground = pEditGround;

                    ground->DrawEffect((CCameraFollow *) NowCamera, NowTime, &EdEffectGroup);
                    EdEffectGroup.Draw();
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
                MGFillBox(screen, (int) fade[0] & 0xFF, (int) fade[1] & 0xFF, (int) fade[2] & 0xFF, (int) fade[3] & 0xFF);
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

            if (info != NULL) {
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


// Retail's MoveCamera, the georama view's, with the mouse turning it beside the right stick.
PC_OVERRIDE void MoveCamera(CCameraFollow *camera) {
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

// Retail's EdDrawClock, kept in the window's top right corner.
PC_OVERRIDE void EdDrawClock(int x, int y) {
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

PC_OVERRIDE int LoadTexture() {
    int        entered;
    int        image;
    u_long128 *buffer = (u_long128 *) (DataBuffer__2.base + DataBuffer__2.used * 16);

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
    char mes_path[64] = "meswin/mes_tex.pak";

    if (LanguageCode > LANG_JAPANESE) {
        sprintf(mes_path, "meswin/mes_tex_%d.pak", LanguageCode);
    }

    LoadFile(mes_path, read_buffer, NULL);
    wait_now_loading_vsync();
    mes_blocks[3].name = (char *) GetPackFile(read_buffer, "gaiji.img", NULL);
    mes_blocks[4].name = (char *) GetPackFile(read_buffer, "fuki256.img", NULL);
    mes_blocks[5].name = (char *) GetPackFile(read_buffer, "syst04.img", NULL);
    TexManager.LoadTextureBlock(-1, mes_blocks);

    LOADTEXTURE_INFO2 blocks[64] = {};

    char system_path[64] = "gedit/system/esys.pak";

    if (LanguageCode > LANG_JAPANESE) {
        sprintf(system_path, "gedit/system/esys_%d.pak", LanguageCode);
    }

    LoadFile(system_path, read_buffer, NULL);
    wait_now_loading_vsync();
    TexManager.EnterIMGFile((u_char *) GetPackFile(read_buffer, "e01t02.img", NULL), -1, 0, 0);
    TexManager.EnterIMGFile((u_char *) GetPackFile(read_buffer, "cursor.img", NULL), -1, 0, 0);

    LOADTEXTURE_INFO2 system_blocks[] = {
        {"#water_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",  0x15, 0},
        {"#shadow_buff#640#" HALF_BUFFER_HEIGHT_STR "#4", 0x16, 0},
        {"#blender#640#" HALF_BUFFER_HEIGHT_STR "#4",     0x18, 0},
        {"#font_buff#640#" HALF_BUFFER_HEIGHT_STR "#4",   0x1F, 0},
        {"img/system.img",                                0x14, 0},
        {"img/pause.img",                                 0x14, 0},
        {"s_eff.img",                                     0x14, 0},
        {"whatsday.img",                                  0x14, 0},
        {"img/ankfont.img",                               0x1F, 0},
        {"#frame_image#640#" SCREEN_HEIGHT_STR "#4",      0x13, 0},
    };

    memcpy(blocks, system_blocks, sizeof(system_blocks));
    blocks[4].name = (char *) GetPackFile(read_buffer, "system.img", NULL);
    blocks[5].name = (char *) GetPackFile(read_buffer, "pause.img", NULL);
    blocks[6].name = (char *) GetPackFile(read_buffer, "s_eff.img", NULL);
    blocks[7].name = (char *) GetPackFile(read_buffer, "whatsday.img", NULL);
    blocks[8].name = (char *) GetPackFile(read_buffer, "ankfont.img", NULL);
    TexManager.LoadTextureBlock(-1, blocks);
    wait_now_loading_vsync();

    CFrameAttr cursor_attr;

    cursor_attr.use_color = true;
    CharaCursor0 = LoadMDSFile(GetPackFile(read_buffer, "cursor01.mds", NULL), &EtcDataBuffer, 0, NULL, NULL);
    CharaCursor0->SetAttr(cursor_attr, 1, 0x200);
    CharaCursor1 = LoadMDSFile(GetPackFile(read_buffer, "cursor02.mds", NULL), &EtcDataBuffer, 0, NULL, NULL);
    CharaCursor1->SetAttr(cursor_attr, 1, 0x200);

    CharaCursor1->SetScale(2.5f, 2.5f, 2.5f);
    CharaCursor2 = LoadMDSFile(GetPackFile(read_buffer, "bic.mds", NULL), &EtcDataBuffer, 0, NULL, NULL);
    CharaCursor2->SetAttr(cursor_attr, 1, 0x200);
    TreasureCursor = LoadMDSFile(GetPackFile(read_buffer, "ibox_0.mds", NULL), &EtcDataBuffer, 0, NULL, NULL);
    TreasureCursorOpen = LoadMDSFile(GetPackFile(read_buffer, "ibox_1.mds", NULL), &EtcDataBuffer, 0, NULL, NULL);

    CFrameAttr box_attr;

    box_attr.clip_enable = false;
    box_attr.fog_enable = true;

    if (TreasureCursor != NULL) {
        TreasureCursor->SetAttr(box_attr, 1, 0x44);
    }

    if (TreasureCursorOpen != NULL) {
        TreasureCursorOpen->SetAttr(box_attr, 1, 0x44);
    }

    SystemEffect[0].texture = TexManager.GetTexture("s_ef01", -1);

    char             stay_path[128];
    LOADTEXTURE_INFO map_blocks[64];
    CRect_i_         effect_rect;

    effect_rect.x = 0;
    effect_rect.y = 0;
    effect_rect.width = 0x20;
    effect_rect.height = 0x20;
    SystemEffect[0].texel = effect_rect;
    SystemEffect[0].alpha_blend = true;
    SystemEffect[0].disable_z_write = true;

    GetEditDataDir(stay_path);
    strcat(stay_path, "img.pak");

    u_int *menu_data = (u_int *) (EdNPCBuffer.base + EdNPCBuffer.used * 16);

    LoadFileMenuData("stayframe.img", menu_data);
    wait_now_loading_vsync();
    TexManager.EnterFixTextureZ((u_char *) menu_data);
    StayTexture = TexManager.GetTexture("stayframe", -1);

    if (LoadFile2(stay_path, menu_data, NULL, 0) != 0) {
        wait_now_loading_vsync();

        entered = 0;
        image = 0;

        while (EditMapInfo->images[image].name[0] != '\0') {
            blocks[entered].name = (char *) GetPackFile(menu_data, EditMapInfo->images[image].name, NULL);
            blocks[entered].block_no = EditMapInfo->images[image].type;
            blocks[entered].mipmap = EditMapInfo->images[image].number;
            entered++;
            image++;
        }

        blocks[entered].name = NULL;
        blocks[entered].block_no = 0;
        blocks[entered].mipmap = false;
        TexManager.LoadTextureBlock(-1, blocks);
        TexAnime.Initialize(TexAnimeData, 0x40);

        int   anime_size;
        char *anime_cfg = (char *) GetPackFile(menu_data, "texanime.cfg", &anime_size);

        if (anime_cfg != NULL) {
            for (int i = 0; i < 64; i++) {
                TexAnimeData[i].Initialize();
            }

            TexAnime.LoadCFGFile(anime_cfg, anime_size);
        }
    } else {
        TexAnime.Initialize(NULL, 0);

        entered = 0;
        image = 0;

        while (EditMapInfo->images[image].name[0] != '\0') {
            map_blocks[entered].name = EditMapInfo->images[image].name;
            map_blocks[entered].block_no = EditMapInfo->images[image].type;
            map_blocks[entered].mipmap = EditMapInfo->images[image].number;
            image++;
            entered++;
        }

        map_blocks[entered].name = EditMapInfo->images[image].name;
        map_blocks[entered].block_no = EditMapInfo->images[image].type;
        map_blocks[entered].mipmap = EditMapInfo->images[image].number;
        TexManager.LoadTextureBlock(-1, map_blocks, read_buffer);
    }

    TexManager.buffer_size = TexManager.buffer_used;
    DataBuffer__2.Alloc((int) (TexManager.buffer + TexManager.buffer_size - buffer) + 16);
    DataBuffer__2.Align64();
    return 0;
}
