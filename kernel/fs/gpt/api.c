#include "api.h"
#include "../api.h"
#include "../../includes/memory.h"
#include "../../includes/log.h"

int FS_GPT_scan(DiskDevice *device) {
	void *lba = kmalloc(512, Slab_Flag_Clear, NULL);
	printk(WHITE, BLACK, "GPT: scan %#018lx, read=%#018lx\n", device, device->read);
	int res = device->read(device, lba, 512, 512);
	int entryNum = 0;
	if (res) { kfree(lba, 0); return res; }
	{
		FS_GPT_Header *hdr = lba;
		printk(WHITE, BLACK, "DiskDevice: %#018lx:\n", device);
		printk(WHITE, BLACK, 
			"	Signature:		%8s\n"
			"	Revision:		%d\n"
			"	Header Size:	%d\n"
			"	CRC32:			%d\n"
			"	contain LBA:	%ld=%#018lx\n"
			"	alter LBA:		%ld=%#018lx\n"
			"	partition num:	%d\n"
			"	parition entry size:	%d\n",
			hdr->signature, hdr->revision, hdr->hdrSize, hdr->chksum, hdr->lba, hdr->lba, hdr->alterLba, hdr->alterLba, hdr->parNum, hdr->entrySz);
			entryNum = hdr->parNum;
	}
	for (int i = 0; i < 2; i++) {
		res = device->read(device, lba, 512 * (i + 2), 512);
		if (res) { kfree(lba, 0); return res; }
		FS_GPT_PartitionEntry *entry = lba;
		char name[37];
		memset(name, 0, sizeof(name));
		
		for (int j = 0; j < 512 / sizeof(FS_GPT_PartitionEntry); j++) {
			for (int k = 0; k < 36; k++) name[k] = entry[j].name[k << 1 | 1];
			printk(WHITE, BLACK, "st:%20lu=%#018lx ed:%20lu=%#018lx attr=%#018lx name=%s\n",
				entry[j].stLba, entry[j].stLba, entry[j].edLba, entry[j].edLba, entry[j].attr, name);
			if (!entry[j].stLba) continue;
			FS_Partition *parition = kmalloc(sizeof(FS_Partition), Slab_Flag_Clear, NULL);
			parition->st = entry[i].stLba, parition[i].ed = entry[i].edLba;
			parition->device = device;
			FS_registerPartition(parition);
		}
	}
	kfree(lba, 0);
}