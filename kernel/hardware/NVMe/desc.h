#ifndef __HARDWARE_NVME_DESC_H__
#define __HARDWARE_NVME_DESC_H__

#include "../PCIe.h"

typedef struct NVMe_QueDesc {
	u64 addr;
	u64 size;
} __attribute__ ((packed)) NVMe_QueDesc;

typedef struct NVMe_SubmQueEntry {
	u32 cmd;
	u32 nsid; // namespace identifier
	u64 reserved;
	u64 metaPtr;
	u32 dtPtr[4]; // data pointer
	u32 cmdSpec[6]; // command spcific
} __attribute__ ((packed)) NVMe_SubmQueEntry;

typedef struct NVMe_Request {
	NVMe_SubmQueEntry entry;
	u64 attr;
	#define NVMe_Request_attr_inQue		(1ul << 0)
	#define NVMe_Request_attr_Finished	(1ul << 1)
} NVMe_Request;

typedef struct NVMe_CmplQueEntry {
	u32 cmdSpec;
	u32 reserved;
	u16 submQueHdrPtr;
	u16 usbmQueId;
	u16 cmdId;
	u16 status;
} __attribute__ ((packed)) NVMe_CmplQueEntry;

typedef struct NVMe_QueMgr {
	NVMe_QueDesc desc;
	u64 hdr, til, len;
	u64 attr, iden;
	#define NVMe_QueMgr_attr_isSubmQue		(1ul << 0)
	#define NVMe_QueMgr_attr_isAdmQue		(1ul << 1)
	void *que;
	union {
		NVMe_Request *reqSrc;
		void *submSrc;
	};
	SpinLock lock;
} __attribute__ ((packed)) NVMe_QueMgr;

typedef struct NVMe_Host {
	List listEle;
	PCIeConfig *pci;
	void *regs;

	NVMe_QueMgr adminSubmQue, adminCmplQue;
	NVMe_QueMgr ioSubmQue[64], ioCmplQue[64];

	u64 cap, capStride;
	
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