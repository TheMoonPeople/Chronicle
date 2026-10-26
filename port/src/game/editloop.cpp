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

CTexture  *StayTexture;
int        NowEditMap;
u_long128 *EdNPCReadBuffer;
int        EdStepTimeFlag;
int        EdInteriorFlag;
int        EdInteriorPartsNo;
int        EdInteriorJumpID;
int        EdInteriorDoorSound;
int        EdInteriorStartEvent;
int        EdBeforeInBgmNo;

ED_EXCHANGE_INFO  EdExchangeInfo;
EDIT_MENU_STATUS  EditMenuStatus;
CEditPartsInfo    EditPartsInfo;
EDIT_ELEMENT_INFO EditElementInfo[36];
CDataAlloc2<1>    EdInteriorBuffer(-1);
CDataAlloc2<1>    EdNPCBuffer(-1);
CDataAlloc2<1>    EdVillagerBuffer(-1);
CDataAlloc2<1>    EdEventBuffer(-1);
CDataAlloc2<1>    EdEventExBuffer(-1);
CDataAlloc2<1>    EdMesBuffer(-1);
CDataAlloc2<1>    EdScriptBuffer(-1);
CDataAlloc2<1>    EdExtMessBuffer(-1);
CDataAlloc2<1>    EdWorkBuffer(-1);
CDataAlloc2<1>    EdMenuBuffer(-1);
char              EditDataDir[0x100];
char              EditMapName[0x20];
char              EdInteriorName[0x20];

CEffectGroup EdEffectGroup;
/** Pool the editor's effect group plays its effects out of. */
CEffect *EffectTable__3;
/** Unreferenced word of the editor's small data. */
static int D_002A2844;
/** Unreferenced word of the editor's small data. */
static int D_002A2848;
/** Unreferenced word of the editor's small data. */
static int     D_002A284C;
EDIT_MAP_INFO *EditMapInfo;
char          *EdEventData;
char          *EdSystemEventData;
int            EdDrawOffFlag;
int            EdDrawOffMap;
int            EdDrawOffMapShadow;
int            EdThunderEffectFlag;
int            EdPauseFlag;
int            clear_screen;
int            key_lock;
int            goto_dungeon;
int            exit_loop;
/** Frames the editor loop has run since the map was entered. */
int loop_counter;
/** Frames the pad has been held for. */
int key_counter;
/** Whether the loop is leaving for the main menu. */
int goto_menu;
/** Whether the loop is returning from the main menu. */
int goto_return_menu;
int sound_off_cnt;
int debug_menu_mode;
int goto_cmp_event;
int goto_cmp_event_level;
int change_time_event;
int draw_npc_cursor;
int draw_clock;
int edit_mode_draw;
int edit_mode_grd_draw;
int depth_of_field;
int edit_mode_lighting;
int draw_sky;
/** Number of the map event about to run. */
int start_event_no;
/** Number of the system event about to run. */
int          start_system_event;
int          move_count;
int          interior_test;
char       **interior_name;
int          oldGameMode;
CEditGround *pEditGround;
CEditArea   *EditArea;
CMapParts   *ObjParts;
CMapParts   *RiverParts;
CMapParts   *RoadParts;
/** Animated map parts. */
CCharacter *MotionParts;
/** Camera the map is drawn through. */
CCamera *NowCamera;
/** Time of day on the editor clock. */
float      NowTime;
CFrameVu1 *SkyBackFrame;
CFrame    *CharaCursor0;
CFrame    *CharaCursor1;
CFrame    *CharaCursor2;
CFrameVu1 *TreasureCursor;
CFrameVu1 *TreasureCursorOpen;
int        end_counter;
int        bgm_vol;
int        bgm_play_flag;
int        bgm_play_start;
/** Frames the opening door has been playing for. */
int        door_open_cnt;
int        door_open;
int        fix_pos_enble;
int        talk_villager;
int        chg_time_cnt;
int        NowSelectAngle;
int        OldSelectAngle;
float      NowCursorRotY;
CMapParts *NowFocusParts;
CMapParts *OldFocusParts;
int        DrawPartsNameCount;

CWind         EdWind;
ClsMes        EditMes1;
ClsMes        EditMes2;
ClsMes        EditEventMes1;
ClsMes        EditEventMes2;
ClsMes        EditNameMes;
ClsMes        EditSystemMes;
ClsMes        EditHelpMes;
CNPCharacter  EdVillager[10];
VILLAGER_INFO EdVillagerInfo[10];
/** Font the editor's debug text is drawn with. */
CDebugFont     DebugFont__3;
EDIT_FOG_INFO  now_fog;
CDataAlloc2<1> EPartsInfoBuff(-1);
CMainChara     MainChara;
CRunEffect     RunEffect;
/** Camera that follows the player. */
CCameraFollow MainCamera(60.0f, 20.0f, 0.0f, 4.0f);
CCameraFollow EditCamera(60.0f, 20.0f, 0.0f, 4.0f);
CCameraFollow IntroCamera(60.0f, 20.0f, 0.0f, 4.0f);
/** Camera used while the player talks to a villager. */
CCameraFollow TalkCamera(60.0f, 20.0f, 0.0f, 4.0f);
/** Camera used while an editor event controls the viewpoint. */
CCameraFollow EventCamera(60.0f, 20.0f, 0.0f, 4.0f);
/** Camera used by the free-look view. */
CCameraFollow ViewCamera(60.0f, 20.0f, 0.0f, 4.0f);
CFrameVu1 *SkyFrame[4];
CFrame    *SunFrame[4];
/** Sprites the system events draw with. */
C3DSprite SystemEffect[8];
/** Where the player is held while a door plays its motion. */
sceVu0FVECTOR fix_chara_pos;
/** Which way the player faces while a door plays its motion. */
sceVu0FVECTOR fix_chara_rot;
sceVu0FVECTOR fix_camera_pos;
char          interior_map_name[0x40];
float         NowPartsCursorPos[4];
/** Model of the cursor the editor places parts with. */
static CFrameVu1 ecursorframe;
sceVu0FVECTOR    NowCursorPos;
sceVu0FVECTOR    NextCursorPos;
/** Texture animation the map plays. */
CTextureAnime TexAnime;
/** Records of the map's texture animation. */
CTexAnimeData TexAnimeData[64];
/** Records of the characters' texture animation. */
CTexAnimeData CharaTexAnimeData[0x80];
/** Arena the map's data is read into before the editor carves it up. */
CDataAlloc2<1> DataBuffer__2(-1);
CDataAlloc2<1> CharaBuffer(-1);
CDataAlloc2<1> InteriorBuffer(-1);
CDataAlloc2<1> EtcDataBuffer(-1);

/** Character the camera and events treat as the player. */
CCharacter *Chara = &MainChara;
/** How far the following camera sits from the player. */
int     camera_dist_mode = 1;
int     NowSelectParts = -1;
CFrame *ECursorFrame = &ecursorframe;
int     PartsNameNum = -1;

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
 * Copies the current editor resource directory into a caller buffer.
 */
void GetEditDataDir(char *name) {
    strcpy(name, EditDataDir);
}

/**
 * Reads the shared and map-specific event scripts into the script arena.
 */
void LoadScript() {
    char  common_path[0x40];
    char  map_path[0x40];
    char  language[0x1c];
    int   size;
    char *map_script;

    EdScriptBuffer.used = 0;
    EdEventData = (char *) (EdScriptBuffer.base + EdScriptBuffer.used * 16);
    sprintf(language, "_%d.mes", LanguageCode);
    GetEditDataDir(common_path);
    GetEditDataDir(map_path);
    strcat(common_path, "event.stb");
    strcat(map_path, EditMapName);
    strcat(map_path, language);

    if (LoadFile2(common_path, EdEventData, &size, 0) != 0) {
        EdScriptBuffer.Alloc((size >> 4) + 1);
        map_script = (char *) (EdScriptBuffer.base + EdScriptBuffer.used * 16);

        if (LoadFile2(map_path, map_script, &size, 0) != 0) {
            EdScriptBuffer.Alloc((size >> 4) + 1);
        } else {
            map_script = NULL;
        }

        EdSetEventScript(EdEventData, map_script, &EdScriptBuffer);
    } else {
        EdEventData = NULL;
    }

    EdScriptBuffer.Align64();
    EdSystemEventData = (char *) (EdScriptBuffer.base + EdScriptBuffer.used * 16);

    if (LoadFile2("gedit/system/event.stb", EdSystemEventData, &size, 0) != 0) {
        EdScriptBuffer.Alloc((size >> 4) + 1);
        return;
    }

    EdSystemEventData = NULL;
}

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
 * Begins a fade into a comparison event unless a higher-priority request is active.
 */
int FadeOutToEvent(int event_no, int level) {
    if (EdDebugEventEnable == 0) {
        return 0;
    }

    if (goto_cmp_event != 0) {
        if (level > goto_cmp_event_level) {
        } else {
            return 0;
        }
    }

    goto_cmp_event = event_no;
    goto_cmp_event_level = level;
    float zero = 0.0f;
    EdFadeOut(60, zero, zero, zero);
    return 1;
}

/**
 * Writes the editor's part progress and clock back to the save data.
 */
void EditSave() {
    int map_no = MapNo;

    if (map_no < TOWN_COUNT && map_no >= 0) {
        pEditGround->Save(NowEditMap, SaveData);
        EditPartsInfo.Save(NowEditMap, SaveData);
    }

    EDIT_MAP_INFO *info = EditMapInfo;

    if (info != NULL && info->time_stop == 0) {
        SaveData->SetNowTime(NowTime);
    }
}

/**
 * Restores the editor's part progress and clock from the save data.
 */
void EditLoad() {
    if (MapNo < TOWN_COUNT) {
        EditPartsInfo.Load(NowEditMap, SaveData, 0);
        pEditGround->Load(NowEditMap, SaveData);
    }

    EditPartsObjectOnOff();
    pEditGround->MakePartsBox();
    NowTime = 0.01f * (float) ((int) (100.0f * SaveData->GetNowTime()) % 1200);
}

/**
 * Saves the map and quiets everything the editor was running before it leaves.
 */
void EditExit() {
    sceVu0FVECTOR fade;

    EditSave();
    MGSetBGColor(0.0f, 0.0f, 0.0f, 128.0f);
    StopAllSound();

    while (ReadBGSync() != 0) {
    }

    SndAmbientStop();

    if (EdEventInfo.map_jump_bgm_stop != 0) {
        SndBgmFadeOutStop();
        SndBgmStop();
        SndBgmInit();
    }

    EdStopSoundSrc();
    SndStopAllSe();
    SndStep();
    MGScisioringForce(0);
    GamePad.KeyLock(0);
    EdGetFadeColor(fade);
    MGSetBGColor(fade);
}

/**
 * Carves the villager, work and menu arenas out of what the NPC arena left.
 */
void InitWorkBuffer();

/**
 * Reports whether any modal editor event currently blocks normal input.
 */
int run_event_check() {
    if (goto_dungeon != 0 || goto_menu != 0 || goto_cmp_event != 0 || debug_menu_mode != 0 || EdSystemMesCheck() != 0 || EdCheckItemOver() != 0) {
        return 1;
    }

    return 0;
}

/**
 * Disables editor input while an event is active or the loop is starting.
 */
