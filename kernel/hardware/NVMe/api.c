#include "api.h"
#include "../../includes/task.h"
#include "../../includes/log.h"

void HW_NVMe_initQue(NVMe_QueMgr *queMgr, u64 queSize, u16 iden, u64 attr) {
	queMgr->attr = attr;
	register u64 entrySize = (queMgr->attr & NVMe_QueMgr_attr_isSubmQue ? sizeof(NVMe_SubmQueEntry) : sizeof(NVMe_CmplQueEntry));
	queMgr->que = kmalloc(queSize * entrySize, Slab_Flag_Clear, NULL);
	queMgr->size = queSize;
	queMgr->iden = iden;
	queMgr->til = 0;
	SpinLock_init(&queMgr->lock);
	if (queMgr->attr & NVMe_QueMgr_attr_isSubmQue)
		queMgr->reqSrc = kmalloc(queSize * sizeof(NVMe_Request *), Slab_Flag_Clear, NULL);
	else queMgr->phaseBit = 1;
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
	req->entry.cmd |= (u32)queMgr->til << 16;
	memcpy(&req->entry, queMgr->submQue + queMgr->til, sizeof(NVMe_SubmQueEntry));

	queMgr->til++;
	if (queMgr->til == queMgr->size) queMgr->til = 0;
	HW_NVMe_ringSubmDb(host, queMgr);
	SpinLock_unlock(&queMgr->lock);
}

int HW_NVMe_insReqWait(NVMe_Host *host, NVMe_QueMgr *queMgr, NVMe_Request *req) {
	HW_NVMe_insReq(host, queMgr, req);
	while (!(req->attr & NVMe_Request_attr_Finished)) Task_releaseProcessor();
	return req->res.status;
}

void HW_NVMe_mkSubmEntry_IO(NVMe_SubmQueEntry *entry, u8 opcode, u32 nsid, void *data, u64 lba, u16 numBlks) {
	memset(entry, 0, sizeof(NVMe_SubmQueEntry));
	entry->cmd = opcode;
	entry->nsid = nsid;
	*(u64 *)&entry->dtPtr[0] = DMAS_virt2Phys(data);
	*(u64 *)&entry->cmdSpec[0] = lba;
	entry->cmdSpec[2] = numBlks - 1;
}

void HW_NVMe_mkSubmEntry_NewSubm(NVMe_SubmQueEntry *entry, NVMe_QueMgr *queMgr, NVMe_QueMgr *cmplQueMgr, u8 priority) {
	memset(entry, 0, sizeof(NVMe_SubmQueEntry));
	entry->cmd = 0x1;
	*(u64 *)&entry->dtPtr[0] = DMAS_virt2Phys(queMgr->submQue);
	entry->cmdSpec[0] = queMgr->iden | ((queMgr->size - 1) << 16);
	entry->cmdSpec[1] = 0x1u | (priority << 1) | ((u32)cmplQueMgr->iden << 16);
}

void HW_NVMe_mkSubmEntry_NewCmpl(NVMe_SubmQueEntry *entry, NVMe_QueMgr *queMgr, u8 intrId) {
	memset(entry, 0, sizeof(NVMe_SubmQueEntry));
	entry->cmd = 0x5;
	*(u64 *)&entry->dtPtr[0] = DMAS_virt2Phys(queMgr->cmplQue);
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
