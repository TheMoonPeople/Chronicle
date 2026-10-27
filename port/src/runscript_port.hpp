#pragma once

/**
 * Event number of the port's "Toan gets an item" event in the towns' event scripts: the camera, the
 * glow, Toan holding the item up, the jingle and the system message naming it. It lies past every
 * program retail's scripts have; EdRunEvent(kEventItemGet) runs the town script's own item-get
 * routine for the item EventScriptSetItemGet gave.
 */
inline constexpr int kEventItemGet = 32000;

/**
 * Returns whether the town's loaded event script has the item-get
 * routine that kEventItemGet runs.
 */
bool EventScriptHasItemGet();

/**
 * Sets the item the next kEventItemGet event shows
 * Toan receiving.
 */
void EventScriptSetItemGet(int item);
