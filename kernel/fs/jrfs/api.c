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
	rootDesc->rootEntryOff = upAlignTo(Page_4KSize + sizeof(FS_JRFS_RootDesc), Page_4KSize) >> Page_4KShift;
	
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
	FS_writePage(partition, lbas, rootDesc->pgDescOff, upAlignTo(sizeof(FS_JRFS_EntryHdr) + sizeof(FS_JRFS_DirNode), Page_4KSize) >> Page_4KShift);
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

	partition->mgr->updInfo(partition->mgr, partition);
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
	cache->accCnt++;
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
	return cache->desc[pgId & (FS_JRFS_PgDescPrePage - 1)];
}

static void _setPgDesc(FS_JRFS_Mgr *mgr, u64 pgId, FS_JRFS_PgDesc *desc) {
	register FS_JRFS_PgDescCache *cache = _getPgDescCache(mgr, pgId & (~(FS_JRFS_PgDescPrePage - 1)));
	memcpy(desc, &cache->desc[pgId & (FS_JRFS_PgDescPrePage - 1)], sizeof(FS_JRFS_PgDesc));
	if (cache->accCnt >= FS_JRFS_PgDescCacheFlushCnt) _flushPgDescCache(mgr, cache);
}

static __always_inline__ u64 _buddyOff(u64 offset, u64 sz) { return offset ^ (1ul << sz); }

// allocate page group with size=2^log2Size, set up page descriptor
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
	FS_JRFS_PgDesc desc = _getPgDesc(mgr, pgId);
	desc.attr = FS_JRFS_PgDesc_IsAllocated | FS_JRFS_PgDesc_IsHeadPage;
	desc.grpSz = log2Size;
	desc.nxtPg = 0;
	_setPgDesc(mgr, pgId, &desc);
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

// search for a directory entry with hash value HS in a specific directory, whose header if HDR
static FS_JRFS_PgCache *_searchDirEntry(FS_JRFS_Mgr *mgr, FS_JRFS_EntryHdr *hdr, u64 hs) {
	FS_JRFS_PgCache *cache = container(hdr, FS_JRFS_PgCache, entryHdr);
	if (!(hdr->attr & FS_JRFS_EntryHdr_attr_isDir)) return NULL;
	FS_JRFS_DirNode *node = &cache->dirEntry.rootNode;
	while (node) {
		u64 dig = (hs >> (node->dep << 3)) & 0xfful;
		u64 nxtPgId;
		if (!(nxtPgId = node->childPgId[dig]))
			return NULL;
		if (node->childAttr[dig] & FS_JRFS_DirNode_childAttr_isEnd) 
			return _getPgCache(mgr, nxtPgId);
		node = _getDirNode(mgr, nxtPgId);
	}
	return NULL;
}

static FS_JRFS_PgCache *_getDirEntry(FS_JRFS_Mgr *mgr, u8 *path, u64 pathLen) {
	if (pathLen == 0) pathLen = strlen(path);
	FS_JRFS_PgCache *curEntry = _getPgCache(mgr, mgr->rootDesc->rootEntryOff);
	for (u64 pos = 0, to, hs = 0; pos < pathLen; pos = to + 1) {
		to = pos;
		while (path[to] != '/') hs = BKDRHash(hs, to), to++;
		FS_JRFS_PgCache *subEntry = _searchDirEntry(mgr, &curEntry->entryHdr, hs);
		if (!subEntry) return NULL;
		curEntry = subEntry;
	}
	return curEntry;
}

static FS_JRFS_File *_openFile(FS_JRFS_Mgr *mgr, u8 *path) {
	FS_JRFS_PgCache *curEntry = _getDirEntry(mgr, path, 0);
	if (!curEntry || curEntry->entryHdr.attr & FS_JRFS_EntryHdr_attr_isDir) {
		printk(WHITE, BLACK, "JRFS: %#018lx: file %s does not exist.\n", mgr, path);
		return NULL;
	}
	// make a file structure
	FS_JRFS_File *file = kmalloc(sizeof(FS_JRFS_File), Slab_Flag_Clear, NULL);
	curEntry->entryHdr.attr |= FS_JRFS_EntryHdr_attr_isOpen;
	if (_accPgCache(mgr, curEntry)) { kfree(file, 0); return NULL; };
	memcpy(&curEntry->entryHdr, &file->hdr, sizeof(FS_JRFS_EntryHdr));

	file->opPgId = file->grpHdrId = curEntry->pgId;
	file->off = sizeof(FS_JRFS_EntryHdr);
	file->mgr = mgr;

	file->file.write = FS_JRFS_write;
	file->file.read = FS_JRFS_read;
	file->file.seek = FS_JRFS_seek;
	file->file.close = FS_JRFS_close;
	return file;
}