void CheckKeyLock() {
    if (run_event_check() != 0) {
        key_lock = true;
    }

    if (key_lock != 0 || loop_counter < 2) {
        EdSetKeyMode(0);
    }
}

/**
 * Settles the editor's input lock and the overlays it allows for this frame.
 */
void EdSetFlag() {
    key_lock = false;

    if (run_event_check() != 0) {
        key_lock = true;
    }

    if (key_lock != 0) {
        EdSetKeyMode(0);
    } else {
        EdSetKeyMode(0xFFFF);
    }

    draw_npc_cursor = GameMode == ED_MODE_WALK;

    if (key_lock != 0) {
        draw_npc_cursor = 0;
    }

    EDIT_CONFIG_VIEW *config = (EDIT_CONFIG_VIEW *) SaveData->GetConfigData();

    draw_clock = !(bool) config->clock_hidden;

    if (key_lock != 0) {
        draw_clock = 0;
    }
}

/**
 * Returns a flag from the current town's persistent map state.
 */
int EdGetMapFlag(int flag_no);

/**
 * Writes a flag in the current town's persistent map state.
 */
int EdSetMapFlag(int flag_no, int value);

/**
 * Cancels editor pause while another modal event is active.
 */
void PauseOffCheck() {
    if (run_event_check() != 0) {
        goto_return_menu = 0;
    }
}

/**
 * Requests that the main editor loop terminate.
 */
int EdExitLoop() {
    return exit_loop = 1;
}

/**
 * Suppresses every editor scene draw pass.
 */
void EdDrawOffAll() {
    EdDrawOffFlag = 1;
}

/**
 * Restores normal editor scene drawing.
 */
void EdDrawOnAll() {
    EdDrawOffFlag = 0;
}

/**
 * Returns the editor clock converted to its displayed hour value.
 */
float EdGetClock() {
    return InvertTime(NowTime);
}

/**
 * Sets the editor clock from a valid displayed hour value.
 */
void EdSetClock(float time) {
    if (time < 0.0f || !(time <= 24.0f)) {
        return;
    }

    NowTime = ConvertTime(time);
}

/**
 * Applies the editor-specific layout and reveal settings to its message windows.
 */
void EdInitMesParam() {
    float             text_rate;
    EDIT_CONFIG_VIEW *config = (EDIT_CONFIG_VIEW *) SaveData->GetConfigData();

    if (config->message_speed != 0) {
        text_rate = 0.6f;
    } else {
        text_rate = 0.3f;
    }

    EditMes1.columns = 21;
    EditMes1.rows = 4;
    EditMes1.text_rate = text_rate;
    EditMes1.text_rate_set = text_rate;
    EditMes1.tex_block = 26;
    EditMes1.style = MES_EDGE_NONE;
    EditMes1.page_arrow = false;
    EditMes1.centre_rows = true;
    float fade_speed = 0.1f;
    EditMes1.fade_speed = fade_speed;
    EditMes1.page_arrow = true;

    EditEventMes1.columns = 21;
    EditEventMes1.rows = 4;
    EditEventMes1.text_rate = text_rate;
    EditEventMes1.text_rate_set = text_rate;
    EditEventMes1.tex_block = 26;
    EditEventMes1.style = MES_EDGE_NONE;
    EditEventMes1.page_arrow = false;
    EditEventMes1.centre_rows = true;
    EditEventMes1.fade_speed = fade_speed;
    EditEventMes1.page_arrow = true;

    EditSystemMes.Preset(MES_PRESET_SYSTEM);
    EditSystemMes.fukidashi = false;
    EditSystemMes.stay_frame = true;
    EditSystemMes.text_rate = 0.0f;
    EditSystemMes.text_rate_set = 0.0f;
    EditSystemMes.page_arrow = false;
    EditSystemMes.fade_speed = 1.0f;
    EditSystemMes.columns = 26;
    EditSystemMes.rows = 1;
    EditSystemMes.centre_rows = false;
    EditSystemMes.style = MES_EDGE_TABLE;

    EditNameMes.Preset(MES_PRESET_NAME);
    EditNameMes.tex_block = 26;
    EditNameMes.rows = 4;
    EditNameMes.fukidashi = false;
    EditNameMes.stay_frame = false;
    EditNameMes.style = MES_EDGE_TABLE;
}

/**
 * Builds every buffer, camera, message window and map the editor loop runs on.
 */
int EditInit(void *param);

/**
 * Returns the sum of the editor category indices from zero through nine.
 */
int cat_end() {
    int total = 0;

    for (int category = 0; category < 10; category++) {
        total += category;
    }

    return total;
}

/**
 * Runs one frame of the editor and reports what it is to do next.
 *
 * @mangled EditLoop__Fv
 * @address 0x1797E0
 * @size 0x1FE8
 */
int EditLoop();

/**
 * Draws the editor's map cursor, and the plate that names the part under it.
 */
void ParamDraw() {
    sceVu0IVECTOR screen;

    if (EdDebugParamDrawOff != 0) {
        return;
    }

    TexManager.ReloadTexture(Vif1Packet, 20);
    VectorInterpolate(NowCursorPos, NowCursorPos, NextCursorPos, 4.0f, INTERPOLATE_FRACTION);
    NowCursorPos[3] = 1.0f;
    MGRotTransPers2D(screen, NowCursorPos, 0);
    CRect_i_ cursor(screen[0] - 10, screen[1] - 32, 32, 32);
    set2DSprite(Vif1Packet, TexManager.GetTexture("syst08", -1), cursor, 0, 0);
    screen[0] = 320;
    screen[1] = SCREEN_HALF_HEIGHT;

    if (NowFocusParts != NULL && DrawPartsNameCount == 0) {
        int name_count = PartsNameNum;

        if (name_count > 0) {
            int top;
            int step;
            int few = 6;
            int many = 9;
            step = 20;
            top = 0;

            if (LanguageCode > LANG_JAPANESE) {
                few = 12;
                many = 18;
                step = 10;
            }

            if (name_count < 7) {
                top += 48;
                step = step + ((step * (few - name_count)) >> 1);
            } else {
                step = step + ((step * (many - name_count)) >> 1);
            }

            setbilinear(0);
            CRect_i_ plate;
            CRect_i_ source;
            int      plate_y = screen[1];
            int      text_y = plate_y + 14;
            EditNameMes.line_pos[0].x = screen[0] + step;
            EditNameMes.line_pos[0].y = text_y;
            source.x = 0;
            source.y = top;
            source.width = 220;
            source.height = 48;
            plate.x = screen[0];
            plate.y = screen[1];
            plate.width = 220;
            plate.height = 48;
            set2DSprite(Vif1Packet, TexManager.GetTexture("pnplate", -1), plate, source, 0x60);
        }
    }
}

/**
 * Enables or disables the cursor drawn over the controlled character.
 */
void EdSetCharaCursor(int on) {
    draw_npc_cursor = on;
}

/**
 * Draws the villager cursors, the attention marker and the event points.
 */
void EdDrawSysCursor(ED_EVENT_POINT *points, int count) {
    if (EdDebugParamDrawOff != 0) {
        return;
    }

    static float offset = 2.5f;
    offset -= 0.1f;

    if (offset < 0.0f) {
        offset = 2.5f;
    }

    SetMonsterNameDrawFlag(0);

    for (int i = 0; i < 10 && draw_npc_cursor != 0; i++) {
        int shown = EdVillager[i].initialized != 0 && EdVillager[i].draw_enabled != 0;

        if (shown) {
            sceVu0FVECTOR position;
            sceVu0CopyVector(position, EdVillager[i].pos);
            position[1] += 2.0f + EdVillager[i].body_height;

            if (EdInteriorFlag == 0 && EdVillager[i].CheckDraw() == 0 && EdVillager[i].talk_target == 0) {
                if (MapNo != TOWN_MUSKA_LACKA || EdVillager[i].villager_id != 8) {
                    position[1] += offset;
                    CharaCursor1->SetPosition(position);
                    float yaw = 0.0f;
                    CharaCursor1->SetRotation(0.0f, yaw, 0.0f);
                    MGDraw(CharaCursor1);
                }
            } else if (EdVillager[i].CheckDraw() != 0 && EdVillager[i].talk_target != 0 && EdCheckViewMode() == 0) {
                CharaCursor0->SetPosition(position);
                MGDraw(CharaCursor0);
                position[3] = 1.0f;
                sceVu0IVECTOR screen;

                if (MGRotTransPers2D(screen, position, 0) != 0) {
                    MonsterNameMake(MapNo * 100 - 2000 + EdVillager[i].villager_id);
                    MonsterNamePosSet(screen[0], screen[1] - 70);
                    SetMonsterNameDrawFlag(1);
                }
            }
        }
    }

    if (EdEventInfo.draw_exclamation_mark != 0) {
        if (CharaCursor2 != NULL) {
            sceVu0FVECTOR position;
            Chara->GetPosition(position);
            static float a = 0.0f;
            position[1] += 3.0f + Chara->body_height + 0.5f * sinf(a);
            a += 0.1f;

            if (!(a <= PI_SHORT)) {
                a -= TWO_PI;
            }

            CharaCursor2->SetPosition(position);
            CFrame *marker = CharaCursor2;
            marker->attr.billboard = 2;
            MGDraw(CharaCursor2);
        }

        EdEventInfo.draw_exclamation_mark = false;
    }

    EdEventPointDraw(points, count, NowTime);
    EdDrawOpenItemBox();
}

float draw_day_cnt;
int   draw_day_flag;

/**
 * Starts the day-transition overlay at its initial counter value.
 */
void EdStartDrawDay() {
    draw_day_cnt = 5.0f;
    draw_day_flag = 1;
}

/**
 * Resets the day-transition overlay state.
 */
void EdInitDrawDay() {
    draw_day_cnt = 0.0f;
    draw_day_flag = 0;
}

/**
 * Gives a rectangle that starts at zero.
 *
 * The four words come out back to front, which `CRect_i_`'s own constructor
 * does not do, and `rect.hpp` cannot serve both orders from one constructor
 * without moving the generated-name counter in every unit that includes it.
 */
class CRectZero : public CRect_i_ {
public:
    CRectZero() {
        height = 0;
        width = 0;
        y = 0;
        x = 0;
    }

    CRectZero(s32 x, s32 y, s32 width, s32 height) {
        this->x = x;
        this->y = y;
        this->width = width;
        this->height = height;
    }
};

