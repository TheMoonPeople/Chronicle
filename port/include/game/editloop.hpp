#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc_fwd.hpp"
#include "edit.hpp"
#include "objanime.hpp"

/**
 * Kinds of town event point, as ED_EVENT_POINT::event_type holds them.
 */
// clang-format off
enum EdEventPointType {
    ED_EVENT_POINT_NONE          = 0, /**< Unused slot. */
    ED_EVENT_POINT_DOOR          = 1, /**< Door. */
    ED_EVENT_POINT_ITEM_BOX      = 2, /**< Item box. */
    ED_EVENT_POINT_EVENT         = 3, /**< Map event. */
    ED_EVENT_POINT_LADDER_BOTTOM = 4, /**< Bottom of a ladder. */
    ED_EVENT_POINT_LADDER_TOP    = 5, /**< Top of a ladder. */
};

// clang-format on

class C3DSprite;

class CEffect;
class CRect_i_;
class CEditArea;
class CWind;
class CRunEffect;
struct sceVif1Packet;

/** Working arena used to load event villagers and the event player model. */
extern CDataAlloc2<1> EdVillagerBuffer;

/** Working arena beginning at the free end of the villager arena for event resources. */
extern CDataAlloc2<1> EdEventBuffer;

/** Working arena beginning at the free end of the event arena for additional resources. */
extern CDataAlloc2<1> EdEventExBuffer;

/** Working arena used to load map NPC resources. */
extern CDataAlloc2<1> EdNPCBuffer;

/**
 * Describes one image resource assigned while loading an editor map.
 */
struct EDIT_IMAGE_INFO {
    char name[0x40]; /**< Resource path relative to the active script directory. */
    int  type;       /**< Image slot receiving the resource. */
    int  number;     /**< Image number selected within the resource. */
};

/**
 * Stores one scene-layer resource selected by a sky or sun script command.
 */
struct EDIT_SCENE_LAYER_INFO {
    char name[0x40]; /**< Resource path relative to the active script directory. */
    int  unk_40;
    int  unk_44;
    int  unk_48;
    u8   unk_4c[0x4c];
    s16  obj_anime[8]; /**< Object animations the layer plays; zero or below where a slot holds none. */
    u8   unk_a8[0x50];
};

/**
 * Stores one editor-area definition parsed from the map script.
 */
struct EDIT_AREA_INFO {
    char  name[0x40]; /**< Resource path relative to the active script directory. */
    int   width;      /**< Width of the editable area. */
    int   height;     /**< Height of the editable area. */
    float unit_size;  /**< World-space width of one grid cell. */
    float unit_alt;   /**< World-space height of one elevation step. */
    float origin_x;   /**< World-space X of the area's grid origin, before centring on the cells. */
    float origin_y;   /**< World-space height of the area's grid origin. */
    float origin_z;   /**< World-space Z of the area's grid origin, before centring on the cells. */
};

/**
 * Stores one named moving map-part range parsed from the map script.
 */
struct EDIT_MOTION_PARTS_INFO {
    char  name[0x40]; /**< Name of the moving map part. */
    float values[9];  /**< Motion limits and rates supplied by the script. */
};

/**
 * Stores one map-part resource and the classification supplied by its script command.
 */
struct MAP_PARTS_INFO {
    char  name[9][0x40]; /**< Model resource names: seven shape variants then two more. */
    int   parts_no;      /**< Part number supplied by the script. */
    int   subtype;       /**< Road, river, bridge, or surface subtype. @see MapPartsSubtype. */
    int   kind;          /**< Broad ground, building, or terrain classification. */
    float position[3];   /**< World-space placement of the part. */
    float rotation[3];   /**< Orientation of the part, in radians. */
    float lift;          /**< How far the part draws above the ground so it does not cut into it; zero for none. */
    s16   anime[8];      /**< Object animations attached to this part. */
    u8    unk_278[0x50];
    s16   events[8]; /**< Event points attached to this part. */
};

/**
 * Stores the cell grid and attached resources of one script-defined map part.
 */
struct EDIT_PARTS_DEF {
    int  width;                  /**< Cells the part covers from west to east. */
    int  height;                 /**< Cells the part covers from north to south. */
    s16  cells[32];              /**< One entry per grid cell, row by row. */
    int  kind;                   /**< Parts classification copied into the packed part header. */
    int  element_ids[6];         /**< Identifier of each optional visual element. */
    char element_names[6][0x20]; /**< Name of each optional visual element. */
};

