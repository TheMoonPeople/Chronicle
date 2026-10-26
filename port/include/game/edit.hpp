#pragma once

/**
 * @file
 * Declares the shared editor helpers implemented by retail editetc.cpp.
 */

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
class CCharacter;
class CDebugFont;
class CEditGround;
class CFrameVu1;
class CMainChara;
class CMapParts;
class CTexture;
struct EDIT_EFFECT_INFO;
struct EPARTS_FUNC_DATA;
class CCharacter;
class CCamera;
class CEditGround;
struct ED_EVENT_POINT;
class CMapObject;
class CFrame;
class CCameraFollow;

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
struct ED_EVENT_POINT;
struct EDIT_MAP_INFO;

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
struct EPARTS_INFO_HEADER;
struct INIT_PARTSINFO;
struct OBJ_ANIME_SEQ;
struct VILLAGER_INFO;
class CCameraFollow;
class CMapParts;

/** Names the sound effects the editor plays for itself. */
enum ED_SOUND_ID {
    ED_SOUND_NONE,
    ED_SOUND_COUNT = 15,
};

class CFrame;
class CFrameVu1;
class CCharacter;
class CNPCharacter;
class ClsMes;
class CTexAnimeData;

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

/** Whether a map's start events run when the editor enters it. */
extern int EdDebugEventEnable;

/** Whether the editor camera is left to the debug controls. */
extern int EdDebugCameraFlag;

/** Disables the editor's parameter overlay. */
extern int EdDebugParamDrawOff;

/** Disables character drawing in the editor debug view. */
extern int EdDebugCharaDrawOff;

/** Whether the editor character is left to the debug controls. */
extern int EdDebugMoveFlag;

/** Event the debug menu asks the editor to run this frame, or -1. PAL only. */
extern int EdDebugRunEventNo;

/**
 * Steps the editor's event system, and says what the event asks the loop to do.
 *
 * @mangled EdEventMode__FP13CCameraFollowi
 * @address 0x197AD0
 * @size 0xCFC
 */
int EdEventMode(CCameraFollow *camera, int kind);

/**
 * Puts the camera back on the parameters the editor uses outside events.
 *
 * @mangled EdInitCameraParam__FP13CCameraFollow
 * @address 0x169F80
 * @size 0x4C
 */
void EdInitCameraParam(CCameraFollow *camera);

/**
 * Starts the editor's ambient sound set for the time of day.
 *
 * @mangled EdAmbientPlay__Ff
 * @address 0x1715D0
 * @size 0x40
 */
void EdAmbientPlay(float time);

/**
 * Sets the volume of the active editor ambient sound.
 *
 * @mangled EdSetAmbientVol__Ff
 * @address 0x171610
 * @size 0x20
 */
void EdSetAmbientVol(float volume);

/**
 * Writes the root directory used by editor map resources.
 *
 * @mangled GetEditDataDir__FPc
 * @address 0x1777D0
 * @size 0x28
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
 *
 * @mangled EdDDebug__Fi
 * @address 0x170250
 * @size 0xC
 */
void EdDDebug(int on);

/**
 * Selects the font used by the editor debug overlay.
 *
 * @mangled EdDSetFont__FP10CDebugFont
 * @address 0x170260
 * @size 0xC
 */
void EdDSetFont(CDebugFont *font);

/**
 * Draws text queued by the editor debug overlay.
 *
 * @mangled EdDDrawFont__Fv
 * @address 0x170270
 * @size 0xB0
 */
void EdDDrawFont();

/**
 * Updates the editor debug overlay and free-movement controls.
 *
 * @mangled EdDCheck__Fv
 * @address 0x170320
 * @size 0x2C
 */
void EdDCheck();

/**
 * Writes the editor debug text buffer to its output file.
 *
 * @mangled EdOutPutFile__Fv
 * @address 0x170350
 * @size 0x7C
 */
void EdOutPutFile();

/**
 * Queues the player character's position, rotation and ground attributes in the editor debug overlay. PAL only.
 *
 * @mangled EdDPrintChara__FP10CMainChara
 * @address 0x170D80
 * @size 0x184
 */
void EdDPrintChara(CMainChara *chara);

