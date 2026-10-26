#pragma once

#include "common.h"

#include "collision.hpp"
#include "mapobject.hpp"

/**
 * @file
 * Declares one part that an edited map is built from.
 */

/**
 * What a map part is, as CMapParts::subtype holds it.
 */
// clang-format off
enum MapPartsSubtype {
    MAP_PARTS_SUBTYPE_NONE     = 0, /**< Building or ground part. */
    MAP_PARTS_SUBTYPE_ROAD     = 1, /**< Road. */
    MAP_PARTS_SUBTYPE_RIVER    = 2, /**< River. */
    MAP_PARTS_SUBTYPE_BRIDGE   = 3, /**< Bridge. */
    MAP_PARTS_SUBTYPE_LAKE     = 4, /**< Lake. */
    MAP_PARTS_SUBTYPE_ON_RIVER = 5, /**< Part that stands on a river. */
};

// clang-format on

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
class CCamera;
class CEffectGroup;
struct EDIT_EFFECT_INFO;
struct EDITPARTS_INFO;

/**
 * Number of effects that one map part can carry.
 */
#define MAP_PARTS_EFFECT_MAX 24

/**
 * Draws one part of an edited map, together with the effects that stand on
 * it, and answers which cells of the grid the part covers.
 *
 * A part faces one of the four compass directions, and the grid that its
 * definition holds is read through that direction, so that one definition
 * serves all four.
 */
class CMapParts : public CMapObject {
public:
    s32             parts_no;              /**< Plot the part stands on, as the part catalogue numbers it. */
    s32             area;                  /**< Area of the ground the part stands in, or -1. */
    s32             rot_y;                 /**< Quarter turns about the vertical axis that the part faces. */
    s32             preview_frame;         /**< Frame drawn as the placement preview where the part fits: its finest level of detail. */
    s32             blocked_preview_frame; /**< Frame drawn as the placement preview where the part does not fit. */
    CFrame         *ripple_frame;          /**< Extra model drawn with the part, such as a river's ripples; zero where the part has none. */
    EDITPARTS_INFO *info;                  /**< Definition of the part; zero until a map gives it one. */
    s32             func_count;            /**< Number of function points the part's definition holds. */
    s32             func_data;             /**< Function points of the part's definition. */
    s32             def_parts_no;          /**< Part number that the part's definition gives. */
    s32             subtype;               /**< Subtype of the part. @see MapPartsSubtype. */
    float           lift;                  /**< Scale of the height the part is raised by to keep it from cutting into the ground; above 1 raises it by a fixed step. */
    float           draw_distance;         /**< Distance beyond which the part is not drawn; zero or below to draw it at any distance. */
    s32             unk_124;
    s32             unk_128;
    s32             unk_12C;
    CBoxVu0         bound; /**< Extremes of everything the part draws, in its own space. */
    u8              unk_150[128];
    float           unit_size; /**< Width of one grid cell of the area the part stands on. */
    s32             unk_1D4;
    /** 1 where the slot carries an effect; below zero where no slot after it does. */
    s32               effect_on[MAP_PARTS_EFFECT_MAX];
    EDIT_EFFECT_INFO *effect[MAP_PARTS_EFFECT_MAX]; /**< Effect of each slot. */

    /**
     * Gets the world position of the part.
     *
     * @mangled GetPosition__9CMapPartsFPf
     * @address 0x19A7B0
     * @size 0x2C
     */
    void GetPosition(float *out_position);

    /**
     * Puts the part at a world position.
     *
     * @mangled SetPosition__9CMapPartsFPf
     * @address 0x19A7E0
     * @size 0x24
     */
    virtual void SetPosition(float *position);

    /**
     * Puts the part at a world position.
     *
     * @mangled SetPosition__9CMapPartsFfff
     * @address 0x19A810
     * @size 0x14
     */
    virtual void SetPosition(float x, float y, float z);