/**
 * Provides the overlapping edit-area and map-part views of script work storage.
 */
union EDIT_MAP_PARTS_WORK {
    EDIT_AREA_INFO edit_areas[444]; /**< Editable-area resources parsed for this map. */

    struct {
        u8             unk_0000[0x170];
        MAP_PARTS_INFO parts[24]; /**< General map-part definitions parsed for this map. */
    } general;

    struct {
        u8             unk_0000[0x45b0];
        EDIT_PARTS_DEF defs[24]; /**< Cell grids defined for the general map parts. */
    } parts_defs;

    struct {
        u8             unk_0000[0x6110];
        MAP_PARTS_INFO parts[6]; /**< Road definitions parsed for this map. */
    } roads;

    struct {
        u8             unk_0000[0x7220];
        MAP_PARTS_INFO parts[16]; /**< River and bridge definitions parsed for this map. */
    } rivers;

    u8 unk_0000[0x9fa0];
};

/**
 * Stores the ordered bounds of one rectangular editor region.
 */
struct EDIT_AREA_RECT_INFO {
    sceVu0FVECTOR maximum; /**< Greater corner of the region. */
    sceVu0FVECTOR minimum; /**< Lesser corner of the region. */
};

/**
 * Stores the four scalar parameters for one editor water wave.
 */
struct EDIT_WATER_WAVE_INFO {
    float row;    /**< Grid row the ripple starts at; below zero picks one at random. */
    float column; /**< Grid column the ripple starts at; below zero picks one at random. */
    float range;  /**< Largest random height added to each push. */
    float power;  /**< Height of each push; an entry with no power and no range ends the list. */
};

/**
 * Stores transient wave offsets for editor water surfaces.
 */
struct EDIT_WATER_INFO {
    char                 name[0x10];   /**< Frame of the owning part that must be drawn for the surface to draw, or empty. */
    int                  grid_rows;    /**< Rows in the surface's ripple grid; zero or below ends the list. */
    int                  grid_columns; /**< Columns in the surface's ripple grid. */
    int                  parts_no;     /**< Map part the surface belongs to. */
    u8                   unk_1c[0x4];
    sceVu0FVECTOR        corner_a; /**< Near-left corner of the surface, which also gives its height. */
    sceVu0FVECTOR        corner_b; /**< Far-right corner of the surface; only X and Z are read. */
    sceVu0FVECTOR        corner_c; /**< Where the surface's frame stands, relative to its part or in the world. */
    int                  red;      /**< Red component of the surface colour. */
    int                  green;    /**< Green component of the surface colour. */
    int                  blue;     /**< Blue component of the surface colour. */
    u8                   unk_5c[0x4];
    sceVu0FVECTOR        ripple_params; /**< Ripple speed, damping, height scale and distortion. */
    int                  follow[3];     /**< Whether the surface keeps ahead of the camera along X, level with it, and ahead along Z. */
    u8                   unk_7c[0x4];
    EDIT_WATER_WAVE_INFO wave[4]; /**< Pending water-wave parameters, terminated by an empty entry. */
};

/**
 * Provides the overlapping view used to address one water-wave slot.
 */
struct EDIT_WATER_WAVE_VIEW {
    u8    unk_00[0x80];
    float row;    /**< Grid row the ripple starts at; below zero picks one at random. */
    float column; /**< Grid column the ripple starts at; below zero picks one at random. */
    float range;  /**< Largest random height added to each push. */
    float power;  /**< Height of each push; an entry with no power and no range ends the list. */
};

/**
 * Stores the editor's runtime state for one town villager.
 */
struct VILLAGER_INFO {
    char          name[0x40];         /**< Villager resource name. */
    int           placed;             /**< Whether runtime placement has assigned this villager to a slot. */
    int           index;              /**< Position of the villager in the parsed list. */
    int           character_no;       /**< Character definition selected for the villager. */
    int           model_no;           /**< Model variant selected for the villager. */
    int           weapon_no;          /**< Equipment or prop selected for the villager. */
    int           initial_motion;     /**< Initial motion selected for the villager. */
    float         move_speed;         /**< Speed used when the villager walks to a selected destination. */
    int           hide_when_complete; /**< Whether completing the associated part suppresses this villager. */
    int           talk_event_no;      /**< Event number started when the villager is addressed. */
    int           talk_event_level;   /**< Priority or variant associated with the talk event. */
    int           talk_rotation;      /**< Rotation constraint applied while the villager talks. */
    int           talk_direction;     /**< Direction constraint applied while the villager talks. */
    sceVu0FVECTOR position;           /**< Initial villager position and scale component. */
    sceVu0FVECTOR rotation;           /**< Initial villager rotation. */
};

