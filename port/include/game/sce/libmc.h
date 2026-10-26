#pragma once

#define MC_WAIT 0
#define MC_NOWAIT 1

#define sceMcIniSucceed 0
#define sceMcIniErrKernel (-101)
#define sceMcIniOldMcserv (-120)
#define sceMcIniOldMcman (-121)

#define sceMcResSucceed 0
#define sceMcResChangedCard (-1)
#define sceMcResNoFormat (-2)
#define sceMcResFullDevice (-3)
#define sceMcResNoEntry (-4)
#define sceMcResDeniedPermit (-5)
#define sceMcResFailReplace (-8)
#define sceMcResFailDetect (-12)

#define sceMcTypeNoCard 0
#define sceMcTypePS1 1
#define sceMcTypePS2 2
#define sceMcTypePDA 3

#include "common.h"

/**
 * One colour of an icon.sys, held as four full words.
 */
struct sceMcColor {
    u_int r; /**< Red the colour draws with. */
    u_int g; /**< Green the colour draws with. */
    u_int b; /**< Blue the colour draws with. */
    u_int a; /**< Alpha the colour draws with. */
};

STATIC_ASSERT(sizeof(sceMcColor) == 0x10);

/**
 * One colour of an icon.sys's lighting, held as floats.
 */
typedef float sceMcColorF[4];

/**
 * One direction of an icon.sys's lighting.
 */
typedef float sceMcVu0FVECTOR[4];

/**
 * The icon.sys the browser reads to draw one save, written alongside it.
 */
struct sceMcIconSys {
    char head[4];                 /**< The "PS2D" file identifier. */
    u_short unknown1;             /**< Reserved. */
    u_short nl_offset;            /**< Byte the second line of the title starts at. */
    u_int unknown2;               /**< Reserved. */
    u_int trans_rate;             /**< How solid the icon draws. */
    sceMcColor bg_color[4];       /**< Colour each corner of the icon's background takes. */
    sceMcVu0FVECTOR light_dir[3]; /**< Direction each of the icon's three lights points. */
    sceMcColorF light_color[3];   /**< Colour each of those lights takes. */
    sceMcColorF ambient;          /**< Ambient light the icon draws under. */
    u_char title_name[68];        /**< Title the browser shows the save under. */
    char fname_view[64];          /**< Icon file the browser draws while viewing the save. */
    char fname_copy[64];          /**< Icon file the browser draws while copying the save. */
    char fname_del[64];           /**< Icon file the browser draws while deleting the save. */
    u_char reserve[512];          /**< Reserved. */
};

STATIC_ASSERT(sizeof(sceMcIconSys) == 0x3C4);

extern "C" {

/**
 * Initializes the memory-card library and returns its status.
 */
int sceMcInit(void);

/**
 * Reports how the library's last call finished, and reads back what it
 * returned. Returns zero while that call is still running.
 */
int sceMcSync(int mode, int *cmd, int *result);

/**
 * Opens the named file on the card in the given port and returns a command id.
 */
int sceMcOpen(int port, int slot, char *name, int flag);

/**
 * Closes the given file and returns a command id.
 */
int sceMcClose(int fd);

/**
 * Reads the given number of bytes of the given file and returns a command id.
 */
int sceMcRead(int fd, void *buffer, int size);

/**
 * Writes the given number of bytes to the given file and returns a command id.
 */
int sceMcWrite(int fd, void *buffer, int size);

/**
 * Writes the buffered data of the given file to the card and returns a command id.
 */
int sceMcFlush(int fd);

/**
 * Enters the named directory of the card, writes back the one it left, and returns a command id.
 */
int sceMcChdir(int port, int slot, char *name, char *current);

/**
 * Creates the named directory on the card and returns a command id.
 */
int sceMcMkdir(int port, int slot, char *name);

/**
 * Deletes the named file of the card and returns a command id.
 */
int sceMcDelete(int port, int slot, char *name);

/**
 * Reads up to the given number of directory entries into the given table and returns a command id.
 */
int sceMcGetDir(int port, int slot, char *name, unsigned int mode, int count, void *table);

/**
 * Asks for the type, the free space and the format flag of the card in the given port.
 */
int sceMcGetInfo(int port, int slot, int *type, int *free_size, int *formatted);

/**
 * Formats the card in the given port and returns a command id.
 */
int sceMcFormat(int port, int slot);

/**
 * Unformats the card in the given port and returns a command id.
 */
int sceMcUnformat(int port, int slot);
}
