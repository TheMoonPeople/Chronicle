#include "btsysscript.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>

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
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editpartsinfo.hpp"
#include "frame.hpp"
#include "itembombeffect.hpp"
#include "mainselect.hpp"
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

namespace dc::game {

BT_EVENT_INFO                 BtEventInfo;
std::array<BT_OBJ_HANDLE, 32> BtObjHdl;
std::array<char, 32>          BtLoadMapFileName;
int                           BtMapJumpFloor;
s32                           BtLoadMapType;

void BtSystemScriptEventInfoInit() {
    BtEventInfo.script_no = -1;
    BtEventInfo.running_script_no = -1;
    BtEventInfo.init_flag_28 = 1;
    BtEventInfo.ext_memory = false;
    BtEventInfo.init_flag_20 = 1;
    BtEventInfo.item_select_result = 0;
    BtEventInfo.entrance_result = nullptr;
    BtEventInfo.escape_result = nullptr;
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
        NPCUnit[i].frame = nullptr;
    }

    EdEventInfo.npcs = NPCUnit;
    EdEventInfo.npc_texture_block = 32;
    EdEventInfo.player_texture_block = 17;
    EdEventInfo.npc_count = 6;
    EdEventInfo.villagers = nullptr;

    std::ranges::fill(EdEventInfo.messages, nullptr);

    EdEventInfo.messages[1] = &BtEventMes0;
    EdEventInfo.messages[2] = &BtEventMes1;
    EdEventAllClear();
}

BT_OBJ_HANDLE *GetObjHDL(int index) {
    if (index < 0 || std::cmp_greater_equal(index, BtObjHdl.size())) {
        std::println("** obj hdl err **");
        return nullptr;
    }

    return &BtObjHdl[index];
}

namespace {

// Held as a full pointer rather than in the s32 BtEventData, which cannot store a host address.
char *g_event_data = nullptr;

char *ScriptBufferTail() {
    return reinterpret_cast<char *>(BtSystemScriptFileBuffer.base + (static_cast<std::ptrdiff_t>(BtSystemScriptFileBuffer.used) * 0x10));
}

} // namespace

void BtSystemScriptLoad(int floor) {
    std::string path = std::format("dun/script/d0{}/event.stb", floor + 1);
    std::string mes_path = std::format("dun/script/d0{}/d0{}_{}.mes", floor + 1, floor + 1, LanguageCode);
    int         read_size = 0;
    int         mes_size = 0;

    BtSystemScriptFileBuffer.used = 0;
    g_event_data = ScriptBufferTail();
    LoadFile(path.data(), g_event_data, &read_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((read_size >> 4) + 1);
    char *mes = ScriptBufferTail();
    LoadFile(mes_path.data(), mes, &mes_size);
    wait_now_loading_vsync();
    BtSystemScriptFileBuffer.Alloc((mes_size >> 4) + 1);
    EdSetEventScript(g_event_data, mes, &BtSystemScriptFileBuffer);
    AddSystemEventScript();
}

void BtSystemScriptInit() {
    sceVu0FVECTOR pos;
    sceVu0FVECTOR ref;

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
        NPCUnit[i].frame = nullptr;
    }

    EdEventInfo.npcs = NPCUnit;
    EdEventInfo.npc_texture_block = 32;
    EdEventInfo.player_texture_block = 17;
    EdEventInfo.npc_count = 6;
    EdEventInfo.villagers = nullptr;

    std::ranges::fill(EdEventInfo.messages, nullptr);

    EdEventInfo.messages[1] = &BtEventMes0;
    EdEventInfo.messages[2] = &BtEventMes1;
    auto *config = static_cast<EDIT_CONFIG_VIEW *>(SaveData->GetConfigData());

    const float text_rate = config->message_speed != 0 ? 0.6f : 0.3f;

    EdEventInfo.messages[1]->text_rate = text_rate;
    EdEventInfo.messages[2]->text_rate = text_rate;
    EdEventInfo.messages[1]->text_rate_set = text_rate;
    EdEventInfo.messages[2]->text_rate_set = text_rate;

    if (BtEventInfo.fade_on_start != 0) {
        EdFadeInit();
    }

    BtEventInfo.fade_on_start = true;

    for (BT_OBJ_HANDLE &handle : BtObjHdl) {
        handle.frame = nullptr;
        handle.character = nullptr;
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
    return EdEventInit(event, arena, g_event_data);
}

void BtSetMapJumpFloor(int floor) {
    BtMapJumpFloor = floor;
}

namespace {

int GetStackInt(RS_STACKDATA *argument) {
    if (argument->type == RS_FLOAT) {
        return static_cast<int>(argument->f);
    }

    return argument->i;
}

float GetStackFloat(RS_STACKDATA *argument) {
    if (argument->type == RS_INT) {
        return static_cast<float>(argument->i);
    }

    return argument->f;
}

char *GetStackString(RS_STACKDATA *argument) {
    return argument->s;
}

void SetStack(RS_STACKDATA *argument, int value) {
    if (argument->type == RS_PTR) {
        argument->p->i = value;
    }
}

void SetStack(RS_STACKDATA *argument, float value) {
    if (argument->type == RS_PTR) {
        argument->p->f = value;
    }
}

} // namespace

int _GET_FLOOR_LEVEL(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, UserStatus->cur_floor);
    return 1;
}

