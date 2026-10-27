#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

namespace dc::game {

class CCamera;
class CFrame;
class CFrameVu1;
class CSaveData;

/**
 * Loads a model from a pack file, exiting if it is missing.
 */
CFrame *LoadMDSFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer);

/**
 * Loads a collision model from a pack file, exiting if it is missing.
 */
CFrame *LoadCollisionFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer);

/**
 * Loads a model from memory.
 */
CFrameVu1 *LoadMDSFile(unsigned int *data, CDataAlloc2<1> *alloc, int attr, char **double_names, char **retain_names);

/**
 * Puts the camera on the two named frames of a model's path.
 */
void setCameraPassData(CFrameVu1 *frame, CCamera *camera, char *position_name, char *reference_name);

/**
 * Gives the world position of one frame of a model.
 */
void getFramePos(CFrameVu1 *frame, char *name, float *position);

/**
 * Resolves the model and texture paths for a battle item.
 */
void BtGetItemNamePath(char *model_path, char *texture_path, int item_no);

/**
 * Builds the character and configuration file names of one of a character's
 * weapons into the two given buffers.
 */
void BtGetWeaponNamePath2(char *chr_name, char *cfg_name, int chara, int weapon_index);

/**
 * Builds the character and configuration file names for an absolute weapon item identifier.
 */
void BtGetWeaponNamePath3(char *chr_name, char *cfg_name, int weapon_no);

/**
 * Records in the save file that an Atla has been collected.
 */
void getAtraToSaveData(int atra, int atra_no, CSaveData *save, int dungeon, int floor);

/**
 * Rolls how much of an attachment one item yields; 0 for items that are not attachments.
 */
int createAttachVolume(int item_no, int dungeon);

/**
 * Resets the battle-music crossfade state.
 */
void BtBattleMusic_Init();

/**
 * Stops the current battle music.
 */
void BtBattleMusic_Stop();

/**
 * Crossfades between the field and battle music as the party nears a monster.
 */
void BtBattleMusic_Excg(float distance, float *field_volume, float *battle_volume);

} // namespace dc::game
