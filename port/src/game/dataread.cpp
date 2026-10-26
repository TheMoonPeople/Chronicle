#include "common.h"

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

static u_int *packfile_buff;

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

