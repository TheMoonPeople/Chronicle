#pragma once

#include "common.h"

#include "rect.hpp"

#define CRect_i_(...) Ps2Lvalue(CRect_i_(__VA_ARGS__))

// The page initializers get Btl_ names, under which the port's battle menu calls them.
#define InitMenuChara Btl_InitMenuChara
#define InitWeaponSelect Btl_InitWeaponSelect
#define InitItemMode Btl_InitItemMode
#define BattleManualInit Btl_BattleManualInit

/**
 * Opens the battle menu's character page and lays out its turntable.
 */
void Btl_InitMenuChara(u_long128 *buffer);

/**
 * Opens the battle menu's weapon page on a party member's weapons.
 */
void Btl_InitWeaponSelect(int mode, int chara);

/**
 * Opens the battle menu's item page on a party member's pack.
 */
void Btl_InitItemMode(int mode, int chara);
