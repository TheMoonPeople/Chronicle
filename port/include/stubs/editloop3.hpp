#pragma once

#define menu_mode PortEventMenuMode
#define _WORLD_MAP PortWorldMapCommand
#define EdMenuMode PortEdEventMenuMode

class CNPCharacter;
struct VILLAGER_INFO;
class CEditGround;
class CRunScript;

/**
 * Event interpreter that the port's EdRunEvent starts, under a fixed
 * symbol name the platform layer declares.
 */
extern CRunScript EdEventScript asm("EditLoop3_EdEventScript");

/**
 * Script variable the bait picker writes its choice to, read by the
 * port's bait fix under a fixed symbol name.
 */
extern char *p_use_item asm("EditLoop3_p_use_item");

/**
 * Picks a villager's next walk target; the port's villager movement
 * calls this in place of the unit-local EdSetVillagerNextPos.
 */
void PortEdSetVillagerNextPos(CNPCharacter *villager, VILLAGER_INFO *info, CEditGround *ground);
