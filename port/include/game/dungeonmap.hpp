#pragma once

#include "common.h"

#include <libvu0.h>

#include "character.hpp"
#include "collision.hpp"
#include "dataalloc_fwd.hpp"
#include "dungeonparts.hpp"
#include "fireomni.hpp"
#include "water.hpp"

/**
 * Kinds of dungeon event, as DUNGEON_EVENT::kind holds them.
 */
// clang-format off
enum DungeonEventKind {
    DNG_EVENT_NONE         = -1, /**< Free. */
    DNG_EVENT_TREASURE_BOX = 2,  /**< Treasure box. */
    DNG_EVENT_ATRA         = 3,  /**< Atla ball. */
    DNG_EVENT_MIMIC        = 8,  /**< Mimic. */
};

// clang-format on

/**
 * Effects of a dungeon magic circle, as MAP_TRAP_CIRCLE::kind holds them.
 */
// clang-format off
enum TrapCircleKind {
    TRAP_CIRCLE_STAMINA             = 0,  /**< Gives the player stamina. */
    TRAP_CIRCLE_MONEY_UP            = 1,  /**< Raises the player's money. */
    TRAP_CIRCLE_ABS_FULL            = 2,  /**< Fills the weapon's ABS. */
    TRAP_CIRCLE_WHP_UP              = 3,  /**< Raises the weapon's maximum WHp. */
    TRAP_CIRCLE_WHP_CURE            = 4,  /**< Restores the weapon's WHp. */
    TRAP_CIRCLE_ALL_MONSTER_STAMINA = 5,  /**< Gives every monster stamina. */
    TRAP_CIRCLE_MONEY_DOWN          = 6,  /**< Lowers the player's money. */
    TRAP_CIRCLE_STAT_DOWN           = 7,  /**< Lowers a random weapon stat. */
    TRAP_CIRCLE_WHP_DOWN            = 8,  /**< Lowers the weapon's maximum WHp. */
    TRAP_CIRCLE_WHP_QUARTER         = 9,  /**< Quarters the weapon's WHp. */
    TRAP_CIRCLE_COUNT               = 10, /**< Number of effects. */
};

// clang-format on

/**
 * Kinds of treasure box, as TREASURE_BOX::kind holds them.
 */
// clang-format off
enum TreasureBoxKind {
    TREASURE_BOX_LARGE = 0, /**< Large box. */
    TREASURE_BOX_SMALL = 1, /**< Small box. */
};

// clang-format on

/**
 * Directions out of a dungeon room, as bits.
 */