static FS_JRFS_Dir *_openDir(FS_JRFS_Mgr *mgr, u8 *path) {
	FS_JRFS_PgCache *curEntry = _getDirEntry(mgr, path, 0);
	if (!curEntry || (~curEntry->entryHdr.attr & FS_JRFS_EntryHdr_attr_isDir)) {
		printk(WHITE, BLACK, "JRFS: %#018lx: directory %s does not exists.\n", mgr, path);
		return NULL;
	}
	FS_JRFS_Dir *dir = kmalloc(sizeof(FS_JRFS_Dir), Slab_Flag_Clear, NULL);
	curEntry->entryHdr.attr |= FS_JRFS_EntryHdr_attr_isOpen;
	if (!_accPgCache(mgr, curEntry)) { kfree(dir, 0); return NULL; }
	memcpy(&curEntry->entryHdr, &dir->hdr, sizeof(FS_JRFS_EntryHdr));
	dir->mgr = mgr;
	dir->curEntryId = curEntry->entryHdr.firEntryPgId;
	dir->dir.nextEntry = FS_JRFS_nextEntry;
	dir->dir.closeDir = FS_JRFS_closeDir;
	return dir;
}

static int _linkEntry(FS_JRFS_Mgr *mgr, u64 dirEntryPgId, u64 subEntryPgId, u8 *name, u64 len, u64 hs) {
	FS_JRFS_PgCache *curPg = _getPgCache(mgr, dirEntryPgId);
	u8 index;
	FS_JRFS_DirNode *curNd = &curPg->dirEntry.rootNode;
	{
		FS_JRFS_EntryHdr *dirHdr = &curPg->entryHdr;
		if (~dirHdr->attr & FS_JRFS_EntryHdr_attr_isDir) {
			printk(RED, BLACK, "JRFS: %#018lx: failed to link directory from %#018lx to %#018lx. %#018lx is not directory.\n", mgr, dirEntryPgId, dirEntryPgId);
			return -1;
		}
		dirHdr->subEntryNum++;
		if (!dirHdr->firEntryPgId) {
			dirHdr->firEntryPgId = subEntryPgId;
			_accPgCache(mgr, curPg);

			// element->prev = element->next = element;
			FS_JRFS_PgCache *tmp = _getPgCache(mgr, subEntryPgId);
			tmp->entryHdr.lstEntryPgId = tmp->entryHdr.nxtEntryPgId = subEntryPgId;
			_accPgCache(mgr, tmp);

			curPg = _getPgCache(mgr, dirEntryPgId);
		} else {
			// insert this entry before the "first entry" of directory, 
			u64 lstEntry;
			// pos->prev = element
			FS_JRFS_PgCache *tmp = _getPgCache(mgr, dirHdr->firEntryPgId);
			lstEntry = tmp->entryHdr.lstEntryPgId;
			tmp->entryHdr.lstEntryPgId = subEntryPgId;
			_accPgCache(mgr, tmp);

			// pos->prev->next = element
			tmp = _getPgCache(mgr, lstEntry);
			tmp->entryHdr.nxtEntryPgId = subEntryPgId;
			_accPgCache(mgr, tmp);

			// element->prev = pos->prev
			// element->next = pos
			tmp = _getPgCache(mgr, subEntryPgId);
			tmp->entryHdr.lstEntryPgId = lstEntry;
			tmp->entryHdr.nxtEntryPgId = dirHdr->firEntryPgId;
			
			curPg = _getPgCache(mgr, dirEntryPgId);
		}
		_accPgCache(mgr, curPg);
	}
	while (1) {
		index = (hs >> (curNd->dep << 3)) & 0xff;

		FS_JRFS_PgCache *nxtPg;
		FS_JRFS_DirNode *nxtNd;
		u64 nxtPgId;
		u8 curDep;

		// there is node of next layer / of an existed entry
		if (curNd->childPgId[index]) {
			if (curNd->childAttr[index] & FS_JRFS_DirNode_childAttr_isEnd) {
				// this node points to an existed entry
				// if the depth is 7, then there is hash conflict, return fail code
				if (curNd->dep == sizeof(u64) / 8 - 1) {
					printk(RED, BLACK, "JRFS: %#018lx: failed to link directory from %#018lx to %#018lx. hash conflict.\n",
						mgr, dirEntryPgId, subEntryPgId);
					return -1;
				}
				// create a new node
				u64 newPgId = _allocPgGrp(mgr, 0);
				if (!newPgId) {
					printk(RED, BLACK, "JRFS: %#018lx: failed to link directory from %#018lx to %#018lx. failed to allocate directory node.\n",
						mgr, dirEntryPgId, subEntryPgId);
					return -1;
				}
				curDep = curNd->dep;
				nxtPgId = curNd->childPgId[index];
				curNd->childAttr[index] &= ~FS_JRFS_DirNode_childAttr_isEnd;
				curNd->childPgId[index] = newPgId;
				_accPgCache(mgr, curPg);
				u64 neighHs = _getPgCache(mgr, nxtPgId)->entryHdr.nameHash;
				nxtPg = _getPgCache(mgr, newPgId);
				// set up the node
				nxtNd = &nxtPg->dirNode;
				memset(nxtNd, 0, sizeof(FS_JRFS_DirNode));
				nxtNd->dep = curDep + 1;
				register u8 neighIndex = (neighHs >> ((curDep + 1) << 3)) & 0xff;
				nxtNd->childAttr[neighIndex] = FS_JRFS_DirNode_childAttr_isEnd;
				nxtNd->childPgId[neighIndex] = nxtPgId;
				nxtNd->subEntryNum = 1;
				_accPgCache(mgr, nxtPg);
			} else {
				// move to next node
				nxtPg = _getPgCache(mgr, curNd->childPgId[index]);
				nxtNd = &nxtPg->dirNode;
			}
		} else // no entry for next layer, then this sub entry can be the link from curNd->child[index]
			break;
		curNd->subEntryNum++;
		curPg = nxtPg, curNd = nxtNd;
	}
	curNd->subEntryNum++;
	curNd->childAttr[index] |= FS_JRFS_DirNode_childAttr_isEnd;
	curNd->childPgId[index] = subEntryPgId;
	_accPgCache(mgr, curPg);
	return 0;
}

