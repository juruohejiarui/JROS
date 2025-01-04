#include "api.h"
#include "../../includes/memory.h"
#include "../../includes/log.h"

int FS_JRFS_mkfs(FS_Part *partition) {
	if (partition->ed - partition->st <= Page_1GSize / 512) {
		printk(RED, BLACK, "JRFS: %#018lx: partition too small\n", partition);
		return 1;
	}
	FS_JRFS_RootDesc *rootDesc = kmalloc(Page_4KSize, Slab_Flag_Clear, NULL);
	FS_readPage(partition, rootDesc, 0, 1);
	memcpy("JRFS", rootDesc->name, 4);
	// set block size to 
	rootDesc->blkSz = FS_JRFS_BlkSize;
	rootDesc->blkNum = (partition->ed - partition->st + 1) / (rootDesc->blkSz / 512);
	rootDesc->pgNum = (rootDesc->blkSz >> Page_4KShift) * rootDesc->blkNum;
	rootDesc->frPgNum = rootDesc->pgNum - (rootDesc->blkSz >> Page_4KShift);
	rootDesc->frSize = rootDesc->frPgNum << Page_4KShift;
	rootDesc->feat = FS_JRFS_RootDesc_feat_HugeDir | FS_JRFS_RootDesc_feat_HugePgGrp;
	rootDesc->rootEntryOff = upAlignTo(sizeof(FS_JRFS_RootDesc), Page_4KSize) >> Page_4KShift;
	
	// calculate offset and length for page descriptor
	rootDesc->pgDescOff = upAlignTo(rootDesc->rootEntryOff * Page_4KSize + upAlignTo(sizeof(FS_JRFS_EntryHdr) + sizeof(FS_JRFS_DirNode), Page_4KSize), Page_4KSize) >> Page_4KShift;
	rootDesc->pgDescSz = upAlignTo(sizeof(FS_JRFS_PgDesc) * rootDesc->pgNum, Page_4KSize) >> Page_4KShift;
	
	rootDesc->pgBmOff = rootDesc->pgDescOff + rootDesc->pgDescSz;
	rootDesc->pgBmSz = upAlignTo(upAlignTo(rootDesc->pgNum, 64) / 64 * sizeof(u64), Page_4KSize) >> Page_4KShift;

	rootDesc->frPgNum = rootDesc->pgNum - (rootDesc->pgBmOff + rootDesc->pgBmSz);
	rootDesc->frSize = rootDesc->frPgNum << Page_4KShift;

	printk(WHITE, BLACK,
		"JRFS: %#018lx:\n\tpgNum:%#018lx blkNum:%#018lx\n\tfrPgNum:%#018lx frSz:%#018lx\n\tpgDesc: off=%#018lx sz=%#018lx\n\tpgBitmap: off=%#018lx sz=%#018lx\n", 
		partition, rootDesc->pgNum, rootDesc->blkNum, rootDesc->frPgNum, rootDesc->frSize, 
		rootDesc->pgDescOff, rootDesc->pgDescSz, rootDesc->pgBmOff, rootDesc->pgBmSz);
	rootDesc->firDtBlk = upAlignTo((rootDesc->pgBmOff + rootDesc->pgBmSz) << Page_4KShift, rootDesc->blkSz) / rootDesc->blkSz;
	rootDesc->lstAllocBlk = rootDesc->firDtBlk;
	printk(WHITE, BLACK, "JRFS: %#018lx:firDtBlk:%#018lx lstAllocBlk:%#018lx\n", partition, rootDesc->firDtBlk, rootDesc->lstAllocBlk);
	FS_writePage(partition, rootDesc, 0, 1);

	void *lbas = kmalloc(Page_4KSize, Slab_Flag_Clear, NULL);
	// write zero to root entry list
	{
		for (int i = 0; i < 256; i++) FS_writePage(partition, lbas, rootDesc->rootEntryOff + i, 1);
	}
	// write page descriptors
	{
		{
			FS_JRFS_PgDesc *pages = lbas;
			for (int i = 0; i < FS_JRFS_PgDescPrePage; i++) pages[i].attr |= FS_JRFS_PgDesc_IsAllocated | FS_JRFS_PgDesc_IsSysPage;
			
			// write system page descriptors to array of system block
			for (u64 i = 0; i < rootDesc->firDtBlk * FS_JRFS_PgDescSzPreBlk; i++)
				printk(WHITE, BLACK, "write page descriptor (sys) %ld/%ld\r", i, rootDesc->blkNum * FS_JRFS_PgDescSzPreBlk),
				FS_writePage(partition, pages, rootDesc->pgDescOff + i, 1);
			memset(pages, 0, Page_4KSize);
			for (u64 i = rootDesc->firDtBlk * FS_JRFS_PgDescSzPreBlk; i < rootDesc->blkNum * FS_JRFS_PgDescSzPreBlk; i++)
				printk(WHITE, BLACK, "write page descriptor (data) %ld/%ld\r", i, rootDesc->blkNum * FS_JRFS_PgDescSzPreBlk),
				FS_writePage(partition, pages, rootDesc->pgDescOff + i, 1);
		}
		printk(WHITE, BLACK, "JRFS: %#018lx: finish initialize page descriptors");
		{
			u64 *bm = lbas;
			memset(bm, 0xff, Page_4KSize);
			for (u64 i = 0; i < rootDesc->firDtBlk * FS_JRFS_BmSzPreBlk; i++)
				printk(WHITE, BLACK, "write page bitmap (sys) %ld/%ld\r", i, rootDesc->blkNum * FS_JRFS_BmSzPreBlk),
				FS_writePage(partition, bm, rootDesc->pgBmOff + i, 1);
			// number of (8 * flags) pre 4K page
			memset(bm, 0, Page_4KSize);
			for (u64 i = rootDesc->firDtBlk * FS_JRFS_BmSzPreBlk; i < rootDesc->blkNum * FS_JRFS_BmSzPreBlk; i++)
				printk(WHITE, BLACK, "write page bitmap (data) %ld/%ld\r", i, rootDesc->blkNum * FS_JRFS_BmSzPreBlk),
				FS_writePage(partition, bm, rootDesc->pgBmOff + i, 1);
			
		}
		printk(WHITE, BLACK, "JRFS: %#018lx: finish initialize page bitmap");
	}
	kfree(lbas, 0);
	kfree(rootDesc, 0);
	return 0;
}

