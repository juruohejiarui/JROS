#include "mgr.h"
#include "../includes/log.h"
#include "../includes/smp.h"
#include "desc.h"

int Task_pidCounter;

Atomic Task_krlPgTblSyncJiffies;
SpinLock Task_krlPgTblLock;

extern u8 Init_stack[32768];

ThreadStruct Init_thread = {
    .rsp    = (u64)(Init_stack + 32768),
    .fs     = Segment_kernelData,
    .gs     = Segment_kernelData,
    .cr2    = 0,
    .trapNum= 0,
    .errCode= 0
};

TaskMemStruct Init_mm = {0};

#pragma region scheduler

i64 _weight[50] = { 1, 2, 3, 4, 5, 6, [6 ... 49] = -1 };

struct CFS_rq Task_cfsStruct;

void Task_switchTo_inner(TaskStruct *prev, TaskStruct *next) {
    SMP_CPUInfoPkg *info = SMP_current;
    // printk(RED, BLACK, "From %#018lx, to %#018lx, rip: %#018lx\n", prev, next, next->thread->rip);
    // Intr_Gate_setTSSstruct(SMP_current->tssTable, next->tss);
	register TSS *tss = (TSS *)SMP_current->tssTable;
	tss->rsp0 = next->tss->rsp0;
	tss->ist1 = next->tss->ist1;
	tss->ist2 = next->tss->ist2;
	if (prev) {
		__asm__ volatile ( "movq %%fs, %0 \n\t" : "=a"(prev->thread->fs));
    	__asm__ volatile ( "movq %%gs, %0 \n\t" : "=a"(prev->thread->gs));
	}
    __asm__ volatile ( "movq %0, %%fs \n\t" : : "a"(next->thread->fs));
    __asm__ volatile ( "movq %0, %%gs \n\t" : : "a"(next->thread->gs));
}

void Task_releaseProcessor() {
	Task_current->flags |= Task_Flag_NeedSchedule;
	Task_current->resRunTime = 0;
	SMP_sendIPI_self(SMP_IPI_Type_Schedule, NULL);
}

void Task_updateCurState() {
	Task_current->vRunTime += _weight[Task_current->priority];
	Task_current->resRunTime -= _weight[Task_current->priority];
	if (Task_current->resRunTime <= 0) {
		Task_current->flags |= Task_Flag_NeedSchedule;
		Task_current->resRunTime = _weight[Task_current->priority] * max(1, 20 / Task_cfsStruct.taskNum[Task_current->cpuId].value);
	} else Task_current->flags &= ~Task_Flag_NeedSchedule;
}

void Task_updateAllProcessorState() {
	if (!Task_cfsStruct.flags) return ;
	SMP_sendIPI_allButSelf(SMP_IPI_Type_Schedule, NULL);
	Task_updateCurState();
}

static int _CFSTree_comparator(RBNode *a, RBNode *b) {
	TaskStruct *task1 = container(a, TaskStruct, wNode), *task2 = container(b, TaskStruct, wNode);
	return task1->vRunTime != task2->vRunTime ? (task1->vRunTime < task2->vRunTime) : (task1->pid < task2->pid);
}



