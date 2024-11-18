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

int HW_NVMe_insSubm(NVMe_QueMgr *queMgr, NVMe_Request *req) {
	req->attr = 0;
	SpinLock_lock(&queMgr->lock);
	if (queMgr->len == queMgr->desc.size) {
		SpinLock_unlock(&queMgr->lock);
		return 0;
	}
	memcpy(&req->entry, ((NVMe_SubmQueEntry *)queMgr->que) + queMgr->til, sizeof(NVMe_SubmQueEntry));
	queMgr->til++;
	
	SpinLock_unlock(&queMgr->lock);
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
	
	host->cap = *(u64 *)(host->regs);
	host->capStride = (1ul << (2 + ((host->cap >> 32) & 0x7ul)));
	printk(WHITE, BLACK, "NVMe: %#018lx: cap:%#018lx capStride:%d\n", host, host->cap, host->capStride);

	for (PCIe_CapabilityHeader *hdr = HW_PCIe_getNxtCapHdr(host->pci, NULL); hdr; hdr = HW_PCIe_getNxtCapHdr(host->pci, hdr)) {
		switch (hdr->capId) {
			case PCIe_CapId_MSI :
				printk(WHITE, BLACK, "NVMe: %#018lx: MSI support\n");
				break;
			case PCIe_CapId_MSIX :
				printk(WHITE, BLACK, "NVMe: %#018lx: MSI-X support\n");
				break;
			default :
				break;
		}
	}
	
	return host;
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