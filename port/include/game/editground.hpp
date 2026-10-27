#pragma once

#include "common.h"

#include <libvu0.h>

#include "mapparts.hpp"
#include "water.hpp"

namespace dc::game {

/**
 * Towns with a Georama ground, as CEditGround::map_no holds them.
 */
// clang-format off
enum Town {
    TOWN_NORUNE       = 0, /**< Norune Village. */
    TOWN_MATATAKI     = 1, /**< Matataki Village. */
    TOWN_QUEENS       = 2, /**< Queens. */
    TOWN_MUSKA_LACKA  = 3, /**< Muska Lacka. */
    TOWN_YELLOW_DROPS = 4, /**< Yellow Drops. */
    TOWN_COUNT        = 5, /**< Number of towns with a ground. */
};

// clang-format on

class CCameraFollow;
class CEditArea;
class CEditPartsInfo;
class CRect_i_;
struct EPARTS_FUNC_DATA;
class CSaveData;

/**
 * Describes one ripple source that keeps a ground water surface moving.
 */
struct GROUND_WATER_RIPPLE {
    float row;    /**< Grid row the ripple starts at; below zero picks one at random. */
    float column; /**< Grid column the ripple starts at; below zero picks one at random. */
    float range;  /**< Largest random height added to each push. */
    float power;  /**< Height of each push before the random part. */
};

/**
 * Holds one water surface of the editable ground and whether it is drawn.
 */
class CGroundWater {
public:
    char                name[32];  /**< Frame of the owning part that must be drawn for the surface to draw, or empty. */
    s32                 draw;      /**< Whether this water surface is drawn. */
    s32                 follow[3]; /**< Whether a free surface keeps ahead of the camera along X, level with it, and ahead along Z. */
    s32                 unk_030;
    s32                 parts_no; /**< Plot of the part the surface stands on, or below zero for a free surface. */
    u8                  unk_038[8];
    sceVu0FVECTOR       offset;     /**< Position of the surface relative to its part, or in the world. */
    GROUND_WATER_RIPPLE ripples[4]; /**< Ripple sources; one with no power and no range ends the list. */
    CWater              water;      /**< Surface that ripples and draws. */
};

STATIC_ASSERT(sizeof(CGroundWater) == 0x3B0);

/**
 * Starts a saved ground: the placed parts follow it as SV_GRD_PART records.
 */
struct GROUND_SAVE_HEADER {
    s16 count;  /**< Number of records before the terminating one. */
    s16 size;   /**< Bytes from the header to the end of the terminating record. */
    s16 offset; /**< Bytes from the header to the first record. */
    s16 unk_06;
};

/**
 * Draws the placement cursor out of corner, edge and centre pieces.
 */
class CPartsCursor {
public:
    s32     area;      /**< Area the cursor is drawn in. */
    float   unit_size; /**< Width of one grid cell. */
    CFrame *pieces[3]; /**< Corner, edge and centre pieces of the cursor. */