static int _disLinkEntry(FS_JRFS_Mgr *mgr, u64 dirEntryPgId, u64 subEntryPgId) {
	u64 hs, lstEntry, nxtEntry;
	{
		FS_JRFS_PgCache *tmp = _getPgCache(mgr, subEntryPgId);
		hs = tmp->entryHdr.nameHash;
		lstEntry = tmp->entryHdr.lstEntryPgId;
		nxtEntry = tmp->entryHdr.nxtEntryPgId;
	}
	FS_JRFS_PgCache *curPg = _getPgCache(mgr, dirEntryPgId);
	FS_JRFS_DirNode *curNd = &curPg->dirEntry.rootNode;
	u8 index; u64 curPgId = dirEntryPgId;
	{
		FS_JRFS_EntryHdr *hdr = &curPg->entryHdr;
		if (~hdr->attr & FS_JRFS_EntryHdr_attr_isDir) {
			printk(RED, BLACK, "JRFS: %#018lx: failed to dislink entry with hash %#018lx from %#018lx. %#018lx is not directory\n.", mgr, hs, dirEntryPgId, dirEntryPgId);
			return -1;
		}
		hdr->subEntryNum--;

		if (hdr->firEntryPgId == subEntryPgId) {
			if (!hdr->subEntryNum) hdr->firEntryPgId = 0;
			else hdr->firEntryPgId = lstEntry;
		}
		_accPgCache(mgr, curPg);

		// delete node
		if (hdr->subEntryNum) {
			// element->prev->next = element->next
			FS_JRFS_PgCache *tmp = _getPgCache(mgr, lstEntry);
			tmp->entryHdr.nxtEntryPgId = nxtEntry;
			_accPgCache(mgr, tmp);
			// element->next->prev = element->prev
			tmp = _getPgCache(mgr, nxtEntry);
			tmp->entryHdr.lstEntryPgId = lstEntry;
			_accPgCache(mgr, tmp);
		}

		curPg = _getPgCache(mgr, dirEntryPgId);
	}
	while (1) {
		index = (hs >> (curNd->dep << 3)) & 0xff;
		if (!curNd->childPgId[index]) {
			printk(RED, BLACK, "JRFS: %#018lx: failed to dislink entry with hash %#018lx from %#018lx\n.", mgr, hs, dirEntryPgId);
			return -1;
		}
		curNd->subEntryNum--;
		u64 nxtPgId = curNd->childPgId[index];
		if (!curNd->subEntryNum || (curNd->childAttr[index] & FS_JRFS_DirNode_childAttr_isEnd))
			curNd->childPgId[index] = 0;
			curNd->childAttr[index] = 0;
		_accPgCache(mgr, curPg);
		if (!curNd->subEntryNum && curPgId != dirEntryPgId) {
			// delete this node
			_freePgGrp(mgr, curPgId);
		}
		curPgId = nxtPgId;
		curPg = _getPgCache(mgr, nxtPgId);
		curNd = &curPg->dirNode;
	}
	return 0;
}

