#include "ds.h"
#include "../includes/log.h"

u32 Bit_ffs(u64 val) {
	u32 n = 1;
	__asm__ volatile (
		"bsfq %1, %%rax				\n\t"
		"movl $0xffffffff, %%edx	\n\t"
		"cmove %%edx, %%eax			\n\t"
		"addl $1, %%eax				\n\t"
		"movl %%eax, %0				\n\t"
		: "=m"(n)
		: "m"(val)
		: "memory", "rax", "rdx"
	);
	return n;
}