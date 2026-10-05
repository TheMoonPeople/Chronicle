#include "common.h"

#include <libvu0.h>

#include <cstdlib>
#include <cstring>

#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "main.hpp"
#include "map.hpp"
#include "mapobject.hpp"
#include "mds.hpp"
#include "mglib.hpp"
#include "objanime.hpp"
#include "title/op_a.hpp"
#include "title/opening.hpp"
#include "title/script.hpp"
#include "vector.hpp"

// The bodies are retail's, which pass string literals as char *.
#pragma clang diagnostic ignored "-Wwritable-strings"

// Retail opening.cpp's scene-definition reader: OPMdsLoad places the scenery in op_a's maps and
// registers op_a's object animations, which the port lays out with the host's CMap and
// OBJ_ANIME_SEQ where opening.cpp declares PS2-sized ones of its own. OPAnalyz and the parse
// state move with it.

static int   debugModeFlag = 1;
static float run_speed = 1.0f;

/* The water plane a definition file may place. The four corners are built from one width and one
   depth, so the quad is always centred on the position and always axis-aligned; everything after
   them is what the simulation reads. */
static sceVu0FVECTOR WaterV1;
static sceVu0FVECTOR WaterV2;
static sceVu0FVECTOR WaterV3;
static sceVu0FVECTOR WaterV4;
static sceVu0FVECTOR WaterPos;
static u_char        WaterR;
static u_char        WaterG;
static u_char        WaterB;
static int           WaterFlag;
static int           WaterMeshW;
static int           WaterMeshH;
static float         WaterShake;
static float         WaterCourant;
static float         WaterDecline;
static float         WaterAmplitude;
static float         WaterRefraction;

/* The two lists the loader appends to as it goes: the shapes a scene is asked collision questions
   against, and the models that cast its shadows. */
static CFrameVu1 *ColModel[64];
static int        ColModelCount;
static CFrameVu1 *ShadowModel[64];
static int        shadowModelCount;

static int animeSpeed1;
static int animeSpeed2;

/* One command of the definition language: the number the loader knows it by, how many arguments it
   takes, and one type per argument. A type-0 argument is a quoted string, a type-1 one a number
   behind a comma and a type-2 one a number standing on its own. The rows are read as plain `int`
   because that is how the reader takes them. */
