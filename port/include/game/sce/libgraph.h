#pragma once

/* Vendor SDK declarations. The reconstruction is given no system include path at all — only the
   game's own headers — which is why this is reached with quotes rather than as <libgraph.h>. */

#include "common.h"

#define SCE_GS_INTERLACE 1
#define SCE_GS_NTSC 2
#define SCE_GS_PAL 3
#define SCE_GS_FIELD 1

#define SCE_GS_PSMCT32 0
#define SCE_GS_PSMCT24 1
#define SCE_GS_PSMCT16 2
#define SCE_GS_PSMT8 19
#define SCE_GS_PSMT4 20
#define SCE_GS_PSMZ24 49

#define SCE_GS_ZGEQUAL 2

#define SCE_GIF_PACKED_AD 14

#define SCE_GS_PRIM 0
#define SCE_GS_RGBAQ 1
#define SCE_GS_UV 3
#define SCE_GS_XYZF2 4
#define SCE_GS_XYZ2 5
#define SCE_GS_TEX0_1 6
#define SCE_GS_CLAMP_1 8
#define SCE_GS_XYZF3 12
#define SCE_GS_TEX1_1 20
#define SCE_GS_TEXA 59
#define SCE_GS_TEXFLUSH 63
#define SCE_GS_SCISSOR_1 64
#define SCE_GS_ALPHA_1 66
#define SCE_GS_TEST_1 71
#define SCE_GS_FRAME_1 76
#define SCE_GS_ZBUF_1 78
#define SCE_GS_BITBLTBUF 80
#define SCE_GS_TRXPOS 81
#define SCE_GS_TRXREG 82
#define SCE_GS_TRXDIR 83

#define SCE_GS_PRIM_LINE 1
#define SCE_GS_PRIM_SPRITE 6

#define SCE_GS_ALWAYS 1

#define SCE_GS_LOCAL_LOCAL 2

/* Each register the display setup writes is a field layout rather than a number, so the setup
   reads as the hardware state it is: a field the game changes is one assignment and the rest of
   the register keeps whatever the SDK's defaults put there. */

typedef struct {
    u_long NLOOP : 15;
    u_long EOP : 1;
    u_long pad16 : 30;
    u_long PRE : 1;
    u_long PRIM : 11;
    u_long FLG : 2;
    u_long NREG : 4;
    u_long REGS0 : 4;
    u_long REGS1 : 4;
    u_long REGS2 : 4;
    u_long REGS3 : 4;
    u_long REGS4 : 4;
    u_long REGS5 : 4;
    u_long REGS6 : 4;
    u_long REGS7 : 4;
    u_long REGS8 : 4;
    u_long REGS9 : 4;
    u_long REGS10 : 4;
    u_long REGS11 : 4;
    u_long REGS12 : 4;
    u_long REGS13 : 4;
    u_long REGS14 : 4;
    u_long REGS15 : 4;
} sceGifTag;

typedef struct {
    union {
        u_long value;

        struct {
            u_long FBP : 9;
            u_long pad9 : 7;
            u_long FBW : 6;
            u_long pad22 : 2;
            u_long PSM : 6;
            u_long pad30 : 2;
            u_long FBMSK : 32;
        };

        struct {
            u_long tbp0 : 9;
            u_long pad09 : 7;
            u_long tbw : 6;
            u_long pad22_ : 2;
            u_long psm : 6;
            u_long pad30_ : 2;
            u_long fbmsk : 32;
        } bits;
    };
} sceGsFrame;

typedef struct {
    union {
        u_long value;

        struct {
            u_long ZBP : 9;
            u_long pad9 : 15;
            u_long PSM : 4;
            u_long pad28 : 4;
            u_long ZMSK : 1;
            u_long pad33 : 31;
        };

        struct {
            u_long zbp : 9;
            u_long pad09 : 15;
            u_long psm : 4;
            u_long pad28_ : 4;
            u_long zmsk : 1;
            u_long pad33_ : 31;
        } bits;
    };
} sceGsZbuf;

typedef struct {
    u_long OFX : 16;
    u_long pad16 : 16;
    u_long OFY : 16;
    u_long pad48 : 16;
} sceGsXyOffset;

