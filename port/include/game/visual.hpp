#pragma once

#include "common.h"

class RenderInfo;
class sceVif1Packet;
struct MDT_MATERIAL;

/**
 * Which of the Vector Unit's drawing modes a visual is drawn in. The image states the type — the
   draw entry points carry it by value — and nothing states its enumerators, so the only one named
   here is the value a frame passes.
 */
enum VU1_PROGRAM {
    VU1_PROGRAM_UNKNOWN6 = 6
};

/**
 * The drawable half of a scene node. A frame carries the transform and one of these carries the
   geometry, so the same model file can be attached to any frame and the same frame can hold any of
   the three kinds below. The unit that defines them is not reconstructed yet, so each class here
   states its extent, the members the model loader reaches, and nothing else. The extents are the
   sizes the loader's own allocations name rather than sizes anybody chose.
 */

/**
 * The base nothing is constructed from directly: two words of its own, the vtable pointer where
   the first virtual declaration puts it, and one word behind it.
 */
class CVisual {
public:
    int unk_00;
    int unk_04;

    /**
     * Copies the visual's scalar state and returns this visual.
     *
     * @mangled __as__7CVisualFRC7CVisual
     * @address 0x1433F0
     * @size 0x1C
     */
    CVisual &operator=(const CVisual &other);

    /**
     * Constructs a visual and clears it.
     *
     * @mangled __ct__7CVisualFv
     * @address 0x134B60
     * @size 0x44
     */
    CVisual();

    /**
     * Clears the visual's draw flags.
     *
     * @mangled Initialize__7CVisualFv
     * @address 0x134B50
     * @size 0x10
     */
    virtual void Initialize();

    int unk_0C;
};

/**
 * Geometry the Vector Unit draws from a data block built once at load time. The address of that
   block and its size in quadwords are what the loader reads back after asking for it, so that it
   can reserve exactly as much as was written and account for it.
 */
class CVisualVu1 : public CVisual {
public:
    int    unk_00;
    int    unk_04;
    u_int *vu_data; /**< Vector Unit command data built for this visual. */
    u_int  vu_size; /**< Size of the Vector Unit data in quadwords. */

    /**
     * Constructs a vector-unit visual and clears it.
     *
     * @mangled __ct__10CVisualVu1Fv
     * @address 0x134F00
     * @size 0x50
     */
    CVisualVu1();

    /**
     * Builds a model's VU data block and returns its size in quadwords.
     *
     * @mangled CreateVUdataFromMDT__10CVisualVu1FPUiPUiii
     * @address 0x135AA0
     * @size 0x3A8
     */
    int CreateVUdataFromMDT(u_int *block, u_int *data, int unknown0, int unknown1);

    /**
     * Rebuilds a VU data block from retained model data and returns its size in quadwords.
     *
     * @mangled CreateVUdataFromMDTRemake__10CVisualVu1FPUiPUii
     * @address 0x135E50
     * @size 0x288
     */
    int CreateVUdataFromMDTRemake(u_int *block, u_int *data, int unknown0);

    /**
 * The one it takes over from its base and the five it adds, in the order the vtable holds them —
       which is what decides the slot each call site reaches, so the order is evidence rather than
       layout.
 */
    /**
     * Clears the vector-unit visual's packet pointers and sizes.
     *
     * @mangled Initialize__10CVisualVu1Fv
     * @address 0x134EC0
     * @size 0x3C
     */
    virtual void   Initialize();
    virtual void   SetMDTDataAddress(u_int *data);
    virtual u_int *GetMDTDataAddress();
    /**
     * Returns zero because this visual has no retained model data to rebuild.
     *
     * @mangled RemakeData__10CVisualVu1FPUi
     * @address 0x134BB0
     * @size 0xC
     */
    virtual int RemakeData(u_int *data);
    virtual int DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);
    /**
     * Draws the visual into a VIF packet and reserves the words it wrote.
     *
     * @mangled DrawVu1__10CVisualVu1FP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x134BC0
     * @size 0xC4
     */
    virtual int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);
};

/**
 * The same with a copy of the model file kept beside the built block, which is what lets the
   geometry be rebuilt later without the loader. Both vu_data_buffer entries are the same
   address unless a second block was built for the model, and then they are the two of them.
 */
