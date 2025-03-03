#ifndef __FS_EXT4_DESC_H__
#define __FS_EXT4_DESC_H__

#include "../../includes/lib.h"

typedef struct FS_EXT4_SuperBlk {
	u32 inodeCnt;
	u32 blkCntLow;
	u32 rBlkCntLow; // number of blocks can only be allocated by the super-user
	u32 freeBlkCntLow; 
	u32 freeInodeCnt;
	u32 firDataBlk; // must be at least 1 for 1k-block filesystem is typically 0 for all other block size
	u32 logBlkSize; // block size is 2 ^ (10 + logBlkSize)
	u32 logClusSize; // cluster size is (2 ^ logClusSize) blocks if bigalloc is enabled. Otherwise logClusSize equals to logBlkSize
	u32 blkPerGrp; // blocks per group
	u32 clusPerGrp; // clusters per group
	u32 inodePerGrp; 	// inodes per group
	u32 mountTime; // mount time, in seconds since the epoch
	u32 writeTime; // write time, in seconds since the epoch
	u16 mntCnt;	// numbers of mount since the last fsck
	u16 mxMntCnt; // number of mounts beyond which a fsck is needed
	u16 magic; // magic signature, 0xef32
	u16 state; // file system state
	#define FS_EXT4_SuperBlk_state_CleanlyUnmounted		0x0001
	#define FS_EXT4_SuperBlk_state_ErrorsDetected			0x0002
	#define FS_EXT4_SuperBlk_state_OrphansBeingRecovered	0x0004
	u16 errors; // behaviour when detecting errors
	#define FS_EXT4_SuperBlk_errorContinue	1
	#define FS_EXT4_SuperBlk_errorRemountRO	2
	#define FS_EXT4_SuperBlk_errorPanic		3
	u16 minorRevLvl; // minor revision level
	u32 lstChk; // time of last check, in seconds since the epoch
	u32 chkInterval; // maximum time between checks, in seconds
	u32 creatorOS; // creator OS
	#define FS_EXT4_SuperBlk_creatorOLinux		0
	#define FS_EXT4_SuperBlk_creatorOHurd		1
	#define FS_EXT4_SuperBlk_creatorOMasix		2
	#define FS_EXT4_SuperBlk_creatorOFreeBSD	3
	#define FS_EXT4_SuperBlk_creatorOLites		4
	u32 revLvl; // revision level
	#define FS_EXT4_SuperBlk_revLvl_OriFormat	0
	#define FS_EXT4_SuperBlk_revLvl_v2Format		1
	u16 defResUID; // default uid for reserved blocks
	u16 defResGID; // default gid for reserved blocks
	u32 firInode; // first non-reserved inode
	u16 inodeSz; // size of inode structure, in bytes
	u16 blkGrpNr; // block group # of this super block
	// compatible feature set flags, kernel can still read/write this fs even if it doesn't understand a flag; fsck should not do that
	u32 featureCompat;
	#define FS_EXT4_SuperBlk_Compat_DirPrealloc		0x1
	#define FS_EXT4_SuperBlk_Compat_ImgInodes			0x2
	#define FS_EXT4_SuperBlk_Compat_HasJournal			0x4
	#define FS_EXT4_SuperBlk_Compat_ExtAttr			0x8
	#define FS_EXT4_SuperBlk_Compat_ResizeInode		0x10
	#define FS_EXT4_SuperBlk_Compat_DirIndex			0x20
	#define FS_EXT4_SuperBlk_Compat_LazyBG				0x40
	#define FS_EXT4_SuperBlk_Compat_ExcludeINode		0x80
	#define FS_EXT4_SuperBlk_Compat_ExcludeBitmap		0x100
	#define FS_EXT4_SuperBlk_Compat_SpareSuper2		0x200
	u32 f_featureIncompat; // incompatible feature set. If the kernel of fsck does not understand one of these bits, it should stop
	#define FS_EXT4_SuperBlk_Incompat_Compression		0x1
	// directory entries record the file type
	#define FS_EXT4_SuperBlk_Incompat_FileType			0x2
	#define FS_EXT4_SuperBlk_Incompat_Recover			0x4
	// has seperate journal device
	#define FS_EXT4_SuperBlk_Incompat_JournalDev		0x8
	#define FS_EXT4_SuperBlk_Incompat_MetaBlkGrp		0x10
	#define FS_EXT4_SuperBlk_Incompat_Extends			0x40
	#define FS_EXT4_SuperBlk_Incompat_64Bits			0x80
	// multiple mount protection
	#define FS_EXT4_SuperBlk_Incompat_MMP				0x100
	#define FS_EXT4_SuperBlk_Incompat_FlexBlkGrp		0x200
	// inodes can be used to store large extended attribute values
	#define FS_EXT4_SuperBlk_Incompat_ExtAttrInode		0x400
	#define FS_EXT4_SuperBlk_Incompat_DirData			0x1000
	// metadata checksum seed is stored in the super block
	#define FS_EXT4_SuperBlk_Incompat_ChksumSeed		0x2000
	#define FS_EXT4_SuperBlk_Incompat_LargeDir			0x4000
	// data in inode
	#define FS_EXT4_SuperBlk_Incompat_InlineData		0x8000
	// encrypted inodes are present on the filesystem
	#define FS_EXT4_SuperBlk_Incompat_Encrypt			0x10000
	u32 featureRoCompat; // readonly-compatible feature set. If the kernel does not understand one of these bits, it can still mount read-only
	#define FS_EXT4_SuperBlk_RoCompat_SparseSuper		0x1
	// this filesystem has been used to store a file greater than 2GiB
	#define FS_EXT4_SuperBlk_RoCompat_LargeFile		0x2
	#define FS_EXT4_SuperBlk_RoCompat_BtreeDir			0x4
	// this filesystem has files whose sizes are represented in units of logical blocks, not 512-byte sectors
	#define FS_EXT4_SuperBlk_RoCompat_HugeFile			0x8
	// group descripo.rs have checksums
	#define FS_EXT4_SuperBlk_RoCompat_GDTChksum		0x10
	// indicates that the old ext3 32000 subdirectory limit no longer applies
	#define FS_EXT4_SuperBlk_RoCompat_DirNLink			0x20
	// indicates that large inodes exist on this filesystem
	#define FS_EXT4_SuperBlk_RoCompat_ExtraInodeSize	0x40
	#define FS_EXT4_SuperBlk_RoCompat_HasSnapshot		0x80
	#define FS_EXT4_SuperBlk_RoCompat_Quota			0x100
	// this filesystem supports "bigalloc"
	#define FS_EXT4_SuperBlk_RoCompat_BigAlloc			0x200
	// this filesystem supports metadata checksumming, implies RoCompat_GDTChksum, though GDTChksum must not be set
	#define FS_EXT4_SuperBlk_RoCompat_MetadataChksum	0x400
	// this filesystem supports replicas
	#define FS_EXT4_SuperBlk_RoCompat_Replica			0x800
	// read-only filesystem image
	#define FS_EXT4_SuperBlk_RoCompat_Readonly			0x1000
	// this filesystem tracks project quotas
	#define FS_EXT4_SuperBlk_RoCompat_Project			0x2000
	u8 uuid[16]; // 128-bit UUID for volume
	i8 volName[16]; // Volume label
	u8 lastMounted[64]; // directory where filesystem was last mounted
	u32 algorithmUsageBitMap; // for compression
	u8 preallocBlk;
	u8 preallocDirBlk;
	u16 resGDTBlk; // number of reserved GDT entries for future filesystem expansion
	u8 journalUUID[16]; // UUID of journal superblock
	u32 journalInode; // inode number of journal file
	u32 journalDev; // device number of journal file, if the external journal feature flag is set
	u32 lstOrphan;  // start of list of orphaned inodes of delete
	u32 hashSeed[4]; // HTREE hash seed
	u8 defHashVer; // default hash algorithm to us for directory hashes
	#define FS_EXT4_SuperBlk_defHashVer_Legacy	0x0
	#define FS_EXT4_SuperBlk_defHashVer_HalfMD4	0x1
	#define FS_EXT4_SuperBlk_defHashVer_Tea		0x2
	#define FS_EXT4_SuperBlk_defHashVer_ULegacy	0x3
	#define FS_EXT4_SuperBlk_defHashVer_UHalfMD4	0x4
	#define FS_EXT4_SuperBlk_defHashVer_UTea		0x5
	/* if this value is 0 or EXT3_JNL_BACKUP_BLOCKS (1) , then the jnl_blocks field contains a duplicate copy
	 of the inode's i_blocks[] array and i_size */
	u8 jnlBackupType;
	// group descriptor size
	u16 descSz;
	// offset 0x100, default mount opts
	u32 defMountOpt;
	#define FS_EXT4_SuperBlk_defMountOpt_Debug			0x1
	#define FS_EXT4_SuperBlk_defMountOpt_BSDGroup		0x2
	// support userspace-provided extended attributes
	#define FS_EXT4_SuperBlk_defMountOpt_ExtAttrUser	0x4
	// Support POSIX access control lists
	#define FS_EXT4_SuperBlk_defMountOpt_PosixACL		0x8
	// do not support 32-bit UIDs
	#define FS_EXT4_SuperBlk_defMountOpt_UID16			0x10
	// all data and metadata are commited to the journal
	#define FS_EXT4_SuperBlk_defMountOpt_JModeData		0x20
	// all data are flushed to the disk before metadata are committed to the journal
	#define FS_EXT4_SuperBlk_defMountOpt_JModeOrdered	0x40
	// data ordering is not preserved; data may be written after the metadata has been written
	#define FS_EXT4_SuperBlk_defMountOpt_JModeWback	0x60
	// disable write flushes
	#define FS_EXT4_SuperBlk_defMountOpt_NoBarrier		0x100
	// track which blocks in a filesystem are metadata and therefore should not be used as data blocks
	#define FS_EXT4_SuperBlk_defMountOpt_Validity		0x200
	// enable DISCARD support, where the storage device is told about blocks becoming unused
	#define FS_EXT4_SuperBlk_defMountOpt_Discard		0x400
	// disable delayed allocation
	#define FS_EXT4_SuperBlk_defMountOpt_NoDelAlloc	0x800
	// first metablock block group, if the meta_bg feature is enabled
	u32 firMetaBg;
	// when the firlesystem was created, in seconds since the epoch
	u32 mkfsTime;
	// backup copy of the journal inode's i_block[] array in the first 15 elements and i_size_high and i_size in the 16-th and 17-th elements, respectively
	u32 jnlBlks[17];
	u32 blkCntHigh;
	u32 rBlkCntHigh;
	u32 freeBlkCntHigh;
	u16 mnExtraInodeSize; // all inodes have at least mnExtraInodeSize bytes
	u16 wantExtraInodeSize;  // new inodes should reserve wantExtraInodeSize bytes
	u32 flags; // miscellanuous flags
	#define FS_EXT4_SuperBlk_FlagSignedDirHash		0x0001
	#define FS_EXT4_SuperBlk_FlagUnsignedDirHash	0x0002
	#define FS_EXT4_SuperBlk_FlagTestDevCode		0x0004
	/* RAID stride. This is the number of logical blocks read from or written to the disk before moving to the next disk. This affects the placement of filesystem metadata,
	which will hopefully make RAID storage faster
	*/
	u16 raidStride;
	/* seconds to wait in muti-mount prevention (MMP) checking.
	In theory, MMP is a mechanism to record in the superblock which host and device have mounted the filesystem, inorder to prevent multiple mounts.
	This feature does not seem to be implemented.*/
	u16 mmpInterval;
	/* Block mmpBlk for multi-mount protection data.*/
	u64 mmpBlk;
	/* RAID stripe width.
	This is the number of logical blocks read from or written to the disk before coming back to the current disk.
	This is used by the block allocator to try to reduce the number of read-modify-write operation in a RAID5/6.
	*/
	u32 raidStripeWidth;
	u8 logGrpPerFlex; // size of a flexible block group is 2 ^ (logGrpPreFlex)
	u8 chksumType; // metadata checksum algorith type. The only valid value is 1 (crc32c)
	u16 reservedPad;
	u64 kbWritten; // number of KiB written to this filesystem over its lifetime
	u32 snapshotInode; // inode number of active snapshot. 
	u32 snapshotId; // sequential ID of active snapshot
	u64 snapshotRBlkCnt;  // number of blocks reserved for activve snapshot's future user
	u32 snapshotLst; // inode number of the head of the on-disk snapshot list
	u32 errorCnt; // number of errors seen
	u32 firErrorTime; // first time an error happend, in seconds since the epoch
	u32 firErrorInode;
	u64 firErrorBlk; // number of block involved of first error
	u8 firErrorFunc[32];  // name of function where the error happened
	u32 firErrorLine; // line number where error happened
	u32 lastErrorTime;
	u32 lastErrorInode;
	u32 lastErrorLine;
	u64 lastErrorBlk;
	u8 lastErrorFunc[32];
	u8 mountOpts[64];
	u32 usrQuotaInode; // Inode number of user quota file
	u32 grpQuotaInode; // Inode number of group quota file
	u32 overheadBlk;
	u32 backupBlkGrp[2]; // block groups containing superblock backups (if sparse_super2)
	u8 encryptAlgos[4]; // encryption algorithms in use. There can be up to four algorithms in use at any time
	#define FS_EXT4_SuperBlk_EncryptAlgoInvalid	0
	#define FS_EXT4_SuperBlk_EncryptAlgoAES256XTS	1
	#define FS_EXT4_SuperBlk_EncryptAlgoAES256GCM	2
	#define FS_EXT4_SuperBlk_EncryptAlgoAES256CBC	3
	u8 encryptPasswordSalt[16]; // salt for the string2key algorithm for encryption
	u32 lpfInode; // inode number for lost + found
	u32 projQuotaInode; // inode that tracks project quotas
	u32 chksumSeed;
	u8 writeTimeHi;
	u8 mountTimeHi;
	u8 mkfsTimeHi;
	u8 lastChkHi;
	u8 firErrorTimeHi;
	u8 lastErrorTimeHi;
	u8 pad[2];
	u32 reserved[96];
	u32 checksum;
} __attribute__((packed)) FS_EXT4_SuperBlk;