static __always_inline__ int _readBlkBm(FS_JRFS_Mgr *mgr, void *tgr, u64 blkId) {
	return FS_readPage(&mgr->partition, tgr, mgr->rootDesc->pgBmOff + FS_JRFS_BmSzPreBlk * blkId, FS_JRFS_BmSzPreBlk);
}
static __always_inline__ int _writeBlkBm(FS_JRFS_Mgr *mgr, void *tgr, u64 blkId) {
	return FS_readPage(&mgr->partition, tgr, mgr->rootDesc->pgBmOff + FS_JRFS_BmSzPreBlk * blkId, FS_JRFS_BmSzPreBlk);
}
// read ``(4K/size of descriptor)=256`` page descriptors from device
// - pgId should be aligned to multiples of 256
static __always_inline__ int _readPgDesc(FS_JRFS_Mgr *mgr, FS_JRFS_PgDesc *tgr, u64 pgId) {
	return FS_readPage(&mgr->partition, tgr, mgr->rootDesc->pgDescOff + (pgId * sizeof(FS_JRFS_PgDesc) >> Page_4KShift), 1);
}
static __always_inline__ int _writePgDesc(FS_JRFS_Mgr *mgr, FS_JRFS_PgDesc *tgr, u64 pgId) {
	return FS_readPage(&mgr->partition, tgr, mgr->rootDesc->pgDescOff + (pgId * sizeof(FS_JRFS_PgDesc) >> Page_4KShift), 1);
}