    /**
     * Draws the placement cursor over a run of cells.
     */
    void Draw(float *position, int width, int height);
};

/**
 * Holds the editable areas of a Georama map and every part placed on them.
 */
class CEditGround {
public:
    s32               map_no;          /**< Georama town the ground belongs to. @see Town. */
    CEditArea        *areas[4];        /**< Editable areas that make up the ground. */
    s32               area_visible[4]; /**< Whether each area is drawn this frame. */
    u8                unk_00024[0xC];
    CMapParts         parts[128]; /**< Parts placed on the ground, indexed by part ID. */
    CEditPartsInfo   *parts_info; /**< Catalogue of the parts the map can hold. */
    u8                unk_15034[0xC];
    CGroundWater      water_surfaces[4]; /**< The four ground water surfaces. */
    s32               effect_count;      /**< Steps left before the falling part lands; zero or less when none falls. */
    s32               effect_parts_id;   /**< ID of the falling part, or -1. */
    float             effect_alt;        /**< Height the falling part starts from. */
    float             effect_target_alt; /**< Height the falling part lands at. */
    float             effect_step;       /**< Height the falling part moves each step. */
    s32               focus_parts_id;    /**< ID of the part under the cursor, or -1. */
    u8                unk_15f18[8];
    sceVu0FVECTOR     clip_plane;  /**< Plane used to clip the editable ground's rendered geometry. */
    CMapParts        *plot_parts;  /**< Template part of each plot, indexed by plot number. */
    CMapParts        *river_parts; /**< Template river pieces, indexed by how the river joins its neighbours. */
    CMapParts        *road_parts;  /**< Template road pieces, indexed by how the road joins its neighbours. */
    u8                unk_15f3c[4];
    CMapParts         fixed_parts[64]; /**< Parts of the map that the player cannot move; in Muska Racka the first is the ground model. */
    s32               spare_words[1];  /**< Words Initialize clears; nothing reads them. */
    CPartsCursor      cursor;          /**< Cursor drawn over the cells a part would occupy. */
    EPARTS_FUNC_DATA *people[128];     /**< Villager markers of the placed parts. */
    s32               people_count;    /**< Number of villager markers in use. */
    s32               suppress_water;  /**< Whether rendering of the editable ground's water is disabled. */
    /**
     * Places one part on the ground, replacing or refusing it according to what already
     * stands there, and gives back the slot it took or -1 when it was refused.
     */
    int SetMapParts(int parts_no, float x, float y, float z, int rot_y);

    /**
     * Re-chooses the river piece one cell away so the water joins up with its
     * neighbours.
     */
    int SetRiverParts(float x, float y, float z, int column_step, int row_step);

    /**
     * Re-chooses the road piece one cell away so the road joins up with its neighbours.
     */
    int SetRoadParts(float x, float y, float z, int column_step, int row_step);

    /**
     * Takes the part standing at a position off the ground and gives back what it was.
     */
    int DeleteMapParts(int *out_parts_no, int *out_rot_y, float x, float y, float z);

    /**
     * Gives the editable area a position falls in, or -1 where none does.
     */
    int GetAreaCode(float x, float y, float z);

    /**
     * Gives the ground height at a position.
     */
    float GetAlt(float x, float y, float z);

    /**
     * Gives the ground height at a position, snapped to the grid.
     */
    int GetAlt_i(float x, float y, float z);

    /**
     * Finds the placed part with a given plot number.
     */
    CMapParts *GetPartsObject(int parts_no);

    /**
     * Gives the placed part standing at a position, or -1 where none does.
     */
    int GetPartsID(float x, float y, float z);

    /**
     * Gives the part standing at a position.
     */
    CMapParts *GetParts(float x, float y, float z);

    /**
     * Reports whether the drop-in effect is still running.
     */
    int CheckEffect();

    /**
     * Starts a part falling into place from above.
     */
    void SetBuildEffect(int parts_id);

    /**
     * Advances the falling part one step and settles it when it lands.
     */
    void EffectTask();

    /**
     * Marks the part at a position as the one the cursor is on.
     */
    int SetFocusParts(float x, float y, float z);

    /**
     * Clips the editable areas against the camera.
     */
    void EditAreaClip(CCamera *camera, float range);

    /**
     * Picks a random flat, unbuilt cell away from the edges, inside the bounds sphere
     * when it has a radius and outside every avoid sphere, and gives its centre.
     */
    int GetRandomPlanePos(sceVu0FVECTOR out_position, sceVu0FVECTOR avoid[], int avoid_count, sceVu0FVECTOR bounds);

    /**
     * Collects the placed parts whose bounds meet a box.
     */
    int GetNearParts(CMapParts **out_parts, int limit, CBoxVu0 *box, CBoxVu0 *fixed_box);

    /**
     * Rebuilds the bounding boxes of every editable area.
     */
    void MakePartsBox();

    /**
     * Gives the bounding box of the area a position falls in.
     */
    void GetPartsBox(CBoxVu0 *out_box, float x, float y, float z);

    /**
     * Gives the position of one of the villagers standing on the ground, and the marker it stands at.
     */
    EPARTS_FUNC_DATA *GetPeoplePos(int villager, float *out_position);

    /**
     * Draws the grid of every editable area.
     */
    void DrawBaseGround();

    /**
     * Draws every placed part, choosing the level of detail from the distance.
     */
    void Draw(float time, int pass, int lowest, int highest, int fixed_lowest, int fixed_highest);

