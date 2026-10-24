#include "common.h"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "camera.hpp"
#include "camera_port.hpp"
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
#include "platform/input.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "scriptinterpreter.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "water.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's interior loop. EditInLoop reaches the static MainDraw, whose static setTexAnim flushed the
// texture cache through raw packets; the flushes are gone and the copy stays on MGMoveImage. MainDraw
// shares the interior's whole state with EditInInit and the script commands, so nearly the whole unit
// moves; only the part loaders, which touch none of it, stay retail.
// Retail declares fourteen of its variables static after editloop.hpp declares the same names extern;
// MWCC made them file-local, clang makes them the editor's globals. They carry an EdIn_ prefix here
// so the interior keeps its own player, cameras and counters as it does on the PS2.

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
namespace {
struct INTERIOR_CAMERA {
    u8            unk_000[0x60];
    CFrame        frame;   /**< Placement of the camera marker in the world. */
    sceVu0FVECTOR max;     /**< Greater corner of the box, in the marker's space. */
    sceVu0FVECTOR min;     /**< Lesser corner of the box, in the marker's space. */
    int           link_id; /**< Marker number; zero marks the fallback camera. */
    u8            unk_2e4[0xC];
};
} // namespace

namespace {
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
} // namespace

void LoadData();

int LoadPTS(CMapParts *parts, u_int *archive);

/**
 * Views one function point as the words it is copied in.
 */
namespace {
struct EPARTS_FUNC_WORDS {
    u_int words[sizeof(EPARTS_FUNC_DATA) / sizeof(u_int)]; /**< The record, one word at a time. */
};
} // namespace

int  GetFuncPoint(int parts_no, u_int *archive, EPARTS_FUNC_DATA *points);
void DrawWaterSurface(CCamera *camera);
void SetCameraPos(CFrame *frame, CCamera *camera, CCharacter *chara);

/* Word-aligned image of one function marker as stored in a part resource. */
namespace {
struct EPARTS_FUNC_RECORD {
    int words[0x30]; /**< The record, one word at a time. */
};
} // namespace

namespace {
/** Whether the camera stays at the interior's fixed camera markers. */
static int fix_camera = 1;

/** Which of the three follow distances the camera is at. */
static int EdIn_camera_dist_mode = 1;
} // namespace

/** Settings of the interior being run. */
extern EDIT_IN_INFO *EdInInfo;