// clang-format off
enum MapDirection {
    MAP_DIR_NORTH = 1, /**< North. */
    MAP_DIR_EAST  = 2, /**< East. */
    MAP_DIR_WEST  = 4, /**< West. */
    MAP_DIR_SOUTH = 8, /**< South. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CCameraFollow;

/**
 * @file
 * Declares the dungeon map and the objects that it holds.
 */

/**
 * Describes one cell of the 20 x 20 dungeon floor grid.
 */
struct MAPPARTS {
    s32   parts_no;    /**< MapPartsNo of the part that the cell draws. */
    s32   direction;   /**< Rotation that the map part uses. */
    float camera_dist; /**< Distance from the camera to the cell, as the last draw measured it. */
    s32   visible;     /**< 1 if the last draw showed the cell. */
};

typedef MAPPARTS MAP_CELL;

/**
 * Describes the position and the extent of one room, in grid cells.
 */
struct ROOM_INFO {
    s32 x;      /**< Grid column of the left edge. */
    s32 y;      /**< Grid row of the top edge. */
    s32 width;  /**< Width in grid cells. */
    s32 height; /**< Height in grid cells. */
};

/**
 * Describes one treasure box, or one mimic that looks like a treasure box.
 */
struct TREASURE_BOX {
    s32           used; /**< 1 if the slot is in use. */
    u8            unk_04[12];
    sceVu0FVECTOR pos;       /**< World position of the box. */
    s32           item_no;   /**< Identifier of the item inside the box. */
    s32           closed;    /**< 1 until the box is opened; the mini map shows only closed boxes. */
    s32           kind;      /**< Size of the box. @see TreasureBoxKind. */
    float         lid_angle; /**< Degrees the lid has swung open. */
    s32           trap_no;   /**< Trap that opening the box sets off, or 0 for none. */
    u8            unk_34[12];
};

/**
 * Describes one atla ball that the player can collect.
 */
struct ATRA_BOLL {
    float pos[4];  /**< World position of the ball. */
    s32   atra_no; /**< Identifier of the atla inside the ball. */
    s32   used;    /**< 1 if the slot is in use. */
    float phase;   /**< Angle that makes the ball move up and down. */
    s32   unk_1C;
};

/**
 * Describes one trap circle that a trap left on the floor.
 */
struct MAP_TRAP_CIRCLE {
    float pos[4]; /**< World position of the circle. */
    s32   state;  /**< 0 for a free slot, 1 while the circle waits, 2 while the circle fades. */
    s32   kind;   /**< Trap that the circle shows. @see TrapCircleKind. */
    float timer;  /**< Time that passed since the circle started to fade. */
    s32   unk_1C;
};

/**
 * Places the door and the treasure box that link one pair of rooms.
 */
struct ROOM_LINK_RESULT {
    s32   door_x; /**< Grid column of the door that the link opens. */
    s32   door_y; /**< Grid row of the door that the link opens. */
    float item_x; /**< Mini map position of the item that the link places. */
    float item_y; /**< Mini map position of the item that the link places. */
    s32   used;   /**< 1 after the linked-room item and door are placed. */
    s32   unk_14;
};

/**
 * Marks one place on the floor that starts an event when the player comes near.
 */
struct DUNGEON_EVENT {
    s32   origin_x;    /**< World X of the object the event belongs to, in whole units. */
    s32   origin_z;    /**< World Z of the object the event belongs to, in whole units. */
    s32   kind;        /**< Kind of event in the slot. @see DungeonEventKind. */
    s32   placed_flag; /**< Cleared when the event is placed; nothing reads it. */
    float pos[4];      /**< World position that the event uses. */
    s32   unk_20;
    float radius;     /**< Distance at which the event starts. */
    s32   index;      /**< Index of the object that the event belongs to. */
    s32   reset_flag; /**< Cleared whenever the floor's events are reset; nothing reads it. */
    u8    unk_30[32];
};

/**
 * Describes one non-player character that walks in the dungeon.
 */
struct MAP_NPC_MODEL {
    CCharacter chara;           /**< Draws and moves the character. */
    float      pos[4];          /**< World position of the character. */
    float      rot[4];          /**< Rotation of the character; its Y part turns every drawn copy. */
    s32        parts_no;        /**< Index of the map part that the character stands on. */
    s32        used;            /**< 1 if the slot is in use. */
    s32        visible;         /**< 0 to hide the character. */
    s32        motion_no;       /**< Motion that the character started with, or -1 to stop it from taking a step. */
    float      draw_pos[16][4]; /**< Position of each copy of the character to draw. */
    s32        draw_param[16];  /**< Parameter of each copy of the character to draw. */
    s32        draw_num;        /**< Number of copies of the character to draw. */
    u8         unk_1324[12];

