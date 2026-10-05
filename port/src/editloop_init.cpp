#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>

#include <cmath>
#include <cstdint>
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
#include "edit_in.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail's EditInit. It carves the town's objects out of EtcDataBuffer with quadword counts sized
// for the PS2's classes (0x470 for four 4544-byte CCharacters); every one of them is larger on the
// host, so each block here is sized from the host class.

void BtSetMapJumpFloor(int floor);
void GetLanguageName(char *name);
void LoadScript();
int  LoadEditMapData(EDIT_MAP_INFO *info, char *name, int kind);
void LoadGroundData();
int  LoadTexture();
void EditPartsObjectOnOff();
void InitWorkBuffer();

extern int           start_event_no;
extern int           start_system_event;
extern CCameraFollow EventCamera;

namespace {

template <class T>
int Quadwords(int count) {
    return static_cast<int>((sizeof(T) * count + 15) / 16);
}

// editloop.cpp's, which is static there.
int RunEvent(int event_no, CCamera *camera) {
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

} // namespace

PC_OVERRIDE int EditInit(void *param) {
    char map_path[0x80];
    char save_path[0x80];
    int  size;
    int  mes_size;

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
        sprintf(EditMapName, "e0%d", map_no + 1);
    }

    int map_id = MapNo;

    if (map_id > 10) {
        int interior = map_id - 10;

        if (interior < 10) {
            sprintf(EditMapName, "s0%d", interior);
        } else {
            sprintf(EditMapName, "s%d", interior);
        }
    }

