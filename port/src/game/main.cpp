#include "main.hpp"

#include <libmc.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "btsysscript.hpp"
#include "character.hpp"
#include "clsmes.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "debugfont.hpp"
#include "dungeonmap.hpp"
#include "editloop.hpp"
#include "fakepointlight.hpp"
#include "frame.hpp"
#include "gamemode.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "hitmark.hpp"
#include "literals.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "object.hpp"
#include "platform/arena.hpp"
#include "rect.hpp"
#include "runscript.hpp"
#include "savedata.hpp"
#include "shot_effect.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "sysmes.hpp"
#include "texture.hpp"
#include "textureanime.hpp"
#include "title/bombeffect.hpp"
#include "title/majinbeem.hpp"
#include "title/title.hpp"
#include "visualvu1.hpp"
#include "vudata.hpp"
#include "vutext.hpp"
#include "water.hpp"

namespace dc::game {

namespace {

/**
 * Steps of the memory card check run before the title.
 */
enum class MemCheckMode : int {
    Check = 0,   /**< Checks the memory cards once the cancel delay has run out. */
    Unk1 = 1,    /**< Never set; does nothing. */
    NoCard = 2,  /**< No usable PS2 memory card; shows the first warning. */
    NoSpace = 3, /**< No card has room for a save; shows the second warning. */
    Done = 4,    /**< The check is over. */
};

} // namespace

/**
 * Holds the category's level-of-detail thresholds and trailing state.
 */
class CategoryAttr {
public:
    float lod[4];  /**< Distances at which the category changes level of detail. */
    int   lowest;  /**< Lowest level of detail that the category may draw. */
    int   highest; /**< Highest level of detail that the category may draw. */

    /**
     * Constructs the category attributes with their starting values.
     */
    CategoryAttr();

