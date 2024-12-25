#include "api.h"
#include "../../includes/memory.h"
#include "../../includes/log.h"

static __always_inline__ int _read(FS_Partition *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->read(part->device, buf, (lbaOff + part->st) << 9, lbaNum << 9);
}
static __always_inline__ int _write(FS_Partition *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->write(part->device, buf, (lbaOff + part->st) << 9, lbaNum << 9);
}

int FS_JRFS_mkfs(FS_Partition *partition) {
    if (partition->ed - partition->st <= Page_1GSize / 512) {
        printk(RED, BLACK, "JRFS: %#018lx: partition too small\n", partition);
        return 1;
    }
    FS_JRFS_RootDesc *rootDesc = kmalloc(Page_4KSize, Slab_Flag_Clear, NULL);
    memcpy("JRFS", rootDesc->name, 4);
    // set block size to 
    rootDesc->blkSz = 64ul << 20;
    rootDesc->blkNum = (partition->ed - partition->st + 1) / (rootDesc->blkSz / 512);
    rootDesc->pgNum = (rootDesc->blkSz >> Page_4KShift) * rootDesc->blkNum;
    rootDesc->frPgNum = rootDesc->pgNum - (rootDesc->blkSz >> Page_4KShift);
    rootDesc->frSize = rootDesc->frPgNum << Page_4KShift;
    rootDesc->feat = FS_JRFS_RootDesc_feat_HugeDir | FS_JRFS_RootDesc_feat_HugePgGrp;
    
    // calculate offset and length for page descriptor
    rootDesc->pgDescOff = upAlignTo(upAlignTo(sizeof(FS_JRFS_RootDesc), Page_4KSize) + sizeof(FS_JRFS_DirEntry) * 256, Page_4KSize) >> Page_4KShift;
    rootDesc->pgDescSz = upAlignTo(sizeof(FS_JRFS_PgDesc) * rootDesc->pgNum, Page_4KSize) >> Page_4KShift;
	
    rootDesc->pgBmOff = rootDesc->pgDescOff + rootDesc->pgDescSz;
    rootDesc->pgBmSz = upAlignTo(upAlignTo(rootDesc->pgNum, 64) / 64 * sizeof(u64), Page_4KSize) >> Page_4KShift;

    rootDesc->frPgNum = rootDesc->pgNum - (rootDesc->pgBmOff + rootDesc->pgBmSz);
    rootDesc->frSize = rootDesc->frPgNum << Page_4KShift;

    printk(WHITE, BLACK, "JRFS: %#018lx:\n\tpgNum:%#018lx blkNum:%#018lx\n\tfrPgNum:%#018lx frSz:%#018lx\n\tpgDesc: off=%#018lx sz=%#018lx\n\tpgBitmap: off=%#018lx sz=%#018lx\n", 
        partition, rootDesc->pgNum, rootDesc->blkNum, rootDesc->frPgNum, rootDesc->frSize, 
        rootDesc->pgDescOff, rootDesc->pgDescSz, rootDesc->pgBmOff, rootDesc->pgBmSz);
    rootDesc->firDtBlk = upAlignTo((rootDesc->pgBmOff + rootDesc->pgBmSz) << Page_4KShift, rootDesc->blkSz) / rootDesc->blkSz;
    rootDesc->lstAllocBlk = rootDesc->firDtBlk;
    printk(WHITE, BLACK, "JRFS: %#018lx:firDtBlk:%#018lx lstAllocBlk:%#018lx\n", partition, rootDesc->firDtBlk, rootDesc->lstAllocBlk);
    _write(partition, rootDesc, 0, Page_4KSize / 512);

    FS_JRFS_PgDesc *sysPage = kmalloc(Page_4KSize, Slab_Flag_Clear, NULL);
    for (int i = 0; i < Page_4KSize / sizeof(FS_JRFS_PgDesc); i++) sysPage->attr |= FS_JRFS_PgDesc_Allocated | FS_JRFS_PgDesc_IsSysPage;
    _write(partition, sysPage, rootDesc->pgDescOff / 512, Page_4KSize / 512);
    return 0;
}