typedef struct {
    u_long SCAX0 : 11;
    u_long pad11 : 5;
    u_long SCAX1 : 11;
    u_long pad27 : 5;
    u_long SCAY0 : 11;
    u_long pad43 : 5;
    u_long SCAY1 : 11;
    u_long pad59 : 5;
} sceGsScissor;

typedef struct {
    u_long AC : 1;
    u_long pad1 : 63;
} sceGsPrModeCont;

typedef struct {
    u_long CLAMP : 1;
    u_long pad1 : 63;
} sceGsColClamp;

typedef struct {
    u_long DTHE : 1;
    u_long pad1 : 63;
} sceGsDthe;

typedef struct {
    union {
        u_long value;

        struct {
            u_long ATE : 1;
            u_long ATST : 3;
            u_long AREF : 8;
            u_long AFAIL : 2;
            u_long DATE : 1;
            u_long DATM : 1;
            u_long ZTE : 1;
            u_long ZTST : 2;
            u_long pad19 : 45;
        };

        struct {
            u_long ate : 1;
            u_long atst : 3;
            u_long aref : 8;
            u_long afail : 2;
            u_long date : 1;
            u_long datm : 1;
            u_long zte : 1;
            u_long ztst : 2;
            u_long pad19_ : 45;
        } bits;
    };
} sceGsTest;

typedef struct {
    union {
        u_long value;

        struct {
            u_long A : 2;
            u_long B : 2;
            u_long C : 2;
            u_long D : 2;
            u_long pad8 : 24;
            u_long FIX : 8;
            u_long pad40 : 24;
        };

        struct {
            u_long a : 2;
            u_long b : 2;
            u_long c : 2;
            u_long d : 2;
            u_long pad08 : 24;
            u_long fix : 8;
            u_long pad40_ : 24;
        } bits;
    };
} sceGsAlpha;

typedef struct {
    u_long TA0 : 8;
    u_long pad8 : 7;
    u_long AEM : 1;
    u_long pad16 : 16;
    u_long TA1 : 8;
    u_long pad40 : 24;
} sceGsTexa;

typedef struct {
    union {
        u_long value;

        struct {
            u_long TBP0 : 14;
            u_long TBW : 6;
            u_long PSM : 6;
            u_long TW : 4;
            u_long TH : 4;
            u_long TCC : 1;
            u_long TFX : 2;
            u_long CBP : 14;
            u_long CPSM : 4;
            u_long CSM : 1;
            u_long CSA : 5;
            u_long CLD : 3;
        };

        struct {
            u_long tbp0 : 14;
            u_long tbw : 6;
            u_long psm : 6;
            u_long tw : 4;
            u_long th : 4;
            u_long tcc : 1;
            u_long tfx : 2;
            u_long cbp : 14;
            u_long cpsm : 4;
            u_long csm : 1;
            u_long csa : 5;
            u_long cld : 3;
        } bits;
    };
} sceGsTex0;

/* The register built as one 64-bit word rather than field by field, which is how the SDK spells a
   value nothing keeps a structure of. The width the block address is shifted by is the argument's
   own — an int, so a 32-bit shift — and every other term is widened before it moves; that is what
   puts a single `sll` among the `dsll`s. The two leading terms are in the order the game's code
   evidences rather than the order the SDK is assumed to have written them: the buffer width first
   is what matches and the block address first is not. Which one the *compiler* computes first says
   nothing about it, and the block below is where that was measured. */
#define SCE_GS_SET_TEX0(tbp0, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa, cld) \
    (((u_long) (tbw) << 14) | (u_long) (tbp0) | ((u_long) (psm) << 20) |            \
     ((u_long) (tw) << 26) | ((u_long) (th) << 30) | ((u_long) (tcc) << 34) |       \
     ((u_long) (tfx) << 35) | ((u_long) (cbp) << 37) | ((u_long) (cpsm) << 51) |    \
     ((u_long) (csm) << 55) | ((u_long) (csa) << 56) | ((u_long) (cld) << 61))

/* The mip-map register beside it, in field order: every term but the level bias and the fixed
   filter settings is a constant at every call site, so what the term order decides here is which
   of them the compiler can fold into one immediate. */