    /**
     * Resets the category's thresholds and trailing state.
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(CategoryAttr) == 0x18);

namespace {

CRunScript RunScript;

} // namespace

CDataAlloc<1, 6000> SystemMesBuffer;

namespace {

// Never read, but its constructor hands out the starting dagger through the shared item
// code at startup, and later game state depends on that side effect.
// init_priority keeps this static's place in the order rand() is drawn from; see app/runtime.cpp.
CSaveData save_data __attribute__((aligned(64), init_priority(65008)));

} // namespace

CSaveData *SaveData;

s32    GameClearFlag;
s32    main_select_menu_no;
s32    MapNo;
s32    OldMapNo;
s32    LocalMapNo;
s32    StartEventNo;
short *SystemMes;
CSound CSnd;
s32    main_select_padrup;

namespace {

CDebugFont DebugFont;

CDataAlloc2<1> mainCDataAlloc2;

} // namespace

/** Top-level game mode currently being updated. */
s32 mode;

namespace {

sceDmaChan *d1;
sceDmaChan *d2;
sceDmaChan *d8;

} // namespace

/** Memory-card operation selected by the developer menu. */
s32 mc_mode;

s32 PolyCount;

/**
 * Selects the next map that the game loads.
 */
s32 NextMapNo = -1;

namespace {

/** Enables accumulation of the save file's play-time counter. */
int play_time_count = 1;

} // namespace

void LoadOverlay(int mode) {
    enum class Overlay {
        None,
        Title,
        Dungeon
    };
    constexpr std::array overlay_modes = {Overlay::Title, Overlay::Title, Overlay::None, Overlay::Dungeon, Overlay::Dungeon,
                                          Overlay::Title, Overlay::None, Overlay::None, Overlay::Dungeon, Overlay::Dungeon,
                                          Overlay::None, Overlay::None, Overlay::None, Overlay::None, Overlay::None};
    static Overlay       loaded = Overlay::None;
    if (mode < 0 || std::cmp_greater_equal(mode, overlay_modes.size()) || overlay_modes[mode] == Overlay::None ||
        overlay_modes[mode] == loaded) {
        return;
    }
    loaded = overlay_modes[mode];
    if (loaded == Overlay::Title) {
        TitleOverlayConstruct();
    }
}

void init_all() {
    InitCDFile();
    DevInit();
    d1 = sceDmaGetChan(1);
    d2 = sceDmaGetChan(2);
    d8 = sceDmaGetChan(8);
    MGInit();
    InitMemoryFile();
    BufferAllClear();
    InitReadBG();
}

sceVu0FMATRIX light = {
    {0.578f, 0.0f, 0.0f, 0.0f},
    {0.578f, 0.0f, 0.0f, 0.0f},
    {0.578f, 0.0f, 0.0f, 0.0f},
    {0.0f,   0.0f, 0.0f, 0.0f},
};

sceVu0FMATRIX lightcolor = {
    {120.0f, 120.0f, 120.0f, 128.0f},
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  },
};

sceVu0FVECTOR ambientlight = {64.0f, 64.0f, 64.0f, 128.0f};

void SetEnv(sceVif1Packet * /*packet*/) {
    MGSetGsTEST(nullptr);
    MGSetGsZBUF(nullptr);
    MGSetGsALPHA(nullptr);
    MGSetWindowRect();
}

void LoadSndTxt() {
    constexpr std::size_t work_size = 48000;
    auto                  work = std::make_unique_for_overwrite<u_char[]>(work_size);
    void                 *aligned = work.get();
    std::size_t           space = work_size;
    std::align(64, 1, aligned, space);
    auto *buffer = static_cast<u_int *>(aligned);

    CSnd.LoadSeInf("sound/tbl/setbl.txt", buffer);
    CSnd.LoadSqInf("sound/tbl/sqtbl.txt", buffer);
    SndInitSeTable();
}

void PlayTimeCountFlag(int flag) {
    play_time_count = flag;
}

int PlayTimeCount(int /*unused*/) {
    if (play_time_count) {
        SaveData->AddPlayTime(1);
    }
    return play_time_count;
}

void LoadSystemMessage() {
    int size = 0;
    SystemMesBuffer.used = 0;
    SystemMesBuffer.Align64();
    SystemMes = reinterpret_cast<short *>(&SystemMesBuffer.block[SystemMesBuffer.used]);
    std::string name = "meswin/system";

    if (LanguageCode > LANG_JAPANESE) {
        name = std::format("meswin/system_{}", LanguageCode);
    }

    name += ".mes";

    if (!LoadFile2(name.data(), SystemMes, &size, 0)) {
        LoadFile("meswin/systeme.bin", SystemMes, &size);
    }

    SystemMesBuffer.Alloc((size >> 4) + 1);
    InitSystemMes();
}

void initialize_data() {
    LoadSystemMessage();
    BtMapJumpFloor = -1;
    LoadSndTxt();
}

namespace {

int edit_map;
int sub_map;
int event_no;

/**
 * Appends formatted text to the developer menu's overlay, keeping it null-terminated.
 */
template <typename... Args>
void AppendDebugText(std::format_string<Args...> format, Args &&...args) {
    std::span<char> free_text = std::span(DebugFont.text).subspan(DebugFont.length);
    auto            result = std::format_to_n(free_text.data(), static_cast<std::ptrdiff_t>(free_text.size() - 1),
                                              format, std::forward<Args>(args)...);
    *result.out = '\0';
    DebugFont.length += static_cast<int>(result.size);
}

} // namespace

void MapJump(int map_no, int event_no) {
    NextMapNo = map_no;
    StartEventNo = event_no;
}

void MenuInit() {
    static LOADTEXTURE_INFO texdata[64] = {{"#frame_buff#640#" HALF_BUFFER_HEIGHT_STR "#4"},
                                           {"img/ankfont.img"},
                                           {""}};
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 100000);
    SetPacketReadBuffer(10000, 100000);
    TexManager.Initialize(16352);
    TexManager.EnterTextureFile(texdata);
    TexManager.LoadTextureBlock(-1, read_buffer);
    MGSetBGColor(0.0f, 0.0f, 128.0f, 128.0f);
    DebugFont.texture_name = "frame_buff";
    DebugFont.x = 16;
    DebugFont.y = 16;
    DebugFont.width = 256;
    DebugFont.height = SCREEN_HALF_HEIGHT;
    DebugFont.alpha = 64;
    GamePad.SetAutoRepeat(PAD_DPAD, 25, 3);
}

