#ifndef __LIB_RB_TREE_H__
#define __LIB_RB_TREE_H__

#include "ds.h"
#include "spinlock.h"

#define RBTree_Color_Red 0
#define RBTree_Color_Black 1

typedef struct RBNode {
	u64 unionParCol;
	struct RBNode *left, *right;
} __attribute__((aligned(sizeof(long)))) RBNode;

#define RBTree_Col_Red		0
#define RBTree_Col_Black	1

#define RBTree_insert(name, comparator) \
static void name(RBTree *tree, RBNode *node, RBNode ***tgr, RBNode **par) { \
	RBNode **src = &tree->root, *lst; \
	while (*src) { \
		lst = *src; \
		if (comparator(node, lst)) \
			src = &(*src)->left; \
		else src = &(*src)->right; \
	} \
	*tgr = src, *par = lst; \
}


struct RBTree;

// the comparator for RBTree, return 1 if a < b, 0 if a > b; invalid for a == b
typedef void (*RBTreeInsert)(struct RBTree *tree, RBNode *node, RBNode ***tgr, RBNode **par);
typedef struct RBTree {
	RBNode *root;
	SpinLock lock;
	RBTreeInsert insert;
} RBTree;

void RBTree_debug(RBTree *tree);

void RBTree_init(RBTree *tree, RBTreeInsert insert);

void RBTree_insNode(RBTree *tree, RBNode *node);
void RBTree_delNode(RBTree *tree, RBNode *node);

RBNode *RBTree_getMin(RBTree *tree);
RBNode *RBTree_getMax(RBTree *tree);

RBNode *RBTree_getNext(RBTree *tree, RBNode *node);

#endif