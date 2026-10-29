#ifndef XRT_INTERNAL_H
#define XRT_INTERNAL_H

#if !defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
	#include <intrin.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#if defined(XRT_FEATURE_NET)
			#include <winapi/winsock2.h>
			#include <winapi/ws2tcpip.h>
			#if defined(XRT_FEATURE_NET_SOCKET)
				#include <winapi/mswsock.h>
				#ifndef WSAID_WSASENDMSG
					typedef INT (WINAPI *LPFN_WSASENDMSG)(
						SOCKET Socket,
						LPWSAMSG pMessage,
						DWORD iFlags,
						LPDWORD pSent,
						LPWSAOVERLAPPED pOverlapped,
						LPWSAOVERLAPPED_COMPLETION_ROUTINE pCompletion
					);
					#define WSAID_WSASENDMSG \
						{0xa441e712, 0x754f, 0x43ca, \
						 {0x84, 0xa7, 0x0d, 0xee, 0x44, 0xcf, 0x60, 0x6d}}
				#endif
			#endif
		#endif
		#include <winapi/windows.h>
		/* TinyCC 自带 Win32 头缺少 Vista 之后的轻量同步声明。 */
		#ifndef XRT_WIN_SYNC_TYPES
			#define XRT_WIN_SYNC_TYPES
			typedef struct { PVOID Ptr; } SRWLOCK, *PSRWLOCK;
			typedef struct { PVOID Ptr; } CONDITION_VARIABLE, *PCONDITION_VARIABLE;
			#ifndef SRWLOCK_INIT
				#define SRWLOCK_INIT { 0 }
			#endif
			void WINAPI InitializeConditionVariable(PCONDITION_VARIABLE pCondition);
			void WINAPI WakeConditionVariable(PCONDITION_VARIABLE pCondition);
			void WINAPI WakeAllConditionVariable(PCONDITION_VARIABLE pCondition);
			BOOL WINAPI SleepConditionVariableCS(
				PCONDITION_VARIABLE pCondition,
				PCRITICAL_SECTION pCriticalSection,
				DWORD iMilliseconds
			);
			BOOL WINAPI SleepConditionVariableSRW(
				PCONDITION_VARIABLE pCondition,
				PSRWLOCK pLock,
				DWORD iMilliseconds,
				ULONG iFlags
			);
			void WINAPI InitializeSRWLock(PSRWLOCK pLock);
			void WINAPI AcquireSRWLockExclusive(PSRWLOCK pLock);
			void WINAPI ReleaseSRWLockExclusive(PSRWLOCK pLock);
			BOOLEAN WINAPI TryAcquireSRWLockExclusive(PSRWLOCK pLock);
		#endif
		#ifndef FLS_OUT_OF_INDEXES
			#define FLS_OUT_OF_INDEXES ((DWORD)0xffffffffu)
			typedef VOID (WINAPI *XRT_FLS_CALLBACK)(PVOID pValue);
			DWORD WINAPI FlsAlloc(XRT_FLS_CALLBACK pCallback);
			BOOL WINAPI FlsFree(DWORD iIndex);
			PVOID WINAPI FlsGetValue(DWORD iIndex);
			BOOL WINAPI FlsSetValue(DWORD iIndex, PVOID pValue);
		#endif
		#if defined(XRT_FEATURE_NET_PORT_IOCP)
			/* TinyCC Win32 头缺少 Vista 引入的精确 Overlapped 取消入口。 */
			BOOL WINAPI CancelIoEx(HANDLE hFile, LPOVERLAPPED pOverlapped);
		#endif
	#else
		#if defined(XRT_FEATURE_NET)
			#include <winsock2.h>
			#include <ws2tcpip.h>
			#if defined(XRT_FEATURE_NET_SOCKET)
				#include <mswsock.h>
			#endif
		#endif
		#include <windows.h>
	#endif
