#pragma once

#include "common.h"

#include "dataalloc_fwd.hpp"

/**
 * @file
 * Declares the opcodes a dungeon floor's system script can call.
 *
 * The opcodes read their arguments off the script's argument stack and report
 * whether the script may carry on. They reach the floor through `NowDngMap`
 * and the party through `UserStatus`, and address the map's frames and
 * characters through the thirty-two object handles `GetObjHDL` hands out.
 */

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
struct RS_STACKDATA;
class CFrame;
class CCharacter;

/** References one frame or character exposed by the battle script system. */
struct BT_OBJ_HANDLE {
    CFrame     *frame;     /**< Frame referenced when type is zero. */
    CCharacter *character; /**< Character referenced when type is one. */
    s32         type;      /**< Selects whether the handle contains a frame or character. */
};

/**
 * Clears the battle event state and points the event at the player's party.
 *
 * @mangled BtSystemScriptEventInfoInit__Fv
 * @address 0x1BB060
 * @size 0x198
 */
void BtSystemScriptEventInfoInit();

/** Table of object handles the battle script system can bind. */
extern BT_OBJ_HANDLE BtObjHdl[32];

/**
 * Gives one of the thirty-two object handles, or nothing where the index is out of range.
 *
 * @mangled GetObjHDL__Fi
 * @address 0x1BB200
 * @size 0x60
 */
BT_OBJ_HANDLE *GetObjHDL(int index);

/**
 * Reads one floor's system script off the disc and installs its opcode table.
 *
 * @mangled BtSystemScriptLoad__Fi
 * @address 0x1BB260
 * @size 0xAC
 */
void BtSystemScriptLoad(int floor);

/**
 * Puts the battle event camera in place of the player's and clears the object handles.
 *
 * @mangled BtSystemScriptInit__Fv
 * @address 0x1BB310
 * @size 0x2C4
 */
void BtSystemScriptInit();

/**
 * Hands control back to the player's camera and clears what the event set.
 *
 * @mangled BtSystemScriptAfter__Fv
 * @address 0x1BB5E0
 * @size 0x304
 */
void BtSystemScriptAfter();

/**
 * Starts one system event script running out of an arena, giving back what
 * `EdEventInit` reports.
 *
 * @mangled BtSystemScriptRun__FiP14CDataAlloc2_1_
 * @address 0x1BB8F0
 * @size 0x24
 */
int BtSystemScriptRun(int event, CDataAlloc2<1> *arena);

/**
 * Selects the floor used by the next battle-map jump.
 *
 * @mangled BtSetMapJumpFloor__Fi
 * @address 0x1BB920
 * @size 0xC
 */
void BtSetMapJumpFloor(int floor);

/** Floor requested for the next battle-map jump. */
extern int BtMapJumpFloor;

/** Name of the map file the next battle-map jump loads. */
extern char BtLoadMapFileName[32];

/** Layout of the map the next battle-map jump loads. */
extern s32 BtLoadMapType;

/**
 * Gives the floor the party stands on.
 *
 * @mangled _GET_FLOOR_LEVEL__FP12RS_STACKDATAi
 * @address 0x1BBA00
 * @size 0x2C
 */
int _GET_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count);

/**
 * Moves the party to a floor.
 *
 * @mangled _SET_FLOOR_LEVEL__FP12RS_STACKDATAi
 * @address 0x1BBA30
 * @size 0x34
 */
int _SET_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the floor the party came from.
 *
 * @mangled _GET_OLD_FLOOR_LEVEL__FP12RS_STACKDATAi
 * @address 0x1BBA70
 * @size 0x2C
 */
