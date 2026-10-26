#pragma once

/* Vendor SDK declarations. The reconstruction is given no system include path at all — only the
   game's own headers — which is why this is reached with quotes rather than as <libdma.h>. */

#include "common.h"

typedef struct {
    u_long DIR : 1;
    u_long pad1 : 1;
    u_long MOD : 2;
    u_long ASP : 2;
    u_long TTE : 1;
    u_long TIE : 1;
    u_long STR : 1;
    u_long pad9 : 7;
    u_long TAG : 16;
} sceDmaChcr;

/* Only the control register is declared: it is the one field anything reconstructed so far
   reaches, and the rest of the channel would be a claim about the SDK rather than a reading of
   the game. */
typedef struct {
    sceDmaChcr chcr;
} sceDmaChan;

/**
 * Stores the DMA-controller environment exchanged with the SDK.
 */
typedef struct sceDmaEnv {
    u_char sts; /**< Source-channel stall-control state. */
    u_char std; /**< Destination-channel stall-control state. */
    u_char mbs; /**< Source memory-bus state. */
    u_char mbd; /**< Destination memory-bus state. */
    u_short unk_04;
    u_short notify; /**< DMA channels that issue completion notifications. */
    u_short unk_08;
    u_short unk_0A;
    u_int unk_0C;
    void *mem; /**< Memory region associated with the DMA environment. */
} sceDmaEnv;

STATIC_ASSERT(sizeof(sceDmaEnv) == 0x14);

extern "C" {

sceDmaChan *sceDmaGetChan(int channel);
int sceDmaSend(sceDmaChan *chan, void *data);
int sceDmaSync(sceDmaChan *chan, int mode, int timeout);

/**
 * Resets the DMA controller using the requested mode.
 */
int sceDmaReset(int mode);

/**
 * Reads the DMA-controller environment into the supplied structure.
 */
int sceDmaGetEnv(sceDmaEnv *env);

/**
 * Applies the supplied DMA-controller environment.
 */
int sceDmaPutEnv(sceDmaEnv *env);
}