typedef struct FS_EXT4_BlkGrpDesc {
	u32 blkBitmapLow; // location of block bitmap
	u32 inodeBitmapLow; // location of inode bitmap
	u32 inodeTblLow; // location of inode table
	u16 freeBlkCntLow;
	u16 freeInodeCntLow;
	u16 usedDirCntLow;
	u16 flags;
	#define FS_EXT4_BlkGrpDesc_flags_Uninit			0x1
	#define FS_EXT4_BlkGrpDesc_flags_BlkBitmapUninit	0x2
	#define FS_EXT4_BlkGrpDesc_flags_InodeTblZero		0x4
	u32 excludeBitMapLow; // lower 32-bits of location of snapshot exclusion bitmap
	u16 blkBitmapChksumLow;
	u16 inodeBitmapChksumLow;
	// lower 16-bits of unused inode count. If set, we needn;t scan past the (SuperBlock.inodePerGrp - inodeTblUnused)-th entry in the inode table for this group
	u16 inodeTblUnusedLow;
	u16 chksum;
	u32 blkBitmapHigh;
	u32 inodeBitmapHigh;
	u32 inodeTblHigh;
	u16 freeBlkCntHigh;
	u16 freeInodeCntHigh;
	u16 usedDirCntHigh;
	u16 inodeTblUnusedHigh;
	u32 excludeBitMapHigh;
	u16 blkBitmapChksumHigh;
	u16 inodeBitmapChksumHigh;
	u32 reserved;
} __attribute__ ((packed)) FS_EXT4_BlkGrpDesc;

