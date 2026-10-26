#include "stubs/edit_in.hpp"
#include "common.h"

#include <libvu0.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "dispctrl.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "edit_in.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editpartsinfo.hpp"
#include "effectgroup.hpp"
#include "effectmacro.hpp"
#include "fireomni.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "menu_misc.hpp"
#include "mglib.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "scriptinterpreter.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "water.hpp"

/**
 * Phases of the interior loop, as GameMode holds them.
 */
// clang-format off
enum EdInMode {
    ED_IN_MODE_WALK      = 0, /**< Player walks the interior. */
    ED_IN_MODE_DOOR_OPEN = 1, /**< Door opening before the player leaves. */
    ED_IN_MODE_LEAVE     = 2, /**< Leaves the interior once the door has opened. */
    ED_IN_MODE_TALK      = 3, /**< Player talks to a villager. */
    ED_IN_MODE_EVENT     = 4, /**< Event script runs. */
    ED_IN_MODE_PAUSE     = 5, /**< Pause menu is open. */
    ED_IN_MODE_MENU_INIT = 6, /**< Menu is being set up. */
    ED_IN_MODE_MENU      = 7, /**< Menu runs. */
};

// clang-format on

/* Where the camera sits for one camera marker of the interior, and the box the player must stand in. */
struct INTERIOR_CAMERA {
    u8            unk_000[0x60];
    CFrame        frame;   /**< Placement of the camera marker in the world. */
    sceVu0FVECTOR max;     /**< Greater corner of the box, in the marker's space. */
    sceVu0FVECTOR min;     /**< Lesser corner of the box, in the marker's space. */
    int           link_id; /**< Marker number; zero marks the fallback camera. */
    u8            unk_2e4[0xC];
};

static void LoadInfo(char *script, int size);
static void setTexAnim();
static void StepWater();
static void MainDraw();
static void MoveCharacter();
static void MoveCamera(CCameraFollow *camera);
static int  GetDoorPos(int door_no, float *position, float *rotation, int *door_sound, int *motion);
static void VillagerCollision();
static int  LoadTexture();
static void LoadChara();
void        LoadData();

int LoadPTS(CMapParts *parts, u_int *archive);

/**
 * Views one function point as the words it is copied in.
 */
struct EPARTS_FUNC_WORDS {
    u_int words[sizeof(EPARTS_FUNC_DATA) / sizeof(u_int)]; /**< The record, one word at a time. */
};

int  GetFuncPoint(int parts_no, u_int *archive, EPARTS_FUNC_DATA *points);
void DrawWaterSurface(CCamera *camera);
void SetCameraPos(CFrame *frame, CCamera *camera, CCharacter *chara);

/* Word-aligned image of one function marker as stored in a part resource. */
struct EPARTS_FUNC_RECORD {
    int words[0x30]; /**< The record, one word at a time. */
};

/** Whether the camera stays at the interior's fixed camera markers. */
static int fix_camera = 1;

/** Which of the three follow distances the camera is at. */
static int camera_dist_mode = 1;

/** Settings of the interior being run. */
EDIT_IN_INFO *EdInInfo;

/** Phase the interior loop is in. */
static int GameMode;

/** Camera marker whose box the player last stood in. */
static INTERIOR_CAMERA *active_camera;

/** Frames the player may stand outside the active camera marker before the camera switches. */
static int camera_change_count;

/** Number of camera markers the interior's parts define. */
static int camera_num;

/** Frames left of the door-opening sequence. */
static int door_open_cnt;

/** Camera the interior is drawn through this frame. */
static CCamera *NowCamera;

/** The player's character in the interior. */
static CCharacter *Chara;

/** Time of day the interior is lit for. */
static float NowTime;

/** Number of object animations the interior uses. */
static int obj_anime_num;

/** Number of effects the interior uses. */
static int effect_num;

/** Number of parts the interior is built from. */
static int parts_num;

/** Function points the interior's parts define. */
static EPARTS_FUNC_DATA *func_point;

/** Number of function points the interior's parts define. */
static int func_num;

/** Event to run when the interior starts, or below zero for none. */
static int start_event_no;

/** Whether the start event is a system event. */
static int start_system_event;

/** Menu the interior leaves for. */
static int goto_menu;

/** Whether the pause key was pressed this frame. */
static int goto_return_menu;

/** Frames since any button was last held. */
static int key_counter;

/** Frames the interior loop has run. */
static int loop_counter;

/** Animated texture the interior's parts share. */
static CTextureAnime TexAnime;

/** Animation records of the interior's animated textures. */
static CTexAnimeData TexAnimeData[64];

/** Light that flickers with the interior's fires. */
static CFireOmni Fire;

/** Position the player is held at while the camera is fixed. */
static sceVu0FVECTOR fix_chara_pos;

/** Rotation the player is held at while the camera is fixed. */
static sceVu0FVECTOR fix_chara_rot;

/** The parts the interior is built from. */
static CMapParts InteriorParts[10];

/** Named parts that move under the interior's script. */
static CCharacter MotionParts__2[4];

/** Water surfaces the interior draws. */
static CGroundWater Water[1];

/** Camera that follows the player. */
static CCameraFollow MainCamera(60.0f, 20.0f, 0.0f, 4.0f);

/** Camera that frames a conversation. */
static CCameraFollow TalkCamera(60.0f, 20.0f, 0.0f, 4.0f);

/** Camera the interior's events drive. */
static CCameraFollow EventCamera(60.0f, 20.0f, 0.0f, 4.0f);

/** Camera that looks from the player's eyes. */
static CCamera ViewCamera__2(4.0f);

/** Camera used while leaving the interior. */
static CCamera ExitCamera(4.0f);

/**
 * Identifies the kind of editor effect requested.
 */
// clang-format off
enum EFFECT_TYPE {
    EFFECT_FIRE = 1, /**< A fire. */
    EFFECT_FLAME,    /**< A flame. */
    EFFECT_BRIGHT,   /**< A glow. */
};

// clang-format on

/**
 * Splits one element's space-separated object names into the caller's name buffers.
 */
