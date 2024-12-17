#include "api.h"
#include "../../includes/log.h"
#include "../../includes/task.h"
#include "../../includes/smp.h"

static List _devList;

// initialize the device and create a management task, return the device if successfully.
NVMe_Host *HW_NVMe_initDevice(PCIeConfig *pciCfg) {
	NVMe_Host *host = kmalloc(sizeof(NVMe_Host), Slab_Flag_Clear, NULL);
	List_init(&host->listEle);
	host->pci = pciCfg;
	host->regs = DMAS_phys2Virt((pciCfg->type0.bar[0] | (((u64)pciCfg->type0.bar[1]) << 32)) & ~0xful);
	printk(WHITE, BLACK, "NVMe: %#018lx: pci:%#018lx regs:%#018lx\n", host, host->pci, host->regs);

	if ((u64)host->regs >= MM_DMAS_bsSize)
		Task_mapKrlPage((u64)host->regs, DMAS_virt2Phys(host->regs), MM_PageTable_Flag_Presented | MM_PageTable_Flag_Writable),
		Task_mapKrlPage((u64)host->regs + Page_4KSize, DMAS_virt2Phys(host->regs) + Page_4KSize, MM_PageTable_Flag_Presented | MM_PageTable_Flag_Writable);
	
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
		printk(WHITE, BLACK, "NVMe: %#018lx: mnPgSize:%dKiB mxPgSize:%dKiB mxQueSize:%d\n", host, 4u << mnPgSize, 4u << mxPgSize, host->cap & ((1u << 16) - 1));
	}

	u64 submQueSize = min(Page_4KSize / sizeof(NVMe_SubmQueEntry), (host->cap & ((1u << 16) - 1)) + 1),
		cmplQueSize = min(Page_4KSize / sizeof(NVMe_CmplQueEntry), (host->cap & ((1u << 16) - 1)) + 1);

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
	host->adminCmplQue = HW_NVMe_allocQue(cmplQueSize, NVMe_QueMgr_attr_isAdmQue);
	host->adminSubmQue = HW_NVMe_allocQue(submQueSize, NVMe_QueMgr_attr_isSubmQue | NVMe_QueMgr_attr_isAdmQue);
	host->adminSubmQue->tgrCmplQue = host->adminCmplQue;
	
	HW_NVMe_writeReg64(host, NVMe_Reg_AdmSubmQue, DMAS_virt2Phys(host->adminSubmQue->que));
	HW_NVMe_writeReg64(host, NVMe_Reg_AdmCmplQue, DMAS_virt2Phys(host->adminCmplQue->que));

	// set admin queue attributes
	HW_NVMe_writeReg32(host, NVMe_Reg_AdmQueAttr, (submQueSize) | (cmplQueSize << 16));

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

	NVMe_Request req;
	
	printk(WHITE, BLACK, "NVMe: %#018lx: cfg=%#010x,status=%#010x\n", host, HW_NVMe_readReg32(host, NVMe_Reg_Cfg), HW_NVMe_readReg32(host, NVMe_Reg_Status));

	// get namespace list and namespace attribute
	{	
		u16 res;
		{
			int *nspInfo = kmalloc(Page_4KSize, Slab_Flag_Private | Slab_Flag_Clear, NULL);
			HW_NVMe_mkSubmEntry_Iden(&req.entry, nspInfo, 0x02, 0);
			res = HW_NVMe_insReqWait(host, host->adminSubmQue, &req);
			if (res) {
				printk(RED, BLACK, "NVMe: %#018lx: failed to get namespace info, res=%#010x\n", host, res);
				goto InitFail;
			}
			
			while (nspInfo[host->nspNum]) host->nspNum++;
			host->nsp = kmalloc(sizeof(NVMe_Nsp) * host->nspNum, Slab_Flag_Clear, NULL);
			// for every namespace get the descriptor and create two submission queues and one completition queues for it
			for (int i = 0; i < host->nspNum; i++) {
				NVMe_Nsp *nsp = &host->nsp[i];
				nsp->ioCmplQue = HW_NVMe_allocQue(cmplQueSize, 0);
				nsp->ioSubmQue[0] = HW_NVMe_allocQue(submQueSize, NVMe_QueMgr_attr_isSubmQue);
				nsp->ioSubmQue[1] = HW_NVMe_allocQue(submQueSize, NVMe_QueMgr_attr_isSubmQue);
				host->ioCmplQue[i] = nsp->ioCmplQue;
				host->ioSubmQue[i << 1] = nsp->ioSubmQue[0];
				host->ioSubmQue[i << 1 | 1] = nsp->ioSubmQue[1];
				nsp->ioCmplQue->iden = i + 1;
				nsp->ioSubmQue[0]->iden = (i << 1) | 1;
				nsp->ioSubmQue[1]->iden = (i << 1) + 2;
				nsp->id = nspInfo[i];
			}
			kfree(nspInfo, 0);
		}
		NVMe_NspDesc *desc = kmalloc(Page_4KSize, Slab_Flag_Clear, NULL);
		for (int i = 0; i < host->nspNum; i++) {
			NVMe_Nsp *nsp = &host->nsp[i];
			HW_NVMe_mkSubmEntry_Iden(&req.entry, desc, NVMe_SubmEntry_Iden_Type_Nsp, nsp->id);
			res = HW_NVMe_insReqWait(host, host->adminSubmQue, &req);
			if (!res)
				printk(WHITE, BLACK, "NVMe: %#018lx: Nsp #%d: NSZ:%#018lx NCAP:%#018lx FLBAS:%x DPS:%d NMIC:%d\n", host, i, desc->size, desc->cap, desc->formatLbaSz, desc->dps, desc->nmic);
			else {
				printk(RED, BLACK, "NVMe: %#018lx: failed to get information of Nsp#%d,res=%#010x\n", i, res);
				goto InitFail;
			}

			// make create completion queue command, submission queue command
			HW_NVMe_mkSubmEntry_NewCmpl(&req.entry, nsp->ioCmplQue, i + 1);
			res = HW_NVMe_insReqWait(host, host->adminSubmQue, &req);
			if (res) {
				printk(RED, BLACK, "NVMe: %#018lx: failed to create IO completion queue #%d, res=%#010x\n", host, i, res);
				goto InitFail;
			}

			HW_NVMe_mkSubmEntry_NewSubm(&req.entry, nsp->ioSubmQue[0], nsp->ioCmplQue, 0);
			res = HW_NVMe_insReqWait(host, host->adminSubmQue, &req);
			if (res) {
				printk(RED, BLACK, "NVMe: %#018lx: failed to create IO submission queue #%d:0, res=%#010x\n", host, i, res);
				goto InitFail;
			}
			HW_NVMe_mkSubmEntry_NewSubm(&req.entry, nsp->ioSubmQue[1], nsp->ioCmplQue, 1);
			res = HW_NVMe_insReqWait(host, host->adminSubmQue, &req);
			if (res) {
				printk(RED, BLACK, "NVMe: %#018lx: failed to create IO submission queue #%d:1, res=%#010x\n", host, i, res);
				goto InitFail;
			}
		}
		kfree(desc, 0);
	}
	u64 *data = kmalloc(Page_4KSize, 0, NULL);
	for (int i = 0; i < Page_4KSize / sizeof(u64); i++) data[i] = i;
	HW_NVMe_mkSumEntry_IO(&req.entry, 0x02, host->nsp[0].id, data, 0x1, 1);
	int res = HW_NVMe_insReqWait(host, host->nsp[0].ioSubmQue[0], &req);
	printk(WHITE, BLACK, "res=%x\n", res);
	for (int i = 0; i < 32; i++) printk(WHITE, BLACK, "%016lx%c", data[i], (i % 8 == 7 ? '\n' : ' '));
	printk(WHITE, BLACK, "\n");
	return host;

	InitFail:
	return NULL;
}