typedef struct FS_EXT4_MMPStruct {
	u32 magic; // 0x004d4d50
	u32 seq; // sequence number, updated periodically
	u64 time; // time that the MMP block wwas last updated
	i8 nodeName[64];
	i8 blkDevName[32];
	u16 chkInterval; // the MMP re-check interval, in seconds
	u16 pad1;
	u32 pad2[226];
	u32 checksum;
} __attribute__ ((packed)) FS_EXT4_MMPStruct;

typedef struct FS_EXT4_JBD2_BlkHdr {
	u32 magic;	// jbd2 magic number, 0xc03b3998
	u32 blkType; // description of what this block contains
	#define FS_EXT4_JBD2_BlkHdr_blkType_Desc			1
	#define FS_EXT4_JBD2_BlkHdr_blkType_CommitRecord	2
	#define FS_EXT4_JBD2_BlkHdr_blkType_JnlSuperBlkV1	3
	#define FS_EXT4_JBD2_BlkHdr_blkType_JnlSuperBlkV2	4
	// block revocation records.
	#define FS_EXT4_JBD2_BlkHdr_blkType_BlkRevoRecord	5
	u32 sequence; // the transaction ID that goes with this block
} __attribute__ ((packed)) FS_EXT4_JBD2_BlkHdr;

