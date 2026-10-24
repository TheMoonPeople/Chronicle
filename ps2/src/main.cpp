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

void LoadOverlay(int mode) {
    if (binfile[mode][0] == '\0') {
        return;
    }

    if (strcmp(binfile[mode], now_binfile) == 0) {
        return;
    }

    strcpy(now_binfile, binfile[mode]);
    void *address = _overlay_group_addresses[1];
    char  path[128] = "cdrom0:\\";
    strcat(path, binfile[mode]);
    strcat(path, ";1");
    mwLoadOverlay(path, address);
}

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

void SetEnv(sceVif1Packet *packet) {
    sceVif1PkCnt(packet, 0);
    sceVif1PkOpenDirectCode(packet, 0);
    sceVif1PkOpenGifTag(packet, *(u_long128 *) &GiftagAD);
    sceVif1PkAddGsAD(packet, SCE_GS_TEX1_1, *(u_long *) &mgTEX1Env);
    sceVif1PkAddGsAD(packet, SCE_GS_TEST_1, *(u_long *) &mgPixelTest);
    sceVif1PkAddGsAD(packet, SCE_GS_ZBUF_1, *(u_long *) &mgZBuffer);
    sceVif1PkAddGsAD(packet, SCE_GS_ALPHA_1, *(u_long *) &mgAlpha);
    sceVif1PkAddGsAD(packet, SCE_GS_CLAMP_1, 5);
    sceVif1PkCloseGifTag(packet);
    sceVif1PkCloseDirectCode(packet);
}

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
    PORT_RETURN(play_time_count);
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
int main(int argc, const char **argv, const char **envp) {
    /* Function-local statics: retail names ("depth$682", "init$683", ...)
     * confirmed via IDA's global list against this exact function; the
     * "$NNN" suffix is MWCC's disambiguation counter for function-local
     * statics (see CLAUDE.md), so these belong here, not in mainselect.hpp. */
    /* `bool` compiles to an unsigned byte load (`lbu`) for these checks;
     * retail uses a signed load (`lb`), confirmed by diffing against
     * retail -- so these are declared `s8`, not `bool`, matching every
     * other signed-byte flag in this codebase (see CLAUDE.md on `s8`
     * being plain `char`, signed by default on this target). */
    static s32 depth;
    static s8  init;
    static s32 wt;
    static s8  init2;
    static s32 init_flag;
    static s8  init3;

    /* Locals in retail's callee-saved order; MWCC assigns s0,s1,... to
     * top-level locals in declaration order:
     *   loop_result -> s0  per-iteration loop-exit result
     *   skip_title  -> s1  "skip the title demo" flag, set in the first switch's
     *                      case 1 and consumed in the second's
     *   i           -> s2  warmup counter, reused as the case-0 once-flag
     *                      ($18 in retail for both)
     *   exist_data  -> s3  InitExistData() result
     *   j           -> s4  chara-name copy counter, sharing its register with the
     *                      other short-lived temporaries as retail does with $20
     *   idle_result -> s5  never written; retail reads an uninitialised
     *                      callee-saved register in the second switch's case 12,
     *                      so this is declared last to take the last slot
     */
    int  loop_result;
    int  skip_title;
    int  i;
    int  exist_data;
    int  j;
    int  idle_result;
    char chara_names[6][64];
    int  in_range;

    mwInit(argc, argv, envp);
#ifdef PAL
    DebugMode = 0;
#endif

    if (!init) {
        depth = 0;
        init = true;
    }

    if (!init2) {
        wt = 1;
        init2 = true;
    }

    /* Plain statements, not declaration initializers -- retail zeroes
     * s0/s1 here, *after* the two static-init blocks above (an
     * initializer at declaration gets scheduled before the mwInit call
     * instead). */
    loop_result = 0;
    skip_title = 0;

    mode = GAME_MODE_MENU;
    main_select_menu_no = 0;

    strcpy(main_select_param, "e01");
    init_all__Fv();

    while (sceGsSyncV(0) == 0) {
    }

    GamePad.Init();

    SaveData = &save_data;
    NextMapNo = -1;
    MapNo = -1;
    OldMapNo = -1;
    StartEventNo = -1;
    GameClearFlag = 0;

    memset(&save_data, 0, sizeof(CSaveData));
    save_data.Initialize();
    GlobalNameInit__Fv();

    /* `(in_range = ...) != 0` rather than a bare `i < 60`: the anonymous
     * form routes the slti result through $at, the named form through a
     * real register ($v0, as retail has) -- see CLAUDE.md's named-
     * variable register trick, applied throughout ps2/src/savedata.cpp. */
    for (i = 0; (in_range = i < 60) != 0; i++) {
        sceGsSyncV(0);
        GamePad.UpDate();

#ifdef PAL
        /* Holding four buttons through the first second turns on debug mode. */
        if (GamePad.On2(PAD_R1) != 0 && GamePad.On2(PAD_R2) != 0 && GamePad.On2(PAD_L1) != 0 && GamePad.On2(PAD_L2) != 0) {
            DebugMode = 1;
        }
#else
        if (GamePad.On2(PAD_R1) != 0 && GamePad.On2(PAD_R2) != 0 && GamePad.On2(PAD_L1) != 0) {
            GamePad.On2(PAD_L2);
        }
#endif
    }

#ifdef PAL
    /* Debug mode starts at the developer menu instead of the language select. */
    if (!DebugMode) {
        MapNo = -1;
        mode = GAME_MODE_LANGUAGE;
        GamePad.KeyLock2(1);
    }
#else
    MapNo = -1;
    mode = GAME_MODE_MEMORY_CHECK;
    GamePad.KeyLock2(1);
#endif

    i = 0;
    exist_data = 0;

    if (!init3) {
        init_flag = 0;
        init3 = true;
    }

    while (1) {
        if (mode != GAME_MODE_UNUSED_12 && init_flag == 0) {
            initialize_data__Fv();
            init_flag = 1;
        }

        InitReadBG__Fv();
        SndInit__Fv();

#ifdef PAL
        if (!DebugMode && mode == GAME_MODE_MENU) {
#else
        if (mode == GAME_MODE_MENU) {
#endif
            MapNo = 801;
            mode = GAME_MODE_RUSH_MOVIE;
        }

        LoadOverlay__Fi(mode);
        /* The third argument is the int 65535 (0xFFFF) converted to float
         * at runtime -- retail: `ori $2,$0,0xFFFF; mtc1 $2,$f0; cvt.s.w
         * $f14,$f0` -- not a compile-time float constant (an earlier pass
         * had `(float)-1`, which the front end folds to a `lui 0xbf80`
         * -1.0f immediate). */
        MGSetRenderInfo__Ffff(800.0f, 10.0f, 65535);
        sceGsSyncPath(0, 0);

        /* A `switch`, not an `||` chain: as a chain MWCC merges 9 and 10
         * into a range check (`addiu v0,v1,-9; sltiu at,v0,2`), which retail
         * does not have. MWCC emits a sparse switch's comparisons in reverse
         * written order, so these are written 9,7,10,14 to get retail's
         * 14,10,7,9. */
        switch (mode) {
            case GAME_MODE_LOADER:
            case GAME_MODE_MENU:
            case GAME_MODE_MEMORY_CHECK:
            case GAME_MODE_LANGUAGE:
                MapNo = -1;
                OldMapNo = -1;
                break;
        }

        FlushCache(0);
        sceDmaSend(d1, My_dma_start0);
        sceGsSyncPath(0, 0);
        FlushCache(0);
        sceDmaSend(d1, Vu_progmain);
        sceGsSyncPath(0, 0);

        if (mode != GAME_MODE_UNUSED_12 && StartEventNo < 0) {
            init_now_loading__Fi(MapNo);
        }

        GamePad.StopVibration();

#ifdef PAL
        if (DebugMode) {
            LoadSystemMessage__Fv();
        }
#endif

        /* Case bodies are in retail's physical body order rather than
         * case-value order, taken from the @875 table in
         * ref/asm/sections/main/main.rodata.s. MWCC lays bodies out in
         * written order, and a jump table's indices are right regardless of
         * that order while the bodies' own addresses are not. */
        switch (mode) {
            case GAME_MODE_LOADER:
                func_01DC1420(mode);
                break;
            case GAME_MODE_LANGUAGE:
                LangsetInit__Fv(mode);
                break;
            case GAME_MODE_TITLE:
                LoadSystemMessage__Fv();
                GlobalNameInit__Fv();
                SndInitialize__Fiiii(4, 30, 4, 5);

                /* `i` is the warmup counter reused as a "ran once" flag,
                 * $18 in retail for both. Retail stores GameClearFlag = 1
                 * here (gp-0x72E0), not main_select_menu_no (gp-0x72DC),
                 * and emits the 1 as a copy of the flag's own register
                 * (`paddub $2,$18`), which the literal reproduces via copy
                 * propagation. */
                if (i == 0) {
                    i = 1;
                    exist_data = InitExistData__Fv();

                    if (*(s32 *) &((SV_CONFIG_SYS *) SaveData->GetConfigData())->game_clear_area[2] != 0) {
                        GameClearFlag = 1;
                    }
                }

                func_01DD1AB0(exist_data);
                break;
            case GAME_MODE_RUSH_MOVIE:
                SndInitialize__Fiiii(4, 30, 4, 5);
                func_01DC8C50();

                /* Sets skip_title (the "skip title demo" flag consumed by the
                 * second switch's case 1 below), not loop_result -- retail writes
                 * $17/s1 here, the same register case 1 below tests and
                 * clears. */
                if (GamePad.On(PAD_START) != 0) {
                    MapJump__Fii(800, -1);
                    skip_title = 1;
                }

                break;
            case GAME_MODE_EDIT:
                EditInit__FPv(0);
                break;
            case GAME_MODE_MENU:
                MenuInit__Fv(mode);
                break;
            case GAME_MODE_SAVE:
                InitSave__Fv(mode);
                break;
            case GAME_MODE_MEMORY_CHECK:
                MemCheckInit__Fv(mode);
                break;
            case GAME_MODE_TRIAL_END:
                TrialEndInit__Fv(mode);
                break;
            case GAME_MODE_OPENING:
                SndInitialize__Fiiii(4, 30, 4, 5);
                func_01DAF1C0();
                break;
            case GAME_MODE_UNUSED_6:
            case GAME_MODE_UNUSED_8:
            case GAME_MODE_UNUSED_12:
                break;
            default:
                func_01DAC1C0();
                break;
        }

        NextMapNo = -1;

        while (check_now_loading__Fv() == 0) {
        }

        MGInitVSyncCallBack__FPFi_i(PlayTimeCount__Fi);
        FlushCache(0);
        sceDmaSend(d1, My_dma_start0);
        sceGsSyncPath(0, 0);
        FlushCache(0);
        sceDmaSend(d1, Vu_progmain);
        sceGsSyncPath(0, 0);

        *(volatile s32 *) 0x10000010 = 131;
        sceGsSyncV(0);

        /* Kick the DMA channel: set the STR bit via a volatile bitfield
         * store (see DMA_CHCR_BYTE above for why this exact source form
         * is required for the daddiu/li/and/or sequence to match). */
        ((volatile DMA_CHCR_BYTE *) d1)->str = 1;
        /* CSaveData::map_no is private, and retail reaches it by offset
         * from main() -- a different translation unit -- so this is direct
         * pointer arithmetic rather than a getter. Value and base both go
         * through named locals, value first: the only form whose temp
         * numbering matches retail (`lw v0,MapNo; lw v1,SaveData;
         * sw v0,0x1c8(v1)`). See re/ai/main.md. */
        {
            s32   map_no = MapNo;
            char *save_bytes = (char *) SaveData;
            *(s32 *) (save_bytes + 0x1C8) = map_no;
        }

        do {
#ifdef PAL
            {
                s32 *config_words = (s32 *) SaveData->GetConfigData();
                MGAdjustScreen(config_words[12], config_words[13]);
            }
#endif
            int game_clear = GameClearFlag;
            *(s32 *) &((SV_CONFIG_SYS *) SaveData->GetConfigData())->game_clear_area[2] = game_clear;

            *(volatile s32 *) 0x10000000 = 0;
            MGBeginFrame__Fv();
            /* Discarded read of the same hardware register just written --
             * retail does this too (`lw v0,0(at)` with the result never
             * used again), confirmed missing by diffing against retail. */
            (void) *(volatile s32 *) 0x10000000;

            /* Retail's shape (lw a0,Vif1Packet; paddub s4,a0; jal SetEnv;
             * lui/addiu a1; paddub a0,s4; paddub a2; jal PkCall) needs all
             * three of: SetEnv taking the global directly, so the load lands
             * in $a0 and the local is a copy from it; a named local holding
             * the pre-call value, since retail does not reload after SetEnv;
             * and the cast on Vu_prog0f, which hoists the lui/addiu pair
             * ahead of the first-argument copy, MWCC evaluating arguments
             * strictly left to right. */
            int vif1_packet = (int) Vif1Packet;
            SetEnv__FP13sceVif1Packet(Vif1Packet);
            sceVif1PkCall((sceVif1Packet *) vif1_packet, (u_long128 *) Vu_prog0f, 0);

            /* gp-0x72A8 here is PolyCount, not mc_mode (gp-0x72AC) -- an
             * earlier pass had the wrong global (off by one slot in the
             * sbss layout; verified against `nm` addresses vs. gp =
             * 0x2A97F0: PolyCount = 0x2A2548 = gp-0x72A8). */
            PolyCount = 0;
            /* Second discarded read of the VIF/DMA register block, present
             * in retail between the PolyCount clear and the old_main_mode
             * store (`lui $1,0x1000; lw $2,0($1)`, result unused). */
            (void) *(volatile s32 *) 0x10000000;
            old_main_mode = mode;

            /* Body order matches retail's compiled address order rather
             * than case-value order, from the .L-label order in
             * ref/asm/split/main/main.s: 14 (0x141710), 0 (0x14173C),
             * 1 (0x141930), 2 (0x141984), 7 (0x1419D8), 13 (0x1419EC),
             * 12 (0x141A10), 10 (0x141A2C), 11 (0x141A58), 5 (0x141A80),
             * 9 (0x141AB8), default (0x141ADC). Cases 3 and 4 share
             * `default`'s slot in retail, so they are not written. */
            switch (mode) {
                case GAME_MODE_LANGUAGE:
                    loop_result = LangsetLoop__Fv();

                    if (loop_result != 0) {
#ifdef PAL
                        MapNo = -1;
                        mode = GAME_MODE_MEMORY_CHECK;
#else
                        MapNo = 801;
                        mode = GAME_MODE_RUSH_MOVIE;
#endif
                    }

                    break;
                case GAME_MODE_TITLE:
                    loop_result = func_01DD2220();

                    if (loop_result == 1) {
                        main_select_menu_no = 0;
                        strcpy(main_select_param, "e01");
                        mode = GAME_MODE_EDIT;
                    }

                    if (loop_result == 2) {
                        main_select_menu_no = 1;
                        strcpy(main_select_param, "e01");
                        mode = GAME_MODE_EDIT;
                    }

                    if (loop_result == 3) {
                        mode = GAME_MODE_DUNGEON;
                        main_select_menu_no = 0;
                    }

                    if (loop_result == 5) {
                        mode = GAME_MODE_OPENING;
                    }

                    if (loop_result == 1) {
                        MapJump__Fii(400, -1);

                        for (j = 0; (in_range = j < 6) != 0; j++) {
                            memcpy(chara_names[j], save_data.GetCharaName(j), 64);
                        }

                        save_data.ConvertConfig(&config_data);
                        memset(&save_data, 0, sizeof(CSaveData));
                        save_data.Initialize();
                        save_data.InvertConfig(&config_data);

                        for (j = 0; (in_range = j < 6) != 0; j++) {
                            memcpy(save_data.GetCharaName(j), chara_names[j], 64);
                        }

                        TrialStart__Fv();
                    }

                    if (loop_result == 4) {
                        MapJump__Fii(801, -1);
                    }

                    if (loop_result == 2) {
                        SndInitialize__Fiiii(4, 30, 4, 5);
                    }

                    break;
                case GAME_MODE_RUSH_MOVIE:
                    if (skip_title != 0) {
                        loop_result = 1;
                        MapJump__Fii(800, -1);
                        skip_title = 0;
                    } else {
                        loop_result = func_01DC8EB0();

                        if (loop_result != 0) {
                            MapJump__Fii(800, -1);
                        }
                    }

                    break;
                case GAME_MODE_EDIT:
                    loop_result = EditLoop__Fv();

                    if (loop_result == 1) {
                        mode = GAME_MODE_TITLE;
                    }

                    if (loop_result == 2) {
                        mode = GAME_MODE_EDIT;
                    }

                    if (loop_result != 0) {
                        mode = GAME_MODE_MENU;
                    }

                    if (loop_result == 3) {
                        mode = GAME_MODE_DUNGEON;
                    }

                    break;
                case GAME_MODE_MENU:
                    loop_result = MenuLoop__Fv();
                    break;
                case GAME_MODE_SAVE:
                    loop_result = LoopSave__Fv();

                    if (loop_result != 0) {
                        mode = GAME_MODE_MENU;
                    }

                    break;
                case GAME_MODE_UNUSED_12:
                    loop_result = idle_result;

                    if (idle_result != 0) {
                        mode = GAME_MODE_MENU;
                    }

                    break;
                case GAME_MODE_MEMORY_CHECK:
                    loop_result = MemCheckLoop__Fv();

                    if (loop_result != 0) {
                        MapNo = 801;
                        mode = GAME_MODE_RUSH_MOVIE;
                    }

                    break;
                case GAME_MODE_TRIAL_END:
                    loop_result = TrialEndLoop__Fv();

                    if (loop_result != 0) {
                        MapNo = 800;
                        mode = GAME_MODE_TITLE;
                    }

                    break;
                case GAME_MODE_OPENING:
                    /* Two separate ifs, not one -- retail emits two
                     * back-to-back `beqz $16` tests (0x141A8C skipping only
                     * the `mode = GAME_MODE_TITLE` store, 0x141A98 skipping the MapJump),
                     * which a single merged if does not produce. */
                    loop_result = func_01DAF970();

                    if (loop_result != 0) {
                        mode = GAME_MODE_TITLE;
                    }

                    if (loop_result != 0) {
                        MapJump__Fii(0, -1);
                    }

                    break;
                case GAME_MODE_LOADER:
                    loop_result = func_01DC1510();

                    if (loop_result != 0) {
                        mode = GAME_MODE_DUNGEON;
                    }

                    break;
                case GAME_MODE_UNUSED_6:
                case GAME_MODE_UNUSED_8:
                    break;
                default:
                    loop_result = func_01DAD980(mode);

                    if (loop_result != 0) {
                        mode = GAME_MODE_MENU;
                    }

                    break;
            }

            GamePad.UpDate();
            SV_CONFIG_SYS *config = (SV_CONFIG_SYS *) SaveData->GetConfigData();
            /* `!x`, not `x == 0`: `!` compiles to sltu/xori/andi-0xff
             * (materialize x!=0, invert, truncate the bool) as retail has;
             * `== 0` compiles to a bare xor/sltiu instead (confirmed by
             * isolated compiles of both forms). */
            GamePad.VibrationEnable(!*(s32 *) &config->values_copy1[10]);
            GamePad.Step();
            MGEndFrame__Fv();

#ifdef PAL
            if (GamePad.On2(PAD_R1) != 0 && GamePad.On2(PAD_R2) != 0 && GamePad.On2(PAD_L1) != 0 && GamePad.On2(PAD_L2) != 0 && GamePad.Down2(PAD_R3) != 0) {
                DebugMode = !DebugMode;
            }
#else
            if (GamePad.On2(PAD_R1) != 0 && GamePad.On2(PAD_R2) != 0 && GamePad.On2(PAD_L1) != 0) {
                GamePad.On2(PAD_L2);
            }
#endif
        } while (loop_result == 0);

        MGBeginFrame__Fv();
        MGEndFrame__Fv();
        sceGsSyncPath(0, 0);

        /* A flat else-if chain in ascending range order, exactly as retail
         * branches it (slti 200 / slti 300 / bne 400 / slti 800, each
         * falling through to the next test) -- an earlier pass's nested
         * `>= 200 { >= 300 { ... } else ... } else ...` shape mirrored
         * the whole structure and couldn't match. */
        if (NextMapNo >= 0) {
            OldMapNo = MapNo;

            if (NextMapNo < 200) {
                mode = GAME_MODE_EDIT;
                MapNo = NextMapNo;
            } else if (NextMapNo < 300) {
                mode = GAME_MODE_DUNGEON;
                MapNo = NextMapNo;
                LocalMapNo = NextMapNo - 200;
                /* Written as a copy of LocalMapNo (retail stores then
                 * reloads it: `sw $2,LocalMapNo; lw $2,LocalMapNo; sw
                 * main_select_menu_no`), not a recomputed
                 * `NextMapNo - 200`. */
                main_select_menu_no = LocalMapNo;
            } else if (NextMapNo == 400) {
                mode = GAME_MODE_OPENING;
                MapNo = NextMapNo;
                LocalMapNo = 0;
                main_select_menu_no = 0;
            } else if ((in_range = NextMapNo < 800) == 0) {
                /* Named-variable trick (see CLAUDE.md): retail's `>= 800`
                 * check is `slti $2,$3,0x320; bnez $2` -- a *real* register
                 * ($v0) where the two range checks above it use $at; only
                 * the named-assignment form (`in_range = NextMapNo < 800`,
                 * tested `== 0` so no bool materialization is needed)
                 * reproduces that without adding an xori. */
                main_select_menu_no = 0;
                strcpy(main_select_param, "title");

                if (NextMapNo == 800) {
                    mode = GAME_MODE_TITLE;
                }

                if (NextMapNo == 801) {
                    mode = GAME_MODE_RUSH_MOVIE;
                }

                MapNo = NextMapNo;
                LocalMapNo = 0;
            }

            if (NextMapNo == 1000) {
                MapNo = -1;
                LocalMapNo = 0;
                mode = GAME_MODE_SAVE;
                /* gp-0x72AC is mc_mode, not d8 (gp-0x72B0) -- same
                 * off-by-one-slot global mixup as the PolyCount store
                 * above, fixed against the retail nm addresses. */
                mc_mode = SAVE_MENU_MODE_ENDING;
            }
        }

        if (CheckTrialEnd__Fv() != 0) {
            mode = GAME_MODE_TRIAL_END;
            MapNo = -1;
            LocalMapNo = -1;
        }

        while (ReadBGSync__Fv() != 0) {
        }
    }
}

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
#ifdef PAL
    MGSetBGColor(0.0f, 0.0f, 128.0f, 128.0f);
#else
    MGSetBGColor(0.0f, 0.0f, background = 128.0f, background);
#endif
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

#ifdef PAL
    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], "Dark Cloud Ver3.07 2001/07/12\n");
