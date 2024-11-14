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
}
void HW_NVMe_ringCmplDb(NVMe_Host *host, NVMe_QueMgr *queMgr);
void HW_NVMe_initQue(NVMe_QueMgr *queMgr, u64 queSize, u64 attr);
NVMe_Host *HW_NVMe_initDevice(PCIeConfig *pciCfg);
void HW_NVMe_init();

#endif