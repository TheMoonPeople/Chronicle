#pragma once

/**
 * @file
 * Declares the calls that read a file off the disc, and the ones that reach
 * into a pack file once it has been read.
 */

#include "common.h"

#include "dataalloc_fwd.hpp"

struct sceVif1Packet;

namespace dc::game {

class CFrameVu1;

/**
 * Stores the location and extent of one file in DATA.DAT.
 */
struct DATA_HEADER {
    int name;    /**< Offset of the file name in the header image. */
    int offset;  /**< Byte offset recorded for the file. */
    int size;    /**< File size in bytes. */
    int sector;  /**< Starting sector relative to DATA.DAT. */
    int sectors; /**< Number of sectors occupied by the file. */
};

/**
 * Tracks one asynchronous background file read.
 */
struct BG_READ_INFO {
    int        busy;      /**< Whether the queue slot is allocated. */
    int        id;        /**< CD/DVD request identifier. */
    int        done;      /**< Whether the request has completed. */
    char       name[128]; /**< File name used for error reporting. */
    u_long128 *buffer;    /**< Destination buffer. */
    int        size;      /**< File size in bytes. */
    int        sector;    /**< Absolute starting sector. */
    int        sectors;   /**< Number of sectors to read. */
};

/**
 * Steps the background read, and says whether one is still going.
 */
int ReadBGSync();

/**
 * Initializes the background-read queue.
 */
void InitReadBG();

/**
 * Queues a file for background reading. The buffer is filled by the drive
 * directly, so it must be reachable by it and should be 64-byte aligned.
 */
int LoadFileBG(char *name, u_long128 *buffer, int *out_size);

/**
 * Returns an allocated background-read queue slot.
 */
BG_READ_INFO *GetReadBGFile(int index);

/**
 * Starts the background-read system.
 */
void StartReadBG();

/**
 * Advances the active background read by one vertical sync. A read is issued
 * on one call and collected on a later one, so each queued file costs at
 * least two frames.
 */
void ReadBG();

/**
 * Cancels the active CD/DVD read and resets the background queue.
 */
void BreakReadBG();

/**
 * Initializes the DATA.DAT file index from disc. Files are afterwards read by
 * sector relative to the start of DATA.DAT.
 */
void InitCDFile();

/**
 * Initializes memory-backed file access.
 */
void InitMemoryFile();

/**
 * Reads a whole file off the disc.
 */
int LoadFile(char *path, void *buffer, int *out_size);

/**
 * Reads a file using the requested loading mode. A device prefix before a
 * colon in the path is ignored.
 */
int LoadFile2(char *path, void *buffer, int *out_size, int mode);

/**
 * Writes a buffer to a file.
 */
int WriteFile(char *path, void *buffer, int size);

/**
 * Loads a pack and makes it the current implicit pack.
 */
int LoadPackFile(char *path, u_int *buffer, int *out_size);

/**
 * Finds a file in the current implicit pack.
 */
u_int *GetPackFile(char *name, int *out_size);

/**
 * Finds a file inside a pack that has already been read. Only the last
 * component of the name is matched against the pack's entries.
 */
u_int *GetPackFile(u_int *pack, char *name, int *out_size);

/**
 * Finds a file by ordinal position in a pack.
 */
u_int *GetPackFile(u_int *pack, int index, char **out_name, int *out_size);

/**
 * Collects up to max_files files with a requested extension from a pack and
 * returns how many were found.
 */
int GetPackFileExt(u_int *pack, char *extension, u_int **files, int max_files, int *sizes, char **names);

/**
 * Shared buffer used for synchronous game-data reads.
 */
extern u_int *read_buffer;

/**
 * Loads the overlay required by a top-level game mode.
 */
void LoadOverlay(int mode);

/**
 * Initializes the IOP services and core rendering and file systems.
 */
void init_all();

/**
 * Writes the persistent GS state into the current VIF packet.
 */
void SetEnv(sceVif1Packet *packet);

/**
 * Loads the sound-effect and sequence metadata tables.
 */
void LoadSndTxt();

/**
 * Enables or disables accumulation of save-file play time.
 */
void PlayTimeCountFlag(int flag);

/**
 * Adds one vertical-sync interval to the save-file play time when enabled.
 */
int PlayTimeCount(int unused);

/**
 * Loads and initializes the system message catalogue for the active language.
 */
void LoadSystemMessage();

/**
 * Loads the global message and sound metadata used by game modes.
 */
void initialize_data();

/**
 * Gives one arena to the loader as its working buffer, sized in quadwords.
 */
void SetDataBuffer(CDataAlloc2<1> *arena, int quads);

/**
 * Sets where packet reads land, and how much room they have.
 */
void SetPacketReadBuffer(int packet_quads, int read_quads);

/**
 * Reports whether a background read is still running.
 */
int check_now_loading();

} // namespace dc::game