typedef struct FS_EXT4_JBD2_SuperBlk {
	FS_EXT4_JBD2_BlkHdr hdr;
	u32 blkSize;
	u32 maxLen;
	u32 fir; // first block of log information
	u32 sequence; // dynamic information describing the current state of the log
	u32 start; // block number of the start of log
	u32 errno; // error value, as set by FS_FS_EXT4_JDB2_journal_abort()
	u32 featureCompat; // compatible feature set
	// journal maintains checksums on the data blocks
	#define FS_EXT4_JBD2_SuperBlk_Compat_Chksum			0x1
	u32 featureIncompat; // incompatible feature set
	// journal has block revocatiuon records
	#define FS_EXT4_JBD2_SuperBlk_Incompat_Revoke			0x1
	// journal can deal with 64-bit block numbers
	#define FS_EXT4_JBD2_SuperBlk_Incompat_64Bit			0x2
	// journal commits asynchronously
	#define FS_EXT4_JBD2_SuperBlk_Incompat_AsyncCommit 	0x4
	#define FS_EXT4_JBD2_SuperBlk_Incompat_ChksumV2		0x8
	#define FS_EXT4_JBD2_SuperBlk_Incompat_ChksumV3		0x10
	u32 featureRoCompat; // read-only compatible feature set
	u8 uuid[16]; // 128-bit uuid for journal.
	u32 numUsers; // number of file systems sharing this journal
	u32 dynsuper; // location of dynamic super block copy
	u32 maxTrans; // limit of journal blocks per transaction
	u32 maxTransData; // limit of data blocks per transaction
	u8 chksumType; // checksum algorithm used for the journal
	#define FS_EXT4_JBD2_SuperBlk_chksumType_CRC32		1
	#define FS_EXT4_JBD2_SuperBlk_chksumType_MD5		2
	#define FS_EXT4_JBD2_SuperBlk_chksumType_SHA1		3
	#define FS_EXT4_JBD2_SuperBlk_chksumType_CRC32C	4
	u8 pad1[3];
	u32 pad2[42];
	u32 chksum;
	u8 users[16 * 48]; // ids of all file systems sharing the log. 
} __attribute__ ((packed)) FS_EXT4_JBD2_SuperBlk;