static int _freePgGrpList(FS_JRFS_Mgr *mgr, u64 firGrpHdrId) {
	// free page table of this file
	u64 curGrpHdrId = firGrpHdrId;
	while (1) {
		FS_JRFS_PgDesc curGrp = _getPgDesc(mgr, curGrpHdrId);
		u64 nxtGrpHdrId = curGrp.nxtPg;
		if (_freePgGrp(mgr, curGrpHdrId)) {
			printk(RED, BLACK, "JRFS: %#018lx: failed to free page group %#018lx\n", mgr, curGrpHdrId);
			return -1;
		}
		if (!nxtGrpHdrId) break;
		curGrpHdrId = nxtGrpHdrId;
	}
	return 0;
}
 
static int _mkFile(FS_JRFS_Mgr *mgr, u8 *path) {
	// get the directory node first
	u64 pathLen = strlen(path), hs = 0, to, dirPgId;
	for (u64 i = 0; i < pathLen; i++) if (path[i] =='/') to = i;
	FS_JRFS_PgCache *dir = _getDirEntry(mgr, path, to - 1);
	if (!dir) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create file %s. path does not exist.\n", mgr, path);
		return -1;
	}
	dirPgId = dir->entryHdr.curPgId;
	for (u64 pos = to + 1; pos < pathLen; pos++) hs = BKDRHash(hs, path[pos]);
	if (_searchDirEntry(mgr, &dir->entryHdr, hs)) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create file %s. hash conflict with %#018lx.\n", mgr, path, hs);
		return -1;
	}
	_createFile:
	// allocate a page for this file
	u64 pgId = _allocPgGrp(mgr, 0);
	if (!pgId) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create file %s. failed to allocate page.\n", mgr, path);
		return -1;
	}
	FS_JRFS_PgCache *cache = _getPgCache(mgr, pgId);
	if (!cache) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create file %s. failed to access page cache.\n", mgr, path);
		return -1;
	}
	{
		FS_JRFS_EntryHdr *hdr = &cache->entryHdr;
		memset(hdr, 0, sizeof(FS_JRFS_EntryHdr));
		hdr->curPgId = pgId;
		hdr->filePgNum = 1;
		hdr->fileSz = (hdr->filePgNum << Page_4KShift);
		hdr->parDirPgId = dirPgId;
	}
	_accPgCache(mgr, cache);
	register int res = _linkEntry(mgr, dirPgId, pgId, path + to + 1, pathLen - to - 1, hs);
	if (!res) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create file %s. failed to link entries.\n", mgr, path);
		_freePgGrp(mgr, pgId);
		return -1;
	}
	return 0;
}

