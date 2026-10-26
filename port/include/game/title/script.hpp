#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * Commands of the title script, indexing Command.
 */
// clang-format off
enum TitleScriptCommand {
    TSC_OBJ_MOTION2   = 0,  /**< OBJ_MOTION2. */
    TSC_OBJ_MOTION    = 1,  /**< OBJ_MOTION. */
    TSC_OBJ_MOVE      = 2,  /**< OBJ_MOVE. */
    TSC_OBJ_TALK      = 3,  /**< OBJ_TALK. */
    TSC_OBJ_EYE       = 4,  /**< OBJ_EYE. */
    TSC_OBJ_MOUTH     = 5,  /**< OBJ_MOUTH. */
    TSC_OBJ_DISP      = 6,  /**< OBJ_DISP. */
    TSC_OBJ_STEP      = 7,  /**< OBJ_STEP. */
    TSC_CAMERA        = 8,  /**< CAMERA. */
    TSC_SE_STOP       = 9,  /**< SE_STOP. */
    TSC_SE            = 10, /**< SE. */
    TSC_SOUND_FADE    = 11, /**< SOUND_FADE. */
    TSC_FADE_IN       = 12, /**< FADE_IN. */
    TSC_FADE_OUT      = 13, /**< FADE_OUT. */
    TSC_MESSAGE       = 14, /**< MESSAGE. */
    TSC_NEXT_MES      = 15, /**< NEXT_MES. */
    TSC_MES_CLR       = 16, /**< MES_CLR. */
    TSC_LOAD_OBJ      = 17, /**< LOAD_OBJ. */
    TSC_SPRITE        = 18, /**< SPRITE. */
    TSC_BOM           = 19, /**< BOM. */
    TSC_BEEM          = 20, /**< BEEM. */
    TSC_SCENE_LOAD    = 21, /**< SCENE_LOAD. */
    TSC_SCENE         = 22, /**< SCENE. */
    TSC_END           = 23, /**< END. */
    TSC_WAIT_KEY      = 24, /**< WAIT_KEY. */
    TSC_WAIT          = 25, /**< WAIT. */
    TSC_COMMAND_COUNT = 26, /**< Number of commands. */
};

// clang-format on

/**
 * Kinds of title script argument, as CSCRIPT_COMMAND::arg_type holds them.
 */
// clang-format off
enum TitleScriptArgType {
    TSARG_STRING      = 0, /**< Quoted string; only the opening's parser reads it. */
    TSARG_COMMA_VALUE = 1, /**< Comma, then ON, OFF or a number. */
    TSARG_VALUE       = 2, /**< ON, OFF or a number. */
};

// clang-format on

/**
 * Fades a title script asks for, as CScript::fade holds them.
 */
// clang-format off
enum TitleScriptFade {
    TSFADE_NONE      = 0, /**< None. */
    TSFADE_IN_BLACK  = 1, /**< Fade in from black. */
    TSFADE_OUT_BLACK = 2, /**< Fade out to black. */
    TSFADE_IN_WHITE  = 3, /**< Fade in from white. */
    TSFADE_OUT_WHITE = 4, /**< Fade out to white. */
};

// clang-format on

/**
 * Sound channels a title script addresses, as CScript::se_kind holds them.
 */
// clang-format off
enum TitleScriptSeKind {
    TSSE_ALL     = -1, /**< Every channel. */
    TSSE_EFFECT  = 0,  /**< The scene effect port. */
    TSSE_SPECIAL = 1,  /**< The special effect port. */
};

// clang-format on

/**
 * Steps of a title script object's load, as SCRIPT_OBJ::load_step holds them.
 */
// clang-format off
enum TitleScriptLoadStep {
    TSLOAD_NONE   = -1, /**< Nothing to load. */
    TSLOAD_START  = 0,  /**< Starting the read. */
    TSLOAD_WAIT   = 1,  /**< Waiting for the read. */
    TSLOAD_UNPACK = 2,  /**< Unpacking. */
};

// clang-format on

/**
 *          Describes one command recognized by the title script parser.
 */
struct CSCRIPT_COMMAND {
    const char *name;        /**< Text that identifies the command. */
    int         length;      /**< Number of bytes compared from the command name. */
    int         arg_count;   /**< Number of arguments consumed by the command. */
    int         arg_type[8]; /**< Syntax category of each possible argument. */
};

STATIC_ASSERT(sizeof(CSCRIPT_COMMAND) == 0x2C);

/**
 *          Holds the pending title-script requests for one scene object.
 */
struct SCRIPT_OBJ {
    u8    disp; /**< Whether the object is displayed. */
    u8    unk_01[3];
    int   motion;     /**< Motion requested for the object. */
    int   motion_end; /**< Motion selected when the request finishes. */
    int   move;       /**< Movement number the MOVE command requested for the object. */
    u8    move_req;   /**< Whether a movement request is waiting for the scene to pick it up. */
    u8    unk_11[3];
    int   eye;        /**< Eye animation requested for the object. */
    float eye_time;   /**< Duration of the eye animation. */
    int   mouth;      /**< Mouth animation requested for the object. */
    float mouth_time; /**< Duration of the mouth animation. */
    u8    talk;       /**< Whether the object is the active speaker. */
    u8    unk_25[3];
    int   load;      /**< Model requested for the object. */
    int   load_step; /**< Stage the requested model load has reached. @see TitleScriptLoadStep. */
    float step;      /**< Blend step the object's motion advances by. */
};

