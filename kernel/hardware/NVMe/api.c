#include "api.h"
#include "../../includes/task.h"

NVMe_QueMgr *HW_NVMe_allocQue(u64 queSize, u64 attr) {
	NVMe_QueMgr *queMgr = kmalloc(sizeof(NVMe_QueMgr), Slab_Flag_Clear | Slab_Flag_Private, (void *)HW_NVMe_freeQue);
	queMgr->attr = attr;
	register u64 entrySize = (queMgr->attr & NVMe_QueMgr_attr_isSubmQue ? sizeof(NVMe_SubmQueEntry) : sizeof(NVMe_CmplQueEntry));
	queMgr->que = kmalloc(queSize * entrySize, Slab_Flag_Clear, NULL);
	queMgr->size = queSize;
	SpinLock_init(&queMgr->lock);
	queMgr->hdr = queMgr->til = queMgr->iden = 0;
	if (queMgr->attr & NVMe_QueMgr_attr_isSubmQue) {
		queMgr->reqSrc = kmalloc(queSize * sizeof(NVMe_Request *), Slab_Flag_Clear, NULL);
	}
	return queMgr;
}

void HW_NVMe_freeQue(NVMe_QueMgr *queMgr) {
	if (queMgr->attr & NVMe_QueMgr_attr_isSubmQue) kfree(queMgr->reqSrc, 0);
	if (queMgr->que != NULL) kfree(queMgr->que, 0);
}

void HW_NVMe_insReq(NVMe_Host *host, NVMe_QueMgr *queMgr, NVMe_Request *req) {
	while (1) {
		req->attr = 0;
		SpinLock_lock(&queMgr->lock);
		if (!queMgr->reqSrc[queMgr->til]) break;
		SpinLock_unlock(&queMgr->lock);
		Task_releaseProcessor();
		continue;
	}
	queMgr->reqSrc[queMgr->til] = req;
	memcpy(&req->entry, (NVMe_SubmQueEntry *)queMgr->que + queMgr->til, sizeof(NVMe_SubmQueEntry));
	queMgr->til++;
	HW_NVMe_ringSubmDb(host, queMgr);
	if (queMgr->til == queMgr->size) queMgr->til = 0;
	SpinLock_unlock(&queMgr->lock);
}

int HW_NVMe_insReqWait(NVMe_Host *host, NVMe_QueMgr *queMgr, NVMe_Request *req) {
	HW_NVMe_insReq(host, queMgr, req);
	while (!(req->attr & NVMe_Request_attr_Finished)) Task_releaseProcessor();
	return req->res.status;
}

void HW_NVMe_mkSumEntry_IO(NVMe_SubmQueEntry *entry, u8 opcode, u32 nsid, void *data, u64 lba, u16 numBlks) {
	memset(entry, 0, sizeof(NVMe_SubmQueEntry));
	entry->cmd = opcode;
	entry->nsid = nsid;
	entry->metaPtr = DMAS_virt2Phys(data);
	*(u64 *)entry->cmdSpec = lba;
	entry->cmdSpec[2] = numBlks - 1;
}

void HW_NVMe_mkSubmEntry_NewSubm(NVMe_SubmQueEntry *entry, NVMe_QueMgr *queMgr, NVMe_QueMgr *cmplQueMgr, u8 priority) {
	memset(entry, 0, sizeof(NVMe_SubmQueEntry));
	entry->cmd = 0x1;
	entry->metaPtr = DMAS_virt2Phys(queMgr->que);
	entry->cmdSpec[0] = queMgr->iden | ((queMgr->size - 1) << 16);
	entry->cmdSpec[1] = 0x1u | (priority << 1) | ((u32)cmplQueMgr->iden << 16);
}

void HW_NVMe_mkSubmEntry_NewCmpl(NVMe_SubmQueEntry *entry, NVMe_QueMgr *queMgr, u8 intrId) {
	memset(entry, 0, sizeof(NVMe_SubmQueEntry));
	entry->cmd = 0x5;
	entry->metaPtr = DMAS_virt2Phys(queMgr->que);
	entry->cmdSpec[0] = queMgr->iden | ((queMgr->size - 1) << 16);
	entry->cmdSpec[1] = 0x3 | (intrId << 16);
}

void HW_NVMe_mkSubmEntry_Iden(NVMe_SubmQueEntry *entry, void *data, u8 type, u32 nspId) {
    memset(entry, 0, sizeof(NVMe_SubmQueEntry));
    entry->cmd = 0x6;
    *(u64 *)&entry->dtPtr[0] = DMAS_virt2Phys(data);
    entry->cmdSpec[0] = type;
    entry->nsid = nspId;
}