/**
 * Queues the camera's position, reference point and projection in the editor debug overlay. PAL only.
 *
 * @mangled EdDPrintCamera__FP7CCamera
 * @address 0x170F10
 * @size 0x174
 */
void EdDPrintCamera(CCamera *camera);

/**
 * Queues a labelled vector in the editor debug overlay.
 *
 * @mangled EdDPrintVector__FPcPf
 * @address 0x170450
 * @size 0xB8
 */
void EdDPrintVector(char *name, float *vector);

/**
 * Queues a line of text in the editor debug overlay.
 *
 * @mangled EdDPrint__FPc
 * @address 0x170510
 * @size 0x3C
 */
void EdDPrint(char *text);

/**
 * Moves the editor camera from debug-pad input.
 *
 * @mangled EdDMoveCamera__FPfPf
 * @address 0x170550
 * @size 0x378
 */
void EdDMoveCamera(float *position, float *reference);

/**
 * Moves the editor camera reference point from debug-pad input.
 *
 * @mangled EdDMoveCameraRef__FPfPf
 * @address 0x1708D0
 * @size 0x294
 */
void EdDMoveCameraRef(float *position, float *reference);

/**
 * Moves an editor character using the debug camera controls.
 *
 * @mangled EdDMoveChara__FP10CCharacterP7CCamera
 * @address 0x170B70
 * @size 0x428
 */
void EdDMoveChara(CCharacter *character, CCamera *camera);

/**
 * Runs the editor debug menu page the shoulder buttons select and draws the debug overlay. PAL only.
 *
 * @mangled EdDebugMenu__Fv
 * @address 0x171BE0
 * @size 0x128
 */
void EdDebugMenu();

/**
 * Runs the main debug menu page of camera, drawing, movement, event and language switches. PAL only.
 *
 * @mangled DM_Main__Fv
 * @address 0x171D10
 * @size 0x7FC
 */
void DM_Main();

/**
 * Runs the sound debug menu page of music, sound effect and sound set tests. PAL only.
 *
 * @mangled DM_Sound__Fv
 * @address 0x172510
 * @size 0x634
 */
void DM_Sound();

/**
 * Runs the flag debug menu page of game, map, completion and talk flags and dungeon progress. PAL only.
 *
 * @mangled DM_Flag__Fv
 * @address 0x172B50
 * @size 0xC18
 */
void DM_Flag();

/**
 * Resets the positional sound-source slots.
 *
 * @mangled EdInitSoundSrc__Fv
 * @address 0x171800
 * @size 0x64
 */
void EdInitSoundSrc();

/**
 * Starts the positional sound sources prepared for the current frame.
 *
 * @mangled EdPlaySoundSrc__Fv
 * @address 0x171870
 * @size 0x260
 */
void EdPlaySoundSrc();

/**
 * Stops every active positional sound source.
 *
 * @mangled EdStopSoundSrc__Fv
 * @address 0x171AD0
 * @size 0x94
 */
void EdStopSoundSrc();

/**
 * Updates positional sound volume and panning for visible map parts.
 *
 * @mangled EdSetSoundSrcVol__FfPP9CMapPartsiPfPf
 * @address 0x171D20
 * @size 0x3E0
 */
void EdSetSoundSrcVol(float time, CMapParts **parts, int count, float *camera_pos, float *camera_dir);

/**
 * Plays the sound one kind of door makes when it opens.
 *
 * @mangled EdDoorOpenSe__FiPf
 * @address 0x172100
 * @size 0x58
 */
void EdDoorOpenSe(int door_no, float *position);

/**
 * Plays the sound one kind of door makes when it closes.
 *
 * @mangled EdDoorCloseSe__FiPf
 * @address 0x172160
 * @size 0x58
 */
void EdDoorCloseSe(int door_no, float *position);

/**
 * Selects the default depth-of-field parameters for a level.
 *
 * @mangled EdSetDOFLevel__Fi
 * @address 0x172210
 * @size 0x8C
 */
void EdSetDOFLevel(int level);

/**
 * Copies a map's depth-of-field parameters into editor state.
 *
 * @mangled EdSetDOF__FP19DEPTH_OF_FIELD_INFO
 * @address 0x1722A0
 * @size 0x50
 */