void GetElementObjName(EDITPARTS_INFO *info, char **names, int element, int mode) {
    names[0][0] = '\0';

    if (info == NULL) {
        return;
    }

    if (element < 0 || element >= 6) {
        return;
    }

    if (mode < 0 || mode >= 4) {
        return;
    }

    char *text = info->elements[element].names[mode];

    if (text == NULL) {
        return;
    }

    int  word = 0;
    int  length = 0;
    char c;

    while ((c = *text) != '\0') {
        if (c == ' ') {
            names[word][length] = '\0';
            length = 0;
            word++;
        } else {
            names[word][length] = c;
            length++;
        }

        text++;
    }

    names[word][length] = '\0';
    word++;
    names[word][0] = '\0';
}

/**
 * Reads one interior object's levels of detail and hangs them off its part.
 *
 * @mangled LoadMapObject__FP9CMapPartsPPUiP14CDataAlloc2_1_
 * @address 0x19B790
 * @size 0x204
 */
void LoadMapObject(CMapParts *parts, u_int **data, CDataAlloc2<1> *alloc) {
    CFrameVu1 *frames[4];
    int        i;

    LoadMDSFileLOD(frames, data, alloc, 1);
    CFrameAttr attr;
    attr.fog_enable = true;
    attr.clip_enable = false;
    attr.alpha_ref = 0x40;

    for (i = 1; i < 4; i++) {
        if (frames[i] != NULL) {
            frames[i]->SetAttr(attr, 1, 0x44);
            SetFrameAttr(frames[i], 1);
        }
    }

    attr.clip_depth = 100.0f;
    attr.clip_enable = true;
    frames[0]->SetAttr(attr, 1, 0);
    SetFrameAttr(frames[0], 1);

    for (i = 0; i < 4; i++) {
        parts->SetFrame(frames[i], i);
    }

    if (data[4] != NULL) {
        parts->shadow_frame = LoadMDSFile(data[4], alloc, 14, NULL, NULL);
    }

    if (data[5] != NULL) {
        parts->collision_frame = LoadCollisionFile(data[5], alloc);
    } else {
        parts->collision_frame = NULL;
    }

    if (data[6] != NULL) {
        parts->shade_frame = LoadMDSFile(data[6], alloc, 0, NULL, NULL);
    }

    if (data[7] != NULL) {
        parts->ripple_frame = LoadMDSFile(data[7], alloc, 0, NULL, NULL);
    }

    if (data[8] != NULL) {
        parts->camera_frame = LoadCollisionFile(data[8], alloc);
    }
}

/**
 * Reads the interior's event script into the script arena.
 *
 * @mangled LoadScript__Fv__2
 * @address 0x19B9A0
 * @size 0x294
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void LoadScript() {
    char  event_path[0x80];
    char  directory[0x40];
    char  message_path[0x40];
    char  language[0x1c];
    int   size;
    char *message_data;

    EdScriptBuffer.used = 0;
    EdEventData = (char *) (EdScriptBuffer.base + EdScriptBuffer.used * 16);
    sprintf(language, "_%d.mes", LanguageCode);
    strcpy(message_path, EdInInfo->name);
    int length = strlen(message_path);

    if (length > 0) {
        char last = message_path[length - 1];

        if (last == 'm' || last == 'n' || last == 'e') {
            message_path[length - 1] = '\0';
        }
    }

    strcat(message_path, language);
    printf("mes = %s\n", message_path);
    strcpy(directory, EdInInfo->name);
    char *cursor = directory;
    char *slash = cursor;
    char  c;

    while ((c = *cursor) != '\0') {
        if (c == '/') {
            slash = cursor;
        }

        cursor++;
    }

    *slash = '\0';
    sprintf(event_path, "%s/event.stb", directory);

    if (LoadFile2(event_path, EdEventData, &size, 0) != 0) {
        EdScriptBuffer.Alloc((size >> 4) + 1);
        message_data = (char *) (EdScriptBuffer.base + EdScriptBuffer.used * 16);

        if (LoadFile2(message_path, message_data, &size, 0) != 0) {
            EdScriptBuffer.Alloc((size >> 4) + 1);
        } else {
            message_data = NULL;
        }

        EdSetEventScript(EdEventData, message_data, &EdScriptBuffer);
    } else {
        EdEventData = NULL;
        EdSetEventScript(NULL, NULL, &EdScriptBuffer);
        EdInitEventParam();
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
 * Starts an interior event, handing it the camera it is to play through.
 *
 * @mangled RunEvent__FiP7CCamera__2
 * @address 0x19BC40
 * @size 0xB4
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void RunEvent(int event_no, CCamera *camera) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;

    if (start_event_no <= 0) {
        if (camera != NULL) {
            camera->GetPos(position);
            camera->GetRef(reference);
            EventCamera.SetPos(position);
            EventCamera.SetRef(reference);
            EventCamera.FollowOff();
            EventCamera.SetRoll(0.0f);
        }

        start_event_no = event_no;
    }
}

/**
 * Starts an interior system event, handing it the camera it is to play through.
 *
 * @mangled RunSystemEvent__FiP7CCamera__2
 * @address 0x19BD00
 * @size 0xA0
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void RunSystemEvent(int event_no, CCamera *camera) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR reference;

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
 * Divides the read buffer into the interior's work, NPC and menu arenas.
 *
 * @mangled InitWorkBuffer__Fv__2
 * @address 0x19BDA0
 * @size 0x90
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void InitWorkBuffer() {
    u_char *free_start = EdNPCBuffer.base + EdNPCBuffer.used * 16;
    int     free_quads = EdNPCBuffer.limit - EdNPCBuffer.used;
    free_start = (u_char *) ((((int) free_start >> 6) + 1) << 6);
    EdWorkBuffer.base = free_start;
    EdWorkBuffer.limit = free_quads - 4;
    EdWorkBuffer.used = 0;
    int villagers_loaded;

    if (EdVillagerBuffer.used > 0) {
        villagers_loaded = 1;
    }

    free_start = (u_char *) read_buffer;
    EdMenuBuffer.base = free_start - 0x180000;
    EdMenuBuffer.limit = 0x3A2E0;
    EdMenuBuffer.used = 0;
}

/**
 * Builds everything one interior runs on: its data, models, script and camera.
 *
 * @mangled EditInInit__FfPc
 * @address 0x19BE30
 * @size 0x478
 */