    /**
     * Copies one slot over another, field by field.
     *
     * @mangled __as__13MAP_NPC_MODELFRC13MAP_NPC_MODEL
     * @address 0x142C90
     * @size 0x104
     * @unknownret
     */
    MAP_NPC_MODEL &operator=(const MAP_NPC_MODEL &other);
};

/**
 * Builds, steps and draws one dungeon floor, and everything that stands on it.
 */
class CDungeonMap {
public:
    s32              unk_0000;
    s32              unk_0004;
    s32              room_seen[16];         /**< 1 for each room that the player found. */
    CFireOmni        fire;                  /**< Draws the fire and the raster of every map part. */
    CWater           water;                 /**< Draws the water surface. */
    float            draw_dist_scale;       /**< Scale that the draw distance uses. */
    s32              bg_model_num;          /**< Number of background models loaded into bg_model. */
    s32              dummy_frame_num;       /**< Number of models loaded into dummy_frame. */
    sceVu0FVECTOR    dummy_pos[8];          /**< World position of each dummy model. */
    s32              dummy_model[8];        /**< Index into dummy_frame of each dummy model. */
    s32              dummy_num;             /**< Number of dummy models on the floor. */
    s32              gate_key_no;           /**< Item number of the key that opens the floor's gate. */
    s32              map_seed;              /**< Seed that the floor was built from. */
    CFrame          *dummy_frame[3];        /**< Models that a dummy model can draw with. */
    CFrame          *bg_model[6];           /**< Models that draw behind the floor. */
    CDungeonParts    parts[72];             /**< Map parts that the floor is built from. */
    s32              mask[400];             /**< 1 for each grid cell that the mini map shows. */
    DUNGEON_EVENT    events[48];            /**< Places that start an event. */
    MAP_CELL         cells[400];            /**< 20 x 20 grid of the floor. */
    ROOM_INFO        rooms[16];             /**< Position and extent of each room. */
    s32              room_num;              /**< Number of rooms on the floor. */
    TREASURE_BOX     boxes[24];             /**< Treasure boxes that stand on the floor. */
    s32              box_num;               /**< Number of treasure boxes on the floor. */
    CFrame          *box_lid_model;         /**< Model that draws the lid of a wooden treasure box. */
    CFrame          *box_body_model;        /**< Model that draws a wooden treasure box. */
    CFrame          *box_collision_model;   /**< Model that the collision of a treasure box uses. */
    CFrame          *chest_lid_model;       /**< Model that draws the lid of a metal treasure box. */
    CFrame          *chest_body_model;      /**< Model that draws a metal treasure box. */
    CFrame          *chest_collision_model; /**< Model that the collision of a metal treasure box uses. */
    CFrame          *fall_model;            /**< Model that draws the hole a fallen map part leaves. */
    ATRA_BOLL        atra[8];               /**< Atla balls that lie on the floor. */
    s32              atra_num;              /**< Number of atla balls on the floor. */
    CFrame          *atra_model;            /**< Model that draws an atla ball. */
    CFrame          *collision_model;       /**< Model that the collision of an atla ball uses. */
    ROOM_LINK_RESULT room_link[4];          /**< Rooms that a door and a treasure box link. */
    s32              map_type;              /**< 1 for a floor laid out on the grid, otherwise a floor of freely placed parts. */
    MAP_NPC_MODEL    npc[4];                /**< Characters that walk in the dungeon. */
    MAP_TRAP_CIRCLE  trap_circle[3];        /**< Trap circles that lie on the floor. */

    /**
     * Puts one non-player character on the floor.
     *
     * @mangled SetNPC__11CDungeonMapFiPUiiPfPfiiP14CDataAlloc2_1_
     * @address 0x1C1C00
     * @size 0x26C
     */
    void SetNPC(int npc_no, unsigned int *pack, int parts_no, sceVu0FVECTOR pos, sceVu0FVECTOR rot, int visible, int motion_no, CDataAlloc2<1> *alloc);

    /**
     * Sets the number of copies to draw of every character to zero.
     *
     * @mangled ClearNPC_Cash__11CDungeonMapFv
     * @address 0x1C1E70
     * @size 0x3C
     */
    void ClearNPC_Cash();

    /**
     * Adds one copy of a character to the list of copies to draw.
     *
     * @mangled ReservNPC_Draw__11CDungeonMapFifffi
     * @address 0x1C1EB0
     * @size 0xC4
     */
    void ReservNPC_Draw(int npc_no, float x, float y, float z, int direction);