/** Runtime metadata for the ten villagers selected for the current period. */
extern VILLAGER_INFO EdVillagerInfo[10];

/**
 * Stores the fog parameters belonging to one lighting preset.
 */
struct EDIT_FOG_INFO {
    float near_distance; /**< Distance at which fog begins. */
    float far_distance;  /**< Distance at which fog reaches full strength. */
    u8    red;           /**< Red component of the fog colour. */
    u8    green;         /**< Green component of the fog colour. */
    u8    blue;          /**< Blue component of the fog colour. */
    u8    unk_0b;
    float intensity; /**< Fog-density parameter supplied by the script. */
    float exponent;  /**< Fog falloff parameter supplied by the script. */
};

/**
 * Describes one spatial or scripted event trigger in an editor map.
 */
struct ED_EVENT_POINT {
    int           enabled;            /**< Whether this event-point entry is available for evaluation. */
    int           map_no;             /**< Map the event moves the player to. */
    int           completion_flag;    /**< Map flag which suppresses an event after completion. */
    int           parts_no;           /**< Ground-parts object associated with the event. */
    int           event_type;         /**< Kind of event represented by the entry. @see EdEventPointType. */
    CMapObject   *map_object;         /**< Optional map object associated with the event. */
    CFrame       *frame;              /**< Optional frame whose draw state gates the event. */
    int           side;               /**< Which side of the entrance the player arrives on. */
    int           linked_value;       /**< Parameter supplied by a paired or directional marker. */
    int           minimum_progress;   /**< Minimum story-progress value required by this event. */
    int           secondary_progress; /**< Additional progress parameter used by scripted event points. */
    u8            unk_2c[0x4];
    char          destination[0x10]; /**< Name of the map the event leads to. */
    float         start_time;        /**< Beginning of the event's active time interval. */
    float         end_time;          /**< End of the event's active time interval. */
    u8            unk_48[0x8];
    sceVu0FVECTOR position;      /**< Primary world-space event position. */
    sceVu0FVECTOR trigger_range; /**< Distance within which the player triggers the event; the fourth component is a flag passed to the event. */
    sceVu0FVECTOR rotation;      /**< World-space orientation associated with the event. */
    sceVu0FVECTOR extent;        /**< Secondary point or spatial extent of the event. */
};

/**
 * Provides the overlapping work-record views used by editor map loading.
 */
union EDIT_MAP_WORK_INFO {
    OBJ_ANIME_SEQ obj_anime[128]; /**< Object animations parsed for this map. */

    struct {
        u8               unk_0000[0x4800];
        EDIT_EFFECT_INFO first[64];  /**< Effects placed on the general map parts. */
        EDIT_EFFECT_INFO second[64]; /**< Effects placed on the map objects. */
    } effects;

    struct {
        u8                unk_0000[0x8000];
        EDIT_OBJECT_TIMER timers[128]; /**< Object timers parsed for this map. */
    } object_timers;

    struct {
        u8             unk_0000[0x9000];
        ED_EVENT_POINT points[256]; /**< Event points parsed for this map. */
    } events;
};

/**
 * Exposes the message-speed word used from the saved configuration block.
 */
struct EDIT_CONFIG_VIEW {
    u8  unk_00[0x8];
    int clock_hidden; /**< Whether the player has turned the editor clock off. */
    int unk_0C;
    int message_speed; /**< Whether editor messages use the faster reveal rate. */
};

/**
 * One of the georama's element slots; the interior test map fills all of them.
 */
struct EDIT_ELEMENT_INFO {
    int unk_00;
    int element_no; /**< Element the slot holds. */
};

/**
 * Stores the parsed resources and environmental settings for an editor map.
 */