int EditInInit(float time, char *name);

/**
 * Runs one frame of the interior and reports when it is to be left.
 *
 * @mangled EditInLoop__Fv
 * @address 0x19C2B0
 * @size 0x1014
 */
int EditInLoop();

/**
 * Draws the interior for one frame.
 *
 * @mangled MainDraw__Fv__2
 * @address 0x19D2D0
 * @size 0x6AC
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void MainDraw() {
    sceVu0FVECTOR position;
    int           i;

    if (EdDrawOffFlag != 0) {
        return;
    }

    if (GameMode == ED_IN_MODE_PAUSE) {
        CTextureAnime::stop_anime = 1;
    } else {
        CTextureAnime::stop_anime = 0;
    }

    EdDrawOffMap = EdEventInfo.suppress_background;
    EdDrawOffMapShadow = EdEventInfo.suppress_shadows;
    Chara->GetPosition(position);
    int hour = (int) (NowTime / 3.0f);

    if (EdDrawOffMap == 0) {
        TexManager.ReloadTexture(GetVif1Packet(), 15);
        TexAnime.TexAnime(15);
        setTexAnim();
        sceVu0FVECTOR lod_distance = {0.0f, 1000.0f, 10000.0f, 1000000.0f};

        for (i = 0; i < parts_num; i++) {
            InteriorParts[i].DrawParts(NowTime, lod_distance, 0, 0, NULL);
        }
    }

    sceGsTex0 frame_tex;
    CRect_i_  screen;
    sceGsTex0 water_tex;
    MGGetFBuffTex(&frame_tex);
    screen.x = 0;
    screen.y = 0;
    screen.width = 0x280;
    screen.height = SCREEN_HALF_HEIGHT;
    water_tex = *(sceGsTex0 *) &TexManager.GetTexture("water_buff", -1)->tex0;
    MGMoveImage(&frame_tex, screen, &water_tex, 0, 0, 0);
    sceVu0FVECTOR ref;
    MainCamera.GetRef(ref);
    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);
    DrawWaterSurface(NowCamera);
    MGSetGsZBUF(&mgZBuffer);

    if (GameMode == ED_IN_MODE_EVENT) {
        EdEventBackSpriteDraw();
    }

    if (GameMode == ED_IN_MODE_EVENT) {
        EdDrawItem();
    }

    ED_EVENT_INFO *event = NULL;
    int            detail = 3;
    int            marks_store[10];
    int           *marks = marks_store;

    if (EdCheckViewMode() != 0) {
        detail = 0;
    }

    if (GameMode == ED_IN_MODE_LEAVE) {
        marks = NULL;
    } else {
        for (i = 0; i < 10; i++) {
            marks_store[i] = 3;
        }
    }

    if (GameMode == ED_IN_MODE_EVENT) {
        event = &EdEventInfo;
        detail = 3;
    }

    EdDrawCharacter(Chara, detail, 10, EdVillager, marks, 1, event);
    TexManager.ReloadTexture(GetVif1Packet(), 0x18);
    sceVu0FVECTOR wind = {0.0f, 0.0f, 0.0f, 0.0f};

    if (GameMode != ED_IN_MODE_PAUSE || EdPauseFlag == 0) {
        EditEffectStep();
        EffectMacroStep(wind);
        EdEffectGroup.Step(1);

        for (i = 0; i < 4; i++) {
            MotionParts__2[i].Step();
        }
    }

    EditEffectStep2();

    if (EdDrawOffMap == 0) {
        EDIT_EFFECT_INFO *effect = EdInInfo->effects;

        for (i = 0; i < effect_num; i++, effect++) {
            if (CheckEditEffect(effect, NowTime) != 0) {
                DrawEditEffect(effect, NowCamera, &EdEffectGroup);
            }
        }
    }

    EdEffectGroup.Draw();

    for (i = 0; i < 4; i++) {
        TexManager.ReloadTexture(GetVif1Packet(), i + 0x32);
        MotionParts__2[i].Draw();
    }

    TexManager.ReloadTexture(Vif1Packet, 0x14);
    EdDrawSysCursor(EdInInfo->event_points, 32);

    if (GameMode == ED_IN_MODE_EVENT) {
        EdEventSpriteDraw();
    }

    if ((unsigned int) (GameMode - 3) < 2U) {
        TexManager.ReloadTexture(Vif1Packet, EditMes1.tex_block);
        EditMes1.DrawMesWin();

        if (GameMode == ED_IN_MODE_EVENT) {
            EditEventMes1.DrawMesWin();
            EditSystemMes.DrawMesWin();
        }
    }

    if (GameMode == ED_IN_MODE_WALK) {
        MonsterNameDraw();
    } else {
        MonsterNameMake(-1);
    }

    EdSystemMesStep();
    EdSystemMesDraw();
    static int debug_flag = 0;
    static int debug_menu_mode = 0;

    if (DebugMode) {
        if (GamePad.Down(PAD_L3)) {
            debug_flag = !debug_flag;
        }

        if (debug_menu_mode) {
            GamePad.KeyLock(0);
            EdDebugMenu();

            if (GamePad.Down(PAD_R3) || EdDebugRunEventNo > 0) {
                debug_menu_mode = 0;
                GamePad.AutoRepeatOff();
                RunEvent(EdDebugRunEventNo, NowCamera);
            } else {
                GamePad.KeyLock(1);
            }
        } else {
            if (GameMode != ED_IN_MODE_EVENT && GamePad.Down(PAD_R3)) {
                GamePad.SetAutoRepeat(PAD_DPAD, 25, 3);
                GamePad.SetAutoRepeat(PAD_L1 | PAD_R1, 25, 3);
                debug_menu_mode = 1;
                debug_flag = 0;
            }

            EdDDebug(debug_flag);
            EdDPrintChara((CMainChara *) Chara);
            EdDPrintCamera(NowCamera);
            EdDDrawFont();
        }
    }

    if (EdDebugParamDrawOff == 0) {
        char pause_texture[] = "pause";

        if (GameMode == ED_IN_MODE_PAUSE || EdPauseFlag != 0) {
            TexManager.ReloadTexture(GetVif1Packet(), 0x14);
            CRect_i_ fade;
            fade.x = 0;
            fade.y = 0;
            fade.width = 0x2800;
            fade.height = SCREEN_HALF_HEIGHT * 16;
            MGFillBox(fade, 0, 0, 0, 0x40);
            setbilinear(0);
            CRect_i_ place;
            CRect_i_ texel;
            texel.x = 0;
            texel.y = 0;
            texel.width = 0x80;
            texel.height = 0x28;
            place.x = 0x100;
            place.y = SCREEN_HALF_HEIGHT - 20;
            place.width = 0x80;
            place.height = 0x28;
            sceVif1Packet *packet = GetVif1Packet();
            set2DSprite(packet, TexManager.GetTexture(pause_texture, -1), place, texel, 0x80);
        }
    }

    EdFadeInOut();
}

/**
 * Draws the interior's water surfaces, ordered back to front from the camera.
 *
 * @mangled DrawWaterSurface__FP7CCamera
 * @address 0x19D980
 * @size 0x15C
 */