#else
	#include <pthread.h>
	#include <sched.h>
	#if defined(__linux__) && !defined(__cplusplus)
		#include <time.h>
		/* 严格 C 模式可能隐藏这些稳定 Linux/POSIX ABI 声明。 */
		#ifndef CLOCK_REALTIME
			#define CLOCK_REALTIME 0
		#endif
		#ifndef CLOCK_MONOTONIC
			#define CLOCK_MONOTONIC 1
		#endif
		extern int clock_gettime(int iClock, struct timespec* pTime);
		extern int nanosleep(
			const struct timespec* pRequest,
			struct timespec* pRemaining
		);
		extern int pthread_condattr_setclock(
			pthread_condattr_t* pAttributes,
			int iClock
		);
	#endif
#endif

#include <xrt.h>



/* 编译器线程局部存储用于非 TinyCC 构建。 */
#if defined(_MSC_VER)
	#define XRT_THREAD_LOCAL __declspec(thread)
#elif !defined(__TINYC__)
	#define XRT_THREAD_LOCAL __thread
#endif



#if !defined(_WIN32) && !defined(_WIN64)
/* 把 POSIX 线程句柄折叠为进程内稳定的非零标识。 */
static inline uint64 __xrtNativeThreadId(pthread_t tThread)
{
	uint64 iId = 0;
	size_t iSize = sizeof(tThread) < sizeof(iId)
		? sizeof(tThread)
		: sizeof(iId);

	memcpy(&iId, &tThread, iSize);
	return iId != 0 ? iId : UINT64_C(1);
}
#endif



/* 返回当前平台线程在进程内稳定的非零标识。 */
static inline uint64 __xrtCurrentThreadId(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetCurrentThreadId();
	#else
		return __xrtNativeThreadId(pthread_self());
	#endif
}



/* 内部紧凑布局使用目标 ABI 的真实类型对齐，不假设 64 位标量在 32 位平台只需四字节对齐。 */
#if defined(_MSC_VER)
	#define XRT_INTERNAL_ALIGNOF(Type) __alignof(Type)
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
	#define XRT_INTERNAL_ALIGNOF(Type) __alignof__(Type)
#elif defined(__cplusplus)
	#define XRT_INTERNAL_ALIGNOF(Type) alignof(Type)
#else
	#define XRT_INTERNAL_ALIGNOF(Type) _Alignof(Type)
#endif



/*
	TCC i386 会为含 64 位成员的聚合报告八字节对齐，但嵌套栈帧实际只保证
	指针对齐。公开对象必须接受编译器自身生成的自动存储。
*/
#if defined(__TINYC__) && defined(__i386__)
	#define XRT_INTERNAL_OBJECT_ALIGNOF(Type) \
		((XRT_INTERNAL_ALIGNOF(Type) > sizeof(void*)) ? \
		 sizeof(void*) : XRT_INTERNAL_ALIGNOF(Type))
#else
	#define XRT_INTERNAL_OBJECT_ALIGNOF(Type) XRT_INTERNAL_ALIGNOF(Type)
#endif