int _SET_FLOOR_LEVEL(RS_STACKDATA *stack, int /*argument_count*/) {
    static_cast<CDngStatusData *>(UserStatus)->SetNowFloor(GetStackInt(stack));
    return 1;
}

int _GET_OLD_FLOOR_LEVEL(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, UserStatus->prev_floor);
    return 1;
}

int _GET_ACTION_MODE(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, BtEventInfo.action_mode);
    return 1;
}

// The selection slot is held in PortItemSelectResult; BtEventInfo.item_select_result only flags
// that a slot is pending, since its s32 cannot store a host address.
int _ITEM_USE_WINDOW(RS_STACKDATA *stack, int argument_count) {
    if (stack == nullptr || argument_count < 1 || argument_count > 17 || stack->type != RS_PTR) {
        return 0;
    }

    std::span<RS_STACKDATA> arguments(stack, argument_count);
    PortItemSelectResult = arguments[0].p;
    BtEventInfo.item_select_result = arguments[0].p != nullptr;

    int item_count = argument_count - 1;

    for (int i = 0; i < item_count; i++) {
        BtEventInfo.item_select_list[i] = GetStackInt(&arguments[i + 1]);
    }

    BtEventInfo.item_select_filtered = argument_count > 1;
    BtEventInfo.item_select_list[item_count] = -1;
    BtEventInfo.request = BT_REQUEST_ITEM_WINDOW;
    return 1;
}

int _CHECK_EVENT_FLG(RS_STACKDATA *stack, int /*argument_count*/) {
    int flag_no = GetStackInt(stack++);
    int flag = UserStatus->ChkEventFlag(flag_no);
    SetStack(stack, flag);
    return 1;
}

int _SET_EVENT_FLG(RS_STACKDATA *stack, int /*argument_count*/) {
    int          flag_no = GetStackInt(stack++);
    int          value = GetStackInt(stack);
    CUserStatus *status = UserStatus;

    if (flag_no >= 0 && flag_no < 50) {
        status->event_flags[flag_no] = value;
    }

    return 1;
}