struct EDIT_MAP_INFO {
    char                   scene_name[0x40];           /**< Scene resource named by the map script. */
    int                    time_stop;                  /**< Whether the map clock is stopped. */
    int                    ambient_sound_off;          /**< Whether ambient map sound is disabled. */
    int                    bgm_no;                     /**< Background-music number selected by the script. */
    int                    sound_set_no;               /**< Environmental sound-set number. */
    int                    reverb_mode[2];             /**< Reverb presets selected for the two sound channels. */
    int                    reverb_depth[2];            /**< Reverb strengths selected for the two sound channels. */
    float                  wind[4];                    /**< Wind direction and strength parameters. */
    float                  ambient[12][4];             /**< Ambient light colours for each light preset. */
    float                  light_direction[12][4][4];  /**< Normalized light directions per preset, one light per column. */
    float                  light_colour[12][4][4];     /**< Colours for four lights per preset. */
    float                  background_colour[12][4];   /**< Primary background colours for each light preset. */
    float                  background_colour_2[12][4]; /**< Secondary background colours for each light preset. */
    EDIT_FOG_INFO          fog[12];                    /**< Fog parameters for each light preset. */
    float                  character_ambient[4][4];    /**< Per-character ambient light colours. */
    EDIT_IMAGE_INFO        images[64];                 /**< Image resources assigned to map rendering slots. */
    int                    sky_follow[3];              /**< Sky-follow parameters supplied by the map script. */
    u8                     unk_1bec[0x4];
    EDIT_SCENE_LAYER_INFO  sky_layers[5]; /**< Scene layers selected by the SKY command. */
    EDIT_SCENE_LAYER_INFO  sun_layers[4]; /**< Scene layers selected by the SUN command. */
    u8                     unk_24a8[0xf8];
    MAP_PARTS_INFO         map_objects[64];        /**< Ground, building and water objects placed by the script. */
    EDIT_MAP_PARTS_WORK    parts_work;             /**< Overlapping area and part-definition work storage. */
    EDIT_MOTION_PARTS_INFO motion_parts[4];        /**< Named map parts with scripted motion. */
    EDIT_WATER_INFO        water_surfaces[8];      /**< Water surfaces parsed for this map. */
    EDIT_AREA_RECT_INFO    edit_area_rects[8];     /**< Rectangular editor regions parsed for this map. */
    float                  dof_start_time;         /**< Time at which the depth-of-field interval begins. */
    float                  dof_end_time;           /**< Time at which the depth-of-field interval ends. */
    float                  dof_near;               /**< Near depth-of-field distance. */
    float                  dof_far;                /**< Far depth-of-field distance. */
    int                    dof_near_level;         /**< Near depth-of-field blur level. */
    int                    dof_far_level;          /**< Far depth-of-field blur level. */
    int                    dof_alpha;              /**< Alpha used while drawing depth of field. */
    float                  shadow_near;            /**< Near distance used by map shadow rendering. */
    float                  shadow_far;             /**< Far distance used by map shadow rendering. */
    int                    shadow_level;           /**< Shadow-quality level selected by the map script. */
    int                    shadow_mode;            /**< First map shadow-mode setting. */
    int                    shadow_mode_2;          /**< Second map shadow-mode setting. */
    VILLAGER_INFO          villagers[16];          /**< Villager placements parsed for this map. */
    int                    time_tables[7][12][16]; /**< Weekly villager schedules parsed for this map. */
    int                    people_list[16];        /**< Villager identifiers parsed for this map. */
    int                    obj_anime_count;        /**< Number of object-animation records in use. */
    int                    object_effect_count;    /**< Number of map-object effect records in use. */
    int                    parts_effect_count;     /**< Number of general map-part effect records in use. */
    int                    object_timer_count;     /**< Number of object-timer records in use. */
    int                    event_count;            /**< Number of event points the map defines. */
    u8                     unk_1a254[0xc];
    EDIT_MAP_WORK_INFO     work; /**< Overlapping work records reset during map initialization. */
};

/** Current parsed editor-map description. */
extern EDIT_MAP_INFO *EditMapInfo;

/** The same description as the map script is being read into it. */
extern EDIT_MAP_INFO *edit_info;

/** Villager currently receiving map-script attributes. */
extern VILLAGER_INFO *now_villinfo;

/** Storage arena containing editor event scripts. */
extern CDataAlloc2<1> EdScriptBuffer;

/** Name of the editor map whose resources are active. */
extern char EditMapName[0x20];

/** Directory the editor map's data files are read from. */
extern char EditDataDir[0x100];

/** Name of the interior the player is entering. */
extern char EdInteriorName[0x20];

/** Scene archive the map's models are read out of. */
extern u_int *scn_data;