void DrawWaterSurface(CCamera *camera);

/**
 * Advances the ripples of the interior's water surfaces.
 *
 * @mangled StepWater__Fv
 * @address 0x19DAE0
 * @size 0x204
 */
static void StepWater() {
    int           i;
    int           j;
    CGroundWater *surface = Water;

    for (i = 0; i < 1; i++, surface++) {
        if (surface->draw == 0) {
            continue;
        }

        if (surface->water.CheckClip()) {
            continue;
        }

        for (j = 0; j < 4; j++) {
            if (surface->ripples[j].power == 0.0f && surface->ripples[j].range == 0.0f) {
                break;
            }

            int row = surface->ripples[j].row;
            int column = surface->ripples[j].column;

            if (row < 0) {
                int rows = surface->water.rows;
                row = rows * (float) rand() / 2.1474836e9f;
            }

            if (column < 0) {
                int rows = surface->water.rows;
                column = rows * (float) rand() / 2.1474836e9f;
            }

            surface->water.Shake(row, column, surface->ripples[j].power + surface->ripples[j].range * (float) rand() / 2.1474836e9f);
        }

        surface->water.Hamon();
    }
}

/**
 * Moves the player and the villagers through the interior for one frame.
 *
 * @mangled MoveCharacter__Fv
 * @address 0x19DCF0
 * @size 0x38C
 */
static void MoveCharacter() {
    sceVu0FVECTOR        follow;
    static sceVu0FVECTOR fix_pos;

    EdMoveCharaInfo.time = NowTime;
    EdMoveCharaInfo.camera = &MainCamera;
    EdMoveCharaInfo.view_camera = &ViewCamera__2;
    EdMoveCharaInfo.key_lock = false;
    EdMoveCharaInfo.chara = Chara;
    EdMoveCharaInfo.interior = true;
    EdMoveCharaInfo.parts = InteriorParts;
    EdMoveCharaInfo.parts_count = parts_num;
    EdMoveCharaInfo.points = EdInInfo->event_points;
    EdMoveCharaInfo.point_count = 32;
    EdMoveCharaInfo.acted = false;
    EdMoveChara();

    if (EdDebugCameraFlag != 0 && GamePad.Down(PAD_CIRCLE) != 0) {
        fix_camera = !fix_camera;
        MainCamera.GetPos(fix_pos);
    }

    if (fix_camera != 0) {
        MainCamera.FollowOff();
    } else {
        MainCamera.FollowOn();
        Chara->GetPosition(follow);
        MainCamera.SetFollow(follow[0], 14.0f + follow[1], follow[2]);
        MoveCamera(&MainCamera);
    }

    if (EdMoveCharaInfo.system_event_no > 0) {
        RunSystemEvent(EdMoveCharaInfo.system_event_no, &MainCamera);
        return;
    }

    if (EdMoveCharaInfo.event_no > 0) {
        RunEvent(EdMoveCharaInfo.event_no, &MainCamera);
        return;
    }

    if (EdMoveCharaInfo.acted == 0 && EdPadDown(0x40, 1) != 0) {
        sceVu0FVECTOR position;
        sceVu0FVECTOR start = {-40.5f, 0.0f, -12.55f, 1.0f};
        sceVu0FVECTOR rotation = {0.0f, 0.0f, 0.0f, 0.0f};
        Chara->GetPosition(position);
        Chara->GetRotation(rotation);
        EPARTS_FUNC_DATA *jump;

        jump = NULL;

        if ((DebugMode && GamePad.Down(PAD_SELECT)) || (jump = SearchMapJump(position, rotation))) {
            int motion;

            EdMoveCharaInit();
            motion = 0;
            door_open_cnt = 140;

            if (jump != NULL) {
                GetDoorPos(jump->link_id, position, rotation, &EdInteriorDoorSound, &motion);
            }

            Chara->SetMotion(motion, 6);
            sceVu0CopyVector(fix_chara_pos, position);
            sceVu0CopyVector(fix_chara_rot, rotation);
            Chara->SetPosition(fix_chara_pos);
            Chara->SetRotation(fix_chara_rot[0], fix_chara_rot[1], fix_chara_rot[2]);
            Chara->Step();
            Chara->ClothStep(-1);
            GameMode = ED_IN_MODE_DOOR_OPEN;
            EdFadeOut(100, 0.0f, 0.0f, 0.0f);
        }
    }
}

/**
 * Applies the right stick to the interior camera, holding its height and distance in
 * range.
 *
 * @mangled MoveCamera__FP13CCameraFollow__2
 * @address 0x19E080
 * @size 0x164
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void MoveCamera(CCameraFollow *camera) {
    static float camera_distance[3] = {20.0f, 60.0f, 100.0f};

    float horizontal = GamePad.GetRXf();
    camera->AddHeight(-GamePad.GetRYf());

    if (!(camera->GetHeight() <= 30.0f)) {
        camera->SetHeight(30.0f);
    }

    camera->AddAngle(0.04f * -horizontal);

    if (GamePad.On(PAD_R1) != 0) {
        camera->AddAngle(-DEG_TO_RAD);
    }

    if (GamePad.On(PAD_L1) != 0) {
        camera->AddAngle(DEG_TO_RAD);
    }

    camera->SetDistance(camera_distance[camera_dist_mode]);

    if (GamePad.Down(PAD_TRIANGLE) != 0) {
        camera_dist_mode++;
    }

    if (camera_dist_mode >= 3) {
        camera_dist_mode = 0;
    }
}

/**
 * Finds the map jump the player is standing on.
 *
 * @mangled SearchMapJump__FPfPf
 * @address 0x19E1F0
 * @size 0xF4
 */