STATIC_ASSERT(sizeof(SCRIPT_OBJ) == 0x34);

/**
 *          Parses and stores the state of the title overlay's text script.
 */
class CScript {
public:
    int           camera_start; /**< Camera selected by the current command. */
    int           camera_no;    /**< Camera that was selected before the current command. */
    int           fade;         /**< Fade request consumed by the title movie. @see TitleScriptFade. */
    float         fade_speed;   /**< Rate of the pending fade. */
    u8            end;          /**< Whether the script reached its end command. */
    u8            unk_11[3];
    int           sprite;     /**< Sprite request consumed by the scene. */
    int           scene;      /**< Scene selected by the script, as an OpeningScene or a RushScene. */
    int           load_no;    /**< Scene data requested for loading, as an OpeningScene or a RushScene. */
    int           init_no;    /**< Scene selected for initialization, as an OpeningScene or a RushScene. */
    int           mes_wait;   /**< Whether message completion blocks execution. */
    float         wait;       /**< Remaining scripted wait time. */
    u8            reset_flag; /**< Flag cleared when a script loads; nothing else reads or writes it. */
    u8            unk_2D[3];
    SCRIPT_OBJ    obj[23];      /**< Pending state for each scripted object. */
    int           motion_start; /**< First frame of the requested camera motion. */
    int           motion_end;   /**< Last frame of the requested camera motion. */
    float         motion_step;  /**< Step applied to the requested camera motion. */
    u8            motion_req;   /**< Whether a camera motion request is pending. */
    u8            unk_4E1[3];
    int           mes_no;        /**< Message selected for display. */
    float         mes_timer;     /**< Duration assigned to the message. */
    int           mes_x;         /**< Horizontal message position. */
    int           mes_y;         /**< Vertical message position. */
    int           mes_talker;    /**< Object speaking the message. */
    int           mes_tail_x;    /**< Horizontal position of the message tail. */
    int           mes_tail_y;    /**< Vertical position of the message tail. */
    int           mes_page_wait; /**< Frames a message page waits before turning on its own. */
    int           unk_504;
    int           bom_no;  /**< Circular slot used by the next bomb request. */
    u8            bom_req; /**< Whether a bomb request is pending. */
    u8            unk_50D[11];
    sceVu0FVECTOR bom_pos[3];  /**< Positions of pending bomb effects. */
    float         bom_size[4]; /**< Sizes assigned to the bomb slots. */
    int           beem_no;     /**< Circular slot used by the next beam request. */
    u8            beem_req;    /**< Whether a beam request is pending. */
    u8            beem_end;    /**< Whether the beam should finish. */
    u8            unk_566[10];
    sceVu0FVECTOR beem_from[3]; /**< Starting points of beam requests. */
    sceVu0FVECTOR beem_to[3];   /**< Ending points of beam requests. */
    int           se_kind;      /**< Sound group the sound request targets. @see TitleScriptSeKind. */
    int           se_no;        /**< Sound selected by the script. */
    int           se_voice;     /**< Voice selected for the sound. */
    int           se_fade_time; /**< Fade duration assigned to the sound. */
    int           se_stop;      /**< Whether the selected sound should stop. */
    int           bgm_fade;     /**< Volume a requested sound fade heads to; zero when no fade is pending. */
    int           size;         /**< Size in bytes of the loaded script. */
    u8            unk_5E4[20];
    char          data[56000]; /**< Storage containing the loaded script text. */
    char         *p;           /**< Script buffer currently being parsed. */
    int           pos;         /**< Current byte position in the script. */
    float         arg[128];    /**< Arguments parsed for the current command. */
    u8            unk_E2C0[56];

    /**
     *          Loads a title script and resets its execution state.
     *
     * @mangled Load__7CScriptFPCc
     * @address 0x1DABD80
     * @size 0x158
     */
    void Load(const char *name);

    /**
     *          Advances the title script until a command yields execution.
     *
     * @mangled Step__7CScriptFv
     * @address 0x1DABEE0
     * @size 0x228
     */
    void Step();

    /**
     *          Applies one parsed command to the title scene state.
     *
     * @mangled CheckScript__7CScriptFPciP15CSCRIPT_COMMANDi
     * @address 0x1DAC110
     * @size 0xB40
     */
    int CheckScript(char *buffer, int position, CSCRIPT_COMMAND *command, int command_no);

    /**
     *          Parses the arguments required by one title script command.
     *
     * @mangled CheckArg__7CScriptFPciP15CSCRIPT_COMMAND
     * @address 0x1DACC50
     * @size 0x43C
     */
    int CheckArg(char *buffer, int position, CSCRIPT_COMMAND *command);

    /**
     *          Skips whitespace and comments before the next script token.
     *
     * @mangled SkipSpace__7CScriptFPci
     * @address 0x1DAD090
     * @size 0x14C
     */
    int SkipSpace(char *buffer, int position);
};

STATIC_ASSERT(sizeof(CScript) == 0xE300);

/** Command grammar recognized by the title script parser. */
extern CSCRIPT_COMMAND Command[26];

/** Shared title script parser and pending scene requests. */
extern class CScript CScript__2;