/** Buffer the map's part archive is read into. */
extern u_int *parts_read_buffer;

/** Optional map-specific event script loaded into the script arena. */
extern char *EdEventData;

/** Shared system event script loaded into the script arena. */
extern char *EdSystemEventData;

/** Whether map geometry drawing is suppressed while an editor event is active. */
extern int EdDrawOffMap;

/** Whether the editor clock overlay is drawn. */
extern int draw_clock;

/** Suppresses the editor's parameter and system overlays for debug captures. */
extern int EdDebugParamDrawOff;

/** Current projected position of the editor's map cursor. */
extern sceVu0FVECTOR NowCursorPos;

/** Target position toward which the editor's map cursor interpolates. */
extern sceVu0FVECTOR NextCursorPos;

/** Map part currently under the editor cursor. */
extern CMapParts *NowFocusParts;

/** Number of frames for which the focused part name remains visible. */
extern int DrawPartsNameCount;

/** Number of labels available for the focused map part. */
extern int PartsNameNum;

/** Primary message window used by ordinary editor conversations. */
extern ClsMes EditMes1;

/** Primary message window used while an editor event is running. */
extern ClsMes EditEventMes1;

/** Message window used for editor character names. */
extern ClsMes EditNameMes;

/** Message window used for editor system notifications. */
extern ClsMes EditSystemMes;

/** Texture shown while the editor waits on a load. */
extern CTexture *StayTexture;

/** Buffer the map's villagers are read into, at the free end of the NPC arena. */
extern u_long128 *EdNPCReadBuffer;

/** Whether the editor clock advances each frame. */
extern int EdStepTimeFlag;

/** Map part number of the interior the player is walking into. */
extern int EdInteriorPartsNo;

/** Map jump the interior's door leads to. */
extern int EdInteriorJumpID;

/** Sound the interior's door plays as it opens. */
extern int EdInteriorDoorSound;

/** Event started once the player is inside the interior. */
extern int EdInteriorStartEvent;

/** Effect group every editor effect is played through. */
extern CEffectGroup EdEffectGroup;

/** Whether everything the editor draws is switched off. */
extern int EdDrawOffFlag;

/** Whether the map's shadows are left undrawn. */
extern int EdDrawOffMapShadow;

/** Whether the editor is paused. */
extern int EdPauseFlag;

/** Whether the next frame starts from a cleared screen. */
extern int clear_screen;

/** Whether the pad is ignored until every button has been released. */
extern int key_lock;

/** Whether the loop is leaving for the dungeon. */
extern int goto_dungeon;

/** Whether the editor loop has been asked to end. */
extern int exit_loop;

/** Frames left before the editor's sound is switched off. */
extern int sound_off_cnt;

/** Page the debug menu is showing. */
extern int debug_menu_mode;

/** Event a fade-out is leading into. */
extern int goto_cmp_event;

/** Priority of the event a fade-out is leading into. */
extern int goto_cmp_event_level;

/** Whether an event has asked for the time of day to change. */
extern int change_time_event;

/** Whether the cursor over a villager who can be talked to is drawn. */
extern int draw_npc_cursor;

/** Whether the map is drawn in edit mode. */
extern int edit_mode_draw;

/** Whether the ground grid is drawn in edit mode. */
extern int edit_mode_grd_draw;

/** Whether depth of field is applied to the map. */
extern int depth_of_field;

/** Lighting preset edit mode draws with. */
extern int edit_mode_lighting;

/** Whether the sky is drawn. */
extern int draw_sky;

/** Frames the player has been walking. */
extern int move_count;

/** Whether the editor is running the interior test map. */
extern int interior_test;

/** Names of the interiors the debug menu offers. */
extern char **interior_name;

/** Game mode the editor returns to once the debug menu closes. */
extern int oldGameMode;

/** Ground the player builds the town on. */
extern CEditGround *pEditGround;

/** Areas the map is divided into. */
extern CEditArea *EditArea;

/** Map object parts. */
extern CMapParts *ObjParts;

/** River parts. */
extern CMapParts *RiverParts;

/** Road parts. */
extern CMapParts *RoadParts;

/** Sky backdrop model. */
extern CFrameVu1 *SkyBackFrame;

/** Cursor drawn over a villager who can be talked to. */
extern CFrame *CharaCursor0;

/** Cursor drawn over a villager who cannot be talked to. */
extern CFrame *CharaCursor1;