EPARTS_FUNC_DATA *SearchMapJump(float *position, float *rotation);

/**
 * Puts the player where the interior's map jump says they arrive.
 *
 * @mangled GetMapJumpPos__FP10CCharacter
 * @address 0x19E2F0
 * @size 0x12C
 */
void GetMapJumpPos(CCharacter *chara);

/**
 * Gives the position, heading and part of one of the interior's doors.
 *
 * @mangled GetDoorPos__FiPfPfPiPi
 * @address 0x19E420
 * @size 0xF8
 */
static int GetDoorPos(int door_no, float *position, float *rotation, int *door_sound, int *motion) {
    EPARTS_FUNC_DATA *point = func_point;

    for (int i = 0; i < func_num; i++, point++) {
        if ((point->kind == EPARTS_FUNC_DOOR_SIDE_A || point->kind == EPARTS_FUNC_DOOR_SIDE_B) && point->link_id == door_no) {
            sceVu0CopyVector(position, point->position);
            sceVu0CopyVector(rotation, point->rotation);
            *door_sound = point->values[0];
            *motion = EdGetDoorMotion(*door_sound, point->kind == EPARTS_FUNC_DOOR_SIDE_A);
            return point->kind == EPARTS_FUNC_DOOR_SIDE_B;
        }
    }

    return -1;
}

/**
 * Places the interior camera so that the player stays in view and the walls do not.
 *
 * @mangled SetCameraPos__FP6CFrameP7CCameraP10CCharacter
 * @address 0x19E520
 * @size 0x3F8
 */
void SetCameraPos(CFrame *frame, CCamera *camera, CCharacter *chara);

/**
 * Collects the collision polygons of the interior parts meeting a box.
 *
 * @mangled GetCollision__FP6CCPolyP7CBoxVu0__2
 * @address 0x19E920
 * @size 0xBC
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static int GetCollision(CCPoly *poly, CBoxVu0 *box) {
    int count = 0;

    for (int i = 0; i < parts_num; i++) {
        CFrame *frame = InteriorParts[i].GetCollisionFrame();

        if (frame != NULL) {
            count += frame->PickUpNearPoly(&poly[count], *box);
        }
    }

    return count;
}

/**
 * Pushes the player and the interior's villagers out of one another.
 *
 * @mangled VillagerCollision__Fv__2
 * @address 0x19E9E0
 * @size 0x2D0
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void VillagerCollision() {
    sceVu0FVECTOR position;
    CBoxVu0       box;
    sceVu0FVECTOR ground;
    CCPoly        poly;

    for (int i = -1; i < 10; i++) {
        int           snap;
        CCharacter   *chara;
        CNPCharacter *villager = NULL;
        int           foot_sound;

        if (i >= 0) {
            villager = &EdVillager[i];
            snap = EdEventInfo.npc_collision[i];

            if (villager->CheckDraw() == 0) {
                continue;
            }

            villager->GetPosition(position);
            foot_sound = EdEventInfo.npc_foot_sound[i];
        } else {
            snap = EdEventInfo.player_collision;
            Chara->GetPosition(position);
            foot_sound = EdEventInfo.player_foot_sound;
        }

        WorkBuffer__2->used = 0;
        CCPoly *polys = (CCPoly *) WorkBuffer__2->Alloc(2000);
        box.max[0] = 1.0f + position[0];
        box.min[0] = position[0] - 1.0f;
        box.max[2] = 1.0f + position[2];
        box.min[2] = position[2] - 1.0f;
        box.max[1] = 1000.0f;
        box.min[1] = -1000.0f;

        if (i >= 0) {
            chara = villager;
        } else {
            chara = Chara;
        }

        chara->FootSoundEnable(0);
        int count = GetCollision(polys, &box);

        if (count > 300) {
            printf("cpoly over!!!! %d\n", count);
        }

        if (count > 0) {
            position[1] += 20.0f;

            if (GetFootPoly(position, 1000.0f, &poly, ground, polys, count, 0) != 0) {
                sceVu0CopyVector(position, ground);
                chara->FootSoundEnable(foot_sound);

                if (foot_sound == 1) {
                    chara->SetFootSoundID(poly.attr.foot_sound);
                }
            } else {
                position[1] = 0.0f;
            }
        } else {
            position[1] = 0.0f;
        }

        if (snap != 0) {
            if (i >= 0) {
                villager->SetPosition(position);
            } else {
                Chara->SetPosition(position);
            }
        }
    }
}

/**
 * Enters the interior's textures once they have been read.
 *
 * @mangled LoadTexture__Fv__2
 * @address 0x19ECB0
 * @size 0x20C
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static int LoadTexture() {
    static LOADTEXTURE_INFO2 texdata[16] = {
        {NULL, 15}
    };

    BG_READ_INFO *file = GetReadBGFile(2);

    if (file == NULL) {
        return 0;
    }

    char *ext = file->name;
    char  c;

    while ((c = *ext) != '\0') {
        if (c == '.') {
            ext++;
            break;
        }

        ext++;
    }

    u_int *cfg_data = NULL;
    int    size;
    TexAnime.Initialize(NULL, 0);

    if (strcmp(ext, "img") == 0) {
        texdata[0].name = (char *) file->buffer;
    } else {
        u_int *found;
        int    cfg_size;

        if (GetPackFileExt((u_int *) file->buffer, "img", &found, 1, NULL, NULL) > 0) {
            texdata[0].name = (char *) found;
        }

        if (GetPackFileExt((u_int *) file->buffer, "cfg", &found, 1, &cfg_size, NULL) > 0) {
            TexAnime.Initialize(TexAnimeData, 64);
            cfg_data = found;
            size = cfg_size;

            for (int i = 0; i < 64; i++) {
                TexAnimeData[i].Initialize();
            }
        }
    }

    TexManager.LoadTextureBlockEX(15, texdata);

    if (cfg_data != NULL) {
        TexAnime.LoadCFGFile((char *) cfg_data, size);
    }

    EdNPCBuffer.Alloc((file->size >> 4) + 1);
    return 0;
}

/**
 * Reserved character-loading hook with no operation.
 */
