#ifndef __INTERRUPT_GATE_H__
#define __INTERRUPT_GATE_H__

#include "../includes/lib.h"

typedef struct PtReg {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbx, rcx, rdx, rsi, rdi, rbp;
    u64 ds, es;
    u64 rax;
    u64 func, errCode;
    u64 rip, cs, rflags, rsp, ss;
} PtReg;

typedef struct TSS {
    u32 reserved0;
    u64 rsp0, rsp1, rsp2;
    u64 reserved1;
    u64 ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomapBaseAddr;
} __attribute__((packed)) TSS;

typedef struct { u8 dt[8]; } GDTItem;
extern GDTItem gdtTable[];
typedef struct { u8 dt[16]; } IDTItem;
extern IDTItem idtTable[];
// the TSS table for BSP
extern u32 tss64Table[26];

void Intr_Gate_loadTR(u16 n);

void Intr_Gate_setIntr(u64 idtIndex, u8 istIndex, void *codeAddr);
void Intr_Gate_setSMPIntr(int cpuId, u64 idtIndex, u8 istIndex, void *codeAddr);
void Intr_Gate_setTrap(u64 idtIndex, u8 istIndex, void *codeAddr);
void Intr_Gate_setSystem(u64 idtIndex, u8 istIndex, void *codeAddr);
void Intr_Gate_setSysIntr(u64 idtIndex, u8 istIndex, void *codeAddr);

void Intr_Gate_setTSS(
        u32 *tss64Table,
        u64 rsp0, u64 rsp1, u64 rsp2, u64 ist1, u64 ist2,
        u64 ist3, u64 ist4, u64 ist5, u64 ist6, u64 ist7);

void Intr_Gate_setTSSstruct(u32 *tss64Table, TSS *tssStruct);

void Intr_Gate_setTSSDesc(u64 idx, u32 *tssAddr);
#endif