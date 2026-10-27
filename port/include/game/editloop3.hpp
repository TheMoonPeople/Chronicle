#pragma once

/**
 * @file
 * Declares the town editor's event points, villagers, event scripts and screen fades.
 */

#include "common.h"

#include "dataalloc_fwd.hpp"
#include "texture.hpp"

namespace dc::game {

/**
 * Periods of a town day, as EdGetTime reports them.
 */
// clang-format off
enum EdTimePeriod {
    ED_TIME_DAY     = 0, /**< Day. */
    ED_TIME_EVENING = 1, /**< Evening. */
    ED_TIME_NIGHT   = 2, /**< Night. */
    ED_TIME_MORNING = 3, /**< Morning. */
};

// clang-format on

class CCharacter;
class CCamera;
class C3DSprite;
class CFrame;
class CMapParts;
class CNPCharacter;
struct EDITPARTS_INFO;
struct RS_STACKDATA;
struct OBJ_ANIME_SEQ;
struct EDIT_MAP_INFO;
class CRunScript;
class CCPoly;
class CEditGround;
struct ED_EVENT_PARAM;
struct ED_EVENT_POINT;
struct EPARTS_FUNC_DATA;
struct VILLAGER_INFO;

/**
 * Shares the active editor map objects with event-point and villager helpers.
 */
struct ED_EXCHANGE_INFO {
    CCharacter  *player; /**< Player character used for villager proximity checks. */
    u8           unk_04[0x8];
    CEditGround *ground;        /**< Editable ground used to resolve part-relative event points. */
    CFrame      *event_marker;  /**< Frame drawn at system event points. */
    C3DSprite   *system_effect; /**< Sprite drawn at special event points. */
    CCamera     *camera;        /**< Camera used to prioritize nearby villagers for drawing. */
};

STATIC_ASSERT(sizeof(ED_EXCHANGE_INFO) == 0x1C);

/**
 * Shared editor objects used by event-point and villager processing.
 */
extern ED_EXCHANGE_INFO EdExchangeInfo;

/**
 * Finds the villager nearest a character that stands close enough and faces it to be talked to.
 */
int EdSearchNearNPC(CCharacter *character, CNPCharacter *npcs, int count);

/**
 * Starts a conversation with a villager, returning zero where it has nothing to say.
 */
int EdTalkModeInit(CNPCharacter *villager, int character_info_id);

/**
 * Resets the villager slots and loads each selected villager's character, or queues its read.
 */
void EdInitVilager(VILLAGER_INFO *villagers, CEditGround *ground, u_long128 *buffer);

/**
 * Loads a villager from an already resident character pack.
 */
int EdLoadVillager(u_int *pack, char *name, CNPCharacter *villager, CDataAlloc2<1> *arena);

/**
 * Reads and loads the named villager character pack.
 */
int EdLoadVillager(char *name, CNPCharacter *villager, CDataAlloc2<1> *arena);

/**
 * Decides which placed villagers are drawn from the state of the parts they stand on.
 */
void EdInitVillagerOnOff(CNPCharacter *characters, VILLAGER_INFO *villagers, CEditGround *ground);

/**
 * Places the initialized event villagers at their selected map positions.
 */
void EdInitVilagerPosition(CNPCharacter *villagers, VILLAGER_INFO *info, CEditGround *ground, float (*transform)[4]);

/**
 * Advances and positions the selected villagers on the editable town map.
 */
void EdMoveVillager(VILLAGER_INFO *villagers);

/**
 * Advances the selected villagers while displaying an editor sub-map.
 */
void EdMoveVillagerSubMap(VILLAGER_INFO *villagers);

/**
 * Finds the nearest event point that the character position and facing satisfy.
 */
int EdGetEvent(ED_EVENT_POINT *points, int count, ED_EVENT_PARAM *param, float *position, float *rotation, float time);

/**
 * Adds the collision polygons surrounding active system event points.
 */
int EdEventPointCpPoly(float *position, ED_EVENT_POINT *points, int count, CCPoly *polygons, float time);

/**
 * Starts loading the editor resources belonging to a named destination map.
 */
void EdMapJump(int kind, char *name);

/**
 * Chooses the villagers on the map for the given time and fills the villager table.
 */
int EdSelectVillager(VILLAGER_INFO *villagers, float clock, EDIT_MAP_INFO *map_info);

/**
 * Tests whether one villager is currently permitted to appear.
 */
int EdCheckVillager(int index, VILLAGER_INFO *villager, CEditGround *ground);

/**
 * Collects every object or frame controlled by one event-script object slot.
 */
struct OBJ_HANDLE {
    CMapParts  *map_parts;  /**< Map part controlled directly by the slot. */
    CCharacter *character;  /**< Character controlled directly by the slot. */
    CFrame     *frames[12]; /**< Model frames controlled when no complete object is retained. */
};

STATIC_ASSERT(sizeof(OBJ_HANDLE) == 0x38);

/**
 * Object handles exposed to editor event scripts.
 */
extern OBJ_HANDLE ObjHandle[32];

/**
 * Character container used for script-loaded scene animation data.
 */
extern CCharacter SceneData;

/**
 * Camera used when no scene motion camera remains attached.
 */
extern CCamera DmmyCamera;

/**
 * Script interpreter reserved for system events; no code runs it.
 */
extern CRunScript SystemEventScript;

/**
 * Interpreter of the town's event script, which EdRunEvent starts.
 */
extern CRunScript EdEventScript;

/**
 * Script variable the use-item and bait menus write their choice to.
 */
extern char *p_use_item;

/**
 * Dispatch table built from the editor-event and system-script external-function registries.
 */
extern int (*ext_func__2[1500])(RS_STACKDATA *, int);

/**
 * Identifies the abstract confirm and cancel bits exposed to editor scripts.
 */
enum ED_PAD_BUTTON {
    ED_PAD_CONFIRM = 0x20, /**< Confirm bit. */
    ED_PAD_CANCEL = 0x40,  /**< Cancel bit. */
};

/**
 * Allocates and initializes an event point from one map-part function marker.
 */
ED_EVENT_POINT *GetNewEventPoint(CMapParts *parts, EPARTS_FUNC_DATA *function, ED_EVENT_POINT *points, int count);

/**
 * Stores one script-controlled sprite drawn during an editor event.
 */
struct ED_SPRITE {
    s16      enabled;       /**< Whether the sprite is active. */
    s16      layer;         /**< Drawing layer that selects when the sprite is emitted. */
    s16      rotated;       /**< Whether the sprite uses the rotated drawing path. */
    s16      bilinear;      /**< Texture filtering mode used while drawing the sprite. */
    float    x;             /**< Horizontal screen position. */
    float    y;             /**< Vertical screen position. */
    s16      width;         /**< Destination width, or negative to use the source width. */
    s16      height;        /**< Destination height, or negative to use the source height. */
    s16      source_x;      /**< Horizontal texture origin. */
    s16      source_y;      /**< Vertical texture origin. */
    s16      source_width;  /**< Width of the source texture rectangle. */
    s16      source_height; /**< Height of the source texture rectangle. */
    s16      red;           /**< Red colour component. */
    s16      green;         /**< Green colour component. */
    s16      blue;          /**< Blue colour component. */
    s16      alpha;         /**< Alpha colour component. */
    s16      rotation_x;    /**< Horizontal centre used for rotation. */
    s16      rotation_y;    /**< Vertical centre used for rotation. */
    float    rotation;      /**< Sprite rotation angle. */
    float    move_x;        /**< Horizontal displacement applied per frame. */
    float    move_y;        /**< Vertical displacement applied per frame. */
    u8       unk_34[4];
    CTexture texture; /**< Texture rendered by this sprite. */