typedef struct FS_EXT4_JBD2_BlkTag {
	u32 blkNumLow; // lower 32-bit of the location of where the corresponding data block should end up on disk
	u16 chksum; // checksum of the journal uuid, the sequence number, and the data block.
	u32 flags;
	#define FS_EXT4_JBD2_BlkTag_Escaped	0x1
	#define FS_EXT4_JBD2_BlkTag_SameUUID	0x2
	#define FS_EXT4_JBD2_BlkTag_Deleted	0x4
	#define FS_EXT4_JBD2_BlkTag_LastTag	0x8
	union {
		// this structure used if the super block indicates support for 64-bit block numbers
		struct {
			u32 blkNumHigh;
			// this field is not present if the "same UUID" flag is set
			u8 uuid[16];
		} _64;
		struct {
			// this field is not present if the "same UUID" flag is set
			u8 uuid[16];
		} _32;
	};
} __attribute__ ((packed)) FS_EXT4_JBD2_BlkTag;

// this structure is used if JBD2_SuperBlk_Incompat_ChksumV3 is set.
typedef struct FS_EXT4_JBD2_BlkTag3 {
	u32 blkNumLow;
	u32 flags; // the bit description is the same as that of FS_EXT4_JBD2_BlkTag
	u32 blkNumHigh;
	u32 chksum;
	// this field is not present if the "same UUID" flag is set
	u8 uuid[16];
} __attribute__ ((packed)) FS_EXT4_JBD2_BlkTag3;

