#include "common.h"

#include <libgraph.h>
#include <libpkt.h>
#include <libvu0.h>
#include <sifdev.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
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
#include <vector>

#include "app/gameloop.hpp"
#include "battlemenu.hpp"
#include "boxvu0.hpp"
#include "btsysscript.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "collision.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "debugfont.hpp"
#include "dialog/npc.hpp"
#include "dispctrl.hpp"
#include "dngstatusdata.hpp"
#include "ebattle.hpp"
#include "edit.hpp"
#include "edit_in.hpp"
#include "editarea.hpp"
#include "editground.hpp"
#include "editloop.hpp"
#include "editloop3.hpp"
#include "editmapscript.hpp"
#include "editmenu.hpp"
#include "editpartsinfo.hpp"
#include "effect.hpp"
#include "effectgroup.hpp"
#include "effectmacro.hpp"
#include "fishing.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "gfx/gfx.hpp"
#include "input/camera_zoom.hpp"
#include "input/mouse_look.hpp"
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
#include "render/draw2d.hpp"
#include "runeffect.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "vutext.hpp"
#include "weaponlevelup.hpp"
#include "wind.hpp"

namespace dc::game {

using namespace dc::dialog;
using namespace dc::input;
using namespace dc::render;

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

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
CEffect       *EffectTable__3;
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

// init_priority keeps this static's place in the order rand() is drawn from; see app/runtime.cpp.
CWind         EdWind __attribute__((init_priority(65009)));
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
CFrameVu1    *SkyFrame[4];
CFrame       *SunFrame[4];
/** Sprites the system events draw with. */
C3DSprite SystemEffect[8];
/** Where the player is held while a door plays its motion. */
sceVu0FVECTOR fix_chara_pos;
/** Which way the player faces while a door plays its motion. */
sceVu0FVECTOR fix_chara_rot;
sceVu0FVECTOR fix_camera_pos;
char          interior_map_name[0x40];
float         NowPartsCursorPos[4];

namespace {

/** Model of the cursor the editor places parts with. */
CFrameVu1 ecursorframe;

} // namespace

sceVu0FVECTOR NowCursorPos;
sceVu0FVECTOR NextCursorPos;
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

void                CopyCMapParts(CMapParts *to, CMapParts *from, CDataAlloc2<1> *arena);
EPARTS_INFO_HEADER *LoadPTS(CMapParts *parts, unsigned int *archive, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime, EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points, CMapParts *shared);
void                LoadPTS(CMapParts *parts, MAP_PARTS_INFO *info, OBJ_ANIME_SEQ *anime, EDIT_EFFECT_INFO *effects, EDIT_OBJECT_TIMER *timers, ED_EVENT_POINT *points);
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

namespace {
void EditMainDraw();

/**
 * The spans of EditMainDraw timed under DC_PERF_GAME_PHASES=1.
 */
enum class GameDrawPhase : std::size_t {
    Main,
    SkyBase,
    GroundFirst,
    GroundSecond,
    Water,
    FarDof,
    FarPrepare,
    FarCharacter,
    FarShadow,
    FarShadowBegin,
    FarShadowParts,
    FarShadowEnd,
    DofRecord,
    Character,
    CharacterStep,
    CharacterNearParts,
    CharacterLights,
    CharacterDraw,
    Shadows,
    ShadowNear,
    ShadowFar,
    ShadowGeorama,
    MotionParts,
    Effects,
    Final2D,
    Count,
};

using GameDrawClock = std::chrono::steady_clock;

struct GameDrawSpan {
    double        total = 0.0;
    double        maximum = 0.0;
    std::uint32_t count = 0;
};

std::array<GameDrawSpan, static_cast<std::size_t>(GameDrawPhase::Count)> game_draw_spans;
std::uint64_t                                                            far_villager_marks = 0;

/**
 * Times one EditMainDraw call and its phases when DC_PERF_GAME_PHASES is 1, and every 120 calls
 * reports each phase's mean and longest time on stderr.
 */
class GameDrawTrace {
public:
    GameDrawTrace() {
        static const bool enabled = [] {
            const char *setting = std::getenv("DC_PERF_GAME_PHASES");
            return setting != nullptr && std::string_view(setting) == "1";
        }();
        enabled_ = enabled;
        start_ = Start();
    }

    GameDrawTrace(const GameDrawTrace &) = delete;
    GameDrawTrace &operator=(const GameDrawTrace &) = delete;

    ~GameDrawTrace() {
        if (!enabled_) {
            return;
        }
        Finish(GameDrawPhase::Main, start_);
        const std::uint32_t calls = game_draw_spans[static_cast<std::size_t>(GameDrawPhase::Main)].count;
        if (calls < 120) {
            return;
        }
        constexpr std::array<const char *, static_cast<std::size_t>(GameDrawPhase::Count)> names = {
            "main",
            "sky_base",
            "ground_first",
            "ground_second",
            "water",
            "far_dof",
            "far_prepare",
            "far_character",
            "far_shadow",
            "far_shadow_begin",
            "far_shadow_parts",
            "far_shadow_end",
            "dof_record",
            "character",
            "character_step",
            "character_near_parts",
            "character_lights",
            "character_draw",
            "shadows",
            "shadow_near",
            "shadow_far",
            "shadow_georama",
            "motion_parts",
            "effects",
            "final_2d",
        };
        for (std::size_t first = 0; first < game_draw_spans.size(); first += 4) {
            std::string report = std::format("cpu gamephases {} calls mean/max ms:", calls);
            if (first == 0) {
                report += std::format(" far_marks={}", far_villager_marks);
            }
            for (std::size_t phase = first; phase < std::min(first + 4, game_draw_spans.size()); ++phase) {
                const GameDrawSpan &span = game_draw_spans[phase];
                const double        mean = span.count == 0 ? 0.0 : span.total / static_cast<double>(span.count);
                report += std::format(" {}[{}]={:.3f}/{:.3f}", names[phase], span.count, mean, span.maximum);
            }
            std::fprintf(stderr, "%s\n", report.c_str());
        }
        game_draw_spans = {};
        far_villager_marks = 0;
    }

