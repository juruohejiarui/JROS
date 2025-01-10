#ifndef __FS_JRFS_DESC_H__
#define __FS_JRFS_DESC_H__

#include "../../includes/lib.h"
#include "../../includes/memory.h"
#include "../api.h"

typedef struct FS_JRFS_BlkDesc {
	u64 frDtNum;
	u64 dtNum;
	u64 attr;
	u64 bmCrc32;
	u64 dtCrc32;
} __attribute__ ((packed)) FS_JRFS_BlkDesc;

typedef struct FS_JRFS_PgDesc {
	u8 grpSz;
	// range : [0, 14]
	#define FS_JRFS_PgDesc_MaxPgGrpSz	14
	u64 attr : 48;
	#define FS_JRFS_PgDesc_IsSysPage	(1ul << 0)
	#define FS_JRFS_PgDesc_IsAllocated	(1ul << 1)
	#define FS_JRFS_PgDesc_IsHeadPage	(1ul << 2)
	u64 nxtPg;
} __attribute__ ((packed)) FS_JRFS_PgDesc;


typedef struct FS_JRFS_DirNode {
	u64 childPgId[256];
	u8 childAttr[256];
	#define FS_JRFS_DirNode_childAttr_isEnd	(1ul << 0)
	u8 attr;
	// the child of this node is the entry header
	u8 dep;
	u16 reserved;
	u32 subEntryNum;
	u64 curPgId;
} __attribute__ ((packed)) FS_JRFS_DirNode;

typedef struct FS_JRFS_EntryHdr {
	u64 firAccTime;
	u64 lstAccTime;
	u64 creatTime;
	u64 modiTime;
	// max supported number of user: 32 * 16 = 512
	u64 accFlag[3];
	#define FS_JRFS_FileHdr_accFlag_Read	(1ul << 0)
	#define FS_JRFS_FileHdr_accFlag_Write	(1ul << 1)
	#define FS_JRFS_FileHdr_accFlag_Delete	(1ul << 2)
	#define FS_JRFS_FileHdr_accFlag_ReadOnly	FS_JRFS_FileHdr_accFlag_Read
	#define FS_JRFS_FileHdr_accFlag_WriteOnly	FS_JRFS_FileHdr_accFlag_Write
	#define FS_JRFS_FileHdr_accFlag_ReadWrite	FS_JRFS_FileHdr_accFlag_Write | FS_JRFS_FileHdr_accFlag_Read
	u32 attr;
	u64 curPgId;
	#define FS_JRFS_FileHdr_attr_isExist	(1ul << 0)
	#define FS_JRFS_FileHdr_attr_isDir		(1ul << 1)
	#define FS_JRFS_FileHdr_attr_isSys		(1ul << 2)
	#define FS_JRFS_FileHdr_attr_isSoft		(1ul << 3)
	#define FS_JRFS_FileHdr_attr_isOpen		(1ul << 4)
	u64 nameHash;
	u8 name[128];
	union {
		struct {
			u64 filePgNum; // count of page
			u64 fileSz;
		};
		struct {
			u64 subEntryNum;
		};
	};
	u64 parDirPgId;
} __attribute__ ((packed)) FS_JRFS_EntryHdr;

typedef struct FS_JRFS_RootDesc {
	u8 name[72];
	u64 size; // size of filesytem (byte)
	u64 pgNum; // size of pages (byte)
	u64 blkNum; // number of blocks
	u64 pgDescSz;
	u64 pgDescOff; // offset of page descriptor array
	u64 pgBmSz; // size of page bitmap array
	u64 pgBmOff; // offset of page bitmap array
    u64 frPgNum; // number of free page
	u64 frSize; // size of free space
	u32 blkSz; // size of blocks (bytes)
	u32 rootEntryNum;
	u32 rootEntryOff;
	u64 feat; // feat set enabled for this file system
	#define FS_JRFS_RootDesc_feat_HugeDir	(1ul << 0)
	#define FS_JRFS_RootDesc_feat_HugeFile	(1ul << 1)
	#define FS_JRFS_RootDesc_feat_HugePgGrp	(1ul << 2)
	#define FS_JRFS_RootDesc_feat_RootBackup	(1ul << 3)
	u64 descBackup;
	u8 reserved[3];
	u64 firDtBlk, lstAllocBlk;
} __attribute__ ((packed)) FS_JRFS_RootDesc;

