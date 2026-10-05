#pragma once

// editloop.cpp defines MainDraw() and MoveChara() too. The PS2 link renames this unit's copies
// (object_fixups.json); without that, the port's link would have two of each.
// port/src/dun/gameloop.cpp replaces DunMainDraw.
#define MainDraw DunMainDraw
#define MoveChara DunMoveChara
