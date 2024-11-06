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
	u32 s_mountTime; // mount time, in seconds since the epoch
	u32 s_writeTime; // write time, in seconds since the epoch
	u16 s_mntCnt;	// numbers of mount since the last fsck
	u16 s_mxMntCnt; // number of mounts beyond which a fsck is needed
	u16 s_magic; // magic signature, 0xef32
	u16 s_state; // file system state
	#define EXT4_SuperBlock_state_CleanlyUnmounted			0x0001
	#define EXT4_SuperBlock_state_ErrorsDetected			0x0002
	#define EXT4_SuperBlock_state_OrphansBeingRecovered		0x0004
	u16 s_errors; // behaviour when detecting errors
	#define EXT4_SuperBlock_errors_Continue		1
	#define EXT4_SuperBlock_errors_RemountRO	2
	#define EXT4_SuperBlock_errors_Panic		3
	u16 s_minorRevLvl; // minor revision level
	u32 s_lstChk; // time of last check, in seconds since the epoch
	u32 s_chkInterval; // maximum time between checks, in seconds
	u32 s_creatorOS; // creator OS
	#define EXT4_SuperBlock_creatorOS_Linux		0
	#define EXT4_SuperBlock_creatorOS_Hurd		1
	#define EXT4_SuperBlock_creatorOS_Masix		2
	#define EXT4_SuperBlock_creatorOS_FreeBSD	3
	#define EXT4_SuperBlock_creatorOS_Lites		4
	u32 s_revLvl; // revision level
	#define EXT4_SuperBlock_revLvl_OriFormat	0
	#define EXT4_SuperBlock_revLvl_v2Format		1
	u16 s_defResUID; // default uid for reserved blocks
	u16 s_defResGID; // default gid for reserved blocks
	u32 s_firInode; // first non-reserved inode
	u16 s_inodeSz; // size of inode structure, in bytes
	u16 s_blkGrpNr; // block group # of this super block
	// compatible feature set flags, kernel can still read/write this fs even if it doesn't understand a flag; fsck should not do that
	u32 s_featureCompat;
	#define EXT4_SuperBlock_Compat_DirPrealloc		0x1
	#define EXT4_SuperBlock_Compat_ImgInodes		0x2
	#define EXT4_SuperBlock_Compat_HasJournal		0x4
	#define EXT4_SuperBlock_Compat_ExtAttr			0x8
	#define EXT4_SuperBlock_Compat_ResizeInode		0x10
	#define EXT4_SuperBlock_Compat_DirIndex			0x20
	#define EXT4_SuperBlock_Compat_LazyBG			0x40
	#define EXT4_SuperBlock_Compat_ExcludeINode		0x80
	#define EXT4_SuperBlock_Compat_ExcludeBitmap	0x100
	#define EXT4_SuperBlock_Compat_SpareSuper2		0x200
	u32 f_featureIncompat; // incompatible feature set. If the kernel of fsck does not understand one of these bits, it should stop
	u32 s_featureRoCompat; // readonly-compatible feature set. If the kernel does not understand one of these bits, it can still mount read-only
	u8 s_uuid[16]; // 128-bit UUID for volume
	i8 s_volName[16]; // Volume label
	u8 s_lastMounted[64]; // directory where filesystem was last mounted
	u32 s_algorithmUsageBitMap; // for compression
	u8 s_preallocBlk;
	u8 s_preallocDirBlk;
	u16 s_resGDTBlk; // number of reserved GDT entries for future filesystem expansion
	u8 s_journalUUID[16]; // UUID of journal superblock
	u32 s_journalInode; // inode number of journal file
	u32 s_journalDev; // device number of journal file, if the external journal feature flag is set
	u32 s_lstOrphan;  // start of list of orphaned inodes of delete
	u32 s_hashSeed[4]; // HTREE hash seed
	u8 s_defHashVer; // default hash algorithm to us for directory hashes
	/* if this value is 0 or EXT3_JNL_BACKUP_BLOCKS (1) , then the s_jnl_blocks field contains a duplicate copy
	 of the inode's i_blocks[] array and i_size */
	u8 s_jnlBackupType;
	// group descriptor size
	u16 s_descSz;
	// offset 0x100, default mount opts
	u32 s_defMountOpt;
	// first metablock block group, if the meta_bg feature is enabled
	u32 s_firMetaBg;
	// when the firlesystem was created, in seconds since the epoch
	u32 s_mkfsTime;
	// backup copy of the journal inode's i_block[] array in the first 15 elements and i_size_high and i_size in the 16-th and 17-th elements, respectively
	u32 s_jnlBlks[17];
	u32 s_blkCntHigh;
	u32 s_rBlkCntHigh;
	u32 s_freeBlkCntHigh;
	u16 s_mnExtraInodeSize; // all inodes have at least s_mnExtraInodeSize bytes
	u16 s_wantExtraInodeSize;  // new inodes should reserve s_wantExtraInodeSize bytes
	u32 s_flags; // miscellanuous flags
	/* RAID stride. This is the number of logical blocks read from or written to the disk before moving to the next disk. This affects the placement of filesystem metadata,
	which will hopefully make RAID storage faster
	*/
	u16 s_raidStride;
	/* seconds to wait in muti-mount prevention (MMP) checking.
	In theory, MMP is a mechanism to record in the superblock which host and device have mounted the filesystem, inorder to prevent multiple mounts.
	This feature does not seem to be implemented.*/
	u16 s_mmpInterval;
	/* Block s_mmpBlk for multi-mount protection data.*/
	u64 s_mmpBlk;
	/* RAID stripe width.
	This is the number of logical blocks read from or written to the disk before coming back to the current disk.
	This is used by the block allocator to try to reduce the number of read-modify-write operation in a RAID5/6.
	*/
	u32 s_raidStripeWidth;
	u8 s_logGrpPerFlex; // size of a flexible block group is 2 ^ (s_logGrpPreFlex)
	u8 s_chksumType; // metadata checksum algorith type. The only valid value is 1 (crc32c)
	u16 s_reservedPad;
	u64 s_kbWritten; // number of KiB written to this filesystem over its lifetime
	u32 s_snapshotInode; // inode number of active snapshot. 
	u32 s_snapshotId; // sequential ID of active snapshot
	u64 s_snapshotRBlkCnt;  // number of blocks reserved for activve snapshot's future user
	u32 s_snapshotLst; // inode number of the head of the on-disk snapshot list
	u32 s_errorCnt; // number of errors seen
	u32 s_firErrorTime; // first time an error happend, in seconds since the epoch
	u32 s_firErrorInode;
	u64 s_firErrorBlk; // number of block involved of first error
	u8 s_firErrorFunc[32];  // name of function where the error happened
	u32 s_firErrorLine; // line number where error happened
	u32 s_lastErrorTime;
	u32 s_lastErrorInode;
	u32 s_lastErrorLine;
	u64 s_lastErrorBlk;
	u8 s_lastErrorFunc[32];
	u8 s_mountOpts[64];
	u32 s_usrQuotaInode; // Inode number of user quota file
	u32 s_grpQuotaInode; // Inode number of group quota file
	u32 s_overheadBlk;
	u32 s_backupBlkGrp[2]; // block groups containing superblock backups (if sparse_super2)
	u8 s_encryptAlgos[4]; // encryption algorithms in use. There can be up to four algorithms in use at any time
	u8 s_encryptPasswordSalt[16]; // salt for the string2key algorithm for encryption
	u32 s_lpfInode; // inode number for lost + found
	u32 s_projQuotaInode; // inode that tracks project quotas
	u32 s_chksumSeed;
	u8 s_writeTimeHi;
	u8 s_mountTimeHi;
	u8 s_mkfsTimeHi;
	u8 s_lastChkHi;
	u8 s_firErrorTimeHi;
	u8 s_lastErrorTimeHi;
	u8 s_pad[2];
	u32 s_reserved[96];
	u32 s_checksum;
} __attribute__((packed)) EXT4_SuperBlock;
#endif