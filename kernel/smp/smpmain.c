#include "../includes/smp.h"
#include "../includes/simd.h"
#include "../includes/log.h"

void testT() {
	printk(WHITE, BLACK, "curCPU=%d\n", SMP_getCurCPUIndex());
}

void startSMP() {
	u64 rsp = 0;
	SMP_CPUInfoPkg *pkg = SMP_current;
	rsp = (u64)pkg->initStk + Init_taskStackSize;	
	u32 x, y;
	// IA32_APIC_BASE register at 0x1b
	{
		u64 val = IO_readMSR(0x1b);
		Bit_set1(&val, 11);
		if (HW_APIC_supportFlag & HW_APIC_supportFlag_X2APIC) Bit_set1(&val, 10);
		IO_writeMSR(0x1b, val);
	}
	if (HW_APIC_supportFlag & HW_APIC_supportFlag_X2APIC) {
		u64 val = IO_readMSR(0x80f);
		Bit_set1(&val, 8);
		if (HW_APIC_supportFlag & HW_APIC_supportFlag_EOIBroadcase) Bit_set1(&val, 12);
		IO_writeMSR(0x80f, val);
	} else {
		// enable SVR[8] and SVR[12]
		u64 val = *(u64 *)DMAS_phys2Virt(0xfee000f0);
		Bit_set1(&val, 8);
		if (HW_APIC_supportFlag & HW_APIC_supportFlag_EOIBroadcase) Bit_set1(&val, 12);
		*(u64 *)DMAS_phys2Virt(0xfee000f0) = val;
	}
	// half of the initStk to be the trap stack
	rsp -= 0x4000ul;
	Intr_Gate_setTSS(pkg->tssTable, rsp + 0x4000ul, rsp + 0x4000ul, rsp + 0x4000ul, rsp, rsp, rsp, rsp, rsp, rsp, rsp);
	Intr_Gate_loadTR(pkg->trIdx);
	IO_sti();
	SMP_current->flags |= SMP_CPUInfo_flag_APUInited;
	Atomic_inc(&SMP_initCpuNum);
	// printk(WHITE, BLACK, "CPU %d inited\n", SMP_getCurCPUIndex());
	Task_Syscall_init();

	while (!Task_cfsStruct.flags) ;

	// wait for the first task, and then jump to it
	int idx = SMP_getCurCPUIndex();
	while (!Task_cfsStruct.taskNum[idx].value) ;
	RBNode *node = RBTree_getMin(&Task_cfsStruct.tree[idx]);
	RBTree_delNode(&Task_cfsStruct.tree[idx], node);
	TaskStruct *task = container(node, TaskStruct, wNode);
	SIMD_enable();
	SIMD_setTS();
	Task_switch_init(NULL, task);
	while (1) IO_hlt();
}