void DrawDay() {
    if (draw_day_flag != 0) {
        int       week;
        int       x;
        int       width;
        int       alpha;
        int       day;
        int       digit[4];
        CTexture *texture;
        int       count;
        int       drawn;
        int       remain;
        int       value;

        draw_day_cnt -= 0.02f;

        if (draw_day_cnt < 0.0f) {
            draw_day_cnt = 0.0f;
            draw_day_flag = 0;
        }

        day = SaveData->GetDay() + 1;
        week = EdGetTime(NowTime);
        week++;

        if (week > 3) {
            week = 0;
        }

        x = 0x140;

        if (draw_day_cnt > 4.0f) {
            alpha = (int) (128.0f * (5.0f - draw_day_cnt));
        } else if (draw_day_cnt > 1.0f) {
            alpha = 0x80;
        } else {
            alpha = (int) (128.0f * draw_day_cnt);
        }

        count = 1;
        digit[3] = day / 1000;
        digit[2] = day % 1000 / 100;
        digit[1] = day % 100 / 10;
        digit[0] = day % 10;
        width = 0xA8;

        if (day >= 10) {
            width += 0x18;
            count++;
        }

        if (day >= 100) {
            width += 0x18;
            count++;
        }

        if (day >= 1000) {
            width += 0x18;
            count++;
        }

        x -= width >> 1;
        setbilinear(0);
        texture = TexManager.GetTexture("whatsday", -1);

        CRect_i_  title(0x30, 0x58, 0x30, 0x18);
        CRect_i_  nichi(0x64, 0x40, 0x10, 0x18);
        CRect_i_  weekday(0, 0x40, 0x18, 0x18);
        CRect_i_  dai(0, 0x58, 0x30, 0x18);
        CRect_i_  bar_left(0, 0x70, 0x10, 0x10);
        CRect_i_  bar_middle(0x10, 0x70, 0x10, 0x10);
        CRect_i_  bar_right(0x20, 0x70, 0x10, 0x10);
        CRect_i_  word(0x78, 0x62, 0x65, 0x20);
        CRect_i_  word_end(0xDD, 0x62, 0x22, 0x20);
        CRectZero week_name[4];

        switch (LanguageCode) {
            case LANG_ENGLISH_US:
            case LANG_ENGLISH_UK:
                week_name[0].x = 0x78;
                week_name[0].y = 2;
                week_name[0].width = 0x50;
                week_name[0].height = 0x20;
                week_name[1].x = 0x78;
                week_name[1].y = 0x22;
                week_name[1].width = 0x68;
                week_name[1].height = 0x20;
                week_name[2].x = 0x78;
                week_name[2].y = 0x42;
                week_name[2].width = 0x38;
                week_name[2].height = 0x20;
                week_name[3].x = 0xC8;
                week_name[3].y = 0x42;
                week_name[3].width = 0x38;
                week_name[3].height = 0x20;
                break;
            case LANG_GERMAN:
                word.x = 0x78;
                word.y = 0x62;
                word.width = 0x88;
                word.height = 0x20;
                word_end.x = 0;
                word_end.y = 0;
                word_end.width = 0;
                word_end.height = 0;
                week_name[0].x = 0x78;
                week_name[0].y = 2;
                week_name[0].width = 0x50;
                week_name[0].height = 0x20;
                week_name[1].x = 0x78;
                week_name[1].y = 0x22;
                week_name[1].width = 0x76;
                week_name[1].height = 0x20;
                week_name[2].x = 0x78;
                week_name[2].y = 0x42;
                week_name[2].width = 0x46;
                week_name[2].height = 0x20;
                week_name[3].x = 0xC4;
                week_name[3].y = 0x42;
                week_name[3].width = 0x40;
                week_name[3].height = 0x20;
                break;
            case LANG_SPANISH:
                word.x = 0x78;
                word.y = 0x62;
                word.width = 0x88;
                word.height = 0x20;
                word_end.x = 0;
                word_end.y = 0;
                word_end.width = 0;
                word_end.height = 0;
                week_name[0].x = 0x78;
                week_name[0].y = 2;
                week_name[0].width = 0x50;
                week_name[0].height = 0x20;
                week_name[1].x = 0x78;
                week_name[1].y = 0x22;
                week_name[1].width = 0x3E;
                week_name[1].height = 0x20;
                week_name[2].x = 0x78;
                week_name[2].y = 0x42;
                week_name[2].width = 0x46;
                week_name[2].height = 0x20;
                week_name[3].x = 0x78;
                week_name[3].y = 0x42;
                week_name[3].width = 0x46;
                week_name[3].height = 0x20;
                break;
            case LANG_FRENCH:
                word.x = 0x78;
                word.y = 0x62;
                word.width = 0x60;
                word.height = 0x20;
                word_end.x = 0;
                word_end.y = 0;
                word_end.width = 0;
                word_end.height = 0;
                week_name[0].x = 0x78;
                week_name[0].y = 2;
                week_name[0].width = 0x40;
                week_name[0].height = 0x20;
                week_name[1].x = 0x78;
                week_name[1].y = 0x22;
                week_name[1].width = 0x8C;
                week_name[1].height = 0x20;
                week_name[2].x = 0x78;
                week_name[2].y = 0x42;
                week_name[2].width = 0x32;
                week_name[2].height = 0x20;
                week_name[3].x = 0xAA;
                week_name[3].y = 0x42;
                week_name[3].width = 0x34;
                week_name[3].height = 0x20;
                break;
            case LANG_ITALIAN:
                word.x = 0x78;
                word.y = 0x62;
                word.width = 0x44;
                word.height = 0x20;
                word_end.x = 0;
                word_end.y = 0;
                word_end.width = 0;
                word_end.height = 0;
                week_name[0].x = 0x78;
                week_name[0].y = 2;
                week_name[0].width = 0x50;
                week_name[0].height = 0x20;
                week_name[1].x = 0x78;
                week_name[1].y = 0x22;
                week_name[1].width = 0x76;
                week_name[1].height = 0x20;
                week_name[2].x = 0x76;
                week_name[2].y = 0x42;
                week_name[2].width = 0x32;
                week_name[2].height = 0x20;
                week_name[3].x = 0xA4;
                week_name[3].y = 0x42;
                week_name[3].width = 0x38;
                week_name[3].height = 0x20;
                break;
        }

        CRectZero week_word;
        week_word = week_name[week];
        CRectZero cell(0, 0, 0x18, 0x20);
        CRectZero number;

        if (texture != NULL) {
            switch (LanguageCode) {
                case LANG_JAPANESE:
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, title.width, title.height), title, alpha);
                    x += 0x30;
                    break;
                case LANG_ENGLISH_US:
                case LANG_ENGLISH_UK:
                    width = week_word.width + (word_end.width + (word.width + ((count - 1) * 32)) + 0x48);
                    x = 0x140 - (int) (width >> 1);
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, word.width, word.height), word, alpha);
                    x += word.width + 0xC;
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, word_end.width, word_end.height), word_end, alpha);
                    x += word_end.width + 0xC;
                    break;
                case LANG_FRENCH:
                case LANG_GERMAN:
                case LANG_ITALIAN:
                case LANG_SPANISH:
                    width = week_word.width + (word_end.width + (word.width + ((count - 1) * 32)) + 0x38);
                    x = 0x140 - (int) (width >> 1);
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, word.width, word.height), word, alpha);
                    x += word.width + 0xC;
                    break;
            }

            int n;

            for (n = count; n > 0; n--) {
                number = cell;
                value = digit[n - 1];

                number.x += number.width * (value % 5);
                number.y += number.height * (value / 5);
                set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE2, number.width, number.height), number, alpha);
                x += number.width;
            }

            switch (LanguageCode) {
                case LANG_JAPANESE:
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, dai.width, dai.height), dai, alpha);
                    x += dai.width;
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, nichi.width, nichi.height), nichi, alpha);
                    x += nichi.width;
                    weekday.x += week * 0x18;
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, weekday.width, weekday.height), weekday, alpha);
                    break;
                case LANG_ENGLISH_US:
                case LANG_ENGLISH_UK:
                case LANG_FRENCH:
                case LANG_GERMAN:
                case LANG_ITALIAN:
                case LANG_SPANISH:
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, nichi.width, nichi.height), nichi, alpha);
                    x += nichi.width;
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, week_word.width, week_word.height), week_word, alpha);
                    break;
            }

            drawn = 0;
            x = 0x134 - (int) (width >> 1);
            set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xFC, bar_left.width, bar_left.height), bar_left, alpha);
            drawn += bar_left.width;
            x += bar_left.width;
            remain = width - (bar_right.width >> 2);

            while (drawn < width - bar_right.width) {
                set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xFC, bar_middle.width, bar_middle.height), bar_middle, alpha);
                x += bar_middle.width;
                remain -= bar_middle.width;
                drawn += bar_middle.width;
            }

            bar_right.x += bar_right.width - remain;
            bar_right.width = remain;
            set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xFC, bar_right.width, bar_right.height), bar_right, alpha);
        }
    }
}

/**
 * Draws the editor's clock, day display, event cursors, and pause overlay.
 */
void DrawSysGra();

/**
 * Draws the town-clock face and its hand at the current editor time.
 */
void EdDrawClock(int x, int y);

/**
 * Selects the main character and advances normal character movement.
 */
void MainMode() {
    Chara = &MainChara;
    MoveChara();
}

/**
 * Changes whether a named child frame participates in drawing.
 */
int FrameOnOff(CFrame *root, char *name, int on) {
    if (root == NULL) {
        return 0;
    }

    if (name == NULL) {
        return 0;
    }

    CFrame *frame = root->SearchFrame(name);

    if (frame == NULL) {
        return 0;
    }

    frame->attr.draw_on = on;
    return 0;
}

/**
 * Synchronizes object visibility for every active editable map part.
 */
void EditPartsObjectOnOff() {
    int parts = EditPartsInfo.GetNextParts(-1);

    while (parts >= 0) {
        EdPartsObjectOnOff(&ObjParts[parts], EditPartsInfo.GetPartsInfo(parts), 0);
        parts = EditPartsInfo.GetNextParts(parts);
    }

    pEditGround->RequestCheck();
}

/**
 * Advances the editable ground effects, collision boxes, cursor and object visibility.
 */
void EditMode() {
    pEditGround->EffectTask();
    pEditGround->MakePartsBox();
    MoveEditCursor();
    EditPartsObjectOnOff();
}

/**
 * Runs the editing mode: the cursor, the part list and the placement rules.
 *
 * @mangled MainEditMode__Fv
 * @address 0x17DEA0
 * @size 0x7A4
 */
