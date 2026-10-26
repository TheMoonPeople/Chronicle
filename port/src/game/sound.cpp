#include "sound.hpp"

#include <eekernel.h>
#include <libsdr.h>
#include <modmsin.h>
#include <sifrpc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dataread.hpp"
#include "gameutil.hpp"

/** The context of the MIDI stream input module. */
static sceCslCtx msinCtx;

/** The stream input module's two buffer groups, input first. */
static sceCslBuffGrp msinBfGrp[2];

/** The stream input module's input buffers, one per effect port. */
static sceCslBuffCtx msinBfCtx[6];

/** The stream input module's message buffers, one per effect port. */
static MSIN_BUFFER msinBf[6] __attribute__((aligned(64)));

/** The bank a load is handing to the MIDI player. */
static MIDI_BANK gBank;

/** The MIDI player's state, one record per bank slot. */
static MIDI_STATE midi_state;

/** The sequence description table. */
static SQ_INF_TABLE sq_inf_tbl;

/** The sound-effect description table. */
static SE_INF_TABLE se_inf_tbl;

/** IOP address the message buffers are copied to, or zero before the first start. */
static int iopMSINBuffAddr;

/** Bytes of bank bodies loaded into the sound processor. */
static int bd_size_total;

/**
 * Starts the sound library and sets the reverberation of both cores.
 *
 * @mangled set_spu__Fiiii
 * @address 0x143D70
 * @size 0x168
 */
void set_spu(int mode0, int mode1, int depth0, int depth1);

/**
 * Copies a bank's body into the sound processor and its header into IOP memory.
 *
 * @mangled TransHdBd__Fiiii
 * @address 0x143EE0
 * @size 0x1EC
 */
int TransHdBd(int hd, int hd_size, int bd, int bd_size);