#define SCE_GS_SET_TEX1(lcm, mxl, mmag, mmin, mtba, l, k)                                       \
    ((u_long) (lcm) | ((u_long) (mxl) << 2) | ((u_long) (mmag) << 5) | ((u_long) (mmin) << 6) | \
     ((u_long) (mtba) << 9) | ((u_long) (l) << 19) | ((u_long) (k) << 32))

/* The registers a transfer or a sprite is built out of, which are one 64-bit word each and have
   no structure in the SDK. Term order is the game's own evidence rather than an assumption, and it
   is *not* readable off which term the compiler computes first: `TRXPOS` proves both halves at
   once. Written in field order it matches and written with the shifted term first it does not —
   yet either way the shift is emitted ahead of the plain load. What a source order settles is the
   operand order of the `|`, and reading it off the evaluation order is what gets it backwards. */
#define SCE_GS_SET_BITBLTBUF(sbp, sbw, spsm, dbp, dbw, dpsm)             \
    ((u_long) (sbp) | ((u_long) (sbw) << 16) | ((u_long) (spsm) << 24) | \
     ((u_long) (dbp) << 32) | ((u_long) (dbw) << 48) | ((u_long) (dpsm) << 56))

#define SCE_GS_SET_TRXPOS(ssax, ssay, dsax, dsay, dir)                     \
    ((u_long) (ssax) | ((u_long) (ssay) << 16) | ((u_long) (dsax) << 32) | \
     ((u_long) (dsay) << 48) | ((u_long) (dir) << 59))

#define SCE_GS_SET_TRXREG(rrw, rrh) ((u_long) (rrw) | ((u_long) (rrh) << 32))

#define SCE_GS_SET_TRXDIR(xdir) ((u_long) (xdir))

#define SCE_GS_SET_FRAME(fbp, fbw, psm, fbmsk) \
    ((u_long) (fbp) | ((u_long) (fbw) << 16) | ((u_long) (psm) << 24) | ((u_long) (fbmsk) << 32))

#define SCE_GS_SET_PRIM(prim, iip, tme, fge, abe, aa1, fst, ctxt, fix)                         \
    ((u_long) (prim) | ((u_long) (iip) << 3) | ((u_long) (tme) << 4) | ((u_long) (fge) << 5) | \
     ((u_long) (abe) << 6) | ((u_long) (aa1) << 7) | ((u_long) (fst) << 8) |                   \
     ((u_long) (ctxt) << 9) | ((u_long) (fix) << 10))

#define SCE_GS_SET_RGBAQ(r, g, b, a, q)                                                 \
    ((u_long) (r) | ((u_long) (g) << 8) | ((u_long) (b) << 16) | ((u_long) (a) << 24) | \
     ((u_long) (q) << 32))

#define SCE_GS_SET_UV(u, v) ((u_long) (u) | ((u_long) (v) << 16))

#define SCE_GS_SET_XYZF2(x, y, z, f) \
    ((u_long) (x) | ((u_long) (y) << 16) | ((u_long) (z) << 32) | ((u_long) (f) << 56))

typedef struct {
    u_long LCM : 1;
    u_long pad1 : 1;
    u_long MXL : 3;
    u_long MMAG : 1;
    u_long MMIN : 3;
    u_long MTBA : 1;
    u_long pad10 : 9;
    u_long L : 2;
    u_long pad21 : 11;
    u_long K : 12;
    u_long pad44 : 20;
} sceGsTex1;

typedef struct {
    u_long PRIM : 3;
    u_long IIP : 1;
    u_long TME : 1;
    u_long FGE : 1;
    u_long ABE : 1;
    u_long AA1 : 1;
    u_long FST : 1;
    u_long CTXT : 1;
    u_long FIX : 1;
    u_long pad11 : 53;
} sceGsPrim;

typedef struct {
    union {
        u_long value;

        struct {
            u_long R : 8;
            u_long G : 8;
            u_long B : 8;
            u_long A : 8;
            u_long Q : 32;
        };

        struct {
            u_long r : 8;
            u_long g : 8;
            u_long b : 8;
            u_long a : 8;
            u_long q : 32;
        } bits;
    };
} sceGsRgbaq;

typedef struct {
    u_long X : 16;
    u_long Y : 16;
    u_long Z : 32;
} sceGsXyz;

/* Five registers rather than a layout the code states: nothing reconstructed reaches inside one,
   and 40 bytes each is what the double buffer's own size leaves room for. */