void Task_schedule() {
	if (!(Task_cfsStruct.flags) || !(Task_current->flags & Task_Flag_NeedSchedule)) return ;
	Task_current->flags &= ~Task_Flag_NeedSchedule;
	if (Task_current->signal) {
		Task_current->state = Task_State_Running;
		IO_sti();
		u64 *signal = &Task_current->signal;
		for (int i = 0; i < Task_signalNum; i++)
			if (*signal & (1ul << i)) {
				*signal &= ~(1ul << i);
				printk(WHITE, BLACK, "Task %ld accept signal %d\n", Task_current->pid, i);
				// when the task handle the signal by the custom handler, then this signal is treated as "handled"
				if (Task_current->signalHandler[i])
					Task_current->signalHandler[i](i, Task_current->signalHandlerParam[i]);
				// using the default signal handler means this signal is "not handled"
				else Task_defaultSignalHandler(i);
			}
		IO_cli();
		return ;
	} else {
		IO_cli();
		SIMD_setTS();
		int cpuId = Task_current->cpuId;
		RBTree *cfsTree = &Task_cfsStruct.tree[cpuId]; 
		// insert this task into the waiting tree
		if (Task_current->priority == Task_Priority_Killed && !Task_cfsStruct.recycTskState.value) {
			RBTree_insNode(&Task_cfsStruct.killedTree, &Task_current->wNode);
			Atomic_inc(&Task_cfsStruct.killedTaskNum);
			Task_current->flags |= Task_Flag_InKillTree;
			Atomic_dec(&Task_cfsStruct.taskNum[cpuId]);
		} else RBTree_insNode(cfsTree, &Task_current->wNode);
		// get the task with least vRuntime
		RBNode *leftMost = RBTree_getMin(cfsTree);
		RBTree_delNode(cfsTree, leftMost);
		Task_switch(container(leftMost, TaskStruct, wNode));
	}	
}

#pragma endregion

#pragma region Signal

void Task_defaultSignalHandler(u64 signal) {
	printk(WHITE, BLACK, "Task %ld get signal %ld\r", Task_current->pid, signal);
	switch (signal) {
		case Task_Signal_Kill :
		case Task_Signal_Int :
		case Task_Signal_FloatError :
			Task_kernelThreadExit(-1);
			break;
		default :
			while (1) IO_hlt();
			break;
	}
}

void Task_setSignalHandler(TaskStruct *task, u64 signal, Task_SignalHandler handler, u64 param) {
	if (signal < 32) task->signalHandlerParam[signal] = param;
	task->signalHandler[signal] = handler;
}

void Task_SysSignal_Timer(u64 signal, void *arg) {
	while (1) {
		SpinLock_lock(&Task_current->timerTreeLock);
		RBNode *nd = RBTree_getMin(&Task_current->timerTree);
		Task_Timer *timer;
		if (!nd || !((timer = container(nd, Task_Timer, wNode))->flags & Task_Timer_Flag_Expire))
			break;
		RBTree_delNode(&Task_current->timerTree, nd);
		timer->flags &= ~Task_Timer_Flag_InQueue;
		SpinLock_unlock(&Task_current->timerTreeLock);
		timer->func(timer->data);
		timer->flags |= Task_Timer_Flag_Enabled;
	}
	SpinLock_unlock(&Task_current->timerTreeLock);
}

void Task_setSysSignalHandler(TaskStruct *task) {
	Task_setSignalHandler(task, Task_Signal_Timer, (Task_SignalHandler)Task_SysSignal_Timer, 0);
}

#pragma endregion

#pragma region Task Timer

void Task_Timer_irqFunc(TimerIrq *irq, TaskStruct *tsk) {
	Task_Timer *tskTimer = container(irq, Task_Timer, irq);
	tskTimer->flags |= Task_Timer_Flag_Expire;
	Task_setSignal(tsk, Task_Signal_Timer);
}

void Task_Timer_init(Task_Timer *timer, u64 jiffies) {
	memset(timer, 0, sizeof(Task_Timer));
	Intr_SoftIrq_Timer_initIrq(&timer->irq, jiffies, (void *)Task_Timer_irqFunc, Task_current);
}

int Task_Timer_modJiffies(Task_Timer *timer, u64 jiffies) {
	int res = 0, fl = 0;
	if (timer->flags & Task_Timer_Flag_InQueue) return -1;
	if (timer->flags & Task_Timer_Flag_Enabled) res = 1;
	Intr_SoftIrq_Timer_initIrq(&timer->irq, jiffies, (void *)Task_Timer_irqFunc, Task_current);
	return res;
}
// add a Task Timer
int Task_Timer_add(Task_Timer *timer) {
	if (timer->flags & Task_Timer_Flag_InQueue) return -1;
	SpinLock_lock(&Task_current->timerTreeLock);
	RBTree_insNode(&Task_current->timerTree, &timer->wNode);
	SpinLock_unlock(&Task_current->timerTreeLock);
	Intr_SoftIrq_Timer_addIrq(&timer->irq);
}

