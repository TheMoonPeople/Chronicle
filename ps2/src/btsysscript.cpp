#include "btsysscript.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "btactstatus.hpp"
#include "btitem.hpp"
#include "btmisc.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "clsmes.hpp"
#include "collisiondata.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dngstatusdata.hpp"
#include "dun/gameloop.hpp"
#include "dungeoneventman.hpp"
#include "dungeonmap.hpp"
#include "dungeonparts.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "frame.hpp"
#include "itembombeffect.hpp"
#ifdef PAL
#include "mainselect.hpp"
#endif
#include "mathutil.hpp"
#include "menu_dungeon.hpp"
#include "menu_misc.hpp"
#include "monstorunit.hpp"
#include "nowload.hpp"
#include "npcharacter.hpp"
#include "runscript.hpp"
#include "savedata.hpp"
#include "shot_freefuncs.hpp"
#include "snd.hpp"
#include "userstatus.hpp"

BT_EVENT_INFO BtEventInfo;
BT_OBJ_HANDLE BtObjHdl[32];
char          BtLoadMapFileName[32];
int           BtMapJumpFloor;
s32           BtLoadMapType;

void BtSystemScriptEventInfoInit() {
    BtEventInfo.script_no = -1;
    BtEventInfo.running_script_no = -1;
    BtEventInfo.init_flag_28 = 1;
    BtEventInfo.ext_memory = false;
    BtEventInfo.init_flag_20 = 1;
    BtEventInfo.item_select_result = 0;
    BtEventInfo.entrance_result = NULL;
    BtEventInfo.escape_result = NULL;
    BtEventInfo.request = BT_REQUEST_NONE;
    BtEventInfo.chained_script_no = -1;
    BtEventInfo.clear_script_no = -1;
    BtEventInfo.clear_script_ext_memory = 0;
    BtEventInfo.script_main_chr = -1;
    BtEventInfo.floor_title_off = 0;
    BtEventInfo.bee_npc = -1;
    BtEventInfo.fade_on_start = true;
    BtEventInfo.no_status_recover = 0;
    EdEventInfo.projection = -1.0f;
    EdEventInfo.main_character = &CharaMain;

    for (int i = 0; i < 6; i++) {
        NPCUnit[i].frame = NULL;
    }

    EdEventInfo.npcs = NPCUnit;
    EdEventInfo.npc_texture_block = 32;
    EdEventInfo.player_texture_block = 17;
    EdEventInfo.npc_count = 6;
    EdEventInfo.villagers = NULL;

    for (int i = 0; i < 8; i++) {
        EdEventInfo.messages[i] = NULL;
    }

    EdEventInfo.messages[1] = &BtEventMes0;
    EdEventInfo.messages[2] = &BtEventMes1;
    EdEventAllClear();
}

BT_OBJ_HANDLE *GetObjHDL(int index) {
    if (index < 0 || index >= 32) {
        printf("** obj hdl err **\n");
        return NULL;
    }

    return &BtObjHdl[index];
}

