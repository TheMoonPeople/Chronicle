#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

class CCamera;
class CFrame;
class CFrameVu1;
class CSaveData;

/**
 * Loads a model from a pack file.
 *
 * @mangled LoadMDSFilePack__FPUiPcP14CDataAlloc2_1_
 * @address 0x1B6D70
 * @size 0x8C
 */
CFrame *LoadMDSFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer);

/**
 * Loads a collision model from a pack file.
 *
 * @mangled LoadCollisionFilePack__FPUiPcP14CDataAlloc2_1_
 * @address 0x1B6E00
 * @size 0x80
 */
CFrame *LoadCollisionFilePack(unsigned int *pack, char *name, CDataAlloc2<1> *buffer);

/**
 * Loads a model from memory.
 *
 * @mangled LoadMDSFile__FPUiP14CDataAlloc2_1_iPPcPPc
 * @address 0x1262B0
 * @size 0x4BC
 */
CFrameVu1 *LoadMDSFile(unsigned int *data, CDataAlloc2<1> *alloc, int attr, char **double_names, char **retain_names);

/**
 * Puts the camera on the two named frames of a model's path.
 *
 * @mangled setCameraPassData__FP9CFrameVu1P7CCameraPcPc
 * @address 0x1B6E80
 * @size 0xA4
 */
void setCameraPassData(CFrameVu1 *frame, CCamera *camera, char *position_name, char *reference_name);

/**
 * Gives the world position of one frame of a model.
 *
 * @mangled getFramePos__FP9CFrameVu1PcPf
 * @address 0x1B6F30
 * @size 0x50
 * @unknownret
 */
void getFramePos(CFrameVu1 *frame, char *name, float *position);

/**
 * Builds the resource name for an absolute weapon item identifier.
 *
 * @mangled makeWeaponName__FPci
 * @address 0x1B6F80
 * @size 0x198
 */
void makeWeaponName(char *name, int weapon_no);

/**
 * Resolves the model and texture paths for a battle item.
 *
 * @mangled BtGetItemNamePath__FPcPci
 * @address 0x1B7120
 * @size 0x124
 */
void BtGetItemNamePath(char *model_path, char *texture_path, int item_no);

/**
 * Builds the character and configuration file names of one of a character's
 * weapons into the two given buffers.
 *
 * @mangled BtGetWeaponNamePath2__FPcPcii
 * @address 0x1B7250
 * @size 0x158
 */
void BtGetWeaponNamePath2(char *chr_name, char *cfg_name, int chara, int weapon_index);

/**
 * Builds the character and configuration file names for an absolute weapon item identifier.
 *
 * @mangled BtGetWeaponNamePath3__FPcPci
 * @address 0x1B73B0
 * @size 0xB4
 */
void BtGetWeaponNamePath3(char *chr_name, char *cfg_name, int weapon_no);

/**
 * Records in the save file that an Atla has been collected.
 *
 * @mangled getAtraToSaveData__FiiP9CSaveDataii
 * @address 0x1B7470
 * @size 0xBC
 */
void getAtraToSaveData(int atra, int atra_no, CSaveData *save, int dungeon, int floor);

/**
 * Stops the current battle music.
 *
 * @mangled BtBattleMusic_Stop__Fv
 * @address 0x1B7640
 * @size 0x48
 */
void BtBattleMusic_Stop();

/**
 * The model file extension.
 */
extern char MdsExtension[];
