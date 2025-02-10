#ifndef __HAL_TASK_TASKSTRUCT_H__
#define __HAL_TASK_TASKSTRUCT_H__

#include "../../../includes/lib.h"

typedef struct hal_RegStruct {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbx, rcx, rdx, rsi, rdi, rbp;
    u64 ds, es;
    u64 rax;
    u64 func, errCode;
    u64 rip, cs, rflags, rsp, ss;
};

typedef struct hal_ThreadStruct {
    u64 rip, rsp;
    u64 cs, ds, fs, gs;
} hal_ThreadStruct;

typedef struct hal_TaskMemStruct {
    u64 cr3;
    u64 krlTblSyncJiff;
} hal_TaskMemStruct;
#endif