void MainEditMode() {
    if (EdEventInfo.lighting_override != 0) {
        MGSetPLight(EdEventInfo.light_direction, EdEventInfo.light_color);
        MGSetAmbient(EdEventInfo.ambient_color);
    } else {
        EdSetLightParam(NowTime, edit_mode_lighting, EditMapInfo, SkyBackFrame);
    }

    pEditGround->StepWater();

    sceVu0FVECTOR wind;

    sceVu0CopyVector(wind, EditMapInfo->wind);

    if (EdEventInfo.wind[3] > 0.0f) {
        sceVu0CopyVector(wind, EdEventInfo.wind);
    }

    float         left_x = GamePad.GetLXf2();
    float         left_y = GamePad.GetLYf2();
    float         camera_angle = MainCamera.GetAngle();
    sceVu0FVECTOR direction;

    direction[0] = wind[0] + (left_x * cosf(camera_angle) + left_y * sinf(camera_angle));
    direction[2] = wind[2] + (left_y * cosf(camera_angle) - left_x * sinf(camera_angle));
    direction[1] = wind[1];
    direction[3] = 0.0f;
    EdWind.SetDir(direction);
    EdWind.SetVelocity(wind[3]);
    EdWind.Step();
    Chara->wind = (int) &EdWind;

    for (int i = 0; i < 4; i++) {
        MotionParts[i].Step();
    }

    for (int i = 0; i < EditMapInfo->obj_anime_count; i++) {
        OBJ_ANIME_SEQ *anime = &EditMapInfo->work.obj_anime[i];

        if (anime->completion_flag <= 0 || EdGetMapFlag(anime->completion_flag) == 0) {
            ObjAnimePlay(anime);
        }
    }

    float saved_time = SaveData->GetNowTime();
    int   clock_kept = 1;
    int   old_hour = EdGetTime(NowTime);

    if (NowTime != SaveData->GetNowTime()) {
        clock_kept = 0;
    }

    SaveData->SetNowTime(NowTime);
    static int time_flag = 1;

    float time_step = 0.0011133334f;

    if (((int *) SaveData->GetConfigData())[3] == 0) {
        time_step /= 2.0f;
    }

    if (change_time_event != 0) {
        time_step = 0.0f;
    }

    if (time_flag != 0 && GameMode == ED_MODE_WALK) {
        SaveData->AddNowTime(time_step);
    }

    if (GamePad.Down2(PAD_DOWN) != 0) {
        time_flag = (u8) !time_flag;
    }

    if (!(NowTime < 12.0f)) {
        NowTime = 0.0f;
    }

    if (GamePad.On2(PAD_RIGHT) != 0) {
        SaveData->AddNowTime(0.021f);
    }

    if (GamePad.On2(PAD_LEFT) != 0) {
        SaveData->AddNowTime(-0.02f);
    }

    int hour_set = 0;

    if (GamePad.Down2(PAD_UP) != 0) {
        NowTime = 1.0f + (float) (int) NowTime;

        if (!(NowTime < 12.0f)) {
            NowTime = 0.0f;
        }

        hour_set = 1;
        SaveData->SetNowTime(NowTime);
    }

    NowTime = SaveData->GetNowTime();

    if (EditMapInfo->time_stop != 0) {
        NowTime = 0.0f;
        SaveData->SetNowTime(saved_time);
    }

    int new_hour = EdGetTime(NowTime);

    if (clock_kept != 0 && time_flag != 0 && old_hour != new_hour && hour_set == 0) {
        if (MapNo < 4) {
            change_time_event = 1;
        } else {
            EdStartDrawDay();
        }
    }

    if (GameMode == ED_MODE_WALK) {
        ItemVolumeStep.LoopStep(60);

        if (sound_off_cnt <= 0) {
            if (EditMapInfo->time_stop == 0) {
                EdSetBgmVol(NowTime);
            } else {
                SndBgmPlay(0);
            }

            PlayAmbient(NowTime);
        }
    }

    int night = EdGetTime(NowTime) == ED_TIME_NIGHT;

    static int parts = 0;

    int level;
    int day_on = 1;
    int night_on = 2;

    if (night != 0) {
        day_on = night_on;
        night_on = 1;
    }

    char day_window[10] = "win1";
    char night_window[10] = "win2";

    for (level = 0; level < 4; level++) {
        CMapParts *obj = &ObjParts[parts];
        CFrame    *frame = obj->frame[level];

        FrameOnOff(frame, day_window, day_on);
        FrameOnOff(frame, night_window, night_on);
    }

    parts++;

    if (!(parts < 24)) {
        parts = 0;
    }

    static int now_obj = 0;

    EDIT_OBJECT_TIMER *timers = EditMapInfo->work.object_timers.timers;

    for (int i = 0; i < 2; i++, now_obj++) {
        if (!(now_obj < EditMapInfo->object_timer_count)) {
            now_obj = 0;
        }

        EDIT_OBJECT_TIMER *timer = &timers[now_obj];

        if (timer->name[0] != '\0') {
            int on = 1;

            if (EdCheckTime(NowTime, timer->start_time, timer->end_time) == 0) {
                on = 2;
            }

            if (timer->object != NULL) {
                timer->object->FrameObjectOnOff(timer->name, on);
            } else if (timer->frame != NULL) {
                CFrame *found = timer->frame->SearchFrame(timer->name);

                if (found != NULL) {
                    found->attr.draw_on = on;
                }
            }
        }
    }
}

/**
 * Steps the player while a door plays its motion, holding them and the camera
 * at the place the door fixed.
 */
void OpenDoorMode() {
    Chara->SetVelocity(CVector3_f_(0.0f, 0.0f, 0.0f));
    Chara->Step();
    Chara->ShadowStep();
    Chara->ClothStep(0);

    if (door_open) {
        Chara->SetPosition(fix_chara_pos);
        Chara->SetRotation(fix_chara_rot[0], fix_chara_rot[1], fix_chara_rot[2]);
        MainCamera.SetPos(fix_camera_pos);
        MainCamera.SetRef(fix_chara_pos[0], 14.0f + fix_chara_pos[1], fix_chara_pos[2]);
    }
}

/**
 * Steps the villager the player is talking to, and leaves the mode when the
 * conversation asks for a menu or an event.
 */
void TalkMode() {
    int i;
    int event_no;

    if (talk_villager < 0 || talk_villager >= 10) {
        GameMode = ED_MODE_WALK;
        return;
    }

    CCharacter *chara = Chara;
    chara->motion_no = 0;
    chara->motion_flags = 0;
    chara->motion_speed = -1.0f;
    Chara->SetVelocity(CVector3_f_(0.0f, 0.0f, 0.0f));
    Chara->Step();
    Chara->ShadowStep();
    Chara->ClothStep(0);

    for (i = 0; i < 10; i++) {
        EdVillager[i].alpha_step_override = 128;
        EdVillager[i].near_camera = false;
    }

    int result = EdTalkMode(Chara, &TalkCamera, 0, &event_no);

    if (result != 0) {
        if (result == 2) {
            goto_menu = ED_MENU_SHOP;
        }

        if (result == 3) {
            goto_menu = ED_MENU_UNK_4;
        }

        if (result == 4 && event_no > 0) {
            RunEvent(event_no, NowCamera);
        }

        GameMode = ED_MODE_WALK;
        NowCamera = &MainCamera;
    } else {
        for (i = 0; i < 10; i++) {
            EdVillager[i].Step();
            EdVillager[i].ClothStep(0);
        }
    }
}

/**
 * Steps a running editor event, and follows it into whatever it asks for: a
 * building, a dungeon, the menu, or the camera it wants to look through.
 */
void EventMode() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR ref;

    int result = EdEventMode(&EventCamera, 0);

    if (result == ED_EVENT_RETURN_NONE) {
        return;
    }

    switch (result) {
        case ED_EVENT_RETURN_INTERIOR:
            GotoInterior(EdEventInfo.interior_name, EdEventInfo.interior_entrance, 0, NULL, EdEventInfo.interior_start_event);
            break;
        case ED_EVENT_RETURN_DUNGEON:
            GotoDungeon();
            break;
        case ED_EVENT_RETURN_CONTINUE:
            break;
        case ED_EVENT_RETURN_MAP_JUMP:
            EdExitLoop();
            break;
        case ED_EVENT_RETURN_EXIT:
            EdExitLoop();
            break;
        case ED_EVENT_RETURN_TALK:
            GameMode = ED_MODE_TALK;
            break;
        case ED_EVENT_RETURN_FISHING:
            GameMode = ED_MODE_FISHING;
            break;
        default:
            GameMode = ED_MODE_WALK;

            if (EdEventInfo.reset_camera_angle > 0) {
                Chara->GetPosition(pos);
                Chara->GetRotation(ref);
                MainCamera.FollowOn();
                float angle = ref[1] + EdEventInfo.reset_camera_yaw;
                angle = AngleLimit(angle);
                MainCamera.SetAngleSoon(angle);
                MainCamera.SetFollow(pos[0], pos[1] + Chara->body_height - 3.0f, pos[2]);
                EdInitCameraParam(&MainCamera);
                MainCamera.Step(-1);
            }

            if (EdEventInfo.reset_camera_angle < 0) {
                EventCamera.GetPos(pos);
                EventCamera.GetRef(ref);
                MainCamera.FollowOff();
                MainCamera.SetPos(pos);
                MainCamera.SetRef(ref);
                MainCamera.Step(-1);
                MainCamera.Step(1);
                MainCamera.FollowOn();
                MainCamera.SetHeight(pos[1] - ref[1]);
                MainCamera.SetFollow(ref[0], ref[1], ref[2]);
                MainCamera.SetAngleSoon(atan2(pos[0] - ref[0], pos[2] - ref[2]));
                MainCamera.Step(-1);
                MainCamera.Step(1);
            }

            if (EdEventInfo.fadeout_event_no >= 0) {
                FadeOutToEvent(EdEventInfo.fadeout_event_no, 0);
            }

            CEditGround *ground = pEditGround;
            ground->clip_plane[3] = -1.0f;
            break;
    }
}

/**
 * Requests a transition from the editor into a dungeon.
 */
int GotoDungeon() {
    return goto_dungeon = 1;
}

/**
 * Clears the parts, elements and plots of the robot's workshop map, so that
 * the story can build it again.
 */
void EdDeleteE05RoboParts() {
    int count;

    for (int i = 0; i < 14; i++) {
        SV_EDIT_PARTS_INFO *plot = SaveData->GetEditPartsInfo(4, i);

        if (plot != NULL) {
            plot->obtained = 0;
            plot->placed = false;
            plot->stock = 0;
        }
    }

    s16 *elements = SaveData->GetElemData(4);

    if (elements != NULL) {
        for (int i = 0; i < 128; i++) {
            *elements = -1;
            elements++;
        }
    }

    SV_GRD_PART *parts = SaveData->GetParts(4, &count);

    for (int i = 0; i < count; i++, parts++) {
        parts->part_id = -1;
    }
}

/**
 * Loads the interior a door leads to and places the player and camera in it.
 */
int GotoInterior(char *name, int entrance, int direction, ED_EVENT_PARAM *param, int start_event) {
    char          *suffix[5] = {"m", "e", "n", "m", ""};
    char           path[0x40];
    ED_EVENT_PARAM entry;
    sceVu0FVECTOR  position;
    sceVu0FVECTOR  rotation;

    int period = EdGetTime(NowTime);
    strcpy(EdInteriorName, name);
    sprintf(path, "%sin/%s/%s%s", EditDataDir, name, name, suffix[period]);
    sprintf(interior_map_name, "%s", path);
    EdInteriorPartsNo = entrance;
    EdNPCBuffer.used = 0;

    if (param == NULL) {
        direction = 0;
    }

    EdInteriorJumpID = 0;

    if (param != NULL) {
        EdInteriorJumpID = param->point->map_no;
    }

    door_open = direction;
    door_open_cnt = 160;
    GameMode = ED_MODE_DOOR_OPEN;
    EdInteriorDoorSound = -1;

    if (direction != 0) {
        fix_pos_enble = 1;
        EdInteriorDoorSound = param->point->linked_value;
        int forward = direction > 0;
        Chara->SetMotion(EdGetDoorMotion(param->point->linked_value, forward), 6);
        sceVu0CopyVector(fix_chara_pos, param->position);
        sceVu0CopyVector(fix_chara_rot, param->rotation);
        Chara->SetPosition(fix_chara_pos);
        Chara->SetRotation(fix_chara_rot[0], fix_chara_rot[1], fix_chara_rot[2]);
        Chara->ClothStep(-1);
        sceVu0CopyVector(fix_camera_pos, param->camera_pos);
        MainCamera.SetPos(fix_camera_pos);
        MainCamera.SetRef(fix_chara_pos[0], 14.0f + fix_chara_pos[1], fix_chara_pos[2]);
        MainCamera.FollowOff();
        MainCamera.Step(10);
        MainCamera.Step(-1);
    } else {
        fix_pos_enble = 0;
        sound_off_cnt = 160;

        if (EdSearchEvent(&entry, name, 0, NowTime) != 0) {
            sceVu0CopyVector(fix_chara_pos, entry.position);
            sceVu0CopyVector(fix_chara_rot, entry.rotation);
            Chara->SetPosition(fix_chara_pos);
            Chara->SetRotation(fix_chara_rot[0], fix_chara_rot[1], fix_chara_rot[2]);
            sceVu0CopyVector(fix_camera_pos, entry.camera_pos);
            MainCamera.SetPos(fix_camera_pos);
            MainCamera.SetRef(fix_chara_pos[0], 14.0f + fix_chara_pos[1], fix_chara_pos[2]);
        } else {
            Chara->GetPosition(position);
            Chara->GetRotation(rotation);
            sceVu0CopyVector(fix_chara_pos, position);
            sceVu0CopyVector(fix_chara_rot, rotation);
            MainCamera.GetPos(position);
            MainCamera.GetRef(rotation);
            sceVu0CopyVector(fix_camera_pos, position);
            MainCamera.SetRef(fix_chara_pos[0], 14.0f + fix_chara_pos[1], fix_chara_pos[2]);
        }
    }

    EdFadeOut(100, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < 10; i++) {
        EdVillager[i].near_camera = false;
    }

    EdInteriorStartEvent = start_event;

    if (start_event < 0) {
        EdInteriorStartEvent = 128;
    }

    return 1;
}