namespace {
/** Phase the interior loop is in. */
static int GameMode;

/** Camera marker whose box the player last stood in, by its place in the list SetCameraPos builds each
 * frame, or -1 for none. */
static int active_camera = -1;

/** Frames the player may stand outside the active camera marker before the camera switches. */
static int camera_change_count;

/** Number of camera markers the interior's parts define. */
static int camera_num;

/** Frames left of the door-opening sequence. */
static int EdIn_door_open_cnt;

/** Camera the interior is drawn through this frame. */
static CCamera *EdIn_NowCamera;

/** The player's character in the interior. */
static CCharacter *EdIn_Chara;

/** Time of day the interior is lit for. */
static float EdIn_NowTime;

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
static int EdIn_goto_menu;

/** Whether the pause key was pressed this frame. */
static int EdIn_goto_return_menu;

/** Frames since any button was last held. */
static int EdIn_key_counter;

/** Frames the interior loop has run. */
static int EdIn_loop_counter;

/** Animated texture the interior's parts share. */
static CTextureAnime TexAnime;

/** Animation records of the interior's animated textures. */
static CTexAnimeData EdIn_TexAnimeData[64];

/** Light that flickers with the interior's fires. */
static CFireOmni Fire;

/** Position the player is held at while the camera is fixed. */
static sceVu0FVECTOR EdIn_fix_chara_pos;

/** Rotation the player is held at while the camera is fixed. */
static sceVu0FVECTOR EdIn_fix_chara_rot;

/** The parts the interior is built from. */
static CMapParts InteriorParts[10];

/** Named parts that move under the interior's script. */
static CCharacter MotionParts__2[4];

/** Water surfaces the interior draws. */
static CGroundWater Water[1];

/** Camera that follows the player. */
static CCameraFollow EdIn_MainCamera(60.0f, 20.0f, 0.0f, 4.0f);

/** Camera that frames a conversation. */
static CCameraFollow EdIn_TalkCamera(60.0f, 20.0f, 0.0f, 4.0f);

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
 * Reads the interior's event script into the script arena.
 *
 * @mangled LoadScript__Fv__2
 * @address 0x19B9A0
 * @size 0x294
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void LoadScript() {
    std::string event_path;
    std::string directory = EdInInfo->name;
    std::string message_path = EdInInfo->name;
    int         size;
    char       *message_data;

    EdScriptBuffer.used = 0;
    EdEventData = reinterpret_cast<char *>(EdScriptBuffer.base + EdScriptBuffer.used * 16);
    const std::string language = "_" + std::to_string(LanguageCode) + ".mes";
    int               length = static_cast<int>(message_path.size());

    if (length > 0) {
        char last = message_path[length - 1];

        if (last == 'm' || last == 'n' || last == 'e') {
            message_path.pop_back();
        }
    }

    message_path += language;
    printf("mes = %s\n", message_path.c_str());
    const auto slash = directory.rfind('/');
    directory.resize(slash == std::string::npos ? 0 : slash);
    event_path = directory + "/event.stb";

    if (LoadFile2(event_path.data(), EdEventData, &size, 0) != 0) {
        EdScriptBuffer.Alloc((size >> 4) + 1);
        message_data = reinterpret_cast<char *>(EdScriptBuffer.base + EdScriptBuffer.used * 16);

        if (LoadFile2(message_path.data(), message_data, &size, 0) != 0) {
            EdScriptBuffer.Alloc((size >> 4) + 1);
        } else {
            message_data = nullptr;
        }

        EdSetEventScript(EdEventData, message_data, &EdScriptBuffer);
    } else {
        EdEventData = nullptr;
        EdSetEventScript(nullptr, nullptr, &EdScriptBuffer);
        EdInitEventParam();
    }

    EdScriptBuffer.Align64();
    EdSystemEventData = reinterpret_cast<char *>(EdScriptBuffer.base + EdScriptBuffer.used * 16);

    if (LoadFile2("gedit/system/event.stb", EdSystemEventData, &size, 0) != 0) {
        EdScriptBuffer.Alloc((size >> 4) + 1);
        return;
    }

    EdSystemEventData = nullptr;
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
        if (camera != nullptr) {
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
    free_start = reinterpret_cast<u_char *>(((reinterpret_cast<std::uintptr_t>(free_start) >> 6) + 1) << 6);
    EdWorkBuffer.base = free_start;
    EdWorkBuffer.limit = free_quads - 4;
    EdWorkBuffer.used = 0;
    [[maybe_unused]] int villagers_loaded;

    if (EdVillagerBuffer.used > 0) {
        villagers_loaded = 1;
    }

    free_start = reinterpret_cast<u_char *>(read_buffer);
    EdMenuBuffer.base = free_start - 0x180000;
    EdMenuBuffer.limit = 0x3A2E0;
    EdMenuBuffer.used = 0;
}
} // namespace

/**
 * Builds everything one interior runs on: its data, models, script and camera.
 *
 * @mangled EditInInit__FfPc
 * @address 0x19BE30
 * @size 0x478
 */
PORT_OVERRIDE int EditInInit(float time, char *name) {
    if (name == nullptr || std::strlen(name) >= sizeof(EdInInfo->name)) {
        return 0;
    }

    MGSetFogParm(10000.0f, 50000.0f, 0, 0, 0, 255.0f, 255.0f);
    memset(EdInInfo, 0, sizeof(EDIT_IN_INFO));
    EdInInfo->projection = 800.0f;
    memcpy(EdInInfo->name, name, std::strlen(name) + 1);
    PlayTimeCountFlag(1);
    simple_event = 0;
    LoadScript();
    EdIn_NowTime = time;
    EdNPCBuffer.used = 0;

    for (int i = 0; i < 10; i++) {
        EdVillager[i].Initialize();
        EdVillager[i].texture_block = 0x36;
    }

    LoadTexture();
    BG_READ_INFO *bg_file = GetReadBGFile(1);

    if (bg_file != nullptr) {
        char *script_text = reinterpret_cast<char *>(EdNPCBuffer.Alloc((bg_file->size >> 4) + 1));
        memcpy(script_text, bg_file->buffer, bg_file->size);
        LoadInfo(script_text, bg_file->size);
    }

    LoadData();
    int     used = EdNPCBuffer.used;
    u_char *start = EdNPCBuffer.base + used * 16;
    int     remaining = EdNPCBuffer.limit - used;
    EdVillagerBuffer.base = start;
    EdVillagerBuffer.limit = remaining;
    EdVillagerBuffer.used = 0;
    printf("buffer %d\n", remaining);
    LoadChara();
    MGSetRenderInfo(EdInInfo->projection, 5.0f, 65535.0f);
    MGSetPLight(EdInInfo->light_direction, EdInInfo->light_colour);
    MGSetAmbient(EdInInfo->ambient);
    MGSetBGColor(EdInInfo->background_colour);
    EdIn_Chara = EdExchangeInfo.player;
    EdIn_camera_dist_mode = 1;

    if (EdIn_Chara != nullptr) {
        EdIn_Chara->SetPosition(0.0f, 0.0f, 0.0f);
    }

    EdFadeIn(0x40, 0.0f, 0.0f, 0.0f);
    EdIn_door_open_cnt = 0;
    GameMode = ED_IN_MODE_WALK;
    EdIn_Chara->wind = 0;
    EdIn_Chara->SetPosition(0.0f, 0.0f, 0.0f);
    EdIn_Chara->SetRotation(0.0f, 0.0f, 0.0f);
    GetMapJumpPos(EdIn_Chara);
    EdIn_Chara->ClothStep(-1);

    if (EdInteriorDoorSound >= 0) {
        sceVu0FVECTOR position;
        sceVu0FVECTOR reference = {0.0f, 0.0f, 10.0f, 0.0f};
        EdIn_Chara->GetPosition(position);
        position[2] -= 10.0f;
        SndSetCamera(position, reference);
        EdIn_Chara->GetPosition(position);
        EdDoorCloseSe(EdInteriorDoorSound, position);
    }

    EdMoveCharaInit();
    active_camera = -1;
    camera_change_count = 0;
    EdIn_MainCamera.FollowOff();
    EdIn_NowCamera = &EdIn_MainCamera;
    start_event_no = EdInteriorStartEvent;
    start_system_event = -1;
    EdIn_goto_menu = 0;
    EdIn_goto_return_menu = 0;
    EdIn_key_counter = 0;
    EdIn_loop_counter = 0;
    EdEventInfo.interior_parts_count = parts_num;
    EdEventInfo.interior_parts = InteriorParts;
    EdInitEventParam();
    InitWorkBuffer();
    printf("buffer %d\n", EdWorkBuffer.limit - EdWorkBuffer.used);
    EdSaveFrameImageInit();
    EdInitMenu(ED_MENU_RESET);
    EdInitSoundSrc();
    return 0;
}

/**
 * Runs one frame of the interior and reports when it is to be left.
 *
 * @mangled EditInLoop__Fv
 * @address 0x19C2B0
 * @size 0x1014
 */
PORT_OVERRIDE int EditInLoop() {
    sceVu0FMATRIX view;
    sceVu0FVECTOR position;

    if (EdSystemMesCheck() || EdCheckItemOver()) {
        EdSetKeyMode(0);
    } else {
        EdSetKeyMode(0xFFFF);
    }

    if (EdPadDown(0x800, 4)) {
        EdIn_goto_return_menu = 1;
    }

    if (GameMode != 14) {
        if (!EdCheckViewMode()) {
            MGSetRenderInfo(EdInInfo->projection, 5.0f, 65535.0f);
        } else {
            MGSetRenderInfo(600.0f, 4.0f, 65535.0f);
        }
    }

    EdIn_Chara->GetPosition(position);

    switch (GameMode) {
        case ED_IN_MODE_MENU_INIT:
        case ED_IN_MODE_MENU:
        case ED_IN_MODE_PAUSE:
            break;
        default:
            int            i;
            OBJ_ANIME_SEQ *anime = EdInInfo->obj_anime;

            for (i = 0; i < obj_anime_num; i++, anime++) {
                int completion_flag = anime->completion_flag;

                if (completion_flag <= 0 || EdGetMapFlag(completion_flag) == 0) {
                    ObjAnimePlay(anime);
                }
            }

            StepWater();
            break;
    }

    if (fix_camera != 0 && camera_num > 0 && GameMode == ED_IN_MODE_WALK) {
        SetCameraPos(static_cast<CFrame *>(InteriorParts[0].frame[0]), &EdIn_MainCamera, EdIn_Chara);
    }

    EdSetCharaCursor(0);

    switch (GameMode) {
        case ED_IN_MODE_WALK:
            if (EdIn_loop_counter > 0) {
                MoveCharacter();
            }

            EdSetCharaCursor(1);

            for (int i = 0; i < 10; i++) {
                EdVillager[i].Step();
                EdVillager[i].ShadowStep();
                EdVillager[i].ClothStep(0);
            }

            EdIn_NowCamera = &EdIn_MainCamera;

            if (EdCheckViewMode() != 0) {
                EdIn_NowCamera = &ViewCamera__2;
            }

            break;
        case ED_IN_MODE_EVENT: {
            EdIn_NowCamera = &EventCamera;
            EdIn_Chara->SetVelocity(CVector3_f_(0.0f, 0.0f, 0.0f));
            EdEventInfo.current_time = EdIn_NowTime;
            int result = EdEventMode(&EventCamera, 0);

            if (result != 0) {
                GameMode = ED_IN_MODE_WALK;
                EdIn_NowCamera = &EdIn_MainCamera;

                if (result == ED_EVENT_RETURN_OUTSIDE) {
                    if (MapNo == TOWN_MUSKA_LACKA && strcmp(EdInteriorName, "i04h04") == 0) {
                        EdInteriorDoorSound = -1;
                    }

                    return 1;
                }

                if (result == ED_EVENT_RETURN_MAP_JUMP) {
                    return 99;
                }

                if (result == ED_EVENT_RETURN_TALK) {
                    GameMode = ED_IN_MODE_TALK;
                }

                if (EdEventInfo.reset_camera_angle < 0) {
                    sceVu0FVECTOR event_pos;
                    sceVu0FVECTOR event_ref;
                    EventCamera.GetPos(event_pos);
                    EventCamera.GetRef(event_ref);
                    EdIn_MainCamera.SetPos(event_pos);
                    EdIn_MainCamera.SetRef(event_ref);
                }

                camera_change_count = 0;
            }

            VillagerCollision();
            EdEventNPCStep();
            EdIn_goto_return_menu = 0;
            break;
        }
        case ED_IN_MODE_DOOR_OPEN: {
            EdIn_NowCamera = &EdIn_MainCamera;
            CVector3_f_ velocity(0.0f, 0.0f, 0.0f);
            EdIn_Chara->SetVelocity(velocity);
            EdIn_Chara->Step();
            EdIn_Chara->ShadowStep();
            EdIn_Chara->SetPosition(EdIn_fix_chara_pos);
            EdIn_Chara->SetRotation(EdIn_fix_chara_rot[0], EdIn_fix_chara_rot[1], EdIn_fix_chara_rot[2]);
            EdIn_Chara->ClothStep(0);

            for (int i = 0; i < 10; i++) {
                EdVillager[i].Step();
                EdVillager[i].ShadowStep();
                EdVillager[i].ClothStep(0);
            }

            EdIn_door_open_cnt--;

            if (EdIn_door_open_cnt == 100 && EdInteriorDoorSound >= 0) {
                sceVu0FVECTOR chara_pos;
                EdIn_Chara->GetPosition(chara_pos);
                EdDoorOpenSe(EdInteriorDoorSound, chara_pos);
            }

            if (EdIn_door_open_cnt < 0) {
                EdIn_door_open_cnt = 0;
                GameMode = ED_IN_MODE_LEAVE;
            }

            break;
        }
        case ED_IN_MODE_LEAVE:
            EdIn_NowCamera = &EdIn_MainCamera;
            EdStopSoundSrc();

            if (EdEventInfo.map_jump_bgm_stop != 0) {
                SndBgmFadeOutStop();
            } else {
                EdBeforeInBgmNo = -1;
            }

            return 1;
        case ED_IN_MODE_TALK: {
            sceVu0FVECTOR direction;
            EdIn_NowCamera = &EdIn_MainCamera;

            if (EdCheckViewMode() != 0) {
                EdIn_NowCamera = &ViewCamera__2;
            }

            EdIn_Chara->SetMotion(0, 0);
            EdIn_Chara->SetVelocity(CVector3_f_(0.0f, 0.0f, 0.0f));
            EdIn_Chara->Step();
            EdIn_Chara->ShadowStep();
            EdIn_Chara->ClothStep(0);

            for (int i = 0; i < 10; i++) {
                EdVillager[i].Step();
                EdVillager[i].ShadowStep();
                EdVillager[i].ClothStep(0);
            }

            CNPCharacter *talker = EdNowTalkChara();
            float         distance = EdIn_Chara->GetDistance(*talker);
            EdIn_Chara->GetDir(*talker, direction);
            float pitch = -atan2f(-EdIn_Chara->body_height + (1.3f + talker->body_height + direction[1]), distance);
            float yaw = atan2f(direction[0], direction[2]);
            float view_h = EdAGetViewAngleH();
            float view_v = EdAGetViewAngleV();
            view_h = AngleInterpolate(view_h, yaw, 0.05f, INTERPOLATE_STEP);
            view_v = AngleInterpolate(view_v, pitch, 0.03f, INTERPOLATE_STEP);
            EdASetViewAngle(view_h, view_v);
            EdEyeCamera(&ViewCamera__2, EdIn_Chara);

            if (EdCheckViewMode() == 0 || ((view_h - yaw < 0.0f ? -(view_h - yaw) : view_h - yaw) < 0.001f && (view_v - pitch < 0.0f ? -(view_v - pitch) : view_v - pitch) < 0.001f)) {
                int event_no;
                EdASetViewAngle(yaw, pitch);
                event_no = 0;
                int result = EdTalkMode(EdIn_Chara, nullptr, EdCheckViewMode(), &event_no);

                if (result != 0) {
                    if (result == 2) {
                        EdIn_goto_menu = ED_MENU_SHOP;
                    }

                    if (result == 3) {
                        EdIn_goto_menu = ED_MENU_UNK_4;
                    }

                    if (result == 4 && event_no > 0) {
                        RunEvent(event_no, EdIn_NowCamera);
                    }

                    GameMode = ED_IN_MODE_WALK;
                }
            }

            break;
        }
        case ED_IN_MODE_MENU_INIT:
            EdIn_Chara->SetMotion(EdIn_Chara->motion_no, 1);
            EdIn_Chara->Step();
            EdIn_Chara->ShadowStep();

            for (int i = 0; i < 10; i++) {
                EdVillager[i].SetMotion(EdVillager[i].motion_no, 1);
                EdVillager[i].Step();
                EdVillager[i].ShadowStep();
            }

            break;
        case ED_IN_MODE_MENU:
            MenuMapJumpMode = -1;

            if (EdMenuMode() != 0) {
                GameMode = ED_IN_MODE_WALK;
                EdExitMenu();

                if (GetInteriorOutFlag() != 0) {
                    EdViewModeOff();
                    return 1;
                }

                for (int i = 0; i < 10; i++) {
                    EdVillager[i].SetMotion(EdVillager[i].motion_no, 0);
                    EdVillager[i].Step();
                    EdVillager[i].ShadowStep();
                }

                return 0;
            }

            break;
    }

    if (EdEventInfo.lighting_override != 0) {
        MGSetPLight(EdEventInfo.light_direction, EdEventInfo.light_color);
        MGSetAmbient(EdEventInfo.ambient_color);
    }

    EdIn_NowCamera->Step(1);
    sceVu0FVECTOR eye;
    sceVu0FVECTOR dir;
    const int     part_count = std::clamp(parts_num, 0, static_cast<int>(sizeof(InteriorParts) / sizeof(InteriorParts[0])));
    auto          parts = std::make_unique<CMapParts *[]>(static_cast<std::size_t>(part_count));
    EdIn_NowCamera->GetCameraMatrix(view);
    EdIn_NowCamera->GetPos(eye);
    EdIn_NowCamera->GetDir(dir);
    MGSetViewMatrix(view, eye);

    for (int i = 0; i < part_count; i++) {
        parts[i] = &InteriorParts[i];
    }

    EdSetSoundSrcVol(EdIn_NowTime, parts.get(), part_count, eye, dir);

    if (GameMode != ED_IN_MODE_MENU) {
        MainDraw();
    } else {
        EdFadeInOut();
    }

    if (GameMode == ED_IN_MODE_MENU_INIT && EdInitModeFinish(EdIn_NowCamera, TexManager.GetTexture("frame_image", -1)) != 0) {
        GameMode = ED_IN_MODE_MENU;
    }

    EdSaveFrameImageTask();

    if (GameMode == ED_IN_MODE_WALK && ((EdPadDown(0x10, 1) != 0 && EdIn_loop_counter >= 2) || EdIn_goto_menu != 0 || (SystemMesCheck() == 0 && EdCheckItemOver() != 0))) {
        int menu = ED_MENU_BATTLE;

        if (EdIn_goto_menu != 0) {
            menu = EdIn_goto_menu;
            EdIn_goto_menu = 0;
            printf("%d\n", menu);
        }

        if (EdInitMenu(menu) != 0) {
            SndSePlay(MENU_SOUND_CONFIRM, -1, 0);
            GameMode = ED_IN_MODE_MENU_INIT;
        }
    }

    static int event_text = 0;

    if (GamePad.Down2(PAD_SQUARE) != 0) {
        event_text = 4;
        EdEventAllClear();
        simple_event = 0;
    }

    event_text--;

    if (event_text < 0) {
        event_text = 0;
    }

    if (event_text == 1) {
        start_event_no = 150;
    }

    if (GameMode != ED_IN_MODE_EVENT && (static_cast<CMainChara *>(EdIn_Chara))->move_info.landed != 0) {
        int ground_event = (static_cast<CMainChara *>(EdIn_Chara))->move_info.ground_poly.attr.ground_kind;

        if (ground_event > 0) {
            start_event_no = ground_event;
        }
    }

    if (EdDebugEventEnable != 0 && (start_event_no > 0 || start_system_event > 0)) {
        if (start_system_event > 0) {
            if (EdEventInit(start_system_event, &EdNPCBuffer, EdSystemEventData) != 0) {
                EdInitMesParam();
                GameMode = ED_IN_MODE_EVENT;
            }
        } else if (EdEventInit(start_event_no, &EdNPCBuffer, EdEventData) != 0) {
            EdInitMesParam();
            GameMode = ED_IN_MODE_EVENT;
        } else if (EdEventInfo.return_code == ED_EVENT_RETURN_TALK) {
            EdInitMesParam();
            GameMode = ED_IN_MODE_TALK;
            simple_event = 0;
        }

        start_event_no = -1;
        start_system_event = -1;
    } else {
        simple_event = 0;
    }

    static int old_mode;

    if (GameMode == ED_IN_MODE_PAUSE && GamePad.Down(PAD_START) != 0) {
        GameMode = old_mode;
        EdSePlay(static_cast<ED_SOUND_ID>(2), -1);
        PlayTimeCountFlag(1);
    } else if (GameMode != ED_IN_MODE_EVENT && EdIn_goto_return_menu != 0 && EdIn_loop_counter > 1 && GameMode == ED_IN_MODE_WALK) {
        old_mode = GameMode;
        GameMode = ED_IN_MODE_PAUSE;
        EdSePlay(static_cast<ED_SOUND_ID>(1), -1);
        PlayTimeCountFlag(0);
    }

    EdIn_goto_return_menu = 0;
    static int end_count = 0;

    if (GamePad.AllOn() != 0 || DebugMode) {
        EdIn_key_counter = 0;
    }

    if (DebugMode && GamePad.On(PAD_SELECT) && GamePad.On(PAD_START) && end_count == 0) {
        float zero = 0.0f;

        end_count = 100;
        EdFadeOut(64, zero, zero, zero);
    }

    if (end_count == 1) {
        end_count = 0;
        MapJump(800, -1);
        return 99;
    }

    end_count--;

    if (end_count < 0) {
        end_count = 0;
    }

    EdIn_key_counter++;
    EdIn_loop_counter++;
    SndStep();
    return 0;
}

/**
 * Draws the interior for one frame.
 *
 * @mangled MainDraw__Fv__2
 * @address 0x19D2D0
 * @size 0x6AC
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
namespace {
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
    EdIn_Chara->GetPosition(position);

    if (EdDrawOffMap == 0) {
        TexManager.ReloadTexture(GetVif1Packet(), 15);
        TexAnime.TexAnime(15);
        setTexAnim();
        sceVu0FVECTOR lod_distance = {0.0f, 1000.0f, 10000.0f, 1000000.0f};

        for (i = 0; i < parts_num; i++) {
            InteriorParts[i].DrawParts(EdIn_NowTime, lod_distance, 0, 0, nullptr);
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
    water_tex = *reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("water_buff", -1)->tex0);
    MGMoveImage(&frame_tex, screen, &water_tex, 0, 0, 0);
    sceVu0FVECTOR ref;
    EdIn_MainCamera.GetRef(ref);
    sceGsZbuf zbuf = mgZBuffer;
    zbuf.bits.zmsk = 1;
    MGSetGsZBUF(&zbuf);
    DrawWaterSurface(EdIn_NowCamera);
    MGSetGsZBUF(&mgZBuffer);

    if (GameMode == ED_IN_MODE_EVENT) {
        EdEventBackSpriteDraw();
    }

    if (GameMode == ED_IN_MODE_EVENT) {
        EdDrawItem();
    }

    ED_EVENT_INFO *event = nullptr;
    int            detail = 3;
    auto           marks_store = std::make_unique<int[]>(10);
    int           *marks = marks_store.get();

    if (EdCheckViewMode() != 0) {
        detail = 0;
    }

    if (GameMode == ED_IN_MODE_LEAVE) {
        marks = nullptr;
    } else {
        for (i = 0; i < 10; i++) {
            marks_store[i] = 3;
        }
    }

    if (GameMode == ED_IN_MODE_EVENT) {
        event = &EdEventInfo;
        detail = 3;
    }

    EdDrawCharacter(EdIn_Chara, detail, 10, EdVillager, marks, 1, event);
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
            if (CheckEditEffect(effect, EdIn_NowTime) != 0) {
                DrawEditEffect(effect, EdIn_NowCamera, &EdEffectGroup);
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

    if (static_cast<unsigned int>(GameMode - 3) < 2U) {
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

            if (GamePad.Down(PAD_R3 | PAD_CIRCLE) || EdDebugRunEventNo > 0) {
                debug_menu_mode = 0;
                InputSetMenuNavigation(false);
                GamePad.AutoRepeatOff();
                RunEvent(EdDebugRunEventNo, EdIn_NowCamera);
            } else {
                GamePad.KeyLock(1);
            }
        } else {
            if (GameMode != ED_IN_MODE_EVENT && GamePad.Down(PAD_R3)) {
                GamePad.SetAutoRepeat(PAD_DPAD, 25, 3);
                GamePad.SetAutoRepeat(PAD_L1 | PAD_R1, 25, 3);
                debug_menu_mode = 1;
                InputSetMenuNavigation(true);
                debug_flag = 0;
            }

            EdDDebug(debug_flag);
            EdDPrintChara(static_cast<CMainChara *>(EdIn_Chara));
            EdDPrintCamera(EdIn_NowCamera);
            EdDDrawFont();
        }
    }

    if (EdDebugParamDrawOff == 0) {
        char      pause_texture[] = "pause";
        CTexture *pause = TexManager.GetTexture(pause_texture, -1);

        if (pause != nullptr && (GameMode == ED_IN_MODE_PAUSE || EdPauseFlag != 0)) {
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
            set2DSprite(packet, pause, place, texel, 0x80);
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
} // namespace

PORT_OVERRIDE void DrawWaterSurface(CCamera *camera) {
    sceVu0FVECTOR eye;
    sceVu0FVECTOR position;
    sceVu0FVECTOR dir;
    int           i;
    CGroundWater *surface = Water;

    camera->GetPos(eye);
    camera->GetDir(dir);
    dir[1] = 0.0f;
    sceVu0Normalize(dir, dir);

    for (i = 0; i < 1; i++, surface++) {
        if (surface->draw == 0) {
            continue;
        }

        CWater *water = &surface->water;
        sceVu0CopyVector(position, surface->offset);

        if (surface->follow[0]) {
            position[0] = eye[0] + 50.0f * dir[0];
        }

        if (surface->follow[1]) {
            position[1] = eye[1];
        }

        if (surface->follow[2]) {
            position[2] = eye[2] + 50.0f * dir[2];
        }

        CVector3_f_ rotation;
        rotation.x = rotation.y = rotation.z = 0.0f;
        water->frame.SetRotation(rotation.x, rotation.y, rotation.z);
        water->frame.SetPosition(position);
        DrawVu1__6CWaterFP10RenderInfoP13sceVif1PacketP1(water, &mgRenderInfo, GetVif1Packet(), nullptr);
    }
}

/**
 * Advances the ripples of the interior's water surfaces.
 *
 * @mangled StepWater__Fv
 * @address 0x19DAE0
 * @size 0x204
 */
namespace {
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
                row = rows * static_cast<float>(rand()) / 2.1474836e9f;
            }

