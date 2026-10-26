#include "stubs/main.hpp"
// C imports
#include <libcdvd.h>
#include <libmc.h>
#include <sifdev.h>
#include <sifrpc.h>

#include <cstdio>
#include <cstring>

// Our imports
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
#include "main.hpp"
#include "mainselect.hpp"
#include "mathutil.hpp"
#include "memcard.hpp"
#include "menu_draw.hpp"
#include "menu_save.hpp"
#include "mglib.hpp"
#include "object.hpp"
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
#include "visualvu1.hpp"
#include "vudata.hpp"
#include "vutext.hpp"
#include "water.hpp"

/**
 * Steps of the memory card check run before the title.
 */
// clang-format off
enum MemCheckMode {
    MEM_CHECK_CHECK    = 0, /**< Checks the memory cards once the cancel delay has run out. */
    MEM_CHECK_UNK_1    = 1, /**< Never set; does nothing. */
    MEM_CHECK_NO_CARD  = 2, /**< No usable PS2 memory card; shows the first warning. */
    MEM_CHECK_NO_SPACE = 3, /**< No card has room for a save; shows the second warning. */
    MEM_CHECK_DONE     = 4, /**< The check is over. */
};

// clang-format on

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
     *
     * @mangled __ct__12CategoryAttrFv
     * @address 0x143410
     * @size 0x30
     */
    CategoryAttr();

    /**
     * Resets the category's thresholds and trailing state.
     *
     * @mangled Initialize__12CategoryAttrFv
     * @address 0x143440
     * @size 0x1C
     */
    void Initialize();
};

STATIC_ASSERT(sizeof(CategoryAttr) == 0x18);

#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 873

/* Global, GLOBAL-linkage per retail `nm` (0x1cbc9b0, `T`). Purpose beyond
 * being constructed here is unanalyzed -- out of scope for this pass. */
CRunScript RunScript;

/* Retail `nm`: 0x1cbca00, LOCAL. Declared GLOBAL here because
 * LoadSystemMessage__Fv is still a raw .s and references it externally,
 * which a `static` in this object cannot satisfy. That costs symbol binding
 * only, not code bytes; restore `static` once it lands in this TU. */
CDataAlloc<1, 6000> SystemMesBuffer;

/* The global save-data instance and the pointer used to reach it. Retail's
 * symbol table:
 *     2433: 01cd4140 78272 OBJECT LOCAL  4 save_data
 *    11183: 002a250c     4 OBJECT GLOBAL 4 SaveData
 * 78272 is exactly sizeof(CSaveData). `save_data` is LOCAL, so it has to
 * share a translation unit with whatever assigns `SaveData = &save_data;` --
 * main(), below. `SaveData` is unmangled and sits in main.sbss at gp-0x72E4
 * from _gp = 0x2A97F0, confirmed against the `lw $4,-0x72E4($28)` preamble of
 * every CSaveData call in ref/asm/sections/dun/dun.text.s. */
static CSaveData save_data __attribute__((aligned(64)));
CSaveData       *SaveData;

/* main()'s own top-level game-state globals -- moved here from
 * ps2/src/mainselect.cpp/ps2/include/mainselect.hpp for the same reason as `pBound` etc. above:
 * retail's `main.sbss` interleaves these (GLOBAL linkage) with `SaveData`
 * just above and `mode`/etc. just below, all in one contiguous run
 * (0x2a2510-0x2a2548) that must come from ONE compiled object. Still
 * *declared* `extern` in ps2/include/mainselect.hpp (in case another, not-yet-
 * decompiled TU needs them), just no longer *defined* there. Declaration
 * order here matches retail's exact main.sbss address order. */
s32    GameClearFlag;
s32    main_select_menu_no;
s32    MapNo;
s32    OldMapNo;
s32    LocalMapNo;
s32    StartEventNo;
short *SystemMes;
CSound CSnd;
s32    main_select_padrup;

/* Scratch buffer for the "reset trial character names, keep the rest"
 * round-trip below. Retail has it as a real named static rather than a stack
 * local: `readelf -s` gives `config_data`, 64 bytes (= sizeof(SV_CONFIG_SYS)),
 * LOCAL, main.bss, and retail allocates no stack for it.
 *
 * Declared before `mainCDataAlloc2` to match retail's .bss order
 * (config_data 0x1ce7300, mainCDataAlloc2 0x1ce7560). It is a POD with no
 * constructor, so this does not disturb sinit order. */
