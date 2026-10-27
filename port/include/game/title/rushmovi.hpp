#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"
#include "title/script.hpp"

namespace dc::game {

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
class CFrame;
class CFrameVu1;
class CTexAnimeData;

/**
 * The frame the opening movies point their camera at.
 */
extern CFrame *OP_CharaFrame;

/**
 * Copies an actor's current eye and mouth frames over its face plate. Retail's FaceChange; the
 * port names it apart from the other title units' FaceChange, as the PS2 link renames it.
 */
void FaceChangeMovie(int actor_no);

/**
 * Sets up the attract movie's buffers, sound and first scene, and starts its fade.
 */
void RushInit();

/**
 * Runs one frame of the attract movie: script, motion, drawing and sound.
 * Returns 1 once the movie ends or Start skips it, otherwise 0.
 */
int RushLoop();

/**
 * Starts a linear rotation animation on the named frame of a model, from a
 * start angle by a fixed step each frame.
 */
void SetObjAnime(char *name, CFrameVu1 *frame, float *start, float *step);

/**
 * Copies the frame buffer into the water texture, ripples the water surface
 * and draws it.
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

} // namespace dc::game
