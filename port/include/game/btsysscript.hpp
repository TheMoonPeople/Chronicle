#pragma once

#include "common.h"

#include <array>

#include "dataalloc_fwd.hpp"

namespace dc::game {

/**
 * @file
 * Declares the opcodes a dungeon floor's system script can call.
 *
 * The opcodes read their arguments off the script's argument stack and report
 * whether the script may carry on. They reach the floor through `NowDngMap`
 * and the party through `UserStatus`, and address the map's frames and
 * characters through the thirty-two object handles `GetObjHDL` hands out.
 */

struct RS_STACKDATA;
class CFrame;
class CCharacter;

/**
 * References one frame or character exposed by the battle script system.
 */
struct BT_OBJ_HANDLE {
    CFrame     *frame;     /**< Frame referenced when type is zero. */
    CCharacter *character; /**< Character referenced when type is one. */
    s32         type;      /**< Selects whether the handle contains a frame or character. */
};

/**
 * Clears the battle event state and points the event at the player's party.
 */
void BtSystemScriptEventInfoInit();

/**
 * Table of object handles the battle script system can bind.
 */
extern std::array<BT_OBJ_HANDLE, 32> BtObjHdl;

/**
 * Gives one of the thirty-two object handles, or nothing where the index is out of range.
 */
BT_OBJ_HANDLE *GetObjHDL(int index);

/**
 * Reads one floor's system script off the disc and installs its opcode table.
 */
void BtSystemScriptLoad(int floor);

/**
 * Puts the battle event camera in place of the player's and clears the object handles.
 */
void BtSystemScriptInit();

/**
 * Hands control back to the player's camera and clears what the event set.
 */
void BtSystemScriptAfter();

/**
 * Starts one system event script running out of an arena, giving back what
 * `EdEventInit` reports.
 */
int BtSystemScriptRun(int event, CDataAlloc2<1> *arena);

/**
 * Selects the floor used by the next battle-map jump.
 */
void BtSetMapJumpFloor(int floor);

/**
 * Floor requested for the next battle-map jump.
 */
extern int BtMapJumpFloor;

/**
 * Name of the map file the next battle-map jump loads.
 */
extern std::array<char, 32> BtLoadMapFileName;

/**
 * Layout of the map the next battle-map jump loads.
 */
extern s32 BtLoadMapType;

/**
 * Gives the floor the party stands on.
 */
int _GET_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count);

/**
 * Moves the party to a floor.
 */
int _SET_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the floor the party came from.
 */
int _GET_OLD_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the mode the battle loop is running in.
 */
