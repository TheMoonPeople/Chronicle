#pragma once

#include "common.h"

#include <libvu0.h>

#include "mapparts.hpp"
#include "water.hpp"

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

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
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
     *
     * @mangled Draw__12CPartsCursorFPfii
     * @address 0x1A5CB0
     * @size 0x328
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
     *
     * @mangled SetMapParts__11CEditGroundFifffi
     * @address 0x1A0470
     * @size 0x79C
     */
    int SetMapParts(int parts_no, float x, float y, float z, int rot_y);

    /**
     * Re-chooses the river piece one cell away so the water joins up with its
     * neighbours.
     *
     * @mangled SetRiverParts__11CEditGroundFfffii
     * @address 0x1A0C10
     * @size 0x3C8
     */
    int SetRiverParts(float x, float y, float z, int column_step, int row_step);

    /**
     * Re-chooses the road piece one cell away so the road joins up with its neighbours.
     *
     * @mangled SetRoadParts__11CEditGroundFfffii
     * @address 0x1A0FE0
     * @size 0x2A4
     */
    int SetRoadParts(float x, float y, float z, int column_step, int row_step);

    /**
     * Takes the part standing at a position off the ground and gives back what it was.
     *
     * @mangled DeleteMapParts__11CEditGroundFPiPifff
     * @address 0x1A1290
     * @size 0x418
     */
    int DeleteMapParts(int *out_parts_no, int *out_rot_y, float x, float y, float z);

    /**
     * Gives the editable area a position falls in, or -1 where none does.
     *
     * @mangled GetAreaCode__11CEditGroundFfff
     * @address 0x1A16B0
     * @size 0xAC
     */
    int GetAreaCode(float x, float y, float z);

    /**
     * Gives the ground height at a position.
     *
     * @mangled GetAlt__11CEditGroundFfff
     * @address 0x1A1760
     * @size 0x84
     */
    float GetAlt(float x, float y, float z);

    /**
     * Gives the ground height at a position, snapped to the grid.
     *
     * @mangled GetAlt_i__11CEditGroundFfff
     * @address 0x1A17F0
     * @size 0x84
     */
    int GetAlt_i(float x, float y, float z);

    /**
     * Finds the placed part with a given plot number.
     *
     * @mangled GetPartsObject__11CEditGroundFi
     * @address 0x1A1880
     * @size 0x50
     */
    CMapParts *GetPartsObject(int parts_no);

    /**
     * Gives the placed part standing at a position, or -1 where none does.
     *
     * @mangled GetPartsID__11CEditGroundFfff
     * @address 0x1A18D0
     * @size 0x84
     */
    int GetPartsID(float x, float y, float z);

    /**
     * Gives the part standing at a position.
     *
     * @mangled GetParts__11CEditGroundFfff
     * @address 0x1A1960
     * @size 0x5C
     */
    CMapParts *GetParts(float x, float y, float z);

    /**
     * Reports whether the drop-in effect is still running.
     *
     * @mangled CheckEffect__11CEditGroundFv
     * @address 0x1A19C0
     * @size 0x18
     */
    int CheckEffect();

    /**
     * Starts a part falling into place from above.
     *
     * @mangled SetBuildEffect__11CEditGroundFi
     * @address 0x1A19E0
     * @size 0x134
     */
    void SetBuildEffect(int parts_id);

    /**
     * Advances the falling part one step and settles it when it lands.
     *
     * @mangled EffectTask__11CEditGroundFv
     * @address 0x1A1B20
     * @size 0x1E0
     */
    void EffectTask();

    /**
     * Marks the part at a position as the one the cursor is on.
     *
     * @mangled SetFocusParts__11CEditGroundFfff
     * @address 0x1A1D00
     * @size 0x130
     */
    int SetFocusParts(float x, float y, float z);

    /**
     * Clips the editable areas against the camera.
     *
     * @mangled EditAreaClip__11CEditGroundFP7CCameraf
     * @address 0x1A1E30
     * @size 0x89C
     */
    void EditAreaClip(CCamera *camera, float range);

    /**
     * Picks a random flat, unbuilt cell away from the edges, inside the bounds sphere
     * when it has a radius and outside every avoid sphere, and gives its centre.
     *
     * @mangled GetRandomPlanePos__11CEditGroundFPfPA4_fiPf
     * @address 0x1A26D0
     * @size 0x3BC
     */
    int GetRandomPlanePos(sceVu0FVECTOR out_position, sceVu0FVECTOR avoid[], int avoid_count, sceVu0FVECTOR bounds);

    /**
     * Collects the placed parts whose bounds meet a box.
     *
     * @mangled GetNearParts__11CEditGroundFPP9CMapPartsiP7CBoxVu0P7CBoxVu0
     * @address 0x1A2A90
     * @size 0x17C
     */
    int GetNearParts(CMapParts **out_parts, int limit, CBoxVu0 *box, CBoxVu0 *fixed_box);

    /**
     * Rebuilds the bounding boxes of every editable area.
     *
     * @mangled MakePartsBox__11CEditGroundFv
     * @address 0x1A2C10
     * @size 0x64
     */
    void MakePartsBox();

    /**
     * Gives the bounding box of the area a position falls in.
     *
     * @mangled GetPartsBox__11CEditGroundFP7CBoxVu0fff
     * @address 0x1A2C80
     * @size 0x8C
     */
    void GetPartsBox(CBoxVu0 *out_box, float x, float y, float z);

    /**
     * Gives the position of one of the villagers standing on the ground, and the marker it stands at.
     *
     * @mangled GetPeoplePos__11CEditGroundFiPf
     * @address 0x1A2D10
     * @size 0x190
     */
    EPARTS_FUNC_DATA *GetPeoplePos(int villager, float *out_position);

    /**
     * Draws the grid of every editable area.
     *
     * @mangled DrawBaseGround__11CEditGroundFv
     * @address 0x1A2EA0
     * @size 0x64
     */
    void DrawBaseGround();

    /**
     * Draws every placed part, choosing the level of detail from the distance.
     *
     * @mangled Draw__11CEditGroundFfiiiii
     * @address 0x1A2F10
     * @size 0x23C
     */
    void Draw(float time, int pass, int lowest, int highest, int fixed_lowest, int fixed_highest);

    /**
     * Advances the ripples of each water surface the view can reach.
     *
     * @mangled StepWater__11CEditGroundFv
     * @address 0x1A3150
     * @size 0x20C
     */
    void StepWater();

    /**
     * Draws the water surfaces, ordered back to front from the camera.
     *
     * @mangled DrawWaterSurface__11CEditGroundFP7CCamera
     * @address 0x1A3360
     * @size 0x2B8
     */
    void DrawWaterSurface(CCamera *camera);

    /**
     * Draws the water pieces of the placed parts.
     *
     * @mangled DrawWater__11CEditGroundFi
     * @address 0x1A3620
     * @size 0x314
     */
    void DrawWater(int pass);

    /**
     * Draws the ripples standing on the placed parts.
     *
     * @mangled DrawRipple__11CEditGroundFi
     * @address 0x1A3940
     * @size 0x2A0
     */
    void DrawRipple(int pass);

    /**
     * Drops the shadows of the placed parts onto the ground.
     *
     * @mangled DrawShadow__11CEditGroundFiff
     * @address 0x1A3BE0
     * @size 0x34C
     */
    void DrawShadow(int pass, float near_distance, float far_distance);

    /**
     * Draws the cursor over the cells a part would occupy, colouring it by whether it
     * may go there.
     *
     * @mangled DrawPartsCursor__11CEditGroundFiPfPfiPfi
     * @address 0x1A3F30
     * @size 0x6D8
     */
    void DrawPartsCursor(int plot, float *position, float *model_pos, int rot_y, float *rotation, int area_no);

    /**
     * Draws the effects standing on the placed parts.
     *
     * @mangled DrawEffect__11CEditGroundFP13CCameraFollowfP12CEffectGroup
     * @address 0x1A4610
     * @size 0x170
     */
    void DrawEffect(CCameraFollow *camera, float time, CEffectGroup *effects);

    /**
     * Writes every placed part to the host debug file gdata0.edt; the argument is unused.
     *
     * @mangled Save__11CEditGroundFPc
     * @address 0x1A4780
     * @size 0x188
     */
    void Save(char *path);

    /**
     * Reads placed parts back out of a buffer, clearing the ground first.
     *
     * @mangled Load__11CEditGroundFPc
     * @address 0x1A4910
     * @size 0x374
     */
    void Load(char *data);

    /**
     * Writes every placed part into the save file.
     *
     * @mangled Save__11CEditGroundFiP9CSaveData
     * @address 0x1A4C90
     * @size 0x17C
     */
    void Save(int town, CSaveData *save);

    /**
     * Reads placed parts back out of the save file.
     *
     * @mangled Load__11CEditGroundFiP9CSaveData
     * @address 0x1A4E10
     * @size 0xD0
     */
    void Load(int town, CSaveData *save);

    /**
     * Collects the collision polygons within thirty units of a position.
     *
     * @mangled PickUpPoly__11CEditGroundFP6CCPolyfff
     * @address 0x1A4EE0
     * @size 0x70
     */
    int PickUpPoly(CCPoly *polygons, float x, float y, float z);

    /**
     * Collects the collision polygons meeting a box.
     *
     * @mangled PickUpPoly__11CEditGroundFP6CCPoly7CBoxVu0i
     * @address 0x1A4F50
     * @size 0x214
     */
    int PickUpPoly(CCPoly *out_polygons, CBoxVu0 box, int flags);

    /**
     * Collects the ground polygons of the area a position falls in.
     *
     * @mangled PickUpEditAreaPoly__11CEditGroundFP6CCPolyfff
     * @address 0x1A5170
     * @size 0x94
     */
    int PickUpEditAreaPoly(CCPoly *polygons, float x, float y, float z);

    /**
     * Collects the polygons the camera collides against.
     *
     * @mangled PickUpCameraPoly__11CEditGroundFP6CCPolyR7CBoxVu0i
     * @address 0x1A5210
     * @size 0x2F4
     */
    int PickUpCameraPoly(CCPoly *out_polygons, CBoxVu0 &box, int flags);

    /**
     * Empties every placed part and every editable area.
     *
     * @mangled Clear__11CEditGroundFv
     * @address 0x1A5510
     * @size 0x2E8
     */
    void Clear();

    /**
     * Clears the ground's areas, parts, water and villager lists.
     *
     * @mangled Initialize__11CEditGroundFv
     * @address 0x1A5800
     * @size 0x1A8
     */
    void Initialize();

    /**
     * Rebuilds the grid the parts are placed on.
     *
     * @mangled RemakeGrid__11CEditGroundFv
     * @address 0x1A59B0
     * @size 0x40
     */
    void RemakeGrid();

    /**
     * Constructs the ground's parts, water surfaces and interior parts.
     *
     * @mangled __ct__11CEditGroundFv
     * @address 0x1A59F0
     * @size 0xE4
     */
    CEditGround();

    /**
     * Counts what stands on the ground and reports which villager requests it
     * satisfies.
     *
     * @mangled RequestCheck__11CEditGroundFv
     * @address 0x1A5FE0
     * @size 0x1F4
     */
    void RequestCheck();

    /**
     * Reports whether a part of a given kind stands inside a rectangle.
     *
     * @mangled CheckPartsRect__11CEditGroundFiiR8CRect_i_
     * @address 0x1A61E0
     * @size 0xE0
     */
    int CheckPartsRect(int parts_no, int area_no, CRect_i_ &rect);

    /**
     * Gives the grid rectangle a part covers, widened by a margin on every side.
     *
     * @mangled GetRectParts__11CEditGroundFP8CRect_i_P9CMapPartsi
     * @address 0x1A62C0
     * @size 0x14C
     */
    void GetRectParts(CRect_i_ *rect, CMapParts *target, int margin);

    /**
     * Gives the grid cell that one cell of a part lands on, turned the way the part faces.
     *
     * @mangled GetRectParts__11CEditGroundFP8CRect_i_P9CMapPartsii
     * @address 0x1A6410
     * @size 0x338
     */
    void GetRectParts(CRect_i_ *rect, CMapParts *target, int column, int row);

    /**
     * Gives the strip of cells a set depth deep along one side of a part, the side
     * taken relative to the way the part faces.
     *
     * @mangled GetRectDirParts__11CEditGroundFP8CRect_i_P9CMapPartsii
     * @address 0x1A6750
     * @size 0x264
     */
    void GetRectDirParts(CRect_i_ *rect, CMapParts *target, int direction, int depth);

    /**
     * Checks the ground against Norn's request.
     *
     * @mangled NornRequest__11CEditGroundFPA64_P9CMapParts
     * @address 0x1A69C0
     * @size 0x3C4
     */
    void NornRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against Matataki's request, which wants the rivers chained up.
     *
     * @mangled MatatagiRequest__11CEditGroundFPA64_P9CMapParts
     * @address 0x1A6D90
     * @size 0x65C
     */
    void MatatagiRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against the Queen's request.
     *
     * @mangled QueensRequest__11CEditGroundFPA64_P9CMapParts
     * @address 0x1A73F0
     * @size 0x49C
     */
    void QueensRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against Muska Racka's request.
     *
     * @mangled MuskaRequest__11CEditGroundFPA64_P9CMapParts
     * @address 0x1A7920
     * @size 0x614
     */
    void MuskaRequest(CMapParts *(*placed)[64]);

    /**
     * Checks the ground against the yellow request, which wants parts facing one
     * another.
     *
     * @mangled YellowRequest__11CEditGroundFPA64_P9CMapParts
     * @address 0x1A7F40
     * @size 0x8A0
     */
    void YellowRequest(CMapParts *(*placed)[64]);
};

STATIC_ASSERT(sizeof(CEditGround) == 0x20960);