#else
    DebugFont.length += sprintf(&DebugFont.text[DebugFont.length], "Dark Cloud Ver2.17 2001/05/11\n");
#endif
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

#ifdef PAL
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
#else
void MemCheckInit() {
    static LOADTEXTURE_INFO texdata[64] = {{"#frame_buff#640#224#4"},
                                           {"img_1/memory.img"},
                                           {gamemode_empty_string}};
    int                     result;
    InitializeDataBuffer();
    SetDataBuffer(&TextureData, 100000);
    SetPacketReadBuffer(10000, 100000);
    TexManager.Initialize(16352);
    result = TexManager.EnterTextureFile(texdata);
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
#endif

#ifdef PAL
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
#else
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
            CRect_i_ rect;
            rect.x = 0;
            rect.y = 0;
            rect.width = 640;
            rect.height = 448;
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("memory01", -1), rect, 0, 0);

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
            rect.x = 0;
            rect.y = 0;
            rect.width = 640;
            rect.height = 448;
            set2DSprite(GetVif1Packet(), TexManager.GetTexture("memory02", -1), rect, 0, 0);

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
#endif

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
extern "C" void *__ct__10CCharacterFv(void *self) {
    CCharacter *chara = (CCharacter *) self;

    __ct__7CObjectFv(self);
    *(void **) ((u_char *) self + 0xA0) = GeneratedCharacterVtable;
    __ct__13CTextureAnimeFv(&chara->tex_anime);
    GeneratedMotionParamCtor(&chara->motion_type);
    GeneratedMotionParamCtor(&chara->shadow_motion_type);
    void *params = ((CCharacter *) self)->motion_storage;
    __construct_array(params, (MWRuntimeObjectFunction) GeneratedMotionParamCtor, NULL, sizeof(MotionParam), CHARA_MOTION_MAX);
    params = ((CCharacter *) self)->shadow_motion_storage;
    __construct_array(params, (MWRuntimeObjectFunction) GeneratedMotionParamCtor, NULL, sizeof(MotionParam), CHARA_MOTION_MAX);
    __construct_array(chara->point_light, GeneratedFakePointLightCtor, NULL, sizeof(CFakePointLight), CHARA_POINT_LIGHT_MAX);
    chara->Initialize();
    return self;
}

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
extern "C" void *__ct__13CTextureAnimeFv(void *self) {
    return GeneratedTextureAnimeCtor(self, NULL, 0);
}

