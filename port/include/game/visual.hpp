#pragma once

#include "common.h"

class sceVif1Packet;

namespace dc::game {

class RenderInfo;
struct MDT_MATERIAL;

/**
 * Which of the Vector Unit's drawing modes a visual is drawn in. Only the value a frame passes is
 * named.
 */
enum VU1_PROGRAM {
    VU1_PROGRAM_UNKNOWN6 = 6
};

/**
 * The drawable half of a scene node: a frame carries the transform and a visual the geometry.
 * This base is never constructed directly; it holds two words, the vtable pointer and one more
 * word.
 */
class CVisual {
public:
    int unk_00;
    int unk_04;

    /**
     * Copies the visual's scalar state and returns this visual.
     */
    CVisual &operator=(const CVisual &other);

    /**
     * Constructs a visual and clears it.
     */
    CVisual();

    /**
     * Clears the visual's draw flags.
     */
    virtual void Initialize();

    int unk_0C;
};

/**
 * Geometry the Vector Unit draws from a data block built once at load time. The loader reads back
 * the block's address and size to reserve exactly as much as was written.
 */
class CVisualVu1 : public CVisual {
public:
    int    unk_00;
    int    unk_04;
    u_int *vu_data; /**< Vector Unit command data built for this visual. */
    u_int  vu_size; /**< Size of the Vector Unit data in quadwords. */

    /**
     * Constructs a vector-unit visual and clears it.
     */
    CVisualVu1();

    /**
     * Builds a model's VU data block and returns its size in quadwords.
     */
    int CreateVUdataFromMDT(u_int *block, u_int *data, int unknown0, int unknown1);

    /**
     * Rebuilds a VU data block from retained model data and returns its size in quadwords.
     */
    int CreateVUdataFromMDTRemake(u_int *block, u_int *data, int unknown0);

    // Declaration order fixes the vtable slot each call site reaches.
    /**
     * Clears the vector-unit visual's packet pointers and sizes.
     */
    void           Initialize() override;
    virtual void   SetMDTDataAddress(u_int *data);
    virtual u_int *GetMDTDataAddress();
    /**
     * Returns zero because this visual has no retained model data to rebuild.
     */
    virtual int RemakeData(u_int *data);
    virtual int DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);
    /**
     * Draws the visual into a VIF packet and reserves the words it wrote.
     */
    virtual int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);
};

/**
 * A vector-unit visual that keeps the model file beside the built block so the geometry can be
 * rebuilt without the loader. Both vu_data_buffer entries are the same address unless a second
 * block was built for the model.
 */
class CVisualMDTVu1 : public CVisualVu1 {
public:
    u_int *data;              /**< Retained source MDT image used to rebuild the visual. */
    int    copy_on_draw;      /**< Whether each draw works from a copy of the built block in the frame's data arena. */
    u_int *vu_data_buffer[2]; /**< Built block each display buffer draws from. */

    /**
     * Constructs a model visual and clears it.
     */
    CVisualMDTVu1();

    /**
     * Clears the model visual's data pointer and packet buffers.
     */
    void   Initialize() override;
    void   SetMDTDataAddress(u_int *data) override;
    u_int *GetMDTDataAddress() override;
    /**
     * Rebuilds VU data and returns its size in quadwords, or zero if no MDT model is retained.
     */
    int RemakeData(u_int *data) override;

    /**
     * Draws the model, from a copy of this frame's block when the visual asks for one.
     */
    int DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2) override;

    /**
     * Draws the model into a VIF packet, choosing the buffer the frame is using.
     */
    int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2) override;
};

/**
 * The silhouette caster. It adds no field of its own and differs from its base in how its block
 * is built and in the edge pass that runs beside it.
 */
class CVisualShadow : public CVisualMDTVu1 {
public:
    /**
     * Rebuilds shadow VU data and returns its size in quadwords, or zero if no MDT model is retained.
     */
    int RemakeData(u_int *block) override;

    /**
     * Draws the model, building and lending it a clipped shadow packet on the shadow pass.
     */
    int DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2) override;

    /**
     * Draws the model into a VIF packet, building and lending it a clipped shadow packet.
     */
    int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2) override;

    /**
     * Writes one model's shadow geometry and returns its size in quadwords.
     */
    int CreateVUdataShadow(u_int *block, u_int *model_data);

    /**
     * Writes visible shadow geometry and returns its size in quadwords.
     */
    int CreateVUdataShadowCLIP(u_int *block, u_int *model_data, RenderInfo *info, float (*matrix)[4]);
};

} // namespace dc::game
