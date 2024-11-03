#include "api.h"
#include "../../includes/log.h"
#include "../../includes/task.h"

static List _devList;

void HW_NVMe_initQue(NVMe_QueMgr *queMgr, u64 queSize, u64 attr) {
	queMgr->desc.size = queSize;
	queMgr->attr = attr;
	u64 entrySize = (queMgr->attr & NVMe_QueMgr_attr_isSubmQue ? sizeof(NVMe_SubmQueEntry) : sizeof(NVMe_CmplQueEntry));
	queMgr->que = kmalloc(queSize * entrySize, Slab_Flag_Clear, NULL);
	queMgr->desc.addr = DMAS_virt2Phys(queMgr->que);
	queMgr->desc.size = queSize;
	SpinLock_init(&queMgr->lock);
	queMgr->hdr = queMgr->til = 0;
	if (queMgr->attr & NVMe_QueMgr_attr_isSubmQue) {
		queMgr->reqSrc = kmalloc(queSize * sizeof(NVMe_Request), Slab_Flag_Clear, NULL);
	} else queMgr->submSrc = NULL;
}

// initialize the device and create a management task, return the device if successfully.
NVMe_Host *HW_NVMe_initDevice(PCIeConfig *pciCfg) {
	NVMe_Host *host = kmalloc(sizeof(NVMe_Host), Slab_Flag_Clear, NULL);
	List_init(&host->listEle);
	host->pci = pciCfg;
	host->regs = DMAS_phys2Virt((pciCfg->type0.bar[0] | (((u64)pciCfg->type0.bar[1]) << 32)) & ~0xful);
	printk(WHITE, BLACK, "NVMe: %#018lx: pci:%#018lx regs:%#018lx\n", host, host->pci, host->regs);

	if ((u64)host->regs >= MM_DMAS_bsSize)
		Task_mapKrlPage((u64)host->regs, DMAS_virt2Phys(host->regs), MM_PageTable_Flag_Presented | MM_PageTable_Flag_Writable);
	return NULL;
}

void HW_NVMe_init() {
	List_init(&_devList);

	List *pcieListHeader = HW_PCIe_getMgrList();
    for (List *pcieListEle = pcieListHeader->next; pcieListEle != pcieListHeader; pcieListEle = pcieListEle->next) {
		PCIeManager *mgrStruct = container(pcieListEle, PCIeManager, listEle);
        if (mgrStruct->cfg->classCode != 0x01 || mgrStruct->cfg->subclass != 0x08)
            continue;
		NVMe_Host *host = HW_NVMe_initDevice(mgrStruct->cfg);
		if (host != NULL) List_insBefore(&host->listEle, &_devList);
	}
}