            if (column < 0) {
                int rows = surface->water.rows;
                column = rows * static_cast<float>(rand()) / 2.1474836e9f;
            }

            surface->water.Shake(row, column, surface->ripples[j].power + surface->ripples[j].range * static_cast<float>(rand()) / 2.1474836e9f);
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

    EdMoveCharaInfo.time = EdIn_NowTime;
    EdMoveCharaInfo.camera = &EdIn_MainCamera;
    EdMoveCharaInfo.view_camera = &ViewCamera__2;
    EdMoveCharaInfo.key_lock = false;
    EdMoveCharaInfo.chara = EdIn_Chara;
    EdMoveCharaInfo.interior = true;
    EdMoveCharaInfo.parts = InteriorParts;
    EdMoveCharaInfo.parts_count = parts_num;
    EdMoveCharaInfo.points = EdInInfo->event_points;
    EdMoveCharaInfo.point_count = 32;
    EdMoveCharaInfo.acted = false;
    EdMoveChara();

    if (EdDebugCameraFlag != 0 && GamePad.Down(PAD_CIRCLE) != 0) {
        fix_camera = !fix_camera;
        EdIn_MainCamera.GetPos(fix_pos);
    }

    if (fix_camera != 0) {
        EdIn_MainCamera.FollowOff();
    } else {
        EdIn_MainCamera.FollowOn();
        EdIn_Chara->GetPosition(follow);
        EdIn_MainCamera.SetFollow(follow[0], 14.0f + follow[1], follow[2]);
        MoveCamera(&EdIn_MainCamera);
    }

