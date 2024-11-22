#include "api.h"
#include "../../includes/log.h"
#include "../../includes/task.h"
#include "../../includes/smp.h"

static List _devList;

NVMe_QueMgr *HW_NVMe_allocQue(u64 queSize, u64 attr) {
	NVMe_QueMgr *queMgr = kmalloc(sizeof(NVMe_QueMgr), Slab_Flag_Clear | Slab_Flag_Private, (void *)HW_NVMe_freeQue);
	queMgr->desc.size = queSize;
	queMgr->attr = attr;
	register u64 entrySize = (queMgr->attr & NVMe_QueMgr_attr_isSubmQue ? sizeof(NVMe_SubmQueEntry) : sizeof(NVMe_CmplQueEntry));
	queMgr->que = kmalloc(queSize * entrySize, Slab_Flag_Clear, NULL);
	queMgr->desc.addr = DMAS_virt2Phys(queMgr->que);
	queMgr->desc.size = queSize;
	SpinLock_init(&queMgr->lock);
	queMgr->hdr = queMgr->til = 0;
	if (queMgr->attr & NVMe_QueMgr_attr_isSubmQue) {
		queMgr->reqSrc = kmalloc(queSize * sizeof(NVMe_Request), Slab_Flag_Clear, NULL);
	} else queMgr->submSrc = NULL;
}

void HW_NVMe_freeQue(NVMe_QueMgr *queMgr) {
	if (queMgr->attr & NVMe_QueMgr_attr_isSubmQue) kfree(queMgr->reqSrc, 0);
	if (queMgr->que != NULL) kfree(queMgr->que, 0);
}

// return the tail index of the inserted request, return -1 if failed to insert
int HW_NVMe_insSubm(NVMe_QueMgr *queMgr, NVMe_Request *req) {
	req->attr = 0;
	SpinLock_lock(&queMgr->lock);
	if (queMgr->len == queMgr->desc.size) {
		SpinLock_unlock(&queMgr->lock);
		return -1;
	}
	memcpy(&req->entry, ((NVMe_SubmQueEntry *)queMgr->que) + queMgr->til, sizeof(NVMe_SubmQueEntry));
	queMgr->til++;
	queMgr->len++;
	SpinLock_unlock(&queMgr->lock);
	if (queMgr->til == queMgr->desc.size) return queMgr->til = 0, queMgr->desc.size;
	else return queMgr->til;
}

