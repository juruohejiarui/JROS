#include "api.h"
#include "../api.h"
#include "../../includes/memory.h"
#include "../../includes/log.h"

static void _calcCrc32(FS_GPT_Header *hdr, DiskDevice *device) {
	u32 chksum = CRC32Init();
	u8 *lba = kmalloc(DiskDevice_LbaSize, Slab_Flag_Clear, NULL);
	for (	u64 lbaId = hdr->firGuidLba, i = 0;
			i < hdr->parNum && !device->read(device, lba, lbaId * DiskDevice_LbaSize, DiskDevice_LbaSize); 
			lbaId++, i += DiskDevice_LbaSize / sizeof(FS_GPT_PartitionEntry)) {
		for (u64 j = 0; j < DiskDevice_LbaSize; j++)
			chksum = CRC32Embed(chksum, lba[j]);
	}
	hdr->parArrayChksum = ~chksum;
	hdr->chksum = 0;
	chksum = CRC32Init();
	memset(lba, 0, DiskDevice_LbaSize);
	memcpy(hdr, lba, sizeof(FS_GPT_Header));
	for (u64 i = 0; i < DiskDevice_LbaSize; i++) chksum = CRC32Embed(chksum, lba[i]);
	hdr->chksum = ~chksum;
	printk(RED, WHITE, "par chksum: %d chksum: %d\n", hdr->parArrayChksum, hdr->chksum);
	// device->write(device, lba, DiskDevice_LbaSize, DiskDevice_LbaSize);
	kfree(lba, 0);
}

int FS_GPT_scan(DiskDevice *device) {
	void *lba = kmalloc(512, Slab_Flag_Clear, NULL);
	printk(WHITE, BLACK, "GPT: scan %#018lx, read=%#018lx\n", device, device->read);
	int res = device->read(device, lba, 512, 512);
	int entryNum = 0;
	if (res) { kfree(lba, 0); return res; }

	FS_GPT_Mgr *mgr = kmalloc(sizeof(FS_GPT_Mgr), Slab_Flag_Clear, NULL);
	SpinLock_init(&mgr->mgr.lock);
	{
		FS_GPT_Header *hdr = lba;
		printk(WHITE, BLACK, "DiskDevice: %#018lx:\n", device);
		printk(WHITE, BLACK, 
			"	Signature:		%8s\n"
			"	Revision:		%d\n"
			"	Header Size:	%d\n"
			"	CRC32:			%d\n"
			"	par CRC32:      %d\n"
			"	contain LBA:	%#018lx\n"
			"	fir GUID LBA:	%#018lx\n"
			"	partition num:	%d;	parition entry size:	%d\n",
			hdr->signature, hdr->revision, hdr->hdrSize, hdr->chksum, hdr->parArrayChksum, hdr->lba, hdr->firGuidLba, hdr->parNum, hdr->entrySz);
			entryNum = hdr->parNum;
		if (strncmp(hdr->signature, "EFI PART", 8)) { kfree(lba, 0), kfree(mgr, 0); return 1; } 

		memcpy(hdr, &mgr->hdr, sizeof(FS_GPT_Header));
		mgr->mgr.device = device;
		mgr->mgr.updInfo = FS_GPT_updParInfo;
	}
	for (int i = 0; i < entryNum / (DiskDevice_LbaSize / sizeof(FS_GPT_PartitionEntry)); i++) {
		res = device->read(device, lba, 512 * (i + 2), 512);
		if (res) { kfree(lba, 0); return res; }
		FS_GPT_PartitionEntry *entry = lba;
		char name[37];
		memset(name, 0, sizeof(name));
		
		for (int j = 0; j < DiskDevice_LbaSize / sizeof(FS_GPT_PartitionEntry); j++) {
			for (int k = 0; k < 36; k++) name[k] = entry[j].name[k << 1];
			if (!entry[j].stLba) continue;
			printk(WHITE, BLACK, "st:%20lu ed:%20lu attr=%#018lx name=%s\n",
				entry[j].stLba, entry[j].edLba, entry[j].attr, name);
			FS_Part *parition = kmalloc(sizeof(FS_Part), Slab_Flag_Clear, NULL);
			parition->st = entry[j].stLba, parition->ed = entry[j].edLba;
			parition->device = device;
			parition->mgr = &mgr->mgr;
			FS_setTypeGUID(parition, &entry[j].uniParGuid, 0);
			FS_setParTypeGUID(parition, &entry[j].parTypeGuid, 0);
			FS_registerPart(parition);
		}
	}
	_calcCrc32(&mgr->hdr, device);
	kfree(lba, 0);
	return 0;
}

int FS_GPT_updParInfo(FS_PartMgr *mgr, FS_Part *part) {
	FS_GPT_Mgr *gptMgr = container(mgr, FS_GPT_Mgr, mgr);
	SpinLock_lock(&gptMgr->mgr.lock);
	DiskDevice *device = mgr->device;
	// get the lba and offset of the partition information entry
	u64 lba = gptMgr->hdr.firLba; u32 off = 0;
	FS_GPT_PartitionEntry *entries = kmalloc(DiskDevice_LbaSize, Slab_Flag_Clear, NULL);
	for (	lba = gptMgr->hdr.firLba, device->read(device, entries, lba * DiskDevice_LbaShift, DiskDevice_LbaSize); 
			lba <= gptMgr->hdr.lstLba; 
			lba++, device->read(device, entries, lba * DiskDevice_LbaSize, DiskDevice_LbaSize)) {
		for (off = 0; off < DiskDevice_LbaSize / sizeof(FS_GPT_PartitionEntry); off++) {
			if (FS_chkGUID(&entries->uniParGuid, &part->uniTypeGuid)) goto FindParEntry;
		}
	}
	SpinLock_unlock(&gptMgr->mgr.lock);
	kfree(entries, 0);
	return -1;

	FindParEntry:
	memcpy(&part->parTypeGuid, &entries[off].parTypeGuid, sizeof(FS_GUID));
	memcpy(&part->name, &entries[off].name, sizeof(part->name));
	// write lba
	device->write(device, entries, lba * DiskDevice_LbaSize, DiskDevice_LbaSize);
	_calcCrc32(&gptMgr->hdr, device);
	SpinLock_unlock(&gptMgr->mgr.lock);
	// write it back and calculate crc32 
	kfree(entries, 0);
	return 0;
}

static void _initHdr(DiskDevice *device, FS_GPT_Header *hdr) {
	memcpy("EFI PART", hdr->signature, sizeof(hdr->signature));
	hdr->revision = 0x010000;
	hdr->hdrSize = sizeof(FS_GPT_Header);
	hdr->chksum = 0;
	hdr->reserved = 0;
	hdr->lba = 1;
	hdr->alterLba = device->size / DiskDevice_LbaSize;
	hdr->firLba = 0x22;
	hdr->lstLba = device->size / DiskDevice_LbaSize - 0x22;
	
}

int FS_GPT_init(DiskDevice *device) {
	void *lba = kmalloc(512, Slab_Flag_Clear, NULL);
	_initHdr(device, lba);

}
