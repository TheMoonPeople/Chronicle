#pragma once

#include "common.h"

/**
 * @file
 * Declares the objects that every menu screen shares.
 */

class CCamera;
class CRect_i_;
class ClsMes;

/**
 * The camera the menus draw their models through.
 */
extern CCamera MenuCamera;

/**
 * The message window the menus share.
 */
extern ClsMes CommonMenuMes1;

/**
 * The second message window the menus share.
 */
extern ClsMes CommonMenuMes2;

/**
 * The third message window the menus share.
 */
extern ClsMes CommonMenuMes3;

/**
 * The message window that names the selected georama element.
 */
extern ClsMes AtoraNameMes;

/**
 * The whole screen, which the menus draw full-screen pictures into.
 */
extern CRect_i_ MenuDispRc;
