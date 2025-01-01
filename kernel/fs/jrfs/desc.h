#ifndef __FS_JRFS_DESC_H__
#define __FS_JRFS_DESC_H__

#include "../../includes/lib.h"

typedef struct FS_JRFS_BlkDesc {
	u64 frDtNum;
	u64 dtNum;
	u64 attr;
	u64 bmCrc32;
	u64 dtCrc32;
} __attribute__ ((packed)) FS_JRFS_BlkDesc;

typedef struct FS_JRFS_PgDesc {
	u64 attr;
	#define FS_JRFS_PgDesc_grpSz(pgHdr) ((pgHdr)->attr & 0xful);
	#define FS_JRFS_PgDesc_IsSysPage	(1ul << 8)
	#define FS_JRFS_PgDesc_Allocated	(1ul << 9)
	u64 nxtPg;
} __attribute__ ((packed)) FS_JRFS_PgDesc;

typedef struct FS_JRFS_FileHdr {
	u64 firAccTime;
	u64 lstAccTime;
	u64 creatTime;
	u64 modiTime;
	u64 sz;
	u64 pgNum; // count of page
	u64 firPg;
	// max supported number of user: 32 * 32 = 1024
	u64 accFlag[32];
	#define FS_JRFS_FileHdr_accFlag_Read	(1ul << 0)
	#define FS_JRFS_FileHdr_accFlag_Write   (1ul << 1)
	#define FS_JRFS_FileHdr_accFlag_ReadOnly	FS_JRFS_FileHdr_accFlag_Read
	#define FS_JRFS_FileHdr_accFlag_WriteOnly   FS_JRFS_FileHdr_accFlag_Write
	#define FS_JRFS_FileHdr_accFlag_ReadWrite   FS_JRFS_FileHdr_accFlag_Write | FS_JRFS_FileHdr_accFlag_Read
	u64 attr;
	#define FS_JRFS_FileHdr_attr_isDir  (1ul << 0)
	#define FS_JRFS_FileHdr_attr_isSys  (1ul << 1)
	#define FS_JRFS_FileHdr_attr_isSoft (1ul << 2)
	#define FS_JRFS_FileHdr_attr_isHuge (1ul << 3)
	#define FS_JRFS_FileHdr_attr_isOpen	(1ul << 4)
	u64 nameHs;
	u8 name[64];
} __attribute__ ((packed)) FS_JRFS_FileHdr;

typedef struct FS_JRFS_DirEntry {
	FS_JRFS_FileHdr fileHdr;
} __attribute__ ((packed)) FS_JRFS_DirEntry;

typedef struct FS_JRFS_HugeDirNode {
	struct {
		u64 pgId[32];
	} __attribute__ ((packed)) child;
	u32 childSz;
	u32 dep;
} __attribute__ ((packed)) FS_JRFS_HugeDirNode;

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
	u64 feat; // feat set enabled for this file system
	#define FS_JRFS_RootDesc_feat_HugeDir	(1ul << 0)
	#define FS_JRFS_RootDesc_feat_HugeFile	(1ul << 1)
	#define FS_JRFS_RootDesc_feat_HugePgGrp	(1ul << 2)
	#define FS_JRFS_RootDesc_feat_RootBackup	(1ul << 3)
	u64 descBackup;
	u64 firDtBlk, lstAllocBlk;
} __attribute__ ((packed)) FS_JRFS_RootDesc;

typedef struct FS_JRFS_Mgr {
	FS_JRFS_RootDesc *rootDesc;
		
};

#endif