    /**
     * Returns the start of a phase: now while tracing, otherwise a zero time.
     */
    GameDrawClock::time_point Start() const { return enabled_ ? GameDrawClock::now() : GameDrawClock::time_point{}; }

    /**
     * Adds the time since start to phase while tracing.
     */
    void Finish(GameDrawPhase phase, GameDrawClock::time_point start) const {
        if (!enabled_) {
            return;
        }
        const double  elapsed = std::chrono::duration<double, std::milli>(GameDrawClock::now() - start).count();
        GameDrawSpan &span = game_draw_spans[static_cast<std::size_t>(phase)];
        span.total += elapsed;
        span.maximum = std::max(span.maximum, elapsed);
        ++span.count;
    }

    /**
     * Counts a villager drawn before the depth of field while tracing.
     */
    void MarkFarVillager() const {
        if (enabled_) {
            ++far_villager_marks;
        }
    }

private:
    bool                      enabled_ = false;
    GameDrawClock::time_point start_{};
};

} // namespace

namespace {

/**
 * Copies a string and its terminator into a buffer the caller has sized for it.
 */
void CopyString(char *destination, std::string_view source) {
    std::ranges::copy(source, destination);
    destination[source.size()] = '\0';
}

} // namespace

/**
 * Copies the current editor resource directory into a caller buffer.
 */
void GetEditDataDir(char *name) {
    CopyString(name, EditDataDir);
}

/**
 * Reads the shared and map-specific event scripts into the script arena.
 */
void LoadScript() {
    int   size = 0;
    char *map_script = nullptr;

    EdScriptBuffer.used = 0;
    EdEventData = reinterpret_cast<char *>(&EdScriptBuffer.base[static_cast<ptrdiff_t>(EdScriptBuffer.used) * 16]);
    std::string common_path = std::format("{}event.stb", EditDataDir);
    std::string map_path = std::format("{}{}_{}.mes", EditDataDir, EditMapName, LanguageCode);

    if (LoadFile2(common_path.data(), EdEventData, &size, 0) != 0) {
        EdScriptBuffer.Alloc((size >> 4) + 1);
        map_script = reinterpret_cast<char *>(&EdScriptBuffer.base[static_cast<ptrdiff_t>(EdScriptBuffer.used) * 16]);

        if (LoadFile2(map_path.data(), map_script, &size, 0) != 0) {
            EdScriptBuffer.Alloc((size >> 4) + 1);
        } else {
            map_script = nullptr;
        }

        EdSetEventScript(EdEventData, map_script, &EdScriptBuffer);
    } else {
        EdEventData = nullptr;
    }

    EdScriptBuffer.Align64();
    EdSystemEventData = reinterpret_cast<char *>(&EdScriptBuffer.base[static_cast<ptrdiff_t>(EdScriptBuffer.used) * 16]);

    if (LoadFile2("gedit/system/event.stb", EdSystemEventData, &size, 0) != 0) {
        EdScriptBuffer.Alloc((size >> 4) + 1);
        return;
    }

    EdSystemEventData = nullptr;
}

namespace {

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

} // namespace

/**
 * Begins a fade into a comparison event unless a higher-priority request is active.
 */
int FadeOutToEvent(int event_no, int level) {
    if (EdDebugEventEnable == 0) {
        return 0;
    }

    if (goto_cmp_event != 0 && level <= goto_cmp_event_level) {
        return 0;
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

    if (info != nullptr && info->time_stop == 0) {
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
    NowTime = 0.01f * static_cast<float>(static_cast<int>(100.0f * SaveData->GetNowTime()) % 1200);
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

    auto *config = static_cast<EDIT_CONFIG_VIEW *>(SaveData->GetConfigData());

    draw_clock = static_cast<int>(config->clock_hidden == 0);

    if (key_lock != 0) {
        draw_clock = 0;
    }
}

namespace {

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
    float text_rate = 0.0f;
    auto *config = static_cast<EDIT_CONFIG_VIEW *>(SaveData->GetConfigData());

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

namespace {
/**
 * The ground the player has built on one map, as the save holds it.
 */
struct ED_GRD_DATA {
    u8  unk_00[0x64];
    int complete_event_seen; /**< Zero until the map's own build event has been seen. */
};

} // namespace

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
                    app::GameLoadTraceRequest(MenuMapJumpMode);
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

    if (GameMode == ED_MODE_WALK && loop_counter >= 2 && key_lock == 0 && start_event_no <= 0 &&
        start_system_event <= 0) {
        app::GameLoadTraceControlsReady();
    }
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
bool EdDrawBeyondRetailReach(int *far_marks, const GameDrawTrace &draw_trace) {
    const auto prepare_start = draw_trace.Start();
    bool       drawn = false;

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
                    draw_trace.MarkFarVillager();
                }
            }

            draw_trace.Finish(GameDrawPhase::FarPrepare, prepare_start);
            if (drawn) {
                const auto character_start = draw_trace.Start();
                int        shadow_on = GameMode != ED_MODE_TALK && EdSystemMesCheck() == 0;
                EdDrawCharacter(Chara, 0, 10, EdVillager, far_marks, shadow_on, nullptr);
                draw_trace.Finish(GameDrawPhase::FarCharacter, character_start);
            }
            break;
        }
        default:
            draw_trace.Finish(GameDrawPhase::FarPrepare, prepare_start);
            break;
    }

