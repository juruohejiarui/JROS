// this file defines some useful data structures and the descriptor of CPU (e.g tss, GDT, IDT...)
#ifndef __LIB_DS_H__
#define __LIB_DS_H__

typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned short u16;
typedef unsigned char u8;
typedef long long i64;
typedef int i32;
typedef short i16;
typedef signed char i8;

typedef struct List {
    struct List *next, *prev;
} List;

#define __always_inline__  inline __attribute__ ((always_inline))
#define __noinline__ __attribute__ ((noinline))

__always_inline__ void List_init(List *list) {
    list->prev = list->next = list;
}

__always_inline__ void List_insBehind(List *ele, List *pos) {
    ele->next = pos->next, ele->prev = pos;
    pos->next->prev = ele;
    pos->next = ele;
}

__always_inline__ void List_insBefore(List *ele, List *pos) {
    ele->next = pos, ele->prev = pos->prev;
    pos->prev->next = ele;
    pos->prev = ele;
}

__always_inline__ int List_isEmpty(List *ele) { return ele->prev == ele && ele->next == ele; }

__always_inline__ void List_del(List *ele) {
    ele->next->prev = ele->prev;
    ele->prev->next = ele->next;
    ele->prev = ele->next = ele;
}

__always_inline__ u64 Bit_get(u64 *addr, u64 index) { return ((*addr) >> index) & 1; }
__always_inline__ void Bit_set1(u64 *addr, u64 index) { 
	__asm__ volatile (
		"btsq %1, %0		\n\t"
		: "+m"(*addr)
		: "r"(index)
		: "memory");
}
__always_inline__ void Bit_set0(u64 *addr, u64 index) {
	__asm__ volatile (
		"btrq %1, %0		\n\t"
		: "+m"(*addr)
		: "r"(index)
		: "memory");
}
__always_inline__ void Bit_rev(u64 *addr, u64 index) {
    __asm__ volatile (
        "btcq %1, %0    \n\t"
        : "+m"(*addr)
        : "r"(index)
        : "memory"
    );
}
// return 1-based index
u32 Bit_ffs(u64 val);

static __always_inline__ u32 endianSwap32(u32 val) { return ((val >> 24) & 0xff) | ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000) | (val << 24); }
static __always_inline__ u64 endianSwap64(u64 val) { return ((u64)endianSwap32(val & 0xfffffffful) << 32) | (endianSwap32(val >> 32)); }

#define memberOffset(type, member) ((u64)(&(((type *)0)->member)))
#define container(memberAddr, type, memberIden) ((type *)(((u64)(memberAddr))-((u64)&(((type *)0)->memberIden))))

#endif