/**
 * Fields of a GS display register.
 */
typedef struct sceGsDisplay {
    u_int DX : 12;  /**< Horizontal display offset. */
    u_int DY : 11;  /**< Vertical display offset. */
    u_int MAGH : 4; /**< Horizontal magnification. */
    u_int MAGV : 2; /**< Vertical magnification. */
    u_int p0 : 3;   /**< Reserved bits. */
    u_int DW : 12;  /**< Display width. */
    u_int DH : 11;  /**< Display height. */
    u_int p1 : 9;   /**< Reserved bits. */
} sceGsDisplay;

typedef struct {
    u_long pmode;
    u_long smode2;
    u_long dispfb;
    sceGsDisplay display;
    u_long bgcolor;
} sceGsDispEnv;

/* The eight register pairs sceGsSetDefDrawEnv fills, without the GIF tag that carries them: the
   tag covers the clear beside them as well, so it belongs to the pair of them rather than to
   either, which is also why sceGsSetHalfOffset is handed this and not the tag. */
typedef struct {
    sceGsFrame frame1;
    u_long frame1addr;
    sceGsZbuf zbuf1;
    u_long zbuf1addr;
    sceGsXyOffset xyoffset1;
    u_long xyoffset1addr;
    sceGsScissor scissor1;
    u_long scissor1addr;
    sceGsPrModeCont prmodecont;
    u_long prmodecontaddr;
    sceGsColClamp colclamp;
    u_long colclampaddr;
    sceGsDthe dthe;
    u_long dtheaddr;
    sceGsTest test1;
    u_long test1addr;
} sceGsDrawEnv1;

typedef struct {
    sceGsTest test0;
    u_long test0addr;
    sceGsPrim prim;
    u_long primaddr;
    sceGsRgbaq rgbaq;
    u_long rgbaqaddr;
    sceGsXyz xyz2_0;
    u_long xyz2_0addr;
    sceGsXyz xyz2_1;
    u_long xyz2_1addr;
    sceGsTest test1;
    u_long test1addr;
} sceGsClear;

typedef struct {
    sceGsDispEnv disp0;
    sceGsDispEnv disp1;
    sceGifTag giftag0;
    sceGsDrawEnv1 draw0;
    sceGsClear clear0;
    sceGifTag giftag1;
    sceGsDrawEnv1 draw1;
    sceGsClear clear1;
} sceGsDBuff;

/* The read-back packet the SDK builds and the game only ever passes by address. Its extent is the
   one thing about it the image states, and it is what the executable gives gs_simage. */
typedef struct {
    u_long chain[7][2];
} sceGsStoreImage;

/**
 * Packet used by the SDK to transfer one image into GS memory.
 */
typedef struct {
    u_long chain[6][2];
} sceGsLoadImage;

extern "C" {

void sceGsResetGraph(short mode, short inter, short omode, short ffmd);

/**
 * Resets the GIF paths used to submit graphics commands.
 */
void sceGsResetPath();

int sceGsSyncV(int mode);
int sceGsSyncPath(int mode, u_short timeout);
void sceGsSyncVCallback(int (*callback)(int));
void sceGsSetDefDBuff(sceGsDBuff *db, int psm, int w, int h, int ztest, int zpsm, int clear);
void sceGsSwapDBuff(sceGsDBuff *db, int id);
void sceGsSetHalfOffset(sceGsDrawEnv1 *env, short offx, short offy, short field);
void sceGsSetDefStoreImage(sceGsStoreImage *si, short sbp, short sbw, short spsm, short ssax,
                           short ssay, short rrw, short rrh);
void sceGsExecStoreImage(sceGsStoreImage *si, u_long128 *dest);
void sceGsSetDefLoadImage(sceGsLoadImage *li, short dbp, short dbw, short dpsm, short dsax,
                          short dsay, short rrw, short rrh);
void sceGsExecLoadImage(sceGsLoadImage *li, u_long128 *source);
}

#define R bits.r
#define G bits.g
#define B bits.b
#define A bits.a
#define FBP bits.tbp0
#define FBW bits.tbw
#define PSM bits.psm
#define TBP0 bits.tbp0
#define TBW bits.tbw
#define CBP bits.cbp
#define CPSM bits.cpsm
