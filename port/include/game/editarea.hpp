#pragma once

/**
 * @file
 * Declares the editable town-building grid.
 */

#include "common.h"

#include "collision.hpp"

namespace dc::game {

class CFrameVu1;
class CMapParts;
class CRect_i_;
class CVector3_f_;
class CVector3_i_;

/**
 * Stores the map-part and elevation state for one editable grid cell.
 */
class CEditAreaCell {
public:
    int parts_no;    /**< Selects the map-part instance occupying the cell. */
    int altitude;    /**< Stores the cell height in vertical grid units. */
    int spare;       /**< Word Clear resets to zero; nothing reads it. */
    int parts_id;    /**< Identifies the map-part definition occupying the cell. */
    int code;        /**< Stores the map-part geometry code for the cell. */
    int parts_extra; /**< Holds the occupying part's MapPartsSubtype, or -1. */
    int spare_index; /**< Index Clear resets to -1; nothing reads it. */
};

STATIC_ASSERT(sizeof(CEditAreaCell) == 0x1C);

/**
 * Maintains the editable map grid and its world-space dimensions.
 */
class CEditArea {
public:
    int           area_id;  /**< Selects the area's special grid rules. */
    int           map_no;   /**< Identifies the map represented by the grid. */
    int           width;    /**< Gives the active grid width in cells. */
    int           height;   /**< Gives the active grid height in cells. */
    float         offset_x; /**< Gives the grid origin on the world X axis. */
    float         offset_y; /**< Gives the grid origin on the world Y axis. */
    float         offset_z; /**< Gives the grid origin on the world Z axis. */
    int           unk_1c;
    float         unit_size;          /**< Gives one cell's horizontal world-space extent. */
    float         unit_alt;           /**< Gives one elevation unit's world-space extent. */
    CEditAreaCell grid[16][16];       /**< Stores the editable cells in grid coordinates. */
    int           chain_work[16][16]; /**< Marks cells visited while following river chains. */
    CFrameVu1    *grid_frame;         /**< Holds the frame used to render the grid overlay. */
    int           unk_202c;
    CBoxVu0       parts_box;         /**< Bounds all occupied cells in world space. */
    int           grid_redraw;       /**< Set to have DrawGrid rebuild the grid overlay. */
    int           grid_redraw_count; /**< Draws left that rebuild the grid overlay. */

    /**
     * Sets the active grid dimensions and world-space unit scales.
     */
    void SetSize(int width, int height, float unit_size, float unit_alt);

    /**
     * Converts a world-space position to integral grid coordinates.
     */
    void GetPos(CVector3_i_ *position, float x, float y, float z);

    /**
     * Converts grid coordinates to a world-space position.
     */
    void GetPos(CVector3_f_ *position, int x, int y, int z);

    /**
     * Sets the map-part number stored at one grid coordinate.
     */
    void SetPartsNo(int x, int y, int parts_no);

    /**
     * Sets the map-part identifier stored at one grid coordinate.
     */
    void SetPartsID(int x, int y, int parts_id);

    /**
     * Sets the area-specific part attribute at one grid coordinate.
     */
    void SetPartsExtra(int x, int y, int parts_extra);

    /**
     * Sets the geometry code stored at one grid coordinate.
     */
    void SetCode(int x, int y, int code);

    /**
     * Returns the map-part identifier at one grid coordinate.
     */
    int GetPartsID(int x, int y);

    /**
     * Returns the geometry code at one grid coordinate.
     */
    int GetCode(int x, int y);

    /**
     * Sets a clamped integral altitude at one grid coordinate.
     */
    void SetAlt(int x, int y, int altitude);

    /**
     * Returns the integral altitude at one grid coordinate.
     */
    int GetAlt_i(int x, int y);

    /**
     * Returns the world-space altitude at one grid coordinate.
     */
    float GetAlt(int x, int y);

    /**
     * Returns the world-space altitude at one world position.
     */
    float GetAlt(float x, float y, float z);

    /**
     * Returns the integral altitude at one world position.
     */
    int GetAlt_i(float x, float y, float z);

    /**
     * Returns the area-specific part attribute at one grid coordinate.
     */
    int GetPartsExtra(int x, int y);