int Task_Timer_comparator(RBNode *a, RBNode *b) {
	Task_Timer 	*ta = container(a, Task_Timer, wNode),
				*tb = container(b, Task_Timer, wNode);
	return (ta->irq.expireJiffies == tb->irq.expireJiffies ? (u64)ta < (u64)tb : ta->irq.expireJiffies < tb->irq.expireJiffies);
}
#pragma endregion

extern void init(u64 (*usrEntry)(void *, u64), u64 *argPtr);

static void *_argWrap(void *arg1, u64 arg2) {
	u64 *argPkg = kmalloc(sizeof(u64) * 2, 0, NULL);
	argPkg[0] = (u64)arg1;
	argPkg[1] = arg2;
	return argPkg;
}
void Task_buildInitTask(u64 cpuId) {
	TaskStruct *task = Task_current;
	memset(task, 0, sizeof(TaskStruct) + sizeof(ThreadStruct) + sizeof(TaskMemStruct) + sizeof(TSS));
	
	// set the pointers of sub-structs
    ThreadStruct *thread = (ThreadStruct *)(task + 1);
	task->thread = thread;
	task->mem = (TaskMemStruct *)(thread + 1);
	task->tss = (TSS *)(task->mem + 1);

	task->cpuId = cpuId;

	thread->rsp = (u64)task + Task_kernelStackSize;
	thread->fs = thread->gs = Segment_kernelData;
	task->state = Task_State_Running;
	task->flags = Task_Flag_Kernel | Task_Flag_Inner;

	// execption stack pointer
	task->tss->ist1 = task->tss->ist2 = task->tss->ist3 = task->tss->ist4 = task->tss->ist5 = task->tss->ist6 = task->tss->ist7 = thread->rsp;
    // interrupt stack pointer
	task->tss->rsp0 = task->tss->rsp1 = task->tss->rsp2 = thread->rsp;

	// initalize the usage information
    List_init(&task->mem->pageUsage);
	List_init(&task->mem->kmallocUsage);

	task->mem->pgdPhyAddr = Task_KrlPagePhyAddr;
	task->pid = Task_pidCounter++;

	task->resRunTime = 20;

	// set cfsStruct
	Task_cfsStruct.taskNum[cpuId].value = 1;
	SMP_current->flags = SMP_CPUInfo_flag_APUInited;

	// set the signal handler
	Task_setSysSignalHandler(task);

	// set the timer tree
	RBTree_init(&task->timerTree, Task_Timer_comparator);
	SpinLock_init(&task->timerTreeLock);
}


