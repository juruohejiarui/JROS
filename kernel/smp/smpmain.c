#include "../includes/smp.h"
#include "../includes/simd.h"
#include "../includes/log.h"

void testT() {
	printk(WHITE, BLACK, "curCPU=%d\n", SMP_getCurCPUIndex());
}

void startSMP() {
	IO_cli();
	Task_buildInitTask(SMP_getCurCPUIndex());
	u64 rsp = (u64)SMP_current->initStk + 32768;
	Intr_Gate_setTSS(SMP_current->tssTable, rsp, rsp, rsp,
		rsp, rsp, rsp, rsp, rsp, rsp, rsp);
	Intr_Gate_loadTR(SMP_current->trIdx);
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
	SIMD_enable();
	SIMD_setTS();
	Atomic_inc(&SMP_initCpuNum);
	Task_Syscall_init();
	IO_sti();
	while (1) {
		while (Task_cfsStruct.taskNum[Task_current->cpuId].value == 1) IO_hlt();
		Task_releaseProcessor();
	}
}