typedef struct FS_EXT4_JBD2_DescBlk {
	FS_EXT4_JBD2_BlkHdr hdr;
	union {
		FS_EXT4_JBD2_BlkTag3 tag3[0];
		FS_EXT4_JBD2_BlkTag tag[0];
	};
} __attribute__ ((packed)) FS_EXT4_JBD2_DescBlk;

typedef struct FS_EXT4_JBD2_RevoBlkHdr {
	FS_EXT4_JBD2_BlkHdr hdr;
	u32 cnt; // number of bytes used in this block
	union {
		u32 blk32[0];
		u64 blk64[0];
	};
} __attribute__ ((packed)) FS_EXT4_JBD2_RevoBlkHdr;

// if FS_EXT4_JBD2_SuperBlk_Incompat_ChksumV2 or FS_EXT4_JBD2_SuperBlk_Incompat_ChksumV3 are set, the end of the revocation block is this structure.
typedef struct FS_EXT4_JBD2_RevoBlkTail {
	u32 chksum; // chksum of the journal UUID + revocation block
} __attribute__ ((packed)) FS_EXT4_JBD2_RevoBlkTail;

typedef struct FS_EXT4_JBD2_CommitBlk {
	FS_EXT4_JBD2_BlkHdr hdr;
	u8 chksumType;
	u8 chksumSz;
	u8 padding[2];
	/* 32 bytes of space to store checksums.
	If FS_EXT4_JBD2_Incompat_ChksumV2 orFS_EXT4_JBD2_Incompat_ChksumV3 are set, the first dword is the checksum of the journal UUID and the entire commit block, with this field zeroed.
	If FS_EXT4_JBD2_compat_Chksum is set, the first dword is the CRC32 of all the blocks already written to the transaction
	*/
	u32 chksum[4];
	u64 commitSec; // the time that the transaction was committed, in seconds since the epoch
	u32 commitNsec; // nanoseconds component of the above timestamp
} __attribute__ ((packed)) FS_EXT4_JBD2_CommitBlk;