int _GET_OBJHDL(RS_STACKDATA *stack, int /*argument_count*/) {
    int            index = GetStackInt(stack++);
    int            parts_no = GetStackInt(stack++);
    char          *name = GetStackString(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (parts_no != -1) {
        CDungeonMap *map = NowDngMap;
        CFrame      *frame = map->parts[parts_no].GetSearchFrame(name);

        if (frame != nullptr) {
            handle->frame = frame;
            handle->type = 0;
        } else {
            std::println("******** Frame NotFount {} *******", name);
            handle->frame = nullptr;
            handle->type = 0;
        }
    } else {
        CFrame *frame = NowDngMap->GetFrameSearch(name);

        if (frame != nullptr) {
            handle->frame = frame;
            handle->type = 0;
        } else {
            std::println("******** All Frame NotFount {} *******", name);
            handle->frame = nullptr;
            handle->type = 0;
        }
    }

    return 1;
}

int _SET_OBJHDL_POS(RS_STACKDATA *stack, int /*argument_count*/) {
    int            index = GetStackInt(stack++);
    float          x = GetStackFloat(stack++);
    float          y = GetStackFloat(stack++);
    float          z = GetStackFloat(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (handle->type == 0 && handle->frame != nullptr) {
        handle->frame->SetPosition(x, y, z);
    }

    if (handle->type == 1 && handle->character != nullptr) {
        handle->character->SetPosition(x, y, z);
    }

    return 1;
}

int _SET_OBJHDL_ROT(RS_STACKDATA *stack, int /*argument_count*/) {
    int            index = GetStackInt(stack++);
    float          x = GetStackFloat(stack++);
    float          y = GetStackFloat(stack++);
    float          z = GetStackFloat(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != nullptr) {
            frame->SetRotType(2);
            frame->SetRotation(AngleLimit(x), AngleLimit(y), AngleLimit(z));
        }
    }

    if (handle->type == 1) {
        CCharacter *chara = handle->character;

        if (chara != nullptr) {
            chara->SetRotation(AngleLimit(x), AngleLimit(y), AngleLimit(z));
        }
    }

    return 1;
}

int _SET_OBJHDL_DRAW_FLAG(RS_STACKDATA *stack, int /*argument_count*/) {
    int            index = GetStackInt(stack++);
    int            draw = GetStackInt(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != nullptr) {
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

        if (chara != nullptr) {
            CFrame *frame = chara->frame;

            if (frame != nullptr) {
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

int _GET_OBJHDL_POS(RS_STACKDATA *stack, int /*argument_count*/) {
    sceVu0FVECTOR  local = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR  pos;
    BT_OBJ_HANDLE *handle = GetObjHDL(GetStackInt(stack++));

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != nullptr) {
            frame->GetWorldPosition(pos, local);
        }
    }

    if (handle->type == 1) {
        CCharacter *chara = handle->character;

        if (chara != nullptr) {
            chara->GetPosition(pos);
        }
    }

    SetStack(stack++, pos[0]);
    SetStack(stack++, pos[1]);
    SetStack(stack, pos[2]);
    return 1;
}

int _GET_OBJHDL_ROT(RS_STACKDATA *stack, int /*argument_count*/) {
    sceVu0FVECTOR  rot = {0.0f, 0.0f, 0.0f, 1.0f};
    sceVu0FMATRIX  matrix;
    BT_OBJ_HANDLE *handle = GetObjHDL(GetStackInt(stack++));

    if (handle->type == 0) {
        CFrame *frame = handle->frame;

        if (frame != nullptr) {
            frame->GetLWMatrix(matrix);
            rot[0] = 0.0f;
            rot[1] = atan2f(matrix[2][0], matrix[2][2]);
            rot[2] = 0.0f;
            rot[3] = 1.0f;
        }
    }

    if (handle->type == 1) {
        CCharacter *chara = handle->character;

        if (chara != nullptr) {
            chara->GetRotation(rot);
        }
    }

    SetStack(stack++, rot[0]);
    SetStack(stack++, rot[1]);
    SetStack(stack, rot[2]);
    return 1;
}

int _SET_URA_DUNGEON(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtEventInfo.request = BT_REQUEST_URA_DUNGEON;
    return 1;
}

int _GET_EVENT_POS(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack++, BtEventInfo.position[0]);
    SetStack(stack++, BtEventInfo.position[1]);
    SetStack(stack, BtEventInfo.position[2]);
    return 1;
}

int _GET_EVENT_ROT(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack++, BtEventInfo.direction[0]);
    SetStack(stack++, BtEventInfo.direction[1]);
    SetStack(stack, BtEventInfo.direction[2]);
    return 1;
}

int _OPEN_ENTRANCE_WINDOW(RS_STACKDATA *stack, int /*argument_count*/) {
    if (stack->type != RS_PTR) {
        return 0;
    }

    BtEventInfo.entrance_result = stack->p;
    BtEventInfo.request = BT_REQUEST_ENTRANCE_WINDOW;
    return 1;
}

int _OPEN_ESCAPE_WINDOW(RS_STACKDATA *stack, int /*argument_count*/) {
    if (stack->type != RS_PTR) {
        return 0;
    }

    BtEventInfo.escape_result = stack->p;
    BtBattleMusic_Stop();
    BtEventInfo.request = BT_REQUEST_ESCAPE_WINDOW;
    return 1;
}

int _GO_DUNGEON(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtEventInfo.request = BT_REQUEST_GO_DUNGEON;
    return 1;
}

int _SET_DUNGEON_MAP(RS_STACKDATA *stack, int /*argument_count*/) {
    const std::string_view map_file = GetStackString(stack++);
    BtLoadMapType = GetStackInt(stack);
    *std::ranges::copy(map_file, BtLoadMapFileName.begin()).out = '\0';
    MainDungeonMap.map_type = BtLoadMapType;
    return 1;
}

int _LOAD_DUNGEON_MAP2(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    CUserStatus *status = UserStatus;
    status->res_limit_zone_current = RES_LIMIT_ZONE_NONE;
    BtMapJumpLoad(BtLoadMapFileName.data());
    return 1;
}

int _LOAD_MONSTOR(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtLoadMonstor(0);
    BtArrengeMonstor();
    return 1;
}

int _SET_RANDOM_MAP(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    if (NowDngMap->map_type == 1) {
        BtCleatRandomMap();
    } else {
        BtCleatFreeMap();
    }

    return 1;
}

int _SET_EVENT_SW(RS_STACKDATA *stack, int /*argument_count*/) {
    int script_no = GetStackInt(stack++);
    NowEventMan->SearchDataSwitch(script_no, GetStackInt(stack));
    return 1;
}

int _SET_MONSTOR_ID(RS_STACKDATA *stack, int argument_count) {
    int   model_no = GetStackInt(stack++);
    int   event_flag = -1;
    float pos[3];

    if (argument_count == 2 && GetStackInt(stack++)) {
        BtLoadMonstor(0);
        NowMonstorUnit->CleanViewMonstor(0);
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

int _CHK_ATRA_HAVE(RS_STACKDATA *stack, int /*argument_count*/) {
    int atra_no = GetStackInt(stack++);
    int have = GetAtraTipNowHave(atra_no, UserStatus->cur_georama);

    std::println("{},{} --> {}", atra_no, static_cast<int>(UserStatus->cur_georama), have);
    SetStack(stack, have);
    return 1;
}

int _SET_ATRA(RS_STACKDATA *stack, int /*argument_count*/) {
    sceVu0FVECTOR pos;

    pos[0] = GetStackFloat(stack++);
    pos[1] = GetStackFloat(stack++);
    pos[2] = GetStackFloat(stack++);
    pos[3] = 1.0f;
    NowDngMap->SetAtraBoll(pos, GetStackInt(stack));
    return 1;
}

int _SET_IBOX(RS_STACKDATA *stack, int /*argument_count*/) {
    sceVu0FVECTOR pos;

    pos[0] = GetStackFloat(stack++);
    pos[1] = GetStackFloat(stack++);
    pos[2] = GetStackFloat(stack++);
    pos[3] = 1.0f;
    NowDngMap->SetTreasureBox(pos, GetStackInt(stack), TREASURE_BOX_SMALL, 0);
    return 1;
}

int _GET_NOW_USER_ID(RS_STACKDATA *stack, int /*argument_count*/) {
    int cur_chara = UserStatus->cur_chara;
    std::println("get id = {}", cur_chara);
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

int _GET_NPC_OBJHDL(RS_STACKDATA *stack, int /*argument_count*/) {
    int            index = GetStackInt(stack++);
    int            npc_no = GetStackInt(stack);
    BT_OBJ_HANDLE *handle = GetObjHDL(index);

    if (npc_no < 0 || npc_no > 3) {
        std::println("over err");
        return 1;
    }

    CCharacter *chara = &NowDngMap->npc[npc_no].chara;

    if (chara->frame != nullptr) {
        handle->character = chara;
        handle->type = 1;
    } else {
        handle->character = nullptr;
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
        speed = static_cast<int>(GetStackFloat(stack++));
    }

    if (argument_count > 3) {
        mode = GetStackInt(stack);
    }

    if (NowDngMap->npc[npc_no].chara.frame == nullptr) {
        return 1;
    }

    if (argument_count < 3) {
        NowDngMap->NPCSetMotion(npc_no, motion_no);
    } else {
        NowDngMap->NPCSetMotion(npc_no, motion_no, speed, mode);
    }

    return 1;
}

int _SET_NPC_ON_OFF(RS_STACKDATA *stack, int /*argument_count*/) {
    int npc_no = GetStackInt(stack++);
    int enable = GetStackInt(stack);

    if (NowDngMap->npc[npc_no].chara.frame == nullptr) {
        return 1;
    }

    NowDngMap->npc[npc_no].visible = enable;
    return 1;
}

int _GET_GATEKEY_NO(RS_STACKDATA *stack, int /*argument_count*/) {
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

int _RESET_MAIN_CHR(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    if (BtEventInfo.script_main_chr == -1) {
        return 1;
    }

    selectChrUnit(BtEventInfo.script_main_chr, 0);
    BtEventInfo.script_main_chr = -1;
    return 1;
}

int _SET_LIMMIT_ZONE(RS_STACKDATA *stack, int /*argument_count*/) {
    UserStatus->res_limit_zone_current = GetStackInt(stack);
    SndSPSeLoad(0x1B);
    return 1;
}

int _SET_DEAD_FLAG(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    static_cast<CDngStatusData *>(UserStatus)->SetDead();
    return 1;
}

int _ALL_DRAW_FLAG(RS_STACKDATA *stack, int argument_count) {
    if (argument_count == 1) {
        BtAllDrawFlag = GetStackInt(stack);
    }

    return 1;
}

int _SET_FLOOR_TITLE(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    FloorTitleOn();
    return 1;
}

int _GET_RUBY_ELEMENT(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, BtRubyDoorKey);
    return 1;
}

int _SET_RUBY_ELEMENT(RS_STACKDATA *stack, int /*argument_count*/) {
    GetStackInt(stack);
    return 1;
}

int _SET_FLOOR_TITLE_OFF(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtEventInfo.floor_title_off = 1;
    return 1;
}

int _SET_RES_LIMMIT_ZONE(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    static_cast<CDngStatusData *>(UserStatus)->SetResLimmitZone();

    if (UserStatus->res_limit_zone_current != RES_LIMIT_ZONE_NONE) {
        SndSPSeLoad(0x1B);
    }

    return 1;
}

int _CLEAR_DEAMON_SHAFT(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    static_cast<CDngStatusData *>(UserStatus)->ClearDeamonShaft();
    return 1;
}

int _INIT_BEE(RS_STACKDATA *stack, int /*argument_count*/) {
    BtEventInfo.bee_npc = GetStackInt(stack);
    InitBee(NPCUnit[BtEventInfo.bee_npc].frame, 15);
    return 1;
}

int _END_BEE(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtEventInfo.bee_npc = -1;
    return 1;
}

int _EASTKING_COMPLETE(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, EastKingCheckComplete());
    return 1;
}

int _GET_ITEM_TRAPID(RS_STACKDATA *stack, int /*argument_count*/) {
    int box = BtEventInfo.treasure_box;
    SetStack(stack, NowDngMap->boxes[box].trap_no);
    return 1;
}

int _RESET_ITEM_TRAP(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    int box = BtEventInfo.treasure_box;
    NowDngMap->boxes[box].trap_no = 0;
    BtEventInfo.treasure_box = 0;
    return 1;
}

int _BOM_SET(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
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

int _SET_STATUS_ERR(RS_STACKDATA *stack, int /*argument_count*/) {
    sceVu0FVECTOR pos;

    BtSetStatusErr(GetStackInt(stack));
    sceVu0CopyVector(pos, CharaMain.pos);
    pos[1] += 16.0f;
    NowColData->Set(pos, 2, 2, 5.0f, 0.0f, 1, 2, 0, 0);
    return 1;
}

int _CHECK_MARDAN(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, SaveData->GetMardanGareyanFlag());
    return 1;
}

int _NO_RESET_CHARA_NO(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtEventInfo.no_reset_chara = 1;
    return 1;
}

int _CHECK_CHR_HELP(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, BtEventInfo.chara_help);
    return 1;
}

int _HOLD_ITEM_EVENT(RS_STACKDATA *stack, int /*argument_count*/) {
    int script_no = GetStackInt(stack);
    DngEventMan.SearchItemEventHold(script_no);
    UraEventMan.SearchItemEventHold(script_no);
    return 1;
}

int _STOP_BATTLE_BGM(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtBattleMusic_Stop();
    return 1;
}

int _NO_STATUS_RECOVER(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    BtEventInfo.no_status_recover = 1;
    return 1;
}

int _SET_QUEST_DUNGEON(RS_STACKDATA *stack, int /*argument_count*/) {
    int dungeon_no = GetStackInt(stack++);
    SaveData->QuestDungeon(dungeon_no, GetStackInt(stack));
    return 1;
}

int _GET_MAP_CODE(RS_STACKDATA *stack, int /*argument_count*/) {
    SetStack(stack, UserStatus->cur_georama + 1);
    return 1;
}

int _SET_ACTIVE_ITEM_ICON(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    LoadActiveItemIcon();
    return 1;
}

int _GET_ITEM_UNIT_NO(RS_STACKDATA *stack, int /*argument_count*/) {
    int item_no = GetStackInt(stack++);
    int owner = -1;

    if (item_no >= ITEM_WEAPON_START) {
        if (item_no < ITEM_WEAPON_WOODENSLINGSHOT_BROKEN) {
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

int _SET_IBOX_ANGLE(RS_STACKDATA *stack, int /*argument_count*/) {
    NowDngMap->boxes[BtEventInfo.treasure_box].lid_angle = GetStackFloat(stack);
    return 1;
}

int _SET_IBOX_FINISH(RS_STACKDATA * /*stack*/, int /*argument_count*/) {
    NowDngMap->boxes[BtEventInfo.treasure_box].lid_angle = -30.0f;
    NowDngMap->boxes[BtEventInfo.treasure_box].closed = 0;
    return 1;
}

int _GET_PIERO_ITEM(RS_STACKDATA *stack, int /*argument_count*/) {
    int item0 = 0;
    int item1 = 0;

    GetPieroItem(selectMapNo, BtUraDongeon, &item0, &item1);
    SetStack(stack++, item0);
    SetStack(stack, item1);
    return 1;
}

namespace {

struct BT_EVENT_EXTERNAL_FUNCTION {
    int (*function)(RS_STACKDATA *, int);
    int operation;
};

// Ended by an entry without a function.
constexpr auto ext_func_info = std::to_array<BT_EVENT_EXTERNAL_FUNCTION>({
    {.function = _GET_FLOOR_LEVEL,         .operation = 1010},
    {.function = _SET_FLOOR_LEVEL,         .operation = 1011},
    {.function = _GET_OLD_FLOOR_LEVEL,     .operation = 1012},
    {.function = _GET_ACTION_MODE,         .operation = 1020},
    {.function = _ITEM_USE_WINDOW,         .operation = 1021},
    {.function = _CHECK_EVENT_FLG,         .operation = 1023},
    {.function = _SET_EVENT_FLG,           .operation = 1024},
    {.function = _GET_OBJHDL,              .operation = 1030},
    {.function = _SET_OBJHDL_POS,          .operation = 1031},
    {.function = _SET_OBJHDL_ROT,          .operation = 1032},
    {.function = _SET_OBJHDL_DRAW_FLAG,    .operation = 1033},
    {.function = _GET_OBJHDL_POS,          .operation = 1034},
    {.function = _GET_OBJHDL_ROT,          .operation = 1035},
    {.function = _SET_URA_DUNGEON,         .operation = 1036},
    {.function = _GET_EVENT_POS,           .operation = 1040},
    {.function = _GET_EVENT_ROT,           .operation = 1041},
    {.function = _OPEN_ENTRANCE_WINDOW,    .operation = 1022},
    {.function = _OPEN_ESCAPE_WINDOW,      .operation = 1070},
    {.function = _GO_DUNGEON,              .operation = 1050},
    {.function = _SET_DUNGEON_MAP,         .operation = 1051},
    {.function = _LOAD_DUNGEON_MAP2,       .operation = 1052},
    {.function = _LOAD_MONSTOR,            .operation = 1053},
    {.function = _SET_RANDOM_MAP,          .operation = 1054},
    {.function = _SET_EVENT_SW,            .operation = 1055},
    {.function = _SET_MONSTOR_ID,          .operation = 1056},
    {.function = _CHK_ATRA_HAVE,           .operation = 1057},
    {.function = _SET_ATRA,                .operation = 1058},
    {.function = _SET_IBOX,                .operation = 1059},
    {.function = _GET_NOW_USER_ID,         .operation = 1060},
    {.function = _RUN_SCRIPT_NO,           .operation = 1061},
    {.function = _CLEAN_MONSTOR_SCRIPT_NO, .operation = 1066},
    {.function = _GET_NPC_OBJHDL,          .operation = 1062},
    {.function = _SET_MOTION_OBJHDL,       .operation = 1063},
    {.function = _SET_NPC_ON_OFF,          .operation = 1094},
    {.function = _GET_GATEKEY_NO,          .operation = 1064},
    {.function = _USER_WEAPON_DRAW,        .operation = 1065},
    {.function = _SET_MAIN_CHR2,           .operation = 1067},
    {.function = _RESET_MAIN_CHR,          .operation = 1071},
    {.function = _SET_LIMMIT_ZONE,         .operation = 1068},
    {.function = _SET_DEAD_FLAG,           .operation = 1069},
    {.function = _ALL_DRAW_FLAG,           .operation = 1072},
    {.function = _SET_FLOOR_TITLE,         .operation = 1073},
    {.function = _GET_RUBY_ELEMENT,        .operation = 1074},
    {.function = _SET_RUBY_ELEMENT,        .operation = 1088},
    {.function = _SET_FLOOR_TITLE_OFF,     .operation = 1076},
    {.function = _INIT_BEE,                .operation = 1077},
    {.function = _END_BEE,                 .operation = 1078},
    {.function = _EASTKING_COMPLETE,       .operation = 1079},
    {.function = _SET_RES_LIMMIT_ZONE,     .operation = 1080},
    {.function = _GET_ITEM_TRAPID,         .operation = 1081},
    {.function = _RESET_ITEM_TRAP,         .operation = 1082},
    {.function = _BOM_SET,                 .operation = 1083},
    {.function = _SET_STATUS_ERR,          .operation = 1084},
    {.function = _CHECK_MARDAN,            .operation = 1086},
    {.function = _NO_RESET_CHARA_NO,       .operation = 1087},
    {.function = _CHECK_CHR_HELP,          .operation = 1089},
    {.function = _HOLD_ITEM_EVENT,         .operation = 1090},
    {.function = _STOP_BATTLE_BGM,         .operation = 1091},
    {.function = _NO_STATUS_RECOVER,       .operation = 1092},
    {.function = _SET_QUEST_DUNGEON,       .operation = 1093},
    {.function = _GET_MAP_CODE,            .operation = 1095},
    {.function = _SET_ACTIVE_ITEM_ICON,    .operation = 1096},
    {.function = _GET_ITEM_UNIT_NO,        .operation = 1097},
    {.function = _SET_IBOX_ANGLE,          .operation = 1098},
    {.function = _SET_IBOX_FINISH,         .operation = 1099},
    {.function = _GET_PIERO_ITEM,          .operation = 1100},
    {.function = _CLEAR_DEAMON_SHAFT,      .operation = 1101},
    {.function = nullptr,                  .operation = -1  },
});

} // namespace

void AddSystemEventScript() {
    for (int i = 0;; i++) {
        if (ext_func_info[i].function == nullptr) {
            break;
        }

        for (int j = 0; j < i; j++) {
            if (ext_func_info[i].operation == ext_func_info[j].operation) {
                std::println("same ext_func_no!!!");

                while (true) {
                }
            }
        }

        int operation = ext_func_info[i].operation;

        if (operation < 0 || operation >= 1500) {
            std::print("ext func over!!");
        } else {
            ext_func__2[operation] = ext_func_info[i].function;
        }
    }
}

} // namespace dc::game
