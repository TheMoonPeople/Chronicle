#pragma once

#include "common.h"

#include <cstring>

#include "dngstatusdata.hpp"
#include "stockitem.hpp"

namespace dc::game {

/**
 * @file
 * Declares the save slot record and its operations.
 */

/**
 * Holds the position that each menu screen shows again when the player opens it.
 */
class CMenuCursor {
public:
    s16 reset_pos; /**< Set to zero to keep the menu positions. Any other value discards them. */
    s16 pos[10];   /**< Contains the cursor position of each menu. */
    s16 mode[10];  /**< Contains the mode of each menu. */
    s16 chara_no;  /**< Identifies the character that the item menu shows. */

    /**
     * Sets all menu positions and modes to their default values.
     */
    void InitPos();
};

STATIC_ASSERT(sizeof(CMenuCursor) == 0x2C);

/**
 * Contains the configuration values in the format that the memory card uses.
 *
 * The values are written three times. If the three copies do not agree, the
 * memory card data is damaged.
 */
struct SV_CONFIG_SYS {
    char values[18];          /**< Contains the configuration values. */
    char values_copy1[18];    /**< Contains the first copy of the configuration values. */
    char values_copy2[18];    /**< Contains the second copy of the configuration values. */
    char game_clear_area[10]; /**< Bytes past the three copies; the word at offset two holds the game clear flag. */
};

/**
 * Contains one entry of the fishing leaderboard.
 */
struct SV_FISH_DATA {
    s32   fish_id; /**< Identifies the fish. */
    float rank;    /**< Contains the score that gives the position in the leaderboard. */
    float unk_8;
    float unk_C;
};

/**
 * Contains the state of one Georama plot.
 */
struct SV_EDIT_PARTS_INFO {
    s32 completion_flags;   /**< Completion flags of the plot's part, as CEditPartsInfo keeps them. */
    s16 obtained;           /**< Set to one once the player has obtained the building. */
    s16 placed;             /**< Copies of the part standing on the map. */
    s16 stock;              /**< Copies of the part the player holds; a stackable part gains five per pickup. */
    s16 element_enabled[6]; /**< Whether each of the part's six attachment elements is in place. */
    s16 unk_16;
};

/**
 * Contains one Georama part that the player put on the map.
 */
struct SV_GRD_PART {
    s16   part_id; /**< Plot part the record places, or -1 to end the list. */
    s16   rot_y;   /**< Quarter turns about the vertical axis that the part faces. */
    float pos_x;   /**< Contains the X position of the part. */
    float pos_y;   /**< Contains the Y position of the part. */
    float pos_z;   /**< Contains the Z position of the part. */
};

/**
 * Contains one NPC slot of a Georama town.
 */
struct SV_GRD_NPC {
    s32  flags; /**< State of the slot; bit one keeps the georama menu from naming its chip. */
    char unk_4[4];
    s32  talk_message; /**< Message number currently assigned to this NPC slot. */
};

/**
 * Contains the Georama save data of one town.
 */
struct SV_GEORAMA_DATA {
    s32                request_count;        /**< Number of requests defined for this town. */
    s32                request_complete[24]; /**< Completion state of each town request. */
    s32                request_event_flag;   /**< Event request stored for this Georama town. */
    SV_EDIT_PARTS_INFO parts_info[24];       /**< Contains the state of each plot. */
    SV_GRD_PART        placed_parts[130];    /**< Contains the parts that the player put on the map. */
    SV_GRD_NPC         npc[20];              /**< Contains the NPC slots of the town. */
};

/**
 * Contains the data of one save slot.
 */
class CSaveData {
    // EdMoveChara counts the Mardan Garayan the player lands.
    friend void EdMoveChara();
    // The fish exchange debug shortcut records a Mardan Garayan catch.
    friend int FishingExchangeKey();

    s32             config[18];        /**< Contains the configuration values. */
    s16             chara_name[6][32]; /**< Contains the name of each character, as sixteen-bit characters. */
    s32             map_no;            /**< Map that the game resumes on. */
    s32             unk_1CC;
    float           now_time;      /**< Contains the time of day. */
    s32             play_time;     /**< Contains the play time. */
    s32             day;           /**< Contains the day count. */
    s32             fishing_point; /**< Contains the fishing points. */
    SV_FISH_DATA    fish_data[64]; /**< Contains the fishing leaderboard. */
    char            unk_5E0[112];
    CMenuCursor     menu_cursor;            /**< Contains the menu positions. */
    SV_GEORAMA_DATA georama[6];             /**< Contains the Georama data of each town. */
    s16             elem_data[6][128];      /**< Contains the chips of each town. */
    char            special_npc[320];       /**< Contains the NPC slots that the sub-maps share. */
    CDngStatusData  dng_status;             /**< Contains the dungeon status data. */
    CStockItem      stock_item;             /**< Contains the stock inventory. */
    u32             game_flags[256];        /**< Contains the game flag bits. */
    s32             game_int_flag[32];      /**< Contains the game integer flags. */
    u32             map_flags[80][16];      /**< Contains the map flag bits of each map. */
    u32             map_init_flags[80][16]; /**< Contains the map initialization flag bits of each map. */
    s16             visit_map[80];          /**< Contains the visit count of each map. */
    s16             quest_dungeon[6];       /**< Contains the quest count of each dungeon. */
    s32             mardan_garayan_caught;  /**< Number of Mardan Garayan caught. */
    s32             quest_dungeon_total;    /**< Contains the quest count of all the dungeons. */
    char            unk_12F9C[548];

public:
    /**
     * Sets all save data fields to their initial state.
     */
    CSaveData();

