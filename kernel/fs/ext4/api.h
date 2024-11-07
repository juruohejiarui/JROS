#ifndef __FS_EXT4_API_H__
#define __FS_EXT3_API_H__

#include "desc.h"

static __always_inline__ u64 FS_EXT4_InodeSz(EXT4_SuperBlk *superBlk, EXT4_Inode *inode) {
    return superBlk->inodeSz + (inode != NULL ? inode->extraInodeSz : 0);
}

static __always_inline__ void FS_EXT4_findInode(EXT4_SuperBlk *superBlk, u64 inodeNum, u64 *bgId, u64 *offset) {
    *bgId = (inodeNum - 1) / superBlk->inodePerGrp;
    *offset = (inodeNum - 1) % superBlk->inodePerGrp * superBlk->inodeSz;
}
void FS_mkfs_ex4();
#endif