/**
 * Draws and operates the developer's top-level map and mode selector.
 */
int MenuLoop() {
    // Rows 1, 2, 6, 7 and 8 are followed by the value they adjust.
    constexpr std::array<std::string_view, 9> labels = {
        "game start", "e0", "s", "interior", "dungeon", "opening", "event", "memory card ", "Language "};
    constexpr std::array<int, 9>              map_no = {800, 0, 11, 99, 200, 400, 0, 0, 0};
    constexpr std::array<std::string_view, 2> cursor = {" ", ">"};
    constexpr int                             row_count = labels.size();

    DebugFont.length = 0;

    static int selected_row = 0;

    if (GamePad.Down(PAD_DOWN)) {
        selected_row++;
    }

    if (GamePad.Down(PAD_UP)) {
        selected_row--;
    }

    selected_row = std::clamp(selected_row, 0, row_count - 1);

    if (selected_row == 1) {
        if (GamePad.Down(PAD_RIGHT)) {
            edit_map++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            edit_map--;
        }

        edit_map = std::clamp(edit_map, 0, 4);
    }

    if (selected_row == 2) {
        if (GamePad.Down(PAD_RIGHT)) {
            sub_map++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            sub_map--;
        }

        if (GamePad.Down(PAD_R1)) {
            sub_map += 10;
        }

        if (GamePad.Down(PAD_L1)) {
            sub_map -= 10;
        }

        if (sub_map < 0) {
            sub_map = 98;
        }

        if (sub_map > 98) {
            sub_map = 0;
        }
    }

    if (selected_row == 6) {
        if (GamePad.Down(PAD_RIGHT)) {
            event_no++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            event_no--;
        }

        if (GamePad.Down(PAD_R1)) {
            event_no += 10;
        }

        if (GamePad.Down(PAD_L1)) {
            event_no -= 10;
        }

        if (event_no < 0) {
            event_no = 2;
        }

        if (event_no > 2) {
            event_no = 0;
        }
    }

    if (selected_row == 7) {
        if (GamePad.Down(PAD_RIGHT)) {
            mc_mode++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            mc_mode--;
        }

        if (mc_mode < 0) {
            mc_mode = SAVE_MENU_MODE_ENDING_NO_CLEAR;
        }

        if (mc_mode > 3) {
            mc_mode = SAVE_MENU_MODE_LOAD;
        }
    }

    if (selected_row == 8) {
        if (GamePad.Down(PAD_RIGHT)) {
            LanguageCode++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            LanguageCode--;
        }

        LanguageCode = std::clamp<s32>(LanguageCode, LANG_JAPANESE, LANG_SPANISH);
    }

    AppendDebugText("Dark Cloud Ver3.07 2001/07/12\n");
    AppendDebugText("{}{}\n", cursor[selected_row == 0], labels[0]);
    AppendDebugText("{}{}{}\n", cursor[selected_row == 1], labels[1], edit_map + 1);
    AppendDebugText("{}{}{}\n", cursor[selected_row == 2], labels[2], sub_map + 1);

    for (int row = 3; row < 6; row++) {
        AppendDebugText("{}{}\n", cursor[row == selected_row], labels[row]);
    }

    AppendDebugText("{}{}{}\n", cursor[selected_row == 6], labels[6], event_no);
    AppendDebugText("{}{}{}\n", cursor[selected_row == 7], labels[7], mc_mode);
    AppendDebugText("{}{}{}\n", cursor[selected_row == 8], labels[8], LanguageCode);

    TexManager.ReloadTexture(GetVif1Packet(), 0);
    DebugFont.Draw();

    if (GamePad.Down(PAD_CIRCLE) || GamePad.Down(PAD_TRIANGLE)) {
        constexpr std::array<int, 3> event_maps = {23, 41, 19};
        constexpr std::array<int, 3> event_numbers = {310, 150, 305};

        main_select_padrup = 0;

        if (GamePad.Down(PAD_TRIANGLE)) {
            main_select_padrup = 1;
        }

        OldMapNo = -1;
        NextMapNo = -1;
        GamePad.AutoRepeatOff();

        main_select_menu_no = map_no[selected_row];
        mode = GAME_MODE_EDIT;

        if (selected_row == 1) {
            main_select_menu_no = edit_map;
            LocalMapNo = edit_map;
        }

        if (selected_row == 2) {
            main_select_menu_no = sub_map + 11;
            LocalMapNo = main_select_menu_no;
        }

        if (selected_row == 6) {
            MapJump(event_maps[event_no], event_numbers[event_no]);
            return 1;
        }

        if (selected_row == 7) {
            mode = GAME_MODE_SAVE;
            return 1;
        }

        MapNo = main_select_menu_no;

        if (main_select_menu_no == 200) {
            main_select_menu_no = 0;
            mode = GAME_MODE_LOADER;
        }

        if (main_select_menu_no == 800) {
            constexpr std::string_view title_param = "title";
            MapNo++;
            main_select_menu_no = 0;
            std::ranges::copy(title_param, main_select_param);
            main_select_param[title_param.size()] = '\0';
            mode = GAME_MODE_RUSH_MOVIE;
        }

        if (main_select_menu_no == 400) {
            main_select_menu_no = 0;
            mode = GAME_MODE_OPENING;
        }

        return 1;
    }

    return 0;
}

