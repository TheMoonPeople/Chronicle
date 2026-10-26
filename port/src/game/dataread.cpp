#pragma helper_mask_gpr 0x30
#pragma helper_mask_fpr 0x1000
#pragma name_counter 234

#include "common.h"
#include "sce/libcdvd.h"
#include "sce/sifdev.h"
#include "sce/sifrpc.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "btsysscript.hpp"
#include "cloth.hpp"
#include "clothread.hpp"
#include "dataalloc.hpp"
#include "dataread.hpp"
#include "dataset.hpp"
#include "framevu1.hpp"
#include "gameutil.hpp"
#include "mathutil.hpp"
#include "mglib.hpp"
#include "savedata.hpp"
#include "snd.hpp"
#include "sound.hpp"
#include "sysmes.hpp"

/* One record of the archive's index file. The four numbers a read needs sit behind twelve bytes
   the index does not use, and the first word is where the entry's name begins in the same file. */
struct DATA_HEADER_READ {
    int name; /**< Offset of the file name in the index image. */
    int unk_04[3];
    int offset;  /**< Byte offset recorded for the file. */
    int size;    /**< File size in bytes. */
    int sector;  /**< Starting sector relative to DATA.DAT. */
    int sectors; /**< Number of sectors occupied by the file. */
};

/* One file inside a pack. The name is the entry's own first bytes, everything it locates is a byte
   offset from the entry rather than from the pack, and a first byte of zero ends the table — so a
   pack can be walked without being told how many files it holds. */
struct PACK_ENTRY {
    char name[64]; /**< Null-terminated file name; empty ends the table. */
    int  offset;   /**< Byte offset of the file data from this entry. */
    int  size;     /**< File size in bytes. */
    int  next;     /**< Byte offset of the next entry from this entry. */
};

/* The index is turned into a tree of path components rather than a list of names, so a lookup
   costs one search per component instead of one comparison per entry. A directory carries no
   entry of its own and leaves the header null. */
struct NAME_TREE {
    char        *name;  /**< Path component this node matches. */
    DATA_HEADER *data;  /**< File located by the path ending here, or null for a directory. */
    NAME_TREE   *child; /**< First node one component deeper. */
    NAME_TREE   *next;  /**< Next node under the same parent. */
};

static char CurrentDir[256] = "y:/ps2/dc_data/";

static int    header_num;
static u_int *packfile_buff;
static int data_sector;
static int old_vsync;
static int start_vsync;

static u_char header_buff[0x50000];
static BG_READ_INFO bg_read_info[32];

static NAME_TREE *search_tree(NAME_TREE *node, char *name);
static int        CDRead(char *path, u_int *buffer, int *out_size);

/* The index is searched as the list it is on the disc, one name comparison per entry. */
static DATA_HEADER *SearchFile(char *path) {
    DATA_HEADER_READ *record;
    int               i;

    record = (DATA_HEADER_READ *) header_buff;

    for (i = 0; i < header_num; i++, record++) {
        if (strcasecmp((char *) record->name, path) == 0) {
            return (DATA_HEADER *) record;
        }
    }

    return 0;
}

void InitReadBG();

/* The buffer goes to the drive rather than through the processor, so an address the drive cannot
   reach is fatal rather than slow, and one that is not on a 64-byte boundary is only reported. */
int LoadFileBG(char *name, u_long128 *buffer, int *out_size);

BG_READ_INFO *GetReadBGFile(int index);

void StartReadBG();

/* One read is issued per vertical sync and the same call collects it, so a queued file costs two
   frames at best and the queue is walked from the front every time. */
void ReadBG();

int ReadBGSync();

void BreakReadBG();

/* A component is looked for in the whole subtree rather than among the node's own children, so a
   path whose middle components are spelled wrong still finds its entry. */

/* The whole tree is built inside the one buffer it is handed: the nodes and their headers grow up
   from the bottom and the names down from the top, so nothing is ever freed and the two meeting is
   what the size report at the end is for. */

/* The drive is asked for the data file itself only to learn where it starts; everything after this
   is read by sector from that base, which is why no path but the index's is ever opened. */
/* The index is read straight into the header buffer and kept as it is, with each entry's name
   offset turned into a pointer to the name and its path separators made forward slashes. */
void InitCDFile();

void InitMemoryFile() {
}

int LoadFile(char *path, void *buffer, int *out_size);

/* A name may carry a device in front of a colon, which is dropped: everything the game ships with
   is on the disc, and the development tree the other devices reached is what CurrentDir names. */
int LoadFile2(char *path, void *buffer, int *out_size, int mode);

static int CDRead(char *path, u_int *buffer, int *out_size) {
    DATA_HEADER_READ *header;
    sceCdRMode mode;

    header = (DATA_HEADER_READ *) SearchFile(path);

    if (!header) {
        return 0;
    }

    mode.trycount = 0;
    mode.spindlctrl = 1;
    mode.datapattern = 0;

    while (1) {
        if (sceCdRead(header->sector + data_sector, header->sectors, buffer, &mode)) {
            sceCdSync(0);

            if (sceCdGetError() == 0) {
                break;
            }
        }
    }

    if (out_size) {
        *out_size = header->size;
    }

    return 1;
}

int WriteFile(char *path, void *buffer, int size);

int LoadPackFile(char *path, u_int *buffer, int *out_size) {
    packfile_buff = buffer;

    if (!LoadFile2(path, buffer, out_size, 0)) {
        packfile_buff = 0;
        return 0;
    }

    return 1;
}

u_int *GetPackFile(char *name, int *out_size) {
    return GetPackFile(packfile_buff, name, out_size);
}

/* A pack is looked up by the last component of a path, so a caller may name a file the way the
   archive spells it and still find it inside the pack it was loaded from. */
u_int *GetPackFile(u_int *pack, char *name, int *out_size);

u_int *GetPackFile(u_int *pack, int index, char **out_name, int *out_size);

/* Every file of one extension at once, which is how a pack of animations or textures is taken
   whole. The caller says how many it has room for and gets back how many it was given. */
int GetPackFileExt(u_int *pack, char *extension, u_int **files, int max_files, int *sizes, char **names);

