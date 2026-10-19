#pragma once

// The port's own events in the towns' event scripts (runscript.cpp).

// "Toan gets an item", as the towns' events show it: the camera, the glow, Toan holding the item up,
// the jingle and the system message naming it. A number past every program retail's scripts have;
// EdRunEvent(kEventItemGet) runs the town script's own item-get routine for the item
// EventScriptSetItemGet gave.
inline constexpr int kEventItemGet = 32000;

// Whether the town's loaded event script has the item-get routine to run.
bool EventScriptHasItemGet();

// The item the next kEventItemGet shows.
void EventScriptSetItemGet(int item);