/**
 * Applies right-stick and digital-pad input to an editor follow camera.
 */
void MoveCamera(CCameraFollow *camera);

/**
 * Selects collision polygons around the centre of an axis-aligned box.
 */
int GetCollision(CCPoly *poly, CBoxVu0 *box) {
    sceVu0FVECTOR centre;
    sceVu0AddVector(centre, box->max, box->min);
    sceVu0ScaleVector(centre, centre, 0.5f);
    return pEditGround->PickUpPoly(poly, centre[0], centre[1], centre[2]);
}

/**
 * Steps the player: walking, the events they stand in, and the doors they open.
 */
void MoveChara() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR part_rot;
    sceVu0FVECTOR part_pos;
    sceVu0FMATRIX world;
    sceVu0FMATRIX inverse;
    sceVu0FVECTOR run_pos;
    sceVu0FVECTOR splash_pos;

    EdMoveCharaInfo.time = NowTime;
    EdMoveCharaInfo.camera = &MainCamera;
    ViewCamera.FollowOff();
    EdMoveCharaInfo.view_camera = &ViewCamera;
    EdMoveCharaInfo.key_lock = key_lock;
    EdMoveCharaInfo.chara = Chara;
    EdMoveCharaInfo.interior = false;
    EdMoveCharaInfo.ground = pEditGround;
    EDIT_MAP_INFO *info = EditMapInfo;
    EdMoveCharaInfo.points = info->work.events.points;
    EdMoveCharaInfo.point_count = info->event_count;
    EdMoveCharaInfo.fishing = (GameMode == ED_MODE_FISHING);
    EdMoveChara();

    if (EdMoveCharaInfo.system_event_no > 0) {
        RunSystemEvent(EdMoveCharaInfo.system_event_no, &MainCamera);
    } else if (EdMoveCharaInfo.event_no > 0) {
        RunEvent(EdMoveCharaInfo.event_no, &MainCamera);
    } else if (EdMoveCharaInfo.event_ready != 0) {
        ED_EVENT_PARAM *param = &EdMoveCharaInfo.param;

        if (param->kind == ED_EVENT_POINT_DOOR) {
            if (strcmp(param->point->destination, "dungeon") == 0) {
                RunEvent(130, &MainCamera);
            } else if (EdPadDown(0x40, 1) != 0 && EdCheckViewMode() == 0 && key_lock == 0) {
                ED_EVENT_POINT *point = param->point;

                if (point->minimum_progress > 0) {
                    RunEvent(EdInitGotoInterior(&EdEventInfo, param), NowCamera);
                } else {
                    GotoInterior(point->destination, param->parts_no, point->side, param, -1);
                }
            }
        }
    } else {
        if (loop_counter > 60) {
            Chara->GetPosition(pos);
            CMapParts *parts = pEditGround->GetParts(pos[0], pos[1], pos[2]);

            if (parts != NULL) {
                int             plot = parts->parts_no;
                EDITPARTS_INFO *plot_info = EditPartsInfo.GetPartsInfo(plot);

                if (plot_info != NULL && plot_info->kind == 2 && !(plot_info->completion_flags & 1) && EditPartsInfo.CheckComplete(plot) != 0) {
                    FadeOutToEvent(plot + 200, 0);
                }
            }
        }

        if (GamePad.Down(PAD_CROSS) != 0) {
            Chara->GetPosition(pos);
            CMapParts *parts = pEditGround->GetParts(pos[0], pos[1], pos[2]);

            if (parts != NULL) {
                parts->GetPosition(part_pos);
                parts->GetRotation(part_rot);
                sceVu0UnitMatrix(world);
                sceVu0RotMatrixY(world, world, part_rot[1]);
                sceVu0CopyVector(world[3], part_pos);
                world[3][3] = 1.0f;
                sceVu0InversMatrix(inverse, world);
                pos[3] = 1.0f;
                sceVu0ApplyMatrix(pos, inverse, pos);
                Chara->GetRotation(pos);
                pos[1] = pos[1] - part_rot[1];

                if (!(pos[1] <= PI_SHORT)) {
                    pos[1] -= TWO_PI_SHORT;
                }

                if (pos[1] < -PI_SHORT) {
                    pos[1] += TWO_PI_SHORT;
                }
            } else {
                Chara->GetRotation(pos);
            }
        }
    }

    if (CheckMotionTime(EdMoveCharaInfo.motion_time_before, EdMoveCharaInfo.motion_time_after, 74.0f) != 0) {
        Chara->GetPosition(run_pos);
        RunEffect.Set(run_pos);
    }

    if (CheckMotionTime(EdMoveCharaInfo.motion_time_before, EdMoveCharaInfo.motion_time_after, 84.0f) != 0) {
        Chara->GetPosition(splash_pos);
        RunEffect.Set(splash_pos);
    }

    if (GameMode == ED_MODE_WALK) {
        if (MapNo < 10) {
            EdMoveVillager(EdVillagerInfo);
        } else {
            EdMoveVillagerSubMap(EdVillagerInfo);
        }
    }
}

/**
 * Drops the player and every drawn villager onto the ground under them, and
 * takes the surface's foot sound and lighting from the polygon they stand on.
 */
void VillagerCollision() {
    sceVu0FVECTOR pos;
    CBoxVu0       box;
    sceVu0FVECTOR foot_pos;
    CCPoly        poly;

    for (int i = -1; i < 10; i++) {
        int           snap_to_ground;
        CCharacter   *chara;
        CNPCharacter *villager = NULL;
        int           foot_sound;

        if (i >= 0) {
            villager = &EdVillager[i];
            snap_to_ground = EdEventInfo.npc_collision[i];

            if (villager->CheckDraw() == 0) {
                continue;
            }

            villager->GetPosition(pos);
            foot_sound = EdEventInfo.npc_foot_sound[i];
        } else {
            snap_to_ground = EdEventInfo.player_collision;
            Chara->GetPosition(pos);
            foot_sound = EdEventInfo.player_foot_sound;
        }

        WorkBuffer__2->used = 0;
        CCPoly *polys = (CCPoly *) WorkBuffer__2->Alloc(2000);
        box.max[0] = 1.0f + pos[0];
        box.min[0] = pos[0] - 1.0f;
        box.max[2] = 1.0f + pos[2];
        box.min[2] = pos[2] - 1.0f;
        float height = pos[1];
        float top = 1000.0f + height;
        box.max[1] = 10.0f + top;
        box.min[1] = 10.0f + (height - 1000.0f);

        if (i >= 0) {
            chara = villager;
        } else {
            chara = Chara;
        }

        if (foot_sound == 1) {
            chara->FootSoundEnable(0);
        }

        int poly_count = GetCollision(polys, &box);

        if (poly_count > 200) {
            printf("cpoly over!!!! %d\n", poly_count);
        }

        if (poly_count > 0) {
            pos[1] += 20.0f;

            if (GetFootPoly(pos, 1000.0f, &poly, foot_pos, polys, poly_count, 0) != 0) {
                sceVu0CopyVector(pos, foot_pos);
                chara->FootSoundEnable(foot_sound);

                if (foot_sound == 1) {
                    chara->SetFootSoundID(poly.attr.foot_sound);
                }

                sceVu0CopyVector(chara->ground_ambient[0], EditMapInfo->character_ambient[0]);
                sceVu0CopyVector(chara->ground_ambient[1], EditMapInfo->character_ambient[1]);
                chara->ground_ambient_no = -1;
                chara->fade_out = 0;

                switch (poly.attr.area_kind) {
                    case AREA_KIND_FADE_OUT:
                        chara->fade_out = 1;
                        break;
                    case AREA_KIND_AMBIENT_0:
                    case AREA_KIND_AMBIENT_1:
                        chara->ground_ambient_no = poly.attr.area_kind - 3;
                        break;
                }
            } else {
                pos[1] = 0.0f;
            }
        } else {
            pos[1] = 0.0f;
        }

        if (snap_to_ground != 0) {
            if (i >= 0) {
                villager->SetPosition(pos);
            } else {
                Chara->SetPosition(pos);
            }
        }
    }
}

/**
 * Says whether the ground under the editor's cursor can be walked on, and
 * gives back the place on it the player would stand.
 */
int CheckEditToWalk(float *position) {
    sceVu0FVECTOR pos;
    CBoxVu0       box;
    sceVu0FVECTOR hit;
    sceVu0FVECTOR from;
    sceVu0FVECTOR to;

    sceVu0CopyVector(pos, ECursorFrame->position);
    pos[3] = 1.0f;
    WorkBuffer__2->used = 0;
    CCPoly *polys = (CCPoly *) WorkBuffer__2->Alloc(1024);
    box.max[0] = 100.0f + pos[0];
    box.min[0] = pos[0] - 100.0f;
    float z = pos[2];
    box.max[2] = 100.0f + z;
    box.min[2] = z - 100.0f;
    box.max[1] = 20000.0f;
    box.min[1] = -20000.0f;
    pos[1] += 500.0f;
    int poly_count = pEditGround->PickUpPoly(polys, box, 0);
    int walkable = 1;

    for (int i = 0; i < 4; i++) {
        sceVu0CopyVector(from, pos);

        if (i == 1) {
            from[0] += 0.10000000149011612f;
        }

        if (i == 1) {
            from[2] += 0.597599983215332f;
        }

        if (i == 2) {
            from[0] -= 0.5809999704360962f;
        }

        if (i == 2) {
            from[2] -= 0.29760000109672546f;
        }

        if (i == 3) {
            from[0] += 0.35100001096725464f;
        }

        if (i == 3) {
            from[2] -= 2.6775999069213867f;
        }

        sceVu0CopyVector(to, from);
        to[1] -= 1000.0f;

        if (CheckHit(polys, poly_count, from, to, hit, 1, 0) >= 0) {
            if (EdDebugMoveFlag <= 0) {
                float alt = pEditGround->GetAlt(from[0], from[1], from[2]);
                int   area = pEditGround->GetAreaCode(from[0], from[1], from[2]);
                float drop;
                drop = hit[1] - alt;
                drop = drop < 0.0f ? -drop : drop;

                if (drop < 0.10000000149011612f && area >= 0) {
                    pos[1] = 1.0f + hit[1];
                } else {
                    walkable = 0;
                }

                CMapParts *parts = pEditGround->GetParts(pos[0], pos[1], pos[2]);

                if (parts != NULL && parts->subtype == MAP_PARTS_SUBTYPE_ROAD) {
                    pos[1] = 1.0f + hit[1];
                    walkable = 1;
                }
            } else {
                pos[1] = 1.0f + hit[1];
                walkable = 1;
            }
        } else {
            walkable = 0;
        }
    }

    sceVu0CopyVector(position, pos);
    return walkable;
}

