#pragma once

#include "common.h"

#include "vu1.hpp"

// Forward declarations for the types these declarations name. The skeleton
// headers are generated from the retail symbol table, which knows the type
// names but not where they live.
struct RenderInfo;
struct sceVif1Packet;

class CVisualMDTVu1 {
public:
    /**
     * @mangled SetMDTDataAddress__13CVisualMDTVu1FPUi
     * @address 0x134F50
     * @size 0xC
     * @unknownret
     */
    void SetMDTDataAddress(unsigned int *);

    /**
     * @mangled GetMDTDataAddress__13CVisualMDTVu1Fv
     * @address 0x134F60
     * @size 0xC
     * @unknownret
     */
    void GetMDTDataAddress();

    /**
     * @mangled Initialize__13CVisualMDTVu1Fv
     * @address 0x134F70
     * @size 0x3C
     * @unknownret
     */
    void Initialize();

    /**
     * @mangled __ct__13CVisualMDTVu1Fv
     * @address 0x134FB0
     * @size 0x48
     */
    CVisualMDTVu1();

    /**
     * @mangled DrawVu1__13CVisualMDTVu1FPUiPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x1360E0
     * @size 0x120
     * @unknownret
     */
    void DrawVu1(unsigned int *, float (*)[4], RenderInfo *, VU1_PROGRAM, RenderInfo *, int, int);

    /**
     * @mangled DrawVu1__13CVisualMDTVu1FP13sceVif1PacketPA4_fP10RenderInfo11VU1_PROGRAMP1ii
     * @address 0x136200
     * @size 0x34
     * @unknownret
     */
    void DrawVu1(sceVif1Packet *, float (*)[4], RenderInfo *, VU1_PROGRAM, sceVif1Packet *, int, int);

    /**
     * Rebuilds VU data and returns its size in quadwords, or zero if no MDT model is retained.
     *
     * @mangled RemakeData__13CVisualMDTVu1FPUi
     * @address 0x136240
     * @size 0x4C
     */
    int RemakeData(unsigned int *);
};
