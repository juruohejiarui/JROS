#ifndef __FS_API_H__
#define __FS_API_H__

#include "../includes/lib.h"
#include "../hardware/diskdevice.h"

typedef struct FS_File {

} FS_File;

typedef struct FS_Dir {

} FS_Dir;

typedef struct FS_Partition {
	DiskDevice *device;
	u64 st, ed; // start LBA and end LBA
	List listEle;

	int (*accFile)(struct FS_Partition *par, u32 uid, u8 *path, struct FS_File *file);
	int (*accDir)(struct FS_Partition *par, u32 uid, u8 *path, struct FS_Dir *dir);
	int (*relFile)(struct FS_Partition *par, struct FS_File *file);
	int (*relDir)(struct FS_Partition *par, struct FS_Dir *file);
	int (*makeFile)(struct FS_Partition *par, u32 uid, u8 *path);
	int (*makeDir)(struct FS_Partition *par, u32 uid, u8 *path);
	int (*delFile)(struct FS_Partition *par, u32 uid, u8 *path);
	int (*delDir)(struct FS_Partition *par, u32 uid, u8 *path);
	int (*setAcc)(struct FS_Partition *par, u32 uid, u8 *path, u32 acc);
	int (*getAcc)(struct FS_Partition *par, u32 uid, u8 *path);
} FS_Partition;

extern List FS_partitionList;

void FS_registerPartition(FS_Partition *partition);
void FS_init();
#endif