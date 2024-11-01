#ifndef __HARDWARE_NVME_API_H__
#define __HARDWARE_NVME_API_H__

#include "desc.h"

NVMe_Host *HW_NVMe_initDevice(PCIeConfig *pciCfg);
void HW_NVMe_init();

#endif