#pragma once

#include "common.h"

#include <libvu0.h>

#include "dataalloc.hpp"
#include "mdt.hpp"
#include "visualvu1.hpp"

class CBound;
class CFrame;

/**
 * Simulates and draws a fixed-capacity grid of cloth vertices.
 */
class CCloth : public CVisualVu1 {
public:
    int           unk_10;
    int           unk_14;
    u_int        *vu_data;               /**< VU packet the base class draws, pointed at this frame's buffer. */
    u_int         vu_size;               /**< Size of one VU packet buffer in quadwords. */
    int           init_20;               /**< Cleared when the parameters reset and never read. */
    u_int        *vu_block[2];           /**< VU packet data for each display buffer. */
    int           num_i;                 /**< Number of active grid rows. */
    int           num_j;                 /**< Number of active grid columns. */
    float         pitch;                 /**< Rest spacing between adjacent vertices. */
    int           stop;                  /**< Carries the vertices rigidly with the frame, without simulation, while set. */
    CFrame       *frame;                 /**< Frame from which the cloth hangs. */
    int           init_40;               /**< Cleared when the parameters reset and never read. */
    CBound       *bound;                 /**< First exclusion box in the chain the vertices are pushed out of. */
    int           floor_on;              /**< Whether the vertices stop at the floor height. */
    float         floor_y;               /**< Height of the floor the vertices rest on. */
    void         *wind;                  /**< Wind whose noise pushes the cloth, or null. */
    float         wind_effect;           /**< Scale applied to the wind's push. */
    float         normal_scale;          /**< Scale applied to each rebuilt normal before it is normalised. */
    MDT_MATERIAL  material;              /**< Material the cloth draws with, taken from its model's mesh. */
    sceVu0FVECTOR gravity;               /**< Pull added to every vertex's speed each step. */
    sceVu0FVECTOR follow;                /**< Share of the frame's movement each axis passes on to the vertices. */
    sceVu0FVECTOR stiffness;             /**< Strength of the spring pulling each vertex towards its rest position on the frame. */
    sceVu0FVECTOR last_position;         /**< World position of the anchor on the previous step. */
    sceVu0FVECTOR position;              /**< Anchor point in the frame's space: the average rest position of the first row. */
    sceVu0FVECTOR home[16][16];          /**< Rest position of each vertex in the frame's space. */
    sceVu0FVECTOR point[16][16];         /**< World position of each vertex. */
    sceVu0FVECTOR last[16][16];          /**< World position of each vertex on the previous step. */
    sceVu0FVECTOR rest[16][16];          /**< Rest lengths to the neighbours in x and y, and in w the friction of the boxes touched, or -1. */
    sceVu0FVECTOR speed[16][16];         /**< Velocity of each vertex. */
    sceVu0FVECTOR normal_grid[16][16];   /**< Normal of each vertex. */
    sceVu0FVECTOR texture_coord[16][16]; /**< Texture coordinate of each vertex. */
    int           polygon_divide[16];    /**< Whether each row draws its strip from the far side. */
    int           mask[16][16];          /**< Exclusion boxes, by mask bit, that each vertex collides with. */
    sceVu0FVECTOR world_home[16][16];    /**< Rest position of each vertex carried into world space by the frame this step. */

    /**
     * Draws the simulated cloth through a temporary world-space frame.
     *
     * @mangled Draw__6CClothFv
     * @address 0x13B640
     * @size 0x160
     */
    void Draw();

    void Clear();
    void Step(int step);
    int  CreateVUData(u_int *packet);
    void InitParam();

    /**
     * Constructs a cloth grid with the requested dimensions and spacing.
     *
     * @mangled __ct__6CClothFiif
     * @address 0x13CB70
     * @size 0x78
     */
    CCloth(int grid_i = 16, int grid_j = 16, float grid_pitch = 1.0f);

    virtual void Initialize(CDataAlloc2<1> *alloc);
    virtual void Initialize(MDT_HEADER *header, CDataAlloc2<1> *alloc);
    /**
     * Rebuilds the cloth's packet for this frame and draws it through the vector unit.
     *
     * @mangled DrawVu1__6CClothFPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x13C470
     * @size 0xC0
     */
    virtual int DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);
    virtual int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);
};
