#ifndef XRT_SYNC_H
#define XRT_SYNC_H

#include <xrt/core.h>
#include <xrt/wait.h>



#if \
	(defined(XRT_FEATURE_MUTEX) || defined(XRT_FEATURE_COND) || \
	 defined(XRT_FEATURE_SEM) || defined(XRT_FEATURE_RWLOCK) || \
	 defined(XRT_FEATURE_EVENT)) && \
	!defined(XRT_FEATURE_SYNC)
	#error "XRT synchronization features require XRT_FEATURE_SYNC"
#endif

#if defined(XRT_FEATURE_COND) && !defined(XRT_FEATURE_MUTEX)
	#error "XRT_FEATURE_COND requires XRT_FEATURE_MUTEX"
#endif

#if \
	(defined(XRT_FEATURE_COND) || defined(XRT_FEATURE_SEM) || \
	 defined(XRT_FEATURE_EVENT)) && \
	!defined(XRT_FEATURE_WAIT)
	#error "timed synchronization features require XRT_FEATURE_WAIT"
#endif



#if defined(_WIN32) || defined(_WIN64)
	#define XRT_MUTEX_STORAGE_SIZE 24u
	#define XRT_COND_STORAGE_SIZE 16u
	#define XRT_SEM_STORAGE_SIZE 24u
	#define XRT_RWLOCK_STORAGE_SIZE 96u
	#define XRT_EVENT_STORAGE_SIZE 24u
#else
	#define XRT_MUTEX_STORAGE_SIZE 96u
	#define XRT_COND_STORAGE_SIZE 80u
	#define XRT_SEM_STORAGE_SIZE 176u
	#define XRT_RWLOCK_STORAGE_SIZE 256u
	#define XRT_EVENT_STORAGE_SIZE 176u
#endif



#if defined(XRT_FEATURE_MUTEX)
/* Mutex 使用固定对齐存储，允许嵌入用户结构且不暴露平台头。 */
typedef union xmutex {
	uint64 Alignment;
	uint8 Storage[XRT_MUTEX_STORAGE_SIZE];
} xmutex;
#endif



#if defined(XRT_FEATURE_COND)
/* 条件变量必须和 XRT mutex 配合使用。 */
typedef union xcond {
	uint64 Alignment;
	uint8 Storage[XRT_COND_STORAGE_SIZE];
} xcond;
#endif



#if defined(XRT_FEATURE_SEM)
/* 信号量的计数范围在所有平台统一为 [0, INT32_MAX]。 */
typedef union xsem {
	uint64 Alignment;
	uint8 Storage[XRT_SEM_STORAGE_SIZE];
} xsem;
#endif



#if defined(XRT_FEATURE_RWLOCK)
/* 读写锁采用写者优先策略并支持升级和降级。 */
typedef union xrwlock {
	uint64 Alignment;
	uint8 Storage[XRT_RWLOCK_STORAGE_SIZE];
} xrwlock;
#endif



#if defined(XRT_FEATURE_EVENT)
/* 事件保存显式信号状态，可选择自动或手动复位。 */
typedef union xevent {
	uint64 Alignment;
	uint8 Storage[XRT_EVENT_STORAGE_SIZE];
} xevent;
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_MUTEX)
/* 初始化调用方存储中的非递归互斥锁。 */
XRT_API bool xrtMutexInit(xmutex* pMutex);



/* 释放互斥锁平台资源；仍被持有时失败且保持对象有效。 */
XRT_API bool xrtMutexUnit(xmutex* pMutex);



/* 创建一个非递归互斥锁。 */
XRT_API xmutex* xrtMutexCreate(void);



/* 释放 Create 返回的互斥锁；仍被持有时失败且不释放对象。 */
XRT_API bool xrtMutexDestroy(xmutex* pMutex);



/* 阻塞到获得互斥锁；同线程递归加锁返回错误。 */
XRT_API bool xrtMutexLock(xmutex* pMutex);



/* 尝试获得互斥锁；锁正忙时返回 false 且不设置错误。 */
XRT_API bool xrtMutexTryLock(xmutex* pMutex);



/* 释放当前线程持有的互斥锁。 */
XRT_API bool xrtMutexUnlock(xmutex* pMutex);
#endif



#if defined(XRT_FEATURE_COND)
/* 初始化调用方存储中的条件变量。 */
XRT_API bool xrtCondInit(xcond* pCond);



/* 释放条件变量平台资源。 */
XRT_API bool xrtCondUnit(xcond* pCond);



/* 创建条件变量。 */
XRT_API xcond* xrtCondCreate(void);



/* 释放 Create 返回的条件变量。 */
XRT_API bool xrtCondDestroy(xcond* pCond);



/* 当前线程持有 mutex 时原子释放并等待通知，返回前重新持有 mutex。 */
XRT_API xwaitresult xrtCondWait(xcond* pCond, xmutex* pMutex);



