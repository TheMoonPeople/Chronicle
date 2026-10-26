#pragma once

#include "common.h"

#include "collision.hpp"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
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
    int parts_extra; /**< Stores the area-specific map-part attribute. */
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
     *
     * @mangled SetSize__9CEditAreaFiiff
     * @address 0x16D860
     * @size 0x18
     */
    void SetSize(int width, int height, float unit_size, float unit_alt);

    /**
     * Converts a world-space position to integral grid coordinates.
     *
     * @mangled GetPos__9CEditAreaFP11CVector3_i_fff
     * @address 0x16D880
     * @size 0xA0
     */
    void GetPos(CVector3_i_ *position, float x, float y, float z);

    /**
     * Converts grid coordinates to a world-space position.
     *
     * @mangled GetPos__9CEditAreaFP11CVector3_f_iii
     * @address 0x16D920
     * @size 0x68
     */
    void GetPos(CVector3_f_ *position, int x, int y, int z);

    /**
     * Sets the map-part number stored at one grid coordinate.
     *
     * @mangled SetPartsNo__9CEditAreaFiii
     * @address 0x16D990
     * @size 0x6C
     */
    void SetPartsNo(int x, int y, int parts_no);

    /**
     * Sets the map-part identifier stored at one grid coordinate.
     *
     * @mangled SetPartsID__9CEditAreaFiii
     * @address 0x16DA00
     * @size 0x6C
     */
    void SetPartsID(int x, int y, int parts_id);

    /**
     * Sets the area-specific part attribute at one grid coordinate.
     *
     * @mangled SetPartsExtra__9CEditAreaFiii
     * @address 0x16DA70
     * @size 0x6C
     */
    void SetPartsExtra(int x, int y, int parts_extra);

    /**
     * Sets the geometry code stored at one grid coordinate.
     *
     * @mangled SetCode__9CEditAreaFiii
     * @address 0x16DAE0
     * @size 0x6C
     */
    void SetCode(int x, int y, int code);

    /**
     * Returns the map-part identifier at one grid coordinate.
     *
     * @mangled GetPartsID__9CEditAreaFii
     * @address 0x16DB50
     * @size 0x78
     */
    int GetPartsID(int x, int y);

    /**
     * Returns the geometry code at one grid coordinate.
     *
     * @mangled GetCode__9CEditAreaFii
     * @address 0x16DBD0
     * @size 0x78
     */
    int GetCode(int x, int y);

    /**
     * Sets a clamped integral altitude at one grid coordinate.
     *
     * @mangled SetAlt__9CEditAreaFiii
     * @address 0x16DC50
     * @size 0x88
     */
    void SetAlt(int x, int y, int altitude);

    /**
     * Returns the integral altitude at one grid coordinate.
     *
     * @mangled GetAlt_i__9CEditAreaFii
     * @address 0x16DCE0
     * @size 0x78
     */
    int GetAlt_i(int x, int y);

    /**
     * Returns the world-space altitude at one grid coordinate.
     *
     * @mangled GetAlt__9CEditAreaFii
     * @address 0x16DD60
     * @size 0x88
     */
    float GetAlt(int x, int y);

    /**
     * Returns the world-space altitude at one world position.
     *
     * @mangled GetAlt__9CEditAreaFfff
     * @address 0x16DDF0
     * @size 0x44
     */
    float GetAlt(float x, float y, float z);

    /**
     * Returns the integral altitude at one world position.
     *
     * @mangled GetAlt_i__9CEditAreaFfff
     * @address 0x16DE40
     * @size 0x44
     */
    int GetAlt_i(float x, float y, float z);

    /**
     * Returns the area-specific part attribute at one grid coordinate.
     *
     * @mangled GetPartsExtra__9CEditAreaFii
     * @address 0x16DE90
     * @size 0x180
     */
    int GetPartsExtra(int x, int y);

    /**
     * Projects one map part into the editable grid.
     *
     * @mangled SetMapParts__9CEditAreaFiP9CMapPartsfffi
     * @address 0x16E010
     * @size 0x1FC
     */
    int SetMapParts(int parts_id, CMapParts *parts, float x, float y, float z, int rot_y);

    /**
     * Removes one map part from the editable grid.
     *
     * @mangled DeleteMapParts__9CEditAreaFiP9CMapPartsfff
     * @address 0x16E210
     * @size 0x25C
     */
    int DeleteMapParts(int parts_no, CMapParts *parts, float x, float y, float z);

    /**
     * Determines the river connection code at one grid coordinate.
     *
     * @mangled SetRiverParts__9CEditAreaFii
     * @address 0x16E470
     * @size 0x440
     */
    int SetRiverParts(int x, int y);

    /**
     * Determines the road connection code at one grid coordinate.
     *
     * @mangled SetRoadParts__9CEditAreaFii
     * @address 0x16E8B0
     * @size 0x32C
     */
    int SetRoadParts(int x, int y);

    /**
     * Returns the map-part identifier at one world position.
     *
     * @mangled SearchPartsID__9CEditAreaFfff
     * @address 0x16EBE0
     * @size 0x5C
     */
    int SearchPartsID(float x, float y, float z);

    /**
     * Returns the area-specific part attribute at one world position.
     *
     * @mangled SearchPartsExtra__9CEditAreaFfff
     * @address 0x16EC40
     * @size 0x44
     */
    int SearchPartsExtra(float x, float y, float z);

    /**
     * Snaps a world-space position to the editable grid.
     *
     * @mangled GetGrid__9CEditAreaFP11CVector3_f_fff
     * @address 0x16EC90
     * @size 0x58
     */
    void GetGrid(CVector3_f_ *snapped, float x, float y, float z);

    /**
     * Rebuilds the frame geometry used to draw the grid.
     *
     * @mangled RemakeGrid__9CEditAreaFv
     * @address 0x16ECF0
     * @size 0x1D8
     */
    void RemakeGrid();

    /**
     * Copies the occupied-parts bounding box to the caller.
     *
     * @mangled GetPartsBox__9CEditAreaFP7CBoxVu0
     * @address 0x16EED0
     * @size 0x30
     */
    void GetPartsBox(CBoxVu0 *box);

    /**
     * Recomputes the occupied-parts bounding box.
     *
     * @mangled MakePartsBox__9CEditAreaFv
     * @address 0x16EF00
     * @size 0x188
     */
    void MakePartsBox();

    /**
     * Returns whether a world position lies inside the editable area.
     *
     * @mangled CheckArea__9CEditAreaFfff
     * @address 0x16F090
     * @size 0xA8
     */
    int CheckArea(float x, float y, float z);

    /**
     * Returns whether a centered grid rectangle fits inside the area.
     *
     * @mangled CheckAreaRect__9CEditAreaFfffii
     * @address 0x16F140
     * @size 0xBC
     */
    int CheckAreaRect(float x, float y, float z, int rect_width, int rect_height);

    /**
     * Returns whether a map part can occupy the requested world position.
     *
     * @mangled CheckParts__9CEditAreaFP9CMapPartsfffi
     * @address 0x16F200
     * @size 0x454
     */
    int CheckParts(CMapParts *parts, float x, float y, float z, int rotation);

    /**
     * Collects collision polygons around one world position.
     *
     * @mangled PickUpPoly__9CEditAreaFP6CCPolyfff
     * @address 0x16F660
     * @size 0xC4
     */
    int PickUpPoly(CCPoly *polygons, float x, float y, float z);

    /**
     * Collects collision polygons from one grid rectangle.
     *
     * @mangled PickUpPoly__9CEditAreaFP6CCPoly8CRect_i_
     * @address 0x16F730
     * @size 0x338
     */
    int PickUpPoly(CCPoly *polygons, CRect_i_ rect);

    /**
     * Collects collision polygons overlapping one world-space box.
     *
     * @mangled PickUpPoly__9CEditAreaFP6CCPoly7CBoxVu0
     * @address 0x16FA70
     * @size 0xE0
     */
    int PickUpPoly(CCPoly *polygons, CBoxVu0 box);

    /**
     * Collects part identifiers from one grid rectangle.
     *
     * @mangled GetPartsRect__9CEditAreaFR8CRect_i_Pii
     * @address 0x16FB50
     * @size 0x150
     */
    int GetPartsRect(CRect_i_ &rect, int *parts_ids, int capacity);

    /**
     * Clears the river-chain traversal marks.
     *
     * @mangled ChainWorkClear__9CEditAreaFv
     * @address 0x16FCA0
     * @size 0x54
     */
    void ChainWorkClear();

    /**
     * Returns whether two river cells belong to one connected chain.
     *
     * @mangled CheckRiverChain__9CEditAreaFiiii
     * @address 0x16FD00
     * @size 0x1F0
     */
    int CheckRiverChain(int x, int y, int target_x, int target_y);

    /**
     * Updates and draws the editable grid overlay.
     *
     * @mangled DrawGrid__9CEditAreaFv
     * @address 0x16FEF0
     * @size 0xD0
     */
    void DrawGrid();

    /**
     * Resets every active grid cell to its empty state.
     *
     * @mangled Clear__9CEditAreaFv
     * @address 0x16FFC0
     * @size 0x98
     */
    void Clear();

    /**
     * Initializes the editable area to an empty 16 by 16 grid.
     *
     * @mangled Initialize__9CEditAreaFv
     * @address 0x170060
     * @size 0x78
     */
    void Initialize();

    /**
     * Selects the map number and area identifier represented by the grid.
     *
     * @mangled SetMapInfo__9CEditAreaFii
     * @address 0x1700E0
     * @size 0x10
     */
    void SetMapInfo(int map_no, int area_id);

    /**
     * Returns the selected map number.
     *
     * @mangled GetMapNo__9CEditAreaFv
     * @address 0x1700F0
     * @size 0xC
     */
    int GetMapNo();

    /**
     * Returns the selected area identifier.
     *
     * @mangled GetAreaID__9CEditAreaFv
     * @address 0x170100
     * @size 0xC
     */
    int GetAreaID();

    /**
     * Selects the frame used to render the grid overlay.
     *
     * @mangled SetGridFrame__9CEditAreaFP9CFrameVu1
     * @address 0x170110
     * @size 0xC
     */
    void SetGridFrame(CFrameVu1 *frame);

    /**
     * Returns the frame used to render the grid overlay.
     *
     * @mangled GetGridFrame__9CEditAreaFv
     * @address 0x170120
     * @size 0xC
     */
    CFrameVu1 *GetGridFrame();

    /**
     * Sets the world-space origin of the editable grid.
     *
     * @mangled SetOffset__9CEditAreaFfff
     * @address 0x170130
     * @size 0x14
     */
    void SetOffset(float x, float y, float z);

    /**
     * Copies the world-space grid origin to the caller.
     *
     * @mangled GetOffset__9CEditAreaFPf
     * @address 0x170150
     * @size 0x20
     */
    void GetOffset(float *offset);

    /**
     * Returns the active grid width.
     *
     * @mangled GetWidth__9CEditAreaFv
     * @address 0x170170
     * @size 0xC
     */
    int GetWidth();

    /**
     * Returns the active grid height.
     *
     * @mangled GetHeight__9CEditAreaFv
     * @address 0x170180
     * @size 0xC
     */
    int GetHeight();

    /**
     * Adds an integral altitude delta to one grid coordinate.
     *
     * @mangled AddAlt__9CEditAreaFiii
     * @address 0x170190
     * @size 0x68
     */
    void AddAlt(int x, int y, int altitude_delta);

    /**
     * Returns one cell's horizontal world-space extent.
     *
     * @mangled GetUnitSize__9CEditAreaFv
     * @address 0x170200
     * @size 0xC
     */
    float GetUnitSize();

    /**
     * Returns one elevation unit's world-space extent.
     *
     * @mangled GetUnitAlt__9CEditAreaFv
     * @address 0x170210
     * @size 0xC
     */
    float GetUnitAlt();

    /**
     * Constructs an initialized editable area.
     *
     * @mangled __ct__9CEditAreaFv
     * @address 0x170220
     * @size 0x30
     */
    CEditArea();
};

STATIC_ASSERT(sizeof(CEditArea) == 0x2060);