    /**
     * Sets all save data fields to their initial state.
     */
    void Initialize();

    /**
     * Returns the configuration values.
     */
    void *GetConfigData();

    /**
     * Returns the menu cursor.
     */
    CMenuCursor *GetMenuCursor();

    /**
     * Returns the name of a character, or nullptr if the character number is not valid.
     */
    s16 *GetCharaName(int chara_no);

    /**
     * Adds a value to the play time.
     */
    void AddPlayTime(int time);

    /**
     * Returns the play time.
     */
    int GetPlayTime();

    /**
     * Returns the map that the game resumes on.
     */
    int GetMapNo();

    /**
     * Sets the map that the game resumes on.
     */
    void SetMapNo(int map_no);

    /**
     * Sets the time of day.
     */
    void SetNowTime(float time);

    /**
     * Returns the time of day.
     */
    float GetNowTime();

    /**
     * Adds a value to the time of day and starts a new day if necessary.
     */
    void AddNowTime(float add);

    /**
     * Sets the day count and keeps the count in the permitted range.
     */
    void SetDay(int day);

    /**
     * Returns the day count.
     */
    int GetDay();

    /**
     * Adds a value to the day count.
     */
    void AddDay(int add);

    /**
     * Adds a value to the fishing points and keeps the points in the permitted range.
     */
    void AddFishingPoint(int add);

    /**
     * Sets the fishing points and keeps the points in the permitted range.
     */
    void SetFishingPoint(int value);

    /**
     * Puts a fish score in the leaderboard and sorts the leaderboard again.
     */
    void SetFishingRank(int fish_id, float score);

    /**
     * Returns a leaderboard entry, or nullptr if the entry is empty.
     */
    SV_FISH_DATA *GetFishingRank(int rank_index);

    /**
     * Returns the Georama data of a town, or nullptr if the town number is not valid.
     */
    SV_GEORAMA_DATA *GetGrdData(int georama_no);

    /**
     * Returns the state of a Georama plot, or nullptr if a number is not valid.
     */
    SV_EDIT_PARTS_INFO *GetEditPartsInfo(int georama_no, int plot_no);

    /**
     * Returns the Georama parts of a town and gives the number of parts that the town has.
     */
    SV_GRD_PART *GetParts(int georama_no, int *out_count);

    /**
     * Returns an NPC slot of a map, or nullptr if a number is not valid.
     */
    SV_GRD_NPC *GetGrdNPCData(int map_no, int npc_no);

    /**
     * Returns the chips of a town, or nullptr if the town number is not valid.
     */
    s16 *GetElemData(int georama_no);

    /**
     * Returns the dungeon status data.
     */
    CDngStatusData *GetDngStatus();

    /**
     * Returns the stock inventory.
     */
    CStockItem *GetStockItem();

    /**
     * Returns the value of a game flag, or zero if the flag number is not valid.
     */
    int GetGameFlag(int flag_no);

    /**
     * Sets a game flag and returns the new value.
     */
    int SetGameFlag(int flag_no, int value);

    /**
     * Returns the value of a game integer flag, or zero if the flag number is not valid.
     */
    int GetGameIntFlag(int flag_no);

    /**
     * Sets a game integer flag and returns the new value.
     */
    int SetGameIntFlag(int flag_no, int value);

    /**
     * Returns the value of a map flag, or zero if a number is not valid.
     */
    int GetMapFlag(int map_no, int flag_no);

    /**
     * Sets a map flag and returns the new value.
     */
    int SetMapFlag(int map_no, int flag_no, int value);

    /**
     * Returns the value of a map initialization flag, or zero if a number is not valid.
     */
    int GetMapInitFlag(int map_no, int flag_no);

    /**
     * Sets a map initialization flag and returns the new value.
     */
    int SetMapInitFlag(int map_no, int flag_no, int value);

    /**
     * Gives the player a Georama building and returns one if the state changed.
     */
    int AtraPartsGet(int georama_no, int plot_no);

    /**
     * Puts a chip in the first empty chip slot of a town and returns one if a slot was empty.
     */
    int AtraChipGet(int georama_no, int chip_id);

    /**
     * Adds a value to the visit count of a map and returns the new count.
     */
    int VisitMap(int map_no, int add);

    /**
     * Adds a value to the quest count of a dungeon and returns the new count.
     */
    int QuestDungeon(int dungeon_no, int add);

    /**
     * Writes the configuration values in the format that the memory card uses.
     */
    void ConvertConfig(SV_CONFIG_SYS *out);

    /**
     * Reads the configuration values from the memory card and returns zero if the copies do not agree.
     */
    int InvertConfig(SV_CONFIG_SYS *in);

    /**
     * Returns the fishing points the player holds.
     */
    s32 GetFishingPoint() const { return fishing_point; }

    /**
     * Returns the state of the Mardan Garayan fishing quest.
     */
    s32 GetMardanGareyanFlag() const { return mardan_garayan_caught; }
};

/**
 * Points to the save data of the game.
 */
extern CSaveData *SaveData;

STATIC_ASSERT(sizeof(CSaveData) == 78272);

} // namespace dc::game