/* 内部二进制协议统一按目标字节序读取未对齐的小端整数。 */
#if defined(_WIN32) || defined(_WIN64) || defined(__LITTLE_ENDIAN__) || \
	defined(__i386__) || defined(__x86_64__) || \
	(defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
	#define XRT_INTERNAL_LITTLE_ENDIAN 1
#elif defined(__BIG_ENDIAN__) || \
	(defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
	#define XRT_INTERNAL_LITTLE_ENDIAN 0
#else
	#error "XRT cannot determine target byte order"
#endif



/* 未对齐读取 16 位小端整数，不违反严格别名规则。 */
static inline uint16 __xrtReadLe16(const void* pData)
{
	uint16 iValue;

	memcpy(&iValue, pData, sizeof(iValue));
#if XRT_INTERNAL_LITTLE_ENDIAN
	return iValue;
#elif defined(__GNUC__) || defined(__clang__)
	return __builtin_bswap16(iValue);
#elif defined(_MSC_VER)
	return _byteswap_ushort(iValue);
#else
	return (uint16)((iValue << 8) | (iValue >> 8));
#endif
}



/* 未对齐读取 32 位小端整数，不违反严格别名规则。 */
static inline uint32 __xrtReadLe32(const void* pData)
{
	uint32 iValue;

	memcpy(&iValue, pData, sizeof(iValue));
#if XRT_INTERNAL_LITTLE_ENDIAN
	return iValue;
#elif defined(__GNUC__) || defined(__clang__)
	return __builtin_bswap32(iValue);
#elif defined(_MSC_VER)
	return _byteswap_ulong(iValue);
#else
	return ((iValue & UINT32_C(0x000000FF)) << 24) |
		((iValue & UINT32_C(0x0000FF00)) << 8) |
		((iValue & UINT32_C(0x00FF0000)) >> 8) |
		((iValue & UINT32_C(0xFF000000)) >> 24);
#endif
}



/* 未对齐读取 64 位小端整数，不违反严格别名规则。 */
static inline uint64 __xrtReadLe64(const void* pData)
{
	uint64 iValue;

	memcpy(&iValue, pData, sizeof(iValue));
#if XRT_INTERNAL_LITTLE_ENDIAN
	return iValue;
#elif defined(__GNUC__) || defined(__clang__)
	return __builtin_bswap64(iValue);
#elif defined(_MSC_VER)
	return _byteswap_uint64(iValue);
#else
	return ((iValue & UINT64_C(0x00000000000000FF)) << 56) |
		((iValue & UINT64_C(0x000000000000FF00)) << 40) |
		((iValue & UINT64_C(0x0000000000FF0000)) << 24) |
		((iValue & UINT64_C(0x00000000FF000000)) << 8) |
		((iValue & UINT64_C(0x000000FF00000000)) >> 8) |
		((iValue & UINT64_C(0x0000FF0000000000)) >> 24) |
		((iValue & UINT64_C(0x00FF000000000000)) >> 40) |
		((iValue & UINT64_C(0xFF00000000000000)) >> 56);
#endif
}



/* 内部实现与扩展库共用同一份公共范围检查语义。 */
#define __xrtRangesOverlap xrtMemRangesOverlap
#define __xrtRangeValid xrtMemRangeValid



/* 错误执行上下文由协程和任务调度器切换。 */
typedef struct xrt_error_context {
	xerror* Error;
} xrt_error_context;



/* 内部短临界区锁在 TinyCC POSIX 上使用 pthread mutex。 */
#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
typedef struct xrt_spinlock {
	pthread_mutex_t Mutex;
} xrt_spinlock;
#else
typedef struct xrt_spinlock {
	volatile int32 Value;
} xrt_spinlock;
#endif



/* 初始化内部短临界区锁。 */
static inline void __xrtSpinInit(xrt_spinlock* pLock)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_mutex_init(&pLock->Mutex, NULL);
	#else
		pLock->Value = 0;
	#endif
}



/* 释放内部短临界区锁占用的平台资源。 */
static inline void __xrtSpinUnit(xrt_spinlock* pLock)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_mutex_destroy(&pLock->Mutex);
	#else
		(void)pLock;
	#endif
}



/* 进入内部短临界区。 */
static inline void __xrtSpinLock(xrt_spinlock* pLock)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_mutex_lock(&pLock->Mutex);
	#else
		uint32 iSpin = 0;

		#if defined(_MSC_VER)
			while ( _InterlockedCompareExchange((volatile long*)&pLock->Value, 1, 0) != 0 ) {
		#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
			while ( InterlockedCompareExchange((volatile LONG*)&pLock->Value, 1, 0) != 0 ) {
		#else
			while ( __sync_val_compare_and_swap(&pLock->Value, 0, 1) != 0 ) {
		#endif
			iSpin++;
			if ( (iSpin & 0x3FFu) == 0 ) {
				#if defined(_WIN32) || defined(_WIN64)
					(void)SwitchToThread();
				#else
					(void)sched_yield();
				#endif
			}
		}
	#endif
}



/* 离开内部短临界区。 */
static inline void __xrtSpinUnlock(xrt_spinlock* pLock)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_mutex_unlock(&pLock->Mutex);
	#elif defined(_MSC_VER)
		(void)_InterlockedExchange((volatile long*)&pLock->Value, 0);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)InterlockedExchange((volatile LONG*)&pLock->Value, 0);
	#else
		__sync_lock_release(&pLock->Value);
	#endif
}