static SV_CONFIG_SYS config_data;

/* Unidentified retail global: `nm` DebugFont, 0x1ce7340, LOCAL, 0x21C bytes.
 * A same-sized stand-in is needed or this object's .bss runs 0x220 short and
 * everything after it lands early. The 4 bytes before mainCDataAlloc2 at
 * 0x1ce7560 are the linker's alignment pad, not part of the object --
 * declaring 0x220 made the symbol four bytes longer than retail's.
 * TODO: identify the real type; the name suggests a font glyph table.
 *
 * GLOBAL rather than retail's LOCAL for the same reason as SystemMesBuffer:
 * MenuLoop__Fv is still a raw .s and references it. */
CDebugFont DebugFont;

/* Global, GLOBAL linkage per retail `nm` (0x1ce7560, `T`). Declared last,
 * after `save_data`/`SaveData`, since retail's sinit thunk constructs it
 * fourth (`SaveData` itself is a plain pointer with no constructor, so it
 * doesn't participate in sinit ordering either way). */
CDataAlloc2<1> mainCDataAlloc2;

/* Everything below is called from main() but not itself decompiled here
 * (out of scope for this pass -- each is its own subsystem). Declared with
 * their exact retail mangled/symbol names so the linker resolves them
 * against the existing reassembled .s files. Functions with no retained
 * retail name use this project's "func_ADDRESS" convention. */
extern "C" {
void init_all__Fv();
void initialize_data__Fv();
void GlobalNameInit__Fv();
void InitReadBG__Fv();
void SndInit__Fv();
void LoadOverlay__Fi(int mode);
void MGSetRenderInfo__Ffff(float a1, float a2, float a3);
void FlushCache(int mode);
void init_now_loading__Fi(int map_no);
void LoadSystemMessage__Fv();
void SndInitialize__Fiiii(int a1, int a2, int a3, int a4);
int  InitExistData__Fv();
void MapJump__Fii(int map_no, int event_no);
void EditInit__FPv(void *param);
void func_01DAF1C0();
void MenuInit__Fv(int mode);
void func_01DC1420(int mode);
void MemCheckInit__Fv(int mode);
void TrialEndInit__Fv(int mode);
void InitSave__Fv(int mode);
void LangsetInit__Fv(int mode);
void func_01DAC1C0();
void func_01DD1AB0(int inited);
void func_01DC8C50();
/* Takes no arguments -- the mangled name is __Fv, and retail's call sites
 * (the `while (check_now_loading...) ` spin loop at 0x1415C0) set up no
 * argument registers at all. An earlier pass of this file passed a saved
 * copy of `mode` here, which forced an extra callee-saved register copy
 * (`paddub s4,a0`) retail doesn't have. */
int  check_now_loading__Fv();
void MGInitVSyncCallBack__FPFi_i(void (*callback)(int));
void PlayTimeCount__Fi(int add);
void MGBeginFrame__Fv();
/* Retail passes Vif1Packet as the (pointer-typed, per the mangling) first
 * argument: `lw $4,Vif1Packet; paddub $20,$4; jal SetEnv...` -- the load
 * goes straight into $a0 and is copied to $s4 only to survive the call for
 * reuse by sceVif1PkCall below. An earlier pass declared this () and made
 * the copy direction come out backwards (lw s4 then paddub a0,s4). */
void SetEnv__FP13sceVif1Packet(sceVif1Packet *vif1_packet);
int  func_01DD2220();
int  func_01DC8EB0();
int  EditLoop__Fv();
int  func_01DAF970();
int  MenuLoop__Fv();
int  func_01DC1510();
int  MemCheckLoop__Fv();
int  TrialEndLoop__Fv();
int  LoopSave__Fv();
int  LangsetLoop__Fv();
int  func_01DAD980(int mode);
void MGEndFrame__Fv();
int  CheckTrialEnd__Fv();
/* No arguments (mangled __Fv); retail's spin loop at 0x141D10 sets up no
 * argument registers. An earlier pass passed `d2` here -- `d2` is in fact
 * never referenced by retail main() at all (no -0x72B4($gp) access
 * anywhere in ref/asm/split/main/main.s). */
int  ReadBGSync__Fv();
void TrialStart__Fv();
}