static int _mkDir(FS_JRFS_Mgr *mgr, u8 *path) {
	u64 pathLen = strlen(path), hs = 0, to, parPgId;
	for (u64 i = 0; i < pathLen; i++) if (path[i] =='/') to = i;
	FS_JRFS_PgCache *par = _getDirEntry(mgr, path, to - 1);
	if (!par) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create file %s. path does not exist.\n", mgr, path);
		return -1;
	}
	parPgId = par->entryHdr.curPgId;
	for (u64 pos = to + 1; pos < pathLen; pos++) hs = BKDRHash(hs, path[pos]);
	if (_searchDirEntry(mgr, &par->entryHdr, hs)) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create directory %s. hash conflict with %#018lx.\n", mgr, path, hs);
		return -1;
	}
	u64 pgId = _allocPgGrp(mgr, 0);
	if (!pgId) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create directory %s. failed to allocate page.\n", mgr, path);
		return -1;
	}
	FS_JRFS_PgCache *cache = _getPgCache(mgr, pgId);
	if (!cache) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create directory %s. failed to access page cache.\n", mgr, path);
		return -1;
	}
	{
		FS_JRFS_EntryHdr *hdr = &cache->entryHdr;
		memset(hdr, 0, sizeof(FS_JRFS_EntryHdr));
		hdr->curPgId = pgId;
		hdr->attr = FS_JRFS_EntryHdr_attr_isDir;
		hdr->parDirPgId = parPgId;
	}
	_accPgCache(mgr, cache);
	register int res = _linkEntry(mgr, parPgId, pgId, path + to + 1, pathLen - to - 1, hs);
	if (!res) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to create directory %s. failed to link entries.\n", mgr, path);
		_freePgGrp(mgr, pgId);
		return -1;
	}
	return 0;
}

static int _delFile(FS_JRFS_Mgr *mgr, u8 *path) {
	FS_JRFS_PgCache *cache = _getDirEntry(mgr, path, 0);
	int res = 0;
	if (!cache) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to delete file %s. does not exists.\n", mgr, path);
		return -1;
	}
	u64 pgId = cache->pgId;
	// dislink the file from the parent directory
	{
		FS_JRFS_EntryHdr *hdr = &cache->entryHdr;
		if (hdr->attr & FS_JRFS_EntryHdr_attr_isOpen) {
			printk(RED, BLACK, "JRFS: %#018lx: failed to delete file %s. it is open.\n", mgr, path);
			return -1;
		}
		res |= _disLinkEntry(mgr, hdr->parDirPgId, pgId);
	}
	res |= _freePgGrpList(mgr, pgId);
	return res;
}

static int _delDir(FS_JRFS_Mgr *mgr, u8 *path) {
	FS_JRFS_PgCache *cache = _getDirEntry(mgr, path, 0);
	int res = 0;
	if (!cache) {
		printk(RED, BLACK, "JRFS: %#018lx: failed to delete directory %s. does not exists.\n", mgr, path);
		return -1;
	}
	u64 pgId = cache->pgId;
	// dislink the directory from the parent
	{
		FS_JRFS_EntryHdr *hdr = &cache->entryHdr;
		if (hdr->subEntryNum) {
			printk(RED, BLACK, "JRFS: %#018lx: failed to delete directory %s. Directory is not empty.\n", mgr, path);
			return -1;
		}
		if (hdr->attr & FS_JRFS_EntryHdr_attr_isOpen) {
			printk(RED, BLACK, "JRFS: %#018lx: failed to delete directory %s. it is open.\n", mgr, path);
			return -1;
		}
		res |= _disLinkEntry(mgr, hdr->parDirPgId, cache->pgId);
	}
	res |= _freePgGrp(mgr, pgId);
	return res;
}

static int _read(FS_JRFS_File *file, void *buf, u64 sz) {
	file->file.opPtr += sz;
	while (sz) {
		FS_JRFS_PgCache *cache = _getPgCache(file->mgr, file->opPgId);
		FS_JRFS_PgDesc grpDesc = _getPgDesc(file->mgr, file->grpHdrId);
		if (!cache || !(grpDesc.attr & FS_JRFS_PgDesc_IsAllocated)) return -1;
		if (sz > Page_4KSize - file->off) {
			register u64 res = Page_4KSize - file->off;
			memcpy(cache->page + file->off, buf, res);
			sz -= res;
			buf = (u8 *)buf + res;
			if ((1ul << grpDesc.grpSz) >= file->opPgId + 1 - file->grpHdrId)
				// move to next group
				file->opPgId = file->grpHdrId = grpDesc.nxtPg;
			else file->opPgId++;
			file->off = 0;
		} else {
			memcpy(cache->page + file->off, buf, sz);
			sz = 0;
			buf = (u8 *)buf + sz;
		}
	}
	return 0;
}