/** Cursor drawn over the character an event wants noticed. */
extern CFrame *CharaCursor2;

/** Closed treasure-box marker model. */
extern CFrameVu1 *TreasureCursor;

/** Open treasure-box marker model. */
extern CFrameVu1 *TreasureCursorOpen;

/** Frames the loop has been ending for. */
extern int end_counter;

/** Background music volume while it fades. */
extern int bgm_vol;

/** Whether the map's background music has been asked for. */
extern int bgm_play_flag;

/** Whether the map's background music has started. */
extern int bgm_play_start;

/** Whether a door is playing its opening motion. */
extern int door_open;

/** Whether the player is held at a fixed position while a door opens. */
extern int fix_pos_enble;

/** Villager the player is talking to. */
extern int talk_villager;

/** Frames left in the time-of-day change. */
extern int chg_time_cnt;

/** Rotation the selected part is placed at. */
extern int NowSelectAngle;

/** Rotation the selected part was placed at before the last change. */
extern int OldSelectAngle;

/** Rotation of the part cursor about the vertical axis. */
extern float NowCursorRotY;

/** Map part that was under the cursor on the previous frame. */
extern CMapParts *OldFocusParts;

/** Time left on the day-transition overlay. */
extern float draw_day_cnt;

/** Whether the day-transition overlay is drawn. */
extern int draw_day_flag;

/** Part selected in the edit menu. */
extern int NowSelectParts;

/** Model of the edit-mode cursor. */
extern CFrame *ECursorFrame;

/** Element each of the map's part slots holds. */
extern EDIT_ELEMENT_INFO EditElementInfo[36];

/** Arena interior data is read into. */
extern CDataAlloc2<1> EdInteriorBuffer;

/** Arena the map's messages are read into. */
extern CDataAlloc2<1> EdMesBuffer;

/** Arena the map's extra messages are read into. */
extern CDataAlloc2<1> EdExtMessBuffer;

/** Working arena carved out of what the villager arena leaves. */
extern CDataAlloc2<1> EdWorkBuffer;

/** Arena the editor's menus are built in. */
extern CDataAlloc2<1> EdMenuBuffer;

/** Wind the editor's cloth is blown by. */
extern CWind EdWind;

/** Secondary message window used by ordinary editor conversations. */
extern ClsMes EditMes2;

/** Secondary message window used while an editor event is running. */
extern ClsMes EditEventMes2;

/** Message window used for editor help text. */
extern ClsMes EditHelpMes;

/** Fog the map is drawn with. */
extern EDIT_FOG_INFO now_fog;

/** Arena the map's part information is read into. */
extern CDataAlloc2<1> EPartsInfoBuff;

/** Character the player leads through the town. */
extern CMainChara MainChara;

/** Dust the player's run leaves behind. */
extern CRunEffect RunEffect;

/** Camera used in edit mode. */
extern CCameraFollow EditCamera;

/** Camera used as the map is entered. */
extern CCameraFollow IntroCamera;

/** Sky models, one per time of day. */
extern CFrameVu1 *SkyFrame[4];

/** Sun models, one per time of day. */
extern CFrame *SunFrame[4];

/** Where the camera is held while a door plays its motion. */
extern sceVu0FVECTOR fix_camera_pos;

/** Map file of the interior being entered. */
extern char interior_map_name[0x40];

/** Position the part cursor is drawn at. */
extern float NowPartsCursorPos[4];

/** Arena the player character is read into. */
extern CDataAlloc2<1> CharaBuffer;

/** Arena interior parts are read into. */
extern CDataAlloc2<1> InteriorBuffer;

/** Arena the editor's miscellaneous data is carved out of. */
extern CDataAlloc2<1> EtcDataBuffer;

