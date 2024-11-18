#include "api.h"
#include "../../../includes/smp.h"
#include "../../../includes/interrupt.h"
#include "../../../includes/log.h"
#include "../../../includes/interrupt.h"

List HW_USB_XHCI_hostList, HW_USB_XHCI_DriverList;

// get the first event trb pointer, dequeue
static XHCI_GenerTRB *_getEvePtr(XHCI_Host *host, int eveId) {
	if (Task_current->cpuId == host->msiDesc[0].cpuId) IO_cli();
	SpinLock_lock(&host->eveLock[eveId]);
	XHCI_GenerTRB *eve = NULL;
	if (host->eveQueLen[eveId])  {
		eve = &host->eveQue[eveId][host->eveQueHdr[eveId]];
		host->eveQueHdr[eveId] = (host->eveQueHdr[eveId] + 1) % XHCI_Host_EveQueSize;
		host->eveQueLen[eveId]--;
	}
	SpinLock_unlock(&host->eveLock[eveId]);
	if (Task_current->cpuId == host->msiDesc[0].cpuId) IO_sti();
	return eve;
}

// enqueue
static XHCI_GenerTRB *_getEmptyEvePtr(XHCI_Host *host, int eveId) {
	if (host->eveQueLen[eveId] < XHCI_Host_EveQueSize) {
		XHCI_GenerTRB *eve = &host->eveQue[eveId][host->eveQueTil[eveId]];
		host->eveQueTil[eveId] = (host->eveQueTil[eveId] + 1) % XHCI_Host_EveQueSize;
		host->eveQueLen[eveId]++;
		return eve;
	} else return NULL;
}

static __always_inline__ int _eveFull(XHCI_Host *host, int eveId) {
	return host->eveQueLen[eveId] == XHCI_Host_EveQueSize;
}

