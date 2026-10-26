#include "common.h"

#include <libpkt.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bound.hpp"
#include "btsysscript.hpp"
#include "cloth.hpp"
#include "clothread.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "framevu1.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "rect.hpp"
#include "savedata.hpp"
#include "scriptinterpreter.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "sysmes.hpp"
#include "visualvu1.hpp"

CCloth *pCloth;

/**
 * Last exclusion bound attached while reading one cloth configuration.
 */
static CBound *pBound;

/**
 * Model frame whose cloth configuration is being read.
 */
static CFrameVu1 *ParentFrame;

/**
 * Allocator used for the cloth currently being initialized.
 */
static CDataAlloc2<1> *DataBuffer;

/**
 * Sets and clamps the cloth grid dimensions.
 */
static void CommandSIZE(void **argv);
/**
 * Attaches the cloth to the frame named by the command.
 */
static void CommandFRAME(void **argv);
/**
 * Sets the scale used when calculating cloth normals.
 */
static void CommandNORMAL(void **argv);
/**
 * Sets the cloth follow vector.
 */
static void CommandFOLLOW(void **argv);
/**
 * Sets the cloth stiffness vector.
 */
static void CommandK(void **argv);
/**
 * Sets the cloth response to wind.
 */
static void CommandWINDEFFECT(void **argv);
/**
 * Sets the cloth gravity vector.
 */
static void CommandGRAVITY(void **argv);
/**
 * Selects which cloth rows divide their polygons.
 */
static void CommandPOLYDIVE(void **argv);
/**
 * Creates and attaches an exclusion bound for the cloth.
 */
static void CommandBOUND(void **argv);

/**
 * Names one keyword of the cloth configuration file.
 *
 * The argument list gives each argument's SCRIPT_ARGUMENT_TYPE and ends at -1.
 */
struct COMMAND_INFO {
    char *name;          /**< Keyword the line starts with. */
    int   arg_types[16]; /**< Kind of each argument, ended by -1. */
};

