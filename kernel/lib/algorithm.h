#ifndef __LIB_ALGORITHM_H__
#define __LIB_ALGORITHM_H__
#define upAlignTo(x, bs) ({__typeof__(x) _x = (x); __typeof__(bs) _bs = bs; \
    (((_x) + (_bs) - 1) / (_bs) * (_bs)); })
#define downAlignTo(x, bs) ({__typeof__(x) _x = (x); __typeof__(bs) _bs = bs; \
    ((_x) / (_bs) * (_bs)); })

#define max(a, b) ({__typeof__(a) ta = (a); __typeof__(b) tb = (b); (ta) > (tb) ? (ta) : (tb); })
#define min(a, b) ({__typeof__(a) ta = (a); __typeof__(b) tb = (b); (ta) > (tb) ? (tb) : (ta); })
#define abs(a) ({__typeof__(a) ta = (a); ta < 0 ? -ta : ta; })

#include "ds.h"

// get floor(log2(n))
i32 log2(u64 n);
// get ceil(log2(n))
i32 log2Ceil(u64 n);

// get the lowest bit of x
#define lowbit(x) ({__typeof__(x) tx = (x); tx & -tx; })

#define BKDRHash(x, ch) ((x) * 31 + (ch))

#define CRC32Init() (~0ul)

#define CRC32Embed(crc32, dt) ({ \
    __typedef__(crc32) t; \
    __asm__ volatile ( \
        "crc32 %1, %0   \n\t" \
        : "=r"(t) \
        : "r"(dt), "0"(crc32) \
    ); \
    t; \
})

#endif