    if (EdMoveCharaInfo.system_event_no > 0) {
        RunSystemEvent(EdMoveCharaInfo.system_event_no, &EdIn_MainCamera);
        return;
    }

    if (EdMoveCharaInfo.event_no > 0) {
        RunEvent(EdMoveCharaInfo.event_no, &EdIn_MainCamera);
        return;
    }

    if (EdMoveCharaInfo.acted == 0 && EdPadDown(0x40, 1) != 0) {
        sceVu0FVECTOR position;
        sceVu0FVECTOR rotation = {0.0f, 0.0f, 0.0f, 0.0f};
        EdIn_Chara->GetPosition(position);
        EdIn_Chara->GetRotation(rotation);
        EPARTS_FUNC_DATA *jump;

        jump = nullptr;

        if ((DebugMode && GamePad.Down(PAD_SELECT)) || (jump = SearchMapJump(position, rotation))) {
            int motion;

            EdMoveCharaInit();
            motion = 0;
            EdIn_door_open_cnt = 140;

            if (jump != nullptr) {
                GetDoorPos(jump->link_id, position, rotation, &EdInteriorDoorSound, &motion);
            }

            EdIn_Chara->SetMotion(motion, 6);
            sceVu0CopyVector(EdIn_fix_chara_pos, position);
            sceVu0CopyVector(EdIn_fix_chara_rot, rotation);
            EdIn_Chara->SetPosition(EdIn_fix_chara_pos);
            EdIn_Chara->SetRotation(EdIn_fix_chara_rot[0], EdIn_fix_chara_rot[1], EdIn_fix_chara_rot[2]);
            EdIn_Chara->Step();
            EdIn_Chara->ClothStep(-1);
            GameMode = ED_IN_MODE_DOOR_OPEN;
            EdFadeOut(100, 0.0f, 0.0f, 0.0f);
        }
    }
}