void HW_NVMe_mkSumEntry_IO(NVMe_SubmQueEntry *entry, u8 opcode, u32 nsid, void *data, u64 lba, u16 numBlks) {
	memset(entry, 0, sizeof(entry));
	entry->cmd = opcode;
	entry->nsid = nsid;
	entry->metaPtr = DMAS_virt2Phys(data);
	*(u64 *)entry->cmdSpec = lba;
	entry->cmdSpec[2] = numBlks - 1;
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
	printk(WHITE, BLACK, "NVMe: %#018lx: cap:%#018lx capStride:%d\t", host, host->cap, host->capStride);
	{
		u32 status = HW_NVMe_readReg32(host, NVMe_Reg_Status), cfg = HW_NVMe_readReg32(host, NVMe_Reg_Cfg);
		printk(WHITE, BLACK, "cfg=%#010x status=%#010x (before reset)\n", cfg, status);
	}
	// support subsystem reset
	if (host->cap & (1ul << 36)) {
		HW_NVMe_writeReg32(host, NVMe_Reg_SubsysRes, 0x4E564D65);
	}
	// stop and reset the controller
	HW_NVMe_writeReg32(host, NVMe_Reg_Cfg, 0);
	
	{
		u32 status = HW_NVMe_readReg32(host, NVMe_Reg_Status), cfg = HW_NVMe_readReg32(host, NVMe_Reg_Cfg);
		for (int i = 0; i < 100; i++, Intr_SoftIrq_Timer_mdelay(1)) {
			status = HW_NVMe_readReg32(host, NVMe_Reg_Status);
			cfg = HW_NVMe_readReg32(host, NVMe_Reg_Cfg);
			if (!(cfg & 1) && !(status & 1)) break;
		}
		if ((cfg & 1) || (status & 1)) {
			printk(RED, BLACK, "NVMe: %#018lx: failed to reset controller\n", host);
			kfree(host, 0);
			return NULL;
		}
	}

	{
		u8 	mnPgSize = (host->cap >> 52) & 0xf,
			mxPgSize = (host->cap >> 56) & 0xf;
		printk(WHITE, BLACK, "NVMe: %#018lx: mnPgSize:%dKiB mxPgSize:%dKiB\n", host, 4u << mnPgSize, 4u << mxPgSize);
	}

	for (PCIe_CapHdr *hdr = HW_PCIe_getNxtCapHdr(host->pci, NULL); hdr; hdr = HW_PCIe_getNxtCapHdr(host->pci, hdr)) {
		switch (hdr->capId) {
			case PCIe_CapId_MSI :
				host->msiCapDesc = container(hdr, PCIe_MSICap, hdr);
				printk(WHITE, BLACK, "NVMe: %#018lx: MSI support\n", host);
				break;
			case PCIe_CapId_MSIX :
				host->msixCapDesc = container(hdr, PCIe_MSIXCap, hdr);
				printk(WHITE, BLACK, "NVMe: %#018lx: MSI-X support\n", host);
				break;
			default :
				break;
		}
	}
	if (!host->msiCapDesc && !host->msixCapDesc) {
		printk(RED, BLACK, "NVMe: %#018lx: no support for MSI/MSI-X.", host);
		if ((u64)host->regs >= MM_DMAS_bsSize) Task_unmapKrlPage((u64)host->regs);
		kfree(host, 0);
		return NULL;
	}
	if (host->msixCapDesc) {
		int vecNum = min(8, PCIe_MSIXCap_vecNum(host->msixCapDesc));
		host->enabledIntrNum = vecNum;
		host->msixTbl = HW_PCIe_MSIX_getTable(host->pci, host->msixCapDesc);
		host->msiDesc = kmalloc(sizeof(PCIe_MSI_Descriptor) * vecNum, Slab_Flag_Clear, NULL);
		for (int i = 0, cpuId = 0; i < vecNum; i++) {
			u8 vecId;
			SMP_allocIntrVec(1, cpuId, &cpuId, &vecId);
			if (cpuId == -1) {
				printk(RED, BLACK, "NVMe: %#018lx: MSI-X: failed to allocate vector ID for interrupt %d\n", host, i);
				/// @warning this process may cause unreleasing of vector id
				kfree(host->msiDesc, 0);
				kfree(host, 0);
				return NULL;
			}
			HW_PCIe_MSIX_setMsgAddr(host->msixTbl, i, SMP_getCPUInfoPkg(cpuId)->apicID, 0, HW_APIC_DestMode_Physical);
			HW_PCIe_MSIX_setMsgData(host->msixTbl, i, vecId, HW_APIC_DeliveryMode_Fixed, HW_APIC_Level_Deassert, HW_APIC_TriggerMode_Edge);
			HW_PCIe_MSI_initDesc(&host->msiDesc[i], cpuId, vecId, HW_NVMe_intrHandler, (u64)host | i);
			HW_PCIe_MSI_setIntr(&host->msiDesc[i]);
			cpuId = (cpuId + 1) % SMP_cpuNum;
		}
	} else {
		int cpuId; u8 vecSt;
		int vecNum = min(8, PCIe_MSICap_vecNum(host->msiCapDesc));
		host->enabledIntrNum = vecNum;
		SMP_allocIntrVec(vecNum, 0, &cpuId, &vecSt);
		if (cpuId == -1) {
			printk(RED, BLACK, "NVMe: %#018lx: MSI: failed to allocate %d vector.\n", host, vecNum);
			kfree(host, 0);
		}
		host->msiDesc = kmalloc(sizeof(PCIe_MSI_Descriptor) * vecNum, Slab_Flag_Clear, NULL);
		HW_PCIe_MSI_setMsgAddr(host->msiCapDesc, SMP_getCPUInfoPkg(cpuId)->apicID, 0, HW_APIC_DestMode_Physical);
		HW_PCIe_MSI_setMsgData(host->msiCapDesc, vecSt, HW_APIC_DeliveryMode_Fixed, HW_APIC_Level_Deassert, HW_APIC_TriggerMode_Edge);
		for (int i = 0; i < vecNum; i++) {
			HW_PCIe_MSI_initDesc(&host->msiDesc[i], cpuId, vecSt + i, HW_NVMe_intrHandler, (u64)host | i);
			HW_PCIe_MSI_setIntr(&host->msiDesc[i]);
		}
	}
	// disable the INTx
	host->pci->command |= (1 << 10);

	// completition queue entry size 	= 2^4
	// submission queue entry size 		= 2^6
	// arbitration mechanism selected	= 000
	// memory page size					= 2^(12+0)
	{
		u32 cc = (6u << 16) | (4 << 20);
		HW_NVMe_writeReg32(host, NVMe_Reg_Cfg, cc);
	}

	// setup admin submission queue and completition queue
	host->adminCmplQue = HW_NVMe_allocQue(Page_4KSize / sizeof(NVMe_CmplQueEntry), NVMe_QueMgr_attr_isAdmQue);
	host->adminSubmQue = HW_NVMe_allocQue(Page_4KSize / sizeof(NVMe_SubmQueEntry), NVMe_QueMgr_attr_isSubmQue | NVMe_QueMgr_attr_isAdmQue);
	host->adminCmplQue->submSrc = host->adminSubmQue;
	
	HW_NVMe_writeReg64(host, NVMe_Reg_AdmSubmQue, DMAS_virt2Phys(host->adminSubmQue->desc.addr));
	HW_NVMe_writeReg64(host, NVMe_Reg_AdmCmplQue, DMAS_virt2Phys(host->adminCmplQue->desc.addr));

	// set admin queue attributes
	HW_NVMe_writeReg32(host, NVMe_Reg_AdmQueAttr, (Page_4KSize / sizeof(NVMe_SubmQueEntry)) | ((Page_4KSize / sizeof(NVMe_CmplQueEntry)) << 16));

	// enable msi/msi-x
	if (host->msixCapDesc) HW_PCIe_MSIX_enableAll(host->pci, host->msixCapDesc);
	else HW_PCIe_MSI_enableAll(host->msiCapDesc);

	// enable nvme
	HW_NVMe_writeReg32(host, NVMe_Reg_Cfg, HW_NVMe_readReg32(host, NVMe_Reg_Cfg) | 1);
	{
		u32 cfg, status;
		for (int i = 0; i < 20; i++, Intr_SoftIrq_Timer_mdelay(1)) {
			cfg = HW_NVMe_readReg32(host, NVMe_Reg_Cfg), status = HW_NVMe_readReg32(host, NVMe_Reg_Status);
			if ((status & 1) && (cfg & 1)) break;
		}
		if ((cfg & 1) && (status & 1)) printk(WHITE, BLACK, "NVMe: %#018lx: start successfully, cfg=%#010x status=%#010x\n", host, cfg, status);
		else printk(RED, BLACK, "NVMe: %#018lx: failed to start, cfg=%#010x status=%#010x\n", host, cfg, status);
	}
	
	// setup 7 IO submission queue and 7 completition queue
	for (int i = 0; i < 7; i++) host->ioSubmQue[i] = HW_NVMe_allocQue(Page_4KSize / sizeof(NVMe_SubmQueEntry), NVMe_QueMgr_attr_isSubmQue);
	for (int i = 0; i < 7; i++) host->ioCmplQue[i] = HW_NVMe_allocQue(Page_4KSize / sizeof(NVMe_CmplQueEntry), 0), host->ioCmplQue[i]->submSrc = host->ioSubmQue[i];

	// set create IO completition queue command and create IO submission queue command
	return host;
}

IntrHandlerDeclare(HW_NVMe_intrHandler) {
	NVMe_Host *host = (NVMe_Host *)(arg & ~0xful); int intrId = arg & 0xful;
	printk(WHITE, BLACK, "NVMe: %#018lx: interrupt %d\n", host, intrId);
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