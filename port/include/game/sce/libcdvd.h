#pragma once

#include "types.h"

/**
 * Describes a file located through the CD/DVD library.
 */
struct sceCdlFILE {
    u_int lsn;      /**< Starting logical sector number. */
    u_int size;     /**< File size in bytes. */
    char name[16];  /**< ISO 9660 file name. */
    u_char date[8]; /**< ISO 9660 recording date. */
};

/**
 * Configures a CD/DVD read request.
 */
struct sceCdRMode {
    u_char trycount;    /**< Number of retry attempts. */
    u_char spindlctrl;  /**< Spindle control mode. */
    u_char datapattern; /**< Requested sector data pattern. */
    u_char pad;         /**< Structure padding. */
};

extern "C" {
int sceCdInit(int mode);
int sceCdMmode(int media);
int sceCdSearchFile(sceCdlFILE *file, const char *name);
int sceCdRead(u_int lsn, u_int sectors, void *buffer, sceCdRMode *mode);
int sceCdSync(int mode);
int sceCdGetError(void);
int sceCdBreak(void);
}