IntrHandlerDeclare(HW_NVMe_intrHandler) {
	NVMe_Host *host = (NVMe_Host *)(arg & ~0xful); int intrId = arg & 0xful;
	printk(RED, BLACK, "NVMe: %#018lx: interrupt %d\n", host, intrId);
	NVMe_QueMgr *cmplQue = (intrId ? host->ioCmplQue[intrId - 1] : host->adminCmplQue);
	while (1) {
		NVMe_CmplQueEntry *cmplEntry = &cmplQue->cmplQue[cmplQue->hdr];
		printk(WHITE, BLACK, "#%d:%d : cmplRes=%#018lx,%#018lx\n", cmplEntry->submQueId, cmplEntry->submQueHdrPtr, *(u64 *)cmplEntry, *(u64 *)((u64)cmplEntry + sizeof(u64)));
		if (!cmplEntry->finishFlag) break;
		NVMe_QueMgr *subQue = cmplEntry->submQueId == 0 ? host->adminSubmQue : host->ioSubmQue[cmplEntry->submQueId - 1];
		NVMe_Request *req = subQue->reqSrc[cmplEntry->submQueHdrPtr - 1];
		subQue->reqSrc[cmplEntry->submQueHdrPtr - 1] = NULL;
		memcpy(cmplEntry, &req->res, sizeof(NVMe_CmplQueEntry));
		cmplEntry->finishFlag = 0;
		req->attr |= NVMe_Request_attr_Finished;
		cmplQue->hdr++;
		HW_NVMe_ringCmplDb(host, cmplQue);
		if (cmplQue->hdr == cmplQue->size) cmplQue->hdr = 0;
	}
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