/* 在相对微秒数内等待通知。 */
XRT_API xwaitresult xrtCondWaitFor(xcond* pCond, xmutex* pMutex, uint64 iTimeout);



/* 等待通知到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtCondWaitUntil(
	xcond* pCond,
	xmutex* pMutex,
	xdeadline iDeadline
);



/* 唤醒一个等待者；通知本身不保存状态。 */
XRT_API bool xrtCondSignal(xcond* pCond);



/* 唤醒全部当前等待者；通知本身不保存状态。 */
XRT_API bool xrtCondBroadcast(xcond* pCond);
#endif



#if defined(XRT_FEATURE_SEM)
/* 初始化计数信号量。 */
XRT_API bool xrtSemInit(xsem* pSem, uint32 iInitial, uint32 iMaximum);



/* 释放信号量平台资源。 */
XRT_API bool xrtSemUnit(xsem* pSem);



/* 创建计数信号量。 */
XRT_API xsem* xrtSemCreate(uint32 iInitial, uint32 iMaximum);



/* 释放 Create 返回的信号量。 */
XRT_API bool xrtSemDestroy(xsem* pSem);



/* 等待并消费一个信号。 */
XRT_API xwaitresult xrtSemWait(xsem* pSem);



/* 非阻塞地尝试消费一个信号。 */
XRT_API xwaitresult xrtSemTryWait(xsem* pSem);



/* 在相对微秒数内等待并消费一个信号。 */
XRT_API xwaitresult xrtSemWaitFor(xsem* pSem, uint64 iTimeout);



/* 等待并消费一个信号到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtSemWaitUntil(xsem* pSem, xdeadline iDeadline);



/* 发布一个信号；达到上限时失败且计数不变。 */
XRT_API bool xrtSemPost(xsem* pSem);



/* 原子发布多个信号；超过上限时失败且不部分发布。 */
XRT_API bool xrtSemPostMany(xsem* pSem, uint32 iCount);
#endif



#if defined(XRT_FEATURE_RWLOCK)
/* 初始化写者优先的读写锁。 */
XRT_API bool xrtRWLockInit(xrwlock* pLock);



/* 释放读写锁平台资源；仍被持有或等待时失败。 */
XRT_API bool xrtRWLockUnit(xrwlock* pLock);



/* 创建写者优先的读写锁。 */
XRT_API xrwlock* xrtRWLockCreate(void);



/* 释放 Create 返回的读写锁。 */
XRT_API bool xrtRWLockDestroy(xrwlock* pLock);



/* 获得非递归共享读锁；读锁所有权由调用方保证。 */
XRT_API bool xrtRWLockRead(xrwlock* pLock);



/* 尝试获得共享读锁；写者存在或等待时返回 false。 */
XRT_API bool xrtRWLockTryRead(xrwlock* pLock);



/* 释放当前线程持有的一个读锁。 */
XRT_API bool xrtRWLockReadUnlock(xrwlock* pLock);



/* 获得独占写锁。 */
XRT_API bool xrtRWLockWrite(xrwlock* pLock);



/* 尝试获得独占写锁。 */
XRT_API bool xrtRWLockTryWrite(xrwlock* pLock);



/* 释放当前线程持有的写锁。 */
XRT_API bool xrtRWLockWriteUnlock(xrwlock* pLock);



/* 原子地把当前线程的写锁降级为一个读锁。 */
XRT_API bool xrtRWLockDowngrade(xrwlock* pLock);



/* 当前线程只持有一个读锁时，释放它并排队获得写锁。 */
XRT_API bool xrtRWLockUpgrade(xrwlock* pLock);
#endif



#if defined(XRT_FEATURE_EVENT)
/* 初始化自动或手动复位事件。 */
XRT_API bool xrtEventInit(xevent* pEvent, bool bManualReset, bool bSignaled);



/* 释放事件平台资源。 */
XRT_API bool xrtEventUnit(xevent* pEvent);



/* 创建自动或手动复位事件。 */
XRT_API xevent* xrtEventCreate(bool bManualReset, bool bSignaled);



/* 释放 Create 返回的事件。 */
XRT_API bool xrtEventDestroy(xevent* pEvent);



/* 等待事件进入信号态。 */
XRT_API xwaitresult xrtEventWait(xevent* pEvent);



/* 非阻塞地检查并消费自动复位事件。 */
XRT_API xwaitresult xrtEventTryWait(xevent* pEvent);



/* 在相对微秒数内等待事件。 */
XRT_API xwaitresult xrtEventWaitFor(xevent* pEvent, uint64 iTimeout);



/* 等待事件到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtEventWaitUntil(xevent* pEvent, xdeadline iDeadline);



/* 设置事件；手动复位唤醒全部等待者，自动复位唤醒一个等待者。 */
XRT_API bool xrtEventSet(xevent* pEvent);



/* 清除事件信号态。 */
XRT_API bool xrtEventReset(xevent* pEvent);
#endif



XRT_EXTERN_C_END

#endif