int _GET_ACTION_MODE(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the window that asks the player to choose one of a list of items.
 */
int _ITEM_USE_WINDOW(RS_STACKDATA *stack, int argument_count);

/**
 * Reads one of the dungeon's event flags.
 */
int _CHECK_EVENT_FLG(RS_STACKDATA *stack, int argument_count);

/**
 * Writes one of the dungeon's fifty event flags.
 */
int _SET_EVENT_FLG(RS_STACKDATA *stack, int argument_count);

/**
 * Binds an object handle to a named frame of the map, or of one of its parts.
 */
int _GET_OBJHDL(RS_STACKDATA *stack, int argument_count);

/**
 * Puts the object a handle names at a world position.
 */
int _SET_OBJHDL_POS(RS_STACKDATA *stack, int argument_count);

/**
 * Turns the object a handle names to three wrapped angles.
 */
int _SET_OBJHDL_ROT(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether the object a handle names is drawn.
 */
int _SET_OBJHDL_DRAW_FLAG(RS_STACKDATA *stack, int argument_count);

/**
 * Reads the world position of the object a handle names.
 */
int _GET_OBJHDL_POS(RS_STACKDATA *stack, int argument_count);

/**
 * Reads the heading of the object a handle names.
 */
int _GET_OBJHDL_ROT(RS_STACKDATA *stack, int argument_count);

/**
 * Requests a transition to the back dungeon.
 */
int _SET_URA_DUNGEON(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the world position the event was started at.
 */
int _GET_EVENT_POS(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the heading the event was started with.
 */
int _GET_EVENT_ROT(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the dungeon entrance window and retains its result slot.
 */
int _OPEN_ENTRANCE_WINDOW(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the dungeon escape window and retains its result slot.
 */
int _OPEN_ESCAPE_WINDOW(RS_STACKDATA *stack, int argument_count);

/**
 * Requests entry into the selected dungeon floor.
 */
int _GO_DUNGEON(RS_STACKDATA *stack, int argument_count);

/**
 * Names the map file and type the next floor loads from.
 */
int _SET_DUNGEON_MAP(RS_STACKDATA *stack, int argument_count);

/**
 * Loads the map the previous opcode named.
 */
int _LOAD_DUNGEON_MAP2(RS_STACKDATA *stack, int argument_count);

/**
 * Reloads and arranges the monsters on the current floor.
 */
int _LOAD_MONSTOR(RS_STACKDATA *stack, int argument_count);

/**
 * Lays out the floor, randomly or freely according to the map's own setting.
 */
int _SET_RANDOM_MAP(RS_STACKDATA *stack, int argument_count);

/**
 * Turns one of the floor's event switches on or off.
 */
int _SET_EVENT_SW(RS_STACKDATA *stack, int argument_count);

/**
 * Puts a monster on the floor, optionally clearing the ones already there first.
 */
int _SET_MONSTOR_ID(RS_STACKDATA *stack, int argument_count);

/**
 * Reports how many of one kind of Atla the party is carrying.
 */
int _CHK_ATRA_HAVE(RS_STACKDATA *stack, int argument_count);

/**
 * Puts an Atla sphere on the floor at a world position.
 */
int _SET_ATRA(RS_STACKDATA *stack, int argument_count);

/**
 * Puts a treasure chest on the floor at a world position.
 */
int _SET_IBOX(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the character the player is controlling.
 */
int _GET_NOW_USER_ID(RS_STACKDATA *stack, int argument_count);

/**
 * Queues another system script to run when this one ends.
 */
int _RUN_SCRIPT_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Queues the script that clears the floor's monsters.
 */
int _CLEAN_MONSTOR_SCRIPT_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Binds an object handle to one of the floor's four non-player characters.
 */
int _GET_NPC_OBJHDL(RS_STACKDATA *stack, int argument_count);

/**
 * Starts a motion on one of the floor's non-player characters.
 */
int _SET_MOTION_OBJHDL(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether one of the floor's non-player characters takes part.
 */
int _SET_NPC_ON_OFF(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the key the floor's gate wants.
 */
int _GET_GATEKEY_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether the player's weapon is drawn.
 */
int _USER_WEAPON_DRAW(RS_STACKDATA *stack, int argument_count);

/**
 * Puts one of the six party members in the player's hands, remembering who was there.
 */
int _SET_MAIN_CHR2(RS_STACKDATA *stack, int argument_count);

/**
 * Selects again, with mode 0, the character _SET_MAIN_CHR2 last asked for, and clears the record.
 */
int _RESET_MAIN_CHR(RS_STACKDATA *stack, int argument_count);

/**
 * Fences the party into a zone and loads the sound that warns them of it.
 */
int _SET_LIMMIT_ZONE(RS_STACKDATA *stack, int argument_count);

/**
 * Marks the party dead.
 */
int _SET_DEAD_FLAG(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether the floor draws at all.
 */
int _ALL_DRAW_FLAG(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the current floor's title display.
 */
int _SET_FLOOR_TITLE(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the element the floor's sealed door wants.
 */
int _GET_RUBY_ELEMENT(RS_STACKDATA *stack, int argument_count);

/**
 * Reads an element off the stack and discards it.
 */
int _SET_RUBY_ELEMENT(RS_STACKDATA *stack, int argument_count);

/**
 * Suppresses the plate that names the floor.
 */
int _SET_FLOOR_TITLE_OFF(RS_STACKDATA *stack, int argument_count);

/**
 * Restores the zone the party was fenced into and its warning sound.
 */
int _SET_RES_LIMMIT_ZONE(RS_STACKDATA *stack, int argument_count);

/**
 * Clears the recorded progress through the Demon Shaft.
 */
int _CLEAR_DEAMON_SHAFT(RS_STACKDATA *stack, int argument_count);

/**
 * Starts the swarm of bees on one of the party's characters.
 */
int _INIT_BEE(RS_STACKDATA *stack, int argument_count);

/**
 * Ends the swarm of bees.
 */
int _END_BEE(RS_STACKDATA *stack, int argument_count);

/**
 * Reports whether the Eastern king's puzzle has been completed.
 */
int _EASTKING_COMPLETE(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the trap on the chest the party is opening.
 */
int _GET_ITEM_TRAPID(RS_STACKDATA *stack, int argument_count);

/**
 * Clears the trap on the chest the party is opening.
 */
int _RESET_ITEM_TRAP(RS_STACKDATA *stack, int argument_count);

/**
 * Sets off an explosion above the party and pushes it away from them.
 */
int _BOM_SET(RS_STACKDATA *stack, int argument_count);

/**
 * Inflicts a status ailment on the party and raises the collision that shows it.
 */
int _SET_STATUS_ERR(RS_STACKDATA *stack, int argument_count);

/**
 * Reports the state of the Mardan Garayan fishing quest.
 */
int _CHECK_MARDAN(RS_STACKDATA *stack, int argument_count);

/**
 * Keeps the controlled character across the end of the event.
 */
int _NO_RESET_CHARA_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Reports whether a party member is waiting to be rescued.
 */
int _CHECK_CHR_HELP(RS_STACKDATA *stack, int argument_count);

/**
 * Holds the item event of a number back in both event managers.
 */
int _HOLD_ITEM_EVENT(RS_STACKDATA *stack, int argument_count);

/**
 * Stops the current battle music.
 */
int _STOP_BATTLE_BGM(RS_STACKDATA *stack, int argument_count);

/**
 * Keeps status ailments across the end of the event.
 */
int _NO_STATUS_RECOVER(RS_STACKDATA *stack, int argument_count);

/**
 * Records the party's progress through a quest dungeon.
 */
int _SET_QUEST_DUNGEON(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the dungeon the party is in, counted from one.
 */
int _GET_MAP_CODE(RS_STACKDATA *stack, int argument_count);

/**
 * Loads the icons used by the active item slots.
 */
int _SET_ACTIVE_ITEM_ICON(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the party member a weapon item belongs to, or -1 for an item that is not a weapon.
 */
int _GET_ITEM_UNIT_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Turns the chest the party is opening.
 */
int _SET_IBOX_ANGLE(RS_STACKDATA *stack, int argument_count);

/**
 * Closes the chest the party is opening and drops it out of sight.
 */
int _SET_IBOX_FINISH(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the two items the clown trades on this floor.
 */
int _GET_PIERO_ITEM(RS_STACKDATA *stack, int argument_count);

/**
 * Fills the opcode table from the extension list, refusing a list that names one twice.
 */
void AddSystemEventScript();

} // namespace dc::game
