#ifndef __FS_GPT_DESC_H__
#define __FS_GPT_DESC_H__

#include "../../includes/lib.h"

typedef struct FS_GPT_Header {
    u8 signature[8];
    u32 revision;
    u32 hdrSize;
    u32 chksum; // CRC32 checksum of GPT header
    u32 reserved;
    u64 lba;
    u64 alterLba;
    u64 firLba; // first usable block that can be contained in a GPT entry
    u64 lstLba; // last usable block that can be contained in a GPT entry
    u8 guid[16]; // GUID of the disk
    u64 firGuidLba; // Starting LBA of the GUID Partition Entry array
    u32 parNum; // number of partition entries
    u32 entrySz; // size (in bytes) of each entry in the partition entry array
    u32 parArrayChksum; // CRC32 of the partition entry array
    u8 reserved2[0];
} __attribute__ ((packed)) FS_GPT_Header;

typedef struct FS_GPT_PartitionEntry {
    u8 parTypeGuid[16];
    u8 uniParGuid[16];
    u64 stLba;
    u64 edLba;
    u64 attr;
    u8 name[72];
} __attribute__ ((packed)) FS_GPT_PartitionEntry;

#endif