/**
 * Applies the right stick and the mouse to the interior camera, holding its height and distance in
 * range.
 *
 * @mangled MoveCamera__FP13CCameraFollow__2
 * @address 0x19E080
 * @size 0x164
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void MoveCamera(CCameraFollow *camera) {
    static float camera_distance[3] = {20.0f, 60.0f, 100.0f};

    float horizontal = MouseLookTurn(camera, 0.04f, GamePad.GetRXf());
    camera->AddHeight(-MouseLookRise(camera, GamePad.GetRYf(), 30.0f));

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

    camera->SetDistance(camera_distance[EdIn_camera_dist_mode]);

    if (GamePad.Down(PAD_TRIANGLE) != 0) {
        EdIn_camera_dist_mode++;
    }

    if (EdIn_camera_dist_mode >= 3) {
        EdIn_camera_dist_mode = 0;
    }
}

/**
 * Finds the map jump the player is standing on.
 *
 * @mangled SearchMapJump__FPfPf
 * @address 0x19E1F0
 * @size 0xF4
 */
} // namespace

PORT_OVERRIDE EPARTS_FUNC_DATA *SearchMapJump(float *position, float *rotation) {
    sceVu0FVECTOR     world;
    int               i;
    EPARTS_FUNC_DATA *point = func_point;

    for (i = 0; i < func_num; i++, point++) {
        if (point->kind == EPARTS_FUNC_DOOR) {
            CFrame *frame = reinterpret_cast<CFrame *>(point->parts);
            point->position[3] = 1.0f;
            frame->GetWorldPosition(world, point->position);
            EdInteriorJumpID = point->link_id;

            if (DistVector(position, world) < 10.0f && AngleCmp(rotation[1], point->rotation[1], 0.87f) == 0) {
                return point;
            }
        }
    }

    return nullptr;
}

/**
 * Puts the player where the interior's map jump says they arrive.
 *
 * @mangled GetMapJumpPos__FP10CCharacter
 * @address 0x19E2F0
 * @size 0x12C
 */
PORT_OVERRIDE void GetMapJumpPos(CCharacter *chara) {
    EPARTS_FUNC_DATA *point = func_point;

    for (int i = 0; i < func_num; i++, point++) {
        if (point->kind == EPARTS_FUNC_DOOR && point->link_id == EdInteriorJumpID) {
            chara->SetPosition(point->position[0], point->position[1], point->position[2]);
            printf("%f %f %f\n", point->position[0], point->position[1], point->position[2]);
            chara->SetRotation(0.0f, AngleLimit(PI_SHORT + point->rotation[1]), 0.0f);
            return;
        }
    }
}

