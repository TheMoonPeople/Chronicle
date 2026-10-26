#include "common.h"
#include "dataread.hpp"

namespace {

u_int *packfile_buff;

} // namespace

void InitMemoryFile() {
}

int LoadPackFile(char *path, u_int *buffer, int *out_size) {
    packfile_buff = buffer;

    if (!LoadFile2(path, buffer, out_size, 0)) {
        packfile_buff = nullptr;
        return 0;
    }

    return 1;
}

u_int *GetPackFile(char *name, int *out_size) {
    return GetPackFile(packfile_buff, name, out_size);
}
