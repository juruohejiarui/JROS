#ifndef __HARDWARE_NVME_API_H__
#define __HARDWARE_NVME_API_H__

#include "desc.h"

void HW_NVMe_ringSubmDb(NVMe_Host *host, NVMe_QueMgr *queMgr);
void HW_NVMe_ringCmplDb(NVMe_Host *host, NVMe_QueMgr *queMgr);
void HW_NVMe_initQue(NVMe_QueMgr *queMgr, u64 queSize, u64 attr);
NVMe_Host *HW_NVMe_initDevice(PCIeConfig *pciCfg);
void HW_NVMe_init();

#endif