/* File-scope statics of the original main translation unit. Retail has them
 * unsuffixed and LOCAL (`nm`: mode, d1, d2, d8, mc_mode, in that main.sbss
 * address order, 0x2a2534-0x2a2548), so file-scope `static` rather than
 * function-local statics, which would get a `$NNN` suffix. No raw .s
 * references them by gp-offset, so real `static` works. `d2`/`d8` are never
 * touched by main() but must still be defined, so this object's .sbss covers
 * their retail byte range. See re/ai/build_pipeline.md. */
/** Top-level game mode currently being updated. */
s32         mode;
sceDmaChan *d1;
sceDmaChan *d2;
sceDmaChan *d8;
/** Memory-card operation selected by the developer menu. */
s32 mc_mode;

/* `PolyCount` (GLOBAL linkage) immediately follows `mc_mode` in retail's
 * main.sbss (0x2a2548) -- moved here from ps2/src/mainselect.cpp for the same reason
 * as `GameClearFlag` etc. above. Still declared `extern` in
 * include/mainselect.hpp. */
s32 PolyCount;

/**
 * Selects the next map that the game loads.
 */
s32 NextMapNo = -1;

/** Enables accumulation of the save file's play-time counter. */
static int play_time_count = 1;

/* Low byte of an EE DMA channel's CHCR, as a bitfield. The "kick this
 * channel" write in main() is a 1-bit field assignment in the original
 * source, not hand masking: only that form makes MWCC materialise the OR
 * operand with `daddiu $3,$0,0x40` -- its bitfield engine works in 64-bit --
 * instead of folding it into an `ori`. The same lbu/daddiu/li/and/or/sb
 * sequence appears twice in MGEndFrame against DmaCH8. */
typedef struct {
    u8 chcr_low : 6; /* DIR/MOD/ASP/TTE etc. -- untouched here */
    u8 str : 1;      /* channel start/busy */
    u8 chcr_top : 1;
} DMA_CHCR_BYTE;

/** Overlay file selected for each top-level game mode. */
static char *binfile[15] = {"TITLE.BIN", "TITLE.BIN", "", "DUN.BIN", "DUN.BIN",
                            "TITLE.BIN", "", "", "DUN.BIN", "DUN.BIN",
                            "", "", "", "", ""};

/** Name of the currently loaded overlay. */
static char now_binfile[128] = "";

void LoadOverlay(int mode);

void init_all() {
    sceSifInitRpc(0);
    sceCdInit(0);
    sceCdMmode(2);

    while (!sceSifRebootIop("cdrom0:\\MODULES\\IOPRP211.IMG;1")) {
    }

    while (!sceSifSyncIop()) {
    }

    sceSifInitRpc(0);
    sceCdInit(0);
    sceCdMmode(2);
    sceFsReset();

    while (sceSifLoadModule("cdrom0:\\MODULES\\SIO2MAN.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\PADMAN.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\MCMAN.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\MCSERV.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\LIBSD.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\SDRDRV.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\MODMIDI.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\MODHSYN.IRX;1", 0, 0) < 0) {
    }

    while (sceSifLoadModule("cdrom0:\\MODULES\\EZMIDI.IRX;1", 0, 0) < 0) {
    }

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

/** Initial parallel-light direction matrix used by the renderer. */
sceVu0FMATRIX light = {
    {0.578f, 0.0f, 0.0f, 0.0f},
    {0.578f, 0.0f, 0.0f, 0.0f},
    {0.578f, 0.0f, 0.0f, 0.0f},
    {0.0f,   0.0f, 0.0f, 0.0f},
};

/** Initial parallel-light colour matrix used by the renderer. */
sceVu0FMATRIX lightcolor = {
    {120.0f, 120.0f, 120.0f, 128.0f},
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  },
    {0.0f,   0.0f,   0.0f,   0.0f  },
};