    /**
     * Constructs the texture owned by an editor event sprite.
     */
    ED_SPRITE();
};

STATIC_ASSERT(sizeof(ED_SPRITE) == 0x88);

/**
 * Toggles whether advancement of the current editor event is paused.
 */
void EdEventPause();

/**
 * Toggles whether execution of the current event is stopped for the event debugger. PAL only.
 */
void EdEventStopPlay();

/**
 * Reports whether advancement of the current editor event is paused. PAL only.
 */
int EdCheckEventPause();

/**
 * Applies one editable part definition's object visibility to its map part.
 */
void EdPartsObjectOnOff(CMapParts *parts, EDITPARTS_INFO *info, int mode);

/**
 * Expands the object names attached to one editable-parts definition.
 */
void GetElementObjName(EDITPARTS_INFO *info, char **names, int element, int mode);

/**
 * Installs the common and map-specific event bytecode streams.
 */
int EdSetEventScript(char *common_script, char *map_script, CDataAlloc2<1> *allocator);

/**
 * Initializes the editor's full-screen fade state.
 */
void EdFadeInit();

/**
 * Starts fading from an opaque colour to the scene; a negative frame
 * count holds the colour over the screen instead of fading.
 */
void EdFadeIn(int frames, float red, float green, float blue);

/**
 * Starts fading from the scene to an opaque colour.
 */
void EdFadeOut(int frames, float red, float green, float blue);

/**
 * Returns whether the active fade has reached its endpoint.
 */
int EdFadeOutCheck();

/**
 * Reads the colour currently used by the editor's screen fade.
 */
void EdGetFadeColor(float *colour);

/**
 * Steps and draws the active full-screen fade.
 */
void EdFadeInOut();

/**
 * Scales and clamps the shadow-light component used by the editor.
 */
void EdLimitShadowLight(float light[][4], float scale);

/**
 * Tests whether an editor time falls inside a possibly wrapping interval.
 */
int EdCheckTime(float time, float start, float end);

/**
 * Classifies an editor time into one of four daily lighting periods. @see EdTimePeriod.
 */
int EdGetTime(float time);

/**
 * Clears every active editor event and its associated presentation state.
 */
int EdEventAllClear();

/**
 * Advances the event's player and non-player characters by one frame, cloth included.
 */
int EdEventNPCStep();

/**
 * Draws the Georama editor's event sprites over the frame.
 */
int EdEventSpriteDraw();

/**
 * Draws the event sprites layered behind the scene.
 */
int EdEventBackSpriteDraw();

/**
 * Returns the villager involved in the active conversation.
 */
CNPCharacter *EdNowTalkChara();

/**
 * Returns the metadata identifier of the active conversation character.
 */
int EdNowTalkCharaInfoID();

/**
 * Tests whether a villager can appear under the current selection and part availability.
 */
int EdCheckVillagerIn(int index, VILLAGER_INFO *villager);

/**
 * Turns a character towards a world position, a limited step each call.
 */
void turn_chara(CCharacter *character, float *position, float speed);

/**
 * Resets editor-event parameters, action sequences, sprites and object handles.
 */
int EdInitEventParamSimple();

/**
 * Starts one editor event: clears the event parameters, takes the light and
 * ambient colour the map is drawn with, loads the event script and runs it.
 */
int EdEventInit(int event_number, CDataAlloc2<1> *arena, char *program);

/**
 * Resets the event workspace from the arena's free space and runs one program of a script.
 */
void RunEvent(CRunScript *script, int program, CDataAlloc2<1> *arena);

/**
 * Reports whether an editable part has been placed and still has work left.
 */
int CheckPartsInfo(EDITPARTS_INFO *info);

/**
 * Draws the editor sky's lens-flare frames for the current time of day.
 */
void EdDrawLensFlare(float time, CFrame **sky);

/**
 * Configures automatic message advancement for the active editor event.
 */
int _SET_MES_AUTOSET(RS_STACKDATA *stack, int argument_count);

/**
 * Whether the running event is a simple one.
 */
extern int simple_event;

} // namespace dc::game