    /**
     * Advances the ripples of each water surface the view can reach.
     */
    void StepWater();

    /**
     * Draws the water surfaces, ordered back to front from the camera.
     */
    void DrawWaterSurface(CCamera *camera);

    /**
     * Draws the water pieces of the placed parts.
     */
    void DrawWater(int pass);

    /**
     * Draws the ripples standing on the placed parts.
     */
    void DrawRipple(int pass);

    /**
     * Drops the shadows of the placed parts onto the ground.
     */
    void DrawShadow(int pass, float near_distance, float far_distance);

    /**
     * Draws the cursor over the cells a part would occupy, colouring it by whether it
     * may go there.
     */
    void DrawPartsCursor(int plot, float *position, float *model_pos, int rot_y, float *rotation, int area_no);

    /**
     * Draws the effects standing on the placed parts.
     */
    void DrawEffect(CCameraFollow *camera, float time, CEffectGroup *effects);

    /**
     * Writes every placed part to the host debug file gdata0.edt; the argument is unused.
     */
    void Save(char *path);

    /**
     * Reads placed parts back out of a buffer, clearing the ground first.
     */
    void Load(char *data);

    /**
     * Writes every placed part into the save file.
     */
    void Save(int town, CSaveData *save);

    /**
     * Reads placed parts back out of the save file.
     */
    void Load(int town, CSaveData *save);

    /**
     * Collects the collision polygons within thirty units of a position.
     */
    int PickUpPoly(CCPoly *polygons, float x, float y, float z);

    /**
     * Collects the collision polygons meeting a box.
     */
    int PickUpPoly(CCPoly *out_polygons, CBoxVu0 box, int flags);

    /**
     * Collects the ground polygons of the area a position falls in.
     */
    int PickUpEditAreaPoly(CCPoly *polygons, float x, float y, float z);

    /**
     * Collects the polygons the camera collides against.
     */
    int PickUpCameraPoly(CCPoly *out_polygons, CBoxVu0 &box, int flags);

    /**
     * Empties every placed part and every editable area.
     */
    void Clear();

    /**
     * Clears the ground's areas, parts, water and villager lists.
     */
    void Initialize();

    /**
     * Rebuilds the grid the parts are placed on.
     */
    void RemakeGrid();

    /**
     * Constructs the ground's parts, water surfaces and interior parts.
     */
    CEditGround();

    /**
     * Counts what stands on the ground and reports which villager requests it
     * satisfies.
     */
    void RequestCheck();

    /**
     * Reports whether a part of a given kind stands inside a rectangle.
     */
    int CheckPartsRect(int parts_no, int area_no, CRect_i_ &rect);

    /**
     * Gives the grid rectangle a part covers, widened by a margin on every side.
     */
    void GetRectParts(CRect_i_ *rect, CMapParts *target, int margin);

    /**
     * Gives the grid cell that one cell of a part lands on, turned the way the part faces.
     */
    void GetRectParts(CRect_i_ *rect, CMapParts *target, int column, int row);

    /**
     * Gives the strip of cells a set depth deep along one side of a part, the side
     * taken relative to the way the part faces.
     */
    void GetRectDirParts(CRect_i_ *rect, CMapParts *target, int direction, int depth);

    /**
     * Checks the ground against Norn's request.
     */
    void NornRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against Matataki's request, which wants the rivers chained up.
     */
    void MatatagiRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against the Queen's request.
     */
    void QueensRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against Muska Racka's request.
     */
    void MuskaRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against the yellow request, which wants parts facing one
     * another.
     */
    void YellowRequest(CMapParts *(*placed)[64]);
};

STATIC_ASSERT(sizeof(CEditGround) == 0x20960);

// Port additions.

/**
 * Casts the shadows of the parts within video.shadow_distance of the eye that lie deeper than the
 * map's shadow_near, which CEditGround::DrawShadow leaves out of the near band. EditMainDraw casts
 * them before the depth of field, so that it softens them with the ground they fall on.
 */
void EdGroundDrawShadowBeyondNear(CEditGround *ground, float shadow_near);

} // namespace dc::game