static void LoadChara() {
}

/**
 * Reads the interior's parts, objects and function points out of its archive.
 *
 * @mangled LoadData__Fv
 * @address 0x19EED0
 * @size 0x818
 */
void LoadData();

/**
 * Builds one interior part from its archive entry.
 *
 * @mangled LoadPTS__FP9CMapPartsPUi
 * @address 0x19F6F0
 * @size 0x238
 */
int LoadPTS(CMapParts *parts, u_int *archive) {
    EPARTS_INFO_HEADER *header = (EPARTS_INFO_HEADER *) ((char *) archive + archive[1]);
    u_int              *files[10] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    EPARTS_ARCHIVE     *pack = (EPARTS_ARCHIVE *) archive;

    if (pack->lod0_size > 0) {
        files[0] = (u_int *) ((char *) archive + pack->lod0_offset);
    } else {
        return 0;
    }

    if (pack->lod1_size > 0) {
        files[1] = (u_int *) ((char *) archive + pack->lod1_offset);
    }

    if (pack->lod2_size > 0) {
        files[2] = (u_int *) ((char *) archive + pack->lod2_offset);
    }

    if (pack->lod3_size > 0) {
        files[3] = (u_int *) ((char *) archive + pack->lod3_offset);
    }

    if (pack->shadow_size > 0) {
        files[4] = (u_int *) ((char *) archive + pack->shadow_offset);
    }

    if (pack->collision_size > 0) {
        files[5] = (u_int *) ((char *) archive + pack->collision_offset);
    }

    if (pack->shade_size > 0) {
        files[6] = (u_int *) ((char *) archive + pack->shade_offset);
    }

    if (pack->extra_size > 0) {
        files[7] = (u_int *) ((char *) archive + pack->extra_offset);
    }

    if (pack->camera_size > 0) {
        files[8] = (u_int *) ((char *) archive + pack->camera_offset);
    }

    parts->SetFrame(LoadMDSFile(files[0], &EdNPCBuffer, 0, NULL, NULL), 0);

    if (files[5] != NULL) {
        parts->collision_frame = LoadCollisionFile(files[5], &EdNPCBuffer);
    }

    if (files[8] != NULL) {
        parts->camera_frame = LoadCollisionFile(files[8], &EdNPCBuffer);
    } else {
        parts->camera_frame = NULL;
    }

    sceVu0FVECTOR position = {0.0f, 0.0f, 0.0f, 1.0f};
    position[0] = header->position[0];
    position[1] = header->position[1];
    position[2] = header->position[2];
    parts->SetPosition(position);
    parts->SetRotation(header->rotation[0], header->rotation[1], header->rotation[2]);
    return 0;
}

/**
 * Collects the function points one interior part defines.
 *
 * @mangled GetFuncPoint__FiPUiP16EPARTS_FUNC_DATA
 * @address 0x19F930
 * @size 0x74
 */
int GetFuncPoint(int parts_no, u_int *archive, EPARTS_FUNC_DATA *points);

/** Frame of the interior's animated texture. */
static int setTexAnimCnt;

/** Clock that advances the interior's animated texture. */
static float setTexAnimCntf;

/**
 * Uploads the interior's texture-animation state to the graphics synthesizer.
 *
 * @mangled setTexAnim__Fv
 * @address 0x19F9B0
 * @size 0x1C0
 */
static void setTexAnim() {
    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);

    if (setTexAnimCntf >= 7.0f) {
        setTexAnimCntf = 0.0f;
    } else {
        setTexAnimCntf += 0.2f;
    }

    setTexAnimCnt = (int) setTexAnimCntf;

    CTexture *strip = TexManager.GetTexture("i01e01_a", -1);
    CTexture *plate = TexManager.GetTexture("i01e01", -1);

    if (strip == 0 || plate == 0) {
        return;
    }

    MGMoveImage((sceGsTex0 *) &strip->tex0, CRect_i_(0, setTexAnimCnt * 64, 64, 64), (sceGsTex0 *) &plate->tex0, 0, 0, 0);
    sceVif1PkCnt(Vif1Packet, 0);
    sceVif1PkOpenDirectCode(Vif1Packet, 0);
    sceVif1PkOpenGifTag(Vif1Packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(Vif1Packet, SCE_GS_TEXFLUSH, 0);
    sceVif1PkCloseGifTag(Vif1Packet);
    sceVif1PkCloseDirectCode(Vif1Packet);
}

/** Number of villagers the info script has placed. */
static int npc_count;

/** Number of object animations the info script has defined. */
static int objanime_list;

/** Number of effects the info script has defined. */
static int effect_list;

/** Whether the info script asked for debug drawing. */
static int debug;

/** Number of motion parts the info script has defined. */
static int motion_parts_list;

/** Number of water surfaces the info script has defined. */
static int water_list;

/** Water surface record the info script is filling in. */
static EDIT_WATER_INFO *water_info;

/** Directory that names in the info script are relative to. */
static char CurrentDir[0x80];

static void CommandAMBIENT(void **arguments);
static void CommandLIGHT_C(void **arguments);
static void CommandFOG(void **arguments);
static void CommandBG_COL(void **arguments);
static void CommandPROJECTION(void **arguments);
static void CommandPEOPLE(void **arguments);
static void CommandCD(void **arguments);
static void CommandOBJ_ANIME(void **arguments);
static void CommandFIRE(void **arguments);
static void CommandFLAME(void **arguments);
static void CommandBRIGHT(void **arguments);
static void CommandDEBUG(void **arguments);
static void CommandMOTION_PARTS(void **arguments);
static void CommandWATER_SURFACE(void **arguments);
static void CommandWATER_SHAKE(void **arguments);