/* The keywords a cloth configuration file may use. */
static COMMAND_INFO Command[9] = {
    {"SIZE",       {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                 },
    {"FRAME",      {SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                           },
    {"NORMAL",     {SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                            },
    {"FOLLOW",     {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                              },
    {"K",          {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                              },
    {"WINDEFFECT", {SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                            },
    {"GRAVITY",    {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                              },
    {"POLYDIV",    {SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                           },
    {"BOUND",      {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}},
};

/* What each of those keywords does, in the same order. */
static void (*CommandExe[9])(void **) = {
    CommandSIZE,
    CommandFRAME,
    CommandNORMAL,
    CommandFOLLOW,
    CommandK,
    CommandWINDEFFECT,
    CommandGRAVITY,
    CommandPOLYDIVE,
    CommandBOUND,
};

static int GetArg(input_str &input, int *arg_types, void **argv);
static int SearchCommand(input_str &input, int *command);
static int SkipSpace(input_str &input);
static int CheckChar(char ch);

/**
 * Reads one model's cloth description and attaches the simulation.
 *
 * @mangled InitCloth__FP9CFrameVu1R9input_strP14CDataAlloc2_1_
 * @address 0x13F9C0
 * @size 0x1B4
 */
CCloth *InitCloth(CFrameVu1 *frame, input_str &input, CDataAlloc2<1> *alloc) {
    char  arg_storage[16][256];
    void *argv[16];
    int   command;

    DataBuffer = alloc;
    pCloth = new ((u_long128 *) alloc->Alloc(0x856)) CCloth(16, 16, 1.0f);
    ParentFrame = frame;
    pBound = NULL;

    if (frame == NULL) {
        return NULL;
    }

    if (pCloth == NULL) {
        return NULL;
    }

    SkipSpace(input);

    for (int i = 0; i < 16; i++) {
        argv[i] = arg_storage[i];
    }

    while (SearchCommand(input, &command) != 0) {
        if (command >= 9 || command < 0) {
            printf("unknown command!!\n");
            continue;
        }

        int arg_status = GetArg(input, Command[command].arg_types, (void **) argv);

        if (arg_status == 0) {
            break;
        }

        if (arg_status < 0) {
            printf("error!!\n");
        }

        CommandExe[command](argv);
    }

    return pCloth;
}

static void CommandSIZE(void **argv) {
    int num_i = *(int *) argv[0];
    int num_j = *(int *) argv[1];

    if (num_i <= 0) {
        num_i = 1;
    }

    if (num_i > 16) {
        num_i = 16;
    }

    if (num_j <= 0) {
        num_j = 1;
    }

    if (num_j > 16) {
        num_j = 16;
    }

    pCloth->num_i = num_i;
    pCloth->num_j = num_j;
}

/**
 * Names the frame the following model commands act on.
 *
 * @mangled CommandFRAME__FPPv
 * @address 0x13FBE0
 * @size 0x7C
 */
static void CommandFRAME(void **argv) {
    CFrameVu1 *frame = (CFrameVu1 *) ParentFrame->SearchFrame((char *) argv[0]);

    pCloth->frame = frame;

    if (frame != NULL) {
        frame->attr.draw_on = 2;
        CVisualVu1 *visual = frame->GetVisual();
        pCloth->Initialize((MDT_HEADER *) visual->GetMDTDataAddress(), DataBuffer);
    }
}

static void CommandNORMAL(void **argv) {
    pCloth->normal_scale = *(float *) argv[0];
}

static void CommandFOLLOW(void **argv) {
    pCloth->follow[0] = *(float *) argv[0];
    pCloth->follow[1] = *(float *) argv[1];
    pCloth->follow[2] = *(float *) argv[2];
}

static void CommandK(void **argv) {
    pCloth->stiffness[0] = *(float *) argv[0];
    pCloth->stiffness[1] = *(float *) argv[1];
    pCloth->stiffness[2] = *(float *) argv[2];
}

static void CommandWINDEFFECT(void **argv) {
    pCloth->wind_effect = *(float *) argv[0];
}

static void CommandGRAVITY(void **argv) {
    pCloth->gravity[0] = *(float *) argv[0];
    pCloth->gravity[1] = *(float *) argv[1];
    pCloth->gravity[2] = *(float *) argv[2];
}

static void CommandPOLYDIVE(void **argv) {
    char *flags = (char *) argv[0];
    int   row = 0;

    while (row < 16) {
        char flag = *flags;

        if (flag == 0) {
            break;
        }

        pCloth->polygon_divide[row] = (flag != '0');
        flags++;
        row++;
    }
}

/**
 * Adds one exclusion box to the cloth on the current frame.
 *
 * @mangled CommandBOUND__FPPv
 * @address 0x13FDB0
 * @size 0x24C
 */
static void CommandBOUND(void **argv) {
    sceVu0FVECTOR box_vectors[4];
    CBound       *bound = new ((u_long128 *) DataBuffer->Alloc(0x14)) CBound(1.0f, 1.0f, 1.0f);

    if (bound == NULL) {
        return;
    }

    int     arg_index = 0;
    CFrame *frame = ParentFrame->SearchFrame((char *) argv[arg_index++]);

    if (frame == NULL) {
        return;
    }

    for (int i = 0; i < 4; i++) {
        box_vectors[i][0] = *(float *) argv[arg_index];
        box_vectors[i][1] = *(float *) argv[(int) (arg_index + 1)];
        box_vectors[i][2] = *(float *) argv[(int) (arg_index + 2)];
        arg_index += 3;
        box_vectors[i][3] = 1.0f;
    }

    box_vectors[0][3] = 0.0f;
    bound->SetDir(frame, box_vectors[1], box_vectors[2], box_vectors[0], box_vectors[3][0], box_vectors[3][1]);
    float half_x;
    float half_y;
    float half_z;
    half_z = box_vectors[3][2];
    half_y = box_vectors[3][1];
    half_x = box_vectors[3][0];
    bound->extent[0] = half_x;
    bound->extent[1] = half_y;
    bound->extent[2] = half_z;

    if (bound->extent[0] > 0.0f) {
        bound->reciprocal[0] = 1.0f / half_x;
    }

    if (bound->extent[1] > 0.0f) {
        bound->reciprocal[1] = 1.0f / half_y;
    }

    if (bound->extent[2] > 0.0f) {
        bound->reciprocal[2] = 1.0f / half_z;
    }

    bound->friction = *(float *) argv[arg_index];

    if (pBound == NULL) {
        pCloth->bound = bound;
    } else {
        pBound->next = bound;
    }

    pBound = bound;
}

/**
 * Reads one command argument out of a model script.
 *
 * @mangled GetArg__FR9input_strPiPPv__2
 * @address 0x140000
 * @size 0x320
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static int GetArg(input_str &input, int *arg_types, void **argv) {
    char word[256];

    if (!SkipSpace(input)) {
        return 0;
    }

    int arg_count = 0;

    while (arg_types[arg_count++] >= 0)
        ;

    int ch;

    for (int i = 0; i < arg_count - 1; i++) {
        int length = 0;

        if (!SkipSpace(input)) {
            return 0;
        }

        while (1) {
            if (input.get(&ch) == 0) {
                return 0;
            }

            if (ch == ',' || !CheckChar(ch)) {
                break;
            }

            word[length++] = ch;
        }

        word[length] = 0;

        switch (arg_types[i]) {
            case SCRIPT_ARGUMENT_STRING:
                if (word[0] != '"') {
                    return -1;
                }

                for (length = 1;; length++) {
                    char letter = word[length];

                    if (letter == '"') {
                        word[length] = 0;
                        break;
                    }

                    if (letter == 0) {
                        return -1;
                    }
                }

                strcpy((char *) argv[i], word + 1);
                break;
            case SCRIPT_ARGUMENT_INTEGER:
                for (length = 0; word[length] != 0; length++) {
                    char letter = word[length];

                    if (letter < '0' || letter > '9') {
                        return -1;
                    }
                }

                *(int *) argv[i] = atoi(word);
                break;
            case SCRIPT_ARGUMENT_FLOAT:
                for (length = 0; word[length] != 0; length++) {
                    char letter = word[length];

                    if ((letter < '0' || letter > '9') && letter != '.' && letter != '-') {
                        return -1;
                    }
                }

                *(float *) argv[i] = (float) atof(word);
                break;
            default:
                return -1;
        }
    }

    return 1;
}

/**
 * Finds the command table entry a model script's next word names.
 *
 * @mangled SearchCommand__FR9input_strPi__2
 * @address 0x140320
 * @size 0x144
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static int SearchCommand(input_str &input, int *command) {
    char word[256];

    if (!SkipSpace(input)) {
        return 0;
    }

    int length = 0;
    int ch;

    while (1) {
        if (input.get(&ch) == 0) {
            return 0;
        }

        if (!CheckChar(ch)) {
            break;
        }

        word[length++] = ch;
    }

    word[length] = 0;

    for (int i = 0; i < 9; i++) {
        if (strcmp(Command[i].name, word) == 0) {
            *command = i;
            return 1;
        }
    }

    *command = 10;
    return 1;
}

/**
 * Steps a model script past whitespace and comments.
 *
 * @mangled SkipSpace__FR9input_str__2
 * @address 0x140470
 * @size 0x94
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static int SkipSpace(input_str &input) {
    char *text;
    int   pos;

    text = input.data;
    pos = input.pos;

    while (pos < input.size) {
        if (CheckChar(text[pos])) {
            break;
        }

        pos++;
    }

    input.pos = pos;

    if (pos >= input.size) {
        return 0;
    }

    return 1;
}

/**
 * Reports whether a character is not whitespace.
 *
 * @mangled CheckChar__Fc__2
 * @address 0x140510
 * @size 0x60
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
static int CheckChar(char ch) {
    int is_space = 0;

    if (ch == ' ') {
        is_space = 1;
    }

    if (ch == '\t') {
        is_space = 1;
    }

    if (ch == '\n') {
        is_space = 1;
    }

    if (ch == '\r') {
        is_space = 1;
    }

    return !is_space;
}

/**
 * Converts analog-stick displacement into a motion speed and movement state.
 */
int keyCtrl(float stick_x, float stick_y, MOTION_INFO *motion) {
    int move_state = 0;

    if (stick_y != 0.0f || stick_x != 0.0f) {
        float tilt;
        float speed;

        move_state = 2;

        if (stick_x < 0.0f) {
            stick_x *= -1.0f;
        }

        if (stick_y < 0.0f) {
            stick_y *= -1.0f;
        }

        if (stick_x >= stick_y) {
            tilt = stick_x;
        } else {
            tilt = stick_y;
        }

        speed = 0.8f * (0.1f + tilt);
        motion[2].speed = speed;

        if (speed >= 0.7f) {
            motion[2].speed = 0.7f;
        }

        if (stick_x + stick_y >= 0.85f) {
            move_state = 1;
        }
    }

    return move_state;
}

/**
 * Copies one rectangle of video memory to another.
 *
 * @mangled MoveImageTest__FP13sceVif1PacketiiiRC8CRect_i_iiiiii
 * @address 0x140660
 * @size 0x1A4
 */
void MoveImageTest(sceVif1Packet *packet, int src_base, int src_width, int src_format, const CRect_i_ &rect, int dst_base, int dst_width, int dst_format, int dst_x, int dst_y, int direction);

/**
 * Turns a frame toward a heading by one angular step and returns the new yaw.
 */
float unitRotation(CFrameVu1 *frame, float heading) {
    float rotation[4];
    float delta;
    float abs_delta;

    frame->GetRotation(rotation);
    delta = heading - rotation[1];

    if (delta <= 0.0f) {
        abs_delta = -1.0f * delta;
    } else {
        abs_delta = delta;
    }

    if (abs_delta <= PI_D) {
        if (abs_delta <= TWELFTH_PI_D) {
            delta = 0.0f;
        }
    } else {
        abs_delta = TWO_PI_D - abs_delta;

        if (abs_delta <= TWELFTH_PI_D) {
            delta = 0.0f;
        }
    }

    if (delta > 0.0f) {
        if (delta <= PI_D) {
            rotation[1] += TWELFTH_PI;
        } else {
            rotation[1] -= TWELFTH_PI;
        }
    }

    if (delta < 0.0f) {
        if (delta >= -PI_D) {
            rotation[1] -= TWELFTH_PI;
        } else {
            rotation[1] += TWELFTH_PI;
        }
    }

    if (0.0f == delta) {
        rotation[1] = heading;
    }

    if (rotation[1] <= -PI_D) {
        rotation[1] += TWO_PI;
    }

    if (rotation[1] >= PI_D) {
        rotation[1] -= TWO_PI;
    }

    return rotation[1];
}
