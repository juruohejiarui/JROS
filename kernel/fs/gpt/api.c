#include "api.h"
#include "../../includes/memory.h"
#include "../../includes/log.h"

int FS_GPT_scan(DiskDevice *device) {
	void *lba = kmalloc(512, Slab_Flag_Clear, NULL);
	printk(WHITE, BLACK, "GPT: scan %#018lx, read=%#018lx\n", device, device->read);
	int res = device->read(device, lba, 512, 512);
	if (res) { kfree(lba, 0); return res; }
	{
		FS_GPT_Header *hdr = lba;
		printk(WHITE, BLACK, "DiskDevice: %#018lx:\n", device);
		printk(WHITE, BLACK, 
			"\tSignature:		%8s\n"
			"\tRevision:		%d\n"
			"\tHeader Size:	 	%d\n"
			"\tCRC32:			%d\n"
			"\tcontain LBA:		%ld=%#018lx\n"
			"\talter LBA:		%ld=%#018lx\n"
			"\tpartition num:	%d\n"
			"\tparition entry size:	%d\n",
			hdr->signature, hdr->revision, hdr->hdrSize, hdr->chksum, hdr->lba, hdr->lba, hdr->alterLba, hdr->alterLba, hdr->parNum, hdr->entrySz);
	}
	kfree(lba, 0);
}