#pragma once

extern "C" {

/**
 * Flushes or invalidates the EE caches selected by an SDK operation code.
 */
void FlushCache(int operation);

/**
 * Flushes the instruction cache from interrupt context.
 */
void iFlushCache(int operation);

/**
 * Synchronizes a data-cache range from interrupt context.
 */
void iSyncDCache(void *start, void *end);

/**
 * Terminates the current EE process with the supplied status.
 */
void Exit(int status);

}
