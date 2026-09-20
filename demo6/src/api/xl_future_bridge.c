#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* The resident array bridge also consumes xruntime's typed Future carrier. */
#include "release/include/xruntime_decl.h"

#include "xl_future_bridge.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_future_aggregate.h"



/* Future 元数据只服务于 xlang 的语言级观测接口。 */
typedef struct xl6_future_name xl6_future_name;
typedef struct xl6_future_meta xl6_future_meta;

struct xl6_future_name {
	xl6_future_name* Next;
	char Text[1];
};

struct xl6_future_meta {
	xl6_future_meta* Next;
	xfuture* Future;
	xpromise* Promise;
	xl6_future_name* Names;
	const char* Name;
	unsigned long long CreatedMs;
	unsigned long long CompletedMs;
	size_t References;
	int Pending;
};



static xonce __xl6FutureOnce = XRT_ONCE_INIT;
static xmutex __xl6FutureLock;
static xl6_future_meta* __xl6FutureMetas;



/* 初始化私有元数据锁。 */
static bool __xl6FutureInit(void* pData)
{
	(void)pData;
	return xrtMutexInit(&__xl6FutureLock);
}



/* 返回当前单调时钟的毫秒刻度。 */
static unsigned long long __xl6FutureNowMs(void)
{
	return (unsigned long long)(xrtClock() / UINT64_C(1000));
}



/* 把语言层毫秒相对时间转换为 XRT 微秒相对时间。 */
static uint64 __xl6FutureTimeout(long long iMilliseconds)
{
	if ( iMilliseconds <= 0 ) {
		return 0;
	}
	if ( (unsigned long long)iMilliseconds >
		(UINT64_MAX / UINT64_C(1000)) ) {
		return UINT64_MAX;
	}
	return (uint64)iMilliseconds * UINT64_C(1000);
}



/* 把语言层毫秒截止时间转换为 XRT 单调微秒截止时间。 */
static xdeadline __xl6FutureDeadline(long long iMilliseconds)
{
	return (xdeadline)__xl6FutureTimeout(iMilliseconds);
}



/* 在持锁状态下查找元数据。 */
static xl6_future_meta* __xl6FutureMetaFind(xfuture* pFuture)
{
	xl6_future_meta* pMeta = __xl6FutureMetas;

	while ( pMeta != NULL ) {
		if ( pMeta->Future == pFuture ) {
			return pMeta;
		}
		pMeta = pMeta->Next;
	}
	return NULL;
}



