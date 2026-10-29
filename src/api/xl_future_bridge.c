#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "include/xruntime_decl.h"

#include "xl_future_bridge.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_future_aggregate.h"
#include "src/runtime/xl6_future_metadata.h"



/* Native duration conversion; observation is owned by the physical Future. */
static uint64 __xl6FutureTimeout(long long milliseconds)
{
    if (milliseconds <= 0) return 0;
    return (unsigned long long)milliseconds > UINT64_MAX / UINT64_C(1000)
        ? UINT64_MAX : (uint64)milliseconds * UINT64_C(1000);
}
static xdeadline __xl6FutureDeadline(long long milliseconds)
{ return (xdeadline)__xl6FutureTimeout(milliseconds); }

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
XL_API xfuture* xl6FutureRef(xfuture* future)
{ return xrtFutureRef(future); }
XL_API void xl6FutureDestroy(xfuture* future)
{ xrtFutureDestroy(future); }

/* 永久等待 Future 进入终态。 */
XL_API bool xl6FutureWait(xfuture* pFuture)
{
	bool bResult = xrtFutureWait(pFuture) == XWAIT_OK;

	return bResult && (xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 在语言层毫秒超时内等待 Future。 */
XL_API bool xl6FutureWaitFor(xfuture* pFuture, long long iTimeoutMs)
{
	bool bResult = xrtFutureWaitFor(
		pFuture,
		__xl6FutureTimeout(iTimeoutMs)
	) == XWAIT_OK;

	return bResult && (xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* 等待 Future 到语言层单调毫秒截止时间。 */
XL_API bool xl6FutureWaitUntil(xfuture* pFuture, long long iDeadlineMs)
{
	bool bResult = xrtFutureWaitUntil(
		pFuture,
		__xl6FutureDeadline(iDeadlineMs)
	) == XWAIT_OK;

	return bResult && (xrtFutureState(pFuture) == XFUTURE_RESOLVED);
}



/* Legacy C access uses the same physical native metadata. */
XL_API bool xl6FutureSetName(xfuture* future, const char* name)
{
    if (!name) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return xrtFutureDebugSetNameN(future,name,strlen(name));
}
XL_API unsigned long long xl6FutureCreatedMs(xfuture* future)
{ return (unsigned long long)xl6FutureFirstObservedMsV2(future); }
XL_API unsigned long long xl6FutureCompletedMs(xfuture* future)
{ return (unsigned long long)xl6FutureFirstTerminalObservedMsV2(future); }
XL_API int xl6FuturePendingCount(xfuture* future)
{
    int64_t count=xl6FuturePendingWatchesV2(future);
    if (count > INT_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int)count;
}

/* 返回与 xlang Future.State 数值一致的状态。 */
XL_API int xl6FutureState(xfuture* pFuture)
{
	int iState = (int)xrtFutureState(pFuture);

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
XL_API bool xl6FutureCancel(xfuture* future)
{
    /* Only the actual producer publishes cancellation after accepted work. */
    return xrtFutureCancel(future);
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



/* 等待任意输入 Future 完成。 */
XL_API xfuture* xl6FutureAny(xfuture** pFutures, int iCount)
{
	return iCount > 0 ? xrtFutureAny(pFutures, (size_t)iCount) : NULL;
}



/* 等待全部输入 Future 完成。 */
XL_API xfuture* xl6FutureAll(xfuture** pFutures, int iCount)
{
	return iCount >= 0 ? xrtFutureAll(pFutures, (size_t)iCount) : NULL;
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
	return pResult;
}



/* One physical mapper context; native groups own the only source slots.
 * New XLIR calls carry explicit code ownership. The old bare-boxer entry is
 * still supported, but cannot acquire collection authority by compatibility. */
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



/* 延续上下文统一管理用户参数与源拥有引用。 */
typedef struct xl6_future_continue {
	xfuture* Source;
	xl6_future_continue_proc Proc;
	xl6_future_finally_proc Finally;
	void* Arg;
	xl6_future_arg_free_proc FreeArg;
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

	__xl6FutureInput(pInput, &tInput);
	pContinue->Finally(&tInput, pContinue->Arg);
}



/* 延续结束、跳过或创建失败后统一释放上下文。 */
static void __xl6FutureContinueFree(void* pValue, void* pData)
{
	xl6_future_continue* pContinue = (xl6_future_continue*)pValue;

	(void)pData;
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
	return pResult;
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
	return pResult;
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
	return pResult;
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
	return pResult;
}