void HW_USB_XHCI_init(PCIeManager *pci) {
	// check the capability list
	if (!(pci->cfg->status & (1 << 4))) {
		printk(RED, BLACK, "XHCI: PCI %#018lx has no capability list\n", pci);
		return ;
	}
	XHCI_Host *host = kmalloc(sizeof(XHCI_Host), Slab_Flag_Clear, NULL);
	List_init(&host->listEle);
	printk(WHITE, BLACK, "capPtr:%x\n", pci->cfg->type0.capPtr);
	for (PCIe_CapabilityHeader *hdr = HW_PCIe_getNxtCapHdr(pci->cfg, NULL); hdr; hdr = HW_PCIe_getNxtCapHdr(pci->cfg, hdr)) {
		switch (hdr->capId) {
			case  PCIe_CapId_MSI:
				host->msiCapDesc = container(hdr, PCIe_MSICapability, hdr);
				break;
			case PCIe_CapId_MSIX:
				host->msixCapDesc = container(hdr, PCIe_MSIXCap, hdr);
				break;
		}
	}
	if (!host->msiCapDesc && !host->msixCapDesc) {
		printk(RED, BLACK, "XHCI %#018lx: no MSI/MSI-X support.\n", host);
		kfree(host, 0);
		return ;
	}

	host->pci = pci;

	printk(WHITE, BLACK, "vendor:%04x device:%04x revision:%04x\n", pci->cfg->vendorID, pci->cfg->devID, pci->cfg->revID);
	if (pci->cfg->vendorID == 0x8086 && pci->cfg->devID == 0x1e31 && pci->cfg->revID == 4) {
		*(u32 *)((u64)pci->cfg + 0xd8) = 0xffffffff;
		*(u32 *)((u64)pci->cfg + 0xd0) = 0xffffffff;
	}
	{
		u64 phyAddr = (pci->cfg->type0.bar[0] | (((u64)pci->cfg->type0.bar[1]) << 32)) & ~0xful;
		host->capRegAddr = (u64)DMAS_phys2Virt(phyAddr);
		if (phyAddr >= MM_DMAS_bsSize) Task_mapKrlPage((u64)DMAS_phys2Virt(phyAddr), phyAddr, MM_PageTable_Flag_Presented | MM_PageTable_Flag_Writable);
	}
	#define AttemptMap(addr) do { \
		if (DMAS_virt2Phys(addr) >= MM_DMAS_bsSize) Task_mapKrlPage((u64)(addr), DMAS_virt2Phys(addr), MM_PageTable_Flag_Presented | MM_PageTable_Flag_Writable); \
	} while (0)
	AttemptMap(host->capRegAddr);
	host->opRegAddr = host->capRegAddr + HW_USB_XHCI_CapReg_capLen(host);
	AttemptMap(host->opRegAddr);
	host->rtRegAddr = host->capRegAddr + HW_USB_XHCI_CapReg_rtsOff(host);
	AttemptMap(host->rtRegAddr);
	host->dbRegAddr = host->capRegAddr + HW_USB_XHCI_CapReg_dbOffset(host);
	AttemptMap(host->dbRegAddr);
	printk(WHITE, BLACK, "XHCI: %#018lx: maxSlot:%d maxIntr:%d maxPort:%d maxScrSz:%d maxERST:%d\n", 
		host, HW_USB_XHCI_maxSlot(host), HW_USB_XHCI_maxIntr(host), HW_USB_XHCI_maxPort(host), HW_USB_XHCI_maxScrSz(host), HW_USB_XHCI_maxERST(host));
	printk(WHITE, BLACK, "\tcapReg:%#018lx opReg:%#018lx rtReg:%#018lx dbReg:%#018lx msi:%#018lx msix:%#018lx\n", 
		host->capRegAddr, host->opRegAddr, host->rtRegAddr, host->dbRegAddr,
		host->msiCapDesc, host->msixCapDesc);

	// check if the host controller support neccessary feature
	if (!(HW_USB_XHCI_readOpReg(host, XHCI_OpReg_pgSize) & 0x1)) {
		printk(RED, BLACK, "XHCI: %#018lx: no support for 4K page, pgSize reigster=%#010x\n", host, HW_USB_XHCI_readOpReg(host, XHCI_OpReg_pgSize));
		kfree(host, 0);
		return ;
	}
	if (!(HW_USB_XHCI_CapReg_hccParam(host, 1) & 1)) {
		printk(RED, BLACK, "XHCI: %#018lx: no support for 64-bit address\n", host);
		kfree(host, 0);
		return ;
	}
	if (HW_USB_XHCI_CapReg_hccParam(host, 1) & (1 << 2)) {
		printk(WHITE, BLACK, "XHCI: %#018lx: context size=8 bytes\n", host);
		host->ctxSize = 64;
	} else host->ctxSize = 32;
	if (HW_USB_XHCI_CapReg_hccParam(host, 1) & (1 << 3)) {
		printk(RED, BLACK, "XHCI: %#018lx: no support for port power control\n", host);
		kfree(host, 0);
		return ;
	}

	// stop the host
	HW_USB_XHCI_writeOpReg(host, XHCI_OpReg_cmd, HW_USB_XHCI_readOpReg(host, XHCI_OpReg_cmd) & ~1u);
	int timeout = 30;
	do {
		if (HW_USB_XHCI_readOpReg(host, XHCI_OpReg_status) & (1 << 0)) break;
		Intr_SoftIrq_Timer_mdelay(1);
		timeout--;
	} while (timeout > 0);
	if (!(HW_USB_XHCI_readOpReg(host, XHCI_OpReg_status) & (1 << 0))) {
		printk(RED, BLACK, "XHCI: %#018lx: failed to stop.\n", host);
		return ;
	}
	// reset the host
	if (!HW_USB_XHCI_reset(host)) {
		printk(RED, BLACK, "XHCI: %#018lx: failed to reset.\n", host);
		kfree(host, 0);
		return ;
	}
	HW_USB_XHCI_waiForHostIsReady(host);

	// set max slot field of config register
	HW_USB_XHCI_writeOpReg(host, XHCI_OpReg_cfg, 
		(HW_USB_XHCI_readOpReg(host, XHCI_OpReg_cfg) & ((1ul << 10) - 1)) | (1ul << 8) | HW_USB_XHCI_maxSlot(host));

	// determine numbers of interrupt and numbers of event handle task
	host->enabledIntrNum = min(4, HW_USB_XHCI_maxIntr(host));
	host->eveHandleTaskNum = min(host->enabledIntrNum, 4);


	// set up msix/msi first
	if (host->msixCapDesc) {
		int vecNum = host->msixCapDesc->msgCtrl & ((1u << 11) - 1);
		PCIe_MSIX_Table *tbl = HW_PCIe_MSIX_getTable(host->pci->cfg, host->msixCapDesc);
		printk(WHITE, BLACK, "XHCI: %#018lx: msix:%#018lx vecNum:%d msgCtrl:%#010x\n", host, tbl, vecNum + 1, host->msixCapDesc->msgCtrl);
		host->msiDesc = kmalloc(sizeof(PCIe_MSI_Descriptor) * (vecNum + 1), Slab_Flag_Clear, NULL);
		int cpuId = 0; u8 vec;
		for (int i = 0; i <= vecNum && i <= host->enabledIntrNum; i++) {
			SMP_allocIntrVec(1, cpuId, &cpuId, &vec);
			if (cpuId == -1) {
				printk(RED, BLACK, "XHCI: %#018lx: failed to allocate interrupt vector for intr #%d\n", host, i);
				kfree(host, 0);
				return ;
			}
			HW_PCIe_MSIX_setMsgAddr(tbl, i, SMP_getCPUInfoPkg(cpuId)->apicID, 0, HW_APIC_DestMode_Physical);
			HW_PCIe_MSIX_setMsgData(tbl, i, vec, HW_APIC_DeliveryMode_Fixed, HW_APIC_Level_Deassert, HW_APIC_TriggerMode_Edge);
			HW_PCIe_MSI_initDesc(&host->msiDesc[i], cpuId, vec, HW_USB_XHCI_msiHandler, (u64)host | i);
			HW_PCIe_MSI_setIntr(&host->msiDesc[i]);
			cpuId = (cpuId + 1) % SMP_cpuNum;
		}
	} else {
		// register MSI register
		int cpuId; u8 vecSt;
		int vecNum = (1 << ((host->msiCapDesc->msgCtrl >> 1) & 0x7));
		host->enabledIntrNum = vecNum = min(host->enabledIntrNum, vecNum);
		host->eveHandleTaskNum = min(host->eveHandleTaskNum, 4);
		SMP_allocIntrVec(vecNum, 0, &cpuId, &vecSt);
		if (cpuId == -1) {
			printk(RED, BLACK, "XHCI: %#0118lx: fail to allocate interrupt for MSI\n");
			kfree(host, 0);
			return ;
		}
		printk(WHITE, BLACK, "XHCI: %#018lx: msiCtrl:%#06x -> get interrupt vector: %#04x~%#04x on processor %d\n", 
			host, host->msiCapDesc->msgCtrl, vecSt, vecSt + vecNum - 1, cpuId);
		HW_PCIe_MSI_setMsgAddr(host->msiCapDesc, SMP_getCPUInfoPkg(cpuId)->apicID, 0, HW_APIC_DestMode_Physical);
		HW_PCIe_MSI_setMsgData(host->msiCapDesc, vecSt, HW_APIC_DeliveryMode_Fixed, HW_APIC_Level_Deassert, HW_APIC_TriggerMode_Edge);
		// allocate the MSI interrupt descriptor
		host->msiDesc = kmalloc(sizeof(PCIe_MSI_Descriptor) * vecNum, Slab_Flag_Clear, NULL);
		for (int i = 0; i < vecNum; i++) {
			// the parameter is the address of the host and the id of interrupt
			// We can easily use the OR operation to combine the two parameters, because the address of the host must be 64-aligned.
			HW_PCIe_MSI_initDesc(&host->msiDesc[i], cpuId, vecSt + i, HW_USB_XHCI_msiHandler, (u64)host | i);
			HW_PCIe_MSI_setIntr(&host->msiDesc[i]);
		}
	}
	printk(WHITE, BLACK, "XHCI: %#018lx: enable interrupt:%ld event handle task:%ld\n", host, host->enabledIntrNum, host->eveHandleTaskNum);

	// disable the INTx
	pci->cfg->command |= (1 << 10);

	if (host->msixCapDesc) {
		PCIe_MSIX_Table *tbl = HW_PCIe_MSIX_getTable(host->pci->cfg, host->msixCapDesc);
		int vecNum = (host->msixCapDesc->msgCtrl) & ((1u << 11) - 1);
		for (int i = 0; i <= vecNum; i++) HW_PCIe_MSIX_unmaskIntr(tbl, i);
		host->msixCapDesc->msgCtrl |= (1u << 15);
	} else {
		// disable mask
		if (host->msiCapDesc->msgCtrl & (1 << 8)) host->msiCapDesc->mask = 0;
		// enable the interrupt
		host->msiCapDesc->msgCtrl |= (1 << 0);
	}

	// allocate the device context base address array
	u64 *dcbaa = kmalloc(2048, Slab_Flag_Clear, NULL);
	HW_USB_XHCI_writeDCBAAP(host, DMAS_virt2Phys(dcbaa));

	// allocate the scratchpad array and items
	{
		int maxScrSz = max(64, HW_USB_XHCI_maxScrSz(host));
		u64 *scrArray = kmalloc(upAlignTo((maxScrSz + 1) * sizeof(u64), 0x1000), Slab_Flag_Clear, NULL);
		for (int i = 0; i <= maxScrSz; i++)
			scrArray[i] = DMAS_virt2Phys(kmalloc(0x1000, Slab_Flag_Clear, NULL));
		dcbaa[0] = DMAS_virt2Phys(scrArray);
	}
	// allocate device context and set dcbaa items
	host->devCtx = kmalloc(max(64, sizeof(XHCI_DevCtx *) * (HW_USB_XHCI_maxSlot(host) + 1)), Slab_Flag_Clear, NULL);
	host->devCtx[0] = (XHCI_DevCtx *)DMAS_phys2Virt(dcbaa[0]);
	for (int i = HW_USB_XHCI_maxSlot(host); i > 0; i--) {
		host->devCtx[i] = kmalloc(sizeof(XHCI_DevCtx), Slab_Flag_Clear, NULL);
		dcbaa[i] = DMAS_virt2Phys(host->devCtx[i]);
	}
	host->dev = kmalloc(sizeof(XHCI_Device *) * (HW_USB_XHCI_maxSlot(host) + 1), Slab_Flag_Clear, NULL);
	
	// release this host from BIOS
	for (void *ecp = HW_USB_XHCI_getNxtECP(host, NULL); ecp; ecp = HW_USB_XHCI_getNxtECP(host, ecp)) {
		if (HW_USB_XHCI_ECP_id(ecp) == XHCI_Ext_Id_Legacy) {
			printk(WHITE, BLACK, "XHCI: %#018lx: legacy support previous value: %#010x\n", HW_USB_XHCI_readDword((u64)ecp));
			HW_USB_XHCI_writeDword((u64)ecp, HW_USB_XHCI_readDword((u64)ecp) | (1u << 24));
			int timeout = 10;
			while (timeout--) {
				u32 cur = HW_USB_XHCI_readDword((u64)ecp) & ((1u << 24) | (1u << 16));
				if (cur == (1u << 24)) break; 
				Intr_SoftIrq_Timer_mdelay(1);
			}
			break;
		}
	}

	// set up the port pairs from the protocol description
	u32 usb3C = 0, usb2C = 0;
	host->port = kmalloc(sizeof(XHCI_PortInfo) * (HW_USB_XHCI_maxPort(host)), Slab_Flag_Clear, NULL);
	for (void *ecp = HW_USB_XHCI_getNxtECP(host, NULL); ecp; ecp = HW_USB_XHCI_getNxtECP(host, ecp)) {
		if (HW_USB_XHCI_ECP_id(ecp) == XHCI_Ext_Id_Protocol) {
			int isUSB3 = (HW_USB_XHCI_readByte((u64)ecp + 3) == 3);
			u8 offset, count;
			{
				u32 desc = HW_USB_XHCI_readDword((u64)ecp + 0x08);
				offset = desc & 0xff, count = (desc >> 8) & 0xff;
			}
			u32 *protoC = (isUSB3 ? &usb3C : &usb2C);
			for (int i = offset; i < offset + count; i++) {
				host->port[i].offset = ++*protoC;
				host->port[i].flags |= XHCI_PortInfo_Flag_Active;
				if (isUSB3)
					host->port[i].flags |= XHCI_PortInfo_Flag_isUSB3;
			}
		}
	}
	for (int i = 1; i <= HW_USB_XHCI_maxPort(host); i++) {
		for (int j = i + 1; j <= HW_USB_XHCI_maxPort(host); j++) {
			if (host->port[i].offset == host->port[j].offset && host->port[i].offset) {
				host->port[i].flags |= XHCI_PortInfo_Flag_Paired;
				host->port[j].flags |= XHCI_PortInfo_Flag_Paired;
				if ((host->port[i].flags & XHCI_PortInfo_Flag_Protocol) == XHCI_PortInfo_Flag_isUSB3)
					host->port[j].flags &= ~XHCI_PortInfo_Flag_Active;
				else host->port[i].flags &= ~XHCI_PortInfo_Flag_Active;
				host->port[i].pairOffset = j;
				host->port[j].pairOffset = i;
			}
		}
	}

	// allocate a command ring
	host->cmdRing = HW_USB_XHCI_allocRing(XHCI_Ring_maxSize);
	// construct a link trb at the end and points to the head
	{
		XHCI_GenerTRB *trb = &host->cmdRing->ring[XHCI_Ring_maxSize - 1];
		HW_USB_XHCI_TRB_setType(trb, XHCI_TRB_Type_Link);
		HW_USB_XHCI_TRB_setToggle(trb, 1);
		HW_USB_XHCI_TRB_setData(trb, DMAS_virt2Phys(&host->cmdRing->ring[0]));
	}
	// set the device notification register
	HW_USB_XHCI_writeOpReg(host, XHCI_OpReg_dnCtrl, (1 << 1) | (HW_USB_XHCI_readOpReg(host, XHCI_OpReg_dnCtrl) & ~0xffffu));

	// allocate event ring for each interrupter
	host->eveRing = kmalloc(sizeof(XHCI_EveRing *) * host->enabledIntrNum, Slab_Flag_Clear, NULL);
	
	// initialize event queue
	host->eveLock 			= kmalloc(sizeof(SpinLock) * HW_USB_XHCI_maxIntr(host), Slab_Flag_Clear, NULL);
	host->eveQue 			= kmalloc(sizeof(XHCI_GenerTRB *) * HW_USB_XHCI_maxIntr(host), Slab_Flag_Clear, NULL);
	host->eveQueHdr 		= kmalloc(sizeof(int) * HW_USB_XHCI_maxIntr(host), Slab_Flag_Clear, NULL);
	host->eveQueTil 		= kmalloc(sizeof(int) * HW_USB_XHCI_maxIntr(host), Slab_Flag_Clear, NULL);
	host->eveQueLen			= kmalloc(sizeof(int) * HW_USB_XHCI_maxIntr(host), Slab_Flag_Clear, NULL);

	for (int i = 0; i < host->enabledIntrNum; i++) {
		int eveRingTblSize = min(4, HW_USB_XHCI_maxERST(host));
		host->eveRing[i] = HW_USB_XHCI_allocEveRing(eveRingTblSize, XHCI_Ring_maxSize);
		u64 *eveRingTbl = kmalloc(max(128, sizeof(u64) * (eveRingTblSize << 1)), Slab_Flag_Clear, NULL);
		for (int j = 0; j < eveRingTblSize; j++)
			eveRingTbl[(j << 1) + 0] = DMAS_virt2Phys(host->eveRing[i]->rings[j]),
			eveRingTbl[(j << 1) + 1] = XHCI_Ring_maxSize;
		HW_USB_XHCI_writeIntrDword(host, i, XHCI_IntrReg_IMod, 0); // set the interval to be 0 microseconds
		HW_USB_XHCI_writeIntrDword(host, i, XHCI_IntrReg_TblSize, eveRingTblSize | (HW_USB_XHCI_readIntrDword(host, i, XHCI_IntrReg_TblSize) & ~0xffffu));
		HW_USB_XHCI_writeIntrQuad(host, i, XHCI_IntrReg_DeqPtr, 
			DMAS_virt2Phys(&host->eveRing[i]->rings[host->eveRing[i]->curRingId][host->eveRing[i]->curPos]) | (1ul << 3));
		HW_USB_XHCI_writeIntrQuad(host, i, XHCI_IntrReg_TblAddr, DMAS_virt2Phys(eveRingTbl) | (HW_USB_XHCI_readIntrQuad(host, i, XHCI_IntrReg_TblAddr) & 0x3fu));
		HW_USB_XHCI_writeIntrDword(host, i, XHCI_IntrReg_IMan, (1 << 1) | (1 << 0) | (HW_USB_XHCI_readIntrDword(host, i, XHCI_IntrReg_IMan) | ~0x3u));

		SpinLock_init(&host->eveLock[i]);
		host->eveQue[i] = kmalloc(sizeof(XHCI_GenerTRB) * XHCI_Host_EveQueSize, Slab_Flag_Clear, NULL);
	}
	
	// initialize the evering handle task
	host->eveHandleTask 	= kmalloc(sizeof(TaskStruct *) * host->eveHandleTaskNum, Slab_Flag_Clear, NULL);

	int evePreTask = host->enabledIntrNum / host->eveHandleTaskNum;
	for  (int i = 0; i < host->eveHandleTaskNum; i++){
		host->eveHandleTask[i] = Task_createTask((Task_Entry)HW_USB_XHCI_eveHandleTask, host, ((1ul << evePreTask) - 1) << (evePreTask * i), Task_Flag_Kernel);
	}

	// power off all the ports and restart them
	for (int i = HW_USB_XHCI_maxPort(host); i > 0; i--) HW_USB_XHCI_writePortReg(host, i, XHCI_PortReg_sc, 0);

	// configure the ports
	for (int i = HW_USB_XHCI_maxPort(host); i > 0; i--)
		HW_USB_XHCI_writePortReg(host, i, XHCI_PortReg_sc, XHCI_PortReg_sc_Power | XHCI_PortReg_sc_AllEve);
	
	// set crcr registers
	HW_USB_XHCI_writeOpRegQuad(host, XHCI_OpReg_crCtrl, DMAS_virt2Phys(host->cmdRing->ring) | 1);
	// restart the host
	HW_USB_XHCI_writeOpReg(host, XHCI_OpReg_cmd, (1 << 0) | (1 << 2) | (1 << 3));

	host->flags |= XHCI_Host_Flag_Initialized;
	
	printk(WHITE, BLACK, "XHCI: %#018lx: finish initialization. host cmd:%#010x state:%#010x\n", 
			host, HW_USB_XHCI_readOpReg(host, XHCI_OpReg_cmd), HW_USB_XHCI_readOpReg(host, XHCI_OpReg_status));

	// for qemu xhci, we need to seed port change event manually for the initialization
	if (host->pci->cfg->vendorID == 0x1b36 && host->pci->cfg->devID == 0x000d) {
		for (int i = HW_USB_XHCI_maxPort(host); i > 0; i--) {
			u32 sc = HW_USB_XHCI_readPortReg(host, i, XHCI_PortReg_sc);
			if (!(sc & 1)) continue;
			printk(WHITE, BLACK, "generate port event for port %d\n", i);
			// make an Port Status Change event
			XHCI_GenerTRB trb, *target = NULL;
			memset(&trb, 0, sizeof(XHCI_GenerTRB));
			HW_USB_XHCI_TRB_setType(&trb, XHCI_TRB_Type_PortStChg);
			HW_USB_XHCI_TRB_setData(&trb, i << 24);
			IO_cli();
			SpinLock_lock(&host->eveLock[0]);
			while ((target = _getEmptyEvePtr(host, 0)) == NULL) ;
			SpinLock_unlock(&host->eveLock[0]);
			IO_sti();
			memcpy(&trb, target, sizeof(XHCI_GenerTRB));
		}
	}
}

