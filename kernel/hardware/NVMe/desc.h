#ifndef __HARDWARE_NVME_DESC_H__
#define __HARDWARE_NVME_DESC_H__

#include "../PCIe.h"
#include "../diskdevice.h"

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

typedef struct NVMe_CmplQueEntry {
	u32 cmdSpec;
	u32 reserved;
	u16 submQueHdrPtr;
	u16 submQueId;
	u16 cmdId;
	u8 phaseBit : 1;
	u16 status : 15;
} __attribute__ ((packed)) NVMe_CmplQueEntry;

typedef struct NVMe_Request {
	NVMe_SubmQueEntry entry;
	NVMe_CmplQueEntry res;
	u64 attr;
	#define NVMe_Request_attr_inQue		(1ul << 0)
	#define NVMe_Request_attr_Finished	(1ul << 1)
} NVMe_Request;

typedef struct NVMe_QueMgr {
	u16 attr;
	u16 size, iden;
	union {
		u16 til;
		u16 hdr;
	};
	#define NVMe_QueMgr_attr_isSubmQue		(1ul << 0)
	#define NVMe_QueMgr_attr_isAdmQue		(1ul << 1)
	union {
		void *que;
		NVMe_SubmQueEntry *submQue;
		NVMe_CmplQueEntry *cmplQue;
	};
	union {
		struct {
			NVMe_Request **reqSrc;
			struct NVMe_QueMgr *tgrCmplQue;
		};
		u8 phaseBit;
	};
	
	SpinLock lock;
} __attribute__ ((packed)) NVMe_QueMgr;

#define NVMe_SubmEntry_Iden_Type_Nsp			0x0
#define NVMe_SubmEntry_Iden_Type_Ctrl			0x1
#define NVMe_SubmEntry_Iden_Type_ActNspList		0x2
#define NVMe_SubmEntry_Iden_Type_AllocNspList 	0x10

typedef struct NVMe_NspDesc {
	u64 size;
	u64 cap; // capacity
	u8 reserved1[10];
	u8 formatLbaSz; // formatted LBA Size
	u8 reserved2[2];
	u8 dps; // end-to-end data protection type setttings
	u8 nmic; // namespace multi-path I/O and namespace sharing capabilities
} __attribute__ ((packed)) NVMe_NspDesc;

struct NVMe_Host;

typedef struct NVMe_Nsp {
	NVMe_QueMgr *ioSubmQue[2];
	NVMe_QueMgr *ioCmplQue;
	u32 id;
	u64 sz, cap;

	DiskDevice device;
	struct NVMe_Host *host;
} NVMe_Nsp;

typedef struct NVMe_Host {
	List listEle;
	PCIeConfig *pci;
	void *regs;

	NVMe_QueMgr *adminSubmQue, *adminCmplQue;
	NVMe_QueMgr *ioSubmQue[64], *ioCmplQue[64];

	u64 cap, capStride;
	u8 pgSize;
	
	PCIe_MSIXCap *msixCapDesc;
	PCIe_MSIX_Table *msixTbl;
	PCIe_MSICap *msiCapDesc;
	PCIe_MSI_Descriptor *msiDesc;
	int enabledIntrNum, nspNum;
	NVMe_Nsp *nsp;

} NVMe_Host;

#define NVMe_Reg_Cap		0x00
#define NVMe_Reg_Version	0x08
#define NVMe_Reg_IntrMskSet	0x0C
#define NVMe_Reg_IntrMskClr	0x10
#define NVMe_Reg_Cfg		0x14
#define NVMe_Reg_Status		0x1C
// subsystem reset, reserved if bit 36 of NVMe_Reg_Cap is clear
#define NVMe_Reg_SubsysRes	0x20
#define NVMe_Reg_AdmQueAttr	0x24
#define NVMe_Reg_AdmSubmQue	0x28
#define NVMe_Reg_AdmCmplQue 0x30


#endif