    if (EdDrawOffMapShadow == 0 && edit_mode_draw == 0 && EditMapInfo->shadow_level > 0) {
        const auto shadow_start = draw_trace.Start();
        const auto shadow_begin_start = draw_trace.Start();
        float      saved_light[4][4];
        float      shadow_light[4][4];
        float      light_colour[4][4];

        MGGetPLight(saved_light, light_colour);
        sceVu0CopyMatrix(shadow_light, saved_light);
        EdLimitShadowLight(shadow_light, 3.0f);
        MGSetPLight(shadow_light, light_colour);
        TexManager.ReloadTexture(Vif1Packet, 0x16);
        MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
        draw_trace.Finish(GameDrawPhase::FarShadowBegin, shadow_begin_start);
        const auto shadow_parts_start = draw_trace.Start();
        EdGroundDrawShadowBeyondNear(pEditGround, EditMapInfo->shadow_near);
        draw_trace.Finish(GameDrawPhase::FarShadowParts, shadow_parts_start);
        const auto shadow_end_start = draw_trace.Start();
        MGEndDrawShadow(EditMapInfo->shadow_mode);
        MGSetPLight(saved_light, light_colour);
        draw_trace.Finish(GameDrawPhase::FarShadowEnd, shadow_end_start);
        draw_trace.Finish(GameDrawPhase::FarShadow, shadow_start);
    }

    return drawn;
}

/**
 * Draws the editor's world for one frame (retail's MainDraw).
 */