/* 原子读取 32 位引用计数。 */
static inline int32 __xrtAtomicRefLoad(const volatile int32* pValue)
{
	#if defined(_MSC_VER)
		return (int32)_InterlockedCompareExchange((volatile long*)pValue, 0, 0);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		return (int32)InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#elif defined(__TINYC__)
		return *pValue;
	#else
		return (int32)__sync_val_compare_and_swap((volatile int32*)pValue, 0, 0);
	#endif
}



/* 原子比较并交换 32 位引用计数，返回交换前的值。 */
static inline int32 __xrtAtomicRefCompareExchange(volatile int32* pValue, int32 iValue, int32 iExpected)
{
	#if defined(_MSC_VER)
		return (int32)_InterlockedCompareExchange((volatile long*)pValue, (long)iValue, (long)iExpected);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		return (int32)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iValue, (LONG)iExpected);
	#elif defined(__TINYC__)
		int32 iOld = *pValue;

		if ( iOld == iExpected ) {
			*pValue = iValue;
		}
		return iOld;
	#else
		return (int32)__sync_val_compare_and_swap(pValue, iExpected, iValue);
	#endif
}



/* 无分配地设置内存不足错误。 */
void __xrtErrorSetOutOfMemory(void);



/* 无分配地设置参数错误。 */
static inline void __xrtErrorSetInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT, "xlang.extlib.xhttp", 1,
		"invalid argument"
	);
}



/* 无分配地设置类型不匹配错误。 */
void __xrtErrorSetType(void);



/* 无分配地设置非法值错误。 */
static inline void __xrtErrorSetValue(void)
{
	xrtSetErrorInfo(
		XERR_VALUE, "xlang.extlib.xhttp", 2,
		"invalid value"
	);
}



/* 无分配地设置状态错误。 */
void __xrtErrorSetInvalidState(void);



/* 无分配地设置大小溢出错误。 */
static inline void __xrtErrorSetSizeOverflow(void)
{
	xrtSetErrorInfo(
		XERR_RANGE, "xlang.extlib.xhttp", 3,
		"size overflow"
	);
}



/* 无分配地设置索引越界错误。 */
static inline void __xrtErrorSetRange(void)
{
	xrtSetErrorInfo(
		XERR_RANGE, "xlang.extlib.xhttp", 4,
		"range is out of bounds"
	);
}



/* 无分配地设置暂时无法继续错误。 */
void __xrtErrorSetAgain(void);



/* 无分配地设置不支持错误。 */
void __xrtErrorSetUnsupported(void);



/* 无分配地设置资源或键已经存在错误。 */
void __xrtErrorSetExists(void);



/* 无分配地设置操作已取消错误。 */
void __xrtErrorSetCancelled(void);



/* 无分配地设置操作超时错误。 */
void __xrtErrorSetTimeout(void);



/* 无分配地设置资源已经关闭错误。 */
void __xrtErrorSetClosed(void);



/* 无分配地设置内部契约错误。 */
void __xrtErrorSetInternal(void);



/* 将错误所有权转移到当前执行上下文。 */
void __xrtErrorSetOwned(xerror* pError);



/* 静默交换当前错误所有权，不通知处理器也不改变引用计数。 */
xerror* __xrtErrorSwapOwned(xerror* pError);



/* 构建并安装带稳定域、代码和可选原因链的结构化错误。 */
void __xrtErrorSetDetail(
	xerrkind Kind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 构建并安装保留原生错误码的结构化系统错误。 */
void __xrtErrorSetSystem(
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	int iSystemCode,
	cstr sMessage
);



/* 取得当前错误并用指定结构化错误包装，构建失败时恢复原错误。 */
void __xrtErrorWrapDetail(
	xerrkind DefaultKind,
	cstr sDomain,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
);



/* 把当前平台系统错误代码映射为跨模块稳定类别。 */
xerrkind __xrtSystemErrorKind(int iCode);



/* 返回当前系统可用于调度的逻辑处理器数量。 */
uint32 __xrtProcessorCount(void);



/* 为协程或任务切换错误执行上下文。 */
xrt_error_context* __xrtErrorContextSwap(xrt_error_context* pContext);

#endif