STATIC_ASSERT(sizeof(EDIT_IMAGE_INFO) == 0x48);
STATIC_ASSERT(sizeof(EDIT_SCENE_LAYER_INFO) == 0xf8);
STATIC_ASSERT(sizeof(EDIT_AREA_INFO) == 0x5c);
STATIC_ASSERT(sizeof(EDIT_MOTION_PARTS_INFO) == 0x64);
STATIC_ASSERT(sizeof(EDIT_AREA_RECT_INFO) == 0x20);
STATIC_ASSERT(sizeof(EDIT_WATER_WAVE_INFO) == 0x10);
STATIC_ASSERT(sizeof(EDIT_WATER_INFO) == 0xc0);
STATIC_ASSERT(sizeof(MAP_PARTS_INFO) == 0x2d8);
STATIC_ASSERT(sizeof(EDIT_MAP_PARTS_WORK) == 0x9fa0);
STATIC_ASSERT(sizeof(EDIT_PARTS_DEF) == 0x124);
STATIC_ASSERT(sizeof(EDIT_MAP_WORK_INFO) == 0x12000);
STATIC_ASSERT(sizeof(EDIT_MAP_INFO) == 0x2C260);
STATIC_ASSERT(sizeof(ED_EVENT_POINT) == 0x90);
STATIC_ASSERT(sizeof(VILLAGER_INFO) == 0x90);
STATIC_ASSERT(sizeof(EDIT_ELEMENT_INFO) == 0x8);

/** Whether the current editor map is an interior. */
extern int EdInteriorFlag;

/** Whether the editor's thunder effect is active. */
extern int EdThunderEffectFlag;

/** Background music remembered before entering an interior. */
extern int EdBeforeInBgmNo;

/** Georama map currently being edited. */
extern int NowEditMap;

/** Returns a map-completion flag from the active Georama map. */
int EdGetMapFlag(int flag_no);

/** Sets a map-completion flag on the active Georama map. */
int EdSetMapFlag(int flag_no, int value);

/**
 * Applies the editor-specific layout and reveal settings to its message windows.
 *
 * @mangled EdInitMesParam__Fv
 * @address 0x1781D0
 * @size 0x1B0
 */
void EdInitMesParam();

/**
 * Enables or disables the cursor drawn over the controlled character.
 *
 * @mangled EdSetCharaCursor__Fi
 * @address 0x17CBE0
 * @size 0xC
 */
void EdSetCharaCursor(int on);

/**
 * Loads the player model and motion archive used by an editor event.
 *
 * @mangled EdLoadMainChara__FPcPcP14CDataAlloc2_1_
 * @address 0x181480
 * @size 0x154
 */
void EdLoadMainChara(char *pack_path, char *info_name, CDataAlloc2<1> *arena);

/**
 * Copies the current editor resource directory into a caller buffer.
 *
 * @mangled GetEditDataDir__FPc
 * @address 0x1777D0
 * @size 0x28
 */
void GetEditDataDir(char *name);

/**
 * Resolves a map object's local position and yaw into world coordinates.
 *
 * @mangled GetPosRot__FP10CMapObjectPfPf
 * @address 0x1838F0
 * @size 0x118
 */
int GetPosRot(CMapObject *object, float *position, float *rotation);

/**
 * Tests whether an event point is enabled and valid at the requested map time.
 *
 * @mangled CheckEventPoint__FP14ED_EVENT_POINTf
 * @address 0x183A60
 * @size 0x1D4
 */
int CheckEventPoint(ED_EVENT_POINT *point, float time);

/**
 * Returns the first unused event-point slot after the reserved first entry.
 *
 * @mangled GetNewEventPoint__FP14ED_EVENT_POINTi
 * @address 0x183A10
 * @size 0x4C
 */
ED_EVENT_POINT *GetNewEventPoint(ED_EVENT_POINT *points, int count);

/**
 * Writes the editor's part progress and clock back to the save data.
 *
 * @mangled EditSave__Fv
 * @address 0x177BF0
 * @size 0x84
 */
void EditSave();

/**
 * Loads the georama editor's state back from the save data.
 *
 * @mangled EditLoad__Fv
 * @address 0x177C80
 * @size 0xBC
 */
void EditLoad();

/**
 * Draws one textured rectangle rotated about a caller-supplied pivot.
 *
 * @mangled set2DSpriteRot__FP13sceVif1PacketP8CTextureRC8CRect_i_RC8CRect_i_iifUc
 * @address 0x1831D0
 * @size 0x590
 */
void set2DSpriteRot(sceVif1Packet *packet, CTexture *texture, const CRect_i_ &screen, const CRect_i_ &texel, int pivot_x, int pivot_y, float angle, unsigned char alpha);

/**
 * Draws the editor's system overlays for the current frame.
 *
 * @mangled DrawSysGra__Fv
 * @address 0x17D9F0
 * @size 0x16C
 */
void DrawSysGra();

/**
 * Draws the cursor associated with one collection of event points.
 *
 * @mangled EdDrawSysCursor__FP14ED_EVENT_POINTi
 * @address 0x17CBF0
 * @size 0x418
 */