    if (NowEditMap == 99) {
        strcpy(EditDataDir, "");
        interior_test = 1;
    } else {
        strcpy(EditDataDir, "gedit/");
        strcat(EditDataDir, EditMapName);
        strcat(EditDataDir, "/");
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
    EditArea = new ((u_long128 *) EtcDataBuffer.Alloc(Quadwords<CEditArea>(4))) CEditArea[4];
    pEditGround = new ((u_long128 *) EtcDataBuffer.Alloc(Quadwords<CEditGround>(1))) CEditGround;
    ObjParts = new ((u_long128 *) EtcDataBuffer.Alloc(Quadwords<CMapParts>(24))) CMapParts[24];
    MotionParts = new ((u_long128 *) EtcDataBuffer.Alloc(Quadwords<CCharacter>(4))) CCharacter[4];
    RiverParts = new ((u_long128 *) EtcDataBuffer.Alloc(Quadwords<CMapParts>(16))) CMapParts[16];
    RoadParts = new ((u_long128 *) EtcDataBuffer.Alloc(Quadwords<CMapParts>(6))) CMapParts[6];
    float near_clip = 10.0f;
    MGSetRenderInfo(800.0f, near_clip, 65535.0f);
    GetEditDataDir(map_path);
    strcat(map_path, "mapinfo.cfb");
    EditMapInfo = (EDIT_MAP_INFO *) EtcDataBuffer.Alloc(Quadwords<EDIT_MAP_INFO>(1));
    EdInInfo = (EDIT_IN_INFO *) EtcDataBuffer.Alloc(Quadwords<EDIT_IN_INFO>(1));

    if (interior_test == 0) {
        LoadEditMapData(EditMapInfo, map_path, MapNo);
    } else {
        LoadEditMapData(EditMapInfo, "gedit/interior/mapinfo.cfg", MapNo);
        LoadFile("gedit/interior/interior.cfg", read_buffer, &size);
        int c;
        int column;
        int row;
        int count;
        int read_pos;
        u8 *text;

        text = (u8 *) read_buffer;
        read_pos = 0;
        count = size;
        column = 0;
        row = 0;

        while (c = text[read_pos++], (count < read_pos) ? 0 : 1) {
            if (c >= 'a' && c < '{') {
                interior_name[row][column] = c;
                column++;
                continue;
            }

            if (c >= '0' && c < ':') {
                interior_name[row][column] = c;
                column++;
                continue;
            }

            if (c == '/' || c == '_') {
                interior_name[row][column] = c;
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
    EdEventData = NULL;
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

    EdSetDOF((DEPTH_OF_FIELD_INFO *) &EditMapInfo->dof_start_time);
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
            GetEditDataDir(save_path);
            strcat(save_path, "gdata0.edt");
            int loaded = LoadFile2(save_path, read_buffer, NULL, 0);

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
                pEditGround->Load((char *) read_buffer);
            }

            pEditGround->MakePartsBox();

            for (int i = 0; i < 128; i++) {
                *element_data = -1;
                element_data++;
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
    std::intptr_t free_start = reinterpret_cast<std::intptr_t>(DataBuffer__2.base) + DataBuffer__2.used * 16;
    int free_quads = DataBuffer__2.limit - DataBuffer__2.used;

    if (free_quads < 326000) {
        printf("Allocation error!!\n");
    }

    int misalign = static_cast<int>(free_start & 0x3F);

    if (free_start < 0 && misalign != 0) {
        misalign -= 0x40;
    }

    if (misalign != 0) {
        free_start += ((0x40 - misalign) >> 4) * 16;
    }

    EdNPCBuffer.base = reinterpret_cast<u_char *>(free_start);
    EdNPCBuffer.limit = free_quads - 4;
    EdNPCBuffer.used = 0;
    EdVillagerBuffer.base = reinterpret_cast<u_char *>(free_start);
    EdVillagerBuffer.limit = free_quads - 4;
    EdVillagerBuffer.used = 0;
    EdNPCReadBuffer = (u_long128 *) (EdNPCBuffer.base + EdNPCBuffer.used * 16 + (free_quads >> 1) * 16);
    int read_misalign = static_cast<int>(reinterpret_cast<std::intptr_t>(EdNPCReadBuffer) & 0x3F);

    if (reinterpret_cast<std::intptr_t>(EdNPCReadBuffer) < 0 && read_misalign != 0) {
        read_misalign -= 0x40;
    }

    if (read_misalign != 0) {
        EdNPCReadBuffer += (0x40 - read_misalign) >> 4;
    }

    EdCreateVillagerTable(EditMapInfo);
    EdInitVillagerControl();
    EdInitVillagerTable(NowTime, EditMapInfo);
    EdLoadMainChara("chara/c01d.chr", "info.cfg", &CharaBuffer);
    sceVu0FVECTOR start_position = {0.0f, 0.0f, 0.0f, 1.0f};
    sceVu0FVECTOR start_rotation = {0.0f, 0.0f, 0.0f, 0.0f};
    sceVu0FVECTOR fade;
    GameMode = ED_MODE_WALK;
    EdGetFadeColor(fade);
    EdFadeIn(128, (float) (int) fade[0], (float) (int) fade[1], (float) (int) fade[2]);
    Chara->SetPosition(start_position);
    Chara->SetRotation(start_rotation);
    float yaw = start_rotation[1] - PI_SHORT;

    if (yaw < PI_SHORT) {
        yaw -= TWO_PI;
    }

    MainCamera.SetAngleSoon(yaw);
    MainCamera.SetFollow(start_position[0], 14.0f + start_position[1], start_position[2]);
    MainCamera.Step(-1);
    EffectTable__3 = (CEffect *) EtcDataBuffer.Alloc(Quadwords<CEffect>(128));
    EdEffectGroup.Initialize(EffectTable__3, 128);
    EdEffectGroup.Clear();
    EdInitMesParam();
    EditMes1.tex_block = 26;
    EditEventMes1.tex_block = 26;
    EditMes1.tex_buff = MesWinTexBuff_01;
    EditEventMes1.tex_buff = MesWinTexBuff_02;
    short *talk_mes = (short *) (EdMesBuffer.base + EdMesBuffer.used * 16);
    char   mes_path[0x40];
    char   sys_path[0x40];
    char   language[0x10];
    GetEditDataDir(sys_path);
    GetEditDataDir(mes_path);
    strcat(mes_path, EditMapName);
    strcat(mes_path, "talk");
    GetLanguageName(language);
    strcat(mes_path, language);
    strcat(mes_path, ".mes");
    strcat(sys_path, "fconv.bin");

    if (LoadFile2(mes_path, talk_mes, &mes_size, 0) == 0 && LoadFile2(sys_path, talk_mes, &mes_size, 0) == 0) {
        LoadFile("gedit/e01/fconv.bin", talk_mes, &mes_size);
    }

    EdMesBuffer.Alloc((mes_size >> 4) + 1);
    EditMes1.SetBuff(talk_mes);
    EdMesBuffer.Align64();
    short *system_mes = (short *) (EdMesBuffer.base + EdMesBuffer.used * 16);
#ifdef PAL
    sprintf(mes_path, "gedit/system/editsys%s.mes", language);
    LoadFile(mes_path, system_mes, &mes_size);
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
    short *window_mes = (short *) (EdMesBuffer.base + EdMesBuffer.used * 16);
    char   window_path[0x40] = "meswin/system14";
    int    language_no = LanguageCode;

    if (language_no > 0) {
        sprintf(window_path, "meswin/system14_%d", language_no);
    }

    strcat(window_path, ".mes");

    if (LoadFile2(window_path, window_mes, &mes_size, 0) == 0) {
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
        RunEvent(128, NULL);
    } else {
        RunEvent(event_no, NULL);
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

// Retail's, with the 64-byte rounding of the work arena's start done on the whole pointer.
PC_OVERRIDE void InitWorkBuffer() {
    int     quads = EdNPCBuffer.used;
    u_char *free_start = EdNPCBuffer.base + quads * 16;
    quads = EdNPCBuffer.limit - quads;
    EdVillagerBuffer.base = free_start;
    EdVillagerBuffer.limit = quads;
    EdVillagerBuffer.used = 0;
    free_start = (u_char *) ((((std::uintptr_t) free_start >> 6) + 1) << 6);
    EdWorkBuffer.base = free_start;
    EdWorkBuffer.limit = quads - 4;
    EdWorkBuffer.used = 0;
    free_start = (u_char *) read_buffer;
    EdMenuBuffer.base = free_start - 0x180000;
    EdMenuBuffer.limit = 0x3A2E0;
    EdMenuBuffer.used = 0;
}