    /**
     * @mangled DrawNPCDraw__11CDungeonMapFv
     * @address 0x1C1F80
     * @size 0x264
     * @unknownret
     */
    void DrawNPCDraw();

    /**
     * Takes one step with every character that is active and visible.
     *
     * @mangled StepNPC__11CDungeonMapFv
     * @address 0x1C21F0
     * @size 0xC0
     * @unknownret
     */
    void StepNPC();

    /**
     * Starts a motion on a character.
     *
     * @mangled NPCSetMotion__11CDungeonMapFii
     * @address 0x1C22B0
     * @size 0x3C
     */
    void NPCSetMotion(int npc_no, int motion_no);

    /**
     * Starts a motion on a character at a given speed.
     *
     * @mangled NPCSetMotion__11CDungeonMapFiifi
     * @address 0x1C22F0
     * @size 0x40
     */
    void NPCSetMotion(int npc_no, int motion_no, float speed, int motion_flags);

    /**
     * Returns the frame with the given name from the map parts, or zero if no part holds it.
     *
     * @mangled GetFrameSearch__11CDungeonMapFPc
     * @address 0x1C2330
     * @size 0x90
     */
    CFrame *GetFrameSearch(char *name);

    /**
     * Draws every map part at the origin, and reserves a copy of each character
     * that stands on one.
     *
     * @mangled DrawMapFreeStyle__11CDungeonMapFv
     * @address 0x1C23C0
     * @size 0xF8
     */
    void DrawMapFreeStyle();

    /**
     * @mangled DrawMapCalc__11CDungeonMapFi
     * @address 0x1C24C0
     * @size 0x1C8
     * @unknownret
     */
    void DrawMapCalc(int mode);

    /**
     * Draws the floor's rooms and corridors.
     *
     * @mangled DrawMap__11CDungeonMapFP13CCameraFollowP9CFrameVu1
     * @address 0x1C2690
     * @size 0x92C
     */
    void DrawMap(CCameraFollow *camera, CFrameVu1 *player);

    /**
     * Draws the background models at the position of the camera.
     *
     * @mangled DrawBGModel__11CDungeonMapFP7CCamera
     * @address 0x1C2FC0
     * @size 0x94
     * @unknownret
     */
    void DrawBGModel(CCamera *camera);

    /**
     * Draws every dummy model that the camera is near enough to.
     *
     * @mangled DrawDummyModel__11CDungeonMapFP7CCamera
     * @address 0x1C3060
     * @size 0x120
     */
    void DrawDummyModel(CCamera *camera);

    /**
     * Draws the corner map of the floor.
     *
     * @mangled DrawMiniMap__11CDungeonMapFPff
     * @address 0x1C3180
     * @size 0x840
     */
    void DrawMiniMap(float *pos, float angle);

    /**
     * Marks the cell at a position, and the room that holds it, as found.
     *
     * @mangled checkMask__11CDungeonMapFff
     * @address 0x1C39C0
     * @size 0x23C
     * @unknownret
     */
    void checkMask(float x, float z);

    /**
     * Hides every cell of the mini map, then shows the cells that hold a stair.
     *
     * @mangled FlushCheckMask__11CDungeonMapFv
     * @address 0x1C3C00
     * @size 0xB4
     */
    void FlushCheckMask();

    /**
     * Draws the floor's freely placed fires.
     *
     * @mangled DrawFireFreeStyle__11CDungeonMapFP9CFrameVu1P13CCameraFollow
     * @address 0x1C3CC0
     * @size 0x3F4
     */
    void DrawFireFreeStyle(CFrameVu1 *frame, CCameraFollow *camera);

    /**
     * Draws the fires standing on the floor's parts.
     *
     * @mangled DrawFire__11CDungeonMapFP9CFrameVu1P13CCameraFollow
     * @address 0x1C40C0
     * @size 0x548
     */
    void DrawFire(CFrameVu1 *frame, CCameraFollow *camera);