/**
 * Gives the position, heading and part of one of the interior's doors.
 *
 * @mangled GetDoorPos__FiPfPfPiPi
 * @address 0x19E420
 * @size 0xF8
 */
namespace {
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
} // namespace

PORT_OVERRIDE void SetCameraPos(CFrame *frame, CCamera *camera, CCharacter *chara) {
    sceVu0FVECTOR     position;
    sceVu0FVECTOR     local;
    int               i;
    EPARTS_FUNC_DATA *point = func_point;
    int               count;
    INTERIOR_CAMERA  *entry;

    chara->GetPosition(position);
    const int camera_capacity = std::min(std::max(func_num, 0), 8);
    if (camera_capacity == 0) {
        return;
    }
    std::array<INTERIOR_CAMERA, 8> cameras;
    sceVu0FMATRIX                  matrix;
    count = 0;

    for (i = 0; i < func_num; i++, point++) {
        if (point->kind == EPARTS_FUNC_CAMERA) {
            entry = &cameras[count];
            entry->link_id = point->link_id;
            CFrame *owner = reinterpret_cast<CFrame *>(point->parts);
            sceVu0CopyVector(entry->min, point->parameters);
            sceVu0CopyVector(entry->max, point->values);
            sceVu0UnitMatrix(matrix);

            if (owner != nullptr) {
                CFrame *found = owner->SearchFrame(point->frame_name);

                if (found != nullptr) {
                    found->GetLWMatrix(matrix);
                }
            }

            sceVu0MulMatrix(matrix, matrix, point->matrix);
            entry->frame.SetTransMatrix(matrix);
            count++;

            if (count >= camera_capacity) {
                break;
            }
        }
    }

    if (count <= 0) {
        return;
    }

    // Retail kept the active camera as a pointer into this list from one frame to the next, which only
    // held because its list sat on the stack at the same address every frame; it is kept by its place.
    int        inside = -1;
    static int cnt = 0;

    for (i = 0; i < count; i++) {
        entry = &cameras[i];

        if (entry->link_id == 0) {
            inside = i;
            continue;
        }

        float (*inverse)[4] = entry->frame.GetInverseMatrix();
        position[3] = 1.0f;
        sceVu0ApplyMatrix(local, inverse, position);

        if (!(local[0] < entry->min[0]) && !(local[1] < entry->min[1]) && !(local[2] < entry->min[2]) && local[0] <= entry->max[0] && local[1] <= entry->max[1] && local[2] <= entry->max[2]) {
            inside = i;
            break;
        }
    }

    cnt++;

    if (cnt > 60) {
        cnt = 0;
    }

    if (active_camera < 0 || active_camera >= count) {
        active_camera = inside;
    }

    if (inside < 0) {
        return;
    }

    if (camera_change_count <= 0) {
        active_camera = inside;
        camera_change_count = 0;
    }

    if (inside == active_camera) {
        camera_change_count = 20;
    } else {
        camera_change_count--;
    }

    if (active_camera >= 0) {
        sceVu0FVECTOR eye = {0.0f, 0.0f, 0.0f, 1.0f};
        cameras[active_camera].frame.GetWorldPosition(eye, eye);
        position[1] += 17.0f;
        camera->SetPos(eye);
        camera->SetRef(position);
        camera->SetNextPos(nullptr, eye[0], eye[1], eye[2]);
        camera->SetNextRef(nullptr, position[0], position[1], position[2]);
    }
}

