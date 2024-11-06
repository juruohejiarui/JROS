#ifndef __FS_EXT4_DESC_H__
#define __FS_EXT4_DESC_H__

#include "../../includes/lib.h"

typedef struct EXT4_SuperBlock {
	u32 s_inodeCnt;
	u32 s_blkCntLow;
	u32 s_rBlkCntLow; // number of blocks can only be allocated by the super-user
	u32 s_freeBlkCntLow; 
	u32 s_freeInodeCnt;
	u32 s_firDataBlk; // must be at least 1 for 1k-block filesystem is typically 0 for all other block size
	u32 s_logBlkSize; // block size is 2 ^ (10 + s_logBlkSize)
	u32 s_logClusSize; // cluster size is (2 ^ s_logClusSize) blocks if bigalloc is enabled. Otherwise s_logClusSize equals to s_logBlkSize
	u32 s_blkPerGrp; // blocks per group
	u32 s_clusPerGrp; // clusters per group
	u32 s_inodePerGrp; 	// inodes per group
	u32 s_mtime; // mount time, in seconds since the epoch
	u32 s_wtime; // write time, in seconds since the epoch
	u16 s_mntCnt;	// numbers of mount since the last fsck
	u16 s_mxMntCnt; // number of mounts beyond which a fsck is needed
	u16 s_magic; // magic signature, 0xef32
	u16 s_state; // file system state
	u16 s_errors; // behaviour when detecting errors
	u16 s_minorRevLvl; // minor revision level
	u32 s_lstChk; // time of last check, in seconds since the epoch
	u32 s_chkInterval; // maximum time between checks, in seconds
	u32 s_creatorOS; // creator OS
	u32 s_revLvl; // revision level
	u16 s_defResUID; // default uid for reserved blocks
	u16 s_defResGID; // default gid for reserved blocks
	u32 s_firInode; // first non-reserved inode
	u16 s_inodeSz; // size of inode structure, in bytes
	u16 s_blkGrpNr; // block group # of this super block
	// compatible feature set flags, kernel can still read/write this fs even if it doesn't understand a flag; fsck should not do that
	u32 s_featureCompat;
	u32 f_featureIncompat; // incompatible feature set. If the kernel of fsck does not understand one of these bits, it should stop
	u32 s_featureRoCompat; // readonly-compatible feature set. If the kernel does not understand one of these bits, it can still mount read-only
	u8 s_uuid[16]; // 128-bit UUID for volume
	i8 s_volName[16]; // Volume label
	u8 s_lstMounted[64]; // directory where filesystem was last mounted
	u32 s_algorithmUsageBitMap; // for compression
	u8 s_preallocBlk;
	u8 s_preallocDirBlk;
} __attribute__((packed)) EXT4_SuperBlock;
#endif