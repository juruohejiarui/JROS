#include "includes/lib.h"
#include "includes/log.h"
#include "includes/hardware.h"
#include "includes/interrupt.h"
#include "includes/memory.h"
#include "includes/task.h"
#include "includes/hardware.h"
#include "includes/smp.h"
#include "includes/simd.h"
#include "includes/fs.h"


u8 Init_stack[32768] __attribute__((__section__ (".data.Init_stack") )) = { 0 };

void startKernel() {
	SMP_cpuNum = -1;
	
    Intr_Gate_setTSS(
            tss64Table,
            (u64)(Init_stack + 32768), (u64)(Init_stack + 32768), (u64)(Init_stack + 32768), 0xffff800000007c00, 0xffff800000007c00,
            0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00);
	Intr_Gate_loadTR(10);
    Intr_Trap_setSysVec();

	memset(SMP_cpuInfo, 0, sizeof(SMP_cpuInfo));
    Task_cfsStruct.flags = 0;
 
    Log_init();

    MM_init();

    Log_enableBuf();

    Intr_init();
    HW_init();

	SMP_init();
    
    HW_Timer_init();

    IO_sti();
    IO_hlt();

    FS_init();

	Task_Syscall_init();
	Task_initMgr();
	SIMD_init();
	Task_buildInitTask(SMP_getCurCPUIndex());
	SIMD_enable();

	SMP_initAPU();
	
    Task_init();
	// setting this flag means launching the task scheduling.
    Task_cfsStruct.flags = 1;
    while (1) IO_hlt();
}