/** Initial ambient-light colour used by the renderer. */
sceVu0FVECTOR ambientlight = {64.0f, 64.0f, 64.0f, 128.0f};

void SetEnv(sceVif1Packet *packet);

void LoadSndTxt() {
    u_char  work[48000];
    u_char *buffer = work;
    int     offset = (int) buffer % 64;

    if (offset) {
        buffer += 64 - offset;
    }

    CSnd.LoadSeInf("sound/tbl/setbl.txt", (u_int *) buffer);
    CSnd.LoadSqInf("sound/tbl/sqtbl.txt", (u_int *) buffer);
    SndInitSeTable();
}

void PlayTimeCountFlag(int flag) {
    play_time_count = flag;
}

int PlayTimeCount(int unused) {
    if (play_time_count) {
        SaveData->AddPlayTime(1);
    }
    return play_time_count;
}

void LoadSystemMessage() {
    int size;
    SystemMesBuffer.used = 0;
    SystemMesBuffer.Align64();
    SystemMes = (short *) &SystemMesBuffer.block[SystemMesBuffer.used];
    char name[64] = "meswin/system";

    if (LanguageCode > LANG_JAPANESE) {
        sprintf(name, "meswin/system_%d", LanguageCode);
    }

    strcat(name, ".mes");

    if (!LoadFile2(name, SystemMes, &size, 0)) {
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

/* @ 0x1410B0 (0xC80 bytes) -- main */
int main(int argc, const char **argv, const char **envp);

static int edit_map;
static int sub_map;
static int event_no;

/* The game's top-level modes and the transitions between them: map jumps, the
 * menu, the save and memory-card screens, the trial ending. */

void MapJump(int map_no, int event_no) {
    NextMapNo = map_no;
    StartEventNo = event_no;
}

void MenuInit() {
    static LOADTEXTURE_INFO texdata[64] = {{"#frame_buff#640#" HALF_BUFFER_HEIGHT_STR "#4"},
                                           {"img/ankfont.img"},
                                           {gamemode_empty_string}};
    float                   background;
    int                     result;
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 100000);
    SetPacketReadBuffer(10000, 100000);
    TexManager.Initialize(16352);
    result = TexManager.EnterTextureFile(texdata);
    result = TexManager.LoadTextureBlock(-1, read_buffer);
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
    static char *menu[10] = {
        "game start",
        "%se0%d\n",
        "%ss%d\n",
        "interior",
        "dungeon",
        "opening",
        "%sevent%d\n",
        "%smemory card %d\n",
        "%sLanguage %d\n",
        gamemode_empty_string};
    static int map_no[10] = {800, 0, 11, 99, 200, 400, 0, 0, 0, -1};

    DebugFont.length = 0;

    int        i = 0;
    static int select = 0;

    while (menu[i][0] != 0) {
        i++;
    }

    char *cursor[2] = {" ", ">"};

    if (GamePad.Down(PAD_DOWN)) {
        select++;
    }

    if (GamePad.Down(PAD_UP)) {
        select--;
    }

    if (select < 0) {
        select = 0;
    }

    if (select >= i) {
        select = i - 1;
    }

    if (select == 1) {
        if (GamePad.Down(PAD_RIGHT)) {
            edit_map++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            edit_map--;
        }

        if (edit_map < 0) {
            edit_map = 0;
        }

        if (edit_map > 4) {
            edit_map = 4;
        }
    }

    if (select == 2) {
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

    if (select == 6) {
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

    if (select == 7) {
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

    if (select == 8) {
        if (GamePad.Down(PAD_RIGHT)) {
            LanguageCode++;
        }

        if (GamePad.Down(PAD_LEFT)) {
            LanguageCode--;
        }

        if (LanguageCode < LANG_JAPANESE) {
            LanguageCode = LANG_JAPANESE;
        }

        if (LanguageCode > LANG_SPANISH) {
            LanguageCode = LANG_SPANISH;
        }
    }

    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], "Dark Cloud Ver3.07 2001/07/12\n");
    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], "%s%s\n", cursor[select == 0], menu[0]);
    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], menu[1], cursor[select == 1], edit_map + 1);
    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], menu[2], cursor[select == 2], sub_map + 1);

    for (i = 3; i < 6; i++) {
        DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], "%s%s\n", cursor[i == select], menu[i]);
    }

    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], menu[i], cursor[i == select], event_no);
    i++;
    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], menu[i], cursor[i == select], mc_mode);
    i++;
    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], menu[i], cursor[i == select], LanguageCode);

    TexManager.ReloadTexture(GetVif1Packet(), 0);
    DebugFont.Draw();

    if (GamePad.Down(PAD_CIRCLE) || GamePad.Down(PAD_TRIANGLE)) {
        static int map[3] = {23, 41, 19};
        static int event[3] = {310, 150, 305};

        main_select_padrup = 0;

        if (GamePad.Down(PAD_TRIANGLE)) {
            main_select_padrup = 1;
        }

        OldMapNo = -1;
        NextMapNo = -1;
        GamePad.AutoRepeatOff();

        main_select_menu_no = map_no[select];
        mode = GAME_MODE_EDIT;

        if (select == 1) {
            main_select_menu_no = edit_map;
            LocalMapNo = edit_map;
        }

        if (select == 2) {
            main_select_menu_no = sub_map + 11;
            LocalMapNo = main_select_menu_no;
        }

        if (select == 6) {
            MapJump(map[event_no], event[event_no]);
            return 1;
        }

        if (select == 7) {
            mode = GAME_MODE_SAVE;
            return 1;
        }

        MapNo = main_select_menu_no;

        if (main_select_menu_no == 200) {
            main_select_menu_no = 0;
            mode = GAME_MODE_LOADER;
        }

        if (main_select_menu_no == 800) {
            MapNo++;
            main_select_menu_no = 0;
            strcpy(main_select_param, "title");
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

static int mem_chk_mode;
static int check_cancel;
static int taiken_cnt;
static int taiken_start;

void MemCheckInit() {
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 100000);
    SetPacketReadBuffer(10000, 100000);
    char filename[64] = "img/memory.img";

    if (LanguageCode > LANG_JAPANESE && LanguageCode < LANG_COUNT) {
        sprintf(filename, "img_%d/memory.img", LanguageCode);
    }

    TexManager.Initialize(16352);
    static LOADTEXTURE_INFO texdata[64] = {{filename}, {gamemode_empty_string}};
    TexManager.EnterTextureFile(texdata);
    TexManager.LoadTextureBlock(-1, read_buffer);
    MGSetBGColor(0.0f, 0.0f, 0.0f, 0.0f);

    if (sceMcInit()) {
        printf("libmc initialize faild\n");
    }

    mem_chk_mode = MEM_CHECK_CHECK;
    check_cancel = 2;
    taiken_cnt = 0;
    taiken_start = 0;
}

int MemCheckLoop() {
    DebugFont.length = 0;
    TexManager.ReloadTexture(GetVif1Packet(), 0);

    switch (mem_chk_mode) {
        case MEM_CHECK_CHECK:
            if (check_cancel > 0) {
                break;
            }

            switch (SaveEnableCheck()) {
                case SAVE_ENABLE_OK:
                    return 1;
                case SAVE_ENABLE_NO_CARD:
                    mem_chk_mode = MEM_CHECK_NO_CARD;
                    break;
                case SAVE_ENABLE_NO_SPACE:
                    mem_chk_mode = MEM_CHECK_NO_SPACE;
                    break;
            }

            break;
        case MEM_CHECK_UNK_1:
            break;
        case MEM_CHECK_NO_CARD: {
            /* The warning images are drawn at NTSC height and stretched over the taller frame. */
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
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("memory01", -1), rect, texel);

            if (GamePad.Down(PAD_CIRCLE)) {
                mem_chk_mode = MEM_CHECK_CHECK;
                check_cancel = 3;
            }

            if (GamePad.Down(PAD_CROSS)) {
                mem_chk_mode = MEM_CHECK_DONE;
            }

            break;
        }
        case MEM_CHECK_NO_SPACE: {
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
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("memory02", -1), rect, texel);

            if (GamePad.Down(PAD_CIRCLE)) {
                mem_chk_mode = MEM_CHECK_CHECK;
                check_cancel = 3;
            }

            if (GamePad.Down(PAD_CROSS)) {
                mem_chk_mode = MEM_CHECK_DONE;
            }

            break;
        }
        case MEM_CHECK_DONE:
            return 1;
    }

    check_cancel--;

    if (check_cancel < 0) {
        check_cancel = 0;
    }

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
    u_int *buffer;
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 100000);
    SetPacketReadBuffer(10000, 100000);
    TexManager.Initialize(16352);
    TexManager.EnterTextureFile(texdata);
    TexManager.LoadTextureBlock(-1, read_buffer);
    LoadFileMenuData((buffer = read_buffer, "stayframe.img"), buffer);
    TexManager.EnterFixTextureZ((u_char *) read_buffer);
    StayTexture = TexManager.GetTexture("stayframe", -1);
    CommonMenuMes2.SetBuff_system(SystemMes);
    EditSystemMes.tex_buff = MesWinTexBuff_01;
    InitMenuSave(mc_mode, 1, (u_long128 *) read_buffer);
}