static __always_inline__ int _bmCacheComparator(RBNode *a, RBNode *b) {
	return container(a, FS_JRFS_BlkBmCache, rbNode)->blkId < container(b, FS_JRFS_BlkBmCache, rbNode)->blkId;
}

RBTree_insert(_insertBmCache, _bmCacheComparator)

static FS_JRFS_BlkBmCache *_mkBmCache(FS_JRFS_Mgr *mgr, u64 blkId) {
	FS_JRFS_BlkBmCache *cache = kmalloc(sizeof(FS_JRFS_BlkBmCache), Slab_Flag_Clear, NULL);
	cache->blkId = blkId;
	// load bitmap
	_readBlkBm(mgr, cache->bmCache64[0], blkId);
	for (int i = 0; i < FS_JRFS_PgDesc_MaxPgGrpSz; i++)
		for (u64 j = 0; j < FS_JRFS_PgDescPrePage; j += min(64, (1ul << (i + 1)))) {
			register u64 a = cache->bmCache64[i][j / 64], b = cache->bmCache64[i][(j | (1ul << i)) / 64];
			cache->bmCache64[i + 1][j / 64] = a | b;
			cache->buddyFlag[i + 1][j / 64] = a ^ b;
		}
	mgr->bmCacheSz++;
	RBTree_insNode(&mgr->bmCache, &cache->rbNode);
	return cache;
}

static void _updBmCacheJiffies(FS_JRFS_BlkBmCache *cache, int tgrLayer) {
	for (int i = FS_JRFS_PgDesc_MaxPgGrpSz; i > tgrLayer; i--) {
		if (cache->bmJiffies[i - 1] == cache->bmJiffies[i]) break;
		cache->bmJiffies[i - 1] = cache->bmJiffies[i]; 
		for (u64 j = 0; j < FS_JRFS_PgDescPrePage; j += min(64, (1ul << (i + 1)))) {
			cache->bmCache64[i - 1][j / 64] |= cache->bmCache64[i][j / 64];
			cache->bmCache64[i - 1][(j | (1ul << i)) / 64] |= cache->bmCache64[i][j / 64];
		}
	}
}

static int _flushBmCache(FS_JRFS_Mgr *mgr, FS_JRFS_BlkBmCache *cache) {
	// down
	_updBmCacheJiffies(cache, 0);
	return _writeBlkBm(mgr, cache->bmCache64[0], cache->blkId);
}

// flush and delete all the bitmap cache
static int _flushAndDelBmCache(FS_JRFS_Mgr *mgr) {
	int res = 0;
	while (mgr->bmCache.root) {
		FS_JRFS_BlkBmCache *cache = container(mgr->bmCache.root, FS_JRFS_BlkBmCache, rbNode);
		RBTree_delNode(&mgr->bmCache, &cache->rbNode);
		res |= _flushBmCache(mgr, cache);
		kfree(cache, 0);
	}
	return res;
}

static FS_JRFS_BlkBmCache *_getBmCache(FS_JRFS_Mgr *mgr, u64 blkId) {
	RBNode *cur = mgr->pgDescCache.root;
	FS_JRFS_BlkBmCache *cache;
	while (cur) {
		cache = container(cur, FS_JRFS_BlkBmCache, rbNode);
		if (cache->blkId < blkId) cur = cur->right;
		else if (cache->blkId == blkId) goto _FindCache;
		else cur = cur->left;
	}
	if (mgr->bmCacheSz == FS_JRFS_BlkBmCacheLimit) _flushAndDelBmCache(mgr);
	cache = _mkBmCache(mgr, blkId);	
	_FindCache:
	Atomic_inc(&cache->accCnt);
	return cache;
}

static __always_inline__ int _pgDescCacheComparator(RBNode *a, RBNode *b) {
	return container(a, FS_JRFS_PgDescCache, rbNode)->pgId < container(b, FS_JRFS_PgDescCache, rbNode)->pgId;
}

RBTree_insert(_insertPgDescCache, _pgDescCacheComparator)

