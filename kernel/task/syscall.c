#include "syscall.h"
#include "mgr.h"
#include "desc.h"
#include "../includes/interrupt.h"
#include "../includes/memory.h"
#include "../includes/log.h"
#include "../includes/smp.h"

void Syscall_entry();
u64 Syscall_exit(u64 retVal);

u64 Syscall_noSystemCall(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    printk(WHITE, BLACK, "no such system call, args:(%#018lx,%#018lx,%#018lx,%#018lx,%#018lx)\n",
        arg1, arg2, arg3, arg4, arg5, arg6);
    return 1919810;
}

u64 Syscall_enableIntr(u64 intrId) {
    HW_APIC_enableIntr(intrId);
    return 0;
}

u64 Syscall_abort(u64 intrId) {
    int t = 1000000000;
    while (t--) ;
    return 0;
}

u64 Syscall_mdelay(u64 msec) {
    if (!(IO_getRflags() & (1 << 9))) {
        printk(RED, BLACK, "interrupt is blocked, unable to execute mdelay()\n");
        return -1;
    }
    Intr_SoftIrq_Timer_mdelay(msec);
    return 0;
}

u64 Syscall_exit(u64 retVal) {
	Task_exit(retVal);
}

typedef u64 (*Syscall)(u64, u64, u64, u64, u64, u64);
Syscall Syscall_list[Syscall_num] = { 
    [0] = (Syscall)Syscall_abort,
    [1] = (Syscall)Syscall_printStr,
	[2] = (Syscall)Syscall_mdelay,
	[3] = (Syscall)Syscall_exit,
    [4 ... Syscall_num - 1] = Syscall_noSystemCall };

u64 Syscall_handler(u64 index, PtReg *regs) {
    u64 arg1 = regs->rdi, arg2 = regs->rsi, arg3 = regs->rdx, arg4 = regs->rcx, arg5 = regs->r8, arg6 = regs->r9;
    // switch stack and segment registers
	IO_sti();
    u64 res = (Syscall_list[index])(arg1, arg2, arg3, arg4, arg5, arg6);
    // switch to user level
	IO_cli();
    return res;
}

u64 Task_Syscall_usrAPI(u64 index, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5, u64 arg6) {
    // directly use "syscall"
    u64 res;
    PtReg regs;
    regs.rdi = arg1, regs.rsi = arg2;
    regs.rdx = arg3, regs.rcx = arg4;
    regs.r8 = arg5, regs.r9 = arg6;
    __asm__ volatile (
        "syscall        \n\t"
        "movq %%rax, %0 \n\t"
         : "=m"(res) 
         : "D"(index), "S"((u64)&regs)
         : "memory", "rax");
    return res;
}


void Task_switchToUsr(u64 (*entry)(void *, u64), void *arg1, u64 arg2) {
	IO_cli();
    __asm__ volatile (
		"pushq %%rcx		\n\t"
		"pushq %%rcx		\n\t"
		"pushq %%rdx		\n\t"
		"pushq %%rax		\n\t"
		"pushq %%rbx		\n\t"
		"pushq %%rbx		\n\t"
        "jmp Syscall_backToUsr	\n\t"
        :
        : "a"((1ul << 9)), "b"(Task_userStackEnd), "c"(Segment_userData), "d"((u64)entry), "D"(arg1), "S"(arg2)
        : "memory"
    );
}

void Task_Syscall_init() {
    // set IA32_EFER.SCE
    IO_writeMSR(0xC0000080, IO_readMSR(0xC0000080) | 1);
    // set IA32_STAR
    IO_writeMSR(0xC0000081, ((u64)(0x28 | 3) << 48) | ((u64)0x8 << 32));
    // set IA32_LSTAR
    IO_writeMSR(0xC0000082, (u64)Syscall_entry);
    // set IA32_SFMASK
    IO_writeMSR(0xC0000084, (1 << 9));
}