int LoopSave() {
    int result = MenuSaveKey();
    DrawMenuSave(0);
    return result;
}

void TrialEndInit() {}

int TrialEndLoop() { return 1; }

void TrialStart() {}

int CheckTrialEnd() { return 0; }

/* MAP_NPC_MODEL's members as its copy assignment reaches them; the tail is alignment padding. */
struct GeneratedNpcModel {
    CCharacter chara;           /**< Draws and moves the character. */
    float      pos[4];          /**< World position of the character. */
    float      rotation[4];     /**< Angle of the character about each axis. */
    s32        parts_no;        /**< Index of the map part that the character stands on. */
    s32        used;            /**< 1 if the slot is in use. */
    s32        unk_11D8;        /**< 0 to hide the character. */
    s32        unk_11DC;        /**< Motion the character plays; -1 to keep it still. */
    float      draw_pos[16][4]; /**< Position of each copy of the character to draw. */
    s32        draw_param[16];  /**< Parameter of each copy of the character to draw. */
    s32        draw_num;        /**< Number of copies of the character to draw. */
};

/**
 * Copies one character over another, field by field.
 *
 * @mangled __as__10CCharacterFRC10CCharacter
 * @address 0x142DA0
 * @size 0x43C
 */
/**
 * Copies one character into another with CCharacter's assignment operator.
 * Nothing calls it: it exists so that the operator documented above is defined in this unit.
 */
