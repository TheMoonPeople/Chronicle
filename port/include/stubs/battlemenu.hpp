#pragma once

#include "common.h"

#include "rect.hpp"

#define CRect_i_(...) Ps2Lvalue(CRect_i_(__VA_ARGS__))

// Expose the static initialization functions as global Btl_ prefixed functions
#define InitMenuChara Btl_InitMenuChara
#define InitWeaponSelect Btl_InitWeaponSelect
#define InitItemMode Btl_InitItemMode
#define BattleManualInit Btl_BattleManualInit

void Btl_InitMenuChara(u_long128 *buffer);
void Btl_InitWeaponSelect(int mode, int chara);
void Btl_InitItemMode(int mode, int chara);