/**
 * Collects the collision polygons of the interior parts meeting a box.
 *
 * @mangled GetCollision__FP6CCPolyP7CBoxVu0__2
 * @address 0x19E920
 * @size 0xBC
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
namespace {
static int GetCollision(CCPoly *poly, CBoxVu0 *box) {
    int count = 0;

    for (int i = 0; i < parts_num; i++) {
        CFrame *frame = InteriorParts[i].GetCollisionFrame();

        if (frame != nullptr) {
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
        CNPCharacter *villager = nullptr;
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
            EdIn_Chara->GetPosition(position);
            foot_sound = EdEventInfo.player_foot_sound;
        }

        WorkBuffer__2->used = 0;
        CCPoly *polys = reinterpret_cast<CCPoly *>(WorkBuffer__2->Alloc(2000));
        box.max[0] = 1.0f + position[0];
        box.min[0] = position[0] - 1.0f;
        box.max[2] = 1.0f + position[2];
        box.min[2] = position[2] - 1.0f;
        box.max[1] = 1000.0f;
        box.min[1] = -1000.0f;

        if (i >= 0) {
            chara = villager;
        } else {
            chara = EdIn_Chara;
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
                EdIn_Chara->SetPosition(position);
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
    auto texdata = std::make_unique<LOADTEXTURE_INFO2[]>(2);
    texdata[0].name = nullptr;
    texdata[0].block_no = 15;
    texdata[1].name = nullptr;

    BG_READ_INFO *file = GetReadBGFile(2);

    if (file == nullptr) {
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

    u_int *cfg_data = nullptr;
    int    size;
    TexAnime.Initialize(nullptr, 0);

    if (strcmp(ext, "img") == 0) {
        texdata[0].name = reinterpret_cast<char *>(file->buffer);
    } else {
        u_int *found;
        int    cfg_size;

        if (GetPackFileExt(reinterpret_cast<u_int *>(file->buffer), "img", &found, 1, nullptr, nullptr) > 0) {
            texdata[0].name = reinterpret_cast<char *>(found);
        }

        if (GetPackFileExt(reinterpret_cast<u_int *>(file->buffer), "cfg", &found, 1, &cfg_size, nullptr) > 0) {
            TexAnime.Initialize(EdIn_TexAnimeData, 64);
            cfg_data = found;
            size = cfg_size;

            for (int i = 0; i < 64; i++) {
                EdIn_TexAnimeData[i].Initialize();
            }
        }
    }

    TexManager.LoadTextureBlockEX(15, texdata.get());

    if (cfg_data != nullptr) {
        TexAnime.LoadCFGFile(reinterpret_cast<char *>(cfg_data), size);
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
} // namespace

PORT_OVERRIDE void LoadData() {
    func_point = new (reinterpret_cast<u_long128 *>(EdNPCBuffer.Alloc(0x600))) EPARTS_FUNC_DATA[128];

    for (int i = 0; i < 128; i++) {
        func_point[i].kind = 0;
    }

    func_num = 0;
    BG_READ_INFO *file = GetReadBGFile(0);
    u_int        *archive = nullptr;

    if (file != nullptr) {
        archive = reinterpret_cast<u_int *>(file->buffer);
    }

    int count;

    for (count = 0;; count++) {
        u_int *part_archive = SearchPTS(archive, count);

        if (part_archive == nullptr) {
            break;
        }

        CMapParts *parts = &InteriorParts[count];
        LoadPTS(parts, part_archive);
        parts->handle = count;
        parts->category_no = 15;
        int first_point = func_num;
        func_num += GetFuncPoint(count, part_archive, &func_point[first_point]);

        if (EdInteriorPartsNo >= 0) {
            EDITPARTS_INFO *info = EditPartsInfo.GetPartsInfo(EdInteriorPartsNo);

            if (info != nullptr) {
                EdPartsObjectOnOff(parts, info, EdInteriorJumpID + 1);
            }
        }

        CFrame *frames[9];

        for (int i = 0; i < 9; i++) {
            frames[i] = nullptr;
        }

        for (int i = 0; i < 4; i++) {
            CFrame *frame = parts->frame[i];
            frames[i] = frame;
        }

        frames[4] = parts->shadow_frame;
        frames[5] = parts->GetCollisionFrame();
        frames[6] = parts->shade_frame;
        frames[7] = parts->ripple_frame;
        CFrame *extra_collision;

        if (parts->camera_frame == nullptr) {
            extra_collision = nullptr;
        } else {
            parts->camera_frame->SetPosition(parts->pos[0], parts->pos[1], parts->pos[2]);
            parts->camera_frame->SetRotation(parts->rotation.x, parts->rotation.y, parts->rotation.z);
            extra_collision = parts->camera_frame;
        }

        frames[8] = extra_collision;
        EPARTS_FUNC_DATA *point = &func_point[first_point];
        effect_num = 32;

        for (int i = 0; i < 24; i++) {
            parts->effect_on[i] = 0;
            parts->effect[i] = nullptr;
        }

        for (; first_point < func_num; first_point++, point++) {
            EnterPartsEffect(parts, point, EdInInfo->effects, 32);

            if (obj_anime_num < 32) {
                OBJ_ANIME_SEQ *seq = &EdInInfo->obj_anime[obj_anime_num];

                if (InitObjAnime(frames, 9, point, seq) != 0) {
                    obj_anime_num++;
                }
            }
        }

        EdInitEventPoint(parts, nullptr, func_point, func_num, EdInInfo->event_points, 32);
    }

    parts_num = count;

    for (int i = 0; i < func_num; i++) {
        func_point[i].parts = reinterpret_cast<CMapParts *>(
            InteriorParts[static_cast<int>(reinterpret_cast<std::intptr_t>(func_point[i].parts))].frame[0]);

        if (func_point[i].completion_flag > 0 && SaveData->GetMapInitFlag(MapNo, func_point[i].completion_flag) == 0) {
            SaveData->SetMapInitFlag(MapNo, func_point[i].completion_flag, 1);
            SaveData->SetMapFlag(MapNo, func_point[i].completion_flag, !static_cast<s8>(func_point[i].unk_28[0]));
        }
    }

    camera_num = 0;
    EPARTS_FUNC_DATA *point = func_point;

    for (int i = 0; i < func_num; i++, point++) {
        if (point->kind == EPARTS_FUNC_CAMERA) {
            camera_num++;
        }
    }

    for (int i = 0; i < 4; i++) {
        EDIT_MOTION_PARTS_INFO *motion = &EdInInfo->motion_parts[i];
        MotionParts__2[i].Initialize();

        if (motion->name[0] != '\0') {
            LoadFile(motion->name, read_buffer, nullptr);
            MotionParts__2[i].LoadPackData2(read_buffer, "info.cfg", &EdNPCBuffer, i + 0x32, &EdNPCBuffer, 0);
            MotionParts__2[i].SetPosition(motion->values[0], motion->values[1], motion->values[2]);
            MotionParts__2[i].SetRotation(motion->values[3], motion->values[4], motion->values[5]);
            MotionParts__2[i].SetScale(motion->values[6], motion->values[7], motion->values[8]);
        }
    }

    for (int i = 0; i < 1; i++) {
        Water[i].draw = false;
    }

    for (int i = 0; i < 1; i++) {
        EDIT_WATER_INFO *info = &EdInInfo->water_surfaces[i];

        if (info->grid_rows <= 0) {
            break;
        }

        CWater       *water;
        int           j;
        CGroundWater *surface = &Water[i];
        water = &surface->water;
        sceVu0FVECTOR near_left = {info->corner_a[0], info->corner_a[1], info->corner_a[2], 1.0f};
        sceVu0FVECTOR near_right = {info->corner_b[0], info->corner_a[1], info->corner_a[2], 1.0f};
        sceVu0FVECTOR far_left = {info->corner_a[0], info->corner_a[1], info->corner_b[2], 1.0f};
        sceVu0FVECTOR far_right = {info->corner_b[0], info->corner_a[1], info->corner_b[2], 1.0f};

        surface->draw = true;
        std::memcpy(surface->name, info->name, sizeof(info->name));
        surface->parts_no = info->parts_no;
        sceVu0CopyVector(surface->offset, info->corner_c);

        for (int j = 0; j < 3; j++) {
            surface->follow[j] = info->follow[j];
        }

        for (j = 0; j < 4; j++) {
            sceVu0CopyVector(&surface->ripples[j].row, &info->wave[j].row);
        }

        water->SetVertex(near_left, near_right, far_left, far_right);
        water->frame.SetPosition(info->corner_c);
        water->SetSize(info->grid_rows, info->grid_columns, &EdNPCBuffer);
        water->SetParam(info->ripple_params[0], info->ripple_params[1], info->ripple_params[2], info->ripple_params[3]);
        water->SetColor(info->red, info->green, info->blue, 0x80);
    }
}

/** Frame of the interior's animated texture. */
namespace {
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
    if (setTexAnimCntf >= 7.0f) {
        setTexAnimCntf = 0.0f;
    } else {
        setTexAnimCntf += 0.2f;
    }

    setTexAnimCnt = static_cast<int>(setTexAnimCntf);

    CTexture *strip = TexManager.GetTexture("i01e01_a", -1);
    CTexture *plate = TexManager.GetTexture("i01e01", -1);

    if (strip == nullptr || plate == nullptr) {
        return;
    }

    MGMoveImage(reinterpret_cast<sceGsTex0 *>(&strip->tex0), CRect_i_(0, setTexAnimCnt * 64, 64, 64), reinterpret_cast<sceGsTex0 *>(&plate->tex0), 0, 0, 0);
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
static std::string CurrentDir;

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
    CurrentDir.clear();
    npc_count = 0;
    objanime_list = 0;
    effect_list = 0;
    motion_parts_list = 0;
    water_list = 0;
    water_info = nullptr;
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
    EdInInfo->ambient[0] = *static_cast<float *>(arguments[0]);
    EdInInfo->ambient[1] = *static_cast<float *>(arguments[1]);
    EdInInfo->ambient[2] = *static_cast<float *>(arguments[2]);
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
    int           light = *static_cast<int *>(arguments[6]);

