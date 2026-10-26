#pragma once

#include "csl.h"

extern "C" {

/**
 * Prepares the MIDI stream input module's buffers.
 */
int sceMSIn_Init(sceCslCtx *ctx);

/**
 * Queues one short MIDI message on a port.
 */
int sceMSIn_PutMsg(sceCslCtx *ctx, unsigned int port, unsigned int msg);

/**
 * Queues one hardware synthesizer extended message on a port.
 */
int sceMSIn_PutHsMsg(sceCslCtx *ctx, unsigned int port, unsigned char *msg);

}