void EdSetDOF(DEPTH_OF_FIELD_INFO *info);

/**
 * Draws depth of field using either the stored or requested level.
 *
 * @mangled EdDrawDOF__Fi
 * @address 0x1722F0
 * @size 0x58
 */
void EdDrawDOF(int level);

/**
 * Resets the editor lightning effect.
 *
 * @mangled EdInitThunderEffect__Fv
 * @address 0x172350
 * @size 0x18
 */
void EdInitThunderEffect();

/**
 * Steps lightning for a map and its editable ground model.
 *
 * @mangled EdThunderEffect__FiP11CEditGround
 * @address 0x172370
 * @size 0x274
 */
void EdThunderEffect(int map, CEditGround *ground);

/**
 * Draws one editor character with its shadow, cursor and event state.
 *
 * @mangled EdDrawCharacter__FP10CCharacteriiP12CNPCharacterPiiP13ED_EVENT_INFO
 * @address 0x1725F0
 * @size 0x4C8
 */
void EdDrawCharacter(CCharacter *player, int player_draw_mask, int npc_count, CNPCharacter *npcs, int *npc_draw_masks, int draw_shadows, ED_EVENT_INFO *event);

/**
 * Starts effects attached to the supplied map parts.
 *
 * @mangled EnterPartsEffect__FP9CMapPartsP16EPARTS_FUNC_DATAP16EDIT_EFFECT_INFOi
 * @address 0x172AC0
 * @size 0xC4
 */
void EnterPartsEffect(CMapParts *parts, EPARTS_FUNC_DATA *func_data, EDIT_EFFECT_INFO *effects, int count);

/**
 * Clears the editor's pending used-item selection.
 *
 * @mangled EdUseItemInit__Fv
 * @address 0x172BD0
 * @size 0x24
 */
void EdUseItemInit();

/**
 * Supplies the item list available to editor menus.
 *
 * @mangled EdSetUseItem__FPi
 * @address 0x172C00
 * @size 0x80
 */
void EdSetUseItem(int *items);

/**
 * Resolves the model and texture file names for an event item.
 *
 * @mangled EdGetItemFile__FiPcPc
 * @address 0x173350
 * @size 0x30
 */
void EdGetItemFile(int item_no, char *model_path, char *texture_path);

/**
 * Draws the item the player is holding up.
 *
 * @mangled EdDrawItem__Fv
 * @address 0x173380
 * @size 0x58
 */
void EdDrawItem();

/**
 * Returns the item selected by the editor menu.
 *
 * @mangled EdGetUseItem__Fv
 * @address 0x172C80
 * @size 0x24
 */
int EdGetUseItem();

/**
 * Selects the character whose name the editor registers.
 *
 * @mangled EdSetNameRegChara__Fi
 * @address 0x172CB0
 * @size 0xC
 */
void EdSetNameRegChara(int character);

/**
 * Selects the shop opened by the editor menu.
 *
 * @mangled EdSetShopNo__Fi
 * @address 0x172CC0
 * @size 0xC
 */
void EdSetShopNo(int shop);

/**
 * Starts an editor menu in the requested mode.
 *
 * @mangled EdInitMenu__Fi
 * @address 0x172CD0
 * @size 0x60
 */
int EdInitMenu(int mode);

/**
 * Completes editor-menu initialization once its resources are ready.
 *
 * @mangled EdInitModeFinish__FP7CCameraP8CTexture
 * @address 0x172D30
 * @size 0x200
 */
int EdInitModeFinish(CCamera *camera, CTexture *texture);

/**
 * Closes the active editor menu.
 *
 * @mangled EdExitMenu__Fv
 * @address 0x172F30
 * @size 0x44
 */
void EdExitMenu();

/**
 * Steps the active editor-menu mode.
 *
 * @mangled EdMenuMode__Fv
 * @address 0x172F80
 * @size 0x114
 */
int EdMenuMode();

/**
 * Adds an item awarded by an editor event.
 *
 * @mangled EdGetItem__Fiii
 * @address 0x1730A0
 * @size 0xD4
 */
void EdGetItem(int item, int count, int attachment);