class CVisualMDTVu1 : public CVisualVu1 {
public:
    u_int *data;              /**< Retained source MDT image used to rebuild the visual. */
    int    copy_on_draw;      /**< Whether each draw works from a copy of the built block in the frame's data arena. */
    u_int *vu_data_buffer[2]; /**< Built block each display buffer draws from. */

    /**
     * Constructs a model visual and clears it.
     *
     * @mangled __ct__13CVisualMDTVu1Fv
     * @address 0x134FB0
     * @size 0x48
     */
    CVisualMDTVu1();

    /* Every one of them, because a block that is kept can be rebuilt and one that is not cannot. */
    /**
     * Clears the model visual's data pointer and packet buffers.
     *
     * @mangled Initialize__13CVisualMDTVu1Fv
     * @address 0x134F70
     * @size 0x3C
     */
    virtual void   Initialize();
    virtual void   SetMDTDataAddress(u_int *data);
    virtual u_int *GetMDTDataAddress();
    /**
     * Rebuilds VU data and returns its size in quadwords, or zero if no MDT model is retained.
     *
     * @mangled RemakeData__13CVisualMDTVu1FPUi
     * @address 0x136240
     * @size 0x4C
     */
    virtual int RemakeData(u_int *data);

    /**
     * Draws the model, from a copy of this frame's block when the visual asks for one.
     *
     * @mangled DrawVu1__13CVisualMDTVu1FPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x1360E0
     * @size 0x120
     */
    virtual int DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);

    /**
     * Draws the model into a VIF packet, choosing the buffer the frame is using.
     *
     * @mangled DrawVu1__13CVisualMDTVu1FP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x136200
     * @size 0x34
     */
    virtual int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);
};

/**
 * The silhouette caster. It adds no field of its own — the loader allocates it at its base class's
   size — and differs in how its block is built and in the edge pass that runs beside it.
 */
class CVisualShadow : public CVisualMDTVu1 {
public:
    /* The three the silhouette builds differently; how the block is kept is its base's business,
       so the three that answer that are the ones it leaves alone. */
    /**
     * Rebuilds shadow VU data and returns its size in quadwords, or zero if no MDT model is retained.
     *
     * @mangled RemakeData__13CVisualShadowFPUi
     * @address 0x136840
     * @size 0x48
     */
    virtual int RemakeData(u_int *block);

    /**
     * Draws the model, building and lending it a clipped shadow packet on the shadow pass.
     *
     * @mangled DrawVu1__13CVisualShadowFPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x136290
     * @size 0x14C
     */
    virtual int DrawVu1(u_int *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);

    /**
     * Draws the model into a VIF packet, building and lending it a clipped shadow packet.
     *
     * @mangled DrawVu1__13CVisualShadowFP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x1363E0
     * @size 0x144
     */
    virtual int DrawVu1(sceVif1Packet *packet, float (*matrix)[4], RenderInfo *info, VU1_PROGRAM program, u_long128 *draw_state, int unknown1, int unknown2);

    /**
     * Writes one model's shadow geometry and returns its size in quadwords.
     *
     * @mangled CreateVUdataShadow__13CVisualShadowFPUiPUi
     * @address 0x136530
     * @size 0x304
     */
    int CreateVUdataShadow(u_int *block, u_int *model_data);

    /**
     * Writes visible shadow geometry and returns its size in quadwords.
     *
     * @mangled CreateVUdataShadowCLIP__13CVisualShadowFPUiPUiP10RenderInfoPA4_f
     * @address 0x136890
     * @size 0xB48
     */
    int CreateVUdataShadowCLIP(u_int *block, u_int *model_data, RenderInfo *info, float (*matrix)[4]);
};

/**
 * Writes material lighting data when supplied and returns the reserved packet length in words.
 *
 * @mangled SetMaterial__FPUiP12MDT_MATERIAL
 * @address 0x134D40
 * @size 0x74
 */
int SetMaterial(u_int *packet, MDT_MATERIAL *material);

/**
 * Writes the GS texture-register packet and returns its length in words.
 *
 * @mangled SetTEX0__FPUiUlUl
 * @address 0x134DC0
 * @size 0xFC
 */
int SetTEX0(u_int *packet, u_long tex0, u_long tex1);
