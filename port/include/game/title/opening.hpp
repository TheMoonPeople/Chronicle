#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc_fwd.hpp"

/**
 * Scenes of the opening movie, as CScript::scene, load_no and init_no hold them.
 */
// clang-format off
enum OpeningScene {
    OP_SCENE_NONE           = -1, /**< No scene requested. */
    OP_SCENE_DUNGEON_SQUARE = 0,  /**< The cursed dance in the dungeon square. */
    OP_SCENE_NORUNE         = 1,  /**< Norune Village. */
    OP_SCENE_TOAN_HOUSE     = 2,  /**< Toan's house. */
    OP_SCENE_NORUNE_NIGHT   = 3,  /**< Norune at night. */
    OP_SCENE_DANCE          = 4,  /**< The dance. */
    OP_SCENE_MAJIN          = 5,  /**< The demon's arrival. */
    OP_SCENE_NORUNE_RUINED  = 6,  /**< The village after the demon. */
    OP_SCENE_NORUNE_BURNING = 7,  /**< Norune's destruction. */
    OP_SCENE_SEIREI_KING    = 8,  /**< The spirit king's hall. */
    OP_SCENE_MEADOW         = 9,  /**< The meadow and the explanation captions. */
};

// clang-format on

class CCharacter;
class CCamera;
class CCameraFollow;
struct MOTION_INFO;

/**
 *          Initializes the opening-movie controller and shared scene state.
 *
 * @mangled OpeningInit__Fv
 * @address 0x1DAF1C0
 * @size 0x2FC
 * @unknownret
 */
void OpeningInit();

/**
 *          Loads the active opening scene's background phase.
 *
 * @mangled LoadSceneBG__Fv
 * @address 0x1DAF7E0
 * @size 0x188
 * @unknownret
 */
void LoadSceneBG();

/**
 *          Advances the opening movie and reports its completion state.
 *
 * @mangled OpeningLoop__Fv
 * @address 0x1DAF970
 * @size 0x128
 * @unknownret
 */
int OpeningLoop();

/**
 *          Starts the opening movie's selected background music.
 *
 * @mangled OpBgmPlay__Fv
 * @address 0x1DB0EB0
 * @size 0x90
 * @unknownret
 */
void OpBgmPlay();

/**
 *          Plays a positionally panned opening sound effect.
 *
 * @mangled OpPlayVolPanSE__FPfffiii
 * @address 0x1DB0F40
 * @size 0x158
 * @unknownret
 */
void OpPlayVolPanSE(float *position, float near_dist, float far_dist, int group, int no, int voice);

/**
 *          Updates a positional opening sound effect's volume and pan.
 *
 * @mangled OpSetVolPanSE__FPfffiii
 * @address 0x1DB10A0
 * @size 0x164
 * @unknownret
 */
void OpSetVolPanSE(float *position, float near_dist, float far_dist, int group, int no, int voice);

/**
 *          Plays an opening sound effect at an explicit volume.
 *
 * @mangled OpPlayVolSE__Fiiif
 * @address 0x1DB1210
 * @size 0xD4
 * @unknownret
 */
void OpPlayVolSE(int group, int no, int voice, float volume);

/**
 *          Returns the current volume of an opening sequence channel.
 *
 * @mangled OpGetVolSQ__Fi
 * @address 0x1DB12F0
 * @size 0x6C
 * @unknownret
 */
int OpGetVolSQ(int no);

/**
 *          Cancels active opening sound fades.
 *
 * @mangled FadeCansel__Fv
 * @address 0x1DB1360
 * @size 0x98
 * @unknownret
 */
void FadeCansel();

/**
 *          Parses an opening scene definition file.
 *
 * @mangled OPAnalyz__FPc
 * @address 0x1DB1400
 * @size 0x152C
 * @unknownret
 */
void OPAnalyz(char *name);

/**
 *          Constructs scene objects from the parsed definition tree.
 *
 * @mangled OPMdsLoad__Fv
 * @address 0x1DB2930
 * @size 0x1EFC
 * @unknownret
 */
void OPMdsLoad();

/**
 * Characters of the opening movie.
 */
extern CCharacter Chara__3[];

/**
 * Texture slot of each opening character.
 */
extern char CharaTex__2[23];

/**
 * Camera-path characters of the opening.
 */
extern CCharacter Cam__2[];

/**
 * Scene the opening is playing.
 */
extern int SceneNp__2;

/**
 * Arenas the opening characters are read into.
 */
extern CDataAlloc2<1> CharaDataBuffer__2[];

/**
 * Arena the opening water is read into.
 */
extern CDataAlloc2<1> WaterBuffer__2;

/**
 * Arena the opening map is read into.
 */
extern CDataAlloc2<1> MapDataBuffer;

/**
 * Camera the opening is drawn through.
 */
extern CCameraFollow OP_MainCamera;

/**
 * Sequencer port the opening BGM plays on.
 */
extern int OpBgmSqPort;

/**
 * Motion the opening characters are keyed by.
 */
extern MOTION_INFO Op_MotionInfo;

/**
 * Whether the opening is paused.
 */
extern int Pause;

/**
 * Colour of the opening fog.
 */
extern u_char op_fogColor[3];

/**
 * Range and thickness of the opening fog.
 */
extern sceVu0FVECTOR op_fogRate;
