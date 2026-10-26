#pragma once

/* Vendor SDK declarations. The reconstruction is given no system include path at all — only the
   game's own headers — which is why this is reached with quotes rather than as <libpkt.h>. */

#include "common.h"

/* A cursor over a buffer the caller supplies, plus the tag currently left open so that closing it
   can write the length nobody knew when it was opened. */
struct sceVif1Packet {
    u_int *pCurrent;
    u_int *pBase;
    u_int *pOpenTag;
    u_int *pOpenDma;
    u_int *reserved[4];
};

/* The GIF channel's own packet, laid out the same way and driven by the same open/close pairs.
   The reconstruction reaches only its base, which is the address the DMA channel is handed once
   the packet is closed. */
struct sceGifPacket {
    u_long128 *pCurrent;
    u_long128 *pBase;
    u_long128 *pOpenTag;
    u_int reserved;
};

extern "C" {

void sceVif1PkInit(sceVif1Packet *packet, u_int *base);
void sceVif1PkReset(sceVif1Packet *packet);
u_int *sceVif1PkTerminate(sceVif1Packet *packet);
u_int *sceVif1PkEnd(sceVif1Packet *packet, u_int next);
void sceVif1PkReserve(sceVif1Packet *packet, int count);

void sceVif1PkCnt(sceVif1Packet *packet, int irq);
void sceVif1PkOpenDirectCode(sceVif1Packet *packet, int irq);
void sceVif1PkCloseDirectCode(sceVif1Packet *packet);
void sceVif1PkOpenGifTag(sceVif1Packet *packet, u_long128 giftag);
void sceVif1PkCloseGifTag(sceVif1Packet *packet);
void sceVif1PkAddGsAD(sceVif1Packet *packet, int reg, u_long data);
void sceVif1PkRefLoadImage(sceVif1Packet *packet, u_short dbp, u_char dpsm, u_short dbw,
                           u_long128 *src, u_int qwc, u_int dsax, u_int dsay,
                           u_int rrw, u_int rrh);
void sceVif1PkCall(sceVif1Packet *packet, u_long128 *address, u_int count);

void sceGifPkInit(sceGifPacket *packet, u_long128 *base);
void sceGifPkReset(sceGifPacket *packet);
u_int *sceGifPkTerminate(sceGifPacket *packet);
u_int *sceGifPkEnd(sceGifPacket *packet, int pce, int irq, int pad);
void sceGifPkCnt(sceGifPacket *packet, int pce, int irq, int pad);
void sceGifPkOpenGifTag(sceGifPacket *packet, u_long128 giftag);
void sceGifPkCloseGifTag(sceGifPacket *packet);
void sceGifPkAddGsAD(sceGifPacket *packet, int reg, u_long data);
void sceGifPkRefLoadImage(sceGifPacket *packet, u_short dbp, u_char dpsm, u_short dbw,
                          u_long128 *src, u_int qwc, u_int dsax, u_int dsay,
                          u_int rrw, u_int rrh);
}
