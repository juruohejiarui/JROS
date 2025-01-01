#ifndef __FS_API_H__
#define __FS_API_H__

#include "../includes/lib.h"
#include "../hardware/diskdevice.h"

typedef struct FS_File {

} FS_File;

typedef struct FS_Dir {

} FS_Dir;

typedef struct FS_PartMgr {
	DiskDevice *device;
	int (*delPart)(struct FS_PartMgr *mgr, struct FS_Part *part);
	int (*addPart)(struct FS_PartMgr *mgr, u64 stLba, u64 edLba, u8 *parTypeGuid, u8 *uniGuid, char *name);
	int (*extPart)(struct FS_PartMgr *mgr, struct FS_Part *part, u64 nwEdLba);
	int (*updInfo)(struct FS_PartMgr *mgr, struct FS_Part *part);
} FS_PartMgr;

typedef struct FS_Part {
	DiskDevice *device;
	u64 st, ed; // start LBA and end LBA
	FS_PartMgr *mgr;

	u8 parTypeGuid[16], uniTypeGuid[16];
	u8 name[72];
	
	List listEle;

	int (*accFile)(struct FS_Part *par, u32 uid, u8 *path, struct FS_File *file);
	int (*accDir)(struct FS_Part *par, u32 uid, u8 *path, struct FS_Dir *dir);
	int (*relFile)(struct FS_Part *par, struct FS_File *file);
	int (*relDir)(struct FS_Part *par, struct FS_Dir *file);
	int (*makeFile)(struct FS_Part *par, u32 uid, u8 *path);
	int (*makeDir)(struct FS_Part *par, u32 uid, u8 *path);
	int (*delFile)(struct FS_Part *par, u32 uid, u8 *path);
	int (*delDir)(struct FS_Part *par, u32 uid, u8 *path);
	int (*setAcc)(struct FS_Part *par, u32 uid, u8 *path, u32 acc);
	int (*getAcc)(struct FS_Part *par, u32 uid, u8 *path);
	
} FS_Part;

extern List FS_partitionList;

void FS_registerPart(FS_Part *part);
void FS_unregisterPart(FS_Part *partition);
void FS_init();
#endif