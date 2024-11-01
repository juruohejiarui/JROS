#ifndef __HARDWARE_NVME_DESC_H__
#define __HARDWARE_NVME_DESC_H__

#include "../PCIe.h"

typedef struct NVMe_QueDesc {
	u64 addr;
	u64 size;
} __attribute__ ((packed)) NVMe_QueDesc;

typedef struct NVMe_QueMgr {
	NVMe_QueDesc desc;
	u64 hdr, til;
} __attribute__ ((packed)) NVMe_QueMgr;

typedef struct NVMe_Host {
	List listEle;
	PCIeConfig *pci;
	void *regs;

	NVMe_QueMgr adminSubmQue, adminCmplQue;
	NVMe_QueMgr ioSubmQue[64], ioCmplQue[64];
	
} NVMe_Host;

#define NVMe_Reg_Cap		0x00
#define NVMe_Reg_Version	0x08
#define NVMe_Reg_IntrMskSet	0x0C
#define NVMe_Reg_IntrMskClr	0x10
#define NVMe_Reg_Cfg		0x14
#define NVMe_Reg_Status		0x18
#define NVMe_Reg_AdmQueAttr	0x24
#define NVMe_Reg_AdmSubmQue	0x28
#define NVMe_Reg_AdmCmplQue 0x30


#endif