/**
 * Moves the placement cursor across the ground with the pad.
 *
 * @mangled MoveEditCursor__Fv
 * @address 0x1800C0
 * @size 0xB40
 */
void MoveEditCursor() {
    float pos[4];
    float camera_distance[3] = {1000.0f, 2000.0f, 4000.0f};
    float camera_height[3] = {1000.0f, 1400.0f, 2000.0f};

    static int camera_dist_mode = 0;

    float cursor_scale[3] = {1.0f, 1.2f, 1.5f};

    if (MapNo == TOWN_YELLOW_DROPS) {
        cursor_scale[0] *= 0.6f;
    }

    float step_z;
    float camera_angle;
    float step_x;
    float forward;
    float right;

    camera_angle = EditCamera.GetAngle();
    right = EdGetLXf(2);
    forward = EdGetLYf(2);

    sceVu0CopyVector(pos, ECursorFrame->position);

    int        area_code = pEditGround->GetAreaCode(pos[0], pos[1], pos[2]);
    CEditArea *area = NULL;

    if (EdPadDown(0xF000, 2) != 0) {
        if (area_code >= 0) {
            area = pEditGround->areas[area_code];
        }

        if (area != NULL) {
            float unit = area->GetUnitSize();

            if (EdPadDown(0x1000, 2) != 0) {
                forward = -unit;
            }

            if (EdPadDown(0x4000, 2) != 0) {
                forward = unit;
            }

            if (EdPadDown(0x2000, 2) != 0) {
                right = unit;
            }

            if (EdPadDown(0x8000, 2) != 0) {
                right = -unit;
            }
        }
    }

    step_x = right * cosf(camera_angle) + forward * sinf(camera_angle);
    step_z = forward * cosf(camera_angle) - right * sinf(camera_angle);

    sceVu0CopyVector(pos, ECursorFrame->position);

    if (area != NULL) {
        if (pEditGround->GetAreaCode(pos[0] + step_x, pos[1], pos[2] + step_z) < 0) {
            step_x = 0.0f;
            step_z = 0.0f;
        }
    } else {
        float scale = cursor_scale[camera_dist_mode];

        step_x = 10.0f * step_x * scale;
        step_z = 10.0f * step_z * scale;
    }

    float move[4] = {step_x, 0.0f, step_z, 0.0f};

    LimitEditCursorPos(pos, move);

    if (move[0] != 0.0f || move[2] != 0.0f) {
        DrawPartsNameCount = 0;
    }

    if (area != NULL) {
        CVector3_f_ grid;

        area->GetGrid(&grid, pos[0], pos[1], pos[2]);

        float half = area->GetUnitSize() / 2.0f;

        pos[0] = grid.x + half;
        pos[2] = grid.z + half;
        ECursorFrame->SetPosition(pos);
    }

    MoveCamera(&EditCamera);
    EditCamera.SetDistance(camera_distance[camera_dist_mode]);
    EditCamera.SetHeight(camera_height[camera_dist_mode]);

    if (MapNo == TOWN_YELLOW_DROPS) {
        EditCamera.SetDistance(0.2f * camera_distance[camera_dist_mode]);
        EditCamera.SetHeight(0.6f * camera_height[camera_dist_mode]);
        cursor_scale[0] *= 0.6f;
    }

    float scale = cursor_scale[camera_dist_mode];

    ECursorFrame->SetScale(scale, scale, scale);

    if (!(camera_dist_mode < 2)) {
        camera_dist_mode = 0;
    }

    ECursorFrame->SetPosition(pos[0], pos[1], pos[2]);
    float parts_pos[4];

    sceVu0CopyVector(parts_pos, ECursorFrame->position);

    if (PI_SHORT * (float) NowSelectAngle / 2.0f == NowCursorRotY) {
        if (EdPadDown(2, 2) != 0) {
            OldSelectAngle = NowSelectAngle;
            NowSelectAngle = NowSelectAngle - 1;
        } else if (EdPadDown(1, 2) != 0) {
            OldSelectAngle = NowSelectAngle;
            NowSelectAngle = NowSelectAngle + 1;
        }
    }

    if (NowSelectAngle > 2) {
        NowSelectAngle = -1;
    }

    if (NowSelectAngle < -1) {
        NowSelectAngle = 2;
    }

    NowCursorRotY = AngleInterpolate(NowCursorRotY, PI_SHORT * (float) NowSelectAngle / 2.0f, 0.1f, INTERPOLATE_STEP);

    int menu_mode = EditMenuStatus.mode;

    OldFocusParts = NowFocusParts;
    NowFocusParts = pEditGround->GetParts(pos[0], pos[1], pos[2]);

    if ((u_int) EditMenuStatus.mode < 2 && EdPadDown(0x40, 2) != 0) {
        int sound_id = 13;

        if (MapNo == TOWN_YELLOW_DROPS) {
            sound_id = 333;
        }

        int plot = pEditGround->SetMapParts(NowSelectParts, parts_pos[0], parts_pos[1], parts_pos[2], NowSelectAngle);

        if (plot >= 0) {
            pEditGround->SetBuildEffect(plot);
            SndSePlay(sound_id, -1, 0);

            EDITPARTS_INFO *info = EditPartsInfo.GetPartsInfo(NowSelectParts);

            if (EditMenuStatus.mode == EDIT_MENU_MODE_REPLACE || (info != NULL && info->stock > 0 && info->placed == info->stock)) {
                menu_mode = EDIT_MENU_MODE_MOVE;
                NowSelectParts = -1;
                NowSelectAngle = 0;
                OldSelectAngle = 0;
            }
        }
    }

    if (EditMenuStatus.mode == EDIT_MENU_MODE_MOVE) {
        int deleted_parts;
        int deleted_angle;

        if (EdPadDown(0x80, 2) != 0 && pEditGround->DeleteMapParts(&deleted_parts, &deleted_angle, parts_pos[0], parts_pos[1], parts_pos[2]) >= 0) {
            EdSePlay((ED_SOUND_ID) 0xE, -1);
        }

        pEditGround->SetFocusParts(parts_pos[0], parts_pos[1], parts_pos[2]);

        if (EdPadDown(0x40, 2) != 0) {
            int taken_parts;
            int taken_angle;

            if (NowFocusParts != NULL) {
                int width = NowFocusParts->GetWidth();

                if (width * NowFocusParts->GetHeight() > 0) {
                    NowFocusParts->GetPosition(NowCursorPos);
                    NowFocusParts->GetPosition(NowPartsCursorPos);
                    NowPartsCursorPos[1] = pEditGround->GetAlt(pos[0], pos[1], pos[2]);

                    float alt = pos[1];

                    NowFocusParts->GetPosition(pos);
                    pos[1] = alt;
                    NowCursorPos[1] = alt;
                }
            }

            if (pEditGround->DeleteMapParts(&taken_parts, &taken_angle, parts_pos[0], parts_pos[1], parts_pos[2]) >= 0) {
                menu_mode = EDIT_MENU_MODE_REPLACE;
                NowSelectParts = taken_parts;
                NowSelectAngle = taken_angle;
                NowCursorRotY = PI_SHORT * (float) taken_angle / 2.0f;
                OldSelectAngle = taken_angle;
                EditMenuStatus.parts = taken_parts;
            }
        }
    }

    EditMenuStatus.mode = menu_mode;

    if (GamePad.Down2(PAD_CROSS) != 0) {
        pEditGround->Clear();
    }

    if (GamePad.Down2(PAD_CIRCLE) != 0) {
        pEditGround->Save(NULL);
    }

    if (GamePad.Down2(PAD_TRIANGLE) != 0) {
        pEditGround->Load(NULL);
    }

    if (pEditGround->GetAreaCode(pos[0], pos[1], pos[2]) >= 0) {
        if (pos[1] < pEditGround->GetAlt(pos[0], pos[1], pos[2])) {
            pos[1] = pEditGround->GetAlt(pos[0], pos[1], pos[2]);
        }
    }

    ECursorFrame->SetPosition(pos[0], pos[1], pos[2]);
    sceVu0CopyVector(NextCursorPos, pos);

    if (NowFocusParts != NULL) {
        if (NowFocusParts->GetWidth() > 1) {
            NowFocusParts->GetHeight();
        }

        EditNameMes.mes_no[0] = GetAtraMsgNo(MapNo, NowFocusParts->parts_no);

        if (OldFocusParts != NowFocusParts) {
            EditNameMes.MakeMesWin(-1);
        }

        if (DrawPartsNameCount > 0) {
            EditNameMes.MakeMesWin(-1);
            PartsNameNum = -1;
        } else {
            EditNameMes.MakeMesWin(1);
            PartsNameNum = EditNameMes.text_columns;
        }
    } else {
        EditNameMes.MakeMesWin(-1);
    }

    DrawPartsNameCount--;

    if (DrawPartsNameCount < 0) {
        DrawPartsNameCount = 0;
    }

    EditCamera.FollowOn();
    EditCamera.SetFollow(pos[0], pos[1], pos[2]);

    if (EditMenuStatus.mode == EDIT_MENU_MODE_CLOSED) {
        float walk[4];

        EdEditMainHelpMes(CheckEditToWalk(walk));
    }

    if ((u_int) EditMenuStatus.mode < 2) {
        EdEditBuildHelpMes(NowSelectParts);
    }

    if (EditMenuStatus.mode == EDIT_MENU_MODE_MOVE) {
        EdEditMoveHelpMes();
    }
}

u_int *scn_data;
u_int *parts_read_buffer;

/**
 * Enters the editor map's textures once they have been read.
 *
 * @mangled LoadTexture__Fv
 * @address 0x180C00
 * @size 0x878
 */
int LoadTexture();

/**
 * Loads the player's model and motions into the arena the caller names, or
 * into the character arena when it names none.
 */
void EdLoadMainChara(char *pack_path, char *info_name, CDataAlloc2<1> *arena) {
    int i;

    if (arena == NULL) {
        arena = &CharaBuffer;
        CharaBuffer.used = 0;
    }

    LoadFile(pack_path, read_buffer, NULL);
    MainChara.Initialize();

    for (i = 0; i < 128; i++) {
        CharaTexAnimeData[i].Initialize();
    }

    MainChara.InitializeTexAnime(CharaTexAnimeData, 128);
    MainChara.LoadPackData2(read_buffer, info_name, arena, 8, arena, 0);
    CFrameAttr attr;
    attr.clip_enable = false;
    attr.fog_enable = true;
    MainChara.frame->SetAttr(attr, 1, 4);
    MainChara.SetPosition(0.0f, 0.0f, 0.0f);
    Chara = &MainChara;
}