IntrHandlerDeclare(HW_USB_XHCI_msiHandler) {
	XHCI_Host *host = (XHCI_Host *)(arg & ~0x40);
	int intrId = arg & 0x7f;
	HW_USB_XHCI_writeOpReg(host, XHCI_OpReg_status, (1 << 3));
	// check if this interrupt is enabled
	if (!(HW_USB_XHCI_readIntrDword(host, intrId, XHCI_IntrReg_IMan) & 2)) return 0;
	// check if the event handler busy bit of DepPtr is set
	if (!(HW_USB_XHCI_readIntrQuad(host, intrId, XHCI_IntrReg_DeqPtr) & (1 << 3))) return 0;
	// clear the busy bit
	XHCI_GenerTRB *trb = NULL, *target = NULL;
	SpinLock_lock(&host->eveLock[intrId]);
	while (!_eveFull(host, intrId) && HW_USB_XHCI_EveRing_getNxt(host->eveRing[intrId], &trb) && (target = _getEmptyEvePtr(host, intrId)) != NULL)
		HW_USB_XHCI_TRB_copy(trb, target);
	SpinLock_unlock(&host->eveLock[intrId]);
	// write the dequeue pointer
	if (trb)
		HW_USB_XHCI_writeIntrQuad(host, intrId, XHCI_IntrReg_DeqPtr, DMAS_virt2Phys(trb) | (1 << 3));
	return 0;
}