namespace {

MemCheckMode mem_chk_mode;
int          check_cancel;
int          taiken_cnt;
int          taiken_start;

/** Path of the memory-card warning images, which the texture table points into. */
std::string memory_image_path;

/**
 * Draws one memory-card warning and handles its retry and continue buttons.
 */
void MemCheckWarning(char *texture_name) {
    // The warning images are drawn at NTSC height and stretched over the taller frame.
    CRect_i_ rect;
    CRect_i_ texel;
    texel.x = 0;
    texel.y = 0;
    texel.width = 640;
    texel.height = 448;
    rect.x = 0;
    rect.y = 0;
    rect.width = 640;
    rect.height = SCREEN_HEIGHT;
    set2DSprite(GetVif1Packet(), TexManager.GetTexture(texture_name, -1), rect, texel);

    if (GamePad.Down(PAD_CIRCLE)) {
        mem_chk_mode = MemCheckMode::Check;
        check_cancel = 3;
    }

    if (GamePad.Down(PAD_CROSS)) {
        mem_chk_mode = MemCheckMode::Done;
    }
}

} // namespace

void MemCheckInit() {
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 100000);
    SetPacketReadBuffer(10000, 100000);
    memory_image_path = "img/memory.img";

    if (LanguageCode > LANG_JAPANESE && LanguageCode < LANG_COUNT) {
        memory_image_path = std::format("img_{}/memory.img", LanguageCode);
    }

    TexManager.Initialize(16352);
    static LOADTEXTURE_INFO texdata[64] = {{nullptr}, {""}};
    texdata[0].name = memory_image_path.data();
    TexManager.EnterTextureFile(texdata);
    TexManager.LoadTextureBlock(-1, read_buffer);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 0.0f);

    if (sceMcInit()) {
        std::println("libmc initialize faild");
    }

    mem_chk_mode = MemCheckMode::Check;
    check_cancel = 2;
    taiken_cnt = 0;
    taiken_start = 0;
}