void EdDrawSysCursor(ED_EVENT_POINT *points, int count);

/**
 * Draws the current day and its transition overlay.
 *
 * @mangled DrawDay__Fv
 * @address 0x17D040
 * @size 0x9A8
 */
void DrawDay();

/**
 * Draws the editor cursor and focused-part label overlay.
 *
 * @mangled ParamDraw__Fv
 * @address 0x17C9B0
 * @size 0x224
 */
void ParamDraw();

/**
 * Draws the town clock at the requested screen offset.
 *
 * @mangled EdDrawClock__Fii
 * @address 0x17DB60
 * @size 0x1AC
 */
void EdDrawClock(int x, int y);

/**
 * Requests that the main editor loop terminate.
 *
 * @mangled EdExitLoop__Fv
 * @address 0x178110
 * @size 0x10
 */
int EdExitLoop();

/**
 * Requests a transition from the editor into a dungeon.
 *
 * @mangled GotoDungeon__Fv
 * @address 0x17ECE0
 * @size 0x10
 */
int GotoDungeon();

/**
 * Provides the empty map-object loading hook used by this editor loop.
 *
 * @mangled LoadMapObject__FP9CMapPartsPPc
 * @address 0x1831C0
 * @size 0x8
 */
void LoadMapObject(CMapParts *map_parts, char **script);

/**
 * Sets the town clock to the requested time of day.
 *
 * @mangled EdSetClock__Ff
 * @address 0x178170
 * @size 0x60
 */
void EdSetClock(float time);

/**
 * Returns the town clock's current time of day.
 *
 * @mangled EdGetClock__Fv
 * @address 0x178140
 * @size 0x24
 */
float EdGetClock();

/**
 * Starts the editor's day-transition display.
 *
 * @mangled EdStartDrawDay__Fv
 * @address 0x17D010
 * @size 0x18
 */
void EdStartDrawDay();

/**
 * Resets the editor's day-transition display.
 *
 * @mangled EdInitDrawDay__Fv
 * @address 0x17D030
 * @size 0x10
 */
void EdInitDrawDay();

/**
 * Removes the event-specific robot-part models.
 *
 * @mangled EdDeleteE05RoboParts__Fv
 * @address 0x17ECF0
 * @size 0xE8
 */
void EdDeleteE05RoboParts();

/** Data whose shape the unit does not need yet. */
extern CTexAnimeData CharaTexAnimeData[];

/** Sprites the system events draw with. */
extern C3DSprite SystemEffect[];

/**
 * Frames the editor loop has run since the map was entered.
 */
extern int loop_counter;

/**
 * Frames the pad has been held for.
 */
extern int key_counter;

/**
 * Whether the loop is leaving for the main menu.
 */
extern int goto_menu;

/**
 * Whether the loop is returning from the main menu.
 */
extern int goto_return_menu;

/**
 * How far the following camera sits from the player.
 */
extern int camera_dist_mode;

/**
 * Animated map parts.
 */
extern CCharacter *MotionParts;

/**
 * Camera the map is drawn through.
 */
extern CCamera *NowCamera;

/**
 * Time of day on the editor clock.
 */
extern float NowTime;

/**
 * Frames the opening door has been playing for.
 */
extern int door_open_cnt;

/**
 * Font the editor's debug text is drawn with.
 */
extern CDebugFont DebugFont__3;

/**
 * Camera that follows the player.
 */
extern CCameraFollow MainCamera;

/**
 * Camera used while the player talks to a villager.
 */
extern CCameraFollow TalkCamera;

/**
 * Camera used by the free-look view.
 */
extern CCameraFollow ViewCamera;

/**
 * Where the player is held while a door plays its motion.
 */
extern sceVu0FVECTOR fix_chara_pos;

/**
 * Which way the player faces while a door plays its motion.
 */
extern sceVu0FVECTOR fix_chara_rot;

/**
 * Arena the map's data is read into before the editor carves it up.
 */
extern CDataAlloc2<1> DataBuffer__2;

/**
 * Pool the editor's effect group plays its effects out of.
 */
extern CEffect *EffectTable__3;

/**
 * Character the camera and events treat as the player.
 */
extern CCharacter *Chara;

/**
 * Records of the map's texture animation.
 */
extern CTexAnimeData TexAnimeData[];