typedef struct FS_EXT4_Inode {
	u16 mode; // file mode
	#define FS_EXT4_Inode_mode_ExecOther 		0x1
	#define FS_EXT4_Inode_mode_WriteOther		0x2
	#define FS_EXT4_Inode_mode_ReadOther		0x4
	#define FS_EXT4_Inode_mode_ExecGrpMem		0x8
	#define FS_EXT4_Inode_mode_WriteGrpMem		0x10
	#define FS_EXT4_Inode_mode_ReadGrpMem		0x20
	#define FS_EXT4_Inode_mode_ExecOwner		0x40
	#define FS_EXT4_Inode_mode_WriteOwner		0x80
	#define FS_EXT4_Inode_mode_ReadOwner		0x100
	#define FS_EXT4_Inode_mode_StickyBit		0x200
	#define FS_EXT4_Inode_mode_SetGID			0x400
	#define FS_EXT4_Inode_mode_SetUID			0x800
	#define FS_EXT4_Inode_mode_FIFO			0x1000
	#define FS_EXT4_Inode_mode_ChrDev			0x2000
	#define FS_EXT4_Inode_mode_Dir				0x4000
	#define FS_EXT4_Inode_mode_BlkDev			0x6000
	#define FS_EXT4_Inode_mode_RegularFile		0x8000
	#define FS_EXT4_Inode_mode_SymbLk			0xA000
	#define FS_EXT4_Inode_mode_Socket			0xC000
	u16 uidLow; // Owner UID
	u32 szLow; // size in byte
	// last access time, in seconds since the epoch, However, if the 
	u32 accTime;
	u32 chgTime;
	u32 modiTime;
	u32 delTime;
	u16 gid;
	u16 lkCnt;
	u32 blkLow; // "block" count
	u32 flags;
	#define FS_EXT4_Inode_flags_SecDel		0x1
	// should be preserved, should undeletion be desired
	#define FS_EXT4_Inode_flags_Preserved	0x2
	// file is compressed
	#define FS_EXT4_Inode_flags_Compress	0x4
	// all writes to the file must be synchronous
	#define FS_EXT4_Inode_flags_Sync		0x8
	#define FS_EXT4_Inode_flags_Immutable	0x10
	// file can only be appended
	#define FS_EXT4_Inode_flags_Append		0x20
	// the dump utility should not dump this file
	#define FS_EXT4_Inode_flags_NoDump		0x40
	// do not update access time
	#define FS_EXT4_Inode_flags_NoAccTime	0x80
	// dirty compressed file
	#define FS_EXT4_Inode_flags_DirtyCmpr	0x100
	// file has one or more compressed clusters
	#define FS_EXT4_Inode_flags_ComprBlk	0x200
	// do not compress file
	#define FS_EXT4_Inode_flags_NoCompr	0x400
	// Encrypted inode
	#define FS_EXT4_Inode_flags_Encrypt	0x800
	// directory has hashed indexes
	#define FS_EXT4_Inode_flags_DirHash	0x1000
	// AFS magic directory
	#define FS_EXT4_Inode_flags_AFSMagic	0x2000
	// file data must always be written through the journal
	#define FS_EXT4_Inode_flags_JrlData	0x4000
	// file tail should not be merged (not used by ext4)
	#define FS_EXT4_Inode_flags_NoTail		0x8000
	// all directory entry data should be written synchronously
	#define FS_EXT4_Inode_flags_DirSync	0x10000
	// top of directory hierarchy
	#define FS_EXT4_Inode_flags_TopDir		0x20000
	#define FS_EXT4_Inode_flags_HugeFile	0x40000
	#define FS_EXT4_Inode_flags_Extents	0x80000
	// inode store a large extended attribute value in its data blocks
	#define FS_EXT4_Inode_flags_ExtAttr	0x100000
	#define FS_EXT4_Inode_flags_InlineData		0x10000000
	// create children with the same project ID
	#define FS_EXT4_Inode_flags_ProjInherit	0x20000000
	// reserved for ext4 library
	#define FS_EXT4_Inode_flags_Reserved		0x80000000
	union {
		u8 osd1[4];
		struct {
			u32 inodeVersion;
		} __attribute__ ((packed)) osd1_linux;
		struct {
			u32 translator;
		} __attribute__ ((packed)) osd1_hurd;
	}__attribute__ ((packed));
	u32 blk[15]; // block map or extent tree
	u32 generation; // file version (for NFS)
	u32 fileAclLow;
	union {
		u32 szHigh;
		u32 dirAcl;
	}__attribute__ ((packed)) ;
	u32 obsoFragAddr; // (Obsolete) fragment address
	union {
		u8 osd2[12];
		struct {
			// upper 16-bit of the block count
			u16 blkHigh;
			u16 fileAclHigh;
			u16 uidHigh;
			u16 gidHigh;
			u16 chksumLow;
			u16 reserved;
		} __attribute__ ((packed)) osd2_linux;
	} __attribute__ ((packed));
	u16 extraInodeSz; // size of this inode - 128 byte
	u16 chksumHigh;
	u32 chgTimeExtra;
	u32 modiTimeExtra;
	u32 accTimeExtra;
	u32 creTime;
	u32 creTimeExtra;
	u32 verHigh;
	u32 projId;
} __attribute__ ((packed)) FS_EXT4_Inode;

