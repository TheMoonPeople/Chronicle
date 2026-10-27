#pragma once

#include "common.h"

#include <cstddef>
#include <cstdio>

#include "dataalloc_fwd.hpp"

/**
 * Constructs an object inside allocator-owned quadword storage.
 */
void *operator new(std::size_t size, u_long128 *block);

/**
 * Constructs an array inside allocator-owned quadword storage.
 */
void *operator new[](std::size_t size, u_long128 *block);

namespace dc::game {

/**
 * Backs the placement operators: returns block, first growing it to size bytes when it is still the
 * last run a CDataAlloc2<1> handed out, so nothing allocated afterwards lies under the object's tail.
 */
void *PlaceDataAllocObject(std::size_t size, u_long128 *block);

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

    /**
     * Creates an empty arena; a negative limit leaves the base unset.
     */
    CDataAlloc2(int limit = -1);

    /**
     * Reserves the given number of quadwords and returns their start.
     */
    u_char *Alloc(int quads);

    /**
     * Aligns to 64 bytes, then reserves the given number of quadwords.
     */
    u_char *Alloc64(int quads);

    /**
     * Rounds the used count up to the next 64-byte boundary.
     */
    void Align64();

    /**
     * Hands the whole arena back, without touching what is in it.
     */
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

    /**
     * Creates an empty arena.
     */
    CDataAlloc() : used(0) {}

    /**
     * Reserves the given number of quadwords and returns their start.
     */
    u_char *Alloc(int quads);

    /**
     * Aligns to 64 bytes, then reserves the given number of quadwords.
     */
    u_char *Alloc64(int quads);

    /**
     * Rounds the used count up to the next 64-byte boundary.
     */
    void Align64();
};

/**
 * Specialisation behind the game's scene arenas.
 */
template <>
class CDataAlloc2<1> {
public:
    u_char *base;   /**< Start of the allocator-owned storage. */
    u_char *buffer; /**< Storage the constructor starts the arena's base at. */
    int     used;   /**< Number of occupied quadwords. */
    int     limit;  /**< Maximum number of available quadwords. */

    /**
     * Creates an empty arena with no limit and the base unset.
     */
    CDataAlloc2();

    /**
     * Creates an empty arena; a negative limit leaves the base unset.
     */
    CDataAlloc2(int limit);

    /**
     * Reserves the given number of quadwords and returns their start.
     */
    u_char *Alloc(int quads);

    /**
     * Aligns to 64 bytes, then reserves the given number of quadwords.
     */
    u_char *Alloc64(int quads);

    /**
     * Rounds the used count up to the next 64-byte boundary.
     */
    void Align64();

    /**
     * Hands the whole arena back, without touching what is in it.
     */
    void Reset() { used = 0; }
};

/**
 * Embedded arena behind SystemMesBuffer.
 */
template <>
class CDataAlloc<1, 6000> {
public:
    u_long128 block[6000]; /**< Embedded allocation storage. */
    int       used;        /**< Number of occupied quadwords in the embedded arena. */

    /**
     * Creates an empty arena.
     */
    CDataAlloc();

    /**
     * Reserves the given number of quadwords and returns their start.
     */
    u_char *Alloc(int quads);

    /**
     * Aligns to 64 bytes, then reserves the given number of quadwords.
     */
    u_char *Alloc64(int quads);

    /**
     * Rounds the used count up to the next 64-byte boundary.
     */
    void Align64();
};

/**
 * Embedded arena behind GlobalDataBuffer.
 */
template <>
class CDataAlloc<1, 1690000> {
public:
    u_long128 block[1690000]; /**< Embedded allocation storage. */
    int       used;           /**< Number of occupied quadwords. */

    /**
     * Creates an empty arena.
     */
    CDataAlloc() : used(0) {}

    /**
     * Reserves the given number of quadwords and returns their start.
     */
    u_char *Alloc(int quads);

    /**
     * Aligns to 64 bytes, then reserves the given number of quadwords.
     */
    u_char *Alloc64(int quads);

    /**
     * Rounds the used count up to the next 64-byte boundary.
     */
    void Align64();
};

template <int Kind, int Size>
u_char *CDataAlloc<Kind, Size>::Alloc(int quads) {
    if (used + quads > Size) {
        std::printf("Alocation Error! %d/%d\n", used, Size);

        for (;;) {
        }
    }

    u_char *allocation = reinterpret_cast<u_char *>(&block[used]);
    used += quads;
    return allocation;
}

template <int Kind, int Size>
u_char *CDataAlloc<Kind, Size>::Alloc64(int quads) {
    Align64();

    u_char *allocation = reinterpret_cast<u_char *>(&block[used]);
    used += quads;

    if (used >= Size) {
        std::printf("Alocation Error! %d/%d\n", used, Size);

        for (;;) {
        }
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

/**
 * Buffer the common-menu effect models are read into.
 */
extern CDataAlloc2<1> MenuEffectCashBuffer;

} // namespace dc::game
