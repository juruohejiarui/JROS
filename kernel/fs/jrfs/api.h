#ifndef __FS_JRFS_API_H__
#define __FS_JRFS_API_H__

#include "desc.h"
#include "../api.h"

int FS_JRFS_mkfs(FS_Part *partition);
int FS_JRFS_loadfs(FS_Part *FS_Partition);

int FS_JRFS_seek(u64 opPtr);
int FS_JRFS_write(FS_File *file, void *buf, u64 sz);
int FS_JRFS_read(FS_File *file, void *buf, u64 sz);
int FS_JRFS_close(FS_File *file);
int FS_JRFS_open(u8 *path);

#endif