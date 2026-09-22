#include "../internal/xrt_http_client_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)

/* Client 关闭等待节点独立持有 Client 和 Promise。 */
struct __xrt_http_client_wait {
	__xrt_http_client_wait* Next;
	xhttpclient* Client;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};



/* 从 Client 等待链表中移除指定节点；调用方持有生命周期锁。 */
static bool __xrtHttpClientWaitRemove(
	xhttpclient* pClient,
	__xrt_http_client_wait* pWaiter
)
{
	__xrt_http_client_wait** ppCurrent =
		&pClient->CloseWaitHead;
	__xrt_http_client_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) &&
		(*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pClient->CloseWaitTail == pWaiter ) {
		pClient->CloseWaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	return true;
}



/* 完成一个已摘除的 Client 关闭等待并释放全部持有资源。 */
static void __xrtHttpClientWaitFinish(
	__xrt_http_client_wait* pWaiter,
	bool bCancelled
)
{
	xpromise* pPromise = pWaiter->Promise;
	xhttpclient* pClient = pWaiter->Client;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	__xrtHttpClientRelease(pClient);
	xrtFree(pWaiter);
	if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseResolve(pPromise, NULL);
	}
	xrtPromiseDestroy(pPromise);
}



/* 取消只摘除当前关闭等待，不排空或中止 Client。 */
static void __xrtHttpClientWaitCancel(ptr pData)
{
	__xrt_http_client_wait* pWaiter =
		(__xrt_http_client_wait*)pData;
	xhttpclient* pClient = pWaiter->Client;
	bool bRemoved;

	xrtSpinLock(&pClient->LifecycleLock);
	bRemoved = pWaiter->Linked &&
		__xrtHttpClientWaitRemove(pClient, pWaiter);
	xrtSpinUnlock(&pClient->LifecycleLock);
	if ( bRemoved ) {
		__xrtHttpClientWaitFinish(pWaiter, true);
	}
}



/* 摘除并成功完成 Client 的全部关闭等待。 */
void __xrtHttpClientFutureFinish(xhttpclient* pClient)
{
	__xrt_http_client_wait* pWaiter;

	if ( pClient == NULL ) {
		return;
	}
	xrtSpinLock(&pClient->LifecycleLock);
	pWaiter = pClient->CloseWaitHead;
	pClient->CloseWaitHead = NULL;
	pClient->CloseWaitTail = NULL;
	for ( __xrt_http_client_wait* pCurrent = pWaiter;
		pCurrent != NULL;
		pCurrent = pCurrent->Next ) {
		pCurrent->Linked = false;
	}
	xrtSpinUnlock(&pClient->LifecycleLock);
	while ( pWaiter != NULL ) {
		__xrt_http_client_wait* pNext = pWaiter->Next;

		pWaiter->Next = NULL;
		__xrtHttpClientWaitFinish(pWaiter, false);
		pWaiter = pNext;
	}
}



/* 建立一个可独立取消的 Client 关闭 Future。 */
XRT_API xfuture* xrtHttpClientWaitAsync(xhttpclient* pClient)
{
	__xrt_http_client_wait* pWaiter;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	bool bClosed;

	if ( pClient == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"wait-http-client",
			"HTTP client is null",
			NULL
		);
		return NULL;
	}
	pWaiter = (__xrt_http_client_wait*)xrtCalloc(
		1,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Client = __xrtHttpClientHold(pClient);
	if ( pWaiter->Client == NULL ) {
		xrtFree(pWaiter);
		return NULL;
	}
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		__xrtHttpClientRelease(pWaiter->Client);
		xrtFree(pWaiter);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtHttpClientWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		__xrtHttpClientRelease(pWaiter->Client);
		xrtFree(pWaiter);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	xrtSpinLock(&pClient->LifecycleLock);
	bClosed = xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	) == XHTTP_CLIENT_CLOSED;
	if ( !bClosed ) {
		pWaiter->Linked = true;
		if ( pClient->CloseWaitTail != NULL ) {
			pClient->CloseWaitTail->Next = pWaiter;
		} else {
			pClient->CloseWaitHead = pWaiter;
		}
		pClient->CloseWaitTail = pWaiter;
	}
	xrtSpinUnlock(&pClient->LifecycleLock);
	if ( bClosed ) {
		__xrtHttpClientWaitFinish(pWaiter, false);
	}
	return pFuture;
}