/** The keywords an interior's info script may use. */
static TAG_PARAM Command[15] = {
    {"AMBIENT",       {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                      },
    {"LIGHT_C",       {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                        },
    {"FOG",           {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                    },
    {"BG_COL",        {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                      },
    {"PROJECTION",    {SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    },
    {"PEOPLE",        {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                      },
    {"CD",            {SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   },
    {"OBJ_ANIME",     {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                  },
    {"FIRE",          {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                       },
    {"FLAME",         {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                       },
    {"BRIGHT",        {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                       },
    {"DEBUG",         {-1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           },
    {"MOTION_PARTS",  {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                    },
    {"WATER_SURFACE", {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}},
    {"WATER_SHAKE",   {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                           },
};

/** Handlers of the info script's keywords, in the order of Command. */
static void (*CommandExe[15])(void **) = {
    CommandAMBIENT,
    CommandLIGHT_C,
    CommandFOG,
    CommandBG_COL,
    CommandPROJECTION,
    CommandPEOPLE,
    CommandCD,
    CommandOBJ_ANIME,
    CommandFIRE,
    CommandFLAME,
    CommandBRIGHT,
    CommandDEBUG,
    CommandMOTION_PARTS,
    CommandWATER_SURFACE,
    CommandWATER_SHAKE,
};

/**
 * Runs the interior's info script through the interpreter.
 *
 * @mangled LoadInfo__FPci
 * @address 0x19FB70
 * @size 0xD8
 */
static void LoadInfo(char *script, int size) {
    CScriptInterpreter interpreter;
    int                command;

    interpreter.SetScript(script, size);
    interpreter.SetTAG(Command, 15);
    CurrentDir[0] = '\0';
    npc_count = 0;
    objanime_list = 0;
    effect_list = 0;
    motion_parts_list = 0;
    water_list = 0;
    water_info = NULL;
    debug = 0;

    for (;;) {
        command = interpreter.GetNextTAG();

        if (command < 0) {
            break;
        }

        CommandExe[command](interpreter.arguments);
    }

    obj_anime_num = objanime_list;
    effect_num = effect_list;
}

/**
 * Sets the interior's ambient light colour.
 *
 * @mangled CommandAMBIENT__FPPv__2
 * @address 0x19FC50
 * @size 0x44
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandAMBIENT(void **arguments) {
    EdInInfo->ambient[0] = *(float *) arguments[0];
    EdInInfo->ambient[1] = *(float *) arguments[1];
    EdInInfo->ambient[2] = *(float *) arguments[2];
    EdInInfo->ambient[3] = 128.0f;
}

/**
 * Sets the colour and direction of one of the interior's lights.
 *
 * @mangled CommandLIGHT_C__FPPv__2
 * @address 0x19FCA0
 * @size 0x128
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandLIGHT_C(void **arguments) {
    sceVu0FVECTOR direction;
    int           light = *(int *) arguments[6];

    direction[0] = *(float *) arguments[0];
    direction[1] = *(float *) arguments[1];
    direction[2] = *(float *) arguments[2];
    direction[3] = 0.0f;
    sceVu0Normalize(direction, direction);
    light--;
    EdInInfo->light_direction[0][light] = direction[0];
    EdInInfo->light_direction[1][light] = direction[1];
    EdInInfo->light_direction[2][light] = direction[2];
    EdInInfo->light_direction[3][light] = direction[3];
    EdInInfo->light_colour[light][0] = *(float *) arguments[3];
    EdInInfo->light_colour[light][1] = *(float *) arguments[4];
    EdInInfo->light_colour[light][2] = *(float *) arguments[5];
    EdInInfo->light_colour[light][3] = 128.0f;
}

/**
 * Sets the interior's fog distances and colour.
 *
 * @mangled CommandFOG__FPPv__2
 * @address 0x19FDD0
 * @size 0x78
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandFOG(void **arguments) {
    EdInInfo->fog.near_distance = *(float *) arguments[0];
    EdInInfo->fog.far_distance = *(float *) arguments[1];
    EdInInfo->fog.red = *(int *) arguments[2];
    EdInInfo->fog.green = *(int *) arguments[3];
    EdInInfo->fog.blue = *(int *) arguments[4];
    EdInInfo->fog.intensity = *(float *) arguments[5];
    EdInInfo->fog.exponent = *(float *) arguments[6];
}

/**
 * Sets the colour the interior clears to.
 *
 * @mangled CommandBG_COL__FPPv__2
 * @address 0x19FE50
 * @size 0x44
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandBG_COL(void **arguments) {
    EdInInfo->background_colour[0] = *(float *) arguments[0];
    EdInInfo->background_colour[1] = *(float *) arguments[1];
    EdInInfo->background_colour[2] = *(float *) arguments[2];
    EdInInfo->background_colour[3] = 128.0f;
}

/**
 * Sets the interior's projection distance.
 *
 * @mangled CommandPROJECTION__FPPv
 * @address 0x19FEA0
 * @size 0x18
 */
static void CommandPROJECTION(void **arguments) {
    EdInInfo->projection = *(float *) arguments[0];
}

/**
 * Accepts the villager command and does nothing with it.
 *
 * @mangled CommandPEOPLE__FPPv__2
 * @address 0x19FEC0
 * @size 0x8
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandPEOPLE(void **arguments) {
}

/**
 * Moves the script's current directory to one below the interior's own.
 *
 * @mangled CommandCD__FPPv__2
 * @address 0x19FED0
 * @size 0x60
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandCD(void **arguments) {
    GetEditDataDir(CurrentDir);
    strcat(CurrentDir, "in/");
    strcat(CurrentDir, (char *) arguments[0]);
}

/**
 * Accepts the object-animation command and does nothing with it.
 *
 * @mangled CommandOBJ_ANIME__FPPv__2
 * @address 0x19FF30
 * @size 0x8
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandOBJ_ANIME(void **arguments) {
}

/**
 * Reserved effect-setup hook with no operation.
 */
static void SetEffect(EFFECT_TYPE type, char *name, float *position, float *scale, float *rotation) {
}

/**
 * Places a fire effect in the interior.
 *
 * @mangled CommandFIRE__FPPv__2
 * @address 0x19FF50
 * @size 0x94
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandFIRE(void **arguments) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR scale;
    sceVu0FVECTOR rotation = {0.0f, 0.0f, 0.0f, 0.0f};

    position[0] = *(float *) arguments[1];
    position[1] = *(float *) arguments[2];
    position[2] = *(float *) arguments[3];
    position[3] = 1.0f;
    scale[0] = *(float *) arguments[4];
    scale[1] = *(float *) arguments[4];
    scale[2] = *(float *) arguments[4];
    SetEffect(EFFECT_FIRE, (char *) arguments[0], position, scale, rotation);
}

/**
 * Places a flame effect in the interior.
 *
 * @mangled CommandFLAME__FPPv__2
 * @address 0x19FFF0
 * @size 0x94
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandFLAME(void **arguments) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR scale;
    sceVu0FVECTOR rotation = {0.0f, 0.0f, 0.0f, 0.0f};

    position[0] = *(float *) arguments[1];
    position[1] = *(float *) arguments[2];
    position[2] = *(float *) arguments[3];
    position[3] = 1.0f;
    scale[0] = *(float *) arguments[4];
    scale[1] = *(float *) arguments[4];
    scale[2] = *(float *) arguments[4];
    SetEffect(EFFECT_FLAME, (char *) arguments[0], position, scale, rotation);
}

/**
 * Places a glow effect in the interior.
 *
 * @mangled CommandBRIGHT__FPPv__2
 * @address 0x1A0090
 * @size 0x94
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandBRIGHT(void **arguments) {
    sceVu0FVECTOR position;
    sceVu0FVECTOR scale;
    sceVu0FVECTOR rotation = {0.0f, 0.0f, 0.0f, 0.0f};

    position[0] = *(float *) arguments[1];
    position[1] = *(float *) arguments[2];
    position[2] = *(float *) arguments[3];
    position[3] = 1.0f;
    scale[0] = *(float *) arguments[4];
    scale[1] = *(float *) arguments[4];
    scale[2] = *(float *) arguments[4];
    SetEffect(EFFECT_BRIGHT, (char *) arguments[0], position, scale, rotation);
}

/**
 * Turns the interior's debug drawing on.
 *
 * @mangled CommandDEBUG__FPPv
 * @address 0x1A0130
 * @size 0x10
 */
static void CommandDEBUG(void **arguments) {
    debug = 1;
}

/**
 * Names one of the interior's four moving parts.
 *
 * @mangled CommandMOTION_PARTS__FPPv__2
 * @address 0x1A0140
 * @size 0xF4
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandMOTION_PARTS(void **arguments) {
    if (motion_parts_list < 4) {
        EDIT_MOTION_PARTS_INFO *motion = &EdInInfo->motion_parts[motion_parts_list];
        motion_parts_list++;
        strcpy(motion->name, CurrentDir);
        strcat(motion->name, (char *) arguments[0]);
        motion->values[0] = *(float *) arguments[1];
        motion->values[1] = *(float *) arguments[2];
        motion->values[2] = *(float *) arguments[3];
        motion->values[3] = *(float *) arguments[4];
        motion->values[4] = *(float *) arguments[5];
        motion->values[5] = *(float *) arguments[6];
        motion->values[6] = *(float *) arguments[7];
        motion->values[7] = *(float *) arguments[8];
        motion->values[8] = *(float *) arguments[9];
    }
}

/**
 * Names one of the interior's eight water surfaces.
 *
 * @mangled CommandWATER_SURFACE__FPPv__2
 * @address 0x1A0240
 * @size 0x184
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandWATER_SURFACE(void **arguments) {
    if (water_list < 8) {
        EDIT_WATER_INFO *surface = &EdInInfo->water_surfaces[water_list];
        water_list++;
        strcpy(surface->name, (char *) arguments[0]);
        surface->grid_rows = *(int *) arguments[1];
        surface->grid_columns = *(int *) arguments[2];
        surface->corner_a[0] = *(float *) arguments[3];
        surface->corner_a[1] = *(float *) arguments[4];
        surface->corner_a[2] = *(float *) arguments[5];
        surface->corner_a[3] = 1.0f;
        surface->corner_b[0] = *(float *) arguments[6];
        surface->corner_b[1] = *(float *) arguments[7];
        surface->corner_b[2] = *(float *) arguments[8];
        surface->corner_b[3] = 1.0f;
        surface->corner_c[0] = *(float *) arguments[9];
        surface->corner_c[1] = *(float *) arguments[10];
        surface->corner_c[2] = *(float *) arguments[11];
        surface->corner_c[3] = 1.0f;
        surface->ripple_params[0] = *(float *) arguments[12];
        surface->ripple_params[1] = *(float *) arguments[13];
        surface->ripple_params[2] = *(float *) arguments[14];
        surface->ripple_params[3] = *(float *) arguments[15];
        surface->red = *(int *) arguments[16];
        surface->green = *(int *) arguments[17];
        surface->blue = *(int *) arguments[18];
        surface->follow[0] = *(int *) arguments[19];
        surface->follow[1] = *(int *) arguments[20];
        surface->follow[2] = *(int *) arguments[21];
        surface->parts_no = -1;
        water_info = surface;
    }
}

/**
 * Starts a ripple on the interior's water surface.
 *
 * @mangled CommandWATER_SHAKE__FPPv__2
 * @address 0x1A03D0
 * @size 0x9C
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void CommandWATER_SHAKE(void **arguments) {
    EDIT_WATER_INFO *info = water_info;

    if (info != NULL) {
        int i = 0;

        while (1) {
            if (info->wave[i].power == 0.0f && info->wave[i].range == 0.0f) {
                EDIT_WATER_WAVE_VIEW *wave = (EDIT_WATER_WAVE_VIEW *) &((EDIT_WATER_WAVE_INFO *) info)[i];
                wave->row = (float) *(int *) arguments[0];
                wave->column = (float) *(int *) arguments[1];
                wave->range = *(float *) arguments[3];
                wave->power = *(float *) arguments[2];
                break;
            }

            i++;
        }
    }
}