int MemCheckLoop() {
    DebugFont.length = 0;
    TexManager.ReloadTexture(GetVif1Packet(), 0);

    switch (mem_chk_mode) {
        case MemCheckMode::Check:
            if (check_cancel > 0) {
                break;
            }

            switch (SaveEnableCheck()) {
                case SAVE_ENABLE_OK:
                    return 1;
                case SAVE_ENABLE_NO_CARD:
                    mem_chk_mode = MemCheckMode::NoCard;
                    break;
                case SAVE_ENABLE_NO_SPACE:
                    mem_chk_mode = MemCheckMode::NoSpace;
                    break;
                default:
                    break;
            }

            break;
        case MemCheckMode::Unk1:
            break;
        case MemCheckMode::NoCard:
            MemCheckWarning("memory01");
            break;
        case MemCheckMode::NoSpace:
            MemCheckWarning("memory02");
            break;
        case MemCheckMode::Done:
            return 1;
    }

    check_cancel = std::max(check_cancel - 1, 0);

    return 0;
}

void InitSave() {
    static LOADTEXTURE_INFO texdata[64] = {
        {"#frame_buff#640#" SCREEN_HEIGHT_STR "#4",         1 },
        {"#mes_frame_buff#640#" SCREEN_HEIGHT_STR "#4",     26},
        {"#fukidashibase#640#" HALF_BUFFER_HEIGHT_STR "#4", 26},
        {"#fontbase#512#256#1",                             26},
        {"#fuki256#128#128#1",                              26},
        {"meswin/gaiji.img",                                26}
    };
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 100000);
    SetPacketReadBuffer(10000, 100000);
    TexManager.Initialize(16352);
    TexManager.EnterTextureFile(texdata);
    TexManager.LoadTextureBlock(-1, read_buffer);
    u_int *buffer = read_buffer;
    LoadFileMenuData(const_cast<char *>("stayframe.img"), buffer);
    TexManager.EnterFixTextureZ(reinterpret_cast<u_char *>(read_buffer));
    StayTexture = TexManager.GetTexture("stayframe", -1);
    CommonMenuMes2.SetBuff_system(SystemMes);
    EditSystemMes.tex_buff = MesWinTexBuff_01;
    InitMenuSave(mc_mode, 1, reinterpret_cast<u_long128 *>(read_buffer));
}

int LoopSave() {
    int result = MenuSaveKey();
    DrawMenuSave(nullptr);
    return result;
}

void TrialEndInit() {}

int TrialEndLoop() { return 1; }

void TrialStart() {}

int CheckTrialEnd() { return 0; }

CObject &CObject::operator=(const CObject &source) {
    // The three words after the mass are alignment padding and are not carried over.
    mass = source.mass;
    const float x = source.pos[0];
    const float y = source.pos[1];
    const float z = source.pos[2];
    const float w = source.pos[3];
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    pos[3] = w;
    velocity = source.velocity;
    acceleration = source.acceleration;
    gravity = source.gravity;
    moment = source.moment;
    rotation = source.rotation;
    rot_velocity = source.rot_velocity;
    rot_acceleration = source.rot_acceleration;
    *reinterpret_cast<CVector3_f_ *>(scale) = *reinterpret_cast<const CVector3_f_ *>(source.scale);
    return *this;
}

