/*
 * xrt_xxhash_tcc_compat.h
 *
 * TCC 兼容垫片：xxHash 在判定为 MSVC x64 目标时会调用 `_umul128`
 * intrinsic 计算 64->128 位乘法，但 TCC 不提供该 intrinsic 的实现，
 * 链接阶段会报 "unresolved reference to '_umul128'"。
 *
 * 这里提供一个可移植的纯 C 实现（Grade-school 乘法），在包含
 * xxhash.h 之前定义 `_umul128`，使 xxHash 的该分支可以正常链接。
 * 仅在缺失时定义，避免与真实 intrinsic 冲突。
 */
#ifndef XRT_XXHASH_TCC_COMPAT_H
#define XRT_XXHASH_TCC_COMPAT_H

typedef unsigned long long xrt_xxhash_u64;

#ifndef _umul128
static inline xrt_xxhash_u64 _umul128(xrt_xxhash_u64 lhs, xrt_xxhash_u64 rhs,
    xrt_xxhash_u64* high)
{
    xrt_xxhash_u64 const lo_lo = (lhs & 0xFFFFFFFFULL) * (rhs & 0xFFFFFFFFULL);
    xrt_xxhash_u64 const hi_lo = (lhs >> 32) * (rhs & 0xFFFFFFFFULL);
    xrt_xxhash_u64 const lo_hi = (lhs & 0xFFFFFFFFULL) * (rhs >> 32);
    xrt_xxhash_u64 const hi_hi = (lhs >> 32) * (rhs >> 32);
    xrt_xxhash_u64 const cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFFULL) + lo_hi;
    xrt_xxhash_u64 const upper = (hi_lo >> 32) + (cross >> 32) + hi_hi;
    xrt_xxhash_u64 const low = lo_lo + (cross << 32);
    if ( high != 0 ) {
        *high = upper;
    }
    return low;
}
#endif

#endif /* XRT_XXHASH_TCC_COMPAT_H */