    /**
     * Draws the light the floor's fires cast on the ground.
     *
     * @mangled DrawRaster__11CDungeonMapFP9CFrameVu1
     * @address 0x1C4610
     * @size 0x324
     */
    void DrawRaster(CFrameVu1 *frame);

    /**
     * Draws the water surface nearest to a position, with its waterfall, and
     * sets the volume of the water sound.
     *
     * @mangled DrawWater__11CDungeonMapFPfi
     * @address 0x1C4940
     * @size 0x494
     * @unknownret
     */
    void DrawWater(float *pos, int mute);

    /**
     * Draws the floor's treasure chests.
     *
     * @mangled DrawItemBox__11CDungeonMapFPf
     * @address 0x1C4DE0
     * @size 0x318
     */
    void DrawItemBox(float *pos);

    /**
     * Draws every atla ball that the camera is near enough to, bobbing on the spot.
     *
     * @mangled DrawAtraBoll__11CDungeonMapFPf
     * @address 0x1C5100
     * @size 0x170
     */
    void DrawAtraBoll(float *pos);

    /**
     * Adds a collision polygon for every atla ball near the player, and returns
     * the new total.
     *
     * @mangled CreateCollision__11CDungeonMapFP6CCPoly7CBoxVu0i
     * @address 0x1C5270
     * @size 0x144
     */
    int CreateCollision(CCPoly *poly, CBoxVu0 box, int num);

    /**
     * Sets every trap circle slot to free.
     *
     * @mangled initTrapCircle__11CDungeonMapFv
     * @address 0x1C79B0
     * @size 0x38
     */
    void initTrapCircle();

    /**
     * Returns the trap circle that is nearer to a position than a distance, or zero if there is none.
     *
     * @mangled CheckTrapCircle__11CDungeonMapFPff
     * @address 0x1C79F0
     * @size 0xC0
     */
    float *CheckTrapCircle(float *pos, float dist);

    /**
     * Puts a trap circle of a random kind in the first free slot at a position.
     *
     * @mangled SetupTrapCircle__11CDungeonMapFPf
     * @address 0x1C7AB0
     * @size 0x118
     */
    void SetupTrapCircle(float *pos);

    /**
     * @mangled DrawTrapCircle__11CDungeonMapFv
     * @address 0x1C7BD0
     * @size 0x1A4
     * @unknownret
     */
    void DrawTrapCircle();

    /**
     * Starts the fade of the trap circle that the player stands on, and returns its position.
     *
     * @mangled DistTrapCircle__11CDungeonMapFv
     * @address 0x1C7D80
     * @size 0xE0
     */
    MAP_TRAP_CIRCLE *DistTrapCircle();

    /**
     * Moves every trap circle that fades one step further, and frees the ones that finished.
     *
     * @mangled StepTrapCircle__11CDungeonMapFv
     * @address 0x1C7E60
     * @size 0x7C
     */
    void StepTrapCircle();

    /**
     * Returns 1 if no treasure box is nearer to a position than a distance.
     *
     * @mangled CheckTreasureBox__11CDungeonMapFPff
     * @address 0x1C7EE0
     * @size 0xBC
     */
    int CheckTreasureBox(float *pos, float dist);

    /**
     * Returns 1 if no atla ball is nearer to a position than a distance.
     *
     * @mangled CheckAtra__11CDungeonMapFPff
     * @address 0x1C7FA0
     * @size 0xD8
     */
    int CheckAtra(float *pos, float dist);

    /**
     * @mangled SetAtraBoll__11CDungeonMapFPfi
     * @address 0x1C8080
     * @size 0x1C0
     * @unknownret
     */
    void SetAtraBoll(float *pos, int atra_no);

    /**
     * Puts a treasure chest on the floor.
     *
     * @mangled SetTreasureBox__11CDungeonMapFPfiii
     * @address 0x1C8240
     * @size 0x2F4
     */
    int SetTreasureBox(float *pos, int item_no, int kind, int trap_no);

