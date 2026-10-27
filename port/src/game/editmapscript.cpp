#include "common.h"

#include <libvu0.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "boxvu0.hpp"
#include "camera.hpp"
#include "camerafollow.hpp"
#include "character.hpp"
#include "chararead.hpp"
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
#include "editmenu.hpp"
#include "editpartsinfo.hpp"
#include "effect.hpp"
#include "effectgroup.hpp"
#include "frame.hpp"
#include "framevu1.hpp"
#include "gamepad.hpp"
#include "gameutil.hpp"
#include "mainselect.hpp"
#include "mapparts.hpp"
#include "mathutil.hpp"
#include "mds.hpp"
#include "menu_misc.hpp"
#include "menuetc.hpp"
#include "menuitemstep.hpp"
#include "mglib.hpp"
#include "npcharacter.hpp"
#include "objanime.hpp"
#include "objectframe.hpp"
#include "rect.hpp"
#include "runeffect.hpp"
#include "savedata.hpp"
#include "scriptinterpreter.hpp"
#include "snd.hpp"
#include "texture.hpp"
#include "weaponlevelup.hpp"

namespace dc::game {

namespace {

using EDIT_SCRIPT_COMMAND = void (*)(void **arguments);

/** One default villager entry stored for each map and list position. */
struct EDIT_CHARA_DATA_ENTRY {
    char *name;         /**< Villager resource name. */
    s16   character_no; /**< Character definition selected for the villager. */
    s16   model_no;     /**< Model variant selected for the villager. */
    int   unk_08;
    float unk_0c;
    s16   hide_when_complete; /**< Whether completed town progress hides the villager. */
    u8    unk_12[2];
};

STATIC_ASSERT(sizeof(EDIT_CHARA_DATA_ENTRY) == 0x14);

void CommandSCN(void **arguments);
void CommandLIGHT_NO(void **arguments);
void CommandAMBIENT(void **arguments);
void CommandLIGHT_C(void **arguments);
void CommandFOG(void **arguments);
void CommandBG_COL(void **arguments);
void CommandBG_COL2(void **arguments);
void CommandDOF(void **arguments);
void CommandCD(void **arguments);
void CommandGRD_IMG(void **arguments);
void CommandBLD_IMG(void **arguments);
void CommandSKY_IMG(void **arguments);
void CommandSUN_IMG(void **arguments);
void CommandWATER_IMG(void **arguments);
void CommandFIRE_IMG(void **arguments);
void CommandFLER_IMG(void **arguments);
void CommandSKY(void **arguments);
void CommandSUN(void **arguments);
void CommandGROUND(void **arguments);
void CommandBUILD(void **arguments);
void CommandWATER(void **arguments);
void CommandWATER_SURFACE(void **arguments);
void CommandEDITAREA(void **arguments);
void CommandBLD_PARTS(void **arguments);
void CommandGRD_PARTS(void **arguments);
void CommandPARTS_INFO(void **arguments);
void CommandROAD_PARTS(void **arguments);
void CommandROAD(void **arguments);
void CommandRIVER_PARTS(void **arguments);
void CommandRIVER(void **arguments);
void CommandBRIDGE_PARTS(void **arguments);
void CommandLAKE_PARTS(void **arguments);
void CommandON_RIVER_PARTS(void **arguments);
void CommandOBJ_ANIME(void **arguments);
void CommandFIRE(void **arguments);
void CommandFLAME(void **arguments);
void CommandBRIGHT(void **arguments);
void CommandOBJECT_TIMER(void **arguments);
void CommandENTRANCE(void **arguments);
void CommandMAPJUMP(void **arguments);
void CommandPEOPLE(void **arguments);
void CommandTIME_TABLE_NO(void **arguments);
void CommandTIME_TABLE(void **arguments);
void CommandTIME_STOP(void **arguments);
void CommandSKY_FOLLOW(void **arguments);
void CommandSHADOW_LEVEL(void **arguments);
void CommandEDITAREA_RECT(void **arguments);
void CommandBGM_NO(void **arguments);
void CommandSOUND_SET(void **arguments);
void CommandREVERBE(void **arguments);
void CommandMOTION_PARTS(void **arguments);
void CommandPEOPLE2(void **arguments);
void CommandSE_AMBIENT_OFF(void **arguments);
void CommandWIND(void **arguments);
void CommandTALK_EVENT(void **arguments);
void CommandCHARA_AMBIENT(void **arguments);
void CommandTALK_ROT(void **arguments);
void CommandTALK_DIR(void **arguments);
void CommandPEOPLE_LIST(void **arguments);

/** Default direction of each of the twelve map lights. */
constexpr float def_light[12][4] = {
    {0.47f,   0.79f, 0.46f, 0.0f},
    {0.0f,    0.89f, 0.46f, 0.0f},
    {-0.47f,  0.79f, 0.41f, 0.0f},
    {-0.83f,  0.5f,  0.25f, 0.0f},
    {-0.996f, 0.5f,  0.02f, 0.0f},
    {-0.9f,   0.5f,  0.02f, 0.0f},
    {0.55f,   0.5f,  0.38f, 0.0f},
    {0.0f,    0.5f,  0.46f, 0.0f},
    {-0.55f,  0.5f,  0.38f, 0.0f},
    {0.9f,    0.5f,  0.02f, 0.0f},
    {0.996f,  0.5f,  0.02f, 0.0f},
    {0.83f,   0.5f,  0.25f, 0.0f},
};

/** Default villagers of each town, twenty per map. */
EDIT_CHARA_DATA_ENTRY EditCharaData[6][20] = {
    {
     {"p47a", 0, 4, 0, 0.3f, 0},
     {"p12a", 4, 5, 1, 0.3f, 1},
     {"p07a", 1, 5, 1, 0.3f, 1},
     {"p01a", 1, 4, 1, 0.3f, 1},
     {"p13a", 5, 5, 1, 0.3f, 1},
     {"p06a", 3, 4, 1, 0.3f, 1},
     {"p09a", 3, 5, 1, 0.3f, 1},
     {"p08a", 6, 4, 1, 0.3f, 1},
     {"p05a", 6, 3, 1, 0.17f, 1},
     {"p03a", 2, 5, 1, 0.5f, 1},
     {"p04a", 2, 4, 1, 0.3f, 1},
     {"p02a", 7, 3, 0, 0.3f, 0},
     {"p10a", 0, 3, 1, 0.2f, 1},
     {"p14a", -1, -1, 1, 0.3f, 0},
     {"c04cat", 0, 5, 1, 0.3f, 0},
     {"p47a", 6, 5, 0, 0.3f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     },
    {
     {"p27a", 4, 4, 1, 0.3f, 1},
     {"p30a", 2, 4, 1, 0.3f, 1},
     {"p29a", 2, 5, 1, 0.3f, 1},
     {"p28a", 1, 5, 1, 0.3f, 1},
     {"p31a", 5, 3, 0, 0.3f, 1},
     {"p25a", 6, 4, 1, 0.3f, 1},
     {"p59a", 14, 4, 1, 0.3f, 1},
     {"p24a", 0, 4, 1, 0.3f, 1},
     {"p26a", 3, 4, 1, 0.3f, 0},
     {"p23a", 3, 5, 1, 0.3f, 0},
     {"p21a", 7, 4, 1, 0.3f, 0},
     {"p22a", 7, 5, 1, 0.3f, 0},
     {"", -1, -1, 1, 0.3f, 0},
     {"", -1, -1, 1, 0.3f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     },
    {
     {"p49a", 3, 5, 0, 0.3f, 1},
     {"p35a", 0, 3, 0, 0.3f, 0},
     {"p37a", 2, 3, 0, 0.3f, 0},
     {"p36a", 1, 4, 0, 0.3f, 0},
     {"p41a", 4, 4, 0, 0.3f, 1},
     {"p39a", 7, 5, 0, 0.3f, 1},
     {"p33a", 8, 5, 0, 0.3f, 1},
     {"p40a", 8, 4, 1, 0.3f, 1},
     {"p43a", 8, 3, 1, 0.3f, 1},
     {"p45a", 5, 2, 0, 0.3f, 1},
     {"p44a", 9, 5, 1, 0.3f, 1},
     {"p34a", 9, 4, 1, 0.3f, 1},
     {"p42a", 6, 3, 1, 0.3f, 1},
     {"", -1, -1, 0, 0.3f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     },
    {
     {"p48a", 0, 3, 1, 0.3f, 1},
     {"p46a", 1, 4, 1, 0.3f, 0},
     {"p50a", 2, 4, 1, 0.3f, 0},
     {"p51a", 3, 5, 1, 0.3f, 1},
     {"p52a", 3, 3, 1, 0.3f, 1},
     {"p53a", 3, 4, 1, 0.3f, 1},
     {"p54a", 5, 5, 1, 0.3f, 1},
     {"p55a", 4, 3, 1, 0.3f, 1},
     {"p56a", 6, 3, 0, 0.3f, 0},
     {"p57a", 7, 3, 1, 0.3f, 1},
     {"p58a", 7, 4, 1, 0.3f, 1},
     {"", -1, -1, 1, 0.3f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     },
    {
     {"p64a", 3, 3, 1, 0.3f, 0},
     {"p62a", 4, 3, 1, 0.3f, 0},
     {"p63a", 9, 1, 1, 0.3f, 0},
     {"p61a", 10, 1, 1, 0.3f, 0},
     {"", -1, -1, 1, 0.3f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     },
    {
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     {"", -1, -1, 0, 0.0f, 0},
     },
};

/** Tags the map script recognises and the arguments each one takes. */
TAG_PARAM Command[61] = {
    {"SCN",            {SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   },
    {"LIGHT_NO",       {SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"AMBIENT",        {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                      },
    {"LIGHT_C",        {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                        },
    {"FOG",            {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                    },
    {"BG_COL",         {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                      },
    {"BG_COL2",        {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                      },
    {"DOF",            {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                    },
    {"CD",             {SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   },
    {"GRD_IMG",        {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"BLD_IMG",        {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"SKY_IMG",        {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
    {"SUN_IMG",        {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"WATER_IMG",      {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"FIRE_IMG",       {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"FLER_IMG",       {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"IMG",            {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
    {"SKY",            {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
    {"SUN",            {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                 },
    {"GROUND",         {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                 },
    {"BUILD",          {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                 },
    {"WATER",          {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                 },
    {"WATER_SURFACE",  {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}},
    {"WATER_SHAKE",    {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                           },
    {"EDITAREA",       {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                     },
    {"BLD_PARTS",      {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"GRD_PARTS",      {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"PARTS_INFO",     {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                  },
    {"ROAD_PARTS",     {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"ROAD",           {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"RIVER_PARTS",    {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"RIVER",          {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"BRIDGE_PARTS",   {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"LAKE_PARTS",     {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"ON_RIVER_PARTS", {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"OBJ_ANIME",      {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                  },
    {"FIRE",           {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                         },
    {"FLAME",          {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                         },
    {"BRIGHT",         {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                         },
    {"OBJECT_TIMER",   {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                     },
    {"ENTRANCE",       {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                    },
    {"MAPJUMP",        {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"PEOPLE",         {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                            },
    {"TIME_TABLE_NO",  {SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"TIME_TABLE",     {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                            },
    {"TIME_STOP",      {-1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           },
    {"SKY_FOLLOW",     {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                },
    {"SHADOW_LEVEL",   {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"EDITAREA_RECT",  {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                 },
    {"BGM_NO",         {SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"SOUND_SET",      {SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"REVERBE",        {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                         },
    {"MOTION_PARTS",   {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                         },
    {"PEOPLE2",        {SCRIPT_ARGUMENT_STRING, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                          },
    {"SE_AMBIENT_OFF", {-1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           },
    {"WIND",           {SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                               },
    {"TALK_EVENT",     {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         },
    {"CHARA_AMBIENT",  {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, SCRIPT_ARGUMENT_FLOAT, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                             },
    {"TALK_ROT",       {SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"TALK_DIR",       {SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  },
    {"PEOPLE_LIST",    {SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, SCRIPT_ARGUMENT_INTEGER, -1}                                                                                                                                                    },
};

/** Handler of each map-script tag, in the order of Command. */
constexpr std::array<EDIT_SCRIPT_COMMAND, 61> CommandExe = {
    CommandSCN,
    CommandLIGHT_NO,
    CommandAMBIENT,
    CommandLIGHT_C,
    CommandFOG,
    CommandBG_COL,
    CommandBG_COL2,
    CommandDOF,
    CommandCD,
    CommandGRD_IMG,
    CommandBLD_IMG,
    CommandSKY_IMG,
    CommandSUN_IMG,
    CommandWATER_IMG,
    CommandFIRE_IMG,
    CommandFLER_IMG,
    CommandIMG,
    CommandSKY,
    CommandSUN,
    CommandGROUND,
    CommandBUILD,
    CommandWATER,
    CommandWATER_SURFACE,
    CommandWATER_SHAKE,
    CommandEDITAREA,
    CommandBLD_PARTS,
    CommandGRD_PARTS,
    CommandPARTS_INFO,
    CommandROAD_PARTS,
    CommandROAD,
    CommandRIVER_PARTS,
    CommandRIVER,
    CommandBRIDGE_PARTS,
    CommandLAKE_PARTS,
    CommandON_RIVER_PARTS,
    CommandOBJ_ANIME,
    CommandFIRE,
    CommandFLAME,
    CommandBRIGHT,
    CommandOBJECT_TIMER,
    CommandENTRANCE,
    CommandMAPJUMP,
    CommandPEOPLE,
    CommandTIME_TABLE_NO,
    CommandTIME_TABLE,
    CommandTIME_STOP,
    CommandSKY_FOLLOW,
    CommandSHADOW_LEVEL,
    CommandEDITAREA_RECT,
    CommandBGM_NO,
    CommandSOUND_SET,
    CommandREVERBE,
    CommandMOTION_PARTS,
    CommandPEOPLE2,
    CommandSE_AMBIENT_OFF,
    CommandWIND,
    CommandTALK_EVENT,
    CommandCHARA_AMBIENT,
    CommandTALK_ROT,
    CommandTALK_DIR,
    CommandPEOPLE_LIST,
};

/** Directory prefixed to every file the map script names. */
std::string CurrentDir;
/** Destination named by the last MAPJUMP tag. */
std::string mapjump_name;

void CopyToBuffer(std::string_view text, char *buffer) {
    std::ranges::copy(text, buffer);
    buffer[text.size()] = '\0';
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
    return std::ranges::equal(left, right, [](char left_char, char right_char) {
        return std::tolower(static_cast<unsigned char>(left_char)) == std::tolower(static_cast<unsigned char>(right_char));
    });
}

/**
 * Sets the directory prefix used while parsing the current map script.
 */
void CommandCD(void **arguments) {
    CurrentDir = static_cast<char *>(arguments[0]);
}

/**
 * Clears the editor-map description and resets every reusable work record.
 */
void InitInfo() {
    std::memset(edit_info, 0, sizeof(EDIT_MAP_INFO));

    for (OBJ_ANIME_SEQ &anime : edit_info->work.obj_anime) {
        anime.property = OBJ_ANIME_PROPERTY_NONE;
    }

    for (auto &effect : edit_info->work.effects.second) {
        effect.kind = EDIT_EFFECT_NONE;
    }

    for (auto &effect : edit_info->work.effects.first) {
        effect.kind = EDIT_EFFECT_NONE;
    }

    for (auto &timer : edit_info->work.object_timers.timers) {
        timer.name[0] = 0;
    }

    for (ED_EVENT_POINT &point : edit_info->work.events.points) {
        point.event_type = ED_EVENT_POINT_NONE;
    }
}

} // namespace

SPI_FUNC_PARAM func_table = {
    "test", {SCRIPT_ARGUMENT_INTEGER, -1},
     test
};

int GameMode = ED_MODE_WALK;

EDIT_MAP_INFO   *edit_info;
int              texture_list;
int              fobject_list;
int              mapobj_list;
int              objanime_list;
int              light_no;
int              partseffect_list;
int              objeffect_list;
int              objtimer_list;
int              event_list;
int              water_list;
int              mapjump_id;
int              people_list;
int              week_no;
char            *objframe;
CMapObject      *mapobj;
CMapParts       *mapparts;
EDIT_WATER_INFO *water_info;
VILLAGER_INFO   *now_villinfo;
int              now_parts_no;
int              binary;
int              edit_rect_list;
int              motion_parts_list;

/**
 * Returns the integer value addressed by a script argument slot.
 */
int test(void **argument) {
    return *static_cast<int *>(argument[0]);
}

/**
 * Reads one editor map's description through the script interpreter.
 */
int LoadEditMapData(EDIT_MAP_INFO *info, char *name, int map_no) {
    int file_size = 0;

    edit_info = info;

    const std::string_view name_view(name);
    const std::size_t      dot = name_view.find('.');
    const std::size_t      extension_at = dot == std::string_view::npos ? name_view.size() : dot + 1;

    if (!LoadFile2(name, read_buffer, &file_size, 0)) {
        // No compiled script: read the text one beside it.
        name[extension_at + 2] = 'g';
        LoadFile(name, read_buffer, &file_size);
    }

    char *script = reinterpret_cast<char *>(read_buffer);
    light_no = 0;
    CurrentDir.clear();
    binary = 0;
    InitInfo();
    texture_list = 0;
    fobject_list = 0;
    mapobj_list = 0;
    objanime_list = 1;
    partseffect_list = 1;
    objeffect_list = 1;
    objtimer_list = 1;
    event_list = 1;
    water_list = 0;
    people_list = 0;
    objframe = nullptr;
    mapobj = nullptr;
    now_villinfo = nullptr;
    now_parts_no = -1;
    mapjump_id = 0;
    mapjump_name.clear();
    week_no = 0;
    water_info = nullptr;
    edit_rect_list = 0;
    motion_parts_list = 0;

    CopyToBuffer("scene.scn", edit_info->scene_name);
    edit_info->sky_follow[0] = 1;
    edit_info->sky_follow[1] = 0;
    edit_info->sky_follow[2] = 1;
    edit_info->shadow_near = 320.0f;
    edit_info->shadow_far = 740.0f;
    edit_info->shadow_level = 2;
    edit_info->shadow_mode = 0x34;
    edit_info->shadow_mode_2 = 0x20;
    edit_info->bgm_no = -1;
    edit_info->sound_set_no = -1;
    edit_info->reverb_mode[0] = 2;
    edit_info->reverb_mode[1] = 4;
    edit_info->reverb_depth[0] = 5;
    edit_info->reverb_depth[1] = 30;
    edit_info->ambient_sound_off = false;
    edit_info->wind[0] = 0.3f;
    edit_info->wind[1] = 0.0f;
    edit_info->wind[2] = 0.1f;
    edit_info->wind[3] = 0.4f;

    for (int &villager : edit_info->people_list) {
        villager = -1;
    }

    if (std::string_view(name).substr(extension_at) == "cfb") {
        // A compiled script: each command is its number and a typed argument list.
        constexpr int          word_size = sizeof(int);
        std::array<void *, 32> arguments{};
        int                    offset = 0;

        binary = 1;

        while (offset < file_size) {
            const int command = *reinterpret_cast<int *>(&script[offset]);
            offset += word_size;

            if (command < 0) {
                break;
            }

            int count = 0;

            for (;;) {
                const int type = *reinterpret_cast<int *>(&script[offset]);
                offset += word_size;

                if (type < 0) {
                    break;
                }

                arguments[count++] = &script[offset];

                if (type == SCRIPT_ARGUMENT_STRING) {
                    offset += static_cast<int>(((std::strlen(&script[offset]) + 4) / 4) * 4);
                } else {
                    offset += word_size;
                }
            }

            CommandExe[command](arguments.data());
        }
    } else {
        // A text script: run it, keeping a compiled copy of each command as it goes.
        CScriptInterpreter interpreter;
        std::vector<int>   cache;

        interpreter.SetScript(script, file_size);
        interpreter.SetTAG(Command, 61);
        interpreter.SetFunction(&func_table, 1);

        for (;;) {
            const int command = interpreter.GetNextTAG();

            if (command < 0) {
                break;
            }

            void **arguments = interpreter.arguments;
            CommandExe[command](arguments);
            cache.push_back(command);

            for (int argument = 0;; argument++) {
                const int kind = Command[command].argument_types[argument];

                if (kind < 0) {
                    break;
                }

                cache.push_back(kind);

                if (kind == SCRIPT_ARGUMENT_STRING) {
                    const std::string_view text = static_cast<char *>(arguments[argument]);
                    const std::size_t      start = cache.size();
                    cache.resize(start + ((text.size() + 4) / 4), 0);
                    std::ranges::copy(text, reinterpret_cast<char *>(&cache[start]));
                } else {
                    cache.push_back(*static_cast<int *>(arguments[argument]));
                }
            }

            cache.push_back(-1);
        }

        cache.push_back(-1);
    }

    edit_info->images[texture_list].name[0] = '\0';
    edit_info->images[texture_list].type = -1;
    edit_info->map_objects[mapobj_list].name[0][0] = '\0';
    edit_info->map_objects[mapobj_list].name[1][0] = '\0';
    edit_info->map_objects[mapobj_list].name[2][0] = '\0';
    edit_info->map_objects[mapobj_list].name[3][0] = '\0';
    edit_info->obj_anime_count = objanime_list;
    edit_info->parts_effect_count = partseffect_list;
    edit_info->object_effect_count = objeffect_list;
    edit_info->object_timer_count = objtimer_list;
    edit_info->event_count = event_list;

    if (map_no >= 0 && map_no < TOWN_COUNT) {
        for (int i = 0; i < 16; i++) {
            VILLAGER_INFO         *villager = &edit_info->villagers[i];
            EDIT_CHARA_DATA_ENTRY *defaults = &EditCharaData[map_no][i];
            CopyToBuffer(defaults->name, villager->name);
            villager->index = i;
            villager->character_no = defaults->character_no;
            villager->model_no = defaults->model_no;
            villager->hide_when_complete = defaults->hide_when_complete;
        }
    }

    if (edit_info->time_stop == 0) {
        for (int i = 0; i < 12; i++) {
            edit_info->light_direction[i][0][0] = def_light[i][0];
            edit_info->light_direction[i][1][0] = def_light[i][1];
            edit_info->light_direction[i][2][0] = def_light[i][2];
        }
    }

    if (edit_info->sound_set_no < 0) {
        edit_info->ambient_sound_off = true;
    }

    return 1;
}

namespace {

/**
 * Sets the scene resource used by the current editor map.
 */
void CommandSCN(void **arguments) {
    CopyToBuffer(static_cast<char *>(arguments[0]), edit_info->scene_name);
}

/**
 * Selects a zero-based light index from a one-based script value.
 */
void CommandLIGHT_NO(void **arguments) {
    light_no = std::clamp(*static_cast<int *>(arguments[0]) - 1, 0, 11);
}

/**
 * Stores the ambient light colour for the selected light preset.
 */
void CommandAMBIENT(void **arguments) {
    edit_info->ambient[light_no][0] = *static_cast<float *>(arguments[0]);
    edit_info->ambient[light_no][1] = *static_cast<float *>(arguments[1]);
    edit_info->ambient[light_no][2] = *static_cast<float *>(arguments[2]);
    edit_info->ambient[light_no][3] = 128.0f;
}

/**
 * Stores the direction and colour of one light within the selected preset.
 */
void CommandLIGHT_C(void **arguments) {
    sceVu0FVECTOR direction;
    const int     index = *static_cast<int *>(arguments[6]) - 1;
    direction[0] = *static_cast<float *>(arguments[0]);
    direction[1] = *static_cast<float *>(arguments[1]);
    direction[2] = *static_cast<float *>(arguments[2]);
    direction[3] = 0.0f;
    sceVu0Normalize(direction, direction);
    edit_info->light_direction[light_no][0][index] = direction[0];
    edit_info->light_direction[light_no][1][index] = direction[1];
    edit_info->light_direction[light_no][2][index] = direction[2];
    edit_info->light_direction[light_no][3][index] = direction[3];
    edit_info->light_colour[light_no][index][0] = *static_cast<float *>(arguments[3]);
    edit_info->light_colour[light_no][index][1] = *static_cast<float *>(arguments[4]);
    edit_info->light_colour[light_no][index][2] = *static_cast<float *>(arguments[5]);
    edit_info->light_colour[light_no][index][3] = 128.0f;
}

/**
 * Stores fog distances, colour and falloff for the selected light preset.
 */
void CommandFOG(void **arguments) {
    edit_info->fog[light_no].near_distance = *static_cast<float *>(arguments[0]);
    edit_info->fog[light_no].far_distance = *static_cast<float *>(arguments[1]);
    edit_info->fog[light_no].red = *static_cast<int *>(arguments[2]);
    edit_info->fog[light_no].green = *static_cast<int *>(arguments[3]);
    edit_info->fog[light_no].blue = *static_cast<int *>(arguments[4]);
    edit_info->fog[light_no].intensity = *static_cast<float *>(arguments[5]);
    edit_info->fog[light_no].exponent = *static_cast<float *>(arguments[6]);
}

/**
 * Stores the primary background colour for the selected light preset.
 */
void CommandBG_COL(void **arguments) {
    edit_info->background_colour[light_no][0] = *static_cast<float *>(arguments[0]);
    edit_info->background_colour[light_no][1] = *static_cast<float *>(arguments[1]);
    edit_info->background_colour[light_no][2] = *static_cast<float *>(arguments[2]);
    edit_info->background_colour[light_no][3] = 128.0f;
}

/**
 * Stores the secondary background colour for the selected light preset.
 */
void CommandBG_COL2(void **arguments) {
    edit_info->background_colour_2[light_no][0] = *static_cast<float *>(arguments[0]);
    edit_info->background_colour_2[light_no][1] = *static_cast<float *>(arguments[1]);
    edit_info->background_colour_2[light_no][2] = *static_cast<float *>(arguments[2]);
    edit_info->background_colour_2[light_no][3] = 128.0f;
}

/**
 * Stores the map's time-bounded depth-of-field settings.
 */
void CommandDOF(void **arguments) {
    edit_info->dof_start_time = ConvertTime(*static_cast<float *>(arguments[0]));
    edit_info->dof_end_time = ConvertTime(*static_cast<float *>(arguments[1]));
    edit_info->dof_near_level = *static_cast<int *>(arguments[2]);
    edit_info->dof_near = *static_cast<float *>(arguments[3]);
    edit_info->dof_far = *static_cast<float *>(arguments[4]);
    edit_info->dof_far_level = *static_cast<int *>(arguments[5]);
    edit_info->dof_alpha = *static_cast<int *>(arguments[6]);
}

/**
 * Appends an image-resource assignment to the current editor map.
 */
void CommandIMGSub(int image_type, int image_number, char *name) {
    edit_info->images[texture_list].type = image_type;
    edit_info->images[texture_list].number = image_number;
    CopyToBuffer(CurrentDir + name, edit_info->images[texture_list].name);
    texture_list++;
}

/**
 * Assigns a ground image from a parsed map-script command.
 */
void CommandGRD_IMG(void **arguments) {
    CommandIMGSub(1, *static_cast<int *>(arguments[1]), static_cast<char *>(arguments[0]));
}

/**
 * Assigns a building image from a parsed map-script command.
 */
void CommandBLD_IMG(void **arguments) {
    CommandIMGSub(2, *static_cast<int *>(arguments[1]), static_cast<char *>(arguments[0]));
}

/**
 * Assigns a sky image to a one-based sky layer from a parsed map-script command.
 */
void CommandSKY_IMG(void **arguments) {
    int index = *static_cast<int *>(arguments[0]) - 1;

    if (index < 0 || index > 3) {
        return;
    }

    CommandIMGSub(index + 3, *static_cast<int *>(arguments[2]), static_cast<char *>(arguments[1]));
}

/**
 * Assigns a sun image from a parsed map-script command.
 */
void CommandSUN_IMG(void **arguments) {
    CommandIMGSub(7, *static_cast<int *>(arguments[1]), static_cast<char *>(arguments[0]));
}

/**
 * Assigns a water image from a parsed map-script command.
 */
void CommandWATER_IMG(void **arguments) {
    CommandIMGSub(21, *static_cast<int *>(arguments[1]), static_cast<char *>(arguments[0]));
}

/**
 * Assigns a fire image from a parsed map-script command.
 */
void CommandFIRE_IMG(void **arguments) {
    CommandIMGSub(24, *static_cast<int *>(arguments[1]), static_cast<char *>(arguments[0]));
}

/**
 * Assigns a flare image from a parsed map-script command.
 */
void CommandFLER_IMG(void **arguments) {
    CommandIMGSub(23, *static_cast<int *>(arguments[1]), static_cast<char *>(arguments[0]));
}

/**
 * Selects a sky scene layer from a one-based script slot.
 */
void CommandSKY(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);
    index--;

    if (index < 0 || index > 4) {
        return;
    }

    CopyToBuffer(CurrentDir + static_cast<char *>(arguments[1]), edit_info->sky_layers[index].name);
    objframe = edit_info->sky_layers[index].name;
    mapobj = nullptr;
    mapparts = nullptr;
}

/**
 * Selects a sun scene layer from a one-based script slot.
 */
void CommandSUN(void **arguments) {
    int index = *static_cast<int *>(arguments[0]) - 1;

    if (index < 0 || index > 3) {
        return;
    }

    CopyToBuffer(CurrentDir + static_cast<char *>(arguments[1]), edit_info->sun_layers[index].name);
    objframe = edit_info->sun_layers[index].name;
    mapobj = nullptr;
    mapparts = nullptr;
}

/**
 * Places one map object of the given kind, with its models, position and orientation.
 */
void PlaceMapObject(void **arguments, int kind) {
    MAP_PARTS_INFO *object = &edit_info->map_objects[mapobj_list];
    object->kind = kind;

    // The ninth name slot reads the first position argument as text.
    for (int i = 0; i < 9; i++) {
        char *name = static_cast<char *>(arguments[i]);

        if (*name != '\0') {
            CopyToBuffer(CurrentDir + name, object->name[i]);
        } else {
            object->name[i][0] = '\0';
        }
    }

    object->position[0] = *static_cast<float *>(arguments[8]);
    object->position[1] = *static_cast<float *>(arguments[9]);
    object->position[2] = *static_cast<float *>(arguments[10]);
    object->rotation[0] = PI * *static_cast<float *>(arguments[11]) / 180.0f;
    object->rotation[1] = PI * *static_cast<float *>(arguments[12]) / 180.0f;
    object->rotation[2] = PI * *static_cast<float *>(arguments[13]) / 180.0f;
    objframe = nullptr;
    mapobj = reinterpret_cast<CMapObject *>(object);
    mapparts = nullptr;
    mapobj_list++;
}

/**
 * Places one ground object, with its models, position and orientation.
 */
void CommandGROUND(void **arguments) {
    PlaceMapObject(arguments, 1);
}

/**
 * Places one building object, with its models, position and orientation.
 */
void CommandBUILD(void **arguments) {
    PlaceMapObject(arguments, 2);
}

/**
 * Places one water object, with its models, position and orientation.
 */
void CommandWATER(void **arguments) {
    PlaceMapObject(arguments, 0x15);
}

/**
 * Places one water surface and the three corners that span it.
 */
void CommandWATER_SURFACE(void **arguments) {
    if (water_list < 8) {
        EDIT_WATER_INFO *surface = &edit_info->water_surfaces[water_list];
        water_list++;
        CopyToBuffer(static_cast<char *>(arguments[0]), surface->name);
        surface->grid_rows = *static_cast<int *>(arguments[1]);
        surface->grid_columns = *static_cast<int *>(arguments[2]);
        surface->corner_a[0] = *static_cast<float *>(arguments[3]);
        surface->corner_a[1] = *static_cast<float *>(arguments[4]);
        surface->corner_a[2] = *static_cast<float *>(arguments[5]);
        surface->corner_a[3] = 1.0f;
        surface->corner_b[0] = *static_cast<float *>(arguments[6]);
        surface->corner_b[1] = *static_cast<float *>(arguments[7]);
        surface->corner_b[2] = *static_cast<float *>(arguments[8]);
        surface->corner_b[3] = 1.0f;
        surface->corner_c[0] = *static_cast<float *>(arguments[9]);
        surface->corner_c[1] = *static_cast<float *>(arguments[10]);
        surface->corner_c[2] = *static_cast<float *>(arguments[11]);
        surface->corner_c[3] = 1.0f;
        surface->ripple_params[0] = *static_cast<float *>(arguments[12]);
        surface->ripple_params[1] = *static_cast<float *>(arguments[13]);
        surface->ripple_params[2] = *static_cast<float *>(arguments[14]);
        surface->ripple_params[3] = *static_cast<float *>(arguments[15]);
        surface->red = *static_cast<int *>(arguments[16]);
        surface->green = *static_cast<int *>(arguments[17]);
        surface->blue = *static_cast<int *>(arguments[18]);
        surface->follow[0] = *static_cast<int *>(arguments[19]);
        surface->follow[1] = *static_cast<int *>(arguments[20]);
        surface->follow[2] = *static_cast<int *>(arguments[21]);
        surface->parts_no = now_parts_no;
        water_info = surface;
    }
}

} // namespace

void CommandWATER_SHAKE(void **arguments) {
    EDIT_WATER_INFO *info = water_info;

    if (info == nullptr) {
        return;
    }

    // The scan has no upper bound, so each slot's address is formed on the integer address rather than by
    // indexing info->wave, which would let the compiler assume the loop stops within its four entries.
    for (std::uintptr_t index = 0;; index++) {
        std::uintptr_t address = reinterpret_cast<std::uintptr_t>(info) + (index * sizeof(sceVu0FVECTOR));
        auto          *wave = reinterpret_cast<EDIT_WATER_WAVE_VIEW *>(address);

        if (wave->power == 0.0f && wave->range == 0.0f) {
            wave->row = static_cast<float>(*static_cast<int *>(arguments[0]));
            wave->column = static_cast<float>(*static_cast<int *>(arguments[1]));
            wave->range = *static_cast<float *>(arguments[3]);
            wave->power = *static_cast<float *>(arguments[2]);
            return;
        }
    }
}

namespace {

/**
 * Stores one editable-area resource and its placement parameters.
 */
void CommandEDITAREA(void **arguments) {
    EDIT_AREA_INFO *area = &edit_info->parts_work.edit_areas[*static_cast<int *>(arguments[0])];
    char           *name = static_cast<char *>(arguments[1]);

    if (*name != '\0') {
        CopyToBuffer(CurrentDir + name, area->name);
    } else {
        area->name[0] = '\0';
    }

    area->width = *static_cast<int *>(arguments[2]);
    area->height = *static_cast<int *>(arguments[3]);
    area->unit_size = *static_cast<float *>(arguments[4]);
    area->unit_alt = *static_cast<float *>(arguments[5]);
    area->origin_x = *static_cast<float *>(arguments[6]);
    area->origin_y = *static_cast<float *>(arguments[7]);
    area->origin_z = *static_cast<float *>(arguments[8]);
}

/**
 * Expands a map-part resource name into the seven model files the loader reads,
 * or keeps it as it stands when the script named a part-definition file.
 */
void GenMdsName(MAP_PARTS_INFO *info, const std::string &name) {
    if (name.ends_with("pts")) {
        CopyToBuffer(std::string_view(name).substr(0, name.find('.')), info->name[0]);
        return;
    }

    CopyToBuffer(name + "0.mds", info->name[0]);
    CopyToBuffer(name + "1.mds", info->name[1]);
    CopyToBuffer(name + "2.mds", info->name[2]);
    CopyToBuffer(name + "m.mds", info->name[3]);
    CopyToBuffer(name + "s.mds", info->name[4]);
    CopyToBuffer(name + "a.mds", info->name[5]);
    CopyToBuffer(name + "k.mds", info->name[6]);
    info->name[7][0] = '\0';
}

/**
 * Defines one building map part from a parsed script command.
 */
void CommandBLD_PARTS(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 24) {
        return;
    }

    now_parts_no = index;
    MAP_PARTS_INFO *parts = &edit_info->parts_work.general.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 2;
    parts->subtype = MAP_PARTS_SUBTYPE_NONE;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Defines one ordinary ground map part from a parsed script command.
 */
void CommandGRD_PARTS(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 24) {
        return;
    }

    now_parts_no = index;
    MAP_PARTS_INFO *parts = &edit_info->parts_work.general.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;
    parts->subtype = MAP_PARTS_SUBTYPE_NONE;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Reads the cell grid and attached resources of the map part being defined.
 */
void CommandPARTS_INFO(void **arguments) {
    int index = now_parts_no;

    if (index < 0 || index >= 24) {
        return;
    }

    EDIT_PARTS_DEF *def = &edit_info->parts_work.parts_defs.defs[index];
    def->width = *static_cast<int *>(arguments[0]);
    def->height = *static_cast<int *>(arguments[1]);
    int   at = 0;
    int   count = 0;
    char *shape = static_cast<char *>(arguments[2]);

    for (auto &cell : def->cells) {
        cell = 0;
    }

    while (shape[at] != '\0') {
        const char c = shape[at];

        if (c >= '0' && c <= '9') {
            def->cells[count] = c - '0';
            count++;
        } else {
            switch (c) {
                case 'd':
                    def->cells[count] = 0x81;
                    count++;
                    break;
                case 'x':
                    def->cells[count] = 0x80;
                    count++;
                    break;
                default:
                    break;
            }
        }

        if (count >= def->width * def->height) {
            break;
        }

        at++;
    }

    def->kind = *static_cast<int *>(arguments[3]);
    int argument = 4;

    for (int i = 0; i < 6; i++) {
        def->element_ids[i] = *static_cast<int *>(arguments[argument++]);
        CopyToBuffer(static_cast<char *>(arguments[argument++]), def->element_names[i]);
    }
}

/**
 * Defines a road-classified map part in the general part table.
 */
void CommandROAD_PARTS(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 24) {
        return;
    }

    now_parts_no = index;
    MAP_PARTS_INFO *parts = &edit_info->parts_work.general.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;
    parts->subtype = MAP_PARTS_SUBTYPE_ROAD;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Defines one road map part in the dedicated road table.
 */
void CommandROAD(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 6) {
        return;
    }

    MAP_PARTS_INFO *parts = &edit_info->parts_work.roads.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;
    parts->subtype = MAP_PARTS_SUBTYPE_ROAD;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Defines a river-classified map part in the general part table.
 */
void CommandRIVER_PARTS(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 24) {
        return;
    }

    now_parts_no = index;
    MAP_PARTS_INFO *parts = &edit_info->parts_work.general.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;
    parts->subtype = MAP_PARTS_SUBTYPE_RIVER;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Defines one river, bridge or lake part in the dedicated river table.
 */
void CommandRIVER(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 16) {
        return;
    }

    MAP_PARTS_INFO *parts = &edit_info->parts_work.rivers.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;

    if (index == 7) {
        parts->kind = 0x15;
    }

    parts->subtype = MAP_PARTS_SUBTYPE_RIVER;

    if (index == 6) {
        parts->subtype = MAP_PARTS_SUBTYPE_BRIDGE;
    }

    if (index + 8 < 16) {
        char *second_name = static_cast<char *>(arguments[4]);

        if (*second_name != '\0') {
            CopyToBuffer(CurrentDir + second_name, parts->name[7]);
        } else {
            parts->name[7][0] = '\0';
        }

        objframe = nullptr;
        mapobj = nullptr;
        mapparts = reinterpret_cast<CMapParts *>(parts);
    }
}

/**
 * Defines a bridge-classified map part in the general part table.
 */
void CommandBRIDGE_PARTS(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 24) {
        return;
    }

    now_parts_no = index;
    MAP_PARTS_INFO *parts = &edit_info->parts_work.general.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;
    parts->subtype = MAP_PARTS_SUBTYPE_BRIDGE;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Defines a lake-classified map part and its secondary resource.
 */
void CommandLAKE_PARTS(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 24) {
        return;
    }

    now_parts_no = index;
    MAP_PARTS_INFO *parts = &edit_info->parts_work.general.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
        CopyToBuffer(CurrentDir + static_cast<char *>(arguments[4]), parts->name[7]);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;
    parts->subtype = MAP_PARTS_SUBTYPE_LAKE;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Defines an on-river map part and its secondary resource.
 */
void CommandON_RIVER_PARTS(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);

    if (index < 0 || index >= 24) {
        return;
    }

    now_parts_no = index;
    MAP_PARTS_INFO *parts = &edit_info->parts_work.general.parts[index];
    char           *source_name = static_cast<char *>(arguments[1]);

    if (*source_name != '\0') {
        GenMdsName(parts, CurrentDir + source_name);
        CopyToBuffer(CurrentDir + static_cast<char *>(arguments[4]), parts->name[7]);
    } else {
        parts->name[0][0] = '\0';
    }

    parts->parts_no = *static_cast<int *>(arguments[2]);
    parts->lift = *static_cast<float *>(arguments[3]);
    parts->kind = 1;
    parts->subtype = MAP_PARTS_SUBTYPE_ON_RIVER;
    objframe = nullptr;
    mapobj = nullptr;
    mapparts = reinterpret_cast<CMapParts *>(parts);
}

/**
 * Attaches one object animation to the map part or object being defined.
 */
void CommandOBJ_ANIME(void **arguments) {
    if (objanime_list >= 128) {
        std::println("obj_anime over!!!");
        return;
    }

    OBJ_ANIME_SEQ *anime = &edit_info->work.obj_anime[objanime_list];
    s16           *slots;
    anime->property = *static_cast<int *>(arguments[0]);

    if (anime->property < 0 || anime->property >= 4) {
        return;
    }

    anime->mode = *static_cast<int *>(arguments[1]);
    CopyToBuffer(static_cast<char *>(arguments[2]), anime->frame_name);
    anime->from[0] = *static_cast<float *>(arguments[3]);
    anime->from[1] = *static_cast<float *>(arguments[4]);
    anime->from[2] = *static_cast<float *>(arguments[5]);
    anime->step[0] = *static_cast<float *>(arguments[6]);
    anime->step[1] = *static_cast<float *>(arguments[7]);
    anime->step[2] = *static_cast<float *>(arguments[8]);
    anime->to[0] = *static_cast<float *>(arguments[9]);
    anime->to[1] = *static_cast<float *>(arguments[10]);
    anime->to[2] = *static_cast<float *>(arguments[11]);

    if (objframe != nullptr) {
        slots = reinterpret_cast<EDIT_SCENE_LAYER_INFO *>(objframe)->obj_anime;
    }

    if (mapobj != nullptr) {
        slots = reinterpret_cast<MAP_PARTS_INFO *>(mapobj)->anime;
    }

    if (mapparts != nullptr) {
        slots = reinterpret_cast<MAP_PARTS_INFO *>(mapparts)->anime;
    }

    for (int i = 0; i < 8; i++) {
        if (slots[i] <= 0) {
            slots[i] = objanime_list;
            break;
        }
    }

    objanime_list++;
}

/**
 * Accepts the legacy fire command, which has no runtime effect.
 */
void CommandFIRE(void ** /*arguments*/) {
}

/**
 * Accepts the legacy flame command, which has no runtime effect.
 */
void CommandFLAME(void ** /*arguments*/) {
}

/**
 * Accepts the legacy brightness command, which has no runtime effect.
 */
void CommandBRIGHT(void ** /*arguments*/) {
}

/**
 * Accepts the legacy object-timer command, which has no runtime effect.
 */
void CommandOBJECT_TIMER(void ** /*arguments*/) {
}

/**
 * Places the entrance the last map-jump command named, on the side the script
 * chose, and attaches it to the part or object being defined.
 */
void CommandENTRANCE(void **arguments) {
    if (event_list >= 256) {
        std::println("event over!!!");
        return;
    }

    event_list--;
    ED_EVENT_POINT *point = &edit_info->work.events.points[event_list];
    s16            *slots;
    point->event_type = ED_EVENT_POINT_DOOR;
    char *side = static_cast<char *>(arguments[0]);
    CopyToBuffer(mapjump_name, point->destination);
    point->map_no = mapjump_id;
    point->side = 0;

    switch (*side) {
        case 'r':
        case 'R':
            point->side = 1;
            break;
        case 'l':
        case 'L':
            point->side = -1;
            break;
        default:
            break;
    }

    point->position[0] = *static_cast<float *>(arguments[1]);
    point->position[1] = *static_cast<float *>(arguments[2]);
    point->position[2] = *static_cast<float *>(arguments[3]);
    point->rotation[0] = *static_cast<float *>(arguments[4]);
    point->rotation[1] = *static_cast<float *>(arguments[5]);
    point->rotation[2] = *static_cast<float *>(arguments[6]);
    point->trigger_range[0] = *static_cast<float *>(arguments[7]);
    point->trigger_range[1] = *static_cast<float *>(arguments[8]);
    point->trigger_range[2] = *static_cast<float *>(arguments[9]);

    if (objframe == nullptr) {
        if (mapobj != nullptr) {
            slots = reinterpret_cast<MAP_PARTS_INFO *>(mapobj)->events;
        }

        if (mapparts != nullptr) {
            slots = reinterpret_cast<MAP_PARTS_INFO *>(mapparts)->events;
        }

        for (int i = 0; i < 8; i++) {
            if (slots[i] <= 0) {
                slots[i] = event_list;
                break;
            }
        }

        event_list++;
    }
}

/**
 * Names the map that the entrances defined after it lead to.
 */
void CommandMAPJUMP(void **arguments) {
    mapjump_id = *static_cast<int *>(arguments[0]);
    mapjump_name = static_cast<char *>(arguments[1]);

    if (event_list >= 256) {
        std::println("event over!!!");
        return;
    }

    ED_EVENT_POINT *point = &edit_info->work.events.points[event_list];
    s16            *slots;
    point->event_type = ED_EVENT_POINT_DOOR;
    CopyToBuffer(mapjump_name, point->destination);
    point->map_no = mapjump_id;
    point->side = 0;
    point->trigger_range[0] = 10.0f;
    point->trigger_range[1] = 10.0f;
    point->trigger_range[2] = 10.0f;

    if (mapobj != nullptr) {
        slots = reinterpret_cast<MAP_PARTS_INFO *>(mapobj)->events;
    }

    if (mapparts != nullptr) {
        slots = reinterpret_cast<MAP_PARTS_INFO *>(mapparts)->events;
    }

    for (int i = 0; i < 8; i++) {
        if (slots[i] <= 0) {
            slots[i] = event_list;
            break;
        }
    }

    event_list++;
}

/**
 * Appends one villager definition to the map's parsed villager list.
 */
void CommandPEOPLE(void **arguments) {
    if (people_list < 16) {
        VILLAGER_INFO *villager = &edit_info->villagers[people_list];
        villager->index = people_list;
        CopyToBuffer(static_cast<char *>(arguments[0]), villager->name);
        villager->character_no = *static_cast<int *>(arguments[1]);
        villager->model_no = *static_cast<int *>(arguments[2]);
        villager->weapon_no = *static_cast<int *>(arguments[3]);
        villager->initial_motion = *static_cast<int *>(arguments[4]);
        villager->move_speed = *static_cast<float *>(arguments[5]);
        villager->position[0] = *static_cast<float *>(arguments[6]);
        villager->position[1] = *static_cast<float *>(arguments[7]);
        villager->position[2] = *static_cast<float *>(arguments[8]);
        villager->position[3] = 1.0f;
        villager->rotation[0] = 0.0f;
        villager->rotation[1] = *static_cast<float *>(arguments[9]);
        villager->rotation[2] = 0.0f;
        people_list++;
    }
}

/**
 * Selects the weekly time table referenced by subsequent script entries.
 */
void CommandTIME_TABLE_NO(void **arguments) {
    int table_no = *static_cast<int *>(arguments[0]);

    if (table_no < 0 || table_no >= 7) {
        table_no = 0;
    }

    week_no = table_no;
}

/**
 * Replaces one weekly villager time-table row from a script command.
 */
void CommandTIME_TABLE(void **arguments) {
    int table = *static_cast<int *>(arguments[0]);

    if (table >= 0) {
        int *entries = edit_info->time_tables[week_no][table];

        for (int i = 0; i < 16; i++) {
            entries[i] = -1;
        }

        for (int i = 0; i < 6; i++) {
            entries[i] = *static_cast<int *>(arguments[i + 1]);
        }
    }
}

/**
 * Stops time progression for the current editor map.
 */
void CommandTIME_STOP(void ** /*arguments*/) {
    edit_info->time_stop = true;
}

/**
 * Stores the three sky-follow parameters parsed for the current map.
 */
void CommandSKY_FOLLOW(void **arguments) {
    edit_info->sky_follow[0] = *static_cast<int *>(arguments[0]);
    edit_info->sky_follow[1] = *static_cast<int *>(arguments[1]);
    edit_info->sky_follow[2] = *static_cast<int *>(arguments[2]);
}

/**
 * Stores the shadow distances, quality level and modes for the current map.
 */
void CommandSHADOW_LEVEL(void **arguments) {
    edit_info->shadow_level = *static_cast<int *>(arguments[0]);
    edit_info->shadow_near = *static_cast<float *>(arguments[1]);
    edit_info->shadow_far = *static_cast<float *>(arguments[2]);
    edit_info->shadow_mode = *static_cast<int *>(arguments[3]);
    edit_info->shadow_mode_2 = *static_cast<int *>(arguments[4]);
}

/**
 * Appends a rectangular editor region with its corners ordered component-wise.
 */
void CommandEDITAREA_RECT(void **arguments) {
    if (edit_rect_list < 8) {
        EDIT_AREA_RECT_INFO *rect = &edit_info->edit_area_rects[edit_rect_list];
        edit_rect_list++;
        sceVu0FVECTOR second;
        sceVu0FVECTOR first;
        first[0] = *static_cast<float *>(arguments[0]);
        first[1] = *static_cast<float *>(arguments[1]);
        first[2] = *static_cast<float *>(arguments[2]);
        second[0] = *static_cast<float *>(arguments[3]);
        second[1] = *static_cast<float *>(arguments[4]);
        second[2] = *static_cast<float *>(arguments[5]);
        VectorMaxMin(rect->maximum, rect->minimum, second, first);
    }
}

/**
 * Selects and reports the background-music number for the current map.
 */
void CommandBGM_NO(void **arguments) {
    edit_info->bgm_no = *static_cast<int *>(arguments[0]);
    std::println("bgm = {}", edit_info->bgm_no);
}

/**
 * Selects and reports the environmental sound set for the current map.
 */
void CommandSOUND_SET(void **arguments) {
    edit_info->sound_set_no = *static_cast<int *>(arguments[0]);
    std::println("sound set = {}", edit_info->sound_set_no);
}

/**
 * Selects the two environmental reverb presets and their strengths.
 */
void CommandREVERBE(void **arguments) {
    constexpr std::array<const char *, 11> rev = {
        "OFF", "ROOM", "STUDIO_A", "STUDIO_B", "STUDIO_C", "HALL", "SPACE", "ECHO", "DELAY", "PIPE", "MAX"};

    // An unknown preset name selects OFF.
    const auto find_mode = [&rev](std::string_view name) {
        for (int mode = 0; mode < std::ssize(rev); mode++) {
            if (EqualsIgnoreCase(rev[mode], name)) {
                return mode;
            }
        }

        return 0;
    };

    edit_info->reverb_mode[0] = find_mode(static_cast<char *>(arguments[0]));
    edit_info->reverb_depth[0] = *static_cast<int *>(arguments[1]);
    edit_info->reverb_mode[1] = find_mode(static_cast<char *>(arguments[2]));
    edit_info->reverb_depth[1] = *static_cast<int *>(arguments[3]);

    std::println("rev0 = {} {}", rev[edit_info->reverb_mode[0]], edit_info->reverb_depth[0]);
    std::println("rev1 = {} {}", rev[edit_info->reverb_mode[1]], edit_info->reverb_depth[1]);
}

/**
 * Appends a named map-part motion range to the current map.
 */
void CommandMOTION_PARTS(void **arguments) {
    if (motion_parts_list < 4) {
        EDIT_MOTION_PARTS_INFO *motion = &edit_info->motion_parts[motion_parts_list];
        motion_parts_list++;
        CopyToBuffer(static_cast<char *>(arguments[0]), motion->name);
        motion->values[0] = *static_cast<float *>(arguments[1]);
        motion->values[1] = *static_cast<float *>(arguments[2]);
        motion->values[2] = *static_cast<float *>(arguments[3]);
        motion->values[3] = *static_cast<float *>(arguments[4]);
        motion->values[4] = *static_cast<float *>(arguments[5]);
        motion->values[5] = *static_cast<float *>(arguments[6]);
    }
}

/**
 * Appends one villager placed by a second-format script entry.
 */
void CommandPEOPLE2(void **arguments) {
    if (people_list < 16) {
        VILLAGER_INFO *villager = &edit_info->villagers[people_list];
        villager->index = people_list;
        CopyToBuffer(static_cast<char *>(arguments[0]), villager->name);
        villager->character_no = -1;
        villager->model_no = -1;
        villager->weapon_no = *static_cast<int *>(arguments[1]);
        villager->initial_motion = *static_cast<int *>(arguments[2]);
        villager->move_speed = *static_cast<float *>(arguments[3]);
        villager->position[0] = 0.0f;
        villager->position[1] = 0.0f;
        villager->position[2] = 0.0f;
        villager->position[3] = 1.0f;
        villager->rotation[0] = 0.0f;
        villager->rotation[1] = 0.0f;
        villager->rotation[2] = 0.0f;
        villager->talk_event_no = -1;
        villager->talk_event_level = 0;
        now_villinfo = villager;
        people_list++;
    }
}

/**
 * Disables ambient sound for the current editor map.
 */
void CommandSE_AMBIENT_OFF(void ** /*arguments*/) {
    edit_info->ambient_sound_off = true;
}

/**
 * Stores the four wind parameters parsed for the current editor map.
 */
void CommandWIND(void **arguments) {
    edit_info->wind[0] = *static_cast<float *>(arguments[0]);
    edit_info->wind[1] = *static_cast<float *>(arguments[1]);
    edit_info->wind[2] = *static_cast<float *>(arguments[2]);
    edit_info->wind[3] = *static_cast<float *>(arguments[3]);
}

/**
 * Assigns the event number and level used when the current villager is addressed.
 */
void CommandTALK_EVENT(void **arguments) {
    if (now_villinfo != nullptr) {
        now_villinfo->talk_event_no = *static_cast<int *>(arguments[0]);
        now_villinfo->talk_event_level = *static_cast<int *>(arguments[1]);
    }
}

/**
 * Stores a character ambient colour selected by a one-based script slot.
 */
void CommandCHARA_AMBIENT(void **arguments) {
    int index = *static_cast<int *>(arguments[0]);
    index--;

    if (index < 0 || index >= 4) {
        return;
    }

    edit_info->character_ambient[index][0] = *static_cast<float *>(arguments[1]);
    edit_info->character_ambient[index][1] = *static_cast<float *>(arguments[2]);
    edit_info->character_ambient[index][2] = *static_cast<float *>(arguments[3]);
    edit_info->character_ambient[index][3] = 0.0f;
}

/**
 * Sets the rotation constraint used while the current villager talks.
 */
void CommandTALK_ROT(void **arguments) {
    if (now_villinfo != nullptr) {
        now_villinfo->talk_rotation = *static_cast<int *>(arguments[0]);
    }
}

/**
 * Sets the direction constraint used while the current villager talks.
 */
void CommandTALK_DIR(void **arguments) {
    if (now_villinfo != nullptr) {
        now_villinfo->talk_direction = *static_cast<int *>(arguments[0]);
    }
}

/**
 * Copies the fifteen villager identifiers parsed for the current editor map.
 */
void CommandPEOPLE_LIST(void **arguments) {
    for (int i = 0; i < 15; i++) {
        edit_info->people_list[i] = *static_cast<int *>(arguments[i]);
    }
}

} // namespace

/**
 * Reports whether the current editor state should draw the fishing interface.
 */
int FishingDrawCheck() {
    if (GameMode == ED_MODE_FISHING) {
        return 1;
    }

    if (GameMode == ED_MODE_RETURN_MENU_WALK && oldGameMode == ED_MODE_FISHING) {
        return 1;
    }

    return 0;
}

/**
 * Finds a file in the active pack or loads it into the shared read buffer.
 */
void *EdLoadFile(char *name) {
    void *file = GetPackFile(name, nullptr);

    if (file != nullptr) {
        return file;
    }

    if (LoadFile2(name, read_buffer, nullptr, 0) == 0) {
        return nullptr;
    }

    return read_buffer;
}

/**
 * Tests whether a motion crossed a nearby target time during the current step.
 */
int CheckMotionTime(float target, float previous, float current) {
    float difference = target - previous;
    difference = difference < 0.0f ? -difference : difference;

    if (difference > 1.5f) {
        return 0;
    }

    return (current >= target && current < previous) & 0xff;
}

/**
 * Starts ambient playback unless the current map disables ambient sound.
 */
void PlayAmbient(float volume) {
    if (EditMapInfo->ambient_sound_off == 0) {
        EdAmbientPlay(volume);
    }
}

/**
 * Provides the editor hook for stopping all currently managed sound.
 */
void StopAllSound() {
}

/**
 * Sets the editor sound-suppression counter within its valid range.
 */
void EdSetSoundOffCount(int count) {
    sound_off_cnt = std::clamp(count, 0, 10);
}

} // namespace dc::game