TaskStruct *Task_createTask(Task_Entry entry, void *arg1, u64 arg2, u64 flag) {
    Page *krlStkPage = MM_Buddy_alloc(3, Page_Flag_Active | Page_Flag_KernelShare); // 32 KB
    // printk(YELLOW, BLACK, "pgdPhyAddr: %#018lx, tskStructPage: %#018lx\t", pgdPhyAddr, tskStructPage->phyAddr);
	// contruct basic structures
	TaskStruct *task = (TaskStruct *)DMAS_phys2Virt(krlStkPage->phyAddr);
    memset(task, 0, sizeof(TaskStruct) + sizeof(ThreadStruct) + sizeof(TaskMemStruct) + sizeof(TSS));
	
	// set the pointers of sub-structs
    ThreadStruct *thread = (ThreadStruct *)(task + 1);
	task->thread = thread;
	task->mem = (TaskMemStruct *)(thread + 1);
	task->tss = (TSS *)(task->mem + 1);
	// printk(WHITE, BLACK, "task=%#018lx ", task);

	task->flags = flag;
    task->pid = Task_pidCounter++;
	// printk(WHITE, BLACK, "pid:%ld ", task->pid);
	task->state = Task_State_Uninterruptible;

	*thread = Init_thread;
    thread->rip = (u64)Task_kernelThreadEntry;
    thread->rsp = (u64)task + Task_kernelStackSize - sizeof(PtReg);
	thread->fs = thread->gs = Segment_kernelData;

	memset(task->tss, 0, sizeof(TSS));
	{
		u64 rsp = (u64)task + Task_kernelStackSize;
    	// execption stack pointer
		task->tss->ist1 = task->tss->ist2 = task->tss->ist3 = task->tss->ist4 = task->tss->ist5 = task->tss->ist6 = task->tss->ist7 = rsp;
		// interrupt stack pointer
		task->tss->rsp0 = task->tss->rsp1 = task->tss->rsp2 = rsp;
	}
	task->resRunTime = 20;

	// prepare the first 
    PtReg regs;
    memset(&regs, 0, sizeof(PtReg));
    regs.rflags = (1 << 9);
    regs.cs = Segment_kernelCode;
    regs.ds = Segment_kernelData;
	regs.es = Segment_kernelData;
	regs.ss = Segment_kernelData;
	regs.rsp = thread->rsp;

	if (flag & Task_Flag_Kernel) {
		regs.rip = (u64)entry;
		regs.rdi = (u64)arg1;
		regs.rsi = arg2;
	} else {
		regs.rip = (u64)init;
		regs.rdi = (u64)entry;
		regs.rsi = (u64)_argWrap(arg1, arg2);
	}

	// for kernel thread, use the shared page address
	if (flag & Task_Flag_Kernel)
		task->mem->pgdPhyAddr = Task_KrlPagePhyAddr;
	else {
		u64 pgd = MM_PageTable_alloc();
		if (!pgd) {
			printk(RED, BLACK, "Failed to create task. Unable to create PGD table.\n");
			MM_Buddy_free(krlStkPage);
		}
		task->mem->pgdPhyAddr = pgd;
		// make a copy of kernel page table
		SpinLock_lock(&Task_krlPgTblLock);
		memcpy(DMAS_phys2Virt(Task_KrlPagePhyAddr), DMAS_phys2Virt(pgd), sizeof(u64) * 512);
		SpinLock_unlock(&Task_krlPgTblLock);
	}

	memcpy(&regs, (void *)((u64)task + Task_kernelStackSize - sizeof(PtReg)), sizeof(PtReg));

    // initalize the usage information
    List_init(&task->mem->pageUsage);
	List_init(&task->mem->kmallocUsage);
	task->mem->krlStkPage = krlStkPage;

	// set the signal handler
	Task_setSysSignalHandler(task);

	// set the timer tree
	RBTree_init(&task->timerTree, Task_Timer_comparator);
	SpinLock_init(&task->timerTreeLock);

	// initialize the simd structure
	task->simdRegs = NULL;

	// choose a processor
	task->cpuId = task->pid % SMP_cpuNum;
	task->vRunTime = ((TaskStruct *)SMP_getCPUInfoPkg(task->cpuId)->initStk)->vRunTime;
	// insert the task into the cfs struct
	IO_cli();
	RBTree_insNode(&Task_cfsStruct.tree[task->cpuId], &task->wNode);
	Atomic_inc(&Task_cfsStruct.taskNum[task->cpuId]);
	IO_sti();
    return task;
}

int Task_getRing() {
    u64 cs;
    __asm__ volatile ("movq %%cs, %0" : "=a"(cs));
    return cs & 3;
}

void Task_syncKrlPageTable() {
	if (Task_userSpaceManage()->krlPgTblJiffies == Task_krlPgTblSyncJiffies.value) return ;
	SpinLock_lock(&Task_krlPgTblLock);
	memcpy(DMAS_phys2Virt(Task_KrlPagePhyAddr + 256 * sizeof(u64)), DMAS_phys2Virt(Task_current->mem->pgdPhyAddr + 256 * sizeof(u64)), 256 * sizeof(u64));
	Task_userSpaceManage()->krlPgTblJiffies = Task_krlPgTblSyncJiffies.value;
	SpinLock_unlock(&Task_krlPgTblLock);
}

