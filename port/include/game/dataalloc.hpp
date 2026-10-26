#pragma once

#include "common.h"

#include <cstdio>

#include "dataalloc_fwd.hpp"

/**
 * Constructs an object inside allocator-owned quadword storage.
 */
void *operator new(size_t size, u_long128 *block);
/**
 * Constructs an array inside allocator-owned quadword storage.
 */
void *operator new[](size_t size, u_long128 *block);

/**
 * Allocates sixteen-byte units from an externally supplied buffer.
 */
template <int Kind>
class CDataAlloc2 {
public:
    u_char *base;   /**< Start of the allocator-owned storage. */
    u_char *buffer; /**< Storage the constructor starts the arena's base at. */
    int     used;   /**< Number of occupied quadwords. */
    int     limit;  /**< Maximum number of available quadwords. */

    CDataAlloc2(int limit = -1);

    u_char *Alloc(int quads);
    u_char *Alloc64(int quads);
    void    Align64();

    /** Hands the whole arena back, without touching what is in it. */
    void Reset() { used = 0; }
};

/**
 * Allocates sixteen-byte units from storage embedded in the allocator.
 */
template <int Kind, int Size>
class CDataAlloc {
public:
    u_long128 block[Size]; /**< Embedded allocation storage. */
    int       used;        /**< Number of occupied quadwords. */

    CDataAlloc() { used = 0; }

    u_char *Alloc(int quads);
    u_char *Alloc64(int quads);
    void    Align64();
};

template <>
class CDataAlloc2<1> {
public:
    u_char *base;   /**< Start of the allocator-owned storage. */
    u_char *buffer; /**< Storage the constructor starts the arena's base at. */
    int     used;   /**< Number of occupied quadwords. */
    int     limit;  /**< Maximum number of available quadwords. */

    CDataAlloc2();
    CDataAlloc2(int limit);

    u_char *Alloc(int quads);
    u_char *Alloc64(int quads);
    void    Align64();

    /** Hands the whole arena back, without touching what is in it. */
    void Reset() { used = 0; }
};

template <>
class CDataAlloc<1, 6000> {
public:
    u_long128 block[6000]; /**< Embedded allocation storage. */
    int       used;        /**< Number of occupied quadwords in the embedded arena. */

    CDataAlloc();

    u_char *Alloc(int quads);
    u_char *Alloc64(int quads);
    void    Align64();
};

template <>
class CDataAlloc<1, 1690000> {
public:
    u_long128 block[1690000]; /**< Embedded allocation storage. */
    int       used;           /**< Number of occupied quadwords. */

    CDataAlloc() { used = 0; }

    u_char *Alloc(int quads);
    u_char *Alloc64(int quads);
    void    Align64();
};

template <int Kind, int Size>
u_char *CDataAlloc<Kind, Size>::Alloc(int quads) {
    u_char *allocation;

    if (used + quads > Size) {
        printf("Alocation Error! %d/%d\n", used, Size);

        while (1)
            ;
    }

    allocation = (u_char *) block + used * 16;
    used += quads;
    return allocation;
}

template <int Kind, int Size>
u_char *CDataAlloc<Kind, Size>::Alloc64(int quads) {
    Align64();

    u_char *allocation = (u_char *) block + used * 16;
    used += quads;

    if (used >= Size) {
        printf("Alocation Error! %d/%d\n", used, Size);

        while (1)
            ;
    }

    return allocation;
}


/**
 * Supplies the main texture-loading allocation arena.
 */
extern CDataAlloc2<1> TextureData;
/**
 * Supplies the main visual-model allocation arena.
 */
extern CDataAlloc2<1> VisualData;
/**
 * Supplies the main motion-data allocation arena.
 */
extern CDataAlloc2<1> MotionData;
/**
 * Supplies the water-surface allocation arena.
 */
extern CDataAlloc2<1> WaterData;
/**
 * Supplies the miscellaneous-data allocation arena.
 */
extern CDataAlloc2<1> VariousData;
/**
 * Supplies the first of the two alternating per-frame VU data arenas.
 */
extern CDataAlloc2<1> ActiveData0;
/**
 * Supplies the second of the two alternating per-frame VU data arenas.
 */
extern CDataAlloc2<1> ActiveData1;
/**
 * Holds the storage that every scene arena is carved out of.
 */
extern CDataAlloc<1, 1690000> GlobalDataBuffer;
/**
 * Backs the shared scratch allocator that WorkBuffer points to.
 */
extern CDataAlloc2<1> workbuffer;

/** Buffer the common-menu effect models are read into. */
extern CDataAlloc2<1> MenuEffectCashBuffer;