void HW_USB_XHCI_portConnect(XHCI_Host *host, int portId) {
	// create the device management structure and task
	XHCI_Device *dev = kmalloc(sizeof(XHCI_Device), Slab_Flag_Clear, NULL);
	dev->host = host;
	Task_createTask((Task_Entry)HW_USB_XHCI_devMgrTask, dev, portId, Task_Flag_Kernel | Task_Flag_Inner);
	host->port[portId].dev = dev;
}
void HW_USB_XHCI_portDisconnect(XHCI_Host *host, int portId) {
	// no management structure for this device
	XHCI_Device *dev = host->port[portId].dev;
	if (!dev) return ;
	host->port[portId].dev = NULL;
	if (dev->mgrTask)
		Task_setSignal(dev->mgrTask, Task_Signal_Int);
}

void HW_USB_XHCI_eveHandleTask(XHCI_Host *host, u64 intrMap) {
	Task_kernelEntryHeader();
	while (!(host->flags & XHCI_Host_Flag_Initialized))
			IO_hlt();
	int fir = Bit_ffs(intrMap) - 1;

	while (1) {
		for (int i = fir; i < host->enabledIntrNum; i++) {
			if (!(intrMap & (1ul << i))) break;
			for (XHCI_GenerTRB *evePtr = _getEvePtr(host, i); evePtr; evePtr = _getEvePtr(host, i)) {
				switch (HW_USB_XHCI_TRB_getType(evePtr)) {
					case XHCI_TRB_Type_PortStChg : {
						int portId = evePtr->data1 >> 24;
						HW_USB_XHCI_writePortReg(host, portId, XHCI_PortReg_sc, XHCI_PortReg_sc_Power | XHCI_PortReg_sc_AllChg | XHCI_PortReg_sc_AllEve);
						HW_USB_XHCI_writeOpReg(host, XHCI_OpReg_status, (1 << 4));
						u32 sc = HW_USB_XHCI_readPortReg(host, portId, XHCI_PortReg_sc);
						if (sc & 1) {
							// this port has been enabled
							if ((sc & (1u << 1)) && ((sc >> 5) & 0xf) == 0) HW_USB_XHCI_portConnect(host, portId);
							else // this port is not enabled, then reset this port for attempting to enable it.
								HW_USB_XHCI_writePortReg(host, portId, XHCI_PortReg_sc, (1u << 4) | XHCI_PortReg_sc_Power |  XHCI_PortReg_sc_AllChg | XHCI_PortReg_sc_AllEve);
						} else HW_USB_XHCI_portDisconnect(host, portId);
						break;
					}
					case XHCI_TRB_Type_CmdCmpl : {
						XHCI_GenerTRB *cmd = DMAS_phys2Virt(*(u64 *)&evePtr->data1);
						// clear the reqSrc
						XHCI_Request *req = HW_USB_XHCI_Ring_release(host->cmdRing, HW_USB_XHCI_TRB_getPos(cmd));
						HW_USB_XHCI_TRB_copy(evePtr, &req->res);
						req->flags |= XHCI_Request_Flag_Finished;
						
						break;
					}
					case XHCI_TRB_Type_TransEve : {
						XHCI_GenerTRB *tr = DMAS_phys2Virt(*(u64 *)&evePtr->data1);
						XHCI_Device *dev = host->dev[HW_USB_XHCI_TRB_getSlot(evePtr)];
						XHCI_Ring *trRing = dev->trRing[((HW_USB_XHCI_readDword((u64)&evePtr->ctrl) >> 16) & 0x1f) - 1];
						XHCI_Request *req = HW_USB_XHCI_Ring_release(trRing, HW_USB_XHCI_TRB_getPos(tr));
						HW_USB_XHCI_TRB_copy(evePtr, &req->res);
						req->flags |= XHCI_Request_Flag_Finished;
						
						break;
					}
				}
			}
		}
		Task_releaseProcessor();
	}
}