    /**
     * Turns the part to an angle about each axis.
     *
     * @mangled SetRotation__9CMapPartsFfff
     * @address 0x19A830
     * @size 0x14
     */
    virtual void SetRotation(float x, float y, float z);

    /**
     * Turns the part to face one of the four compass directions.
     *
     * @mangled SetRotY__9CMapPartsFi
     * @address 0x19A850
     * @size 0x5C
     */
    void SetRotY(int direction);

    /**
     * Gets the compass direction that the part faces.
     *
     * @mangled GetRotY__9CMapPartsFv
     * @address 0x19A8B0
     * @size 0xC
     */
    int GetRotY();

    /**
     * Puts the part at rest at the world origin, with no definition and no
     * effect.
     *
     * @mangled Initialize__9CMapPartsFv
     * @address 0x19A8C0
     * @size 0x9C
     */
    void Initialize();

    /**
     * Makes a part that a map has yet to define.
     *
     * @mangled __ct__9CMapPartsFv
     * @address 0x19A960
     * @size 0x48
     */
    CMapParts();

    /**
     * Turns a named part of every frame the part holds on or off.
     *
     * @mangled FrameObjectOnOff__9CMapPartsFPci
     * @address 0x19A9B0
     * @size 0x68
     */
    virtual void FrameObjectOnOff(char *name, int on);

    /**
     * Gets the number of grid cells that the part covers from west to east.
     *
     * @mangled GetWidth__9CMapPartsFv
     * @address 0x19AA20
     * @size 0x58
     */
    int GetWidth();

    /**
     * Gets the number of grid cells that the part covers from north to south.
     *
     * @mangled GetHeight__9CMapPartsFv
     * @address 0x19AA80
     * @size 0x58
     */
    int GetHeight();

    /**
     * Reads the definition of one grid cell of the part, through the
     * direction that the part faces.
     *
     * @mangled GetInfoData__9CMapPartsFii
     * @address 0x19AAE0
     * @size 0x160
     */
    int GetInfoData(int x, int y);

    /**
     * Gives back 1 while one cell of the part accepts a change of height.
     *
     * @mangled ChangeAltData__9CMapPartsFv
     * @address 0x19AC40
     * @size 0x98
     */
    int ChangeAltData();

    /**
     * Gives back 1 while one cell of the part accepts digging.
     *
     * @mangled ChangeDigData__9CMapPartsFv
     * @address 0x19ACE0
     * @size 0x98
     */
    int ChangeDigData();

    /**
     * Gives back 1 while the part meets a box.
     *
     * @mangled CheckBox__9CMapPartsFP7CBoxVu0
     * @address 0x19AD80
     * @size 0x134
     */
    int CheckBox(CBoxVu0 *box);

    /**
     * Gives back 1 while the part meets a box, taking every corner of the
     * part into account.
     *
     * @mangled CheckBox2__9CMapPartsFP7CBoxVu0
     * @address 0x19AEC0
     * @size 0x1D8
     */
    int CheckBox2(CBoxVu0 *box);

    /**
     * Draws the part at the level of detail that suits its distance from the
     * eye, lifted by the height that its definition asks for.
     *
     * @mangled DrawLOD__9CMapPartsFPfiiPi
     * @address 0x19B0A0
     * @size 0x1B8
     */
    virtual void DrawLOD(float *distance, int lowest, int highest, int *out_level);

    /**
     * Draws the part with the lights that the effects standing on it give.
     *
     * @mangled DrawParts__9CMapPartsFfPfiiPi
     * @address 0x19B260
     * @size 0x2EC
     */
    void DrawParts(float time, float *distance, int lowest, int highest, int *out_level);

    /**
     * Draws every effect that stands on the part.
     *
     * @mangled DrawEffect__9CMapPartsFP7CCamerafP12CEffectGroup
     * @address 0x19B550
     * @size 0x130
     */
    void DrawEffect(CCamera *camera, float time, CEffectGroup *group);
};

STATIC_ASSERT(sizeof(CMapParts) == 0x2A0);