#pragma push
#pragma dont_inline on

static void CopyNpcModel(GeneratedNpcModel &dest, const GeneratedNpcModel &src) {
    dest = src;
}

static void CopyCharacter(CCharacter &dest, const CCharacter &src) {
    dest = src;
}

#pragma pop

CObject &CObject::operator=(const CObject &source) {
    // The three words after the mass are alignment padding and are not carried over.
    mass = source.mass;
    float w, z, y, x;
    x = source.pos[0];
    y = source.pos[1];
    z = source.pos[2];
    w = source.pos[3];
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
    *(CVector3_f_ *) scale = *(const CVector3_f_ *) source.scale;
    return *this;
}

/* CWater's members as its copy assignment reaches them. */
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

CWater &CWater::operator=(CWater &src) {
    *(DraftWaterLayout *) this = *(DraftWaterLayout *) &src;
    return *this;
}

/**
 * Copies one polygon visual over another, field by field.
 *
 * @mangled __as__14CVisualPolyVu1FRC14CVisualPolyVu1
 * @address 0x143360
 * @size 0x30
 */
CVisualPolyVu1 &CVisualPolyVu1::operator=(const CVisualPolyVu1 &src) {
    CVisualVu1::operator=(src);
    return *this;
}

/**
 * Copies one vector-unit visual over another, field by field.
 *
 * @mangled __as__10CVisualVu1FRC10CVisualVu1
 * @address 0x143390
 * @size 0x5C
 */
/* The full layout: CVisual's words, then the vector-unit block and its size. */
struct DraftVisualVu1Layout {
    CVisual base;       /**< The base visual's words. */
    s32     vu_state_0; /**< First word of the vector-unit visual's own state. */
    s32     vu_state_1; /**< Second word of the vector-unit visual's own state. */
    u_int  *vu_data;    /**< Vector-unit packet the visual is drawn from. */
    u_int   vu_size;    /**< Size of the vector-unit packet in quadwords. */
};