static int _write(FS_JRFS_File *file, void *buf, u64 sz) {
	file->file.opPtr += sz;
	while (sz) {
		FS_JRFS_PgCache *cache = _getPgCache(file->mgr, file->opPgId);
		FS_JRFS_PgDesc grpDesc = _getPgDesc(file->mgr, file->grpHdrId);
		if (!cache || !(grpDesc.attr & FS_JRFS_PgDesc_IsAllocated)) return -1;
		if (sz > Page_4KSize - file->off) {
			register u64 res = Page_4KSize - file->off;
			memcpy(buf, cache->page + file->off, res);
			_accPgCache(file->mgr, cache);
			sz -= res;
			buf = (u8 *)buf + res;
			if ((1ul << grpDesc.grpSz) >= file->opPgId + 1 - file->grpHdrId) {
				// allocate 1 page if necessary
				if (!grpDesc.nxtPg) {
					u64 pgId = _allocPgGrp(file->mgr, 0);
					if (!pgId) return -1;
					grpDesc.nxtPg = pgId;
					_setPgDesc(file->mgr, file->grpHdrId, &grpDesc);
					file->hdr.filePgNum++;
					file->hdr.fileSz += Page_4KSize;
				}
				file->opPgId = file->grpHdrId = grpDesc.nxtPg;
			} else file->opPgId++;
			file->off = 0;
		} else {
			memcpy(buf, cache->page + file->off, sz);
			_accPgCache(file->mgr, cache);
			buf = (u8 *)buf + sz;
			sz = 0;
		}
	}
	return 0;
}

static int _seek(FS_JRFS_File *file, u64 opPtr) {
	if (file->hdr.fileSz <= opPtr) return -1;
	file->file.opPtr = 0;
	file->off = sizeof(FS_JRFS_EntryHdr);
	file->opPgId = file->hdr.curPgId;
	while (opPtr != file->file.opPtr) {
		FS_JRFS_PgDesc grpDesc = _getPgDesc(file->mgr, file->grpHdrId);
		u64 res = (1ul << (grpDesc.nxtPg + Page_4KShift)) - file->off;
		if (file->file.opPtr + res < opPtr) {
			file->off = 0;
			file->grpHdrId = file->opPgId = grpDesc.nxtPg;
			file->opPgId += res;
		} else {
			file->off = (file->off + (opPtr - file->file.opPtr)) % Page_4KSize;
			file->opPgId = file->grpHdrId + ((file->off + (opPtr - file->file.opPtr)) >> Page_4KShift);
			file->file.opPtr = opPtr;
		}
	}
	return 0;
}

static int _close(FS_JRFS_File *file) {
	// write entry hdr back
	FS_JRFS_PgCache *hdrCache = _getPgCache(file->mgr, file->hdr.curPgId);
	if (!hdrCache) return -1;
	hdrCache->entryHdr.attr &= ~FS_JRFS_EntryHdr_attr_isOpen;
	hdrCache->entryHdr.modiTime = 0;
	hdrCache->entryHdr.fileSz = file->hdr.fileSz;
	hdrCache->entryHdr.filePgNum = file->hdr.filePgNum;
	_accPgCache(file->mgr, hdrCache);
	kfree(file, 0);
	return 0;
}

static int _closeDir(FS_JRFS_Dir *dir) {
	FS_JRFS_PgCache *hdrCache = _getPgCache(dir->mgr, dir->hdr.curPgId);
	if (!hdrCache) return -1;
	hdrCache->entryHdr.attr &= ~FS_JRFS_EntryHdr_attr_isOpen;
	hdrCache->entryHdr.modiTime = 0;
	_accPgCache(dir->mgr, hdrCache);
	kfree(dir, 0);
	return 0;
}

