#include "api.h"
#include "../../includes/memory.h"
#include "../../includes/log.h"

static __always_inline__ int _read(FS_Partition *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->read(part->device, buf, (lbaOff + part->st) << 9, lbaNum << 9);
}
static __always_inline__ int _write(FS_Partition *part, void *buf, u64 lbaOff, u64 lbaNum) {
    return part->device->write(part->device, buf, (lbaOff + part->st) << 9, lbaNum << 9);
}
static __always_inline__ int _read4KPage(FS_Partition *part, void *buf, u64 pageOff, u64 pageNum) {
    return part->device->read(part->device, buf, (pageOff + part->st) << Page_4KShift, pageNum << Page_4KShift);
}
static __always_inline__ int _write4KPage(FS_Partition *part, void *buf, u64 pageOff, u64 pageNum) {
    // printk(WHITE, BLACK, "JRFS: %#018lx: write 4K Page : offset=%#018lx,pageNum=%#018lx\n", part, pageOff, pageNum);
    return part->device->write(part->device, buf, (pageOff + part->st) << Page_4KShift, pageNum << Page_4KShift);   
}

int FS_JRFS_mkfs(FS_Partition *partition) {
    if (partition->ed - partition->st <= Page_1GSize / 512) {
        printk(RED, BLACK, "JRFS: %#018lx: partition too small\n", partition);
        return 1;
    }
    FS_JRFS_RootDesc *rootDesc = kmalloc(Page_4KSize, Slab_Flag_Clear, NULL);
    _read(partition, rootDesc, 0, Page_4KSize / 512);
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

    printk(WHITE, BLACK,
        "JRFS: %#018lx:\n\tpgNum:%#018lx blkNum:%#018lx\n\tfrPgNum:%#018lx frSz:%#018lx\n\tpgDesc: off=%#018lx sz=%#018lx\n\tpgBitmap: off=%#018lx sz=%#018lx\n", 
        partition, rootDesc->pgNum, rootDesc->blkNum, rootDesc->frPgNum, rootDesc->frSize, 
        rootDesc->pgDescOff, rootDesc->pgDescSz, rootDesc->pgBmOff, rootDesc->pgBmSz);
    rootDesc->firDtBlk = upAlignTo((rootDesc->pgBmOff + rootDesc->pgBmSz) << Page_4KShift, rootDesc->blkSz) / rootDesc->blkSz;
    rootDesc->lstAllocBlk = rootDesc->firDtBlk;
    printk(WHITE, BLACK, "JRFS: %#018lx:firDtBlk:%#018lx lstAllocBlk:%#018lx\n", partition, rootDesc->firDtBlk, rootDesc->lstAllocBlk);
    _write(partition, rootDesc, 0, Page_4KSize / 512);

    void *lbas = kmalloc(Page_4KSize, Slab_Flag_Clear, NULL);
    // write page descriptors
    {
        const u64 pagePreBlk = rootDesc->blkSz / Page_4KSize;
        printk(WHITE, BLACK, "page pre block:%ld\n", pagePreBlk);
        {
            FS_JRFS_PgDesc *pages = lbas;
            for (int i = 0; i < Page_4KSize / sizeof(FS_JRFS_PgDesc); i++) pages->attr |= FS_JRFS_PgDesc_Allocated | FS_JRFS_PgDesc_IsSysPage;
            // write system page descriptors to array of system block
            for (u64 i = 0; i < rootDesc->firDtBlk * pagePreBlk; i++)
                _write4KPage(partition, pages, rootDesc->pgDescOff + i, 1);
            memset(pages, 0, Page_4KSize);
            for (u64 i = rootDesc->firDtBlk * pagePreBlk; i < rootDesc->blkNum * pagePreBlk; i++)
                _write4KPage(partition, pages, rootDesc->pgDescOff + i, 1);
        }
        printk(WHITE, BLACK, "JRFS: %#018lx: finish initialize page descriptors");
        {
            u64 *bm = lbas;
            // number of (8 * flags) pre 4K page
            const u64 flagM8PrePage = Page_4KSize / sizeof(u8);
            for (u64 i = rootDesc->firDtBlk * pagePreBlk / flagM8PrePage; i < rootDesc->blkNum * pagePreBlk / flagM8PrePage; i++)
                _write4KPage(partition, bm, rootDesc->pgBmOff + i, 1);
            memset(bm, 0xff, Page_4KSize);
            printk(WHITE, BLACK, "bitmap size pre block:%ld\n", rootDesc->firDtBlk * pagePreBlk / flagM8PrePage);
            for (u64 i = 0; i < rootDesc->firDtBlk * pagePreBlk / flagM8PrePage; i++)
                _write4KPage(partition, bm, rootDesc->pgBmOff + i, 1);
        }
        printk(WHITE, BLACK, "JRFS: %#018lx: finish initialize page bitmap");
    }
    kfree(lbas, 0);

    return 0;
}