    /**
     * Lays the floor's events out over its rooms.
     *
     * @mangled buildEventData__11CDungeonMapFiii
     * @address 0x1C8540
     * @size 0x680
     */
    void buildEventData(int floor_no, int enabled, int place_atla);

    /**
     * @mangled SetMimicEvent__11CDungeonMapFfffii
     * @address 0x1C8BC0
     * @size 0x28C
     * @unknownret
     */
    void SetMimicEvent(float x, float y, float z, int item_no, int kind);

    /**
     * Frees every mimic event, and the treasure box that each one shows.
     *
     * @mangled RsetMimicEvent__11CDungeonMapFv
     * @address 0x1C8E50
     * @size 0x7C
     */
    void RsetMimicEvent();

    /**
     * Returns the event that the player stands in, or -1 if the player stands in none.
     *
     * @mangled GetActiveIvent__11CDungeonMapFP9CFrameVu1
     * @address 0x1C8ED0
     * @size 0x144
     */
    int GetActiveIvent(CFrameVu1 *frame);

    /**
     * @mangled buildDummyModel__11CDungeonMapFv
     * @address 0x1C9020
     * @size 0x5B0
     * @unknownret
     */
    void buildDummyModel();

    /**
     * Gives the table saying which of the floor's rooms join up.
     *
     * @mangled GetRoomLinkInfo__11CDungeonMapFv
     * @address 0x1C95D0
     * @size 0xC70
     */
    void GetRoomLinkInfo();

    /**
     * Puts the part that closes the right edge of the grid in every row.
     *
     * @mangled SetUnderLoad__11CDungeonMapFv
     * @address 0x1CA240
     * @size 0x50
     */
    void SetUnderLoad();

    /**
     * @mangled CreatPartsList__11CDungeonMapFPiiii
     * @address 0x1CA290
     * @size 0x1E4
     */
    int CreatPartsList(int *list, int max, int first_part, int last_part);

    /**
     * @mangled BuildCharaSpecialParts__11CDungeonMapFv
     * @address 0x1CA480
     * @size 0xDA4
     * @unknownret
     */
    void BuildCharaSpecialParts();

    /**
     * @mangled SetCharaDoor__11CDungeonMapFi
     * @address 0x1CB230
     * @size 0x438
     */
    int SetCharaDoor(int chara_no);

    /**
     * Builds a floor: places the rooms, joins them with corridors, fills the
     * grid with map parts and picks the set of parts that the floor draws with.
     *
     * @mangled buildRandomMap__11CDungeonMapFii
     * @address 0x1CB670
     * @size 0xA80
     */
    void buildRandomMap(int room_max, int first_build);

    /**
     * @mangled initSubmap__11CDungeonMapFP14CDataAlloc2_1_
     * @address 0x1CC0F0
     * @size 0x3A0
     * @unknownret
     */
    void initSubmap(CDataAlloc2<1> *alloc);

    /**
     * @mangled initalize__11CDungeonMapFv
     * @address 0x1CC490
     * @size 0x390
     * @unknownret
     */
    void initalize();
};

/**
 * Turns the grid that the builder worked on into the map parts that draw it.
 *
 * @mangled mapPartsFilter__Fv
 * @address 0x1C5550
 * @size 0xBDC
 */
void mapPartsFilter();

STATIC_ASSERT(sizeof(MAP_CELL) == 0x10);
STATIC_ASSERT(sizeof(ROOM_INFO) == 0x10);
STATIC_ASSERT(sizeof(TREASURE_BOX) == 0x40);
STATIC_ASSERT(sizeof(ATRA_BOLL) == 0x20);
STATIC_ASSERT(sizeof(MAP_TRAP_CIRCLE) == 0x20);
STATIC_ASSERT(sizeof(ROOM_LINK_RESULT) == 0x18);
STATIC_ASSERT(sizeof(DUNGEON_EVENT) == 0x50);
STATIC_ASSERT(sizeof(MAP_NPC_MODEL) == 0x1330);
STATIC_ASSERT(sizeof(CDungeonMap) == 0x10B10);