static int _nextEntry(FS_JRFS_Dir *dir, FS_DirEntry *entry) {
	if (!dir->curEntryId) return -1;
	u64 firPg = _getPgCache(dir->mgr, dir->hdr.curPgId)->entryHdr.firEntryPgId;
	FS_JRFS_PgCache *entryPg = _getPgCache(dir->mgr, dir->curEntryId);
	if (!entryPg) return -1;
	entry->attr = 0;
	memcpy(entryPg->entryHdr.name, entry->name, sizeof(u8) * FS_MaxFileNameLen);
	dir->curEntryId = entryPg->entryHdr.nxtEntryPgId;
	if (dir->curEntryId == firPg) dir->curEntryId = 0;
	return 0;
}

int FS_JRFS_read(FS_File *file, void *buf, u64 sz) {
	FS_JRFS_File *jrfsFile = container(file, FS_JRFS_File, file);
	SpinLock_lock(&jrfsFile->mgr->lock);
	register int res = _read(jrfsFile, buf, sz);
	SpinLock_unlock(&jrfsFile->mgr->lock);
	return res;
}

int FS_JRFS_write(FS_File *file, void *buf, u64 sz) {
	FS_JRFS_File *jrfsFile = container(file, FS_JRFS_File, file);
	SpinLock_lock(&jrfsFile->mgr->lock);
	register int res = _write(jrfsFile, buf, sz);
	SpinLock_unlock(&jrfsFile->mgr->lock);
	return res;
}

int FS_JRFS_seek(FS_File *file, u64 ptr) {
	FS_JRFS_File *jrfsFile = container(file, FS_JRFS_File, file);
	SpinLock_lock(&jrfsFile->mgr->lock);
	register int res = _seek(jrfsFile, ptr);
	SpinLock_unlock(&jrfsFile->mgr->lock);
	return res;
}

int FS_JRFS_close(FS_File *file) {
	FS_JRFS_File *jrfsFile = container(file, FS_JRFS_File, file);
	SpinLock_lock(&jrfsFile->mgr->lock);
	register int res = _close(jrfsFile);
	SpinLock_unlock(&jrfsFile->mgr->lock);
	return res;
}

int FS_JRFS_nextEntry(FS_Dir *dir, FS_DirEntry *entry) {
	FS_JRFS_Dir *jrfsDir = container(dir, FS_JRFS_Dir, dir);
	SpinLock_lock(&jrfsDir->mgr->lock);
	register int res = _nextEntry(jrfsDir, entry);
	SpinLock_unlock(&jrfsDir->mgr->lock);
	return res;
}

int FS_JRFS_closeDir(FS_Dir *dir) {
	FS_JRFS_Dir *jrfsDir = container(dir, FS_JRFS_Dir, dir);
	SpinLock_lock(&jrfsDir->mgr->lock);
	register int res = _closeDir(jrfsDir);
	SpinLock_unlock(&jrfsDir->mgr->lock);
	return res;
}

FS_File *FS_JRFS_openFile(FS_Part *par, u32 uid, u8 *path) {
	FS_JRFS_Mgr *mgr = container(par, FS_JRFS_Mgr, partition);
	SpinLock_lock(&mgr->lock);
	FS_JRFS_File *jrfsFile = _openFile(mgr, path);
	SpinLock_unlock(&mgr->lock);
	return &jrfsFile->file;
}

FS_Dir 	*FS_JRFS_openDir(FS_Part *par, u32 uid, u8 *path) {
	FS_JRFS_Mgr *mgr = container(par, FS_JRFS_Mgr, partition);
	SpinLock_lock(&mgr->lock);
	FS_JRFS_Dir *jrfsDir = _openDir(mgr, path);
	SpinLock_unlock(&mgr->lock);
	return &jrfsDir->dir;
}

int FS_JRFS_loadfs(FS_Part *part) {
	FS_unregisterPart(part);
	FS_JRFS_Mgr *mgr = kmalloc(sizeof(FS_JRFS_Mgr), Slab_Flag_Clear, NULL);
	memcpy(part, &mgr->partition, sizeof(FS_Part));
	kfree(part, 0);
	RBTree_init(&mgr->pgDescCache, _insertPgDescCache);
	RBTree_init(&mgr->bmCache, _insertBmCache);
	RBTree_init(&mgr->pgCache, _insertPgCache);
	SpinLock_init(&mgr->lock);

	mgr->partition.openFile = FS_JRFS_openFile;
	mgr->partition.openDir = FS_JRFS_openDir;

	FS_registerPart(&mgr->partition);

	return 0;
}