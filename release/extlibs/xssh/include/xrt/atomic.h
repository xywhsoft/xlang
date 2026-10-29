#ifndef XRT_ATOMIC_H
#define XRT_ATOMIC_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_ATOMIC)

/* 原子操作的内存顺序与 C11 语义一致。 */
typedef enum xmemoryorder {
	XMEMORY_RELAXED = 0,
	XMEMORY_ACQUIRE = 1,
	XMEMORY_RELEASE = 2,
	XMEMORY_ACQ_REL = 3,
	XMEMORY_SEQ_CST = 4
} xmemoryorder;



#if defined(_MSC_VER) || \
	(defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64)))
	#define XRT_ATOMIC_ALIGN(iAlignment) __declspec(align(iAlignment))
#elif defined(__GNUC__) || defined(__clang__)
	#define XRT_ATOMIC_ALIGN(iAlignment) __attribute__((aligned(iAlignment)))
#else
	#define XRT_ATOMIC_ALIGN(iAlignment)
#endif

#if UINTPTR_MAX == UINT64_MAX
	#define XRT_ATOMIC_PTR_ALIGNMENT 8
#else
	#define XRT_ATOMIC_PTR_ALIGNMENT 4
#endif



/* 32 位原子整数只允许通过 Atomic API 并发访问 Value。 */
typedef struct XRT_ATOMIC_ALIGN(4) xatomic32 {
	volatile uint32 Value;
} xatomic32;



/* 64 位原子整数显式保证 8 字节对齐。 */
typedef struct XRT_ATOMIC_ALIGN(8) xatomic64 {
	volatile uint64 Value;
} xatomic64;



/* 原子指针只保存指针值，不拥有指针目标。 */
typedef struct XRT_ATOMIC_ALIGN(XRT_ATOMIC_PTR_ALIGNMENT) xatomicptr {
	ptr volatile Value;
} xatomicptr;



#undef XRT_ATOMIC_ALIGN
#undef XRT_ATOMIC_PTR_ALIGNMENT



/* 静态原子对象初始化器只能用于对象定义。 */
#define XRT_ATOMIC32_INIT(iValue) { (uint32)(iValue) }
#define XRT_ATOMIC64_INIT(iValue) { (uint64)(iValue) }
#define XRT_ATOMICPTR_INIT(pValue) { (ptr)(pValue) }



XRT_EXTERN_C_BEGIN



/* 判断自然对齐的指定宽度是否由当前目标无锁实现。 */
XRT_API bool xrtAtomicIsLockFree(size_t iSize);



/* 在对象发布给其他线程前初始化 32 位原子值。 */
XRT_API void xrtAtomic32Init(xatomic32* pAtomic, uint32 iValue);



/* 按指定内存顺序读取 32 位原子值。 */
XRT_API uint32 xrtAtomic32Load(const xatomic32* pAtomic, xmemoryorder iOrder);



/* 按指定内存顺序写入 32 位原子值。 */
XRT_API void xrtAtomic32Store(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);



/* 原子交换 32 位值并返回旧值。 */
XRT_API uint32 xrtAtomic32Exchange(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);



/* 强比较交换 32 位值，失败时把实际值写回 Expected。 */
XRT_API bool xrtAtomic32CompareExchange(
	xatomic32* pAtomic,
	uint32* pExpected,
	uint32 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);



/* 原子加 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchAdd(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);



/* 原子减 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchSub(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);



/* 原子按位与 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchAnd(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);



/* 原子按位或 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchOr(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);



/* 原子按位异或 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchXor(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);



/* 在对象发布给其他线程前初始化 64 位原子值。 */
XRT_API void xrtAtomic64Init(xatomic64* pAtomic, uint64 iValue);



/* 按指定内存顺序读取 64 位原子值。 */
XRT_API uint64 xrtAtomic64Load(const xatomic64* pAtomic, xmemoryorder iOrder);



/* 按指定内存顺序写入 64 位原子值。 */
XRT_API void xrtAtomic64Store(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);



/* 原子交换 64 位值并返回旧值。 */
XRT_API uint64 xrtAtomic64Exchange(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);



/* 强比较交换 64 位值，失败时把实际值写回 Expected。 */
XRT_API bool xrtAtomic64CompareExchange(
	xatomic64* pAtomic,
	uint64* pExpected,
	uint64 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);



/* 原子加 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchAdd(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);



/* 原子减 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchSub(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);



/* 原子按位与 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchAnd(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);



/* 原子按位或 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchOr(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);



/* 原子按位异或 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchXor(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);



/* 在对象发布给其他线程前初始化原子指针。 */
XRT_API void xrtAtomicPtrInit(xatomicptr* pAtomic, ptr pValue);



/* 按指定内存顺序读取原子指针。 */
XRT_API ptr xrtAtomicPtrLoad(const xatomicptr* pAtomic, xmemoryorder iOrder);



/* 按指定内存顺序写入原子指针。 */
XRT_API void xrtAtomicPtrStore(xatomicptr* pAtomic, ptr pValue, xmemoryorder iOrder);



/* 原子交换指针并返回旧值。 */
XRT_API ptr xrtAtomicPtrExchange(xatomicptr* pAtomic, ptr pValue, xmemoryorder iOrder);



/* 强比较交换指针，失败时把实际值写回 Expected。 */
XRT_API bool xrtAtomicPtrCompareExchange(
	xatomicptr* pAtomic,
	ptr* pExpected,
	ptr pDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);



/* 建立线程间内存栅栏。 */
XRT_API void xrtAtomicThreadFence(xmemoryorder iOrder);



/* 建立当前线程与信号处理器之间的编译器栅栏。 */
XRT_API void xrtAtomicSignalFence(xmemoryorder iOrder);



/* 向处理器提示当前线程处于短自旋等待。 */
XRT_API void xrtAtomicPause(void);



XRT_EXTERN_C_END

#endif

#endif