/* Future 成功值独立拥有响应、Upgrade 传输和只读元数据。 */
struct xhttpresult {
	volatile int32 References;
	xatomicptr Response;
	xatomicptr Tcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xatomicptr Tls;
	#endif
	xhttpcallinfo Info;
	size_t Buffered;
	bool Upgraded;
};



/*
	桥接上下文先服务于 Call，成功完成后原位转为 Future 拥有的结果。
	Result 必须保持首成员，结果析构才能直接释放整块内存。
*/
typedef struct xrt_http_client_future {
	xhttpresult Result;
	xfuturebridge Bridge;
	xhttpcall* Call;
} xrt_http_client_future;



/* 中止并释放一个未被调用方取走的明文 Upgrade 传输。 */
static void __xrtHttpResultTcpDestroy(xnetstream* pStream)
{
	if ( pStream == NULL ) {
		return;
	}
	(void)xrtNetStreamAbort(pStream);
	xrtNetStreamDestroy(pStream);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 中止并释放一个未被调用方取走的 TLS Upgrade 传输。 */
static void __xrtHttpResultTlsDestroy(xtlsstream* pStream)
{
	if ( pStream == NULL ) {
		return;
	}
	(void)xrtTlsStreamAbort(pStream);
	xrtTlsStreamDestroy(pStream);
}

#endif



/* 增加结果引用并返回原指针。 */
XRT_API xhttpresult* xrtHttpResultRef(xhttpresult* pResult)
{
	if ( (pResult == NULL) ||
		(xrtRefRetain(&pResult->References) < 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return pResult;
}



/* 释放最后一个结果引用及其尚未取走的所有权。 */
XRT_API void xrtHttpResultDestroy(xhttpresult* pResult)
{
	xhttpresponse* pResponse;
	xnetstream* pTcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls;
	#endif

	if ( (pResult == NULL) ||
		(xrtRefRelease(&pResult->References) != 0) ) {
		return;
	}
	pResponse = (xhttpresponse*)xrtAtomicPtrExchange(
		&pResult->Response,
		NULL,
		XMEMORY_ACQ_REL
	);
	pTcp = (xnetstream*)xrtAtomicPtrExchange(
		&pResult->Tcp,
		NULL,
		XMEMORY_ACQ_REL
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		pTls = (xtlsstream*)xrtAtomicPtrExchange(
			&pResult->Tls,
			NULL,
			XMEMORY_ACQ_REL
		);
	#endif
	xrtHttpResponseDestroy(pResponse);
	__xrtHttpResultTcpDestroy(pTcp);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpResultTlsDestroy(pTls);
	#endif
	memset(pResult, 0, sizeof(*pResult));
	xrtFree(pResult);
}



/* 返回结果借用的响应。 */
XRT_API const xhttpresponse* xrtHttpResultResponse(
	const xhttpresult* pResult
)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (const xhttpresponse*)xrtAtomicPtrLoad(
		&pResult->Response,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的响应。 */
XRT_API xhttpresponse* xrtHttpResultTakeResponse(
	xhttpresult* pResult
)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xhttpresponse*)xrtAtomicPtrExchange(
		&pResult->Response,
		NULL,
		XMEMORY_ACQ_REL
	);
}



/* 返回结果借用的明文 Upgrade Stream。 */
XRT_API xnetstream* xrtHttpResultTcp(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xnetstream*)xrtAtomicPtrLoad(
		&pResult->Tcp,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的明文 Upgrade Stream。 */
XRT_API xnetstream* xrtHttpResultTakeTcp(xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xnetstream*)xrtAtomicPtrExchange(
		&pResult->Tcp,
		NULL,
		XMEMORY_ACQ_REL
	);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 返回结果借用的 TLS Upgrade Stream。 */
XRT_API xtlsstream* xrtHttpResultTls(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrLoad(
		&pResult->Tls,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的 TLS Upgrade Stream。 */
XRT_API xtlsstream* xrtHttpResultTakeTls(xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrExchange(
		&pResult->Tls,
		NULL,
		XMEMORY_ACQ_REL
	);
}

#endif



/* 返回 Upgrade 后已经缓冲的协议外字节数。 */
XRT_API size_t xrtHttpResultBuffered(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	return pResult->Buffered;
}



/* 返回已经跟随的重定向次数。 */
XRT_API size_t xrtHttpResultRedirects(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	return pResult->Info.Redirects;
}



/* 判断结果是否已经交付 Upgrade 传输。 */
XRT_API bool xrtHttpResultUpgraded(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return pResult->Upgraded;
}



/* 复制成功结果携带的不可变 Call 诊断快照。 */
XRT_API bool xrtHttpResultInfo(
	const xhttpresult* pResult,
	xhttpcallinfo* pInfo
)
{
	if ( (pResult == NULL) ||
		!xrtMemRangeValid(pInfo, sizeof(pResult->Info)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pInfo, &pResult->Info, sizeof(pResult->Info));
	return true;
}



/* Future 拥有值的析构过程只释放其一个结果引用。 */
static void __xrtHttpFutureResultFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtHttpResultDestroy((xhttpresult*)pValue);
}



/* 把 Future 的协作取消请求转发给完整 HTTP Call。 */
static void __xrtHttpFutureCancel(ptr pData)
{
	xrt_http_client_future* pContext =
		(xrt_http_client_future*)pData;

	(void)xrtHttpCallCancel(pContext->Call);
}



/*
	等待提交线程安装取消观察器。
	这只覆盖回调与非 Worker 提交线程并发完成的极短窗口。
*/
/* 把成功回调转为 Future 拥有的完整结果。 */
static void __xrtHttpFutureResolve(
	xrt_http_client_future* pContext,
	const xhttpcallresult* pResult
)
{
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xhttpcall* pCall = pContext->Call;
	bool bResolved;

	xrtAtomicPtrStore(
		&pContext->Result.Response,
		pResult->Response,
		XMEMORY_RELEASE
	);
	xrtAtomicPtrStore(
		&pContext->Result.Tcp,
		pResult->Tcp,
		XMEMORY_RELEASE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtAtomicPtrStore(
			&pContext->Result.Tls,
			pResult->Tls,
			XMEMORY_RELEASE
		);
	#endif
	pContext->Result.Info = pResult->Info;
	pContext->Result.Buffered = pResult->Buffered;
	pContext->Result.Upgraded = pResult->Upgraded;
	xrtHttpCallDestroy(pCall);
	bResolved = xrtPromiseResolveOwned(
		pPromise,
		&pContext->Result,
		__xrtHttpFutureResultFree,
		NULL
	);
	xrtPromiseDestroy(pPromise);
	if ( !bResolved ) {
		xrtHttpResultDestroy(&pContext->Result);
	}
}



/* 把失败或取消回调转为对应 Future 终态并释放桥接上下文。 */
static void __xrtHttpFutureReject(
	xrt_http_client_future* pContext,
	const xhttpcallresult* pResult
)
{
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xhttpcall* pCall = pContext->Call;
	xerror* pError = xrtErrorRef(pResult->Error);
	xnetresult Result = pResult->Result;

	xrtHttpResponseDestroy(pResult->Response);
	__xrtHttpResultTcpDestroy(pResult->Tcp);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpResultTlsDestroy(pResult->Tls);
	#endif
	xrtHttpCallDestroy(pCall);
	xrtFree(pContext);

	if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else if ( pError != NULL ) {
		(void)xrtPromiseReject(pPromise, pError);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
}



/* 回收畸形成功结果中的全部所有权，并把契约破坏转为结构化内部错误。 */
static void __xrtHttpFutureRejectInvalidSuccess(
	xrt_http_client_future* pContext,
	const xhttpcallresult* pResult
)
{
	xhttpcallresult Reject = *pResult;
	xerror* pError;

	xrtHttpResponseDestroy(Reject.Response);
	__xrtHttpResultTcpDestroy(Reject.Tcp);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpResultTlsDestroy(Reject.Tls);
		Reject.Tls = NULL;
	#endif
	Reject.Result = XNET_RESULT_ERROR;
	Reject.Response = NULL;
	Reject.Tcp = NULL;
	pError = __xrtHttpClientErrorCreate(
		XERR_INTERNAL,
		XHTTP_CLIENT_ERROR_INTERNAL,
		"complete-http-future",
		"HTTP call reported success without a response",
		NULL
	);
	Reject.Error = pError != NULL ? pError : xrtGetError();
	__xrtHttpFutureReject(pContext, &Reject);
	if ( pError != NULL ) {
		xrtErrorFree(pError);
	}
}



/* 接收 Call 唯一终态并完成 Future。 */
static void __xrtHttpFutureDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	xrt_http_client_future* pContext =
		(xrt_http_client_future*)pData;
	bool bReady;

	(void)pCall;
	bReady = xrtFutureBridgeWait(&pContext->Bridge);
	xrtFutureBridgeUnwatch(&pContext->Bridge);
	if ( !bReady ) {
		xrtHttpResponseDestroy(pResult->Response);
		__xrtHttpResultTcpDestroy(pResult->Tcp);
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			__xrtHttpResultTlsDestroy(pResult->Tls);
		#endif
		xrtHttpCallDestroy(pContext->Call);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return;
	}
	if ( (pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) ) {
		__xrtHttpFutureResolve(pContext, pResult);
	} else if ( pResult->Result == XNET_RESULT_OK ) {
		__xrtHttpFutureRejectInvalidSuccess(pContext, pResult);
	} else {
		__xrtHttpFutureReject(pContext, pResult);
	}
}



/* 创建 Promise、Call 和双向取消桥。 */
XRT_API xfuture* xrtHttpClientDoAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
)
{
	xhttpcalloptions Options;
	xrt_http_client_future* pContext;
	const xhttpcalloptions* pCallOptions = NULL;
	xcancel* pParent = NULL;
	xfuture* pFuture;
	xerror* pError;

	if ( pOptions != NULL ) {
		if ( !xrtMemRangeValid(pOptions, sizeof(Options)) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				"run-http-future",
				"HTTP call options range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Options, pOptions, sizeof(Options));
		pCallOptions = &Options;
		pParent = Options.Cancel;
	}
	pContext = (xrt_http_client_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Result.References = 1;
	xrtAtomicPtrInit(&pContext->Result.Response, NULL);
	xrtAtomicPtrInit(&pContext->Result.Tcp, NULL);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtAtomicPtrInit(&pContext->Result.Tls, NULL);
	#endif
	pFuture = xrtFutureBridgeCreate(
		&pContext->Bridge,
		pParent
	);
	if ( pFuture == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	pContext->Call = xrtHttpClientDo(
		pClient,
		pRequest,
		pCallOptions,
		__xrtHttpFutureDone,
		pContext
	);
	if ( pContext->Call == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return NULL;
	}
	if ( !xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtHttpFutureCancel,
		pContext
	) ) {
		pError = xrtTakeError();
		(void)xrtFutureBridgeFail(&pContext->Bridge);
		(void)xrtHttpCallCancel(pContext->Call);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	(void)xrtFutureBridgeReady(&pContext->Bridge);
	return pFuture;
}



/* 设置同步等待层自己的稳定 HTTP Client 错误。 */
static void __xrtHttpSyncError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sMessage
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		"wait-http-call",
		sMessage,
		NULL
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 在宿主线程阻塞等待同一 Future 契约，并保留一个结果引用。 */
XRT_API xhttpresult* xrtHttpClientDoSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
)
{
	xfuturestate State;
	xfuture* pFuture;
	xhttpresult* pResult = NULL;

	if ( pClient == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( xrtNetEngineCurrent(pClient->Engine) != NULL ) {
		__xrtHttpSyncError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_INTERNAL,
			"network Worker cannot block on its own HTTP call"
		);
		return NULL;
	}
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		pOptions
	);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
		xerror* pError = xrtTakeError();

		(void)xrtFutureCancel(pFuture);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	State = xrtFutureState(pFuture);
	if ( State == XFUTURE_RESOLVED ) {
		pResult = xrtHttpResultRef(
			(xhttpresult*)xrtFutureValue(pFuture)
		);
	} else if ( State == XFUTURE_FAILED ) {
		(void)xrtFutureValue(pFuture);
	} else if ( State == XFUTURE_CANCELLED ) {
		__xrtHttpSyncError(
			XERR_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			"HTTP call was cancelled while waiting"
		);
	} else {
		__xrtHttpSyncError(
			XERR_INTERNAL,
			XHTTP_CLIENT_ERROR_INTERNAL,
			"HTTP Future completed without a usable result"
		);
	}
	xrtFutureDestroy(pFuture);
	return pResult;
}

#endif