/**
 * Expands the inventory limit used by editor rewards.
 *
 * @mangled EdAddMaxItem__Fi
 * @address 0x173180
 * @size 0x84
 */
int EdAddMaxItem(int amount);

/**
 * Returns whether the preceding editor reward exceeded inventory capacity.
 *
 * @mangled EdCheckItemOver__Fv
 * @address 0x173210
 * @size 0x28
 */
int EdCheckItemOver();

/**
 * Clears the editor reward's inventory-overflow flag.
 *
 * @mangled EdClearItemOverFlag__Fv
 * @address 0x173240
 * @size 0x28
 */
void EdClearItemOverFlag();

/**
 * Returns whether the inventory contains the requested item.
 *
 * @mangled EdCheckItem__Fi
 * @address 0x173270
 * @size 0x6C
 */
int EdCheckItem(int item);

/**
 * Sets the number of event sound channels which remain disabled.
 *
 * @mangled EdSetSoundOffCount__Fi
 * @address 0x177760
 * @size 0x28
 */
void EdSetSoundOffCount(int count);

/**
 * Returns whether the requested item can be awarded.
 *
 * @mangled EdCheckGetItem__Fi
 * @address 0x1732E0
 * @size 0x6C
 */
int EdCheckGetItem(int item);

/**
 * Configures the editor's system-message prompt.
 *
 * @mangled EdSetSystemMes__FiiiiPii
 * @address 0x1733E0
 * @size 0xB4
 */
void EdSetSystemMes(int mes_no, int count, int position, int input_key, int *args, int value);

/**
 * Configures the editor's contextual help message.
 *
 * @mangled EdSetHelpMes__FiiiPii
 * @address 0x1734A0
 * @size 0x134
 */
void EdSetHelpMes(int mes_no, int count, int position, int *args, int value);

/**
 * Clears the active editor system message.
 *
 * @mangled EdClearSystemMes__Fv
 * @address 0x1735E0
 * @size 0x60
 */
void EdClearSystemMes();

/**
 * Returns whether the active editor system message has completed.
 *
 * @mangled EdSystemMesCheck__Fv
 * @address 0x173640
 * @size 0x20
 */
int EdSystemMesCheck();

/**
 * Advances the active editor system message.
 *
 * @mangled EdSystemMesStep__Fv
 * @address 0x173660
 * @size 0xE8
 */
void EdSystemMesStep();

/**
 * Draws the active editor system message.
 *
 * @mangled EdSystemMesDraw__Fv
 * @address 0x173750
 * @size 0x70
 */
void EdSystemMesDraw();

/**
 * Selects the main editor help message for the current build state.
 *
 * @mangled EdEditMainHelpMes__Fi
 * @address 0x1737C0
 * @size 0x6C
 */
void EdEditMainHelpMes(int building);

/**
 * Displays the prompt for walking into editor mode.
 *
 * @mangled EdWalkToEditMes__Fi
 * @address 0x173830
 * @size 0x50
 */
void EdWalkToEditMes(int wait);

/**
 * Displays the contextual help for placing a building part.
 *
 * @mangled EdEditBuildHelpMes__Fi
 * @address 0x173880
 * @size 0xAC
 */
void EdEditBuildHelpMes(int part_no);

/**
 * Displays the contextual help for moving an editor part.
 *
 * @mangled EdEditMoveHelpMes__Fv
 * @address 0x173930
 * @size 0x74
 */
void EdEditMoveHelpMes();

/**
 * Displays the fishing movement help message.
 *
 * @mangled EdFishingWalkHelpMes__Fi
 * @address 0x1739B0
 * @size 0x68
 */
void EdFishingWalkHelpMes(int bait);

/**
 * Displays the fishing-angle help message.
 *
 * @mangled EdFishingAngleHelpMEs__Fi
 * @address 0x173A20
 * @size 0x8
 */
void EdFishingAngleHelpMEs(int bait);

/**
 * Displays the message for losing fishing bait.
 *
 * @mangled EdFishingLostEsaMes__Fv
 * @address 0x173A30
 * @size 0x3C
 */
void EdFishingLostEsaMes();

/**
 * Displays the message describing an awarded item.
 *
 * @mangled EdItemGetMes__Fiiii
 * @address 0x173A70
 * @size 0x2C
 */
