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

// _WORLD_MAP keeps the script slot the chosen map goes to in p_jump_map_no, an int read as stack->i,
// for EdEventMode to write through as the map closes: on the host that is the low 32 bits of the
// slot's address. EdEventMode's EdMenuMode is the port's (port/src/editloop3.cpp), which writes the
// choice through the whole slot first and clears p_jump_map_no, so retail's write is never reached.
extern int menu_mode asm("EditLoop3_menu_mode");
extern int p_jump_map_no asm("EditLoop3_p_jump_map_no");
#define EdMenuMode PortEdEventMenuMode

static void EdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground);

void PortEdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground) {
    EdSetVillagerNextPos(villager, info, ground);
}
