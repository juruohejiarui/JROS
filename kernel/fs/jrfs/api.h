#ifndef __FS_JRFS_API_H__
#define __FS_JRFS_API_H__

#include "desc.h"
#include "../api.h"

void FS_JRFS_init();
int FS_JRFS_mkfs(FS_Part *partition);
int FS_JRFS_loadfs(FS_Part *FS_Partition);

int FS_JRFS_seek(FS_File *file, u64 opPtr);
int FS_JRFS_write(FS_File *file, void *buf, u64 sz);
int FS_JRFS_read(FS_File *file, void *buf, u64 sz);
int FS_JRFS_close(FS_File *file);

int FS_JRFS_nextEntry(FS_Dir *dir, FS_DirEntry *entry);
int FS_JRFS_closeDir(FS_Dir *dir);

FS_File *FS_JRFS_openFile(FS_Part *par, u32 uid, u8 *path);
FS_Dir *FS_JRFS_openDir(FS_Part *par, u32 uid, u8 *path);

#endif