void EditMainDraw() {
    const GameDrawTrace draw_trace;
    auto                far_marks = std::make_unique<int[]>(10);
    bool                far_drawn = false;

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
            const auto sky_base_start = draw_trace.Start();
            if (draw_sky != 0) {
                EdDrawSky(NowTime, SkyFrame, SunFrame, SkyBackFrame, NowCamera, EditMapInfo->sky_follow);
            }

            TexManager.ReloadTexture(Vif1Packet, 1);
            TexAnime.TexAnime(1);
            pEditGround->DrawBaseGround();
            draw_trace.Finish(GameDrawPhase::SkyBase, sky_base_start);

            // Georama mode draws the parts at walking's levels of detail, where retail drew them all at
            // their coarsest.
            constexpr int far_lod = 0;
            constexpr int near_lod = 2;
            constexpr int mid_lod = 0;

            const auto ground_first_start = draw_trace.Start();
            pEditGround->Draw(NowTime, 1, far_lod, near_lod, mid_lod, near_lod);
            draw_trace.Finish(GameDrawPhase::GroundFirst, ground_first_start);

            if (edit_mode_draw != 0) {
                sceVu0FVECTOR at;
                sceVu0FVECTOR colour = {0.0f, 0.0f, 0.0f, 0.0f};

                sceVu0CopyVector(at, ECursorFrame->position);
                colour[1] = NowCursorRotY;
                at[1] = pEditGround->GetAlt(at[0], at[1], at[2]);
                pEditGround->DrawPartsCursor(NowSelectParts, at, NowPartsCursorPos, NowSelectAngle, colour, 1);
            }

            const auto ground_second_start = draw_trace.Start();
            TexManager.ReloadTexture(Vif1Packet, 2);
            TexAnime.TexAnime(2);
            pEditGround->Draw(NowTime, 2, far_lod, near_lod, 0, 3);
            draw_trace.Finish(GameDrawPhase::GroundSecond, ground_second_start);

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

            const auto water_start = draw_trace.Start();
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
            draw_trace.Finish(GameDrawPhase::Water, water_start);

            if (depth_of_field != 0) {
                const auto far_dof_start = draw_trace.Start();
                int        dof_level = 2;

                far_drawn = EdDrawBeyondRetailReach(far_marks.get(), draw_trace);

                const auto dof_start = draw_trace.Start();
                if ((static_cast<int *>(SaveData->GetConfigData()))[6] != 0) {
                    dof_level = 1;
                }

                EdDrawDOF(dof_level);
                draw_trace.Finish(GameDrawPhase::DofRecord, dof_start);
                draw_trace.Finish(GameDrawPhase::FarDof, far_dof_start);
            }
        }

        if (FishingDrawCheck() != 0) {
            FishLineDraw(1);
        }

        if (GameMode == ED_MODE_EVENT) {
            EdEventBackSpriteDraw();
        }

        const auto character_start = draw_trace.Start();
        PolyCount = 0;

        if (GameMode == ED_MODE_EVENT) {
            EdDrawItem();
        }

        switch (GameMode) {
            case ED_MODE_RETURN_MENU_WALK:
            case ED_MODE_MENU_INIT: {
                const auto step_start = draw_trace.Start();
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
                draw_trace.Finish(GameDrawPhase::CharacterStep, step_start);
            }
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

                const auto near_parts_start = draw_trace.Start();
                int        found = std::clamp(pEditGround->GetNearParts(near_parts.get(), near_parts_capacity, &box, nullptr), 0,
                                              near_parts_capacity);
                draw_trace.Finish(GameDrawPhase::CharacterNearParts, near_parts_start);

                const auto lights_start = draw_trace.Start();
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

                draw_trace.Finish(GameDrawPhase::CharacterLights, lights_start);
                const auto draw_start = draw_trace.Start();
                EdDrawCharacter(Chara, detail, 0xA, EdVillager, marks, shadow_on, event);
                draw_trace.Finish(GameDrawPhase::CharacterDraw, draw_start);
                break;
            }
            default:
                break;
        }

        draw_trace.Finish(GameDrawPhase::Character, character_start);

        if (EdDrawOffMapShadow == 0 && EdDrawOffMap == 0) {
            const auto shadows_start = draw_trace.Start();
            float      saved_light[4][4];
            float      shadow_light[4][4];
            float      light_colour[4][4];

            MGGetPLight(saved_light, light_colour);
            sceVu0CopyMatrix(shadow_light, saved_light);
            EdLimitShadowLight(shadow_light, 3.0f);
            MGSetPLight(shadow_light, light_colour);
            TexManager.ReloadTexture(Vif1Packet, 0x16);

            float shadow_range[4] = {-100.0f, EditMapInfo->shadow_near, EditMapInfo->shadow_far, 0.0f};

            if (edit_mode_draw != 0) {
                const auto shadow_georama_start = draw_trace.Start();
                MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
                pEditGround->DrawShadow(1, 0.0f, 100000.0f);
                MGEndDrawShadow(0x20);
                draw_trace.Finish(GameDrawPhase::ShadowGeorama, shadow_georama_start);
            } else {
                if (EditMapInfo->shadow_level > 0) {
                    const auto shadow_near_start = draw_trace.Start();
                    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
                    pEditGround->DrawShadow(0, shadow_range[0], shadow_range[1]);
                    MGEndDrawShadow(EditMapInfo->shadow_mode);
                    draw_trace.Finish(GameDrawPhase::ShadowNear, shadow_near_start);
                }

                if (EditMapInfo->shadow_level >= 2) {
                    const auto shadow_far_start = draw_trace.Start();
                    MGBeginDrawShadow(*reinterpret_cast<sceGsTex0 *>(&TexManager.GetTexture("shadow_buff", -1)->tex0));
                    pEditGround->DrawShadow(0, shadow_range[1], shadow_range[2]);
                    MGEndDrawShadow(EditMapInfo->shadow_mode_2);
                    draw_trace.Finish(GameDrawPhase::ShadowFar, shadow_far_start);
                }
            }

            MGSetPLight(saved_light, light_colour);
            draw_trace.Finish(GameDrawPhase::Shadows, shadows_start);
        }

        const auto motion_parts_start = draw_trace.Start();
        for (int i = 0; i < 4; i++) {
            TexManager.ReloadTexture(Vif1Packet, i + 0x1B);
            MotionParts[i].TextureAnime(i + 0x1B);
            MotionParts[i].Draw();
        }
        draw_trace.Finish(GameDrawPhase::MotionParts, motion_parts_start);

        if (EdDrawOffMap == 0) {
            const auto effects_start = draw_trace.Start();
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
            draw_trace.Finish(GameDrawPhase::Effects, effects_start);
        }

        const auto final_2d_start = draw_trace.Start();
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
        draw_trace.Finish(GameDrawPhase::Final2D, final_2d_start);
    }
}
} // namespace

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

    if (NowFocusParts != nullptr && DrawPartsNameCount == 0) {
        int name_count = PartsNameNum;

        if (name_count > 0) {
            int top = 0;
            int step = 0;
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
        if (CharaCursor2 != nullptr) {
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

namespace {

/**
 * Rectangle that starts zeroed, for the day overlay's texel cells.
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

} // namespace

void DrawDay() {
    if (draw_day_flag != 0) {
        int       week = 0;
        int       x = 0;
        int       width = 0;
        int       alpha = 0;
        int       day = 0;
        int       digit[4];
        CTexture *texture = nullptr;
        int       count = 0;
        int       drawn = 0;
        int       remain = 0;
        int       value = 0;

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
            alpha = static_cast<int>(128.0f * (5.0f - draw_day_cnt));
        } else if (draw_day_cnt > 1.0f) {
            alpha = 0x80;
        } else {
            alpha = static_cast<int>(128.0f * draw_day_cnt);
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

        if (texture != nullptr) {
            switch (LanguageCode) {
                case LANG_JAPANESE:
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, title.width, title.height), title, alpha);
                    x += 0x30;
                    break;
                case LANG_ENGLISH_US:
                case LANG_ENGLISH_UK:
                    width = week_word.width + (word_end.width + (word.width + ((count - 1) * 32)) + 0x48);
                    x = 0x140 - (width >> 1);
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
                    x = 0x140 - (width >> 1);
                    set2DSprite(GetVif1Packet(), texture, CRect_i_(x, 0xE4, word.width, word.height), word, alpha);
                    x += word.width + 0xC;
                    break;
            }

            for (int n = count; n > 0; n--) {
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
            x = 0x134 - (width >> 1);
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

/**
 * Selects the main character and advances normal character movement.
 */
void MainMode() {
    Chara = &MainChara;
    MoveChara();
}

namespace {

/**
 * Changes whether a named child frame participates in drawing.
 */
int FrameOnOff(CFrame *root, char *name, int on) {
    if (root == nullptr) {
        return 0;
    }

    if (name == nullptr) {
        return 0;
    }

    CFrame *frame = root->SearchFrame(name);

    if (frame == nullptr) {
        return 0;
    }

    frame->attr.draw_on = on;
    return 0;
}

} // namespace

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
    Chara->wind = static_cast<int>(reinterpret_cast<intptr_t>(&EdWind));

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

    if (static_cast<EDIT_CONFIG_VIEW *>(SaveData->GetConfigData())->unk_0c == 0) {
        time_step /= 2.0f;
    }

    if (change_time_event != 0) {
        time_step = 0.0f;
    }

    if (time_flag != 0 && GameMode == ED_MODE_WALK) {
        SaveData->AddNowTime(time_step);
    }

    if (GamePad.Down2(PAD_DOWN) != 0) {
        time_flag = static_cast<int>(time_flag == 0);
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
        NowTime = 1.0f + static_cast<float>(static_cast<int>(NowTime));

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

    int level = 0;
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

            if (timer->object != nullptr) {
                timer->object->FrameObjectOnOff(timer->name, on);
            } else if (timer->frame != nullptr) {
                CFrame *found = timer->frame->SearchFrame(timer->name);

                if (found != nullptr) {
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
    int i = 0;
    int event_no = 0;

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
            GotoInterior(EdEventInfo.interior_name, EdEventInfo.interior_entrance, 0, nullptr, EdEventInfo.interior_start_event);
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
    int count = 0;

    for (int i = 0; i < 14; i++) {
        SV_EDIT_PARTS_INFO *plot = SaveData->GetEditPartsInfo(4, i);

        if (plot != nullptr) {
            plot->obtained = 0;
            plot->placed = false;
            plot->stock = 0;
        }
    }

    s16 *elements = SaveData->GetElemData(4);

    if (elements != nullptr) {
        for (int i = 0; i < 128; i++) {
            elements[i] = -1;
        }
    }

    SV_GRD_PART *parts = SaveData->GetParts(4, &count);

    for (int i = 0; i < count; i++) {
        parts[i].part_id = -1;
    }
}

/**
 * Loads the interior a door leads to and places the player and camera in it.
 */
int GotoInterior(char *name, int entrance, int direction, ED_EVENT_PARAM *param, int start_event) {
    constexpr std::string_view suffix[5] = {"m", "e", "n", "m", ""};
    ED_EVENT_PARAM             entry;
    sceVu0FVECTOR              position;
    sceVu0FVECTOR              rotation;

    int period = EdGetTime(NowTime);
    CopyString(EdInteriorName, name);
    CopyString(interior_map_name, std::format("{}in/{}/{}{}", EditDataDir, name, name, suffix[period]));
    EdInteriorPartsNo = entrance;
    EdNPCBuffer.used = 0;

    if (param == nullptr) {
        direction = 0;
    }

    EdInteriorJumpID = 0;

    if (param != nullptr) {
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
            if (std::string_view(param->point->destination) == "dungeon") {
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

            if (parts != nullptr) {
                int             plot = parts->parts_no;
                EDITPARTS_INFO *plot_info = EditPartsInfo.GetPartsInfo(plot);

                if (plot_info != nullptr && plot_info->kind == 2 && !(plot_info->completion_flags & 1) && EditPartsInfo.CheckComplete(plot) != 0) {
                    FadeOutToEvent(plot + 200, 0);
                }
            }
        }

        if (GamePad.Down(PAD_CROSS) != 0) {
            Chara->GetPosition(pos);
            CMapParts *parts = pEditGround->GetParts(pos[0], pos[1], pos[2]);

            if (parts != nullptr) {
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
        int           snap_to_ground = 0;
        CCharacter   *chara = nullptr;
        CNPCharacter *villager = nullptr;
        int           foot_sound = 0;

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

        WorkBuffer->used = 0;
        auto *polys = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(2000));
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
            std::printf("cpoly over!!!! %d\n", poly_count);
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
    WorkBuffer->used = 0;
    auto *polys = reinterpret_cast<CCPoly *>(WorkBuffer->Alloc(1024));
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
                float drop = 0.0f;
                drop = hit[1] - alt;
                drop = drop < 0.0f ? -drop : drop;

                if (drop < 0.10000000149011612f && area >= 0) {
                    pos[1] = 1.0f + hit[1];
                } else {
                    walkable = 0;
                }

                CMapParts *parts = pEditGround->GetParts(pos[0], pos[1], pos[2]);

                if (parts != nullptr && parts->subtype == MAP_PARTS_SUBTYPE_ROAD) {
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

    float step_z = 0.0f;
    float camera_angle = 0.0f;
    float step_x = 0.0f;
    float forward = 0.0f;
    float right = 0.0f;

    camera_angle = EditCamera.GetAngle();
    right = EdGetLXf(2);
    forward = EdGetLYf(2);

    sceVu0CopyVector(pos, ECursorFrame->position);

    int        area_code = pEditGround->GetAreaCode(pos[0], pos[1], pos[2]);
    CEditArea *area = nullptr;

    if (EdPadDown(0xF000, 2) != 0) {
        if (area_code >= 0) {
            area = pEditGround->areas[area_code];
        }

        if (area != nullptr) {
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

    if (area != nullptr) {
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

    if (area != nullptr) {
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

    if (PI_SHORT * static_cast<float>(NowSelectAngle) / 2.0f == NowCursorRotY) {
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

    NowCursorRotY = AngleInterpolate(NowCursorRotY, PI_SHORT * static_cast<float>(NowSelectAngle) / 2.0f, 0.1f, INTERPOLATE_STEP);

    int menu_mode = EditMenuStatus.mode;

    OldFocusParts = NowFocusParts;
    NowFocusParts = pEditGround->GetParts(pos[0], pos[1], pos[2]);

    if (static_cast<u_int>(EditMenuStatus.mode) < 2 && EdPadDown(0x40, 2) != 0) {
        int sound_id = 13;

        if (MapNo == TOWN_YELLOW_DROPS) {
            sound_id = 333;
        }

        int plot = pEditGround->SetMapParts(NowSelectParts, parts_pos[0], parts_pos[1], parts_pos[2], NowSelectAngle);

        if (plot >= 0) {
            pEditGround->SetBuildEffect(plot);
            SndSePlay(sound_id, -1, 0);

            EDITPARTS_INFO *info = EditPartsInfo.GetPartsInfo(NowSelectParts);

            if (EditMenuStatus.mode == EDIT_MENU_MODE_REPLACE || (info != nullptr && info->stock > 0 && info->placed == info->stock)) {
                menu_mode = EDIT_MENU_MODE_MOVE;
                NowSelectParts = -1;
                NowSelectAngle = 0;
                OldSelectAngle = 0;
            }
        }
    }

    if (EditMenuStatus.mode == EDIT_MENU_MODE_MOVE) {
        int deleted_parts = 0;
        int deleted_angle = 0;

        if (EdPadDown(0x80, 2) != 0 && pEditGround->DeleteMapParts(&deleted_parts, &deleted_angle, parts_pos[0], parts_pos[1], parts_pos[2]) >= 0) {
            EdSePlay(static_cast<ED_SOUND_ID>(0xE), -1);
        }

        pEditGround->SetFocusParts(parts_pos[0], parts_pos[1], parts_pos[2]);

        if (EdPadDown(0x40, 2) != 0) {
            int taken_parts = 0;
            int taken_angle = 0;

            if (NowFocusParts != nullptr) {
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
                NowCursorRotY = PI_SHORT * static_cast<float>(taken_angle) / 2.0f;
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
        pEditGround->Save(nullptr);
    }

    if (GamePad.Down2(PAD_TRIANGLE) != 0) {
        pEditGround->Load(nullptr);
    }

    if (pEditGround->GetAreaCode(pos[0], pos[1], pos[2]) >= 0) {
        if (pos[1] < pEditGround->GetAlt(pos[0], pos[1], pos[2])) {
            pos[1] = pEditGround->GetAlt(pos[0], pos[1], pos[2]);
        }
    }

    ECursorFrame->SetPosition(pos[0], pos[1], pos[2]);
    sceVu0CopyVector(NextCursorPos, pos);

    if (NowFocusParts != nullptr) {
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

    if (static_cast<u_int>(EditMenuStatus.mode) < 2) {
        EdEditBuildHelpMes(NowSelectParts);
    }

    if (EditMenuStatus.mode == EDIT_MENU_MODE_MOVE) {
        EdEditMoveHelpMes();
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

    // Room for the zeroed entry that ends either list.
    auto blocks = std::make_unique<LOADTEXTURE_INFO2[]>(std::max(image_count, std::size(system_blocks)) + 1);
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

u_int *scn_data;
u_int *parts_read_buffer;

namespace {

/**
 * A part already built while the map loads, so later copies can share it.
 */
struct LOADED_PARTS {
    std::string_view name;  /**< Resource name the part was built from. */
    CMapParts       *parts; /**< Part built from that name. */
};

} // namespace

/**
 * Loads the player's model and motions into the arena the caller names, or
 * into the character arena when it names none.
 */
void EdLoadMainChara(char *pack_path, char *info_name, CDataAlloc2<1> *arena) {
    int i = 0;

    if (arena == nullptr) {
        arena = &CharaBuffer;
        CharaBuffer.used = 0;
    }

    LoadFile(pack_path, read_buffer, nullptr);
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
 */
void LoadGroundData() {
    parts_read_buffer = read_buffer;

    std::string pack_path = std::format("{}mds.pak", EditDataDir);

    LoadPackFile(pack_path.data(), parts_read_buffer, nullptr);
    wait_now_loading_vsync();

    u_int             *data = nullptr;
    OBJ_ANIME_SEQ     *anime = EditMapInfo->work.obj_anime;
    EDIT_EFFECT_INFO  *effects = EditMapInfo->work.effects.first;
    EDIT_OBJECT_TIMER *timers = EditMapInfo->work.object_timers.timers;
    ED_EVENT_POINT    *points = EditMapInfo->work.events.points;

    pEditGround->Initialize();
    pEditGround->map_no = NowEditMap;

    char *sun_names[4] = {"sun1", "sun2", "sun3", "moon"};

    for (int i = 0; i < 4; i++) {
        SkyFrame[i] = nullptr;

        CFrameAttr layer_attr;

        layer_attr.depth_write = false;

        if (EditMapInfo->sky_layers[i].name[0] != '\0') {
            data = static_cast<u_int *>(EdLoadFile(EditMapInfo->sky_layers[i].name));

            SkyFrame[i] = LoadMDSFile(data, &DataBuffer__2, 0, nullptr, nullptr);
            SkyFrame[i]->SetAttr(layer_attr, 1, 0x1000);

            for (int j = 0; j < 8; j++) {
                short no = EditMapInfo->sky_layers[i].obj_anime[j];

                if (no > 0) {
                    InitObjAnime(SkyFrame[i], &anime[no]);
                }
            }
        }

        SunFrame[i] = nullptr;

        if (EditMapInfo->sun_layers[i].name[0] != '\0') {
            data = static_cast<u_int *>(EdLoadFile(EditMapInfo->sun_layers[i].name));

            SunFrame[i] = LoadMDSFile(data, &DataBuffer__2, 0, nullptr, nullptr);
            SunFrame[i]->SetAttr(layer_attr, 1, 0x1000);
        }

        if (SunFrame[i] != nullptr) {
            sceVu0FMATRIX world;

            sceVu0UnitMatrix(world);
            SunFrame[i]->SetTransMatrix(world);
            SunFrame[i]->SetRotation(0.5f, 0.0f, 0.0f);
            SunFrame[i] = SunFrame[i]->SearchFrame(sun_names[i]);
        }
    }

    SkyBackFrame = nullptr;

    if (EditMapInfo->sky_layers[4].name[0] != '\0') {
        data = static_cast<u_int *>(EdLoadFile(EditMapInfo->sky_layers[4].name));

        CFrameAttr back_attr;

        back_attr.depth_write = false;
        back_attr.use_color = true;
        SkyBackFrame = LoadMDSFile(data, &DataBuffer__2, 6, nullptr, nullptr);
        SkyBackFrame->SetAttr(back_attr, 1, 0x1200);
    }

    for (int i = 0; i < 4; i++) {
        EDIT_AREA_INFO *info = &EditMapInfo->parts_work.edit_areas[i];

        pEditGround->areas[i] = nullptr;

        if (info->name[0] != '\0') {
            data = static_cast<u_int *>(EdLoadFile(info->name));
            EditArea[i].SetMapInfo(NowEditMap, i);
            EditArea[i].SetSize(info->width, info->height, info->unit_size, info->unit_alt);
            EditArea[i].SetOffset(info->origin_x - 0.01f * static_cast<float>(info->width), info->origin_y, info->origin_z - 0.01f * static_cast<float>(info->height));
            EditArea[i].SetGridFrame(LoadMDSFile(data, &DataBuffer__2, 6, nullptr, nullptr));

            CFrameAttr grid_attr;

            grid_attr.fog_enable = true;
            EditArea[i].GetGridFrame()->SetAttr(grid_attr, 1, 0x40);
            pEditGround->areas[i] = &EditArea[i];
        }
    }

    if (MapNo < 6) {
        data = static_cast<u_int *>(EdLoadFile("gedit/e01/mds/e01a03_0.mds"));

        pEditGround->cursor.pieces[0] = LoadMDSFile(data, &DataBuffer__2, 0, nullptr, nullptr);

        data = static_cast<u_int *>(EdLoadFile("gedit/e01/mds/e01a04_0.mds"));

        pEditGround->cursor.pieces[1] = LoadMDSFile(data, &DataBuffer__2, 0, nullptr, nullptr);

        data = static_cast<u_int *>(EdLoadFile("gedit/e01/mds/e01a05_0.mds"));

        pEditGround->cursor.pieces[2] = LoadMDSFile(data, &DataBuffer__2, 0, nullptr, nullptr);
        pEditGround->cursor.area = 2;
    } else {
        pEditGround->cursor.pieces[0] = nullptr;
        pEditGround->cursor.pieces[1] = nullptr;
        pEditGround->cursor.pieces[2] = nullptr;
    }

    wait_now_loading_vsync();

    for (int i = 0; i < 4; i++) {
        EDIT_MOTION_PARTS_INFO *info = &EditMapInfo->motion_parts[i];

        MotionParts[i].Initialize();

        if (info->name[0] != '\0') {
            data = static_cast<u_int *>(EdLoadFile(info->name));
            MotionParts[i].InitializeTexAnime(TexAnimeData, 64);
            MotionParts[i].LoadPackData2(data, "info.cfg", &DataBuffer__2, i + 0x1B, &DataBuffer__2, 0);
            MotionParts[i].SetPosition(info->values[0], info->values[1], info->values[2]);
            MotionParts[i].SetRotation(info->values[3], info->values[4], info->values[5]);
        }
    }

    scn_data = reinterpret_cast<u_int *>(&EdNPCBuffer.base[static_cast<ptrdiff_t>(EdNPCBuffer.used) * 16]);

    std::string scene_path = std::format("{}{}", EditDataDir, EditMapInfo->scene_name);
    int         scene_size = 0;

    if (LoadFile2(scene_path.data(), scn_data, &scene_size, 0) == 0) {
        scn_data = nullptr;
        scene_size = 0;
    }

    if (scn_data != nullptr) {
        parts_read_buffer = reinterpret_cast<u_int *>(&reinterpret_cast<u8 *>(scn_data)[((scene_size >> 6) + 1) << 6]);
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

    std::vector<LOADED_PARTS> built;
    int                       i = 0;

    for (i = 0;; i++) {
        MAP_PARTS_INFO *info = &EditMapInfo->map_objects[i];

        if (info->name[0][0] == '\0' && info->name[1][0] == '\0' && info->name[2][0] == '\0' && info->name[3][0] == '\0') {
            break;
        }

        CMapParts *parts = &pEditGround->fixed_parts[i];

        parts->Initialize();

        u_int *pts = SearchPTS(scn_data, info->name[0]);

        if (pts != nullptr) {
            CMapParts *shared = nullptr;

            for (const LOADED_PARTS &loaded : built) {
                if (loaded.name == info->name[0]) {
                    shared = loaded.parts;
                    break;
                }
            }

            LoadPTS(parts, pts, info, anime, effects, timers, points, shared);

            std::string_view base = info->name[0];

            if (const auto slash = base.rfind('/'); slash != std::string_view::npos) {
                base.remove_prefix(slash + 1);
            }

            if (base == "e02c13") {
                CFrameAttr door_attr;

                door_attr.fog_enable = true;

                CFrame *door = parts->ripple_frame;

                if (door != nullptr) {
                    door = door->SearchFrame("cube1");
                }

                if (door != nullptr) {
                    door->SetAttr(door_attr, 1, 0x40);
                }
            }

            sceVu0FVECTOR position = {info->position[0], info->position[1], info->position[2],
                                      1.0f};

            parts->SetPosition(position);
            parts->SetRotation(info->rotation[0], info->rotation[1], info->rotation[2]);

            if (shared == nullptr) {
                built.push_back({.name = info->name[0], .parts = parts});
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
        CopyString(surface->name, info->name);
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
    std::vector<LOADED_PARTS> loaded;

    OBJ_ANIME_SEQ     *anime = EditMapInfo->work.obj_anime;
    EDIT_EFFECT_INFO  *effects = EditMapInfo->work.effects.second;
    EDIT_OBJECT_TIMER *timers = EditMapInfo->work.object_timers.timers;
    ED_EVENT_POINT    *points = EditMapInfo->work.events.points;

    int river = 0;
    int road = 0;

    for (int i = 0; i < 46; i++) {
        CMapParts      *shared = nullptr;
        int             has_archive = 0;
        CMapParts      *parts = nullptr;
        MAP_PARTS_INFO *info = &EditMapInfo->parts_work.general.parts[i];
        EDITPARTS_INFO *plot = nullptr;
        int             index = i;
        int             slot = 0;

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

        for (const LOADED_PARTS &entry : loaded) {
            if (entry.name == info->name[0]) {
                shared = entry.parts;
                break;
            }
        }

        u_int *archive = SearchPTS(scn_data, info->name[0]);

        if (archive != nullptr) {
            has_archive = 1;
        }

        parts->Initialize();

        if (plot == nullptr) {
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

        EPARTS_INFO_HEADER *header = nullptr;

        if (has_archive != 0) {
            header = LoadPTS(parts, archive, info, anime, effects, timers, points, shared);
        } else {
            if (plot != nullptr) {
                header = reinterpret_cast<EPARTS_INFO_HEADER *>(&EPartsInfoBuff.base[static_cast<ptrdiff_t>(EPartsInfoBuff.used) * 16]);
                EPartsInfoBuff.Alloc((EdInitToEPInfo(reinterpret_cast<INIT_PARTSINFO *>(&EditMapInfo->parts_work.parts_defs.defs[i]), header) >> 4) + 1);
            }

            LoadPTS(parts, info, anime, effects, timers, points);
        }

        if (plot != nullptr) {
            EditPartsInfo.Initialize(i, header);
        }

        parts->info = EditPartsInfo.GetPartsInfo(index);

        for (int level = 3; level >= 0; level--) {
            if (parts->frame[level] != nullptr) {
                parts->preview_frame = static_cast<int>(reinterpret_cast<intptr_t>(parts->frame[level]));
                break;
            }
        }

        if (shared == nullptr) {
            loaded.push_back({.name = info->name[0], .parts = parts});
        }
    }
}

namespace {

/**
 * Puts a map part's camera collision frame where the part stands and returns it.
 */
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

// The .pts definition is decoded into host records (editpartsinfo.hpp) rather than relocated in place.
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

/**
 * Builds one map part that has no part-definition file: its models are loaded
 * straight from the names the script gave, and its bound comes from them.
 */
void LoadPTS(CMapParts *parts, MAP_PARTS_INFO *info, [[maybe_unused]] OBJ_ANIME_SEQ *anime, [[maybe_unused]] EDIT_EFFECT_INFO *effects, [[maybe_unused]] EDIT_OBJECT_TIMER *timers, [[maybe_unused]] ED_EVENT_POINT *points) {
    char      *names[9];
    CFrameVu1 *frames[4];
    CBoxVu0    bound;
    CBoxVu0    part_bound;

    if (info == nullptr || info->name[0][0] == '\0') {
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

    if (frames[0] != nullptr) {
        frames[0]->GetBoundBox(&bound, 1);
        CFrame *collision = parts->GetCollisionFrame();

        if (collision != nullptr) {
            collision->GetBoundBox(&part_bound, 1);
            VectorMaxMin(bound.max, bound.min, bound.max, bound.min, part_bound.max, part_bound.min);
        }

        CFrame *camera_frame = GetCameraFrame(parts);

        if (camera_frame != nullptr) {
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
    to->shadow_frame = CopyFrameVu1(static_cast<CFrameVu1 *>(from->shadow_frame), arena);
    to->shade_frame = CopyFrameVu1(static_cast<CFrameVu1 *>(from->shade_frame), arena);
    to->camera_frame = CopyFrame(GetCameraFrame(from), arena);
    to->ripple_frame = CopyFrameVu1(static_cast<CFrameVu1 *>(from->ripple_frame), arena);
}

/**
 * Derives an object's world position and normalized yaw from its first frame.
 */
int GetPosRot(CMapObject *object, float *position, float *rotation) {
    if (object == nullptr) {
        return 0;
    }

    CFrame *frame = object->frame[0];

    if (frame == nullptr) {
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

    return nullptr;
}

/**
 * Says whether one event point is ready to fire at the given time of day.
 */
int CheckEventPoint(ED_EVENT_POINT *point, float time) {
    if (point == nullptr) {
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

    if (frame != nullptr && !(frame->attr.draw_on & 1)) {
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

} // namespace dc::game