CVisualVu1 &CVisualVu1::operator=(const CVisualVu1 &src) {
    DraftVisualVu1Layout       *dst_layout = (DraftVisualVu1Layout *) this;
    const DraftVisualVu1Layout *src_layout = (const DraftVisualVu1Layout *) &src;
    dst_layout->base = src_layout->base;
    dst_layout->vu_state_0 = src_layout->vu_state_0;
    dst_layout->vu_state_1 = src_layout->vu_state_1;
    dst_layout->vu_data = src_layout->vu_data;
    dst_layout->vu_size = src_layout->vu_size;
    return *this;
}

/**
 * Copies one visual over another, field by field.
 *
 * @mangled __as__7CVisualFRC7CVisual
 * @address 0x1433F0
 * @size 0x1C
 */
CVisual &CVisual::operator=(const CVisual &other) {
    unk_00 = other.unk_00;
    unk_04 = other.unk_04;
    return *this;
}

/**
 * Clears the category attributes.
 *
 * @mangled __ct__12CategoryAttrFv
 * @address 0x143410
 * @size 0x30
 */
CategoryAttr::CategoryAttr() {
    Initialize();
}

/**
 * Gives the category attributes their starting values.
 *
 * @mangled Initialize__12CategoryAttrFv
 * @address 0x143440
 * @size 0x1C
 */
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
 *
 * @mangled Initialize__11CBombEffectFv
 * @address 0x143490
 * @size 0x30
 */
void CBombEffect::Initialize() {
    for (int i = 0; i < 8; i++) {
        active[i] = 0;
    }
}

CMajinBeem::CMajinBeem() {
    Initialize();
}

/**
 * Clears the beam effect.
 *
 * @mangled Initialize__10CMajinBeemFv
 * @address 0x1434F0
 * @size 0x10
 */
void CMajinBeem::Initialize() {
    active = false;
    alphas[59] = 0.0f;
}

/**
 * Constructs one map character slot.
 *
 * @mangled __ct__13MAP_NPC_MODELFv
 * @address 0x143500
 * @size 0x30
 */
extern "C" void *__ct__13MAP_NPC_MODELFv(void *self) {
    __ct__10CCharacterFv(self);
    return self;
}

/**
 * Constructs a character with no model, motion or texture animation.
 *
 * @mangled __ct__10CCharacterFv
 * @address 0x143530
 * @size 0xD4
 */
extern "C" void *__ct__10CCharacterFv(void *self);

/**
 * Constructs the motion parameters.
 *
 * @mangled __ct__11MotionParamFv
 * @address 0x143610
 * @size 0xC
 */
MotionParam::MotionParam() {}

/**
 * Constructs a texture animation with no data attached.
 *
 * @mangled __ct__13CTextureAnimeFv
 * @address 0x143620
 * @size 0x28
 */
extern "C" void *__ct__13CTextureAnimeFv(void *self);

/**
 * Constructs an object at the origin with an identity transform.
 *
 * @mangled __ct__7CObjectFv
 * @address 0x143650
 * @size 0x28
 */
extern "C" void *__ct__7CObjectFv(void *self);

/**
 * Constructs a projectile effect.
 *
 * @mangled __ct__12CSHOT_EFFECTFv
 * @address 0x143680
 * @size 0x54
 */
extern "C" void *__ct__12CSHOT_EFFECTFv(void *self) {
    __ct__10CCharacterFv((u_char *) self + 0x10);
    u_char *array = (u_char *) self + 0x11C0;
    __construct_array(array, (MWRuntimeObjectFunction) __ct__10CCharacterFv, NULL, 0x11B0, 8);
    return self;
}

/**
 * Constructs a hit marker.
 *
 * @mangled __ct__8CHitMarkFv
 * @address 0x1436E0
 * @size 0x3C
 */
extern "C" void *__ct__8CHitMarkFv(void *self);

/**
 * Takes a run of quadwords out of the six-thousand-quadword arena.
 *
 * @mangled Alloc__18CDataAlloc_1_6000_Fi
 * @address 0x143720
 * @size 0x68
 */