    /**
     * Projects one map part into the editable grid.
     */
    int SetMapParts(int parts_id, CMapParts *parts, float x, float y, float z, int rot_y);

    /**
     * Removes one map part from the editable grid.
     */
    int DeleteMapParts(int parts_no, CMapParts *parts, float x, float y, float z);

    /**
     * Determines the river connection code at one grid coordinate.
     */
    int SetRiverParts(int x, int y);

    /**
     * Determines the road connection code at one grid coordinate.
     */
    int SetRoadParts(int x, int y);

    /**
     * Returns the map-part identifier at one world position.
     */
    int SearchPartsID(float x, float y, float z);

    /**
     * Returns the area-specific part attribute at one world position.
     */
    int SearchPartsExtra(float x, float y, float z);

    /**
     * Snaps a world-space position to the editable grid.
     */
    void GetGrid(CVector3_f_ *snapped, float x, float y, float z);

    /**
     * Rebuilds the frame geometry used to draw the grid.
     */
    void RemakeGrid();

    /**
     * Copies the occupied-parts bounding box to the caller.
     */
    void GetPartsBox(CBoxVu0 *box);

    /**
     * Recomputes the occupied-parts bounding box.
     */
    void MakePartsBox();

    /**
     * Returns whether a world position lies inside the editable area.
     */
    int CheckArea(float x, float y, float z);

    /**
     * Returns whether a centered grid rectangle fits inside the area.
     */
    int CheckAreaRect(float x, float y, float z, int rect_width, int rect_height);

    /**
     * Returns whether a map part can occupy the requested world position.
     */
    int CheckParts(CMapParts *parts, float x, float y, float z, int rotation);

    /**
     * Collects collision polygons around one world position.
     */
    int PickUpPoly(CCPoly *polygons, float x, float y, float z);

    /**
     * Collects collision polygons from one grid rectangle.
     */
    int PickUpPoly(CCPoly *polygons, CRect_i_ rect);

    /**
     * Collects collision polygons overlapping one world-space box.
     */
    int PickUpPoly(CCPoly *polygons, CBoxVu0 box);

    /**
     * Collects part identifiers from one grid rectangle.
     */
    int GetPartsRect(CRect_i_ &rect, int *parts_ids, int capacity);

    /**
     * Clears the river-chain traversal marks.
     */
    void ChainWorkClear();

    /**
     * Returns whether two river cells belong to one connected chain.
     */
    int CheckRiverChain(int x, int y, int target_x, int target_y);

    /**
     * Updates and draws the editable grid overlay.
     */
    void DrawGrid();

    /**
     * Resets every active grid cell to its empty state.
     */
    void Clear();

    /**
     * Initializes the editable area to an empty 16 by 16 grid.
     */
    void Initialize();

    /**
     * Selects the map number and area identifier represented by the grid.
     */
    void SetMapInfo(int map_no, int area_id);

    /**
     * Returns the selected map number.
     */
    int GetMapNo();

    /**
     * Returns the selected area identifier.
     */
    int GetAreaID();

    /**
     * Selects the frame used to render the grid overlay.
     */
    void SetGridFrame(CFrameVu1 *frame);

    /**
     * Returns the frame used to render the grid overlay.
     */
    CFrameVu1 *GetGridFrame();

    /**
     * Sets the world-space origin of the editable grid.
     */
    void SetOffset(float x, float y, float z);

    /**
     * Copies the world-space grid origin to the caller.
     */
    void GetOffset(float *offset);

    /**
     * Returns the active grid width.
     */
    int GetWidth();

    /**
     * Returns the active grid height.
     */
    int GetHeight();

    /**
     * Adds an integral altitude delta to one grid coordinate.
     */
    void AddAlt(int x, int y, int altitude_delta);

    /**
     * Returns one cell's horizontal world-space extent.
     */
    float GetUnitSize();

    /**
     * Returns one elevation unit's world-space extent.
     */
    float GetUnitAlt();

    /**
     * Constructs an initialized editable area.
     */
    CEditArea();
};

STATIC_ASSERT(sizeof(CEditArea) == 0x2060);

} // namespace dc::game