namespace {

/** CWater's members as its copy assignment reaches them. */
struct DraftWaterLayout {
    s32            rows;              /**< Grid rows the surface is divided into. */
    s32            columns;           /**< Grid columns the surface is divided into. */
    float         *height;            /**< Wave heights the surface currently draws from. */
    float          height_buffers[2]; /**< The two wave-height buffers. */
    sceVu0FVECTOR  vertex[4];         /**< The four corners of the surface. */
    u_int         *packet;            /**< First VU packet of the surface. */
    float          buffer_packets[2]; /**< VU packets for each display buffer. */
    CVisualPolyVu1 visual;            /**< Draws the polygons of the surface. */
    float          color;             /**< Red, green, blue and alpha channels of the surface. */
    float          wave_speed;        /**< Speed the ripples travel across the grid at. */
    float          damping;           /**< Rate the ripples lose height at. */
    float          height_scale;      /**< Scale from a wave height to the vertical displacement of its vertex. */
    float          distortion;        /**< Scale from a height difference to the texture-coordinate shift it causes. */
    s32            tags_built;        /**< Set once both packets hold their tags. */
    CFrameVu1      frame;             /**< Frame that places the surface. */
};

/** CVisualVu1's full layout: CVisual's words, then the vector-unit block and its size. */
struct DraftVisualVu1Layout {
    CVisual base;       /**< The base visual's words. */
    s32     vu_state_0; /**< First word of the vector-unit visual's own state. */
    s32     vu_state_1; /**< Second word of the vector-unit visual's own state. */
    u_int  *vu_data;    /**< Vector-unit packet the visual is drawn from. */
    u_int   vu_size;    /**< Size of the vector-unit packet in quadwords. */
};

} // namespace

CWater &CWater::operator=(CWater &src) {
    *reinterpret_cast<DraftWaterLayout *>(this) = *reinterpret_cast<DraftWaterLayout *>(&src);
    return *this;
}

/**
 * Copies one polygon visual over another, field by field.
 */
CVisualPolyVu1 &CVisualPolyVu1::operator=(const CVisualPolyVu1 &src) {
    CVisualVu1::operator=(src);
    return *this;
}

/**
 * Copies one vector-unit visual over another, field by field.
 */
CVisualVu1 &CVisualVu1::operator=(const CVisualVu1 &src) {
    auto       *dst_layout = reinterpret_cast<DraftVisualVu1Layout *>(this);
    const auto *src_layout = reinterpret_cast<const DraftVisualVu1Layout *>(&src);
    dst_layout->base = src_layout->base;
    dst_layout->vu_state_0 = src_layout->vu_state_0;
    dst_layout->vu_state_1 = src_layout->vu_state_1;
    dst_layout->vu_data = src_layout->vu_data;
    dst_layout->vu_size = src_layout->vu_size;
    return *this;
}

/**
 * Copies one visual over another, field by field.
 */
CVisual &CVisual::operator=(const CVisual &other) {
    unk_00 = other.unk_00;
    unk_04 = other.unk_04;
    return *this;
}

CategoryAttr::CategoryAttr() {
    Initialize();
}

void CategoryAttr::Initialize() {
    lod[0] = -1.0f;
    lowest = 0;
    highest = 3;
}

CBombEffect::CBombEffect() {
    Initialize();
}

/**
 * Clears the bomb effect.
 */
void CBombEffect::Initialize() {
    std::ranges::fill(active, 0);
}

CMajinBeem::CMajinBeem() {
    Initialize();
}

/**
 * Clears the beam effect.
 */
void CMajinBeem::Initialize() {
    active = false;
    alphas[59] = 0.0f;
}

MotionParam::MotionParam() = default;

u_char *CDataAlloc<1, 6000>::Alloc(int quads) {
    if (quads < 0 || used < 0 || used > 6000 || quads > 6000 - used) {
        ArenaOverflow(this, -1, 6000);
    }
    auto *run = reinterpret_cast<u_char *>(&block[used]);
    used += quads;
    return run;
}

// The PS2 link renames GeneratedNpcModel's implicit assignment to this: every member but the tail
// padding, the character through its own assignment.
MAP_NPC_MODEL &MAP_NPC_MODEL::operator=(const MAP_NPC_MODEL &other) {
    chara = other.chara;
    for (int i = 0; i < 4; i++) {
        pos[i] = other.pos[i];
        rot[i] = other.rot[i];
    }
    parts_no = other.parts_no;
    used = other.used;
    visible = other.visible;
    motion_no = other.motion_no;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 4; j++) {
            draw_pos[i][j] = other.draw_pos[i][j];
        }
        draw_param[i] = other.draw_param[i];
    }
    draw_num = other.draw_num;
    return *this;
}

} // namespace dc::game
