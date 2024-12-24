#include "api.h"
#include "../../includes/memory.h"

static __always_inline__ int _read(FS_Partition *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->read(part->device, buf, (lbaOff + part->st) << 9, lbaNum << 9);
}
static __always_inline__ int _write(FS_Partition *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->write(part->device, buf, (lbaOff + part->st) << 9, lbaNum << 9);
}

int FS_JRFS_mkfs(FS_Partition *partition) {
    FS_JRFS_RootDesc *rootDesc = kmalloc(Page_4KSize, 0, NULL);
    memset(&rootDesc, 0, sizeof(FS_JRFS_RootDesc));
    memcpy("JRFS", rootDesc->name, 4);
    // set block size to 
    rootDesc->blkSz = 128ul << 8;
    rootDesc->blkNum = (partition->ed - partition->st + 1) / rootDesc->blkSz;
    rootDesc->pgNum = (rootDesc->blkSz >> Page_4KShift) * rootDesc->blkNum;
    rootDesc->frPgNum = rootDesc->pgNum - (rootDesc->blkSz >> Page_4KShift);
    rootDesc->frSize = rootDesc->frPgNum << Page_4KShift;
    rootDesc->feat = FS_JRFS_RootDesc_feat_HugeDir | FS_JRFS_RootDesc_feat_HugePgGrp;
	return 0;
}