/**
 * Reads the editor map's ground, areas and grid.
 *
 * @mangled LoadGroundData__Fv
 * @address 0x1815E0
 * @size 0xDA8
 */
void LoadGroundData() {
    parts_read_buffer = read_buffer;

    char pack_path[64];

    GetEditDataDir(pack_path);
    strcat(pack_path, "mds.pak");
    LoadPackFile(pack_path, parts_read_buffer, NULL);
    wait_now_loading_vsync();

    u_int             *data;
    OBJ_ANIME_SEQ     *anime = EditMapInfo->work.obj_anime;
    EDIT_EFFECT_INFO  *effects = EditMapInfo->work.effects.first;
    EDIT_OBJECT_TIMER *timers = EditMapInfo->work.object_timers.timers;
    ED_EVENT_POINT    *points = EditMapInfo->work.events.points;

    pEditGround->Initialize();
    pEditGround->map_no = NowEditMap;

    char *sun_names[4] = {"sun1", "sun2", "sun3", "moon"};

    for (int i = 0; i < 4; i++) {
        SkyFrame[i] = NULL;

        CFrameAttr layer_attr;

        layer_attr.depth_write = false;

        if (EditMapInfo->sky_layers[i].name[0] != '\0') {
            data = (u_int *) EdLoadFile(EditMapInfo->sky_layers[i].name);

            SkyFrame[i] = LoadMDSFile(data, &DataBuffer__2, 0, NULL, NULL);
            SkyFrame[i]->SetAttr(layer_attr, 1, 0x1000);

            for (int j = 0; j < 8; j++) {
                short no = EditMapInfo->sky_layers[i].obj_anime[j];

                if (no > 0) {
                    InitObjAnime(SkyFrame[i], &anime[no]);
                }
            }
        }

        SunFrame[i] = NULL;

        if (EditMapInfo->sun_layers[i].name[0] != '\0') {
            data = (u_int *) EdLoadFile(EditMapInfo->sun_layers[i].name);

            SunFrame[i] = LoadMDSFile(data, &DataBuffer__2, 0, NULL, NULL);
            SunFrame[i]->SetAttr(layer_attr, 1, 0x1000);
        }

        if (SunFrame[i] != NULL) {
            sceVu0FMATRIX world;

            sceVu0UnitMatrix(world);
            SunFrame[i]->SetTransMatrix(world);
            SunFrame[i]->SetRotation(0.5f, 0.0f, 0.0f);
            SunFrame[i] = SunFrame[i]->SearchFrame(sun_names[i]);
        }
    }

    SkyBackFrame = NULL;

    if (EditMapInfo->sky_layers[4].name[0] != '\0') {
        data = (u_int *) EdLoadFile(EditMapInfo->sky_layers[4].name);

        CFrameAttr back_attr;

        back_attr.depth_write = false;
        back_attr.use_color = true;
        SkyBackFrame = LoadMDSFile(data, &DataBuffer__2, 6, NULL, NULL);
        SkyBackFrame->SetAttr(back_attr, 1, 0x1200);
    }

    for (int i = 0; i < 4; i++) {
        EDIT_AREA_INFO *info = &EditMapInfo->parts_work.edit_areas[i];

        pEditGround->areas[i] = NULL;

        if (info->name[0] != '\0') {
            data = (u_int *) EdLoadFile(info->name);
            EditArea[i].SetMapInfo(NowEditMap, i);
            EditArea[i].SetSize(info->width, info->height, info->unit_size, info->unit_alt);
            EditArea[i].SetOffset(info->origin_x - 0.01f * (float) info->width, info->origin_y, info->origin_z - 0.01f * (float) info->height);
            EditArea[i].SetGridFrame(LoadMDSFile(data, &DataBuffer__2, 6, NULL, NULL));

            CFrameAttr grid_attr;

            grid_attr.fog_enable = true;
            EditArea[i].GetGridFrame()->SetAttr(grid_attr, 1, 0x40);
            pEditGround->areas[i] = &EditArea[i];
        }
    }

    if (MapNo < 6) {
        data = (u_int *) EdLoadFile("gedit/e01/mds/e01a03_0.mds");

        pEditGround->cursor.pieces[0] = LoadMDSFile(data, &DataBuffer__2, 0, NULL, NULL);

        data = (u_int *) EdLoadFile("gedit/e01/mds/e01a04_0.mds");

        pEditGround->cursor.pieces[1] = LoadMDSFile(data, &DataBuffer__2, 0, NULL, NULL);

        data = (u_int *) EdLoadFile("gedit/e01/mds/e01a05_0.mds");

        pEditGround->cursor.pieces[2] = LoadMDSFile(data, &DataBuffer__2, 0, NULL, NULL);
        pEditGround->cursor.area = 2;
    } else {
        pEditGround->cursor.pieces[0] = NULL;
        pEditGround->cursor.pieces[1] = NULL;
        pEditGround->cursor.pieces[2] = NULL;
    }

    wait_now_loading_vsync();

    for (int i = 0; i < 4; i++) {
        EDIT_MOTION_PARTS_INFO *info = &EditMapInfo->motion_parts[i];

        MotionParts[i].Initialize();

        if (info->name[0] != '\0') {
            data = (u_int *) EdLoadFile(info->name);
            MotionParts[i].InitializeTexAnime(TexAnimeData, 64);
            MotionParts[i].LoadPackData2(data, "info.cfg", &DataBuffer__2, i + 0x1B, &DataBuffer__2, 0);
            MotionParts[i].SetPosition(info->values[0], info->values[1], info->values[2]);
            MotionParts[i].SetRotation(info->values[3], info->values[4], info->values[5]);
        }
    }

    scn_data = (u_int *) (EdNPCBuffer.base + EdNPCBuffer.used * 16);

    char scene_path[64];
    int  scene_size;

    GetEditDataDir(scene_path);
    strcat(scene_path, EditMapInfo->scene_name);

    if (LoadFile2(scene_path, (void *) scn_data, &scene_size, 0) == 0) {
        scn_data = NULL;
        scene_size = 0;
    }

    if (scn_data != NULL) {
        parts_read_buffer = (u_int *) ((char *) scn_data + (((scene_size >> 6) + 1) << 6));
    } else {
        parts_read_buffer = read_buffer;
    }

    while (check_now_loading() == 0) {
    }

    EditPartsInfo.Initialize(NowEditMap);
    LoadObjectParts();
    pEditGround->plot_parts = ObjParts;
    pEditGround->river_parts = RiverParts;
    pEditGround->road_parts = RoadParts;
    pEditGround->parts_info = &EditPartsInfo;
    ObjParts[12].draw_distance = 490.0f;

    /**
     * A part already built while the map loads, so later copies can share it.
     */
    struct LOADED_PARTS {
        char       name[0x20]; /**< Resource name the part was built from. */
        CMapParts *parts;      /**< Part built from that name. */
    } built[64];

    int i;

    for (i = 0; i < 64; i++) {
        built[i].name[0] = '\0';
    }

    int built_count = 0;

    for (i = 0;; i++) {
        MAP_PARTS_INFO *info = &EditMapInfo->map_objects[i];

        if (info->name[0][0] == '\0' && info->name[1][0] == '\0' && info->name[2][0] == '\0' && info->name[3][0] == '\0') {
            break;
        }

        CMapParts *parts = &pEditGround->fixed_parts[i];

        parts->Initialize();

        u_int *pts = SearchPTS(scn_data, info->name[0]);

        if (pts != NULL) {
            CMapParts *shared = NULL;

            for (int j = 0; j < built_count; j++) {
                if (strcmp(info->name[0], built[j].name) == 0) {
                    shared = built[j].parts;
                    break;
                }
            }

            LoadPTS(parts, pts, info, anime, effects, timers, points, shared);

            char *c = info->name[0];
            char *base = c;
            char  letter;

            for (; (letter = *c) != '\0'; c++) {
                if (letter == '/') {
                    base = c + 1;
                }
            }

            if (strcmp(base, "e02c13") == 0) {
                CFrameAttr door_attr;

                door_attr.fog_enable = true;

                CFrame *door = parts->ripple_frame;

                if (door != NULL) {
                    door = door->SearchFrame("cube1");
                }

                if (door != NULL) {
                    door->SetAttr(door_attr, 1, 0x40);
                }
            }

            sceVu0FVECTOR position = {info->position[0], info->position[1], info->position[2],
                                      1.0f};

            parts->SetPosition(position);
            parts->SetRotation(info->rotation[0], info->rotation[1], info->rotation[2]);

            if (shared == NULL) {
                strcpy(built[built_count].name, info->name[0]);
                built[built_count].parts = parts;
                built_count++;
            }
        } else {
            LoadPTS(parts, info, anime, effects, timers, points);

            sceVu0FVECTOR position = {info->position[0], info->position[1], info->position[2],
                                      1.0f};

            parts->SetPosition(position);
            parts->SetRotation(info->rotation[0], info->rotation[1], info->rotation[2]);
        }

        int kind = info->kind;

        parts->handle = i;
        parts->category_no = kind;
    }

    for (i = 0; i < 4; i++) {
        EDIT_WATER_INFO *info = &EditMapInfo->water_surfaces[i];

        if (info->grid_rows <= 0) {
            break;
        }

        CGroundWater *surface = &pEditGround->water_surfaces[i];
        CWater       *water = &pEditGround->water_surfaces[i].water;
        sceVu0FVECTOR near_left = {info->corner_a[0], info->corner_a[1], info->corner_a[2],
                                   1.0f};
        sceVu0FVECTOR near_right = {info->corner_b[0], info->corner_a[1], info->corner_a[2],
                                    1.0f};
        sceVu0FVECTOR far_left = {info->corner_a[0], info->corner_a[1], info->corner_b[2],
                                  1.0f};
        sceVu0FVECTOR far_right = {info->corner_b[0], info->corner_a[1], info->corner_b[2],
                                   1.0f};

        surface->draw = true;
        strcpy(surface->name, info->name);
        surface->parts_no = info->parts_no;
        sceVu0CopyVector(surface->offset, info->corner_c);

        for (int j = 0; j < 3; j++) {
            surface->follow[j] = info->follow[j];
        }

        for (int j = 0; j < 4; j++) {
            sceVu0CopyVector(&surface->ripples[j].row, &info->wave[j].row);
        }

        water->SetVertex(near_left, near_right, far_left, far_right);
        water->frame.SetPosition(info->corner_c);
        water->SetSize(info->grid_rows, info->grid_columns, &DataBuffer__2);
        water->SetParam(info->ripple_params[0], info->ripple_params[1], info->ripple_params[2], info->ripple_params[3]);
        water->SetColor(info->red, info->green, info->blue, 0x80);
    }
}

/**
 * Builds every map part the script placed -- the objects, then the roads and
 * rivers -- and hands a part that names a model already loaded the part that
 * loaded it, so the two share one set of frames.
 */
