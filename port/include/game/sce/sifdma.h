#pragma once

/**
 * Describes one DMA transfer submitted to the SIF hardware.
 */
struct sceSifDmaData {
    void *data; /**< EE source address of the transfer. */
    void *addr; /**< IOP destination address of the transfer. */
    int size;   /**< Number of bytes transferred. */
    int mode;   /**< SDK transfer-mode flags. */
};

extern "C" {

/**
 * Submits one or more SIF DMA descriptors and returns their transfer identifier.
 */
int sceSifSetDma(struct sceSifDmaData *transfer, int count);

/**
 * Returns a nonnegative value while a SIF DMA transfer remains active.
 */
int sceSifDmaStat(int id);

}