void HW_USB_XHCI_devMgrTask_int(u64 signal, XHCI_Device *dev) {
	if (dev->slotId) {
		if (dev->driver) {
			if (dev->driver->disable) dev->driver->disable(dev);
			if (dev->driver->uninstall) dev->driver->uninstall(dev);
		}
		XHCI_Request *req = HW_USB_XHCI_allocReq(1);
		HW_USB_XHCI_TRB_setType(&req->trb[0], XHCI_TRB_Type_DisblSlot);
		HW_USB_XHCI_TRB_setSlot(&req->trb[0], dev->slotId);
		req->flags |= XHCI_Request_Flag_IsCommand;
		HW_USB_XHCI_Ring_insReq(dev->host->cmdRing, req);
		HW_USB_XHCI_writeDbReg(dev->host, 0, 0, 0);
		if (HW_USB_XHCI_Req_wait(req) != XHCI_TRB_CmplCode_Succ) {
			printk(RED, BLACK, "Unable to disable the slot %d of device:%#018lx\n", dev->slotId, dev);
		}
		dev->host->dev[dev->slotId] = NULL;
	}
	kfree(dev, 0);
	Task_kernelThreadExit(-1);
}

void HW_USB_XHCI_devMgrTask(XHCI_Device *dev, u64 rootPort) {
	Task_kernelEntryHeader();
	Task_setSignalHandler(Task_current, Task_Signal_Int, (Task_SignalHandler)HW_USB_XHCI_devMgrTask_int, (u64)dev);
	dev->mgrTask = Task_current;
	int speed = (HW_USB_XHCI_readPortReg(dev->host, rootPort, XHCI_PortReg_sc) >> 10) & ((1 << 4) - 1);
	printk(YELLOW, BLACK, "dev:%#018lx port:%d speed:%d\n", dev, rootPort, speed);
	// enable a slot for this device
	XHCI_Request *req0;
	{
		req0 = HW_USB_XHCI_allocReq(1);
		HW_USB_XHCI_TRB_setType(&req0->trb[0], XHCI_TRB_Type_EnblSlot);
		HW_USB_XHCI_Ring_insReq(dev->host->cmdRing, req0);
		if (HW_USB_XHCI_Req_ringDbWait(dev->host, 0, 0, 0, req0) != XHCI_TRB_CmplCode_Succ) {
			printk(RED, BLACK, "dev:%#018lx failed to allocate slot, code=%d\n", dev, HW_USB_XHCI_TRB_getCmplCode(&req0->res));
			dev->mgrTask = NULL;
			Task_setSignal(Task_current, Task_Signal_Int);
			while (1) IO_hlt();
		}
		dev->slotId = HW_USB_XHCI_TRB_getSlot(&req0->res);
		dev->host->dev[dev->slotId] = dev;
		dev->ctx = dev->host->devCtx[dev->slotId];
		printk(GREEN, BLACK, "dev %#018lx on slot %d\n", dev, dev->slotId);
	}
	// create input context structure
	{
		req0->flags = 0;
		HW_USB_XHCI_TRB_setType(&req0->trb[0], XHCI_TRB_Type_AddrDev);
		HW_USB_XHCI_TRB_setSlot(&req0->trb[0], dev->slotId);
		// set BSR bit
		dev->inCtx = kmalloc(0x1000, Slab_Flag_Clear | Slab_Flag_Private, NULL);
		dev->inCtx->ctrl.addFlags = (1 << 0) | (1 << 1);
		{
			XHCI_SlotCtx *slot = HW_USB_XHCI_getCtx(dev, XHCI_InCtx_Slot);
			HW_USB_XHCI_writeCtx(slot, 0, XHCI_SlotCtx_ctxEntries, 1);
			HW_USB_XHCI_writeCtx(slot, 0, XHCI_SlotCtx_speed, speed);
			HW_USB_XHCI_writeCtx(slot, 1, XHCI_SlotCtx_rootPortNum, rootPort);
		}
		{
			XHCI_EpCtx *ep0 = HW_USB_XHCI_getCtx(dev, XHCI_InCtx_CtrlEp);
			HW_USB_XHCI_writeCtx(ep0, 1, XHCI_EpCtx_epType, XHCI_EpCtx_epType_Control);
			HW_USB_XHCI_writeCtx(ep0, 1, XHCI_EpCtx_CErr, 3);
			HW_USB_XHCI_writeCtx(ep0, 1, XHCI_EpCtx_mxPackSize, HW_USB_XHCI_EpCtx_getMxPackSize0(speed));


			dev->trRing[0] = HW_USB_XHCI_allocRing(XHCI_Ring_maxSize);

			HW_USB_XHCI_writeQuad((u64)&ep0->deqPtr, DMAS_virt2Phys(dev->trRing[0]->cur));
			HW_USB_XHCI_writeCtx(ep0, 2, XHCI_EpCtx_dcs, 1);
			
			HW_USB_XHCI_writeCtx(ep0, 4, XHCI_EpCtx_aveTrbLen, 8);
		}
		HW_USB_XHCI_TRB_setData(&req0->trb[0], DMAS_virt2Phys(dev->inCtx));
		
		HW_USB_XHCI_Ring_insReq(dev->host->cmdRing, req0);
		if (HW_USB_XHCI_Req_ringDbWait(dev->host, 0, 0, 0, req0) != XHCI_TRB_CmplCode_Succ) {
			printk(RED, BLACK, "dev:%#018lx failed to address device, code=%d\n", dev, HW_USB_XHCI_TRB_getCmplCode(&req0->res));
			dev->mgrTask = NULL;
			Task_setSignal(Task_current, Task_Signal_Int);
			while (1) IO_hlt();
		}
	}
	// get the first 8 bytes of the device descriptor and modify the maxPacketSize0
	XHCI_Request *req1 = HW_USB_XHCI_allocReq(3);
	dev->devDesc = kmalloc(0xff, Slab_Flag_Private, NULL);
	HW_USB_XHCI_ctrlDataReq(req1,
			HW_USB_XHCI_TRB_mkSetup(0x80, 0x6, 0x0100, 0x0, 0x8),
			XHCI_TRB_Ctrl_Dir_In,
			dev->devDesc, 8);
	HW_USB_XHCI_Ring_insReq(dev->trRing[0], req1);
	if (HW_USB_XHCI_Req_ringDbWait(dev->host, dev->slotId, 1, 0, req1) != XHCI_TRB_CmplCode_Succ) {
		printk(RED, BLACK, "dev %#018lx: failed to get device descriptor, code=%d\n", dev, HW_USB_XHCI_TRB_getCmplCode(&req1->res));
		dev->mgrTask = NULL;
		Task_setSignal(Task_current, Task_Signal_Int);
		while (1) IO_hlt();
	}
	u32 val = (speed >= 4 ? (1u << (dev->devDesc->bMaxPackSz0)) : dev->devDesc->bMaxPackSz0);
	// the max packet size for control endpoint is not correct
	if (val != HW_USB_XHCI_EpCtx_getMxPackSize0(speed))
		// modify the control endpoint context and update evaluate context command to update
		HW_USB_XHCI_writeCtx(HW_USB_XHCI_getCtx(dev, XHCI_InCtx_CtrlEp), 1, XHCI_EpCtx_mxPackSize, val);
	HW_USB_XHCI_TRB_setType(&req0->trb[0], XHCI_TRB_Type_EvalCtx);
	HW_USB_XHCI_Ring_insReq(dev->host->cmdRing, req0);
	if (HW_USB_XHCI_Req_ringDbWait(dev->host, 0, 0, 0, req0) != XHCI_TRB_CmplCode_Succ) {
		printk(RED, BLACK, "dev %#018lx: failed to evaluate context, code=%d\n", dev, HW_USB_XHCI_TRB_getCmplCode(&req1->res));
		dev->mgrTask = NULL;
		Task_setSignal(Task_current, Task_Signal_Int);

		while (1) IO_hlt();
	}
	printk(GREEN, BLACK, "dev %#018lx address deivce & evaluate context successfully\n", dev);
	
	// get the full device descriptor
	HW_USB_XHCI_TRB_setData(&req1->trb[0], 	HW_USB_XHCI_TRB_mkSetup(0x80, 0x6, 0x0100, 0x0, 0xff));
	HW_USB_XHCI_TRB_setStatus(&req1->trb[1], HW_USB_XHCI_TRB_mkStatus(0xff, 0, 0));
	HW_USB_XHCI_Ring_insReq(dev->trRing[0], req1);
	if (HW_USB_XHCI_Req_ringDbWait(dev->host, dev->slotId, 1, 0, req1) != XHCI_TRB_CmplCode_Succ) {
		printk(RED, BLACK, "dev %#018lx: failed to get device descriptor, code=%d\n", dev, HW_USB_XHCI_TRB_getCmplCode(&req1->res));
		dev->mgrTask = NULL;
		Task_setSignal(Task_current, Task_Signal_Int);
		while (1) IO_hlt();
	}
	
	dev->cfgDesc = kmalloc(sizeof(void *) * dev->devDesc->bNumCfg, Slab_Flag_Clear | Slab_Flag_Private, NULL);
	for (int i = 0; i < dev->devDesc->bNumCfg; i++) {
		// get configuration descriptor
		dev->cfgDesc[i] = kmalloc(0xff, Slab_Flag_Private, NULL);
		HW_USB_XHCI_TRB_setData(&req1->trb[0], HW_USB_XHCI_TRB_mkSetup(0x80, 0x6, 0x0200 | i, 0x0, 0xff));
		HW_USB_XHCI_TRB_setData(&req1->trb[1], DMAS_virt2Phys(dev->cfgDesc[i]));
		HW_USB_XHCI_Ring_insReq(dev->trRing[0], req1);
		if (HW_USB_XHCI_Req_ringDbWait(dev->host, dev->slotId, 1, 0, req1) != XHCI_TRB_CmplCode_Succ) {
			printk(RED, BLACK, "dev %#018lx: failed to get configuration descriptor #%ld, code=%d\n", dev, i, HW_USB_XHCI_TRB_getCmplCode(&req1->res));
			dev->mgrTask = NULL;
			Task_setSignal(Task_current, Task_Signal_Int);
			while (1) IO_hlt();
		}
	}
	kfree(req0, Slab_Flag_Private);
	kfree(req1, Slab_Flag_Private);
	printk(YELLOW, BLACK, "dev %#018lx: search for driver\n", dev);
	while (1) {
		// search for a compatible driver
		for (List *drvList = HW_USB_XHCI_DriverList.next; drvList != &HW_USB_XHCI_DriverList; drvList = drvList->next) {
			XHCI_Driver *driver = container(drvList, XHCI_Driver, list);
			if (driver->check(dev)) {
				dev->driver = driver;
				// enable this device
				if (driver->enable) driver->enable(dev);
				dev->state = XHCI_Device_State_Enable;
				// move to the specific process in the driver for this device
				driver->process(dev);
				dev->driver = NULL;
				goto End;
			}
		}
		Intr_SoftIrq_Timer_mdelay(10);
	}
	End:
	while (1) IO_hlt();
	Task_kernelThreadExit(0);
}
