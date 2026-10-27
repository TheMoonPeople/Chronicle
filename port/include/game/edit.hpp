#pragma once

#include "common.h"

#include <libvu0.h>

/**
 * @file
 * Declares the Georama editor's shared state, sound, menu and message helpers.
 */

namespace dc::game {

/**
 * What a town event asks for when it ends, as ED_EVENT_INFO::return_code holds it.
 */
// clang-format off
enum EdEventReturn {
    ED_EVENT_RETURN_NONE     = 0,  /**< Nothing. */
    ED_EVENT_RETURN_INTERIOR = 4,  /**< Go into an interior. */
    ED_EVENT_RETURN_DUNGEON  = 5,  /**< Go to the dungeon. */
    ED_EVENT_RETURN_CONTINUE = 6,  /**< Carry on. */
    ED_EVENT_RETURN_OUTSIDE  = 7,  /**< Leave the interior. */
    ED_EVENT_RETURN_MAP_JUMP = 8,  /**< Jump to another map. */
    ED_EVENT_RETURN_TALK     = 9,  /**< Return to talking. */
    ED_EVENT_RETURN_EXIT     = 10, /**< Leave the town loop. */
    ED_EVENT_RETURN_FISHING  = 11, /**< Start fishing. */
};

// clang-format on

/**
 * Menus a town script can open, as EdInitMenu takes them.
 */
// clang-format off
enum EdMenuKind {
    ED_MENU_RESET            = -1, /**< Clears the menu request without opening a menu. */
    ED_MENU_NONE             = 0,  /**< No menu. */
    ED_MENU_EDIT             = 1,  /**< Edit menu. */
    ED_MENU_BATTLE           = 2,  /**< Battle menu. */
    ED_MENU_SHOP             = 3,  /**< Shop. */
    ED_MENU_UNK_4            = 4,  /**< Opens a shop like ED_MENU_SHOP. */
    ED_MENU_USE_ITEM         = 5,  /**< Item use selection. */
    ED_MENU_NAME_REGISTRY    = 6,  /**< Name entry. */
    ED_MENU_WORLD_MAP        = 7,  /**< World map. */
    ED_MENU_FISHING_EXCHANGE = 8,  /**< Fish point exchange. */
    ED_MENU_CHANGE_ESA       = 9,  /**< Bait selection. */
    ED_MENU_FISH_RANKING     = 10, /**< Fishing record. */
};

// clang-format on

class CCamera;
class CCameraFollow;
class CCharacter;
class CDebugFont;
class CEditGround;
class CFrame;
class CFrameVu1;
class CMainChara;
class CMapObject;
class CMapParts;
class CNPCharacter;
class CTexAnimeData;
class CTexture;
class ClsMes;
struct ED_EVENT_POINT;
struct EDIT_EFFECT_INFO;
struct EDIT_MAP_INFO;
struct EPARTS_FUNC_DATA;
struct EPARTS_INFO_HEADER;
struct INIT_PARTSINFO;
struct OBJ_ANIME_SEQ;
struct VILLAGER_INFO;

/**
 * Describes the event the player is standing in, and where it puts them.
 */
struct ED_EVENT_PARAM {
    int             kind;     /**< What kind of event the point is. @see EdEventPointType. */
    int             parts_no; /**< Map part the event point hangs off, or negative for none; a door enters the interior through it. */
    u8              unk_08[0x8];
    sceVu0FVECTOR   position;   /**< Where the player stands while the door plays. */
    sceVu0FVECTOR   camera_pos; /**< Where the camera stands while the door plays. */
    sceVu0FVECTOR   rotation;   /**< Which way the player faces while it plays. */
    u8              unk_40[0x10];
    ED_EVENT_POINT *point; /**< Event point the parameters were read from. */
    u8              unk_54[0xc];
};

/**
 * What the editor hands to EdMoveChara, and what it gets back.
 */
struct ED_MOVE_CHARA_INFO {
    float           time;        /**< Time of day the step happens at. */
    CCharacter     *chara;       /**< Character being moved. */
    CCameraFollow  *camera;      /**< Camera that follows the character and the movement is relative to. */
    CCamera        *view_camera; /**< Camera that looks from the character's eyes in the first-person view. */
    int             key_lock;    /**< Whether the pad is ignored this step. */
    u8              unk_14[0x4];
    int             interior;    /**< Whether the character walks through an interior's parts rather than the ground. */
    CEditGround    *ground;      /**< Ground the character walks on. */
    CMapParts      *parts;       /**< Parts of the interior the character walks through. */
    int             parts_count; /**< How many of them there are. */
    int             event_ready; /**< Whether the character stands within reach of an event point this step. */
    u8              unk_2c[0x4];
    ED_EVENT_PARAM  param;              /**< The event the character stands in. */
    ED_EVENT_POINT *points;             /**< Event points of the map. */
    int             point_count;        /**< How many of them there are. */
    int             event_no;           /**< Map event the step asks to run. */
    int             system_event_no;    /**< System event the step asks to run. */
    int             acted;              /**< Whether the step acted on the confirm button. */
    int             fishing;            /**< Whether the character stands fishing. */
    float           motion_time_after;  /**< Motion time once the step has advanced the character. */
    float           motion_time_before; /**< Motion time before the step advanced it. */
};

STATIC_ASSERT(sizeof(ED_EVENT_PARAM) == 0x60);
STATIC_ASSERT(sizeof(ED_MOVE_CHARA_INFO) == 0xb0);

/**
 * Stores one script-defined timer that gates a map object.
 */
struct EDIT_OBJECT_TIMER {
    char        name[16];   /**< Frame the timer switches; empty leaves the record unused. */
    CMapObject *object;     /**< Object the frame belongs to, where one owns it. */
    CFrame     *frame;      /**< Root the frame is searched under, where no object does. */
    float       start_time; /**< Hour at which the frame starts being drawn. */
    float       end_time;   /**< Hour at which it stops. */
};

STATIC_ASSERT(sizeof(EDIT_OBJECT_TIMER) == 0x20);

/**
 * Names the sound effects the editor plays for itself.
 */
enum ED_SOUND_ID {
    ED_SOUND_NONE,
    ED_SOUND_COUNT = 15,
};

/**
 * Names what the Georama editor is doing; the dungeon reads a few of its
 * fields on the way in and out.
 */
struct ED_EVENT_INFO {
    s32            event_no;                     /**< Number of the event script being run. */
    s32            world_coord_enable;           /**< Whether the event's stored world coordinate is applied each step. */
    u8             world_coord[0x28];            /**< Stored world coordinate: position at byte 8, rotation at byte 24. */
    s32            map_jump_bgm_stop;            /**< Whether a map jump stops the current background music. */
    s32            map_jump_bgm_play;            /**< Whether a map jump starts the destination's background music. */
    s32            sound_off_count;              /**< Number of sound channels suppressed by the event. */
    float          projection;                   /**< Projection distance used while rendering an event. */
    CCameraFollow *camera;                       /**< Camera controller used by the active event. */
    s32            reset_camera_angle;           /**< Camera-angle reset mode requested by the event. */
    float          reset_camera_yaw;             /**< Yaw offset used by the requested camera reset. */
    CCharacter    *main_character;               /**< Player character controlled by the active event. */
    CTexAnimeData *main_texture_animation;       /**< Texture-animation data restored to the player after an event. */
    s32            main_texture_animation_count; /**< Number of saved player texture-animation records. */
    CNPCharacter  *npcs;                         /**< Contiguous array of event NPC characters. */
    s32            player_collision;             /**< Whether ground collision is applied to the player during the event. */
    s32            player_draw;                  /**< Whether the player model is drawn during the event. */
    s32            player_shadow_draw;           /**< Whether the player's shadow is drawn during the event. */
    s32            player_foot_sound;            /**< Footstep mode applied to the player during the event. */
    s32            player_stop;                  /**< Whether the player's event motion is held. */
    s32            player_texture_block;         /**< Texture block assigned to the event player model. */
    s32            npc_collision[16];            /**< Ground-collision mode assigned to each event NPC. */
    s32            npc_draw[16];                 /**< Whether each event NPC model is drawn. */
    s32            npc_shadow_draw[16];          /**< Whether each event NPC shadow is drawn. */
    s32            npc_foot_sound[16];           /**< Footstep mode assigned to each event NPC. */
    s32            npc_stop[16];                 /**< Whether each event NPC's motion is held. */
    s32            npc_draw_before[16];          /**< Draw-order group assigned to each event NPC. */
    s32            npc_texture_block;            /**< First texture block assigned to the event NPC models. */
    s32            npc_count;                    /**< Number of valid entries in the event NPC array. */
    VILLAGER_INFO *villagers;                    /**< Metadata array for the event villagers. */
    ClsMes        *messages[8];                  /**< Message windows owned by the active event. */
    float          current_time;                 /**< Current editor time used by time-of-day opcodes. */
    u8             unk_224[0xC];
    sceVu0FVECTOR  wind;                  /**< Wind vector applied while the event is active. */
    CEditGround   *edit_ground;           /**< Ground and placed-part database used by the active event. */
    s32            fixed_parts_count;     /**< Number of fixed map parts available to object-handle opcodes. */
    CMapParts     *fixed_parts;           /**< Fixed map parts available to object-handle opcodes. */
    s32            edit_parts_count;      /**< Number of placed map parts available to object-handle opcodes. */
    CMapParts     *edit_parts;            /**< Placed map parts available to object-handle opcodes. */
    s32            interior_parts_count;  /**< Number of interior parts available to object-handle opcodes. */
    CMapParts     *interior_parts;        /**< Interior parts available to object-handle opcodes. */
    CFrameVu1     *item_frame[1];         /**< VU1 frame used for the single event-held item. */
    s32            talk_npc_id;           /**< Identifier of the NPC currently engaged in conversation. */
    s32            talk_messages[16];     /**< Message identifiers assigned to the current conversation sequence. */
    s32            talk_select_message;   /**< Message selected after a conversation choice. */
    s32            talk_select_prompt;    /**< Message containing the current conversation choices. */
    s32            interior_entrance;     /**< Entrance of the interior a GOTO_INTERIOR event walks into. */
    char           interior_name[0x20];   /**< Name of the interior a GOTO_INTERIOR event walks into. */
    s32            interior_start_event;  /**< Event run on arrival in that interior, or -1 for none. */
    s32            outside_map_no;        /**< Map requested when an event exits to the outside. */
    s32            draw_exclamation_mark; /**< Whether the event requests the attention marker. */
    s32            suppress_background;   /**< Whether event rendering suppresses the scene background. */
    s32            suppress_shadows;      /**< Whether event rendering suppresses character shadows. */
    u8             unk_2e4[0xC];
    float          background_color[3]; /**< RGB override for the event background. */
    u8             unk_2fc[4];
    s32            screen_filter; /**< Whether a full-screen event colour filter is active. */
    u8             unk_304[0xC];
    sceVu0FVECTOR  screen_filter_color; /**< RGBA colour of the event screen filter. */
    s32            lighting_override;   /**< Whether script-provided scene lighting replaces the map lighting. */
    u8             unk_324[0xC];
    sceVu0FMATRIX  light_direction;      /**< Direction vectors for the four event point lights. */
    sceVu0FMATRIX  light_color;          /**< RGB intensities for the four event point lights. */
    sceVu0FVECTOR  ambient_color;        /**< RGBA ambient-light colour for the event scene. */
    sceVu0FVECTOR  vector_arguments[4];  /**< Vector arguments supplied to the active event. */
    s32            integer_arguments[8]; /**< Integer arguments supplied to the active event. */
    s32            flag_arguments[8];    /**< Flag-like arguments supplied to the active event. */
    s32            fadeout_event_no;     /**< Event started after the current fade-out completes. */
    s32            next_event;           /**< Event number requested by the NEXT_EVENT opcode. */
    s32            return_code;          /**< Result code passed from an editor event back to the game loop. @see EdEventReturn. */
    s32            exit_code;            /**< Exit status supplied by the event script. */
};

STATIC_ASSERT(sizeof(ED_EVENT_INFO) == 0x450);

/**
 * What the Georama editor is doing now.
 */
extern "C" ED_EVENT_INFO EdEventInfo;

/**
 * Whether a map's start events run when the editor enters it.
 */
extern int EdDebugEventEnable;

/**
 * Whether the editor camera is left to the debug controls.
 */
extern int EdDebugCameraFlag;

/**
 * Disables the editor's parameter overlay.
 */
extern int EdDebugParamDrawOff;

/**
 * Disables character drawing in the editor debug view.
 */
extern int EdDebugCharaDrawOff;

/**
 * Whether the editor character is left to the debug controls.
 */
extern int EdDebugMoveFlag;

/**
 * Event the debug menu asks the editor to run this frame, or -1. PAL only.
 */
extern int EdDebugRunEventNo;

/**
 * Steps the editor's event system, and says what the event asks the loop to do.
 */
int EdEventMode(CCameraFollow *camera, int kind);

/**
 * Puts the camera back on the parameters the editor uses outside events.
 */
void EdInitCameraParam(CCameraFollow *camera);

/**
 * Starts the editor's ambient sound set for the time of day.
 */
void EdAmbientPlay(float time);

/**
 * Sets the volume of the active editor ambient sound.
 */
void EdSetAmbientVol(float volume);

/**
 * Writes the root directory used by editor map resources.
 */
void GetEditDataDir(char *name);

/**
 * Describes the time range and rendering parameters of depth of field.
 */
struct DEPTH_OF_FIELD_INFO {
    float start;       /**< Start time on the game's twelve-unit clock. */
    float end;         /**< End time on the game's twelve-unit clock. */
    float distance[2]; /**< Near and far focus distances. */
    int   level;       /**< Strength preset selected for the effect. */
    int   alpha;       /**< Blend alpha of the effect. */
    int   blur;        /**< Blur-kernel setting of the effect. */
};

/**
 * Enables or disables the editor debug controls.
 */
void EdDDebug(int on);

/**
 * Selects the font used by the editor debug overlay.
 */
void EdDSetFont(CDebugFont *font);

/**
 * Draws text queued by the editor debug overlay.
 */
void EdDDrawFont();

/**
 * Updates the editor debug overlay and free-movement controls.
 */
void EdDCheck();

/**
 * Writes the editor debug text buffer to its output file.
 */
void EdOutPutFile();

/**
 * Queues the player character's position, rotation and ground attributes in the editor debug overlay. PAL only.
 */
void EdDPrintChara(CMainChara *chara);

/**
 * Queues the camera's position, reference point and projection in the editor debug overlay. PAL only.
 */
void EdDPrintCamera(CCamera *camera);

/**
 * Queues a labelled vector in the editor debug overlay.
 */
void EdDPrintVector(char *name, float *vector);

/**
 * Queues a line of text in the editor debug overlay.
 */
void EdDPrint(char *text);

/**
 * Moves the editor camera from debug-pad input.
 */
void EdDMoveCamera(float *position, float *reference);

/**
 * Moves the editor camera reference point from debug-pad input.
 */
void EdDMoveCameraRef(float *position, float *reference);

/**
 * Moves an editor character using the debug camera controls.
 */
void EdDMoveChara(CCharacter *character, CCamera *camera);

/**
 * Runs the editor debug menu page the shoulder buttons select and draws the debug overlay. PAL only.
 */
void EdDebugMenu();

/**
 * Runs the main debug menu page of camera, drawing, movement, event and language switches. PAL only.
 */
void DM_Main();

/**
 * Runs the sound debug menu page of music, sound effect and sound set tests. PAL only.
 */
void DM_Sound();

/**
 * Runs the flag debug menu page of game, map, completion and talk flags and dungeon progress. PAL only.
 */
void DM_Flag();

/**
 * Resets the positional sound-source slots.
 */
void EdInitSoundSrc();

/**
 * Starts the positional sound sources prepared for the current frame.
 */
void EdPlaySoundSrc();

/**
 * Stops every active positional sound source.
 */
void EdStopSoundSrc();

/**
 * Updates positional sound volume and panning for visible map parts.
 */
void EdSetSoundSrcVol(float time, CMapParts **parts, int count, float *camera_pos, float *camera_dir);

/**
 * Plays the sound one kind of door makes when it opens.
 */
void EdDoorOpenSe(int door_no, float *position);

/**
 * Plays the sound one kind of door makes when it closes.
 */
void EdDoorCloseSe(int door_no, float *position);

/**
 * Selects the default depth-of-field parameters for a level.
 */
void EdSetDOFLevel(int level);

/**
 * Copies a map's depth-of-field parameters into editor state.
 */
void EdSetDOF(DEPTH_OF_FIELD_INFO *info);

/**
 * Draws depth of field using either the stored or requested level.
 */
void EdDrawDOF(int level);

/**
 * Resets the editor lightning effect.
 */
void EdInitThunderEffect();

/**
 * Steps lightning for a map and its editable ground model.
 */
void EdThunderEffect(int map, CEditGround *ground);

/**
 * Draws one editor character with its shadow, cursor and event state.
 */
void EdDrawCharacter(CCharacter *player, int player_draw_mask, int npc_count, CNPCharacter *npcs, int *npc_draw_masks, int draw_shadows, ED_EVENT_INFO *event);

/**
 * Starts effects attached to the supplied map parts.
 */
void EnterPartsEffect(CMapParts *parts, EPARTS_FUNC_DATA *func_data, EDIT_EFFECT_INFO *effects, int count);

/**
 * Clears the editor's pending used-item selection.
 */
void EdUseItemInit();

/**
 * Supplies the item list available to editor menus.
 */
void EdSetUseItem(int *items);

/**
 * Resolves the model and texture file names for an event item.
 */
void EdGetItemFile(int item_no, char *model_path, char *texture_path);

/**
 * Draws the item the player is holding up.
 */
void EdDrawItem();

/**
 * Returns the item selected by the editor menu.
 */
int EdGetUseItem();

/**
 * Selects the character whose name the editor registers.
 */
void EdSetNameRegChara(int character);

/**
 * Selects the shop opened by the editor menu.
 */
void EdSetShopNo(int shop);

/**
 * Starts an editor menu in the requested mode.
 */
int EdInitMenu(int mode);

/**
 * Completes editor-menu initialization once its resources are ready.
 */
int EdInitModeFinish(CCamera *camera, CTexture *texture);

/**
 * Closes the active editor menu.
 */
void EdExitMenu();

/**
 * Steps the active editor-menu mode.
 */
int EdMenuMode();

/**
 * Adds an item awarded by an editor event.
 */
void EdGetItem(int item, int count, int attachment);

/**
 * Expands the inventory limit used by editor rewards.
 */
int EdAddMaxItem(int amount);

/**
 * Returns whether the preceding editor reward exceeded inventory capacity.
 */
int EdCheckItemOver();

/**
 * Clears the editor reward's inventory-overflow flag.
 */
void EdClearItemOverFlag();

/**
 * Returns whether the inventory contains the requested item.
 */
int EdCheckItem(int item);

/**
 * Sets the number of event sound channels which remain disabled.
 */
void EdSetSoundOffCount(int count);

/**
 * Returns whether the requested item can be awarded.
 */
int EdCheckGetItem(int item);

/**
 * Configures the editor's system-message prompt.
 */
void EdSetSystemMes(int mes_no, int count, int position, int input_key, int *args, int value);

/**
 * Configures the editor's contextual help message.
 */
void EdSetHelpMes(int mes_no, int count, int position, int *args, int value);

/**
 * Clears the active editor system message.
 */
void EdClearSystemMes();

/**
 * Returns whether the active editor system message has completed.
 */
int EdSystemMesCheck();

/**
 * Advances the active editor system message.
 */
void EdSystemMesStep();

/**
 * Draws the active editor system message.
 */
void EdSystemMesDraw();

/**
 * Selects the main editor help message for the current build state.
 */
void EdEditMainHelpMes(int building);

/**
 * Displays the prompt for walking into editor mode.
 */
void EdWalkToEditMes(int wait);

/**
 * Displays the contextual help for placing a building part.
 */
void EdEditBuildHelpMes(int part_no);

/**
 * Displays the contextual help for moving an editor part.
 */
void EdEditMoveHelpMes();

/**
 * Displays the fishing movement help message.
 */
void EdFishingWalkHelpMes(int bait);

/**
 * Displays the fishing-angle help message.
 */
void EdFishingAngleHelpMEs(int bait);

/**
 * Displays the message for losing fishing bait.
 */
void EdFishingLostEsaMes();

/**
 * Displays the message describing an awarded item.
 */
void EdItemGetMes(int item, int kind, int attachment, int duration);

/**
 * Initializes the editor's animated item box frames.
 */
void EdInitOpenItemBox(CFrameVu1 *frame, CFrameVu1 *base);

/**
 * Positions the editor's animated item box.
 */
void EdSetOpenItemBox(float *position, float *rotation);

/**
 * Advances the editor's item-box opening animation.
 */
void EdStepOpenItemBox();

/**
 * Draws the editor's animated item box.
 */
void EdDrawOpenItemBox();

/**
 * Converts a clock hour into the game's twelve-unit time scale.
 */
float ConvertTime(float hour);

/**
 * Converts the game's twelve-unit time scale back into a clock hour.
 */
float InvertTime(float time);

/**
 * Gives back which of the editor views is on screen.
 */
int EdCheckViewMode();

/**
 * Builds the villager table a map description asks for.
 */
void EdCreateVillagerTable(EDIT_MAP_INFO *info);

/**
 * Draws the markers of the event points active at the given time.
 */
void EdEventPointDraw(ED_EVENT_POINT *points, int count, float time);

/**
 * Gives back the motion a door plays for one state.
 */
int EdGetDoorMotion(int door_no, int state);

/**
 * Clears the parameters an editor event script reads.
 */
int EdInitEventParam();

/**
 * Fills the event points one map-parts entry describes.
 */
int EdInitEventPoint(CMapParts *parts, short *indices, EPARTS_FUNC_DATA *functions, int function_count, ED_EVENT_POINT *points, int point_count);

/**
 * Copies ladder endpoints and event parameters into the active event.
 */
void EdInitHashigo(ED_EVENT_INFO *info, ED_EVENT_PARAM *param);

/**
 * Prepares the event that walks the player into an interior.
 */
int EdInitGotoInterior(ED_EVENT_INFO *info, ED_EVENT_PARAM *param);

/**
 * Runs one frame of the editor's message menu and reports when it closes.
 */
int EdMenuLoop(ClsMes *message);

/**
 * Copies a parts-info header into the editor parts record.
 */
int EdInitToEPInfo(INIT_PARTSINFO *init, EPARTS_INFO_HEADER *header);

/**
 * Clears what the editor knows about villager movement.
 */
void EdInitVillagerControl();

/**
 * Fills the villager table for one time of day.
 */
void EdInitVillagerTable(float clock, EDIT_MAP_INFO *info);

/**
 * Runs the player character for one editor frame.
 */
void EdMoveChara();

/**
 * Clears the editor movement state before a map runs.
 */
void EdMoveCharaInit();

/**
 * Runs one villager for a frame on the current map.
 */
void EdMoveVillager(VILLAGER_INFO *villagers);

/**
 * Runs one villager for a frame on the interior map.
 */
void EdMoveVillagerSubMap(VILLAGER_INFO *villagers);

/**
 * Texture used to hold a captured editor frame.
 */
extern CTexture frame_image_tex;

/**
 * Whether the editor has a pending frame capture.
 */
extern int frame_image_flag;

/**
 * Captures a pending editor frame and clears the request.
 */
void EdSaveFrameImageTask();

/**
 * Prepares the frame capture the editor writes to a file.
 */
void EdSaveFrameImageInit();

/**
 * Requests that the drawn frame be copied into a texture for a menu to show behind itself.
 */
void EdSaveFrameImage(CTexture texture);

/**
 * Plays one of the editor sound effects.
 */
void EdSePlay(ED_SOUND_ID sound, int pan);

/**
 * Finds the event a name and a range select.
 */
int EdSearchEvent(ED_EVENT_PARAM *param, char *name, int map_no, float time);

/**
 * Sets the editor background music volume for the time of day.
 */
void EdSetBgmVol(float time);

/**
 * Applies a map description lighting preset to a frame.
 */
void EdSetLightParam(float clock, int fixed, EDIT_MAP_INFO *info, CFrameVu1 *sky);

/**
 * Draws the editor sky dome, celestial layers, and their time-of-day transitions.
 */
void EdDrawSky(float clock, CFrameVu1 **sky, CFrame **sun, CFrameVu1 *clouds, CCamera *camera, int *follow_axes);

/**
 * Runs the talking-to-a-villager mode for a frame.
 */
int EdTalkMode(CCharacter *player, CCameraFollow *camera, int mode, int *selection);

/**
 * Holds the edit cursor inside the area the map allows.
 */
void LimitEditCursorPos(float *position, float *movement);

} // namespace dc::game
