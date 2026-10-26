#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

/**
 * Scenes of the attract movie, as CScript::scene, load_no and init_no hold them.
 */
// clang-format off
enum RushScene {
    RUSH_SCENE_NONE  = -1, /**< None. */
    RUSH_SCENE_A     = 0,  /**< Dark Heaven Castle. */
    RUSH_SCENE_B     = 1,  /**< Divine beast floor. */
    RUSH_SCENE_C     = 2,  /**< Brownboo. */
    RUSH_SCENE_D     = 3,  /**< White-beard boss map. */
    RUSH_SCENE_E     = 4,  /**< Toan's house. */
    RUSH_SCENE_F     = 5,  /**< Master Utan. */
    RUSH_SCENE_G     = 6,  /**< Outside Goro's house. */
    RUSH_SCENE_H     = 7,  /**< Divine beast boss floor. */
    RUSH_SCENE_I     = 8,  /**< Moon ship dock. */
    RUSH_SCENE_TITLE = 9,  /**< Title. */
};

// clang-format on

class CFireOmni;
class CRunEffect;
class CCharacter;
class CCameraFollow;
class CWater;
class CWind;

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CFrame;
class CFrameVu1;

/**
 * The frame the opening movies point their camera at.
 */
extern CFrame *OP_CharaFrame;

/**
 * @mangled RushInit__Fv
 * @address 0x1DC8C50
 * @size 0x254
 * @unknownret
 */
void RushInit();

/**
 * @mangled RushLoop__Fv
 * @address 0x1DC8EB0
 * @size 0x1E4
 * @unknownret
 */
int RushLoop();

/**
 * @mangled SetObjAnime__FPcP9CFrameVu1PfPf
 * @address 0x1DCAEC0
 * @size 0x160
 * @unknownret
 */
void SetObjAnime(char *name, CFrameVu1 *frame, float *start, float *step);

/**
 * @mangled WaterProcess__Fv__2
 * @address 0x1DCB020
 * @size 0x290
 * @unknownret
 * @note disambiguated by disassembler ("__2" suffix); real retail name has no suffix
 */
void WaterProcess();

/**
 * Fire light of the rush movie.
 */
extern CFireOmni CFire__4;

/**
 * Run effect of the rush movie.
 */
extern CRunEffect CRunFx;

/**
 * Camera-path characters of the rush movie.
 */
extern CCharacter Cam[];

/**
 * Arena the rush movie characters are read into.
 */
extern CDataAlloc2<1> CharaDataBuffer;

/**
 * Texture slot of each rush movie character.
 */
extern char CharaTex[9];

/**
 * Camera that follows the rush movie.
 */
extern CCameraFollow MainCamera__3;

/**
 * Arena the camera paths are read into.
 */
extern CDataAlloc2<1> PathDataBuffer;

/**
 * Scene the rush movie is playing.
 */
extern int SceneNp;

/**
 * Whether the lightning has started.
 */
extern int StartLightning;

/**
 * Texture animation records of the movie.
 */
extern CTexAnimeData TexAnimeDataMovie[];

/**
 * Angle the title camera has turned through.
 */
extern float TitleAngle;

/**
 * Fade the title screen is running.
 */
extern int TitleFade;

/**
 * Frames the title fade has run for.
 */
extern int TitleFadeCnt;

/**
 * Water surface of the rush movie.
 */
extern CWater Water__2;

/**
 * Arena the rush movie water is read into.
 */
extern CDataAlloc2<1> WaterBuffer;

/**
 * Wind of the rush movie.
 */
extern CWind Wind__4;

/**
 * Rate the Atlamillia status display advances at.
 */
extern float atraGetStatusRate;

/**
 * Script state of the rush movie, which the title screen reads as well.
 */
extern class CScript CScript;