void EdItemGetMes(int item, int kind, int attachment, int duration);

/**
 * Initializes the editor's animated item box frames.
 *
 * @mangled EdInitOpenItemBox__FP9CFrameVu1P9CFrameVu1
 * @address 0x173AA0
 * @size 0x1C
 */
void EdInitOpenItemBox(CFrameVu1 *frame, CFrameVu1 *base);

/**
 * Positions the editor's animated item box.
 *
 * @mangled EdSetOpenItemBox__FPfPf
 * @address 0x173AC0
 * @size 0x70
 */
void EdSetOpenItemBox(float *position, float *rotation);

/**
 * Advances the editor's item-box opening animation.
 *
 * @mangled EdStepOpenItemBox__Fv
 * @address 0x173B30
 * @size 0x1C8
 */
void EdStepOpenItemBox();

/**
 * Draws the editor's animated item box.
 *
 * @mangled EdDrawOpenItemBox__Fv
 * @address 0x173D00
 * @size 0xF8
 */
void EdDrawOpenItemBox();

/**
 * Converts a clock hour into the game's twelve-unit time scale.
 *
 * @mangled ConvertTime__Ff
 * @address 0x1740F0
 * @size 0x54
 */
float ConvertTime(float hour);

/**
 * Converts the game's twelve-unit time scale back into a clock hour.
 *
 * @mangled InvertTime__Ff
 * @address 0x174150
 * @size 0x4C
 */
float InvertTime(float time);

/**
 * Gives back which of the editor views is on screen.
 *
 * @mangled EdCheckViewMode__Fv
 * @address 0x16A120
 * @size 0xC
 */
int EdCheckViewMode();

/**
 * Builds the villager table a map description asks for.
 *
 * @mangled EdCreateVillagerTable__FP13EDIT_MAP_INFO
 * @address 0x185320
 * @size 0x19C
 */
void EdCreateVillagerTable(EDIT_MAP_INFO *info);

/**
 * Draws the markers of the event points active at the given time.
 *
 * @mangled EdEventPointDraw__FP14ED_EVENT_POINTif
 * @address 0x184750
 * @size 0x300
 */
void EdEventPointDraw(ED_EVENT_POINT *points, int count, float time);

/**
 * Gives back the motion a door plays for one state.
 *
 * @mangled EdGetDoorMotion__Fii
 * @address 0x1721C0
 * @size 0x4C
 * @unknownret
 */
int EdGetDoorMotion(int door_no, int state);

/**
 * Clears the parameters an editor event script reads.
 *
 * @mangled EdInitEventParam__Fv
 * @address 0x197220
 * @size 0x190
 */
int EdInitEventParam();

/**
 * Fills the event points one map-parts entry describes.
 *
 * @mangled EdInitEventPoint__FP9CMapPartsPsP16EPARTS_FUNC_DATAiP14ED_EVENT_POINTi
 * @address 0x183D50
 * @size 0x600
 */
int EdInitEventPoint(CMapParts *parts, short *indices, EPARTS_FUNC_DATA *functions, int function_count, ED_EVENT_POINT *points, int point_count);

/**
 * Copies ladder endpoints and event parameters into the active event.
 *
 * @mangled EdInitHashigo__FP13ED_EVENT_INFOP14ED_EVENT_PARAM
 * @address 0x16D720
 * @size 0xD0
 */
void EdInitHashigo(ED_EVENT_INFO *info, ED_EVENT_PARAM *param);

/**
 * Prepares the event that walks the player into an interior.
 *
 * @mangled EdInitGotoInterior__FP13ED_EVENT_INFOP14ED_EVENT_PARAM
 * @address 0x16D7F0
 * @size 0x68
 */
int EdInitGotoInterior(ED_EVENT_INFO *info, ED_EVENT_PARAM *param);

/**
 * Runs one frame of the editor's message menu and reports when it closes.
 *
 * @mangled EdMenuLoop__FP6ClsMes
 * @address 0x173F00
 * @size 0x1EC
 */
int EdMenuLoop(ClsMes *message);

