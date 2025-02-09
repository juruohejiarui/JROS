#ifndef __FS_API_H__
#define __FS_API_H__

#include "../includes/lib.h"
#include "../includes/memory.h"
#include "../hardware/diskdevice.h"

#define FS_MaxFileNameLen	128
struct FS_DirEntry;

typedef struct FS_File {
	u64 opPtr;
	u64 curFileSz;
	int (*seek)(struct FS_File *file, u64 opPtr);
	// write SZ bytes to file and move this->opPtr to opPtr + sz
	int (*write)(struct FS_File *file, void *buf, u64 sz);
	// read SZ bytes to file and move this->opPtr to opPtr + sz
	int (*read)(struct FS_File *file, void *buf, u64 sz);
	int (*close)(struct FS_File *file);
	u8 name[FS_MaxFileNameLen];
} FS_File;

typedef struct FS_DirEntry {
	u64 attr;
	u8 name[FS_MaxFileNameLen];
} FS_DirEntry;

typedef struct FS_Dir {
	int (*nextEntry)(struct FS_Dir *dir, FS_DirEntry *dirEntry);
	int (*closeDir)(struct FS_Dir *dir);
	u8 name[FS_MaxFileNameLen];
} FS_Dir;

struct FS_Part;

typedef struct FS_PartMgr {
	DiskDevice *device;
	SpinLock lock;
	int (*delPart)(struct FS_PartMgr *mgr, struct FS_Part *part);
	int (*addPart)(struct FS_PartMgr *mgr, u64 stLba, u64 edLba, u8 *parTypeGuid, u8 *uniGuid, char *name);
	int (*extPart)(struct FS_PartMgr *mgr, struct FS_Part *part, u64 nwEdLba);
	int (*updInfo)(struct FS_PartMgr *mgr, struct FS_Part *part);
} FS_PartMgr;

typedef struct FS_GUID {
	u8 data[16];
} __attribute__ ((packed)) FS_GUID;

typedef struct FS_Part {
	DiskDevice *device;
	u64 st, ed; // start LBA and end LBA
	FS_PartMgr *mgr;

	FS_GUID parTypeGuid, uniTypeGuid;
	u8 name[72];
	
	List listEle;

	// process after remove the listEle from the partition list
	void (*unregister)(struct FS_Part *par);

	struct FS_File *(*openFile)(struct FS_Part *par, u32 uid, u8 *path);
	struct FS_Dir *(*openDir)(struct FS_Part *par, u32 uid, u8 *path);
	int (*mkFile)(struct FS_Part *par, u32 uid, u8 *path);
	int (*mkDir)(struct FS_Part *par, u32 uid, u8 *path);
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

static __always_inline__ int FS_chkGUID(FS_GUID *a, FS_GUID *b) { return memcmp(a->data, b->data, sizeof(u8) * 16); }

static __always_inline__ void FS_setTypeGUID(FS_Part *part, FS_GUID *guid, int updInfo) {
	memcpy(guid->data, part->uniTypeGuid.data, sizeof(u8) * 16);
	if (updInfo && part->mgr->updInfo) part->mgr->updInfo(part->mgr, part);
}

static __always_inline__ void FS_setParTypeGUID(FS_Part *part, FS_GUID *guid, int updInfo) {
	memcpy(guid->data, part->parTypeGuid.data, sizeof(u8) * 16);
	if (updInfo && part->mgr->updInfo) part->mgr->updInfo(part->mgr, part);
}

#endif