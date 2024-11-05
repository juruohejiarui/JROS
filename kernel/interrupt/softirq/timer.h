#ifndef __INTERRUPT_SOFTIRQ_TIMER_H__
#define __INTERRUPT_SOFTIRQ_TIMER_H__

#include "../softirq.h"

typedef struct TimerIrq {
	u64 expireJiffies;
	void (*func)(struct TimerIrq *irq, void *data);
	void *data;
	// rbnode for timer tree
	RBNode rbNode;
	u64 id;
} TimerIrq;

void Intr_SoftIrq_Timer_initIrq(TimerIrq *irq, u64 expireJiffies, void (*func)(TimerIrq *irq, void *data), void *data);

void Intr_SoftIrq_Timer_init();

void Intr_SoftIrq_Timer_addIrq(TimerIrq *irq);

void Intr_SoftIrq_Timer_updateState();

void Intr_SoftIrq_Timer_mdelay(u64 msec);

void Intr_SoftIrq_Timer_rbTreeIns(RBTree *tree, RBNode *node, RBNode ***tgr, RBNode **par);

#endif