static int TEIGI_GRD_IMG[] = {0, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_BLD_IMG[] = {1, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_SKY_IMG[] = {2, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_FIRE_IMG[] = {31, 1, TSARG_STRING};
static int TEIGI_GRD[] = {3, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_BLD[] = {4, 8, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_LOD[] = {5, 1, TSARG_STRING};
static int TEIGI_CRD[] = {6, 1, TSARG_STRING};
static int TEIGI_SKY[] = {7, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_FOG[] = {8, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_AMBIENT[] = {9, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_LIGHT_COL[] = {10, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_FARCLIP[] = {11, 1, TSARG_VALUE};
static int TEIGI_BG_COL2[] = {12, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_BG_COL[] = {12, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_NORMALCLIP_OFF[] = {13, 1, TSARG_VALUE};
static int TEIGI_RUN_SPEED[] = {14, 1, TSARG_VALUE};
static int TEIGI_EDIT_FOG[] = {16, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_WATER_SET[] = {17, 5, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_WATER_RGB[] = {18, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_WATER_PARAM[] = {19, 7, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_LEVEL_FAR[] = {21, 4, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_DebugFlag[] = {22, 1, TSARG_VALUE};
static int TEIGI_AnimeSpeed[] = {23, 2, TSARG_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_UPER[] = {24, 8, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_UPR_IMG[] = {25, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_PLIGHT[] = {26, 9, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_ADD_CRD[] = {27, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_DEF_PATS[] = {50, 0};
static int TEIGI_DEF_ENDS[] = {51, 0};
static int TEIGI_S_VOLUME[] = {28, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_PROJECTION[] = {29, 1, TSARG_VALUE};
static int TEIGI_OBJ_ROT[] = {30, 7, TSARG_STRING, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_FIRE[] = {32, 5, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_MAPINFO[] = {80, 1, TSARG_STRING};
static int TEIGI_PT_BASE[] = {52, 2, TSARG_STRING, TSARG_COMMA_VALUE};
static int TEIGI_PT_COLS[] = {54, 1, TSARG_STRING};
static int TEIGI_MAPD[] = {53, 32, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_PT_FIRE[] = {55, 3, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};
static int TEIGI_PT_WATER[] = {56, 9, TSARG_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE, TSARG_COMMA_VALUE};

/* The projection the scene is drawn with, and the editor's own fog beside the renderer's. */
static float  Projection = 800.0f;
static u_char editFogColor[3] = {96, 160, 239};
static float  editFogRate[4] = {1000.0f, 3500.0f, 0.0f, 255.0f};

/* The four distances a level-of-detail object changes model at, which every such object in a scene
   shares because the loader copies them into the map's own row rather than into the object. */
static float levelOfDitialZ[4] = {200.0f, 400.0f, 800.0f, 1600.0f};

/* One of the ninety-six lights a definition file may place. The first word is what the file has
   asked for and everything after it is what it asked for it to be, which is why the parser clears
   only that word and the loader writes the rest. */
struct POINT_LIGHT {
    int    used; /**< Whether the definition file placed this light. */
    float  x;    /**< Horizontal world position. */
    float  y;    /**< Vertical world position. */
    float  z;    /**< Depth world position. */
    u_char r;    /**< Red component of the light's colour. */
    u_char g;    /**< Green component of the light's colour. */
    u_char b;    /**< Blue component of the light's colour. */
    char   unk_13[1];
    u_int  arg7; /**< Seventh argument of the PLIGHT command, truncated to an integer. */
    u_int  arg8; /**< Eighth argument of the PLIGHT command, truncated to an integer. */
    float  arg9; /**< Ninth argument of the PLIGHT command. */
};

static POINT_LIGHT pointLight[96];
static int         pointLightStack;

/* The parse state the two readers share. `argLevel` is the nesting the caller has reached, which
   is what gives each level of a definition its own row of the two argument buffers; the file size
   is read once by the loader and then bounds every walk over the text. */
static int argLevel;
static int teigiFileSize;

/* The definition file is read straight into this one off the disc, and the drive transfers by
   DMA into whole cache lines. */
static char  teigiBuff[4096] __attribute__((aligned(64)));
static char  argStrBuff[128][64];
static float argValBuff[128][64];

/* The four model names one level-of-detail object is built from, as the loader hands them to the
   loader below it: a null entry is a level the file left out. */
static char *LODNameBuff[4];

static int nowObjCnt;
static int nowObjCnt2;
static int nowPartsCnt;

static int skipSpace(char *buf, int pos);
static int checkArg(char *buf, int pos, int *command);

/* Read one definition file and turn it into rows of arguments the loader can walk. Every line
   ending becomes a pair of NULs first, so the rest of the file is one long run of tokens with no
   line structure left in it; each command the text names is then matched by its keyword, its
   arguments are checked into the buffers at the next level, and the level moves on. A line naming
   no command at all is fatal, because a definition file the loader half-understands would place
   half a scene. */
PC_OVERRIDE void OPAnalyz(char *name) {
    char *buffer;
    int   i;
    int   position;
    int   matched;

    argLevel = 0;
    buffer = teigiBuff;

    if (LoadFile(name, teigiBuff, &teigiFileSize) == 0) {
        return;
    }

    for (i = 0; i < teigiFileSize; i++) {
        if (buffer[i] == 13 && buffer[i + 1] == 10) {
            buffer[i + 1] = 0;
            buffer[i] = 0;
        }
    }

    for (i = 0; i < 96; i++) {
        pointLight[i].used = 0;
    }

    pointLightStack = 0;

    position = 0;

    while (position < teigiFileSize) {
        matched = 0;

        position = skipSpace(buffer, position);

        if (memcmp(&buffer[position], "GRD_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_GRD_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "BLD_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_BLD_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "SKY_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_SKY_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "GND", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_GRD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "BLD", 3) == 0 && memcmp(&buffer[position], "BLD_IMG", 7) != 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_BLD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "LOD", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_LOD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "SKY", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_SKY);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FOG", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_FOG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "CRD", 3) == 0) {
            position = skipSpace(buffer, position + 3);
            position = checkArg(buffer, position, TEIGI_CRD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "AMBIENT", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_AMBIENT);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "LIGHT_C", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_LIGHT_COL);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FARCLIP", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_FARCLIP);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        /* Six where the keyword is seven, which is the original's own step: the `2` is left
           standing, and a digit is not a separator, so it is the argument reader that meets it. */
        if (memcmp(&buffer[position], "BG_COL2", 7) == 0) {
            position = skipSpace(buffer, position + 6);
            position = checkArg(buffer, position, TEIGI_BG_COL2);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "BG_COL", 6) == 0) {
            position = skipSpace(buffer, position + 6);
            position = checkArg(buffer, position, TEIGI_BG_COL);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "NORMALCLIP_OFF", 14) == 0) {
            position = skipSpace(buffer, position + 14);
            position = checkArg(buffer, position, TEIGI_NORMALCLIP_OFF);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "RUN_SPEED", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_RUN_SPEED);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "EDIT_FOG", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_EDIT_FOG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "WATER_SET", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_WATER_SET);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "WATER_RGB", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_WATER_RGB);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "WATER_PARAM", 11) == 0) {
            position = skipSpace(buffer, position + 11);
            position = checkArg(buffer, position, TEIGI_WATER_PARAM);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "LEVEL_FAR", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_LEVEL_FAR);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "DebugFlag", 9) == 0) {
            position = skipSpace(buffer, position + 9);
            position = checkArg(buffer, position, TEIGI_DebugFlag);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "AnimeSpeed", 10) == 0) {
            position = skipSpace(buffer, position + 10);
            position = checkArg(buffer, position, TEIGI_AnimeSpeed);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "UPER", 4) == 0) {
            position = skipSpace(buffer, position + 4);
            position = checkArg(buffer, position, TEIGI_UPER);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "UPR_IMG", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_UPR_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PLIGHT", 6) == 0) {
            position = skipSpace(buffer, position + 6);
            position = checkArg(buffer, position, TEIGI_PLIGHT);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "ADD_CRD", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_ADD_CRD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "DEF_PATS", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            argValBuff[argLevel][0] = (float) TEIGI_DEF_PATS[0];
            matched = 1;
            argLevel++;
        }

        if (memcmp(&buffer[position], "DEF_ENDS", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            argValBuff[argLevel][0] = (float) TEIGI_DEF_ENDS[0];
            matched = 1;
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_BASE", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_PT_BASE);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "MAPD", 4) == 0) {
            position = skipSpace(buffer, position + 4);
            position = checkArg(buffer, position, TEIGI_MAPD);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_COLS", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_PT_COLS);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_FIRE", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_PT_FIRE);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PT_WATER", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_PT_WATER);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "S_VOLUME", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_S_VOLUME);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "PROJECTION", 10) == 0) {
            position = skipSpace(buffer, position + 10);
            position = checkArg(buffer, position, TEIGI_PROJECTION);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "OBJ_ROT", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_OBJ_ROT);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "MAPINFO", 7) == 0) {
            position = skipSpace(buffer, position + 7);
            position = checkArg(buffer, position, TEIGI_MAPINFO);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FIRE_IMG", 8) == 0) {
            position = skipSpace(buffer, position + 8);
            position = checkArg(buffer, position, TEIGI_FIRE_IMG);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (memcmp(&buffer[position], "FIRE", 4) == 0) {
            position = skipSpace(buffer, position + 4);
            position = checkArg(buffer, position, TEIGI_FIRE);

            if (position != -1) {
                matched = 1;
            }

            position = skipSpace(buffer, position);
            argLevel++;
        }

        if (!matched) {
            exit__2(-1);
        }

        position = skipSpace(buffer, position);
    }
}

/* Walk the levels the reader left behind and do what each one asks. A level's number stands in
   slot 0 of its row and every command is tested against every level, so a file may repeat a
   command as often as it likes and the last one to run wins. The frame attribute the loader builds
   as it goes is what every model it places is given, which is why the two commands that place a
   whole building save it and put it back: what they set is theirs alone. */
PC_OVERRIDE void OPMdsLoad() {
    CFrameAttr    saved_attr;
    CFrameAttr    attr;
    char          path[4][128];
    CFrameVu1    *lod_frames[4];
    sceVu0FVECTOR light_dir;
    int           i;
    int           j;
    int           k;
    int           light_no;
    int           light_slot;
    CFrameVu1    *frame;
    CFrameVu1    *rot_frame;
    CMapObject   *object;
    CFrame       *shadow;
    float         rx;
    float         ry;
    float         rz;
    float         light_x;

    attr.clip_depth = 20.0f;
    attr.clip_enable = true;
    attr.program_option = 0;

    nowObjCnt = 0;
    nowObjCnt2 = 0;

    for (i = 0; i < argLevel; i++) {
        if (TEIGI_AnimeSpeed[0] == (int) argValBuff[i][0]) {
            animeSpeed1 = (int) argValBuff[i][1];
            animeSpeed2 = (int) argValBuff[i][2];
        }

        if (TEIGI_DebugFlag[0] == (int) argValBuff[i][0]) {
            debugModeFlag = (int) argValBuff[i][1];
        }

        if (TEIGI_LEVEL_FAR[0] == (int) argValBuff[i][0]) {
            levelOfDitialZ[0] = argValBuff[i][1];
            levelOfDitialZ[1] = argValBuff[i][2];
            levelOfDitialZ[2] = argValBuff[i][3];
            levelOfDitialZ[3] = argValBuff[i][4];
        }

        if (TEIGI_WATER_PARAM[0] == (int) argValBuff[i][0]) {
            WaterMeshW = (int) argValBuff[i][1];
            WaterMeshH = (int) argValBuff[i][2];
            WaterShake = argValBuff[i][3];
            WaterCourant = argValBuff[i][4];
            WaterDecline = argValBuff[i][5];
            WaterAmplitude = argValBuff[i][6];
            WaterRefraction = argValBuff[i][7];
        }

        if (TEIGI_WATER_SET[0] == (int) argValBuff[i][0]) {
            float half_width = argValBuff[i][1] / 2.0f;
            float half_depth = argValBuff[i][2] / 2.0f;

            WaterV1[0] = -half_width;
            WaterV1[2] = -half_depth;
            WaterV2[0] = half_width;
            WaterV2[2] = -half_depth;
            WaterV3[0] = -half_width;
            WaterV3[2] = half_depth;
            WaterV4[0] = half_width;
            WaterV4[2] = half_depth;
            WaterPos[0] = argValBuff[i][3];
            WaterPos[1] = argValBuff[i][4];
            WaterPos[2] = argValBuff[i][5];
            WaterFlag = 1;
        }

        if (TEIGI_WATER_RGB[0] == (int) argValBuff[i][0]) {
            WaterR = argValBuff[i][1];
            WaterG = argValBuff[i][2];
            WaterB = argValBuff[i][3];
        }

        if (TEIGI_RUN_SPEED[0] == (int) argValBuff[i][0]) {
            run_speed = argValBuff[i][1];
        }

        if (TEIGI_NORMALCLIP_OFF[0] == (int) argValBuff[i][0]) {
            if (argValBuff[i][1] == 0.0f) {
                attr.program_option = 0;
            } else {
                attr.program_option = 1;
            }
        }

        if (TEIGI_BG_COL2[0] == (int) argValBuff[i][0]) {
            MGSetBGColor((float) (u_int) argValBuff[i][1], (float) (u_int) argValBuff[i][2], (float) (u_int) argValBuff[i][3], 128.0f);
        }

        if (TEIGI_BG_COL[0] == (int) argValBuff[i][0]) {
            MGSetBGColor((float) (u_int) argValBuff[i][1], (float) (u_int) argValBuff[i][2], (float) (u_int) argValBuff[i][3], 128.0f);
        }

        if (TEIGI_FARCLIP[0] == (int) argValBuff[i][0]) {
            attr.far_clip_enable = true;
            attr.far_clip = argValBuff[i][1];
        }

        if (TEIGI_LIGHT_COL[0] == (int) argValBuff[i][0]) {
            light_no = (int) argValBuff[i][7];
            light_dir[0] = argValBuff[i][1];
            light_dir[1] = argValBuff[i][2];
            light_dir[2] = argValBuff[i][3];
            sceVu0Normalize(light_dir, light_dir);
            /* The row's index is materialised between the first component's load and its store,
               and an assignment's right-hand side is emitted before the subscript it is stored
               through, so the component has to already be in a register when the statement that
               computes the index runs. */
            light_x = light_dir[0];
            light_slot = light_no - 1;
            light[0][light_slot] = light_x;
            light[1][light_slot] = light_dir[1];
            light[2][light_slot] = light_dir[2];
            lightcolor[light_slot][0] = argValBuff[i][4];
            lightcolor[light_slot][1] = argValBuff[i][5];
            lightcolor[light_slot][2] = argValBuff[i][6];
            MGSetPLight(light, lightcolor);
        }

        if (TEIGI_AMBIENT[0] == (int) argValBuff[i][0]) {
            ambientlight[0] = argValBuff[i][1];
            ambientlight[1] = argValBuff[i][2];
            ambientlight[2] = argValBuff[i][3];
            MGSetAmbient(ambientlight);
        }

        if (TEIGI_CRD[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            OP_GroundCol = LoadCollisionFile(read_buffer);
        }

        if (TEIGI_FOG[0] == (int) argValBuff[i][0]) {
            op_fogRate[0] = argValBuff[i][1];
            op_fogRate[1] = argValBuff[i][2];
            op_fogRate[2] = argValBuff[i][6];
            op_fogRate[3] = argValBuff[i][7];
            op_fogColor[0] = argValBuff[i][3];
            op_fogColor[1] = argValBuff[i][4];
            op_fogColor[2] = argValBuff[i][5];
            MGSetFogParm(op_fogRate[0], op_fogRate[1], op_fogColor[0], op_fogColor[1], op_fogColor[2], op_fogRate[2], op_fogRate[3]);
            attr.fog_enable = true;
        }

        if (TEIGI_EDIT_FOG[0] == (int) argValBuff[i][0]) {
            editFogRate[0] = argValBuff[i][1];
            editFogRate[1] = argValBuff[i][2];
            editFogRate[2] = argValBuff[i][6];
            editFogRate[3] = argValBuff[i][7];
            editFogColor[0] = argValBuff[i][3];
            editFogColor[1] = argValBuff[i][4];
            editFogColor[2] = argValBuff[i][5];
            attr.fog_enable = true;
        }

        if (TEIGI_PLIGHT[0] == (int) argValBuff[i][0]) {
            pointLight[pointLightStack].used = 1;
            pointLight[pointLightStack].x = 10.0f * argValBuff[i][1];
            pointLight[pointLightStack].y = 10.0f * argValBuff[i][2];
            pointLight[pointLightStack].z = 10.0f * argValBuff[i][3];
            pointLight[pointLightStack].r = argValBuff[i][4];
            pointLight[pointLightStack].g = argValBuff[i][5];
            pointLight[pointLightStack].b = argValBuff[i][6];
            pointLight[pointLightStack].arg7 = argValBuff[i][7];
            pointLight[pointLightStack].arg8 = argValBuff[i][8];
            pointLight[pointLightStack].arg9 = argValBuff[i][9];
            pointLightStack++;
        }

        if (TEIGI_GRD[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
            rot_frame = frame;
            frame->SetAttr(attr, 1, 64);
            SetFrameAttr(frame, 1);
            object = OP_GroundMap.SetObject(frame, 0, 0);
            CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            object->SetPosition(position);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            CVector3_f_ rotation(rx, ry, rz);
            object->SetRotation(rotation);
        }

        if (TEIGI_SKY[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            OP_SkyFrame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);

            if (argValBuff[i][2] == 1.0f) {
                OP_SkyFrame->SetAttr(attr, 1, 64);
            }
        }

        if (TEIGI_BLD[0] == (int) argValBuff[i][0]) {
            saved_attr = attr;

            if (argValBuff[i][8] == 0.0f) {
                strcpy(path[0], "sim:");
                strcat(path[0], argStrBuff[i]);
                LoadFile(path[0], (void *) read_buffer, 0);
                frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
                rot_frame = frame;
                frame->SetAttr(attr, 1, 64);
                SetFrameAttr(frame, 1);
                object = OP_BuildingMap.SetObject(nowObjCnt++, frame, 0, 0);
                object->handle = 1;
                object->category_no = 5;
                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            } else {
                for (j = 0; j < 4; j++) {
                    if (argStrBuff[i + j][0] != 0) {
                        strcpy(path[j], "sim:");
                        strcat(path[j], argStrBuff[i + j]);
                        LODNameBuff[j] = path[j];
                    } else {
                        LODNameBuff[j] = 0;
                    }
                }

                LoadLODData(lod_frames, LODNameBuff, read_buffer, 0);

                object = OP_BuildingMap.GetObject(nowObjCnt++);
                object->handle = 1;
                object->category_no = 1;

                CMapCategoryAttr *category = &OP_BuildingMap.category[1];

                for (k = 0; k < 4; k++) {
                    category->lod[k] = levelOfDitialZ[k];
                }

                category->lowest = 0;
                category->highest = 3;

                for (j = 0; j < 4; j++) {
                    object->SetFrame(lod_frames[j], j);

                    if (lod_frames[j] != 0) {
                        lod_frames[j]->SetAttr(attr, 1, 64);
                        SetFrameAttr(lod_frames[j], 1);
                        shadow = frame->SearchFrame("shadow");

                        if (shadow != 0) {
                            attr.alpha_ref = 1;
                            shadow->SetAttr(attr, 1, 64);
                            SetFrameAttr(shadow, 1);
                        }
                    }
                }

                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            }

            attr = saved_attr;
        }

        if (TEIGI_UPER[0] == (int) argValBuff[i][0]) {
            saved_attr = attr;

            if (argValBuff[i][8] == 0.0f) {
                strcpy(path[0], "sim:");
                strcat(path[0], argStrBuff[i]);
                LoadFile(path[0], (void *) read_buffer, 0);
                frame = LoadMDSFile(read_buffer, &MapDataBuffer, 2, 0, 0);
                frame->SetAttr(attr, 1, 64);
                SetFrameAttr(frame, 1);
                object = OP_BuildingMap2.SetObject(nowObjCnt2++, frame, 0, 0);
                object->handle = 1;
                object->category_no = 5;
                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            } else {
                for (j = 0; j < 4; j++) {
                    if (argStrBuff[i + j][0] != 0) {
                        strcpy(path[j], "sim:");
                        strcat(path[j], argStrBuff[i + j]);
                        LODNameBuff[j] = path[j];
                    } else {
                        LODNameBuff[j] = 0;
                    }
                }

                LoadLODData(lod_frames, LODNameBuff, read_buffer, 0);

                object = OP_BuildingMap2.GetObject(nowObjCnt++);
                object->handle = 1;
                object->category_no = 1;

                CMapCategoryAttr *category = &OP_BuildingMap2.category[1];

                for (k = 0; k < 4; k++) {
                    category->lod[k] = levelOfDitialZ[k];
                }

                category->lowest = 0;
                category->highest = 3;

                for (j = 0; j < 4; j++) {
                    object->SetFrame(lod_frames[j], j);

                    if (lod_frames[j] != 0) {
                        lod_frames[j]->SetAttr(attr, 1, 64);
                        SetFrameAttr(lod_frames[j], 1);
                        shadow = frame->SearchFrame("shadow");

                        if (shadow != 0) {
                            attr.alpha_ref = 1;
                            shadow->SetAttr(attr, 1, 64);
                            SetFrameAttr(shadow, 1);
                        }
                    }
                }

                CVector3_f_ position(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
                object->SetPosition(position);
                rx = PI * argValBuff[i][5] / 180.0f;
                ry = PI * argValBuff[i][6] / 180.0f;
                rz = PI * argValBuff[i][7] / 180.0f;
                CVector3_f_ rotation(rx, ry, rz);
                object->SetRotation(rotation);
            }

            attr = saved_attr;
        }

        if (TEIGI_S_VOLUME[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            ShadowModel[shadowModelCount] = LoadMDSFile(read_buffer, 14, 0);
            ShadowModel[shadowModelCount]->SetPosition(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            ShadowModel[shadowModelCount]->SetRotation(rx, ry, rz);
            shadowModelCount++;
        }

        if (TEIGI_ADD_CRD[0] == (int) argValBuff[i][0]) {
            strcpy(path[0], "sim:");
            strcat(path[0], argStrBuff[i]);
            LoadFile(path[0], (void *) read_buffer, 0);
            ColModel[ColModelCount] = LoadCollisionFile(read_buffer);
            ColModel[ColModelCount]->SetPosition(10.0f * argValBuff[i][2], 10.0f * argValBuff[i][3], 10.0f * argValBuff[i][4]);
            rx = PI * argValBuff[i][5] / 180.0f;
            ry = PI * argValBuff[i][6] / 180.0f;
            rz = PI * argValBuff[i][7] / 180.0f;
            ColModel[ColModelCount]->SetRotation(rx, ry, rz);
            ColModelCount++;
        }

        if (TEIGI_PROJECTION[0] == (int) argValBuff[i][0]) {
            Projection = argValBuff[i][1];
        }

        if (TEIGI_OBJ_ROT[0] == (int) argValBuff[i][0]) {
            OP_AnimeSeq[OP_AnimeSeqRot].Initialize();
            OP_AnimeSeq[OP_AnimeSeqRot].property = 0;
            OP_AnimeSeq[OP_AnimeSeqRot].mode = 0;
            OP_AnimeSeq[OP_AnimeSeqRot].from[0] = argValBuff[i][2];
            OP_AnimeSeq[OP_AnimeSeqRot].from[1] = argValBuff[i][3];
            OP_AnimeSeq[OP_AnimeSeqRot].from[2] = argValBuff[i][4];
            OP_AnimeSeq[OP_AnimeSeqRot].step[0] = argValBuff[i][5];
            OP_AnimeSeq[OP_AnimeSeqRot].step[1] = argValBuff[i][6];
            OP_AnimeSeq[OP_AnimeSeqRot].step[2] = argValBuff[i][7];
            strcpy(OP_AnimeSeq[OP_AnimeSeqRot].frame_name, argStrBuff[i]);
            InitObjAnime(rot_frame, &OP_AnimeSeq[OP_AnimeSeqRot]);
            OP_AnimeSeqRot++;
        }

        if (TEIGI_FIRE[0] == (int) argValBuff[i][0]) {
            OP_FirePosition[OP_FireList][0] = argValBuff[i][1];
            OP_FirePosition[OP_FireList][1] = argValBuff[i][2];
            OP_FirePosition[OP_FireList][2] = argValBuff[i][3];
            OP_FireScale[OP_FireList] = argValBuff[i][4];
            OP_FireFlg[OP_FireList] = (int) argValBuff[i][5];
            OP_FireList++;
        }

        if (TEIGI_DEF_ENDS[0] == (int) argValBuff[i][0]) {
            nowPartsCnt++;
        }
    }
}

/**
 * The run of separators standing before a token. The loader turns every line ending into a pair of
 * NULs before anything reads the text, so a NUL is a separator here and a comment is the run up to
 * the next one; the ideographic space is two bytes, which is why the skip is a loop over the file
 * rather than a walk over one kind of byte.
 *
 * @mangled skipSpace__FPci
 * @address 0x1DB4830
 * @size 0x110
 * @unknownret
 */
static int skipSpace(char *buffer, int position) {
    int skipped;

    while (position < teigiFileSize) {
        skipped = 0;

        if (memcmp(&buffer[position], "\x81\x40", 2) == 0) {
            position++;
            skipped = 1;
        }

        if (buffer[position] == ' ') {
            skipped = 1;
        }

        if (buffer[position] == '\t') {
            skipped = 1;
        }

        if (buffer[position] == '\0') {
            position++;
            skipped = 1;
        }

        if (memcmp(&buffer[position], "//", 2) == 0) {
            while (buffer[position] != '\0') {
                position++;
            }

            position++;
            skipped = 1;
        }

        if (!skipped) {
            return position;
        }

        position++;
    }

    return teigiFileSize;
}

/**
 * One command's arguments, described by the row the caller passes: its own number, how many
 * arguments it takes, and one type per argument. A type-0 argument is a quoted string and lands in
 * the string buffer; the other two are numbers and land in the value buffer beside the command
 * number, and differ only in whether a comma has to stand in front. Anything the forms do not
 * cover hands back -1.
 *
 * @mangled checkArg__FPciPi
 * @address 0x1DB4940
 * @size 0x574
 * @unknownret
 */
static int checkArg(char *buffer, int position, int *command) {
    int i;
    int cursor;
    int char_count;
    int accepted;

    cursor = position;

    if (command[1] == 0) {
        return position;
    }

    for (i = 0; i < command[1]; i++) {
        argValBuff[argLevel][0] = (float) command[0];

        switch (command[2 + i]) {
            case TSARG_STRING:
                if (buffer[cursor] != '"') {
                    return -1;
                }

                cursor++;

                for (char_count = 0; char_count < 64; char_count++) {
                    if (buffer[cursor] == '"') {
                        argStrBuff[argLevel][char_count] = '\0';
                        cursor++;
                        break;
                    }

                    argStrBuff[argLevel][char_count] = buffer[cursor];
                    cursor++;
                }

                if (char_count == 64) {
                    return -1;
                }

                cursor = skipSpace(buffer, cursor);
                break;

            case TSARG_COMMA_VALUE:
                if (buffer[cursor] != ',') {
                    return -1;
                }

                cursor = skipSpace(buffer, cursor + 1);

                if (memcmp(&buffer[cursor], "ON", 2) == 0) {
                    argValBuff[argLevel][1 + i] = 1.0f;
                    cursor += 2;
                } else if (memcmp(&buffer[cursor], "OFF", 3) == 0) {
                    argValBuff[argLevel][1 + i] = 0;
                    cursor += 3;
                } else {
                    accepted = 0;

                    if (buffer[cursor] == '-') {
                        accepted = 1;
                    }

                    if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                        accepted = 1;
                    }

                    if (!accepted) {
                        return -1;
                    }

                    argValBuff[argLevel][1 + i] = (float) atof(&buffer[cursor]);

                    for (char_count = 0; char_count < 32; char_count++) {
                        accepted = 0;

                        if (buffer[cursor] == '-') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] == '.') {
                            cursor++;
                            accepted = 1;
                        }

                        if (!accepted) {
                            break;
                        }
                    }

                    if (char_count == 32) {
                        return -1;
                    }
                }

                cursor = skipSpace(buffer, cursor);
                break;

            case TSARG_VALUE:
                if (memcmp(&buffer[cursor], "ON", 2) == 0) {
                    argValBuff[argLevel][1 + i] = 1.0f;
                    cursor += 2;
                } else if (memcmp(&buffer[cursor], "OFF", 3) == 0) {
                    argValBuff[argLevel][1 + i] = 0;
                    cursor += 3;
                } else {
                    accepted = 0;

                    if (buffer[cursor] == '-') {
                        accepted = 1;
                    }

                    if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                        accepted = 1;
                    }

                    if (!accepted) {
                        return -1;
                    }

                    argValBuff[argLevel][1 + i] = (float) atof(&buffer[cursor]);

                    for (char_count = 0; char_count < 32; char_count++) {
                        accepted = 0;

                        if (buffer[cursor] == '-') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] >= '0' && buffer[cursor] <= '9') {
                            cursor++;
                            accepted = 1;
                        }

                        if (buffer[cursor] == '.') {
                            cursor++;
                            accepted = 1;
                        }

                        if (!accepted) {
                            break;
                        }
                    }

                    if (char_count == 32) {
                        return -1;
                    }
                }

                cursor = skipSpace(buffer, cursor);
                break;
        }
    }

    return cursor;
}