    direction[0] = *static_cast<float *>(arguments[0]);
    direction[1] = *static_cast<float *>(arguments[1]);
    direction[2] = *static_cast<float *>(arguments[2]);
    direction[3] = 0.0f;
    sceVu0Normalize(direction, direction);
    light--;
    EdInInfo->light_direction[0][light] = direction[0];
    EdInInfo->light_direction[1][light] = direction[1];
    EdInInfo->light_direction[2][light] = direction[2];
    EdInInfo->light_direction[3][light] = direction[3];
    EdInInfo->light_colour[light][0] = *static_cast<float *>(arguments[3]);
    EdInInfo->light_colour[light][1] = *static_cast<float *>(arguments[4]);
    EdInInfo->light_colour[light][2] = *static_cast<float *>(arguments[5]);
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
    EdInInfo->fog.near_distance = *static_cast<float *>(arguments[0]);
    EdInInfo->fog.far_distance = *static_cast<float *>(arguments[1]);
    EdInInfo->fog.red = *static_cast<int *>(arguments[2]);
    EdInInfo->fog.green = *static_cast<int *>(arguments[3]);
    EdInInfo->fog.blue = *static_cast<int *>(arguments[4]);
    EdInInfo->fog.intensity = *static_cast<float *>(arguments[5]);
    EdInInfo->fog.exponent = *static_cast<float *>(arguments[6]);
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
    EdInInfo->background_colour[0] = *static_cast<float *>(arguments[0]);
    EdInInfo->background_colour[1] = *static_cast<float *>(arguments[1]);
    EdInInfo->background_colour[2] = *static_cast<float *>(arguments[2]);
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
    EdInInfo->projection = *static_cast<float *>(arguments[0]);
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
    CurrentDir = std::string(EditDataDir) + "in/" + static_cast<const char *>(arguments[0]);
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

    position[0] = *static_cast<float *>(arguments[1]);
    position[1] = *static_cast<float *>(arguments[2]);
    position[2] = *static_cast<float *>(arguments[3]);
    position[3] = 1.0f;
    scale[0] = *static_cast<float *>(arguments[4]);
    scale[1] = *static_cast<float *>(arguments[4]);
    scale[2] = *static_cast<float *>(arguments[4]);
    SetEffect(EFFECT_FIRE, static_cast<char *>(arguments[0]), position, scale, rotation);
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

    position[0] = *static_cast<float *>(arguments[1]);
    position[1] = *static_cast<float *>(arguments[2]);
    position[2] = *static_cast<float *>(arguments[3]);
    position[3] = 1.0f;
    scale[0] = *static_cast<float *>(arguments[4]);
    scale[1] = *static_cast<float *>(arguments[4]);
    scale[2] = *static_cast<float *>(arguments[4]);
    SetEffect(EFFECT_FLAME, static_cast<char *>(arguments[0]), position, scale, rotation);
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

    position[0] = *static_cast<float *>(arguments[1]);
    position[1] = *static_cast<float *>(arguments[2]);
    position[2] = *static_cast<float *>(arguments[3]);
    position[3] = 1.0f;
    scale[0] = *static_cast<float *>(arguments[4]);
    scale[1] = *static_cast<float *>(arguments[4]);
    scale[2] = *static_cast<float *>(arguments[4]);
    SetEffect(EFFECT_BRIGHT, static_cast<char *>(arguments[0]), position, scale, rotation);
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
        const std::string name = CurrentDir + static_cast<const char *>(arguments[0]);

        if (name.size() >= sizeof(motion->name)) {
            exit__2(-1);
        }

        std::memcpy(motion->name, name.c_str(), name.size() + 1);
        motion->values[0] = *static_cast<float *>(arguments[1]);
        motion->values[1] = *static_cast<float *>(arguments[2]);
        motion->values[2] = *static_cast<float *>(arguments[3]);
        motion->values[3] = *static_cast<float *>(arguments[4]);
        motion->values[4] = *static_cast<float *>(arguments[5]);
        motion->values[5] = *static_cast<float *>(arguments[6]);
        motion->values[6] = *static_cast<float *>(arguments[7]);
        motion->values[7] = *static_cast<float *>(arguments[8]);
        motion->values[8] = *static_cast<float *>(arguments[9]);
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
        std::strncpy(surface->name, static_cast<char *>(arguments[0]), sizeof(surface->name) - 1);
        surface->name[sizeof(surface->name) - 1] = '\0';
        surface->grid_rows = *static_cast<int *>(arguments[1]);
        surface->grid_columns = *static_cast<int *>(arguments[2]);
        surface->corner_a[0] = *static_cast<float *>(arguments[3]);
        surface->corner_a[1] = *static_cast<float *>(arguments[4]);
        surface->corner_a[2] = *static_cast<float *>(arguments[5]);
        surface->corner_a[3] = 1.0f;
        surface->corner_b[0] = *static_cast<float *>(arguments[6]);
        surface->corner_b[1] = *static_cast<float *>(arguments[7]);
        surface->corner_b[2] = *static_cast<float *>(arguments[8]);
        surface->corner_b[3] = 1.0f;
        surface->corner_c[0] = *static_cast<float *>(arguments[9]);
        surface->corner_c[1] = *static_cast<float *>(arguments[10]);
        surface->corner_c[2] = *static_cast<float *>(arguments[11]);
        surface->corner_c[3] = 1.0f;
        surface->ripple_params[0] = *static_cast<float *>(arguments[12]);
        surface->ripple_params[1] = *static_cast<float *>(arguments[13]);
        surface->ripple_params[2] = *static_cast<float *>(arguments[14]);
        surface->ripple_params[3] = *static_cast<float *>(arguments[15]);
        surface->red = *static_cast<int *>(arguments[16]);
        surface->green = *static_cast<int *>(arguments[17]);
        surface->blue = *static_cast<int *>(arguments[18]);
        surface->follow[0] = *static_cast<int *>(arguments[19]);
        surface->follow[1] = *static_cast<int *>(arguments[20]);
        surface->follow[2] = *static_cast<int *>(arguments[21]);
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

    if (info != nullptr) {
        for (int i = 0; i < 4; i++) {
            if (info->wave[i].power == 0.0f && info->wave[i].range == 0.0f) {
                EDIT_WATER_WAVE_VIEW *wave = reinterpret_cast<EDIT_WATER_WAVE_VIEW *>(&(reinterpret_cast<EDIT_WATER_WAVE_INFO *>(info))[i]);
                wave->row = static_cast<float>(*static_cast<int *>(arguments[0]));
                wave->column = static_cast<float>(*static_cast<int *>(arguments[1]));
                wave->range = *static_cast<float *>(arguments[3]);
                wave->power = *static_cast<float *>(arguments[2]);
                return;
            }
        }
    }
}
} // namespace

#include "common.h"

#include <cstdint>

#include "eparts_port.hpp"
#include "objanime.hpp"

// Retail's GetFuncPoint copies each interior part's function records with the host's EPARTS_FUNC_DATA
// stride and words; the disc's are the PS2's (eparts_port.hpp). parts carries the part's index, which
// LoadData turns into its frame.
PORT_OVERRIDE int GetFuncPoint(int parts_no, u_int *archive, EPARTS_FUNC_DATA *points) {
    const char      *definition = reinterpret_cast<char *>(archive) + archive[1];
    EPartsDiscHeader header = EPartsReadHeader(definition);
    int              i;

    if (header.func_count < 0 || header.func_count > 128 - func_num) {
        return 0;
    }

    for (i = 0; i < header.func_count; points++) {
        EPartsReadFunc(definition, i, points);
        points->parts = reinterpret_cast<CMapParts *>(static_cast<std::intptr_t>(parts_no));
        i++;
    }

    return header.func_count;
}
