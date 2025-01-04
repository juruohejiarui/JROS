#ifndef __FS_API_H__
#define __FS_API_H__

#include "../includes/lib.h"
#include "../includes/memory.h"
#include "../hardware/diskdevice.h"

struct FS_DirEntry;

typedef struct FS_File {
	u64 opPtr;
	u64 curFileSz;
	int (*seek)(u64 opPtr);
	// write SZ bytes to file and move this->opPtr to opPtr + sz
	int (*write)(struct FS_File *file, void *buf, u64 sz);
	// read SZ bytes to file and move this->opPtr to opPtr + sz
	int (*read)(struct FS_File *file, void *buf, u64 sz);
	int (*close)(struct FS_File *file);
	int (*open)(struct FS_File *file);
	u8 name[128];
} FS_File;

typedef struct FS_Dir {
	int (*newFile)(struct FS_Dir *dir, char *name);
	int (*newDir)(struct FS_Dir *dir, char *name);
	int (*delFile)(struct FS_Dir *dir, struct FS_File *file);
	int (*delDir)(struct FS_Dir *dir, struct FS_Dir *subDir);
	struct FS_File* (*getFile)(struct FS_Dir *dir, char *name);
	struct FS_Dir* (*getDir)(struct FS_Dir *dir, char *name);
	int (*nextEntry)(struct FS_DirEntry *entry);
	int (*open)(struct FS_File *dir);
	int (*close)(struct FS_Dir *dir);
	u8 name[128];
} FS_Dir;

typedef struct FS_DirEntry {
	int entryType;
	u8 name[128];
	u64 nameLen;
} FS_DirEntry;

struct FS_Part;

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

	// process after remove the listEle from the partition list
	void (*unregister)(struct FS_Part *par);

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

static __always_inline__ int FS_readLba(FS_Part *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->read(part->device, buf, (lbaOff + part->st) << DiskDevice_LbaShift, lbaNum << DiskDevice_LbaShift);
}
static __always_inline__ int FS_writeLba(FS_Part *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->write(part->device, buf, (lbaOff + part->st) << DiskDevice_LbaShift, lbaNum << DiskDevice_LbaShift);
}
static __always_inline__ int FS_readPage(FS_Part *part, void *buf, u64 pageOff, u64 pageNum) {
    return part->device->read(part->device, buf, (pageOff << Page_4KShift) + (part->st << DiskDevice_LbaShift), pageNum << Page_4KShift);
}
static __always_inline__ int FS_writePage(FS_Part *part, void *buf, u64 pageOff, u64 pageNum) {
    // printk(WHITE, BLACK, "JRFS: %#018lx: write 4K Page : offset=%#018lx,pageNum=%#018lx\r", part, pageOff, pageNum);
    return part->device->write(part->device, buf, (pageOff << Page_4KShift) + (part->st << DiskDevice_LbaShift), pageNum << Page_4KShift);   
}

void FS_registerPart(FS_Part *part);
void FS_unregisterPart(FS_Part *partition);
void FS_init();
#endif