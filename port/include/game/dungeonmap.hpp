#pragma once

/**
 * @file
 * Declares the dungeon map and the objects that it holds.
 */

#include "common.h"

#include <libvu0.h>

#include "character.hpp"
#include "collision.hpp"
#include "dataalloc_fwd.hpp"
#include "dungeonparts.hpp"
#include "fireomni.hpp"
#include "water.hpp"

namespace dc::game {

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

class CCameraFollow;

/**
 * Describes one cell of the 20 x 20 dungeon floor grid.
 */
struct MAPPARTS {
    s32   parts_no;    /**< MapPartsNo of the part that the cell draws. */
    s32   direction;   /**< Rotation that the map part uses. */
    float camera_dist; /**< Distance from the camera to the cell, as the last draw measured it. */
    s32   visible;     /**< 1 if the last draw showed the cell. */
};

using MAP_CELL = MAPPARTS;

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
     */
    void SetNPC(int npc_no, unsigned int *pack, int parts_no, sceVu0FVECTOR pos, sceVu0FVECTOR rot, int visible, int motion_no, CDataAlloc2<1> *alloc);

    /**
     * Sets the number of copies to draw of every character to zero.
     */
    void ClearNPC_Cash();

    /**
     * Adds one copy of a character to the list of copies to draw.
     */
    void ReservNPC_Draw(int npc_no, float x, float y, float z, int direction);

    /**
     * Draws every copy of every character that was reserved since the last clear.
     */
    void DrawNPCDraw();

    /**
     * Takes one step with every character that is active and visible.
     */
    void StepNPC();

    /**
     * Starts a motion on a character.
     */
    void NPCSetMotion(int npc_no, int motion_no);

    /**
     * Starts a motion on a character at a given speed.
     */
    void NPCSetMotion(int npc_no, int motion_no, float speed, int motion_flags);

    /**
     * Returns the frame with the given name from the map parts, or nullptr if no part holds it.
     */
    CFrame *GetFrameSearch(char *name);

    /**
     * Draws every map part at the origin, and reserves a copy of each character
     * that stands on one.
     */
    void DrawMapFreeStyle();

    /**
     * Prepares every map part for the coming draw, placing each one at its grid
     * cell when mode is 1, or leaving it where it stands otherwise.
     */
    void DrawMapCalc(int mode);

    /**
     * Draws the floor's rooms and corridors.
     */
    void DrawMap(CCameraFollow *camera, CFrameVu1 *player);

    /**
     * Draws the background models at the position of the camera.
     */
    void DrawBGModel(CCamera *camera);

    /**
     * Draws every dummy model that the camera is near enough to.
     */
    void DrawDummyModel(CCamera *camera);

    /**
     * Draws the corner map of the floor.
     */
    void DrawMiniMap(float *pos, float angle);

    /**
     * Marks the cell at a position, and the room that holds it, as found.
     */
    void checkMask(float x, float z);

    /**
     * Hides every cell of the mini map, then shows the cells that hold a stair.
     */
    void FlushCheckMask();

    /**
     * Draws the floor's freely placed fires.
     */
    void DrawFireFreeStyle(CFrameVu1 *frame, CCameraFollow *camera);

    /**
     * Draws the fires standing on the floor's parts.
     */
    void DrawFire(CFrameVu1 *frame, CCameraFollow *camera);

    /**
     * Draws the light the floor's fires cast on the ground.
     */
    void DrawRaster(CFrameVu1 *frame);

    /**
     * Draws the water surface nearest to a position, with its waterfall, and
     * sets the volume of the water sound.
     */
    void DrawWater(float *pos, int mute);

    /**
     * Draws the floor's treasure chests.
     */
    void DrawItemBox(float *pos);

    /**
     * Draws every atla ball that the camera is near enough to, bobbing on the spot.
     */
    void DrawAtraBoll(float *pos);

    /**
     * Adds a collision polygon for every atla ball near the player, and returns
     * the new total.
     */
    int CreateCollision(CCPoly *poly, CBoxVu0 box, int num);

    /**
     * Sets every trap circle slot to free.
     */
    void initTrapCircle();

    /**
     * Returns the trap circle that is nearer to a position than a distance, or nullptr if there is none.
     */
    float *CheckTrapCircle(float *pos, float dist);

    /**
     * Puts a trap circle of a random kind in the first free slot at a position.
     */
    void SetupTrapCircle(float *pos);

    /**
     * Draws every trap circle that the player is near enough to.
     */
    void DrawTrapCircle();

    /**
     * Starts the fade of the trap circle that the player stands on, and returns its position.
     */
    MAP_TRAP_CIRCLE *DistTrapCircle();

    /**
     * Moves every trap circle that fades one step further, and frees the ones that finished.
     */
    void StepTrapCircle();

    /**
     * Returns 1 if no treasure box is nearer to a position than a distance.
     */
    int CheckTreasureBox(float *pos, float dist);

    /**
     * Returns 1 if no atla ball is nearer to a position than a distance.
     */
    int CheckAtra(float *pos, float dist);

    /**
     * Puts an atla ball on the floor at a position, with the event that picks it up.
     */
    void SetAtraBoll(float *pos, int atra_no);

    /**
     * Puts a treasure chest on the floor.
     */
    int SetTreasureBox(float *pos, int item_no, int kind, int trap_no);

    /**
     * Lays the floor's events out over its rooms.
     */
    void buildEventData(int floor_no, int enabled, int place_atla);

    /**
     * Puts a mimic on the floor, as a treasure box and the event that opens it.
     */
    void SetMimicEvent(float x, float y, float z, int item_no, int kind);

    /**
     * Frees every mimic event, and the treasure box that each one shows.
     */
    void RsetMimicEvent();

    /**
     * Returns the event that the player stands in, or -1 if the player stands in none.
     */
    int GetActiveIvent(CFrameVu1 *frame);

    /**
     * Scatters up to eight dummy models over the empty cells of the grid.
     */
    void buildDummyModel();

    /**
     * Gives the table saying which of the floor's rooms join up.
     */
    void GetRoomLinkInfo();

    /**
     * Puts the part that closes the right edge of the grid in every row.
     */
    void SetUnderLoad();

    /**
     * Fills list with the column and row of each cell that holds a part from
     * first_part to last_part, or with first_part alone when last_part is -1,
     * and returns how many cells it found.
     */
    int CreatPartsList(int *list, int max, int first_part, int last_part);

    /**
     * Puts the doors that the party characters open on the floor.
     */
    void BuildCharaSpecialParts();

    /**
     * Puts the door that a party character opens on a map part that suits it, and
     * returns the number of cells that could take it.
     */
    int SetCharaDoor(int chara_no);

    /**
     * Builds a floor: places the rooms, joins them with corridors, fills the
     * grid with map parts and picks the set of parts that the floor draws with.
     */
    void buildRandomMap(int room_max, int first_build);

    /**
     * Empties the floor and sizes the water surface from an allocator.
     */
    void initSubmap(CDataAlloc2<1> *alloc);

    /**
     * Empties the floor and resets the draw distance.
     */
    void initalize();
};

/**
 * Turns the grid that the builder worked on into the map parts that draw it.
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

} // namespace dc::game
