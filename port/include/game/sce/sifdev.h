#pragma once

#define SCE_RDONLY 0x0001
#define SCE_WRONLY 0x0002
#define SCE_CREAT 0x0200
#define SCE_TRUNC 0x0400

#define SCE_SEEK_SET 0
#define SCE_SEEK_CUR 1
#define SCE_SEEK_END 2

extern "C" {
void sceFsReset(void);
int sceOpen(const char *name, int flags, ...);
int sceClose(int fd);
int sceLseek(int fd, int offset, int whence);
int sceRead(int fd, void *buffer, int size);
int sceWrite(int fd, void *buffer, int size);
}