void BtSystemScriptLoad(int floor) {
#ifdef PAL
    char  path[32];
    char  mes_path[40];
    int   read_size;
    int   mes_size;
    char *mes;

    sprintf(path, "dun/script/d0%d/event.stb", floor + 1);
    sprintf(mes_path, "dun/script/d0%d/d0%d_%d.mes", floor + 1, floor + 1, LanguageCode);
    BtSystemScriptFileBuffer.used = 0;
    BtEventData = (s32) (BtSystemScriptFileBuffer.base + BtSystemScriptFileBuffer.used * 0x10);
    LoadFile(path, (void *) BtEventData, &read_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((read_size >> 4) + 1);
    mes = (char *) (BtSystemScriptFileBuffer.base + BtSystemScriptFileBuffer.used * 0x10);
    LoadFile(mes_path, mes, &mes_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((mes_size >> 4) + 1);
    EdSetEventScript((char *) BtEventData, mes, &BtSystemScriptFileBuffer);
    AddSystemEventScript();
#else
    char path[44];
    int  read_size;

    sprintf(path, "dun/script/d0%d/event.stb", floor + 1);
    BtSystemScriptFileBuffer.used = 0;
    BtEventData = (s32) (BtSystemScriptFileBuffer.base + BtSystemScriptFileBuffer.used * 0x10);
    LoadFile(path, (void *) BtEventData, &read_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((read_size >> 4) + 1);
    EdSetEventScript((char *) BtEventData, NULL, &BtSystemScriptFileBuffer);
    AddSystemEventScript();
#endif
}

void BtSystemScriptInit() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR ref;
    float         text_rate;

    BtEventMode = 1;
    BtAllClear = 1;
    NowCamera__3->GetPos(pos);
    NowCamera__3->GetRef(ref);
    SubCamera.FollowOff();
    SubCamera.SetPos(pos);
    SubCamera.SetRef(ref);
    SubCamera.Step(-1);
    NowCamera__3 = &SubCamera;
    NowCamera__3->FollowOff();
    CUserStatus *status = UserStatus;
    status->step_disable = 1;
    EdEventInfo.projection = -1.0f;
    EdEventInfo.camera = NowCamera__3;
    EdEventInfo.main_character = &CharaMain;

    for (int i = 0; i < 6; i++) {
        NPCUnit[i].frame = NULL;
    }

    EdEventInfo.npcs = NPCUnit;
    EdEventInfo.npc_texture_block = 32;
    EdEventInfo.player_texture_block = 17;
    EdEventInfo.npc_count = 6;
    EdEventInfo.villagers = NULL;

    for (int i = 0; i < 8; i++) {
        EdEventInfo.messages[i] = NULL;
    }

    EdEventInfo.messages[1] = &BtEventMes0;
    EdEventInfo.messages[2] = &BtEventMes1;
    EDIT_CONFIG_VIEW *config = (EDIT_CONFIG_VIEW *) SaveData->GetConfigData();

    if (config->message_speed != 0) {
        text_rate = 0.6f;
    } else {
        text_rate = 0.3f;
    }

    EdEventInfo.messages[1]->text_rate = text_rate;
    EdEventInfo.messages[2]->text_rate = text_rate;
    EdEventInfo.messages[1]->text_rate_set = text_rate;
    EdEventInfo.messages[2]->text_rate_set = text_rate;

    if (BtEventInfo.fade_on_start != 0) {
        EdFadeInit();
    }

    BtEventInfo.fade_on_start = true;

    for (int i = 0; i < 32; i++) {
        BtObjHdl[i].frame = NULL;
        BtObjHdl[i].character = NULL;
    }

    BtEventInfo.event_marker = false;
    BtEventInfo.floor_title_off = 0;
    BtEventInfo.no_reset_chara = 0;
    BtActStatus.motion_no = 0;
    BtActStatus.main_motion = -1;
    BtActStatus.hand_motion = -1;
    BtActStatus.action_no = 0;
    BtActStatus.action_on = 0;
    BtActStatus.unk_028 = 0;
}

void BtSystemScriptAfter() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR ref;

    BtEventMode = 0;
    BtAllClear = 0;
    BtEventInfo.running_script_no = -1;
    SetMIniMapStatus(1);
    CharaMain.SetMotion(0, 0);
    BtActStatus.motion_no = 0;
    BtActStatus.main_motion = -1;
    BtActStatus.hand_motion = -1;
    BtActStatus.player_visible = 1;
    BtActStatus.weapon_visible = 1;
    BtActStatus.shadow_visible = true;
    EdEventInfo.player_shadow_draw = true;
    EdEventInfo.player_draw = true;
    EdEventAllClear();
    UserStatus->step_disable = 0;
    BtMapJumpFloor = -1;

    if (EdEventInfo.reset_camera_angle > 0) {
        sceVu0CopyVector(pos, CharaMain.pos);
        CharaMain.GetRotation(ref);
        float angle = ref[1] + EdEventInfo.reset_camera_yaw;

        if (!(angle <= PI_SHORT)) {
            angle -= TWO_PI;
        }

        MainCamera__4.SetAngleSoon(angle);
        MainCamera__4.SetFollow(pos[0], pos[1] + 16.0f - 3.0f, pos[2]);
        MainCamera__4.Step(-1);
    }

    if (EdEventInfo.reset_camera_angle < 0) {
        SubCamera.GetPos(pos);
        SubCamera.GetRef(ref);
        MainCamera__4.FollowOff();
        MainCamera__4.SetPos(pos);
        MainCamera__4.SetRef(ref);
        MainCamera__4.Step(-1);
        MainCamera__4.Step(1);
        MainCamera__4.FollowOn();
        MainCamera__4.SetHeight(pos[1] - ref[1]);
        MainCamera__4.SetFollow(ref[0], ref[1], ref[2]);
        MainCamera__4.SetAngleSoon(atan2(pos[0] - ref[0], pos[2] - ref[2]));
        MainCamera__4.Step(-1);
        MainCamera__4.Step(1);
    }

    NowCamera__3 = &MainCamera__4;
}

int BtSystemScriptRun(int event, CDataAlloc2<1> *arena) {
    return EdEventInit(event, arena, (char *) BtEventData);
}

void BtSetMapJumpFloor(int floor) {
    BtMapJumpFloor = floor;
}

/**
 * Reads one script argument as an integer, converting it where the slot holds a float.
 *
 * @mangled GetStackInt__FP12RS_STACKDATA__2
 * @address 0x1BB930
 * @size 0x44
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static int GetStackInt(RS_STACKDATA *argument) {
    if (argument->type == RS_FLOAT) {
        return (int) argument->f;
    }

    return argument->i;
}

/**
 * Reads one script argument as a float, converting it where the slot holds an integer.
 *
 * @mangled GetStackFloat__FP12RS_STACKDATA__2
 * @address 0x1BB980
 * @size 0x2C
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static float GetStackFloat(RS_STACKDATA *argument) {
    if (argument->type == RS_INT) {
        return (float) argument->i;
    }

    return argument->f;
}

/**
 * Reads one script argument as a string.
 *
 * @mangled GetStackString__FP12RS_STACKDATA__2
 * @address 0x1BB9B0
 * @size 0xC
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static char *GetStackString(RS_STACKDATA *argument) {
    return argument->s;
}

/**
 * Writes an integer back through a script argument that names a variable.
 *
 * @mangled SetStack__FP12RS_STACKDATAi__2
 * @address 0x1BB9C0
 * @size 0x20
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void SetStack(RS_STACKDATA *argument, int value) {
    if (argument->type == RS_PTR) {
        argument->p->i = value;
    }
}

/**
 * Writes a float back through a script argument that names a variable.
 *
 * @mangled SetStack__FP12RS_STACKDATAf__2
 * @address 0x1BB9E0
 * @size 0x20
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static void SetStack(RS_STACKDATA *argument, float value) {
    if (argument->type == RS_PTR) {
        argument->p->f = value;
    }
}

int _GET_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, UserStatus->cur_floor);
    return 1;
}

int _SET_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count) {
    ((CDngStatusData *) UserStatus)->SetNowFloor(GetStackInt(stack));
    return 1;
}

int _GET_OLD_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, UserStatus->prev_floor);
    return 1;
}

int _GET_ACTION_MODE(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, BtEventInfo.action_mode);
    return 1;
}

int _ITEM_USE_WINDOW(RS_STACKDATA *stack, int argument_count) {
    if (stack->type != RS_PTR) {
        return 0;
    }

    BtEventInfo.item_select_result = (s32) stack->p;
    stack++;
    int i;

    for (i = 0; i < argument_count - 1; i++) {
        BtEventInfo.item_select_list[i] = GetStackInt(stack++);
    }

    BtEventInfo.item_select_filtered = false;

    if (argument_count > 1) {
        BtEventInfo.item_select_filtered = true;
    }

    BtEventInfo.item_select_list[i] = -1;
    BtEventInfo.request = BT_REQUEST_ITEM_WINDOW;
    return 1;
}

int _CHECK_EVENT_FLG(RS_STACKDATA *stack, int argument_count) {
    int flag_no = GetStackInt(stack++);
    int flag = UserStatus->ChkEventFlag(flag_no);
    SetStack(stack, flag);
    return 1;
}

int _SET_EVENT_FLG(RS_STACKDATA *stack, int argument_count) {
    int          flag_no = GetStackInt(stack++);
    int          value = GetStackInt(stack);
    CUserStatus *status = UserStatus;

    if (flag_no >= 0 && flag_no < 50) {
        status->event_flags[flag_no] = value;
    }

    return 1;
}

int _GET_OBJHDL(RS_STACKDATA *stack, int argument_count) {
    int            index = GetStackInt(stack++);
    int            parts_no = GetStackInt(stack++);
    char          *name = GetStackString(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (parts_no != -1) {
        CDungeonMap *map = NowDngMap;
        CFrame      *frame = map->parts[parts_no].GetSearchFrame(name);

        if (frame != NULL) {
            handle->frame = frame;
            handle->type = 0;
        } else {
            printf("******** Frame NotFount %s *******\n", name);
            handle->frame = NULL;
            handle->type = 0;
        }
    } else {
        CFrame *frame = NowDngMap->GetFrameSearch(name);

        if (frame != NULL) {
            handle->frame = frame;
            handle->type = 0;
        } else {
            printf("******** All Frame NotFount %s *******\n", name);
            handle->frame = NULL;
            handle->type = 0;
        }
    }

    return 1;
}

int _SET_OBJHDL_POS(RS_STACKDATA *stack, int argument_count) {
    int            index = GetStackInt(stack++);
    float          x = GetStackFloat(stack++);
    float          y = GetStackFloat(stack++);
    float          z = GetStackFloat(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (handle->type == 0 && handle->frame != NULL) {
        handle->frame->SetPosition(x, y, z);
    }

    if (handle->type == 1 && handle->character != NULL) {
        handle->character->SetPosition(x, y, z);
    }

    return 1;
}

int _SET_OBJHDL_ROT(RS_STACKDATA *stack, int argument_count) {
    int            index = GetStackInt(stack++);
    float          x = GetStackFloat(stack++);
    float          y = GetStackFloat(stack++);
    float          z = GetStackFloat(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != NULL) {
            frame->SetRotType(2);
            frame->SetRotation(AngleLimit(x), AngleLimit(y), AngleLimit(z));
        }
    }

    if (handle->type == 1) {
        CCharacter *chara = handle->character;

        if (chara != NULL) {
            chara->SetRotation(AngleLimit(x), AngleLimit(y), AngleLimit(z));
        }
    }

    return 1;
}

int _SET_OBJHDL_DRAW_FLAG(RS_STACKDATA *stack, int argument_count) {
    int            index = GetStackInt(stack++);
    int            draw = GetStackInt(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != NULL) {
            int draw_on = 2;

            if (draw) {
                draw_on = 1;
            }

            frame->attr.draw_on = draw_on;
            frame->flags = draw_on;
        }
    }

    if (handle->type == 1) {
        CCharacter *chara = handle->character;

        if (chara != NULL) {
            CFrame *frame = chara->frame;

            if (frame != NULL) {
                int draw_on = 2;

                if (draw) {
                    draw_on = 1;
                }

                frame->attr.draw_on = draw_on;
            }
        }
    }

    return 1;
}

int _GET_OBJHDL_POS(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR  local = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR  pos;
    BT_OBJ_HANDLE *handle = GetObjHDL(GetStackInt(stack++));

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != NULL) {
            frame->GetWorldPosition(pos, local);
        }
    }

    if (handle->type == 1) {
        CCharacter *chara = handle->character;

        if (chara != NULL) {
            chara->GetPosition(pos);
        }
    }

    SetStack(stack++, pos[0]);
    SetStack(stack++, pos[1]);
    SetStack(stack, pos[2]);
    return 1;
}

int _GET_OBJHDL_ROT(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR  rot = {0.0f, 0.0f, 0.0f, 1.0f};
    sceVu0FMATRIX  matrix;
    BT_OBJ_HANDLE *handle = GetObjHDL(GetStackInt(stack++));

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != NULL) {
            frame->GetLWMatrix(matrix);
            rot[0] = 0.0f;
            rot[1] = atan2f(matrix[2][0], matrix[2][2]);
            rot[2] = 0.0f;
            rot[3] = 1.0f;
        }
    }

    if (handle->type == 1) {
        CCharacter *chara = handle->character;

        if (chara != NULL) {
            chara->GetRotation(rot);
        }
    }

    SetStack(stack++, rot[0]);
    SetStack(stack++, rot[1]);
    SetStack(stack, rot[2]);
    return 1;
}

int _SET_URA_DUNGEON(RS_STACKDATA *stack, int argument_count) {
    BtEventInfo.request = BT_REQUEST_URA_DUNGEON;
    return 1;
}

int _GET_EVENT_POS(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack++, BtEventInfo.position[0]);
    SetStack(stack++, BtEventInfo.position[1]);
    SetStack(stack, BtEventInfo.position[2]);
    return 1;
}

int _GET_EVENT_ROT(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack++, BtEventInfo.direction[0]);
    SetStack(stack++, BtEventInfo.direction[1]);
    SetStack(stack, BtEventInfo.direction[2]);
    return 1;
}

int _OPEN_ENTRANCE_WINDOW(RS_STACKDATA *stack, int argument_count) {
    if (stack->type != RS_PTR) {
        return 0;
    }

    BtEventInfo.entrance_result = stack->p;
    BtEventInfo.request = BT_REQUEST_ENTRANCE_WINDOW;
    return 1;
}

int _OPEN_ESCAPE_WINDOW(RS_STACKDATA *stack, int argument_count) {
    if (stack->type != RS_PTR) {
        return 0;
    }

    BtEventInfo.escape_result = stack->p;
    BtBattleMusic_Stop();
    BtEventInfo.request = BT_REQUEST_ESCAPE_WINDOW;
    return 1;
}

int _GO_DUNGEON(RS_STACKDATA *stack, int argument_count) {
    BtEventInfo.request = BT_REQUEST_GO_DUNGEON;
    return 1;
}

int _SET_DUNGEON_MAP(RS_STACKDATA *stack, int argument_count) {
    char *map_file = GetStackString(stack++);
    BtLoadMapType = GetStackInt(stack);
    strcpy(BtLoadMapFileName, map_file);
    MainDungeonMap.map_type = BtLoadMapType;
    return 1;
}

int _LOAD_DUNGEON_MAP2(RS_STACKDATA *stack, int argument_count) {
    CUserStatus *status = UserStatus;
    status->res_limit_zone_current = RES_LIMIT_ZONE_NONE;
    BtMapJumpLoad(BtLoadMapFileName);
    return 1;
}

int _LOAD_MONSTOR(RS_STACKDATA *stack, int argument_count) {
    BtLoadMonstor(0);
    BtArrengeMonstor();
    return 1;
}

int _SET_RANDOM_MAP(RS_STACKDATA *stack, int argument_count) {
    if (NowDngMap->map_type == 1) {
        BtCleatRandomMap();
    } else {
        BtCleatFreeMap();
    }

    return 1;
}

int _SET_EVENT_SW(RS_STACKDATA *stack, int argument_count) {
    int script_no = GetStackInt(stack++);
    NowEventMan->SearchDataSwitch(script_no, GetStackInt(stack));
    return 1;
}

int _SET_MONSTOR_ID(RS_STACKDATA *stack, int argument_count) {
    int   model_no = GetStackInt(stack++);
    int   event_flag = -1;
    float pos[3];

    if (argument_count == 2) {
        if (GetStackInt(stack++)) {
            BtLoadMonstor(0);
            NowMonstorUnit->CleanViewMonstor(0);
        }
    }

    if (argument_count == 5) {
        int clean = GetStackInt(stack++);
        pos[0] = GetStackFloat(stack++);
        pos[1] = GetStackFloat(stack++);
        pos[2] = GetStackFloat(stack++);

        if (clean) {
            BtLoadMonstor(0);
            NowMonstorUnit->CleanViewMonstor(0);
        }
    }

    if (argument_count == 6) {
        int clean = GetStackInt(stack++);
        pos[0] = GetStackFloat(stack++);
        pos[1] = GetStackFloat(stack++);
        pos[2] = GetStackFloat(stack++);
        event_flag = GetStackInt(stack);

        if (clean) {
            BtLoadMonstor(0);
            NowMonstorUnit->CleanViewMonstor(0);
        }
    }

    NowMonstorUnit->SetupViewMonstor(model_no, pos, event_flag);
    return 1;
}

int _CHK_ATRA_HAVE(RS_STACKDATA *stack, int argument_count) {
    int atra_no = GetStackInt(stack++);
    int have = GetAtraTipNowHave(atra_no, UserStatus->cur_georama);

    printf("%d,%d --> %d\n", atra_no, UserStatus->cur_georama, have);
    SetStack(stack, have);
    return 1;
}

int _SET_ATRA(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR pos;

    pos[0] = GetStackFloat(stack++);
    pos[1] = GetStackFloat(stack++);
    pos[2] = GetStackFloat(stack++);
    pos[3] = 1.0f;
    NowDngMap->SetAtraBoll(pos, GetStackInt(stack));
    return 1;
}

int _SET_IBOX(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR pos;

    pos[0] = GetStackFloat(stack++);
    pos[1] = GetStackFloat(stack++);
    pos[2] = GetStackFloat(stack++);
    pos[3] = 1.0f;
    NowDngMap->SetTreasureBox(pos, GetStackInt(stack), TREASURE_BOX_SMALL, 0);
    return 1;
}

int _GET_NOW_USER_ID(RS_STACKDATA *stack, int argument_count) {
    int cur_chara = UserStatus->cur_chara;
    printf("get id = %d\n", cur_chara);
    SetStack(stack, cur_chara);
    return 1;
}

int _RUN_SCRIPT_NO(RS_STACKDATA *stack, int argument_count) {
    int script_no = GetStackInt(stack++);

    BtEventInfo.ext_memory = false;

    if (argument_count == 2) {
        BtEventInfo.ext_memory = GetStackInt(stack++);
    }

    if (argument_count == 3) {
        BtEventInfo.ext_memory = GetStackInt(stack++);
        BtEventInfo.fade_on_start = GetStackInt(stack);
    }

    BtEventInfo.request = BT_REQUEST_RUN_SCRIPT;
    BtEventInfo.chained_script_no = script_no;
    return 1;
}

int _CLEAN_MONSTOR_SCRIPT_NO(RS_STACKDATA *stack, int argument_count) {
    int script_no = GetStackInt(stack++);

    BtEventInfo.clear_script_ext_memory = 0;

    if (argument_count > 1) {
        BtEventInfo.clear_script_ext_memory = GetStackInt(stack);
    }

    BtEventInfo.clear_script_no = script_no;
    return 1;
}

int _GET_NPC_OBJHDL(RS_STACKDATA *stack, int argument_count) {
    int            index = GetStackInt(stack++);
    int            npc_no = GetStackInt(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (npc_no < 0 || npc_no > 3) {
        printf("over err\n");
        return 1;
    }

    CCharacter *chara = &NowDngMap->npc[npc_no].chara;

    if (chara->frame != NULL) {
        handle->character = chara;
        handle->type = 1;
    } else {
        handle->character = NULL;
        handle->type = 1;
    }

    return 1;
}

int _SET_MOTION_OBJHDL(RS_STACKDATA *stack, int argument_count) {
    int npc_no = GetStackInt(stack++);
    int motion_no = GetStackInt(stack++);
    int speed = -1;
    int mode = 0;

    if (argument_count > 2) {
        speed = GetStackFloat(stack++);
    }

    if (argument_count > 3) {
        mode = GetStackInt(stack);
    }

    if (NowDngMap->npc[npc_no].chara.frame == NULL) {
        return 1;
    }

    if (argument_count < 3) {
        NowDngMap->NPCSetMotion(npc_no, motion_no);
    } else {
        NowDngMap->NPCSetMotion(npc_no, motion_no, speed, mode);
    }

    return 1;
}

int _SET_NPC_ON_OFF(RS_STACKDATA *stack, int argument_count) {
    int npc_no = GetStackInt(stack++);
    int enable = GetStackInt(stack);

    if (NowDngMap->npc[npc_no].chara.frame == NULL) {
        return 1;
    }

    NowDngMap->npc[npc_no].visible = enable;
    return 1;
}

int _GET_GATEKEY_NO(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, NowDngMap->gate_key_no);
    return 1;
}

int _USER_WEAPON_DRAW(RS_STACKDATA *stack, int argument_count) {
    if (argument_count != 1) {
        return 1;
    }

    BtActStatus.weapon_visible = GetStackInt(stack);
    return 1;
}

int _SET_MAIN_CHR2(RS_STACKDATA *stack, int argument_count) {
    int chara = GetStackInt(stack++);
    int reload = 0;

    if (argument_count == 2) {
        reload = GetStackInt(stack);
    }

    if (chara < 0 || chara > 5) {
        return 1;
    }

    selectChrUnit(chara, reload);
    BtEventInfo.script_main_chr = chara;
    return 1;
}

int _RESET_MAIN_CHR(RS_STACKDATA *stack, int argument_count) {
    if (BtEventInfo.script_main_chr == -1) {
        return 1;
    }

    selectChrUnit(BtEventInfo.script_main_chr, 0);
    BtEventInfo.script_main_chr = -1;
    // Past the early exit the result is whatever selectChrUnit left behind.
    PORT_RETURN(1);
}

int _SET_LIMMIT_ZONE(RS_STACKDATA *stack, int argument_count) {
    UserStatus->res_limit_zone_current = GetStackInt(stack);
    SndSPSeLoad(0x1B);
    return 1;
}

int _SET_DEAD_FLAG(RS_STACKDATA *stack, int argument_count) {
    ((CDngStatusData *) UserStatus)->SetDead();
    return 1;
}

int _ALL_DRAW_FLAG(RS_STACKDATA *stack, int argument_count) {
    if (argument_count == 1) {
        BtAllDrawFlag = GetStackInt(stack);
    }

    return 1;
}

int _SET_FLOOR_TITLE(RS_STACKDATA *stack, int argument_count) {
    FloorTitleOn();
    return 1;
}

int _GET_RUBY_ELEMENT(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, BtRubyDoorKey);
    return 1;
}

int _SET_RUBY_ELEMENT(RS_STACKDATA *stack, int argument_count) {
    GetStackInt(stack);
    return 1;
}

int _SET_FLOOR_TITLE_OFF(RS_STACKDATA *stack, int argument_count) {
    BtEventInfo.floor_title_off = 1;
    return 1;
}

int _SET_RES_LIMMIT_ZONE(RS_STACKDATA *stack, int argument_count) {
    ((CDngStatusData *) UserStatus)->SetResLimmitZone();

    if (UserStatus->res_limit_zone_current != RES_LIMIT_ZONE_NONE) {
        SndSPSeLoad(0x1B);
    }

    return 1;
}

int _CLEAR_DEAMON_SHAFT(RS_STACKDATA *stack, int argument_count) {
    ((CDngStatusData *) UserStatus)->ClearDeamonShaft();
    return 1;
}

int _INIT_BEE(RS_STACKDATA *stack, int argument_count) {
    BtEventInfo.bee_npc = GetStackInt(stack);
    InitBee(NPCUnit[BtEventInfo.bee_npc].frame, 15);
    return 1;
}

int _END_BEE(RS_STACKDATA *stack, int argument_count) {
    BtEventInfo.bee_npc = -1;
    return 1;
}

int _EASTKING_COMPLETE(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, EastKingCheckComplete());
    return 1;
}

int _GET_ITEM_TRAPID(RS_STACKDATA *stack, int argument_count) {
    int box = BtEventInfo.treasure_box;
    SetStack(stack, NowDngMap->boxes[box].trap_no);
    return 1;
}

int _RESET_ITEM_TRAP(RS_STACKDATA *stack, int argument_count) {
    int box = BtEventInfo.treasure_box;
    NowDngMap->boxes[box].trap_no = 0;
    BtEventInfo.treasure_box = 0;
    return 1;
}

int _BOM_SET(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR push = {0.0f, 0.0f, 1.0f, 1.0f};
    sceVu0FVECTOR direction;

    sceVu0CopyVector(pos, CharaMain.pos);
    pos[1] += 16.0f;
    pos[2] -= 3.0f;
    int bomb = SetBombEffect(pos, 1, selectMapNo * 20 + 20, 0.8f);

    if (bomb != -1) {
        CCollisionData *collision = NowColData;
        sceVu0Normalize(direction, push);
        sceVu0ScaleVectorXYZ(collision->hit[bomb].velocity, direction, 0.8f);
    }

    BtActStatus.event_block_frames = 30;
    return 1;
}

int _SET_STATUS_ERR(RS_STACKDATA *stack, int argument_count) {
    sceVu0FVECTOR pos;

    BtSetStatusErr(GetStackInt(stack));
    sceVu0CopyVector(pos, CharaMain.pos);
    pos[1] += 16.0f;
    NowColData->Set(pos, 2, 2, 5.0f, 0.0f, 1, 2, 0, 0);
    return 1;
}

int _CHECK_MARDAN(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, SaveData->GetMardanGareyanFlag());
    return 1;
}

int _NO_RESET_CHARA_NO(RS_STACKDATA *stack, int argument_count) {
    BtEventInfo.no_reset_chara = 1;
    return 1;
}

int _CHECK_CHR_HELP(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, BtEventInfo.chara_help);
    return 1;
}

int _HOLD_ITEM_EVENT(RS_STACKDATA *stack, int argument_count) {
    int script_no = GetStackInt(stack);
    DngEventMan.SearchItemEventHold(script_no);
    UraEventMan.SearchItemEventHold(script_no);
    return 1;
}

int _STOP_BATTLE_BGM(RS_STACKDATA *stack, int argument_count) {
    BtBattleMusic_Stop();
    return 1;
}

int _NO_STATUS_RECOVER(RS_STACKDATA *stack, int argument_count) {
    BtEventInfo.no_status_recover = 1;
    return 1;
}

int _SET_QUEST_DUNGEON(RS_STACKDATA *stack, int argument_count) {
    int dungeon_no = GetStackInt(stack++);
    SaveData->QuestDungeon(dungeon_no, GetStackInt(stack));
    return 1;
}

int _GET_MAP_CODE(RS_STACKDATA *stack, int argument_count) {
    SetStack(stack, UserStatus->cur_georama + 1);
    return 1;
}

int _SET_ACTIVE_ITEM_ICON(RS_STACKDATA *stack, int argument_count) {
    LoadActiveItemIcon();
    return 1;
}

int _GET_ITEM_UNIT_NO(RS_STACKDATA *stack, int argument_count) {
    int item_no = GetStackInt(stack++);
    int owner = -1;
    int not_weapon = item_no < ITEM_WEAPON_START;

    if (!not_weapon) {
        if (!not_weapon && item_no < ITEM_WEAPON_WOODENSLINGSHOT_BROKEN) {
            owner = 0;
        }

        if (item_no >= ITEM_WEAPON_WOODENSLINGSHOT_BROKEN && item_no < ITEM_WEAPON_MALLET_BROKEN) {
            owner = 1;
        }

        if (item_no >= ITEM_WEAPON_MALLET_BROKEN && item_no < ITEM_WEAPON_GOLD_RING_BROKEN) {
            owner = 2;
        }

        if (item_no >= ITEM_WEAPON_GOLD_RING_BROKEN && item_no < ITEM_WEAPON_FIGHTING_STICK_BROKEN) {
            owner = 3;
        }

        if (item_no >= ITEM_WEAPON_FIGHTING_STICK_BROKEN && item_no < ITEM_WEAPON_MACHINE_GUN_BROKEN) {
            owner = 4;
        }

        if (item_no >= ITEM_WEAPON_MACHINE_GUN_BROKEN) {
            owner = 5;
        }
    }

    SetStack(stack, owner);
    return 1;
}

int _SET_IBOX_ANGLE(RS_STACKDATA *stack, int argument_count) {
    NowDngMap->boxes[BtEventInfo.treasure_box].lid_angle = GetStackFloat(stack);
    return 1;
}

int _SET_IBOX_FINISH(RS_STACKDATA *stack, int argument_count) {
    NowDngMap->boxes[BtEventInfo.treasure_box].lid_angle = -30.0f;
    NowDngMap->boxes[BtEventInfo.treasure_box].closed = 0;
    return 1;
}

int _GET_PIERO_ITEM(RS_STACKDATA *stack, int argument_count) {
    int item0, item1;

    GetPieroItem(selectMapNo, BtUraDongeon, &item0, &item1);
    SetStack(stack++, item0);
    SetStack(stack, item1);
    return 1;
}

/** Pairs one system script opcode function with its bytecode operation number. */
struct BT_EVENT_EXTERNAL_FUNCTION {
    int (*function)(RS_STACKDATA *, int); /**< Native function invoked by the bytecode operation. */
    int operation;                        /**< Bytecode operation number assigned to the function. */
};

/** Opcode functions of the system script, ended by an entry without a function. */
static BT_EVENT_EXTERNAL_FUNCTION ext_func_info[] = {
    {_GET_FLOOR_LEVEL,         1010},
    {_SET_FLOOR_LEVEL,         1011},
    {_GET_OLD_FLOOR_LEVEL,     1012},
    {_GET_ACTION_MODE,         1020},
    {_ITEM_USE_WINDOW,         1021},
    {_CHECK_EVENT_FLG,         1023},
    {_SET_EVENT_FLG,           1024},
    {_GET_OBJHDL,              1030},
    {_SET_OBJHDL_POS,          1031},
    {_SET_OBJHDL_ROT,          1032},
    {_SET_OBJHDL_DRAW_FLAG,    1033},
    {_GET_OBJHDL_POS,          1034},
    {_GET_OBJHDL_ROT,          1035},
    {_SET_URA_DUNGEON,         1036},
    {_GET_EVENT_POS,           1040},
    {_GET_EVENT_ROT,           1041},
    {_OPEN_ENTRANCE_WINDOW,    1022},
    {_OPEN_ESCAPE_WINDOW,      1070},
    {_GO_DUNGEON,              1050},
    {_SET_DUNGEON_MAP,         1051},
    {_LOAD_DUNGEON_MAP2,       1052},
    {_LOAD_MONSTOR,            1053},
    {_SET_RANDOM_MAP,          1054},
    {_SET_EVENT_SW,            1055},
    {_SET_MONSTOR_ID,          1056},
    {_CHK_ATRA_HAVE,           1057},
    {_SET_ATRA,                1058},
    {_SET_IBOX,                1059},
    {_GET_NOW_USER_ID,         1060},
    {_RUN_SCRIPT_NO,           1061},
    {_CLEAN_MONSTOR_SCRIPT_NO, 1066},
    {_GET_NPC_OBJHDL,          1062},
    {_SET_MOTION_OBJHDL,       1063},
    {_SET_NPC_ON_OFF,          1094},
    {_GET_GATEKEY_NO,          1064},
    {_USER_WEAPON_DRAW,        1065},
    {_SET_MAIN_CHR2,           1067},
    {_RESET_MAIN_CHR,          1071},
    {_SET_LIMMIT_ZONE,         1068},
    {_SET_DEAD_FLAG,           1069},
    {_ALL_DRAW_FLAG,           1072},
    {_SET_FLOOR_TITLE,         1073},
    {_GET_RUBY_ELEMENT,        1074},
    {_SET_RUBY_ELEMENT,        1088},
    {_SET_FLOOR_TITLE_OFF,     1076},
    {_INIT_BEE,                1077},
    {_END_BEE,                 1078},
    {_EASTKING_COMPLETE,       1079},
    {_SET_RES_LIMMIT_ZONE,     1080},
    {_GET_ITEM_TRAPID,         1081},
    {_RESET_ITEM_TRAP,         1082},
    {_BOM_SET,                 1083},
    {_SET_STATUS_ERR,          1084},
    {_CHECK_MARDAN,            1086},
    {_NO_RESET_CHARA_NO,       1087},
    {_CHECK_CHR_HELP,          1089},
    {_HOLD_ITEM_EVENT,         1090},
    {_STOP_BATTLE_BGM,         1091},
    {_NO_STATUS_RECOVER,       1092},
    {_SET_QUEST_DUNGEON,       1093},
    {_GET_MAP_CODE,            1095},
    {_SET_ACTIVE_ITEM_ICON,    1096},
    {_GET_ITEM_UNIT_NO,        1097},
    {_SET_IBOX_ANGLE,          1098},
    {_SET_IBOX_FINISH,         1099},
    {_GET_PIERO_ITEM,          1100},
    {_CLEAR_DEAMON_SHAFT,      1101},
    {NULL,                     -1  },
};

void AddSystemEventScript() {
    for (int i = 0;; i++) {
        if (ext_func_info[i].function == NULL) {
            break;
        }

        int j;

        for (j = 0; j < i; j++) {
            if (ext_func_info[i].operation == ext_func_info[j].operation) {
                printf("same ext_func_no!!!\n");

                while (1) {
                }
            }
        }

        int operation = ext_func_info[i].operation;

        if (operation < 0 || operation >= 1500) {
            printf("ext func over!!");
        } else {
            ext_func__2[operation] = ext_func_info[i].function;
        }
    }
}