#define FS_JRFS_BlkBmCacheLimit		16
#define FS_JRFS_PgDescCacheLimit	32
#define FS_JRFS_PgCacheLimit		32
#define FS_JRFS_BlkBmCacheFlushCnt	16
#define FS_JRFS_PgDescCacheFlushCnt	8
#define FS_JRFS_PgCacheFlushCnt		8
#define FS_JRFS_BlkShift	(27)
#define FS_JRFS_BlkSize		(1ul << FS_JRFS_BlkShift)
#define FS_JRFS_PagePreBlk	(FS_JRFS_BlkSize >> Page_4KShift)
#define FS_JRFS_BmSzPreBlk	(FS_JRFS_PagePreBlk / 8 * sizeof(u8) >> Page_4KShift)
#define FS_JRFS_PgDescSzPreBlk	(FS_JRFS_PagePreBlk * sizeof(FS_JRFS_PgDesc) >> Page_4KShift)
#define FS_JRFS_PgDescPrePage	(Page_4KSize / sizeof(FS_JRFS_PgDesc))

typedef struct FS_JRFS_PgDescCache {
	RBNode rbNode;
	u64 pgId;
	u64 accCnt;
	union {
		u8 descRaw[Page_4KSize];
		FS_JRFS_PgDesc desc[0];
	};
} FS_JRFS_PgDescCache;

typedef struct FS_JRFS_BlkBmCache {
	RBNode rbNode;
	u64 blkId;
	u64 accCnt;
	// bitmap just like buddy system
	u64 jiffies;
	u64 bmJiffies[FS_JRFS_PgDesc_MaxPgGrpSz];
	u64 bmCache64[FS_JRFS_PgDesc_MaxPgGrpSz][(FS_JRFS_BmSzPreBlk << Page_4KShift) / sizeof(u64)];
	u64 buddyFlag[FS_JRFS_PgDesc_MaxPgGrpSz][(FS_JRFS_BmSzPreBlk << Page_4KShift) / sizeof(u64)];
} FS_JRFS_BlkBmCache;

typedef struct FS_JRFS_PgCache {
	RBNode rbNode;
	u64 pgId;
	u64 accCnt;
	union {
		u8 page[Page_4KSize];
		FS_JRFS_EntryHdr entryHdr;
		FS_JRFS_DirNode dirNode;
		struct DirEntryHdr {
			FS_JRFS_EntryHdr hdr;
			FS_JRFS_DirNode rootNode;
		} __attribute__ ((packed)) dirEntry;
	};
} FS_JRFS_PgCache;

typedef struct FS_JRFS_File {
	FS_JRFS_EntryHdr hdr;
	char *parPath;
	u64 pathLen;
	u64 opPgId, grpHdrId;
	u32 off;
	struct FS_JRFS_Mgr *mgr;
	FS_File file;
} FS_JRFS_File;

typedef struct FS_JRFS_Dir {
	FS_JRFS_EntryHdr hdr;
	char *parPath;
	u64 pathLen;
	u64 curEntryId;
	struct FS_JRFS_Mgr *mgr;
	FS_Dir dir;
} FS_JRFS_Dir;

typedef struct FS_JRFS_Mgr {
	SpinLock lock;
	FS_JRFS_RootDesc *rootDesc;
	u32 rootDescAccCnt;
	RBTree pgDescCache, pgCache, bmCache;
	u64 bmCacheSz, pgCacheSz, pgDescCacheSz;

	FS_Part partition;
} FS_JRFS_Mgr;

#endif