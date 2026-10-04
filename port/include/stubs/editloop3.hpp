#pragma once

// The port's EdMoveVillager and EdMoveVillagerSubMap (port/src/editloop3.cpp) call this unit's
// static EdSetVillagerNextPos, which gets a global forwarder; that also keeps clang from dropping
// the static as unused.

class CNPCharacter;
struct VILLAGER_INFO;
class CEditGround;
class CRunScript;

// The port's EdRunEvent (port/src/runscript.cpp) starts this unit's static event interpreter,
// declared extern here first so it has a global name the port can reach.
extern CRunScript EdEventScript asm("EditLoop3_EdEventScript");

static void EdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground);

void PortEdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground) {
    EdSetVillagerNextPos(villager, info, ground);
}