void LoadObjectParts() {
    char  name_buffer[7][0x40];
    char *names[7];

    /**
     * A part already built while the map loads, so later copies can share it.
     */
    struct LOADED_PARTS {
        char       name[0x20]; /**< Resource name the part was built from. */
        CMapParts *parts;      /**< Part built from that name. */
    } loaded[64];

    OBJ_ANIME_SEQ     *anime = EditMapInfo->work.obj_anime;
    EDIT_EFFECT_INFO  *effects = EditMapInfo->work.effects.second;
    EDIT_OBJECT_TIMER *timers = EditMapInfo->work.object_timers.timers;
    ED_EVENT_POINT    *points = EditMapInfo->work.events.points;

    for (int i = 0; i < 7; i++) {
        names[i] = name_buffer[i];
        *names[i] = '\0';
    }

    int i;
    int river = 0;
    int road = 0;

    for (i = 0; i < 64; i++) {
        loaded[i].name[0] = '\0';
    }

    int loaded_count = 0;

    for (i = 0; i < 46; i++) {
        CMapParts      *shared = NULL;
        int             has_archive = 0;
        CMapParts      *parts;
        MAP_PARTS_INFO *info = &EditMapInfo->parts_work.general.parts[i];
        EDITPARTS_INFO *plot = NULL;
        int             index = i;
        int             slot;

        if (i >= 24) {
            info = &EditMapInfo->parts_work.roads.parts[road];

            if (i >= 30) {
                info = &EditMapInfo->parts_work.rivers.parts[river];
            }

            switch (info->subtype) {
                case MAP_PARTS_SUBTYPE_RIVER:
                case MAP_PARTS_SUBTYPE_BRIDGE:
                    parts = &RiverParts[river];
                    slot = river;
                    river++;
                    break;
                case MAP_PARTS_SUBTYPE_ROAD:
                    parts = &RoadParts[road];
                    slot = road;
                    road++;
                    break;
                default:
                    continue;
            }
        } else {
            parts = &ObjParts[i];
            plot = EditPartsInfo.GetPartsInfo(i);
            slot = i;
        }

        for (int j = 0; j < loaded_count; j++) {
            if (strcmp(info->name[0], loaded[j].name) == 0) {
                shared = loaded[j].parts;
                break;
            }
        }

        u_int *archive = SearchPTS(scn_data, info->name[0]);

        if (archive != NULL) {
            has_archive = 1;
        }

        parts->Initialize();

        if (plot == NULL) {
            for (int j = 0; j < 24; j++) {
                if (info->subtype == ObjParts[j].subtype) {
                    index = j;
                    break;
                }
            }
        }

        int kind = info->kind;
        parts->handle = slot;
        parts->category_no = kind;
        parts->parts_no = index;
        parts->subtype = info->subtype;
        parts->lift = info->lift;
        parts->def_parts_no = info->parts_no;

        EPARTS_INFO_HEADER *header;

        if (has_archive != 0) {
            header = LoadPTS(parts, archive, info, anime, effects, timers, points, shared);
        } else {
            if (plot != NULL) {
                header = (EPARTS_INFO_HEADER *) (EPartsInfoBuff.base + EPartsInfoBuff.used * 16);
                EPartsInfoBuff.Alloc((EdInitToEPInfo((INIT_PARTSINFO *) &EditMapInfo->parts_work.parts_defs.defs[i], header) >> 4) + 1);
            }

            LoadPTS(parts, info, anime, effects, timers, points);
        }

        if (plot != NULL) {
            EditPartsInfo.Initialize(i, header);
        }

        parts->info = EditPartsInfo.GetPartsInfo(index);

        if (parts->frame[3] != NULL) {
            parts->preview_frame = (int) parts->frame[3];
        } else if (parts->frame[2] != NULL) {
            parts->preview_frame = (int) parts->frame[2];
        } else if (parts->frame[1] != NULL) {
            parts->preview_frame = (int) parts->frame[1];
        } else if (parts->frame[0] != NULL) {
            parts->preview_frame = (int) parts->frame[0];
        }

        if (shared == NULL) {
            strcpy(loaded[loaded_count].name, info->name[0]);
            loaded[loaded_count].parts = parts;
            loaded_count++;
        }
    }
}

/**
 * Puts a map part's camera collision frame where the part stands and returns it.
 */
static inline CFrame *GetCameraFrame(CMapParts *parts) {
    if (parts->camera_frame == NULL) {
        return NULL;
    }

    parts->camera_frame->SetPosition(parts->pos[0], parts->pos[1], parts->pos[2]);
    parts->camera_frame->SetRotation(parts->rotation.x, parts->rotation.y, parts->rotation.z);
    return parts->camera_frame;
}

/**
 * Builds one map part from its archive entry, sharing the models of a part already
 * built.
 *
 * @mangled LoadPTS__FP9CMapPartsPUiP14MAP_PARTS_INFOP13OBJ_ANIME_SEQP16EDIT_EFFECT_INFOP17EDIT_OBJECT_TIMERP14ED_EVENT_POINTP9CMapParts
 * @address 0x1828B0
 * @size 0x754
 */
EPARTS_INFO_HEADER *LoadPTS(CMapParts *parts, unsigned int *archive, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime, EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points, CMapParts *shared);

/**
 * Builds one map part that has no part-definition file: its models are loaded
 * straight from the names the script gave, and its bound comes from them.
 */
void LoadPTS(CMapParts *parts, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime, EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points) {
    char      *names[9];
    CFrameVu1 *frames[4];
    CBoxVu0    bound;
    CBoxVu0    part_bound;

    if (info == NULL || info->name[0][0] == '\0') {
        return;
    }

    for (int i = 0; i < 9; i++) {
        names[i] = info->name[i];
    }

    LoadMapObject(parts, names);

    for (int i = 0; i < 4; i++) {
        CFrameVu1 *frame = parts->frame[i];
        frames[i] = frame;
    }

    if (frames[0] != NULL) {
        frames[0]->GetBoundBox(&bound, 1);
        CFrame *collision = parts->GetCollisionFrame();

        if (collision != NULL) {
            collision->GetBoundBox(&part_bound, 1);
            VectorMaxMin(bound.max, bound.min, bound.max, bound.min, part_bound.max, part_bound.min);
        }

        CFrame *camera_frame = parts->camera_frame;

        if (camera_frame == NULL) {
            camera_frame = NULL;
        } else {
            camera_frame->SetPosition(parts->pos[0], parts->pos[1], parts->pos[2]);
            parts->camera_frame->SetRotation(parts->rotation.x, parts->rotation.y, parts->rotation.z);
            camera_frame = parts->camera_frame;
        }

        if (camera_frame != NULL) {
            camera_frame->GetBoundBox(&part_bound, 1);
            VectorMaxMin(bound.max, bound.min, bound.max, bound.min, part_bound.max, part_bound.min);
        }

        parts->bound = bound;
    }
}

/**
 * Provides the empty map-object loading hook used by this editor loop.
 */
void LoadMapObject(CMapParts *map_parts, char **script) {
}

/**
 * Draws one textured rectangle rotated about a caller-supplied pivot.
 */
void set2DSpriteRot(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle, unsigned char alpha);

C3DSprite::C3DSprite() {
    texel.x = texel.y = texel.width = texel.height = 0;
    Initialize();
}

/**
 * Gives one map part its own copy of every frame another one draws with.
 */
void CopyCMapParts(CMapParts *to, CMapParts *from, CDataAlloc2<1> *arena) {
    for (int i = 0; i < 4; i++) {
        to->SetFrame(CopyFrameVu1(from->frame[i], arena), i);
    }

    to->collision_frame = CopyFrame(from->GetCollisionFrame(), arena);
    to->shadow_frame = (CFrame *) CopyFrameVu1((CFrameVu1 *) from->shadow_frame, arena);
    to->shade_frame = (CFrame *) CopyFrameVu1((CFrameVu1 *) from->shade_frame, arena);
    CFrame *camera_frame = from->camera_frame;

    if (camera_frame == NULL) {
        camera_frame = NULL;
    } else {
        camera_frame->SetPosition(from->pos[0], from->pos[1], from->pos[2]);
        from->camera_frame->SetRotation(from->rotation.x, from->rotation.y, from->rotation.z);
        camera_frame = from->camera_frame;
    }

    to->camera_frame = CopyFrame(camera_frame, arena);
    to->ripple_frame = (CFrame *) CopyFrameVu1((CFrameVu1 *) from->ripple_frame, arena);
}

/**
 * Derives an object's world position and normalized yaw from its first frame.
 */
int GetPosRot(CMapObject *object, float *position, float *rotation) {
    if (object == NULL) {
        return 0;
    }

    CFrame *frame = object->frame[0];

    if (frame == NULL) {
        return 0;
    }

    sceVu0FVECTOR local_position;
    sceVu0CopyVector(local_position, object->pos);
    frame->SetPosition(local_position);
    position[3] = 1.0f;
    frame->GetWorldPosition(position, position);
    sceVu0FVECTOR frame_rotation;
    frame->GetRotation(frame_rotation);
    rotation[0] = 0.0f;
    rotation[1] = frame_rotation[1] + rotation[1];
    rotation[2] = 0.0f;

    if (!(rotation[1] <= PI_SHORT)) {
        rotation[1] -= TWO_PI_SHORT;
    }

    if (rotation[1] < -PI_SHORT) {
        rotation[1] += TWO_PI_SHORT;
    }

    return 1;
}

/**
 * Returns the first unused event-point slot after the reserved first entry.
 */
ED_EVENT_POINT *GetNewEventPoint(ED_EVENT_POINT *points, int count) {
    for (int i = 1; i < count; i++) {
        if (points[i].event_type == ED_EVENT_POINT_NONE) {
            return &points[i];
        }
    }

    return NULL;
}

/**
 * Says whether one event point is ready to fire at the given time of day.
 */
int CheckEventPoint(ED_EVENT_POINT *point, float time) {
    if (point == NULL) {
        return 0;
    }

    if (point->enabled == 0) {
        return 0;
    }

    if (point->event_type <= ED_EVENT_POINT_NONE) {
        return 0;
    }

    int flag = point->completion_flag;

    if (flag > 0 && EdGetMapFlag(flag) != 0) {
        return 0;
    }

    CFrame *frame = point->frame;

    if (frame != NULL && !(frame->attr.draw_on & 1)) {
        return 0;
    }

    if (EdCheckTime(time, point->start_time, point->end_time) == 0) {
        return 0;
    }

    if (point->event_type == ED_EVENT_POINT_ITEM_BOX) {
        int dungeon = MapNo;

        if (dungeon == 11) {
            dungeon = 1;
        }

        if (dungeon == 13) {
            dungeon = 1;
        }

        if (dungeon == 33) {
            dungeon = 1;
        }

        if (dungeon == 19) {
            dungeon = 2;
        }

        if (dungeon == 42) {
            dungeon = 3;
        }

        if (dungeon == 23) {
            dungeon = 4;
        }

        if (dungeon == 38) {
            dungeon = 5;
        }

        if (dungeon == 40) {
            dungeon = 5;
        }

        if (dungeon < 5) {
            CDngStatusData *status = SaveData->GetDngStatus();
            int             reached = status->floor_reached[dungeon] + 1;
            int             wanted = point->minimum_progress;

            if (wanted > 0 && reached < wanted) {
                return 0;
            }
        }
    }

    return 1;
}