static FS_JRFS_PgDescCache *_mkPgDescCache(FS_JRFS_Mgr *mgr, u64 pgId) {
	FS_JRFS_PgDescCache *cache = kmalloc(sizeof(FS_JRFS_PgDescCache), Slab_Flag_Clear, NULL);
	cache->pgId = pgId;
	if (_readPgDesc(mgr, cache->desc, pgId)) return NULL;
	mgr->pgDescCacheSz++;
	return cache;
}
static __always_inline__ int _flushPgDescCache(FS_JRFS_Mgr *mgr, FS_JRFS_PgDescCache *cache) {
	cache->accCnt = 0;
	return _writePgDesc(mgr, cache->desc, cache->pgId);
}
static int _flushAndDelPgDescCache(FS_JRFS_Mgr *mgr) {
	int res = 0;
	while (mgr->pgDescCache.root) {
		FS_JRFS_PgDescCache *cache = container(&mgr->pgDescCache.root, FS_JRFS_PgDescCache, rbNode);
		RBTree_delNode(&mgr->pgDescCache, &cache->rbNode);
		res |= _flushPgDescCache(mgr, cache);
		kfree(cache, 0);
	}
	mgr->pgDescCacheSz = 0;
	return res;
}

// get page descriptor cache
// - create when this page is not in cache
// - remove a random cache if the cache size is out of limit
// - pgId should be aligned to multiples of 256
static FS_JRFS_PgDescCache *_getPgDescCache(FS_JRFS_Mgr *mgr, u64 pgId) {
	RBNode *cur = mgr->pgDescCache.root;
	FS_JRFS_PgDescCache *cache;
	while (cur) {
		cache = container(cur, FS_JRFS_PgDescCache, rbNode);
		if (cache->pgId < pgId) cur = cur->right;
		else if (cache->pgId == pgId) {
			goto _FindCache;
		}
		else cur = cur->left;
	}
	// create cache
	if (mgr->pgDescCacheSz == FS_JRFS_PgDescCacheLimit) _flushAndDelPgDescCache(mgr);
	cache = _mkPgDescCache(mgr, pgId);
	_FindCache:
	cache->accCnt++;
	return cache;
}

static FS_JRFS_PgDesc _getPgDesc(FS_JRFS_Mgr *mgr, u64 pgId) {
	register FS_JRFS_PgDescCache *cache = _getPgDescCache(mgr, pgId & (~(FS_JRFS_PgDescPrePage - 1)));
	FS_JRFS_PgDesc desc = cache->desc[pgId & (FS_JRFS_PgDescPrePage - 1)];
	return desc;
}

static void _setPgDesc(FS_JRFS_Mgr *mgr, u64 pgId, FS_JRFS_PgDesc *desc) {
	register FS_JRFS_PgDescCache *cache = _getPgDescCache(mgr, pgId & (~(FS_JRFS_PgDescPrePage - 1)));
	memcpy(desc, &cache->desc[pgId & (FS_JRFS_PgDescPrePage - 1)], sizeof(FS_JRFS_PgDesc));
	if (cache->accCnt >= FS_JRFS_PgDescCacheFlushCnt) _flushPgDescCache(mgr, cache);
}

static __always_inline__ u64 _buddyOff(u64 offset, u64 sz) { return offset ^ (1ul << sz); }