int _GET_OLD_FLOOR_LEVEL(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the mode the battle loop is running in.
 *
 * @mangled _GET_ACTION_MODE__FP12RS_STACKDATAi
 * @address 0x1BBAA0
 * @size 0x2C
 */
int _GET_ACTION_MODE(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the window that asks the player to choose one of a list of items.
 *
 * @mangled _ITEM_USE_WINDOW__FP12RS_STACKDATAi
 * @address 0x1BBAD0
 * @size 0xEC
 */
int _ITEM_USE_WINDOW(RS_STACKDATA *stack, int argument_count);

/**
 * Reads one of the dungeon's event flags.
 *
 * @mangled _CHECK_EVENT_FLG__FP12RS_STACKDATAi
 * @address 0x1BBBC0
 * @size 0x50
 */
int _CHECK_EVENT_FLG(RS_STACKDATA *stack, int argument_count);

/**
 * Writes one of the dungeon's fifty event flags.
 *
 * @mangled _SET_EVENT_FLG__FP12RS_STACKDATAi
 * @address 0x1BBC10
 * @size 0x70
 */
int _SET_EVENT_FLG(RS_STACKDATA *stack, int argument_count);

/**
 * Binds an object handle to a named frame of the map, or of one of its parts.
 *
 * @mangled _GET_OBJHDL__FP12RS_STACKDATAi
 * @address 0x1BBC80
 * @size 0x130
 */
int _GET_OBJHDL(RS_STACKDATA *stack, int argument_count);

/**
 * Puts the object a handle names at a world position.
 *
 * @mangled _SET_OBJHDL_POS__FP12RS_STACKDATAi
 * @address 0x1BBDB0
 * @size 0xF8
 */
int _SET_OBJHDL_POS(RS_STACKDATA *stack, int argument_count);

/**
 * Turns the object a handle names to three wrapped angles.
 *
 * @mangled _SET_OBJHDL_ROT__FP12RS_STACKDATAi
 * @address 0x1BBEB0
 * @size 0x180
 */
int _SET_OBJHDL_ROT(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether the object a handle names is drawn.
 *
 * @mangled _SET_OBJHDL_DRAW_FLAG__FP12RS_STACKDATAi
 * @address 0x1BC030
 * @size 0xC4
 */
int _SET_OBJHDL_DRAW_FLAG(RS_STACKDATA *stack, int argument_count);

/**
 * Reads the world position of the object a handle names.
 *
 * @mangled _GET_OBJHDL_POS__FP12RS_STACKDATAi
 * @address 0x1BC100
 * @size 0xEC
 */
int _GET_OBJHDL_POS(RS_STACKDATA *stack, int argument_count);

/**
 * Reads the heading of the object a handle names.
 *
 * @mangled _GET_OBJHDL_ROT__FP12RS_STACKDATAi
 * @address 0x1BC1F0
 * @size 0x10C
 */
int _GET_OBJHDL_ROT(RS_STACKDATA *stack, int argument_count);

/**
 * Requests a transition to the back dungeon.
 *
 * @mangled _SET_URA_DUNGEON__FP12RS_STACKDATAi
 * @address 0x1BC300
 * @size 0x18
 */
int _SET_URA_DUNGEON(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the world position the event was started at.
 *
 * @mangled _GET_EVENT_POS__FP12RS_STACKDATAi
 * @address 0x1BC320
 * @size 0x5C
 */
int _GET_EVENT_POS(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the heading the event was started with.
 *
 * @mangled _GET_EVENT_ROT__FP12RS_STACKDATAi
 * @address 0x1BC380
 * @size 0x5C
 */
int _GET_EVENT_ROT(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the dungeon entrance window and retains its result slot.
 *
 * @mangled _OPEN_ENTRANCE_WINDOW__FP12RS_STACKDATAi
 * @address 0x1BC3E0
 * @size 0x3C
 */
int _OPEN_ENTRANCE_WINDOW(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the dungeon escape window and retains its result slot.
 *
 * @mangled _OPEN_ESCAPE_WINDOW__FP12RS_STACKDATAi
 * @address 0x1BC420
 * @size 0x58
 */
int _OPEN_ESCAPE_WINDOW(RS_STACKDATA *stack, int argument_count);

/**
 * Requests entry into the selected dungeon floor.
 *
 * @mangled _GO_DUNGEON__FP12RS_STACKDATAi
 * @address 0x1BC480
 * @size 0x18
 */
int _GO_DUNGEON(RS_STACKDATA *stack, int argument_count);

/**
 * Names the map file and type the next floor loads from.
 *
 * @mangled _SET_DUNGEON_MAP__FP12RS_STACKDATAi
 * @address 0x1BC4A0
 * @size 0x64
 */
int _SET_DUNGEON_MAP(RS_STACKDATA *stack, int argument_count);

/**
 * Loads the map the previous opcode named.
 *
 * @mangled _LOAD_DUNGEON_MAP2__FP12RS_STACKDATAi
 * @address 0x1BC510
 * @size 0x40
 */
int _LOAD_DUNGEON_MAP2(RS_STACKDATA *stack, int argument_count);

/**
 * Reloads and arranges the monsters on the current floor.
 *
 * @mangled _LOAD_MONSTOR__FP12RS_STACKDATAi
 * @address 0x1BC550
 * @size 0x30
 */
int _LOAD_MONSTOR(RS_STACKDATA *stack, int argument_count);

/**
 * Lays out the floor, randomly or freely according to the map's own setting.
 *
 * @mangled _SET_RANDOM_MAP__FP12RS_STACKDATAi
 * @address 0x1BC580
 * @size 0x50
 */
int _SET_RANDOM_MAP(RS_STACKDATA *stack, int argument_count);

/**
 * Turns one of the floor's event switches on or off.
 *
 * @mangled _SET_EVENT_SW__FP12RS_STACKDATAi
 * @address 0x1BC5D0
 * @size 0x5C
 */
int _SET_EVENT_SW(RS_STACKDATA *stack, int argument_count);

/**
 * Puts a monster on the floor, optionally clearing the ones already there first.
 *
 * @mangled _SET_MONSTOR_ID__FP12RS_STACKDATAi
 * @address 0x1BC630
 * @size 0x1BC
 */
int _SET_MONSTOR_ID(RS_STACKDATA *stack, int argument_count);

/**
 * Reports how many of one kind of Atla the party is carrying.
 *
 * @mangled _CHK_ATRA_HAVE__FP12RS_STACKDATAi
 * @address 0x1BC7F0
 * @size 0x8C
 */
int _CHK_ATRA_HAVE(RS_STACKDATA *stack, int argument_count);

/**
 * Puts an Atla sphere on the floor at a world position.
 *
 * @mangled _SET_ATRA__FP12RS_STACKDATAi
 * @address 0x1BC880
 * @size 0x7C
 */
int _SET_ATRA(RS_STACKDATA *stack, int argument_count);

/**
 * Puts a treasure chest on the floor at a world position.
 *
 * @mangled _SET_IBOX__FP12RS_STACKDATAi
 * @address 0x1BC900
 * @size 0x84
 */
int _SET_IBOX(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the character the player is controlling.
 *
 * @mangled _GET_NOW_USER_ID__FP12RS_STACKDATAi
 * @address 0x1BC990
 * @size 0x5C
 */
int _GET_NOW_USER_ID(RS_STACKDATA *stack, int argument_count);

/**
 * Queues another system script to run when this one ends.
 *
 * @mangled _RUN_SCRIPT_NO__FP12RS_STACKDATAi
 * @address 0x1BC9F0
 * @size 0xC0
 */
int _RUN_SCRIPT_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Queues the script that clears the floor's monsters.
 *
 * @mangled _CLEAN_MONSTOR_SCRIPT_NO__FP12RS_STACKDATAi
 * @address 0x1BCAB0
 * @size 0x78
 */
int _CLEAN_MONSTOR_SCRIPT_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Binds an object handle to one of the floor's four non-player characters.
 *
 * @mangled _GET_NPC_OBJHDL__FP12RS_STACKDATAi
 * @address 0x1BCB30
 * @size 0xCC
 */
int _GET_NPC_OBJHDL(RS_STACKDATA *stack, int argument_count);

/**
 * Starts a motion on one of the floor's non-player characters.
 *
 * @mangled _SET_MOTION_OBJHDL__FP12RS_STACKDATAi
 * @address 0x1BCC00
 * @size 0x138
 */
int _SET_MOTION_OBJHDL(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether one of the floor's non-player characters takes part.
 *
 * @mangled _SET_NPC_ON_OFF__FP12RS_STACKDATAi
 * @address 0x1BCD40
 * @size 0x84
 */
int _SET_NPC_ON_OFF(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the key the floor's gate wants.
 *
 * @mangled _GET_GATEKEY_NO__FP12RS_STACKDATAi
 * @address 0x1BCDD0
 * @size 0x2C
 */
int _GET_GATEKEY_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether the player's weapon is drawn.
 *
 * @mangled _USER_WEAPON_DRAW__FP12RS_STACKDATAi
 * @address 0x1BCE00
 * @size 0x40
 */
int _USER_WEAPON_DRAW(RS_STACKDATA *stack, int argument_count);

/**
 * Puts one of the six party members in the player's hands, remembering who was there.
 *
 * @mangled _SET_MAIN_CHR2__FP12RS_STACKDATAi
 * @address 0x1BCE40
 * @size 0x9C
 */
int _SET_MAIN_CHR2(RS_STACKDATA *stack, int argument_count);

/**
 * Selects again, with mode 0, the character _SET_MAIN_CHR2 last asked for, and clears the record.
 *
 * @mangled _RESET_MAIN_CHR__FP12RS_STACKDATAi
 * @address 0x1BCEE0
 * @size 0x50
 */
int _RESET_MAIN_CHR(RS_STACKDATA *stack, int argument_count);

/**
 * Fences the party into a zone and loads the sound that warns them of it.
 *
 * @mangled _SET_LIMMIT_ZONE__FP12RS_STACKDATAi
 * @address 0x1BCF30
 * @size 0x40
 */
int _SET_LIMMIT_ZONE(RS_STACKDATA *stack, int argument_count);

/**
 * Marks the party dead.
 *
 * @mangled _SET_DEAD_FLAG__FP12RS_STACKDATAi
 * @address 0x1BCF70
 * @size 0x28
 */
int _SET_DEAD_FLAG(RS_STACKDATA *stack, int argument_count);

/**
 * Sets whether the floor draws at all.
 *
 * @mangled _ALL_DRAW_FLAG__FP12RS_STACKDATAi
 * @address 0x1BCFA0
 * @size 0x34
 */
int _ALL_DRAW_FLAG(RS_STACKDATA *stack, int argument_count);

/**
 * Opens the current floor's title display.
 *
 * @mangled _SET_FLOOR_TITLE__FP12RS_STACKDATAi
 * @address 0x1BCFE0
 * @size 0x24
 */
int _SET_FLOOR_TITLE(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the element the floor's sealed door wants.
 *
 * @mangled _GET_RUBY_ELEMENT__FP12RS_STACKDATAi
 * @address 0x1BD010
 * @size 0x28
 */
int _GET_RUBY_ELEMENT(RS_STACKDATA *stack, int argument_count);

/**
 * Reads an element off the stack and discards it.
 *
 * @mangled _SET_RUBY_ELEMENT__FP12RS_STACKDATAi
 * @address 0x1BD040
 * @size 0x24
 */
int _SET_RUBY_ELEMENT(RS_STACKDATA *stack, int argument_count);

/**
 * Suppresses the plate that names the floor.
 *
 * @mangled _SET_FLOOR_TITLE_OFF__FP12RS_STACKDATAi
 * @address 0x1BD070
 * @size 0x14
 */
int _SET_FLOOR_TITLE_OFF(RS_STACKDATA *stack, int argument_count);

/**
 * Restores the zone the party was fenced into and its warning sound.
 *
 * @mangled _SET_RES_LIMMIT_ZONE__FP12RS_STACKDATAi
 * @address 0x1BD090
 * @size 0x50
 */
int _SET_RES_LIMMIT_ZONE(RS_STACKDATA *stack, int argument_count);

/**
 * Clears the recorded progress through the Demon Shaft.
 *
 * @mangled _CLEAR_DEAMON_SHAFT__FP12RS_STACKDATAi
 * @address 0x1BD0E0
 * @size 0x28
 */
int _CLEAR_DEAMON_SHAFT(RS_STACKDATA *stack, int argument_count);

/**
 * Starts the swarm of bees on one of the party's characters.
 *
 * @mangled _INIT_BEE__FP12RS_STACKDATAi
 * @address 0x1BD110
 * @size 0x64
 */
int _INIT_BEE(RS_STACKDATA *stack, int argument_count);

/**
 * Ends the swarm of bees.
 *
 * @mangled _END_BEE__FP12RS_STACKDATAi
 * @address 0x1BD180
 * @size 0x18
 */
int _END_BEE(RS_STACKDATA *stack, int argument_count);

/**
 * Reports whether the Eastern king's puzzle has been completed.
 *
 * @mangled _EASTKING_COMPLETE__FP12RS_STACKDATAi
 * @address 0x1BD1A0
 * @size 0x40
 */
int _EASTKING_COMPLETE(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the trap on the chest the party is opening.
 *
 * @mangled _GET_ITEM_TRAPID__FP12RS_STACKDATAi
 * @address 0x1BD1E0
 * @size 0x44
 */
int _GET_ITEM_TRAPID(RS_STACKDATA *stack, int argument_count);

/**
 * Clears the trap on the chest the party is opening.
 *
 * @mangled _RESET_ITEM_TRAP__FP12RS_STACKDATAi
 * @address 0x1BD230
 * @size 0x34
 */
int _RESET_ITEM_TRAP(RS_STACKDATA *stack, int argument_count);

/**
 * Sets off an explosion above the party and pushes it away from them.
 *
 * @mangled _BOM_SET__FP12RS_STACKDATAi
 * @address 0x1BD270
 * @size 0x100
 */
int _BOM_SET(RS_STACKDATA *stack, int argument_count);

/**
 * Inflicts a status ailment on the party and raises the collision that shows it.
 *
 * @mangled _SET_STATUS_ERR__FP12RS_STACKDATAi
 * @address 0x1BD370
 * @size 0x90
 */
int _SET_STATUS_ERR(RS_STACKDATA *stack, int argument_count);

/**
 * Reports the state of the Mardan Garayan fishing quest.
 *
 * @mangled _CHECK_MARDAN__FP12RS_STACKDATAi
 * @address 0x1BD400
 * @size 0x34
 */
int _CHECK_MARDAN(RS_STACKDATA *stack, int argument_count);

/**
 * Keeps the controlled character across the end of the event.
 *
 * @mangled _NO_RESET_CHARA_NO__FP12RS_STACKDATAi
 * @address 0x1BD440
 * @size 0x14
 */
int _NO_RESET_CHARA_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Reports whether a party member is waiting to be rescued.
 *
 * @mangled _CHECK_CHR_HELP__FP12RS_STACKDATAi
 * @address 0x1BD460
 * @size 0x2C
 */
int _CHECK_CHR_HELP(RS_STACKDATA *stack, int argument_count);

/**
 * Holds the item event of a number back in both event managers.
 *
 * @mangled _HOLD_ITEM_EVENT__FP12RS_STACKDATAi
 * @address 0x1BD490
 * @size 0x58
 */
int _HOLD_ITEM_EVENT(RS_STACKDATA *stack, int argument_count);

/**
 * Stops the current battle music.
 *
 * @mangled _STOP_BATTLE_BGM__FP12RS_STACKDATAi
 * @address 0x1BD4F0
 * @size 0x24
 */
int _STOP_BATTLE_BGM(RS_STACKDATA *stack, int argument_count);

/**
 * Keeps status ailments across the end of the event.
 *
 * @mangled _NO_STATUS_RECOVER__FP12RS_STACKDATAi
 * @address 0x1BD520
 * @size 0x14
 */
int _NO_STATUS_RECOVER(RS_STACKDATA *stack, int argument_count);

/**
 * Records the party's progress through a quest dungeon.
 *
 * @mangled _SET_QUEST_DUNGEON__FP12RS_STACKDATAi
 * @address 0x1BD540
 * @size 0x5C
 */
int _SET_QUEST_DUNGEON(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the dungeon the party is in, counted from one.
 *
 * @mangled _GET_MAP_CODE__FP12RS_STACKDATAi
 * @address 0x1BD5A0
 * @size 0x30
 */
int _GET_MAP_CODE(RS_STACKDATA *stack, int argument_count);

/**
 * Loads the icons used by the active item slots.
 *
 * @mangled _SET_ACTIVE_ITEM_ICON__FP12RS_STACKDATAi
 * @address 0x1BD5D0
 * @size 0x24
 */
int _SET_ACTIVE_ITEM_ICON(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the party member a weapon item belongs to, or -1 for an item that is not a weapon.
 *
 * @mangled _GET_ITEM_UNIT_NO__FP12RS_STACKDATAi
 * @address 0x1BD600
 * @size 0xE4
 */
int _GET_ITEM_UNIT_NO(RS_STACKDATA *stack, int argument_count);

/**
 * Turns the chest the party is opening.
 *
 * @mangled _SET_IBOX_ANGLE__FP12RS_STACKDATAi
 * @address 0x1BD6F0
 * @size 0x44
 */
int _SET_IBOX_ANGLE(RS_STACKDATA *stack, int argument_count);

/**
 * Closes the chest the party is opening and drops it out of sight.
 *
 * @mangled _SET_IBOX_FINISH__FP12RS_STACKDATAi
 * @address 0x1BD740
 * @size 0x50
 */
int _SET_IBOX_FINISH(RS_STACKDATA *stack, int argument_count);

/**
 * Gives the two items the clown trades on this floor.
 *
 * @mangled _GET_PIERO_ITEM__FP12RS_STACKDATAi
 * @address 0x1BD790
 * @size 0x64
 */
int _GET_PIERO_ITEM(RS_STACKDATA *stack, int argument_count);

/**
 * Fills the opcode table from the extension list, refusing a list that names one twice.
 *
 * @mangled AddSystemEventScript__Fv
 * @address 0x1BD800
 * @size 0xFC
 */
void AddSystemEventScript();