/**
 * Constructs an object at the origin with an identity transform.
 *
 * @mangled __ct__7CObjectFv
 * @address 0x143650
 * @size 0x28
 */
extern "C" void *__ct__7CObjectFv(void *self) {
    return GeneratedObjectCtor(self, 1.0f);
}

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
extern "C" void *__ct__8CHitMarkFv(void *self) {
    __ct__7CObjectFv(self);
    *(void **) ((u_char *) self + 0xA0) = GeneratedHitMarkVtable;
    return self;
}

/**
 * Takes a run of quadwords out of the six-thousand-quadword arena.
 *
 * @mangled Alloc__18CDataAlloc_1_6000_Fi
 * @address 0x143720
 * @size 0x68
 */
u_char *CDataAlloc<1, 6000>::Alloc(int quads) {
    int filled = used + quads;

    if (filled > 6000) {
        printf("Alocation Error! %d/%d\n", used, 6000);

        while (1)
            ;
    }

    u_char *run = (u_char *) block + used * 16;
    used = filled;
    return run;
}
#ifndef PORT
/**
 * Rounds the six-thousand-quadword arena's cursor up to sixty-four bytes.
 *
 * @mangled Align64__18CDataAlloc_1_6000_Fv
 * @address 0x143790
 * @size 0x90
 */
void CDataAlloc<1, 6000>::Align64() {
    asm {
        bne $0, $0, done
    }

    u_int slack = (u_int) ((u_char *) block + used * 16) & 63;

    if (slack) {
        used += (64 - slack) >> 4;
    }

    if (used >= 6000) {
        printf("Alocation Error! %d/%d\n", used, 6000);

        while (1)
            ;
    }

done:;
}
#endif