void Task_mapKrlPage(u64 vAddr, u64 pAddr, u64 flags) {
	SpinLock_lock(&Task_krlPgTblLock);
	Atomic_inc(&Task_krlPgTblSyncJiffies);
	MM_PageTable_map(Task_KrlPagePhyAddr, vAddr, pAddr, flags);
	SpinLock_unlock(&Task_krlPgTblLock);
}

void Task_unmapKrlPage(u64 vAddr) {
	SpinLock_lock(&Task_krlPgTblLock);
	Atomic_inc(&Task_krlPgTblSyncJiffies);
	MM_PageTable_unmap(Task_KrlPagePhyAddr, vAddr);
	SpinLock_unlock(&Task_krlPgTblLock);
}

__always_inline__ void Task_kernelEntryHeader() {
	Task_current->state = Task_State_Running;
	SIMD_setTS();
	IO_sti();
}

/// @brief when the task is finished, this function will be executed to recycle the resource that this task used. (e.g. memory, ports)
void Task_exit(int retVal) {
	for (List *pageList = Task_current->mem->pageUsage.next, *nxt = NULL; pageList != &Task_current->mem->pageUsage; pageList = Task_current->mem->pageUsage.next) {
        MM_Buddy_free(container(pageList, Page, listEle));
    }
	for (List *kmallocList = Task_current->mem->kmallocUsage.next; kmallocList != &Task_current->mem->kmallocUsage; kmallocList = Task_current->mem->kmallocUsage.next) {
		Task_KmallocUsage *usage = container(kmallocList, Task_KmallocUsage, listEle);
		kfree(usage->addr, Slab_Flag_Private);
	}
	
    if (Task_current->mem->totUsage > 0) {
        printk(RED, BLACK, "Task_exit(): task %ld: failed to recycle all the page frame, remain %ld pages.\n", Task_current->pid, Task_current->mem->totUsage);
        while (1) IO_hlt();
    }
	IO_cli();
	Task_current->priority = Task_Priority_Killed;
	IO_sti();

    while (1) Task_releaseProcessor();
}

void Task_recycleThread(void *arg1, u64 arg2) {
    Task_kernelEntryHeader();
    while (1) {
		while (Task_cfsStruct.killedTaskNum.value == 0) IO_hlt();
		Atomic_inc(&Task_cfsStruct.recycTskState);
        RBNode *rMost = RBTree_getMax(&Task_cfsStruct.killedTree);
        if (rMost != NULL) {
			TaskStruct *tsk = container(rMost, TaskStruct, wNode);
			if (!(tsk->flags & Task_Flag_InKillTree)) {
				Atomic_dec(&Task_cfsStruct.recycTskState);
				IO_hlt();
				continue;
			}
            RBTree_delNode(&Task_cfsStruct.killedTree, rMost);
			Atomic_dec(&Task_cfsStruct.killedTaskNum);
			Atomic_dec(&Task_cfsStruct.recycTskState);
            printk(BLACK, WHITE, "recycle task %d at %#018lx\n", tsk->pid, tsk);
            if (tsk->mem->pgdPhyAddr != Task_KrlPagePhyAddr) MM_PageTable_cleanMap(tsk->mem->pgdPhyAddr);
			MM_Buddy_free(tsk->mem->krlStkPage);
        } else Atomic_dec(&Task_cfsStruct.recycTskState);
		IO_hlt();
    }
    Task_kernelThreadExit(0);
}

void Task_initMgr() {
	printk(RED, BLACK, "Task_initMgr()\n");
	for (int i = 0; i < SMP_cpuNum; i++) {
		RBTree_init(&Task_cfsStruct.tree[i], _CFSTree_comparator);
		SpinLock_init(&Task_cfsStruct.lock[i]);
	}
	memset(Task_cfsStruct.taskNum, 0, sizeof(Task_cfsStruct.taskNum));
    RBTree_init(&Task_cfsStruct.killedTree, _CFSTree_comparator);
    SpinLock_init(&Task_cfsStruct.killedTreeLock);

	Task_cfsStruct.killedTaskNum.value = 0;
	Task_cfsStruct.recycTskState.value = 0;
	Task_pidCounter = 0;

	SpinLock_init(&Task_krlPgTblLock);
}