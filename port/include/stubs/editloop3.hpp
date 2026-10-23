#pragma once

#define menu_mode PortEventMenuMode
#define _WORLD_MAP PortWorldMapCommand
#define EdMenuMode PortEdEventMenuMode

// The port's EdMoveVillager and EdMoveVillagerSubMap (port/src/editloop3.cpp) call this unit's
// static EdSetVillagerNextPos, which gets a global forwarder; that also keeps clang from dropping
// the static as unused.

class CNPCharacter;
struct VILLAGER_INFO;
class CEditGround;
class CRunScript;

// The port's EdRunEvent (port/src/editloop3.cpp) starts this unit's static event interpreter,
// declared extern here first so it has a global name the port can reach.
extern CRunScript EdEventScript asm("EditLoop3_EdEventScript");

// The port's bait fix (ListBaitForScript, port/src/fish_port.hpp) reads the script variable the bait picker
// writes its choice to, through this global name for the unit's static p_use_item.
extern char *p_use_item asm("EditLoop3_p_use_item");

static void EdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground);

void PortEdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground) {
    EdSetVillagerNextPos(villager, info, ground);
}
