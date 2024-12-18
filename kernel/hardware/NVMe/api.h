#ifndef __HARDWARE_NVME_API_H__
#define __HARDWARE_NVME_API_H__

#include "desc.h"


static __always_inline__ void HW_NVMe_writeReg32(NVMe_Host *host, u64 offset, u32 data) {
	*(u32 *)((u64)host->regs + offset) = data;
}
static __always_inline__ void HW_NVMe_writeReg64(NVMe_Host *host, u64 offset, u64 data) {
	*(u64 *)((u64)host->regs + offset) = data;
}
static __always_inline__ u32 HW_NVMe_readReg32(NVMe_Host *host, u64 offset) {
	return *(u32 *)((u64)host->regs + offset);
}
static __always_inline__ u64 HW_NVMe_readReg64(NVMe_Host *host, u64 offset) {
	return *(u64 *)((u64)host->regs + offset);
}

static __always_inline__ void HW_NVMe_ringSubmDb(NVMe_Host *host, NVMe_QueMgr *queMgr) {
	HW_NVMe_writeReg32(host, 0x1000 + 2 * queMgr->iden * host->capStride, queMgr->til);
}
static __always_inline__ void HW_NVMe_ringCmplDb(NVMe_Host *host, NVMe_QueMgr *queMgr) {
	HW_NVMe_writeReg32(host, 0x1000 + (2 * queMgr->iden + 1) * host->capStride, queMgr->hdr);
}
static __always_inline__ u64 HW_NVMe_pageSize(NVMe_Host *host) { return Page_4KSize << host->pgSize; }

int HW_NVMe_insReqWait(NVMe_Host *host, NVMe_QueMgr *queMgr, NVMe_Request *req);

void HW_NVMe_mkSubmEntry_IO(NVMe_SubmQueEntry *entry, u8 opcode, u32 nsid, void *data, u64 lba, u16 numBlks);
void HW_NVMe_mkSubmEntry_NewSubm(NVMe_SubmQueEntry *entry, NVMe_QueMgr *queMgr, NVMe_QueMgr *cmplQueMgr, u8 priority);
void HW_NVMe_mkSubmEntry_NewCmpl(NVMe_SubmQueEntry *entry, NVMe_QueMgr *queMgr, u8 intrId);
void HW_NVMe_mkSubmEntry_Iden(NVMe_SubmQueEntry *entry, void *data, u8 type, u32 nspId);

NVMe_QueMgr *HW_NVMe_allocQue(u64 queSize, u64 attr);
void HW_NVMe_freeQue(NVMe_QueMgr *queMgr);
NVMe_Host *HW_NVMe_initDevice(PCIeConfig *pciCfg);

IntrHandlerDeclare(HW_NVMe_intrHandler);

void HW_NVMe_init();

#endif