/* 确保 Future 具有一条元数据记录，初始引用数由调用路径决定。 */
static xl6_future_meta* __xl6FutureMetaEnsure(
	xfuture* pFuture,
	size_t iReferences
)
{
	xl6_future_meta* pMeta;

	if ( (pFuture == NULL) ||
		!xrtOnce(&__xl6FutureOnce, __xl6FutureInit, NULL) ) {
		return NULL;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	pMeta = __xl6FutureMetaFind(pFuture);
	if ( pMeta == NULL ) {
		pMeta = (xl6_future_meta*)xrtCalloc(1, sizeof(xl6_future_meta));
		if ( pMeta != NULL ) {
			pMeta->Future = pFuture;
			pMeta->CreatedMs = __xl6FutureNowMs();
			pMeta->References = iReferences;
			pMeta->Next = __xl6FutureMetas;
			__xl6FutureMetas = pMeta;
		}
	}
	(void)xrtMutexUnlock(&__xl6FutureLock);
	return pMeta;
}



/* 为桥接层创建的线程任务保存一个可立即提交终态的 Promise 引用。 */
static bool __xl6FutureMetaBindPromise(
	xfuture* pFuture,
	xpromise* pPromise
)
{
	xl6_future_meta* pMeta;
	xpromise* pOwned;
	bool bBound = false;

	if ( (pFuture == NULL) || (pPromise == NULL) ) {
		return false;
	}
	pOwned = xrtPromiseRef(pPromise);
	if ( pOwned == NULL ) {
		return false;
	}
	pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	if ( pMeta != NULL ) {
		(void)xrtMutexLock(&__xl6FutureLock);
		pMeta = __xl6FutureMetaFind(pFuture);
		if ( (pMeta != NULL) && (pMeta->Promise == NULL) ) {
			pMeta->Promise = pOwned;
			pOwned = NULL;
			bBound = true;
		}
		(void)xrtMutexUnlock(&__xl6FutureLock);
	}
	xrtPromiseDestroy(pOwned);
	return bBound;
}



/* 返回元数据中 Promise 的新增引用。 */
static xpromise* __xl6FutureMetaPromise(xfuture* pFuture)
{
	xl6_future_meta* pMeta;
	xpromise* pPromise = NULL;

	if ( !xrtOnce(&__xl6FutureOnce, __xl6FutureInit, NULL) ) {
		return NULL;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	pMeta = __xl6FutureMetaFind(pFuture);
	if ( (pMeta != NULL) && (pMeta->Promise != NULL) ) {
		pPromise = xrtPromiseRef(pMeta->Promise);
	}
	(void)xrtMutexUnlock(&__xl6FutureLock);
	return pPromise;
}



/* 增加元数据对应的语言 Future 引用。 */
static void __xl6FutureMetaRef(xfuture* pFuture)
{
	xl6_future_meta* pMeta;

	if ( !xrtOnce(&__xl6FutureOnce, __xl6FutureInit, NULL) ) {
		return;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	pMeta = __xl6FutureMetaFind(pFuture);
	if ( pMeta != NULL ) {
		pMeta->References++;
	} else {
		pMeta = (xl6_future_meta*)xrtCalloc(1, sizeof(xl6_future_meta));
		if ( pMeta != NULL ) {
			pMeta->Future = pFuture;
			pMeta->CreatedMs = __xl6FutureNowMs();
			pMeta->References = 2;
			pMeta->Next = __xl6FutureMetas;
			__xl6FutureMetas = pMeta;
		}
	}
	(void)xrtMutexUnlock(&__xl6FutureLock);
}



/* 释放元数据持有的名称历史。 */
static void __xl6FutureNamesFree(xl6_future_name* pName)
{
	while ( pName != NULL ) {
		xl6_future_name* pNext = pName->Next;

		xrtFree(pName);
		pName = pNext;
	}
}



/* 释放一个语言 Future 引用对应的元数据计数。 */
static void __xl6FutureMetaDestroy(xfuture* pFuture)
{
	xl6_future_meta* pPrevious = NULL;
	xl6_future_meta* pMeta;
	xl6_future_name* pNames = NULL;
	xpromise* pPromise = NULL;

	if ( !xrtOnce(&__xl6FutureOnce, __xl6FutureInit, NULL) ) {
		return;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	pMeta = __xl6FutureMetas;
	while ( pMeta != NULL ) {
		if ( pMeta->Future == pFuture ) {
			if ( pMeta->References > 0 ) {
				pMeta->References--;
			}
			if ( pMeta->References == 0 ) {
				if ( pPrevious != NULL ) {
					pPrevious->Next = pMeta->Next;
				} else {
					__xl6FutureMetas = pMeta->Next;
				}
				pNames = pMeta->Names;
				pPromise = pMeta->Promise;
			} else {
				pMeta = NULL;
			}
			break;
		}
		pPrevious = pMeta;
		pMeta = pMeta->Next;
	}
	(void)xrtMutexUnlock(&__xl6FutureLock);
	if ( pMeta != NULL ) {
		__xl6FutureNamesFree(pNames);
		xrtPromiseDestroy(pPromise);
		xrtFree(pMeta);
	}
}



/* Future 进入终态后记录首次观测到的完成时刻。 */
static void __xl6FutureMetaObserve(xfuture* pFuture)
{
	xl6_future_meta* pMeta;

	if ( (pFuture == NULL) || !xrtFutureDone(pFuture) ) {
		return;
	}
	pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	if ( pMeta == NULL ) {
		return;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	pMeta = __xl6FutureMetaFind(pFuture);
	if ( (pMeta != NULL) && (pMeta->CompletedMs == 0) ) {
		pMeta->CompletedMs = __xl6FutureNowMs();
		if ( pMeta->CompletedMs < pMeta->CreatedMs ) {
			pMeta->CompletedMs = pMeta->CreatedMs;
		}
	}
	(void)xrtMutexUnlock(&__xl6FutureLock);
}



/* 调整语言适配层尚未触发的延续数量。 */
static void __xl6FutureMetaPending(xfuture* pFuture, int iDelta)
{
	xl6_future_meta* pMeta;

	pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	if ( pMeta == NULL ) {
		return;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	pMeta = __xl6FutureMetaFind(pFuture);
	if ( pMeta != NULL ) {
		if ( (iDelta < 0) && (pMeta->Pending < -iDelta) ) {
			pMeta->Pending = 0;
		} else {
			pMeta->Pending += iDelta;
		}
	}
	(void)xrtMutexUnlock(&__xl6FutureLock);
}



/* 释放旧结果中仍由适配层持有的值和错误。 */
static void __xl6FutureResultFree(xl6_future_result* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( ((pResult->iFlags & XL6_FUTURE_OWN_VALUE) != 0) &&
		(pResult->pfnFreeValue != NULL) ) {
		pResult->pfnFreeValue(pResult->pValue);
	}
	xrtErrorFree(pResult->Error);
	memset(pResult, 0, sizeof(*pResult));
}



/* 旧值析构过程需要一个小上下文才能进入 XRT 的双参数析构契约。 */
typedef struct xl6_future_destroy {
	void (*Proc)(void* pValue);
	xfuture* Owner;
} xl6_future_destroy;



/* 释放适配后的 Future 成功值及其借用所有者。 */
static void __xl6FutureValueDestroy(void* pValue, void* pData)
{
	xl6_future_destroy* pDestroy = (xl6_future_destroy*)pData;

	if ( pDestroy->Proc != NULL ) {
		pDestroy->Proc(pValue);
	}
	xrtFutureDestroy(pDestroy->Owner);
	xrtFree(pDestroy);
}



/* 使用结构化错误完成 Promise。 */
static bool __xl6FutureReject(xpromise* pPromise, const char* sMessage)
{
	xerror* pError = xrtErrorCreate(
		XERR_INTERNAL,
		"xlang.async",
		1,
		sMessage != NULL ? sMessage : "asynchronous operation failed"
	);
	bool bResult;

	if ( pError == NULL ) {
		return xrtPromiseClose(pPromise);
	}
	bResult = xrtPromiseReject(pPromise, pError);
	xrtErrorFree(pError);
	return bResult;
}



/* 使用调用方提供的结构化错误完成 Promise；空错误退化为内部错误。 */
static bool __xl6FutureRejectError(
	xpromise* pPromise,
	const xerror* pError,
	const char* sFallback
)
{
	if ( pError != NULL ) {
		return xrtPromiseReject(pPromise, pError);
	}
	return __xl6FutureReject(pPromise, sFallback);
}



/* 把私有回调结果转移到新版 XRT Promise。 */
static bool __xl6FutureComplete(
	xpromise* pPromise,
	xfuture* pOwner,
	int iReturn,
	xl6_future_result* pResult
)
{
	xl6_future_destroy* pDestroy;
	bool bCompleted;

	if ( (iReturn != 0) ||
		(pResult->iStatus == XL6_FUTURE_STATUS_FAILED) ) {
		bCompleted = __xl6FutureRejectError(
			pPromise,
			pResult->Error,
			"asynchronous operation failed"
		);
		__xl6FutureResultFree(pResult);
		return bCompleted;
	}
	if ( pResult->iStatus == XL6_FUTURE_STATUS_CANCELLED ) {
		bCompleted = xrtPromiseCancel(pPromise);
		__xl6FutureResultFree(pResult);
		return bCompleted;
	}
	if ( pResult->iStatus == XL6_FUTURE_STATUS_CLOSED ) {
		bCompleted = xrtPromiseClose(pPromise);
		__xl6FutureResultFree(pResult);
		return bCompleted;
	}
	if ( pResult->iStatus != XL6_FUTURE_STATUS_OK ) {
		bCompleted = __xl6FutureReject(pPromise, "invalid future result status");
		__xl6FutureResultFree(pResult);
		return bCompleted;
	}
	if ( (pResult->iFlags & XL6_FUTURE_OWN_VALUE) != 0 ) {
		pDestroy = (xl6_future_destroy*)xrtCalloc(
			1,
			sizeof(xl6_future_destroy)
		);
		if ( pDestroy == NULL ) {
			__xl6FutureResultFree(pResult);
			return __xl6FutureReject(pPromise, "future value ownership allocation failed");
		}
		pDestroy->Proc = pResult->pfnFreeValue;
		bCompleted = xrtPromiseResolveOwned(
			pPromise,
			pResult->pValue,
			__xl6FutureValueDestroy,
			pDestroy
		);
		if ( !bCompleted ) {
			__xl6FutureValueDestroy(pResult->pValue, pDestroy);
		}
		pResult->iFlags &= ~XL6_FUTURE_OWN_VALUE;
		pResult->pValue = NULL;
		pResult->pfnFreeValue = NULL;
	} else if ( pResult->pValue != NULL ) {
		pDestroy = (xl6_future_destroy*)xrtCalloc(
			1,
			sizeof(xl6_future_destroy)
		);
		if ( pDestroy == NULL ) {
			__xl6FutureResultFree(pResult);
			return __xl6FutureReject(pPromise, "future borrowed value retention failed");
		}
		pDestroy->Owner = xrtFutureRef(pOwner);
		if ( pDestroy->Owner == NULL ) {
			xrtFree(pDestroy);
			__xl6FutureResultFree(pResult);
			return __xl6FutureReject(pPromise, "future borrowed value owner is unavailable");
		}
		bCompleted = xrtPromiseResolveOwned(
			pPromise,
			pResult->pValue,
			__xl6FutureValueDestroy,
			pDestroy
		);
		if ( !bCompleted ) {
			__xl6FutureValueDestroy(pResult->pValue, pDestroy);
		}
	} else {
		bCompleted = xrtPromiseResolve(pPromise, NULL);
	}
	xrtErrorFree(pResult->Error);
	memset(pResult, 0, sizeof(*pResult));
	return bCompleted;
}



/* 填充只借用当前 XRT Future 结果的回调视图。 */
static void __xl6FutureInput(
	const xfutureresult* pInput,
	xl6_future_result* pOutput
)
{
	memset(pOutput, 0, sizeof(*pOutput));
	if ( pInput->State == XFUTURE_RESOLVED ) {
		pOutput->iStatus = XL6_FUTURE_STATUS_OK;
	} else if ( pInput->State == XFUTURE_FAILED ) {
		pOutput->iStatus = XL6_FUTURE_STATUS_FAILED;
	} else if ( pInput->State == XFUTURE_CANCELLED ) {
		pOutput->iStatus = XL6_FUTURE_STATUS_CANCELLED;
	} else {
		pOutput->iStatus = XL6_FUTURE_STATUS_CLOSED;
	}
	pOutput->pValue = pInput->Value;
	pOutput->Error = (xerror*)pInput->Error;
}



/* 线程任务上下文把私有回调与新版 Promise 绑定。 */
typedef struct xl6_future_thread {
	xl6_task_thread_proc Proc;
	void* Arg;
	xpromise* Promise;
	xcancel* Cancel;
} xl6_future_thread;



/* 在线程退出前把任务结果发布到 Future。 */
static int32 __xl6FutureThreadEntry(void* pData)
{
	xl6_future_thread* pThread = (xl6_future_thread*)pData;
	xl6_future_result tResult;
	int iReturn;

	memset(&tResult, 0, sizeof(tResult));
	iReturn = pThread->Proc(pThread->Arg, &tResult);
	if ( xrtCancelRequested(pThread->Cancel) ) {
		__xl6FutureResultFree(&tResult);
		(void)xrtPromiseCancel(pThread->Promise);
	} else {
		(void)__xl6FutureComplete(
			pThread->Promise,
			NULL,
			iReturn,
			&tResult
		);
	}
	xrtCancelDestroy(pThread->Cancel);
	xrtPromiseDestroy(pThread->Promise);
	xrtFree(pThread);
	return iReturn;
}



/* 启动一个与语言任务回调匹配的分离线程。 */
XL_API xfuture* xl6FutureRunThread(
	xl6_task_thread_proc pProc,
	void* pArg,
	size_t iStackSize
)
{
	xl6_future_thread* pContext;
	xfuture* pFuture;
	xthread* pThread;

	if ( pProc == NULL ) {
		return NULL;
	}
	pContext = (xl6_future_thread*)xrtCalloc(1, sizeof(xl6_future_thread));
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pContext->Promise == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	pContext->Cancel = xrtPromiseCancelToken(pContext->Promise);
	if ( pContext->Cancel == NULL ) {
		xrtPromiseDestroy(pContext->Promise);
		xrtFutureDestroy(pFuture);
		xrtFree(pContext);
		return NULL;
	}
	if ( !__xl6FutureMetaBindPromise(pFuture, pContext->Promise) ) {
		xrtCancelDestroy(pContext->Cancel);
		xrtPromiseDestroy(pContext->Promise);
		xrtFutureDestroy(pFuture);
		xrtFree(pContext);
		return NULL;
	}
	pContext->Proc = pProc;
	pContext->Arg = pArg;
	pThread = xrtThreadCreate(__xl6FutureThreadEntry, pContext, iStackSize);
	if ( pThread == NULL ) {
		xrtCancelDestroy(pContext->Cancel);
		xrtPromiseDestroy(pContext->Promise);
		xl6FutureDestroy(pFuture);
		xrtFree(pContext);
		return NULL;
	}
	xrtThreadDestroy(pThread);
	return pFuture;
}



/* 增加 Future 引用。 */
XL_API xfuture* xl6FutureRef(xfuture* pFuture)
{
	xfuture* pResult = xrtFutureRef(pFuture);

	if ( pResult != NULL ) {
		__xl6FutureMetaRef(pResult);
	}
	return pResult;
}



/* 释放 Future 引用。 */
XL_API void xl6FutureDestroy(xfuture* pFuture)
{
	if ( pFuture != NULL ) {
		__xl6FutureMetaDestroy(pFuture);
		xrtFutureDestroy(pFuture);
	}
}



/* 永久等待 Future 进入终态。 */
XL_API bool xl6FutureWait(xfuture* pFuture)
{
	bool bResult = xrtFutureWait(pFuture) == XWAIT_OK;

	__xl6FutureMetaObserve(pFuture);
	return bResult && (xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 在语言层毫秒超时内等待 Future。 */
XL_API bool xl6FutureWaitFor(xfuture* pFuture, long long iTimeoutMs)
{
	bool bResult = xrtFutureWaitFor(
		pFuture,
		__xl6FutureTimeout(iTimeoutMs)
	) == XWAIT_OK;

	__xl6FutureMetaObserve(pFuture);
	return bResult && (xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 等待 Future 到语言层单调毫秒截止时间。 */
XL_API bool xl6FutureWaitUntil(xfuture* pFuture, long long iDeadlineMs)
{
	bool bResult = xrtFutureWaitUntil(
		pFuture,
		__xl6FutureDeadline(iDeadlineMs)
	) == XWAIT_OK;

	__xl6FutureMetaObserve(pFuture);
	return bResult && (xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 设置稳定到 Future 最后一个语言引用释放为止的调试名称。 */
XL_API bool xl6FutureSetName(xfuture* pFuture, const char* sName)
{
	xl6_future_name* pName;
	xl6_future_meta* pMeta;
	size_t iSize;

	if ( (pFuture == NULL) || (sName == NULL) ) {
		return false;
	}
	iSize = strlen(sName);
	if ( iSize > (SIZE_MAX - sizeof(xl6_future_name)) ) {
		return false;
	}
	pName = (xl6_future_name*)xrtMalloc(sizeof(xl6_future_name) + iSize);
	if ( pName == NULL ) {
		return false;
	}
	memcpy(pName->Text, sName, iSize + 1);
	pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	if ( pMeta == NULL ) {
		xrtFree(pName);
		return false;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	pName->Next = pMeta->Names;
	pMeta->Names = pName;
	pMeta->Name = pName->Text;
	(void)xrtMutexUnlock(&__xl6FutureLock);
	return true;
}



/* 返回借用的调试名称。 */
XL_API const char* xl6FutureName(xfuture* pFuture)
{
	xl6_future_meta* pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	const char* sName = NULL;

	if ( pMeta == NULL ) {
		return NULL;
	}
	(void)xrtMutexLock(&__xl6FutureLock);
	sName = pMeta->Name;
	(void)xrtMutexUnlock(&__xl6FutureLock);
	return sName;
}



/* 返回 Future 首次进入语言适配层的时间。 */
XL_API unsigned long long xl6FutureCreatedMs(xfuture* pFuture)
{
	xl6_future_meta* pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	unsigned long long iResult = 0;

	if ( pMeta != NULL ) {
		(void)xrtMutexLock(&__xl6FutureLock);
		iResult = pMeta->CreatedMs;
		(void)xrtMutexUnlock(&__xl6FutureLock);
	}
	return iResult;
}



/* 返回首次观测到终态的时间，未完成时返回零。 */
XL_API unsigned long long xl6FutureCompletedMs(xfuture* pFuture)
{
	xl6_future_meta* pMeta;
	unsigned long long iResult = 0;

	__xl6FutureMetaObserve(pFuture);
	pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	if ( pMeta != NULL ) {
		(void)xrtMutexLock(&__xl6FutureLock);
		iResult = pMeta->CompletedMs;
		(void)xrtMutexUnlock(&__xl6FutureLock);
	}
	return iResult;
}



/* 返回尚未触发的语言适配层延续数量。 */
XL_API int xl6FuturePendingCount(xfuture* pFuture)
{
	xl6_future_meta* pMeta = __xl6FutureMetaEnsure(pFuture, 1);
	int iResult = 0;

	if ( pMeta != NULL ) {
		(void)xrtMutexLock(&__xl6FutureLock);
		iResult = pMeta->Pending;
		(void)xrtMutexUnlock(&__xl6FutureLock);
	}
	return iResult;
}



/* 返回与 xlang Future.State 数值一致的状态。 */
XL_API int xl6FutureState(xfuture* pFuture)
{
	int iState = (int)xrtFutureState(pFuture);

	__xl6FutureMetaObserve(pFuture);
	return iState;
}



/* 返回语言层简化完成状态。 */
XL_API int xl6FutureStatus(xfuture* pFuture)
{
	int iState = xl6FutureState(pFuture);

	if ( (iState == XFUTURE_PENDING) || (iState == XFUTURE_RESOLVED) ) {
		return 0;
	}
	if ( iState == XFUTURE_CANCELLED ) {
		return -2;
	}
	if ( iState == XFUTURE_CLOSED ) {
		return -3;
	}
	return -1;
}



/* 返回借用的成功值。 */
XL_API void* xl6FutureValue(xfuture* pFuture)
{
	return xrtFutureValue(pFuture);
}



/* 返回借用的结构化错误消息。 */
XL_API const xerror* xl6FutureError(xfuture* pFuture)
{
	return xrtFutureError(pFuture);
}



/* 请求 Future 的生产过程协作取消。 */
XL_API bool xl6FutureCancel(xfuture* pFuture)
{
	xpromise* pPromise = __xl6FutureMetaPromise(pFuture);
	bool bResult = xrtFutureCancel(pFuture);

	if ( bResult && (pPromise != NULL) ) {
		(void)xrtPromiseCancel(pPromise);
	}
	xrtPromiseDestroy(pPromise);
	__xl6FutureMetaObserve(pFuture);
	return bResult;
}



/* 取消转发上下文由源 Future 的终态观察器管理。 */
typedef struct xl6_future_cancel_link {
	xfuture* Target;
	xcancelwatch* Watch;
} xl6_future_cancel_link;



/* 源 Future 收到取消请求时转发到目标 Future。 */
static void __xl6FutureCancelForward(void* pData)
{
	xl6_future_cancel_link* pLink = (xl6_future_cancel_link*)pData;

	(void)xrtFutureCancel(pLink->Target);
}



/* 终态观察器本身不改变源结果。 */
static void __xl6FutureCancelDone(
	const xfutureresult* pInput,
	void* pData
)
{
	(void)pInput;
	(void)pData;
}



/* 源终态后注销取消观察并释放目标引用。 */
static void __xl6FutureCancelLinkFree(void* pValue, void* pData)
{
	xl6_future_cancel_link* pLink = (xl6_future_cancel_link*)pValue;

	(void)pData;
	xrtCancelUnwatch(pLink->Watch);
	xrtFutureDestroy(pLink->Target);
	xrtFree(pLink);
}



/* 把一个 Future 的取消请求转发到另一个 Future。 */
XL_API bool xl6FutureLinkCancel(xfuture* pFuture, xfuture* pTarget)
{
	xl6_future_cancel_link* pLink;
	xcancel* pCancel;
	xfuture* pObserver;

	if ( (pFuture == NULL) || (pTarget == NULL) ) {
		return false;
	}
	pLink = (xl6_future_cancel_link*)xrtCalloc(
		1,
		sizeof(xl6_future_cancel_link)
	);
	if ( pLink == NULL ) {
		return false;
	}
	pLink->Target = xrtFutureRef(pTarget);
	pCancel = xrtFutureCancelToken(pFuture);
	if ( (pLink->Target == NULL) || (pCancel == NULL) ) {
		xrtFutureDestroy(pLink->Target);
		xrtCancelDestroy(pCancel);
		xrtFree(pLink);
		return false;
	}
	pLink->Watch = xrtCancelWatch(
		pCancel,
		__xl6FutureCancelForward,
		pLink
	);
	xrtCancelDestroy(pCancel);
	if ( pLink->Watch == NULL ) {
		xrtFutureDestroy(pLink->Target);
		xrtFree(pLink);
		return false;
	}
	pObserver = xrtFutureFinallyOwned(
		pFuture,
		__xl6FutureCancelDone,
		pLink,
		__xl6FutureCancelLinkFree,
		NULL
	);
	if ( pObserver == NULL ) {
		__xl6FutureCancelLinkFree(pLink, NULL);
		return false;
	}
	xrtFutureDestroy(pObserver);
	return true;
}



/* 创建组合 Future 并纳入语言引用跟踪。 */
static xfuture* __xl6FutureTrackNew(xfuture* pFuture)
{
	if ( pFuture != NULL ) {
		(void)__xl6FutureMetaEnsure(pFuture, 1);
	}
	return pFuture;
}



/* 等待任意输入 Future 完成。 */
XL_API xfuture* xl6FutureAny(xfuture** pFutures, int iCount)
{
	return iCount > 0 ? __xl6FutureTrackNew(
		xrtFutureAny(pFutures, (size_t)iCount)
	) : NULL;
}



/* 等待全部输入 Future 完成。 */
XL_API xfuture* xl6FutureAll(xfuture** pFutures, int iCount)
{
	return iCount >= 0 ? __xl6FutureTrackNew(
		xrtFutureAll(pFutures, (size_t)iCount)
	) : NULL;
}



/* 把底层 Race 选中的源终态原样透传给语言 Future。 */
static void __xl6FutureRaceForward(
	const xfutureresult* pInput,
	xpromise* pOutput,
	void* pData
)
{
	const xfuturepick* pPick;

	(void)pData;
	pPick = pInput != NULL ? (const xfuturepick*)pInput->Value : NULL;
	if ( (pPick == NULL) || (pPick->Future == NULL) ||
		!xrtPromiseForward(pOutput, pPick->Future) ) {
		(void)__xl6FutureReject(
			pOutput,
			"task.race result forwarding failed"
		);
	}
}



/* 等待首个终态、取消其余输入并透传胜出源结果。 */
XL_API xfuture* xl6FutureRace(xfuture** pFutures, int iCount)
{
	xfuture* pPick;
	xfuture* pResult;

	if ( iCount <= 0 ) {
		return NULL;
	}
	pPick = xrtFutureRace(pFutures, (size_t)iCount);
	if ( pPick == NULL ) {
		return NULL;
	}
	pResult = xrtFutureThen(pPick, __xl6FutureRaceForward, NULL);
	if ( (pResult != NULL) && !xrtFutureDone(pResult) &&
		!xl6FutureLinkCancel(pResult, pPick) ) {
		xrtFutureDestroy(pResult);
		pResult = NULL;
	}
	xrtFutureDestroy(pPick);
	return __xl6FutureTrackNew(pResult);
}



/* One physical mapper context; native groups own the only source slots.
 * New XLIR calls carry explicit code ownership. The old bare-boxer entry is
 * still supported, but cannot acquire collection authority by compatibility. */
typedef struct xl6_future_all_xvalues {
    volatile int32 References;
    xl6_future_payload_box_proc Box;
    XL6_CodeLease* Lease;
    const void* Claim;
    unsigned Mode;
    bool Certified, Cleared;
} xl6_future_all_xvalues;
enum { XL6_AGGREGATE_ALL, XL6_AGGREGATE_RACE, XL6_AGGREGATE_ANY };
static xfuture* __xl6FutureAggregateStart(unsigned, xfuture* const*, size_t,
    xl6_future_payload_box_proc, const XL6_CodeOwner*, bool);

static void __xl6FutureSetAggregateError(const char* message)
{
    xerrordesc desc = {XERR_INTERNAL, 1, 0, "xlang.async.aggregate",
        "aggregate", message != NULL ? message : "future aggregate failed", "TaskError", NULL};
    xerror* error;
    if (xrtGetError() != NULL) return;
    error = xrtErrorBuild(&desc);
    if (error != NULL) xrtSetErrorTake(error);
    else xrtSetErrorKind(XERR_MEMORY);
}
static void __xl6FutureAggregateReject(xpromise* output, const char* message)
{
    xerror* error;
    __xl6FutureSetAggregateError(message); error = xrtTakeError();
    if (error == NULL || !xrtPromiseReject(output, error)) (void)xrtPromiseClose(output);
    xrtErrorFree(error);
}
static bool __xl6FutureAggregateCount(const void* data, size_t* count)
{
    const xl6_future_all_xvalues* value = data;
    if (!value || !count || value->Cleared || value->References <= 0) return false;
    *count = (size_t)value->References; return true;
}
static bool __xl6FutureAggregateTrace(const void* data, xrtownershipvisitor visit, void* context)
{
    const xl6_future_all_xvalues* value = data; size_t count;
    return visit && __xl6FutureAggregateCount(data, &count) && (!value->Lease || visit(xl6CodeLeaseOwnership(value->Lease), context));
}
static const xrtownershipops __xl6FutureAggregateOps = {__xl6FutureAggregateCount, __xl6FutureAggregateTrace};
static void __xl6FutureAggregateDrop(const void* data)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data; XL6_CodeLease* lease = NULL;
    xrtownershipscope mutation = {0};
    if (!value) return;
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    int32 left = xrtRefRelease(&value->References);
    if (left < 0) abort();
    if (!left) { lease = value->Lease; value->Lease = NULL; xrtFree(value); }
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    xl6CodeLeaseRelease(lease); /* only resident code remains after this release */
}
static void __xl6FutureAggregateContextFree(void* data, void* context)
{ if (context) abort(); __xl6FutureAggregateDrop(data); }
static bool __xl6FutureAggregateContextTrace(const void* data, const void* context,
    xrtownershipvisitor visit, void* visitContext)
{
    return context == NULL && visit != NULL && visit((xrtownershipref){data, &__xl6FutureAggregateOps}, visitContext);
}
static bool __xl6FutureAllForwardNonSuccess(const xfutureall* input, xpromise* output)
{
    if (input == NULL) {
        __xl6FutureAggregateReject(output, "task.all has no source descriptor"); return true;
    }
    for (size_t i = 0; i < input->Count; ++i) {
        if (xrtFutureState(input->Futures[i]) != XFUTURE_RESOLVED) {
            if (!xrtPromiseForward(output, input->Futures[i]))
                __xl6FutureAggregateReject(output, "task.all source outcome forwarding failed");
            return true;
        }
    }
    return false;
}
static void __xl6FutureAllXValuesMap(const xfutureall* input, xpromise* output, void* data)
{
    const xl6_future_all_xvalues* all = data;
    xvalue* array; xvalue** box;
    if (input == NULL || all == NULL || all->Box == NULL) {
        __xl6FutureAggregateReject(output, "task.all has no verified payload boxer"); return;
    }
    /* Preserve the exact failing/cancelled/closed source outcome. No boxing
     * and no legacy metadata allocation is needed to observe those outcomes. */
    if (__xl6FutureAllForwardNonSuccess(input, output)) return;
    array = xrtValueArray();
    if (array == NULL) {
        __xl6FutureAggregateReject(output, "task.all result array allocation failed"); return;
    }
    for (size_t i = 0; i < input->Count; ++i) {
        const void* payload = xrtFutureValue(input->Futures[i]);
        xvalue* item = payload != NULL ? all->Box(payload) : NULL;
        if (item == NULL || !xrtValueArrayAppendNew(array, item)) {
            xrtValueRelease(array);
            __xl6FutureAggregateReject(output, "task.all payload boxing failed"); return;
        }
    }
    box = xrtMalloc(sizeof(*box));
    if (box == NULL) {
        xrtValueRelease(array);
        __xl6FutureAggregateReject(output, "task.all result box allocation failed"); return;
    }
    *box = array;
    if (!xl6PayloadResolveOwned(output, box, XL6_FUTURE_PAYLOAD_XVALUE)) {
        xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE)(box, NULL);
        __xl6FutureAggregateReject(output, "task.all result publication failed");
    }
}
XL_API xfuture* xl6FutureAllXValues(xfuture** futures, int count, xl6_future_payload_box_proc box)
{
    if (futures == NULL || count <= 0 || box == NULL) {
        __xl6FutureSetAggregateError("task.all requires a non-empty verified Future payload list"); return NULL;
    }
    return __xl6FutureAggregateStart(XL6_AGGREGATE_ALL, futures, (size_t)count, box, NULL, false);
}
static void __xl6FutureAllVoidMap(const xfutureall* input, xpromise* output, void* data)
{
    (void)data;
    if (__xl6FutureAllForwardNonSuccess(input, output)) return;
    if (!xrtPromiseResolve(output, NULL))
        __xl6FutureAggregateReject(output, "task.all void result publication failed");
}
XL_API xfuture* xl6FutureAllChecked(xfuture** futures, int count)
{
    xfuture* output;
    if (futures == NULL || count <= 0) {
        __xl6FutureSetAggregateError("task.all requires a non-empty Future list"); return NULL;
    }
    output = __xl6FutureAggregateStart(XL6_AGGREGATE_ALL, futures, (size_t)count, NULL, NULL, true);
    if (output == NULL) __xl6FutureSetAggregateError("task.all preparation failed");
    return output;
}
static void __xl6FutureRaceMap(const xfuturepick* input, xpromise* output, void* data)
{
    (void)data;
    if (input == NULL || input->Future == NULL || !xrtPromiseForward(output, input->Future))
        __xl6FutureAggregateReject(output, "task.race source outcome forwarding failed");
}
XL_API xfuture* xl6FutureRaceChecked(xfuture** futures, int count)
{
    xfuture* output;
    if (futures == NULL || count <= 0) {
        __xl6FutureSetAggregateError("task.race requires a non-empty Future payload list"); return NULL;
    }
    output = __xl6FutureAggregateStart(XL6_AGGREGATE_RACE, futures, (size_t)count, NULL, NULL, true);
    if (output == NULL) __xl6FutureSetAggregateError("task.race preparation failed");
    return output;
}
static void __xl6FutureWhenAnyIndexMap(const xfuturepick* input, xpromise* output, void* data)
{
    int64* index; (void)data;
    if (input == NULL || input->Future == NULL || input->Index > (size_t)INT64_MAX) {
        __xl6FutureAggregateReject(output, "task.whenAny has no valid source index"); return;
    }
    index = xrtMalloc(sizeof(*index));
    if (index == NULL) {
        __xl6FutureAggregateReject(output, "task.whenAny result allocation failed"); return;
    }
    *index = (int64)input->Index;
    if (!xl6PayloadResolveOwned(output, index, XL6_FUTURE_PAYLOAD_VALUE)) {
        xrtFree(index); __xl6FutureAggregateReject(output, "task.whenAny result publication failed");
    }
}
XL_API xfuture* xl6FutureWhenAnyIndex(xfuture* const* futures, size_t count)
{
    xfuture* output;
    if (futures == NULL || count == 0 || count > (size_t)INT64_MAX) {
        __xl6FutureSetAggregateError("task.whenAny requires a non-empty Future payload list"); return NULL;
    }
    output = __xl6FutureAggregateStart(XL6_AGGREGATE_ANY, futures, count, NULL, NULL, true);
    if (output == NULL) __xl6FutureSetAggregateError("task.whenAny preparation failed");
    return output;
}

static void __xl6FutureAggregateAllMap(const xfutureall* input, xpromise* output, void* data)
{
    xl6_future_all_xvalues* value = data; XL6_CodeScope execution = {0};
    if (!value || value->Cleared || value->Mode != XL6_AGGREGATE_ALL) abort();
    xl6CodeScopeEnter(&execution, value->Lease);
    if (value->Box) __xl6FutureAllXValuesMap(input, output, value);
    else __xl6FutureAllVoidMap(input, output, value);
    xl6CodeScopeLeave(&execution);
}
static void __xl6FutureAggregatePickMap(const xfuturepick* input, xpromise* output, void* data)
{
    xl6_future_all_xvalues* value = data;
    if (!value || value->Cleared || value->Box || value->Lease) abort();
    if (value->Mode == XL6_AGGREGATE_RACE) __xl6FutureRaceMap(input, output, value);
    else if (value->Mode == XL6_AGGREGATE_ANY) __xl6FutureWhenAnyIndexMap(input, output, value);
    else abort();
}
static const xfuturecombineownershipv1 __xl6FutureAggregatePolicy = {
    sizeof(xfuturecombineownershipv1), __xl6FutureAggregateAllMap, __xl6FutureAggregatePickMap,
    __xl6FutureAggregateDrop, &__xl6FutureAggregateOps
};
const xfuturecombineownershipv1* xl6FutureAggregatePolicyV1Get(void)
{ return &__xl6FutureAggregatePolicy; }
static xfuture* __xl6FutureAggregateStart(unsigned mode, xfuture* const* futures, size_t count,
    xl6_future_payload_box_proc box, const XL6_CodeOwner* owner, bool certified)
{
    xl6_future_all_xvalues* data; XL6_CodeLease* lease = NULL; xfuture* output;
    xrtownershipscope mutation = {0};
    if (!futures || !count || count > (size_t)INT64_MAX ||
        (mode != XL6_AGGREGATE_ANY && count > (size_t)INT_MAX) || mode > XL6_AGGREGATE_ANY ||
        (mode != XL6_AGGREGATE_ALL && box) || (!box && owner) || (!certified && owner)) {
        __xl6FutureSetAggregateError("task aggregate requires a valid non-empty Future list and mapper ABI"); return NULL;
    }
    if (box && certified && xl6CodeLeaseAcquireScoped(owner, &lease) != XL6_CODE_LEASE_ACQUIRED) {
        __xl6FutureSetAggregateError("task.all mapper code admission failed"); return NULL;
    }
    if (!xrtOwnershipMutationBegin(&mutation)) { xl6CodeLeaseRelease(lease); return NULL; }
    data = xrtCalloc(1, sizeof(*data));
    if (data) {
        data->References = 1; data->Box = box; data->Lease = lease;
        data->Mode = mode; data->Certified = certified;
    }
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    if (!data) {
        xerror* error = xrtTakeError(); xl6CodeLeaseRelease(lease); xrtClearError(); xrtSetErrorTake(error);
        __xl6FutureSetAggregateError("task aggregate mapper allocation failed"); return NULL;
    }
    if (!certified) {
        /* Legacy borrowed code retains conservative callback scopes and never
         * matches the new native mapped-policy admission contract. */
        output = xrtFutureAllMapOwnedTraced(futures, count, __xl6FutureAggregateAllMap,
            data, __xl6FutureAggregateContextFree, NULL, __xl6FutureAggregateContextTrace);
    } else if (mode == XL6_AGGREGATE_ALL) output = xrtFutureAllMapOwnedPolicyV1(futures, count, data, &__xl6FutureAggregatePolicy);
    else if (mode == XL6_AGGREGATE_RACE) output = xrtFutureRaceMapOwnedPolicyV1(futures, count, data, &__xl6FutureAggregatePolicy);
    else output = xrtFutureAnyMapOwnedPolicyV1(futures, count, data, &__xl6FutureAggregatePolicy);
    if (!output) {
        xerror* error = xrtTakeError(); __xl6FutureAggregateDrop(data); xrtClearError(); xrtSetErrorTake(error);
        __xl6FutureSetAggregateError("task aggregate preparation failed");
    }
    return output;
}
static bool __xl6FutureAggregateHold(const void* data)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data; xrtownershipscope mutation = {0}; bool held;
    if (!xrtOwnershipMutationBegin(&mutation)) return false;
    held = !value->Cleared && xrtRefRetain(&value->References) > 0;
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    return held;
}
static bool __xl6FutureAggregateClaim(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data;
    if (!token || value->Cleared || (value->Claim && value->Claim != token)) return false;
    value->Claim = token; return true;
}
static void __xl6FutureAggregateRestore(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data;
    if (!token || value->Claim != token || value->Cleared) abort();
    value->Claim = NULL;
}
static void __xl6FutureAggregateClear(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data;
    if (!token || value->Claim != token || value->Cleared) abort();
    value->Cleared = true;
}
static bool __xl6FutureAggregateFinish(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data; XL6_CodeLease* lease;
    xrtownershipscope mutation = {0};
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    if (!token || value->Claim != token || !value->Cleared) abort();
    lease = value->Lease; value->Lease = NULL; value->Box = NULL;
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    xl6CodeLeaseRelease(lease); return true;
}
const xrtownershipadapterv1* xl6FutureAggregateOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xl6FutureAggregateHold,
        __xl6FutureAggregateDrop, __xl6FutureAggregateClaim, __xl6FutureAggregateRestore,
        NULL, __xl6FutureAggregateClear, __xl6FutureAggregateFinish};
    const xl6_future_all_xvalues* data; size_t count;
    if (!ref.Data || ref.Ops != &__xl6FutureAggregateOps) return NULL;
    data = ref.Data;
    return data->Certified && __xl6FutureAggregateCount(data, &count) ? &adapter : NULL;
}
static xfuture* __xl6FutureAggregateAll(xfuture* const* futures, size_t count, XL6_FutureAggregateBoxV1 box, const XL6_CodeOwner* owner)
{ return __xl6FutureAggregateStart(XL6_AGGREGATE_ALL, futures, count, box, owner, true); }
static xfuture* __xl6FutureAggregateRace(xfuture* const* futures, size_t count)
{ return __xl6FutureAggregateStart(XL6_AGGREGATE_RACE, futures, count, NULL, NULL, true); }
static xfuture* __xl6FutureAggregateAny(xfuture* const* futures, size_t count)
{ return __xl6FutureAggregateStart(XL6_AGGREGATE_ANY, futures, count, NULL, NULL, true); }
static xfuture* __xl6FutureAggregateArray(unsigned mode, const xvalue* array, XL6_FutureAggregateBoxV1 box, const XL6_CodeOwner* owner)
{
    xfuture** futures; xfuture* output; size_t count;
    if (!array || !(count = xrtValueCount(array)) || count > SIZE_MAX / sizeof(*futures) ||
        count > (size_t)INT64_MAX || (mode != XL6_AGGREGATE_ANY && count > (size_t)INT_MAX)) {
        __xl6FutureSetAggregateError("task aggregate requires a non-empty representable Future array"); return NULL;
    }
    futures = xrtMalloc(count * sizeof(*futures));
    if (!futures) { __xl6FutureSetAggregateError("task aggregate Future array allocation failed"); return NULL; }
    for (size_t i = 0; i < count; ++i) {
        const xvalue* item = xrtValueArrayGet(array, i);
        futures[i] = item ? xrtValueGetFuture(item) : NULL;
        if (!futures[i]) { xrtFree(futures); __xl6FutureSetAggregateError("task aggregate array contains a non-Future value"); return NULL; }
    }
    output = __xl6FutureAggregateStart(mode, futures, count, box, owner, true);
    xrtFree(futures); return output;
}
static xfuture* __xl6FutureAggregateAllArray(const xvalue* array, XL6_FutureAggregateBoxV1 box, const XL6_CodeOwner* owner)
{ return __xl6FutureAggregateArray(XL6_AGGREGATE_ALL, array, box, owner); }
static xfuture* __xl6FutureAggregateRaceArray(const xvalue* array)
{ return __xl6FutureAggregateArray(XL6_AGGREGATE_RACE, array, NULL, NULL); }
static xfuture* __xl6FutureAggregateAnyArray(const xvalue* array)
{ return __xl6FutureAggregateArray(XL6_AGGREGATE_ANY, array, NULL, NULL); }
const XL6_FutureAggregateRuntimeV1* xl6FutureAggregateRuntimeV1Get(void)
{
    static const XL6_FutureAggregateRuntimeV1 runtime = {__xl6FutureAggregateAll, __xl6FutureAggregateRace, __xl6FutureAggregateAny,
        __xl6FutureAggregateAllArray, __xl6FutureAggregateRaceArray, __xl6FutureAggregateAnyArray};
    return &runtime;
}



/* 返回 Any 或 Race 选中的输入下标。 */
XL_API int xl6FuturePickIndex(xfuture* pFuture)
{
	const xfuturepick* pPick = (const xfuturepick*)xrtFutureValue(pFuture);

	return (pPick != NULL) && (pPick->Index <= (size_t)INT_MAX) ?
		(int)pPick->Index : -1;
}



/* 返回 All 结果中的输入数量。 */
XL_API int xl6FutureAllCount(xfuture* pFuture)
{
	const xfutureall* pAll = (const xfutureall*)xrtFutureValue(pFuture);

	return (pAll != NULL) && (pAll->Count <= (size_t)INT_MAX) ?
		(int)pAll->Count : -1;
}



/* 返回 All 结果中指定输入 Future 的借用成功值。 */
XL_API void* xl6FutureAllValue(xfuture* pFuture, int iIndex)
{
	const xfutureall* pAll = (const xfutureall*)xrtFutureValue(pFuture);

	if ( (pAll == NULL) || (iIndex < 0) ||
		((size_t)iIndex >= pAll->Count) ) {
		return NULL;
	}
	return xrtFutureValue(pAll->Futures[iIndex]);
}



/* 延续上下文统一管理用户参数、源借用和计数。 */
typedef struct xl6_future_continue {
	xfuture* Source;
	xl6_future_continue_proc Proc;
	xl6_future_finally_proc Finally;
	void* Arg;
	xl6_future_arg_free_proc FreeArg;
	bool Pending;
} xl6_future_continue;



/* 延续命中后把私有结果写入输出 Promise。 */
static void __xl6FutureContinueRun(
	const xfutureresult* pInput,
	xpromise* pOutput,
	void* pData
)
{
	xl6_future_continue* pContinue = (xl6_future_continue*)pData;
	xl6_future_result tInput;
	xl6_future_result tOutput;
	int iReturn;

	if ( pContinue->Pending ) {
		pContinue->Pending = false;
		__xl6FutureMetaPending(pContinue->Source, -1);
	}
	__xl6FutureInput(pInput, &tInput);
	memset(&tOutput, 0, sizeof(tOutput));
	iReturn = pContinue->Proc(&tInput, pContinue->Arg, &tOutput);
	(void)__xl6FutureComplete(
		pOutput,
		pContinue->Source,
		iReturn,
		&tOutput
	);
}



/* Finally 观察器只调用语言回调，原结果由 XRT 自动透传。 */
static void __xl6FutureFinallyRun(
	const xfutureresult* pInput,
	void* pData
)
{
	xl6_future_continue* pContinue = (xl6_future_continue*)pData;
	xl6_future_result tInput;

	if ( pContinue->Pending ) {
		pContinue->Pending = false;
		__xl6FutureMetaPending(pContinue->Source, -1);
	}
	__xl6FutureInput(pInput, &tInput);
	pContinue->Finally(&tInput, pContinue->Arg);
}



/* 延续结束、跳过或创建失败后统一释放上下文。 */
static void __xl6FutureContinueFree(void* pValue, void* pData)
{
	xl6_future_continue* pContinue = (xl6_future_continue*)pValue;

	(void)pData;
	if ( pContinue->Pending ) {
		__xl6FutureMetaPending(pContinue->Source, -1);
	}
	if ( pContinue->FreeArg != NULL ) {
		pContinue->FreeArg(pContinue->Arg);
	}
	xrtFutureDestroy(pContinue->Source);
	xrtFree(pContinue);
}



/* 创建一个统一的私有延续上下文。 */
static xl6_future_continue* __xl6FutureContinueCreate(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	xl6_future_finally_proc pFinally,
	void* pArg,
	xl6_future_arg_free_proc pFreeArg
)
{
	xl6_future_continue* pContinue;

	if ( (pFuture == NULL) ||
		((pProc == NULL) && (pFinally == NULL)) ) {
		return NULL;
	}
	pContinue = (xl6_future_continue*)xrtCalloc(
		1,
		sizeof(xl6_future_continue)
	);
	if ( pContinue == NULL ) {
		return NULL;
	}
	pContinue->Source = xrtFutureRef(pFuture);
	if ( pContinue->Source == NULL ) {
		xrtFree(pContinue);
		return NULL;
	}
	pContinue->Proc = pProc;
	pContinue->Finally = pFinally;
	pContinue->Arg = pArg;
	pContinue->FreeArg = pFreeArg;
	pContinue->Pending = true;
	__xl6FutureMetaPending(pFuture, 1);
	return pContinue;
}



/* 只在成功终态执行私有延续。 */
XL_API xfuture* xl6FutureThen(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg
)
{
	return xl6FutureThenOwned(pFuture, pProc, pArg, NULL);
}



/* 只在成功终态执行并接管参数的私有延续。 */
XL_API xfuture* xl6FutureThenOwned(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg,
	xl6_future_arg_free_proc pFreeArg
)
{
	xl6_future_continue* pContinue = __xl6FutureContinueCreate(
		pFuture,
		pProc,
		NULL,
		pArg,
		pFreeArg
	);
	xfuture* pResult;

	if ( pContinue == NULL ) {
		return NULL;
	}
	pResult = xrtFutureThenOwned(
		pFuture,
		__xl6FutureContinueRun,
		pContinue,
		__xl6FutureContinueFree,
		NULL
	);
	if ( pResult == NULL ) {
		pContinue->FreeArg = NULL;
		__xl6FutureContinueFree(pContinue, NULL);
		return NULL;
	}
	return __xl6FutureTrackNew(pResult);
}



/* 只在失败终态执行并接管参数的私有延续。 */
XL_API xfuture* xl6FutureCatchOwned(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg,
	xl6_future_arg_free_proc pFreeArg
)
{
	xl6_future_continue* pContinue = __xl6FutureContinueCreate(
		pFuture,
		pProc,
		NULL,
		pArg,
		pFreeArg
	);
	xfuture* pResult;

	if ( pContinue == NULL ) {
		return NULL;
	}
	pResult = xrtFutureCatchOwned(
		pFuture,
		__xl6FutureContinueRun,
		pContinue,
		__xl6FutureContinueFree,
		NULL
	);
	if ( pResult == NULL ) {
		pContinue->FreeArg = NULL;
		__xl6FutureContinueFree(pContinue, NULL);
		return NULL;
	}
	return __xl6FutureTrackNew(pResult);
}



/* 对任意终态执行并接管参数的私有延续。 */
XL_API xfuture* xl6FutureContinueOwned(
	xfuture* pFuture,
	xl6_future_continue_proc pProc,
	void* pArg,
	xl6_future_arg_free_proc pFreeArg
)
{
	xl6_future_continue* pContinue = __xl6FutureContinueCreate(
		pFuture,
		pProc,
		NULL,
		pArg,
		pFreeArg
	);
	xfuture* pResult;

	if ( pContinue == NULL ) {
		return NULL;
	}
	pResult = xrtFutureContinueOwned(
		pFuture,
		__xl6FutureContinueRun,
		pContinue,
		__xl6FutureContinueFree,
		NULL
	);
	if ( pResult == NULL ) {
		pContinue->FreeArg = NULL;
		__xl6FutureContinueFree(pContinue, NULL);
		return NULL;
	}
	return __xl6FutureTrackNew(pResult);
}



/* 观察任意终态并透传原结果。 */
XL_API xfuture* xl6FutureFinally(
	xfuture* pFuture,
	xl6_future_finally_proc pProc,
	void* pArg
)
{
	xl6_future_continue* pContinue = __xl6FutureContinueCreate(
		pFuture,
		NULL,
		pProc,
		pArg,
		NULL
	);
	xfuture* pResult;

	if ( pContinue == NULL ) {
		return NULL;
	}
	pResult = xrtFutureFinallyOwned(
		pFuture,
		__xl6FutureFinallyRun,
		pContinue,
		__xl6FutureContinueFree,
		NULL
	);
	if ( pResult == NULL ) {
		__xl6FutureContinueFree(pContinue, NULL);
		return NULL;
	}
	return __xl6FutureTrackNew(pResult);
}