/**
 * Copies a parts-info header into the editor parts record.
 *
 * @mangled EdInitToEPInfo__FP14INIT_PARTSINFOP18EPARTS_INFO_HEADER
 * @address 0x189570
 * @size 0x154
 * @unknownret
 */
int EdInitToEPInfo(INIT_PARTSINFO *init, EPARTS_INFO_HEADER *header);

/**
 * Clears what the editor knows about villager movement.
 *
 * @mangled EdInitVillagerControl__Fv
 * @address 0x1852B0
 * @size 0x64
 */
void EdInitVillagerControl();

/**
 * Fills the villager table for one time of day.
 *
 * @mangled EdInitVillagerTable__FfP13EDIT_MAP_INFO
 * @address 0x1854C0
 * @size 0x94
 */
void EdInitVillagerTable(float clock, EDIT_MAP_INFO *info);

/**
 * Runs the player character for one editor frame.
 *
 * @mangled EdMoveChara__Fv
 * @address 0x16A160
 * @size 0x35B4
 */
void EdMoveChara();

/**
 * Clears the editor movement state before a map runs.
 *
 * @mangled EdMoveCharaInit__Fv
 * @address 0x169FD0
 * @size 0x18
 */
void EdMoveCharaInit();

/**
 * Runs one villager for a frame on the current map.
 *
 * @mangled EdMoveVillager__FP13VILLAGER_INFO
 * @address 0x186EF0
 * @size 0x5B4
 */
void EdMoveVillager(VILLAGER_INFO *villagers);

/**
 * Runs one villager for a frame on the interior map.
 *
 * @mangled EdMoveVillagerSubMap__FP13VILLAGER_INFO
 * @address 0x1874B0
 * @size 0x258
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
 *
 * @mangled EdSaveFrameImageTask__Fv
 * @address 0x173E70
 * @size 0x54
 */
void EdSaveFrameImageTask();

/**
 * Prepares the frame capture the editor writes to a file.
 *
 * @mangled EdSaveFrameImageInit__Fv
 * @address 0x173ED0
 * @size 0x2C
 */
void EdSaveFrameImageInit();

/**
 * Plays one of the editor sound effects.
 *
 * @mangled EdSePlay__F11ED_SOUND_IDi
 * @address 0x18A110
 * @size 0x40
 */
void EdSePlay(ED_SOUND_ID sound, int pan);

/**
 * Finds the event a name and a range select.
 *
 * @mangled EdSearchEvent__FP14ED_EVENT_PARAMPcif
 * @address 0x184CB0
 * @size 0x200
 * @unknownret
 */
int EdSearchEvent(ED_EVENT_PARAM *param, char *name, int map_no, float time);

/**
 * Sets the editor background music volume for the time of day.
 *
 * @mangled EdSetBgmVol__Ff
 * @address 0x171480
 * @size 0x14C
 */
void EdSetBgmVol(float time);

/**
 * Applies a map description lighting preset to a frame.
 *
 * @mangled EdSetLightParam__FfiP13EDIT_MAP_INFOP9CFrameVu1
 * @address 0x188D50
 * @size 0x818
 */
void EdSetLightParam(float clock, int fixed, EDIT_MAP_INFO *info, CFrameVu1 *sky);

/**
 * Draws the editor sky dome, celestial layers, and their time-of-day transitions.
 *
 * @mangled EdDrawSky__FfPP9CFrameVu1PP6CFrameP9CFrameVu1P7CCameraPi
 * @address 0x187F90
 * @size 0xA88
 */
void EdDrawSky(float clock, CFrameVu1 **sky, CFrame **sun, CFrameVu1 *clouds, CCamera *camera, int *follow_axes);

/**
 * Runs the talking-to-a-villager mode for a frame.
 *
 * @mangled EdTalkMode__FP10CCharacterP13CCameraFollowiPi
 * @address 0x199090
 * @size 0xEBC
 */
int EdTalkMode(CCharacter *player, CCameraFollow *camera, int mode, int *selection);

/**
 * Holds the edit cursor inside the area the map allows.
 *
 * @mangled LimitEditCursorPos__FPfPf
 * @address 0x189B80
 * @size 0x588
 */
void LimitEditCursorPos(float *position, float *movement);