typedef struct FS_EXT4_ExtentHdr {
	u16 magic; // 0xf30a
	u16 entries; // number of valid entries following the header
	u16 maxEntries; // maximum number of entries that could follow the header
	/*
	depth of theis extent node in the extent tree.
	0= this extent node points to data blocks
	otherwise= points to other extent nodes.
	The extent tree can be at most 5 levels deep; 
	a logical block number can be at most 2^32, and the smallest n that satisfies 4 * (((blkSz - 12) / 12) ^ n) >= 2^32 is 5
	*/
	u16 depth;
	u32 generation;
} __attribute__ ((packed)) FS_EXT4_ExtentHdr;

typedef struct FS_EXT4_ExtentIdx {
	u32 blk; // this index node covers file blocks from 'block' onward
	// block number of the extent node that is the next level lower in the tree.
	u32 leafLow;
	u16 leafHigh;
	u16 unused;
} __attribute__ ((packed)) FS_EXT4_ExtentIdx;

typedef struct FS_EXT4_Extent {
	u32 blk; // first file block number that this extent covers
	// number of blocks covered by extent
	// <=32768; initialized
	// > 32768; uninitialized and actual size is len - 32768
	u16 len;
	u16 startHigh; // block number to which this extent points
	u32 startLow;
} __attribute__ ((packed)) FS_EXT4_Extent;

typedef struct FS_EXT4_ExtentTail {
	u32 chksum;
} __attribute__ ((packed)) FS_EXT4_ExtentTail;

typedef struct FS_EXT4_DirEntry {
	u32 inode; // number of the inode that this directory entry points to
	u16 recLen; // length of this directory entry. Must be a multiple of 4
	u16 nameLen; // length of the file name
	u8 name[255];
} __attribute__ ((packed)) FS_EXT4_DirEntry;

typedef struct FS_EXT4_DirEntryFile {
	u32 inode;
	u16 recLen;
	u8 nameLen;
	u8 fileType;
	u8 name[255];
} __attribute__ ((packed)) FS_EXT4_DirEntryFile;

typedef struct FS_EXT4_DirEntryTail {
	u32 reservedZ;
	u16 recLen;
	u8 reservedZ2;
	u8 reserved; // must be 0xde
	u32 chksum;
} __attribute__ ((packed)) FS_EXT4_DirEntryTail;

typedef struct FS_EXT4_DirHashRoot {
	u32 inode; // inode number of this directory
	u16 recordlen; // length of this record,12.
	u8 nameLen; // length of this name, 1
	u8 fileType; // file type of this entry, 0x2, (if the feature flag is set)
	i8 name[4]; // ".\0\0\0"
	u32 parInode; // inode number of parent directory
	u16 parRecordLen; // block_size - 12. The record length is long enough to cover all htree data
	u8 parNameLen; // length of the name, 2.
	u8 parFileType; // file type of this entry, 0x2
	i8 parName[4]; // "..\0\0"
	u32 resZ;
	u8 hashVer;
	u8 infoLen;
	u8 indirectLvl;
	u8 unusedFlags;
	// maximum number of DirHashRoot that can follow this header plus 1 for the header itself
	u16 lmt;
	// actual number of DirHashEntries that follow this header, plus 1 for the header it self
	u16 cnt;
	// the block number that goes width hash=0
	u32 blk;
} __attribute__ ((packed)) FS_EXT4_DirHashRoot;
#endif