// allocate page group with size=2^log2Size
// return 0 if failed
static u64 _allocPgGrp(FS_JRFS_Mgr *mgr, u8 log2Size) {
	u64 off, cur = mgr->rootDesc->lstAllocBlk;
	int sz;
	FS_JRFS_BlkBmCache *cache;
	for (u64 _i = 0; _i < mgr->rootDesc->blkNum; _i++) {
		cache = _getBmCache(mgr, cur);
		_updBmCacheJiffies(cache, log2Size);
		for (sz = log2Size; sz <= FS_JRFS_PgDesc_MaxPgGrpSz; sz++)
			for (off = 0; off < FS_JRFS_PagePreBlk; off += (1ul << sz)) {
				if (Bit_get(&cache->bmCache64[sz][off / 64], off % 64))	continue;
				goto _FindPgId;
			}
		cur++;
		if (cur == mgr->rootDesc->blkNum) cur = 0;
	}
	return 0;
	_FindPgId:
	cache->bmJiffies[sz] = ++cache->jiffies;
	cache->bmCache64[sz][off / 64] |= (1ul << (off % 64));
	u64 nxtOff = off & ~(1ul << sz), pgId = cache->blkId * FS_JRFS_PagePreBlk | off;
	// calculate the buddy flag and upper bitmap
	for (int i = sz; i < FS_JRFS_PgDesc_MaxPgGrpSz; i++, off = nxtOff, nxtOff &= ~(1ul << sz)) {
		register u64 a = cache->bmCache64[i][off], b = cache->bmCache64[i][_buddyOff(off, i)];
		cache->bmCache64[i + 1][nxtOff] = a | b;
		cache->buddyFlag[i + 1][nxtOff] = a ^ b;
		cache->bmJiffies[i + 1] = cache->jiffies;
	}
	return pgId;
}
static int _freePgGrp(FS_JRFS_Mgr *mgr, u64 pgId) {
	FS_JRFS_BlkBmCache *cache = _getBmCache(mgr, pgId / FS_JRFS_PagePreBlk);
	FS_JRFS_PgDesc desc = _getPgDesc(mgr, pgId);
	// turn it to page offset
	u64 off = pgId & (FS_JRFS_PagePreBlk - 1);
	int sz = desc.grpSz;
	_updBmCacheJiffies(cache, 0);
	cache->jiffies++;
	if (!Bit_get(&cache->bmCache64[sz][off / 64], off % 64)) return -1;
	for (int lyr = sz; lyr >= 0; lyr--) {
		cache->bmJiffies[lyr] = cache->jiffies;
		for (u64 p = off; p < off + (1ul << sz); p += (1ul << lyr))
			cache->bmCache64[lyr][p / 64] ^= (1ul << (p % 64));
	}
	desc.attr = 0;
	_setPgDesc(mgr, pgId, &desc);
	return 0;
}

static __always_inline__ int _pgCacheComparator(RBNode *a, RBNode *b) {
	return container(a, FS_JRFS_PgCache, rbNode)->pgId < container(b, FS_JRFS_PgCache, rbNode)->pgId;
}

RBTree_insert(_insertPgCache, _pgCacheComparator)

static FS_JRFS_PgCache *_mkPgCache(FS_JRFS_Mgr *mgr, u64 pgId) {
	FS_JRFS_PgCache *cache = kmalloc(sizeof(FS_JRFS_PgCache), Slab_Flag_Clear, NULL);
	cache->pgId = pgId;
	int res;
	if (res = FS_readPage(&mgr->partition, cache->page, pgId, 1)) {
		kfree(cache, 0);
		return NULL;
	}
	return cache;
}

static __always_inline__ int _flushPgCache(FS_JRFS_Mgr *mgr, FS_JRFS_PgCache *cache) {
	cache->accCnt = 0;
	return FS_writePage(&mgr->partition, cache->page, cache->pgId, 1);
}

static int _flushAndDelPgCache(FS_JRFS_Mgr *mgr) {
	int res;
	while (mgr->pgCache.root) {
		FS_JRFS_PgCache *cache = container(&mgr->pgCache.root, FS_JRFS_PgCache, rbNode);
		RBTree_delNode(&mgr->pgDescCache, &cache->rbNode);
		res |= _flushPgCache(mgr, cache);
		kfree(cache, 0);
	}
	return res;
}
static FS_JRFS_PgCache *_getPgCache(FS_JRFS_Mgr *mgr, u64 pgId) {
	RBNode *cur = mgr->pgCache.root;
	FS_JRFS_PgCache *cache;
	while (cur) {
		cache = container(cur, FS_JRFS_PgCache, rbNode);
		if (cache->pgId < pgId) cur = cur->right;
		else if (cache->pgId == pgId) goto _FindCache;
		else cur = cur->left;
	}
	// create cache
	if (mgr->pgDescCacheSz == FS_JRFS_PgDescCacheLimit) _flushAndDelPgCache(mgr);
	cache = _mkPgCache(mgr, pgId);
	_FindCache:
	cache->accCnt++;
	return cache;
}

static __always_inline__ int _accPgCache(FS_JRFS_Mgr *mgr, FS_JRFS_PgCache *cache) {
	cache->accCnt++;
	if (cache->accCnt >= FS_JRFS_PgCacheFlushCnt) return _flushPgCache(mgr, cache);
	return 0;
}

static __always_inline__ FS_JRFS_EntryHdr *_getEntryHdr(FS_JRFS_Mgr *mgr, u64 pgId) {
	return &_getPgCache(mgr, pgId)->entryHdr;
}
static __always_inline__ void *_afterModifEntryHdr(FS_JRFS_Mgr *mgr, FS_JRFS_EntryHdr *hdr) {
	_accPgCache(mgr, container(hdr, FS_JRFS_PgCache, entryHdr));
}
static __always_inline__ FS_JRFS_DirNode *_getDirNode(FS_JRFS_Mgr *mgr, u64 pgId) {
	return &_getPgCache(mgr, pgId)->dirNode;
}

static FS_JRFS_EntryHdr *_searchDirEntry(FS_JRFS_Mgr *mgr, FS_JRFS_EntryHdr *hdr, u64 hs) {
	FS_JRFS_PgCache *cache = container(hdr, FS_JRFS_PgCache, entryHdr);
	if (!(hdr->attr & FS_JRFS_FileHdr_attr_isDir)) return NULL;
	FS_JRFS_DirNode *node = &cache->dirEntry.rootNode;
	while (node) {
		u64 dig = (hs >> (node->dep << 3)) & 0xfful;
		u64 nxtPgId;
		if (!(nxtPgId = node->childPgId[dig]))
			return NULL;
		if (node->attr & FS_JRFS_DirNode_attr_isEnd) 
			return _getEntryHdr(mgr, nxtPgId);
		node = _getDirNode(mgr, nxtPgId);
	}
	return NULL;
}

static __always_inline__ FS_JRFS_File *_getFile(FS_JRFS_Mgr *mgr, u8 *path) {
	u64 pathLen = strlen(path);
	FS_JRFS_EntryHdr *curEntry = _getEntryHdr(mgr, mgr->rootDesc->rootEntryOff);
	for (u64 pos = 0, to, hs = 0; pos < pathLen; pos = to + 1) {
		to = pos;
		while (path[to] != '/') hs = BKDRHash(hs, to), to++;
		FS_JRFS_EntryHdr *subEntry = _searchDirEntry(mgr, curEntry, hs);
		if (!subEntry) return NULL;
		curEntry = subEntry;
	}
	if (curEntry->attr & FS_JRFS_FileHdr_attr_isDir) return NULL;
	// make a file structure
	FS_JRFS_File *file = kmalloc(sizeof(FS_JRFS_File), Slab_Flag_Clear, NULL);
	memcpy(&curEntry, &file->hdr, sizeof(FS_JRFS_EntryHdr));
	file->opPgId = curEntry->curPgId;
	file->off = sizeof(FS_JRFS_EntryHdr);
	return file;
}

int FS_JRFS_loadfs(FS_Part *part) {
	FS_unregisterPart(part);
	FS_JRFS_Mgr *mgr = kmalloc(sizeof(FS_JRFS_Mgr), Slab_Flag_Clear, NULL);
	memcpy(part, &mgr->partition, sizeof(FS_Part));
	kfree(part, 0);
	RBTree_init(&mgr->pgDescCache, _insertPgDescCache);
	RBTree_init(&mgr->bmCache, _insertBmCache);
	RBTree_init(&mgr->pgCache, _insertPgCache);
	FS_registerPart(&mgr->partition);

	return 0;
}
