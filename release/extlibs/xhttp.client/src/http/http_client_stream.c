#include "../internal/xrt_http_client_stream.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM)

#define XRT_HTTP1_CALL_WRITE_DEFAULT 16384u



/* 在当前驱动退出后处理可能同步到达的传输输入终点。 */
static void __xrtHttp1CallEnd(xhttp1call* pCall);



/* 建立调用层结构化错误，并保留明确的下层原因。 */
static xerror* __xrtHttp1CallErrorCreate(
	xerrkind Kind,
	xhttp1callerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.call";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 建立稳定的取消错误；分配失败时退回核心静态取消错误。 */
static xerror* __xrtHttp1CallCancelledError(
	const xerror* pCause
)
{
	xerror* pError = __xrtHttp1CallErrorCreate(
		XERR_CANCELLED,
		XHTTP1_CALL_ERROR_CANCELLED,
		"cancel-http1-call",
		"HTTP/1 call was cancelled",
		pCause
	);

	if ( (pError != NULL) &&
		(xrtErrorKind(pError) == XERR_CANCELLED) ) {
		return pError;
	}
	xrtErrorFree(pError);
	xrtSetErrorKind(XERR_CANCELLED);
	return xrtErrorRef(xrtGetError());
}



/* 设置创建阶段当前线程错误。 */
static void __xrtHttp1CallSetError(
	xerrkind Kind,
	xhttp1callerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttp1CallErrorCreate(
		Kind, Code, sOperation, sMessage, pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 验证调用配置不会造成零进展输出。 */
bool __xrtHttp1CallConfigValid(
	const xhttp1callconfig* pConfig
)
{
	if ( (pConfig == NULL) || (pConfig->WriteSize == 0) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"configure-http1-call",
			"HTTP/1 call write size must be non-zero",
			NULL
		);
		return false;
	}
	return true;
}



/* 增加调用引用。 */
XRT_API xhttp1call* xrtHttp1CallRef(xhttp1call* pCall)
{
	if ( (pCall == NULL) ||
		(xrtRefRetain(&pCall->References) < 0) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"retain-http1-call",
			"HTTP/1 call is null or already released",
			NULL
		);
		return NULL;
	}
	return pCall;
}



/* 释放调用尚未转移的传输引用。 */
static void __xrtHttp1CallStreamDestroy(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		xrtNetStreamDestroy((xnetstream*)pStream);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		xrtTlsStreamDestroy((xtlsstream*)pStream);
	#else
		(void)pCall;
	#endif
}



/* 在所有权锁内取得传输独立引用，供跨线程路径在锁外使用。 */
static ptr __xrtHttp1CallStreamRef(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return NULL;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamRef((xnetstream*)pStream);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamRef((xtlsstream*)pStream);
	#else
		(void)pCall;
		return NULL;
	#endif
}



/* 在所有权锁内摘除调用持有的传输引用。 */
static ptr __xrtHttp1CallStreamTake(xhttp1call* pCall)
{
	ptr pStream;

	(void)xrtSpinLock(&pCall->Lock);
	pStream = xrtAtomicPtrExchange(
		&pCall->Stream,
		NULL,
		XMEMORY_ACQ_REL
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	return pStream;
}



/* 异常关闭调用尚未转移的传输。 */
static void __xrtHttp1CallStreamAbort(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		(void)xrtNetStreamAbort((xnetstream*)pStream);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		(void)xrtTlsStreamAbort((xtlsstream*)pStream);
	#else
		(void)pCall;
	#endif
}



/* 正常关闭不再复用的传输。 */
static void __xrtHttp1CallStreamClose(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		(void)xrtNetStreamClose((xnetstream*)pStream);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		(void)xrtTlsStreamClose((xtlsstream*)pStream);
	#else
		(void)pCall;
	#endif
}



/* 返回 Call 当前传输实际使用的 TCP 读门。 */
static xnetstream* __xrtHttp1CallTcpTransport(
	const xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return NULL;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return (xnetstream*)pStream;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamTransport((xtlsstream*)pStream);
	#else
		(void)pCall;
		return NULL;
	#endif
}



/* 立即关闭底层 TCP 读门；TLS 已解密明文继续由 TLS 自身限额约束。 */
static void __xrtHttp1CallPauseTransport(
	const xhttp1call* pCall,
	ptr pStream
)
{
	xnetstream* pTransport = __xrtHttp1CallTcpTransport(
		pCall, pStream
	);

	if ( pTransport != NULL ) {
		xrtNetStreamPause(pTransport);
	}
}



/* 恢复实际 TCP 读门；已有缓冲总是先由 Call 驱动消费。 */
static bool __xrtHttp1CallResumeTransport(
	const xhttp1call* pCall,
	ptr pStream
)
{
	xnetstream* pTransport = __xrtHttp1CallTcpTransport(
		pCall, pStream
	);

	return (pTransport != NULL) && xrtNetStreamResume(pTransport);
}



/* 释放最后一个调用引用和全部尚未交出的状态。 */
XRT_API void xrtHttp1CallDestroy(xhttp1call* pCall)
{
	ptr pStream;

	if ( (pCall == NULL) ||
		(xrtRefRelease(&pCall->References) != 0) ) {
		return;
	}
	pStream = __xrtHttp1CallStreamTake(pCall);
	__xrtHttp1CallStreamDestroy(pCall, pStream);
	xrtHttp1ExchangeDestroy(pCall->Exchange);
	xrtErrorFree(pCall->Error);
	(void)xrtSpinUnit(&pCall->Lock);
	memset(pCall, 0, sizeof(*pCall));
	xrtFree(pCall);
}



/* 终态若没有 TCP 输出租约在途，立即释放 Exchange。 */
static void __xrtHttp1CallExchangeUnit(xhttp1call* pCall)
{
	if ( !pCall->OutputQueued && (pCall->Exchange != NULL) ) {
		xrtHttp1ExchangeDestroy(pCall->Exchange);
		pCall->Exchange = NULL;
	}
}



/* 返回传输当前仍未交给 Exchange 的字节数。 */
static size_t __xrtHttp1CallBuffered(
	const xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return 0;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamAvailable(
			(const xnetstream*)pStream
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamAvailable(
			(const xtlsstream*)pStream
		);
	#else
		return 0;
	#endif
}



/* 判断普通 TCP 两个方向和接收缓冲都仍可承载下一次 HTTP/1 事务。 */
bool __xrtHttp1TcpReusable(const xnetstream* pStream)
{
	xnetstreamstats Stats;

	if ( (pStream == NULL) ||
		!xrtNetStreamStats(pStream, &Stats) ) {
		return false;
	}
	return (Stats.State == XNET_STREAM_OPEN) &&
		!Stats.ReadEnded &&
		!Stats.WriteEnded &&
		(Stats.BufferedBytes == 0);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 判断 TLS 会话、明文缓冲和底层 TCP 都仍可承载下一次 HTTP/1 事务。 */
bool __xrtHttp1TlsReusable(xtlsstream* pStream)
{
	xtlssession* pSession;
	xnetstream* pTransport;

	if ( (pStream == NULL) ||
		(xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN) ||
		(xrtTlsStreamAvailable(pStream) != 0) ) {
		return false;
	}
	pSession = xrtTlsStreamSession(pStream);
	pTransport = xrtTlsStreamTransport(pStream);
	return (pSession != NULL) &&
		(xrtTlsSessionState(pSession) == XTLS_STATE_READY) &&
		__xrtHttp1TcpReusable(pTransport);
}

#endif



/* 在所属 Worker 上摘除调用事件，后续传输不再引用调用对象。 */
static bool __xrtHttp1CallDetach(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamSetEvents(
			(xnetstream*)pStream,
			NULL,
			NULL
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamSetEvents(
			(xtlsstream*)pStream,
			NULL,
			NULL
		);
	#else
		return false;
	#endif
}



/* 在所有权锁内封闭终态，并清除已经不可能恢复的输入门。 */
static void __xrtHttp1CallSeal(xhttp1call* pCall)
{
	xrtAtomic32Store(
		&pCall->FinishGate,
		1,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
}



/* 发布失败或取消终态；调用方保证当前 Stream 不会再回调本对象。 */
static void __xrtHttp1CallFinishFailure(
	xhttp1call* pCall,
	xnetresult Result,
	const xerror* pCause
)
{
	xhttp1callresult CallResult;
	xerror* pCancelledError;
	xerror* pPreviousError;
	ptr pStream;
	bool bCancelled;

	/*
		取消接纳与终态提交共用一条线性化边界。
		提交后 Cancel 必须返回 false，提交前接纳的取消必须决定最终结果。
	*/
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return;
	}
	bCancelled = xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) != 0;
	__xrtHttp1CallSeal(pCall);
	(void)xrtSpinUnlock(&pCall->Lock);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
		__xrtHttp1CallAsyncStop(pCall);
	#endif
	if ( !bCancelled && (Result == XNET_RESULT_OK) ) {
		Result = XNET_RESULT_ERROR;
	}
	if ( bCancelled &&
		((pCall->Error == NULL) ||
		 (xrtErrorKind(pCall->Error) != XERR_CANCELLED)) ) {
		pPreviousError = pCall->Error;
		pCancelledError = __xrtHttp1CallCancelledError(
			pPreviousError != NULL ?
				pPreviousError :
				pCause
		);
		if ( pCancelledError != NULL ) {
			pCall->Error = pCancelledError;
			xrtErrorFree(pPreviousError);
		}
	} else if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			XERR_IO,
			XHTTP1_CALL_ERROR_TRANSPORT,
			"run-http1-call",
			"HTTP/1 call transport failed",
			pCause
		);
	}
	Result = bCancelled ? XNET_RESULT_CANCELLED : Result;
	xrtAtomic32Store(
		&pCall->State,
		bCancelled ?
			XHTTP1_CALL_CANCELLED :
			XHTTP1_CALL_FAILED,
		XMEMORY_RELEASE
	);
	pStream = __xrtHttp1CallStreamTake(pCall);
	__xrtHttp1CallExchangeUnit(pCall);
	memset(&CallResult, 0, sizeof(CallResult));
	CallResult.Result = Result;
	CallResult.Error = pCall->Error;
	pCall->Events.Done(
		pCall,
		&CallResult,
		pCall->Events.Data
	);
	__xrtHttp1CallStreamDestroy(pCall, pStream);
	if ( pCall->RuntimeHeld ) {
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
	}
}



/* 摘除事件、异常关闭传输并发布调用层原因。 */
void __xrtHttp1CallFail(
	xhttp1call* pCall,
	xhttp1callerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	ptr pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);

	if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			Kind,
			Code,
			sOperation,
			sMessage,
			pCause
		);
	}
	if ( (pStream != NULL) &&
		__xrtHttp1CallDetach(pCall, pStream) ) {
		pStream = __xrtHttp1CallStreamTake(pCall);
		__xrtHttp1CallStreamAbort(pCall, pStream);
		__xrtHttp1CallStreamDestroy(pCall, pStream);
		__xrtHttp1CallFinishFailure(
			pCall,
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ? XNET_RESULT_CANCELLED : XNET_RESULT_ERROR,
			pCall->Error
		);
		return;
	}
	__xrtHttp1CallStreamAbort(pCall, pStream);
}



/* 发布成功响应，并归还可复用或已升级的传输。 */
static void __xrtHttp1CallFinishSuccess(xhttp1call* pCall)
{
	xhttp1callresult CallResult;
	xhttpresponse* pResponse;
	ptr pStream;
	size_t iBuffered;
	bool bUpgraded;
	bool bReusable;
	bool bTransfer;

	pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);
	pResponse = xrtHttp1ExchangeTakeResponse(
		pCall->Exchange
	);
	if ( pResponse == NULL ) {
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_EXCHANGE,
			XERR_INTERNAL,
			"complete-http1-call",
			"HTTP/1 Exchange completed without a response",
			xrtGetError()
		);
		return;
	}
	bUpgraded = xrtHttp1ExchangeUpgraded(pCall->Exchange);
	iBuffered = __xrtHttp1CallBuffered(pCall, pStream);
	bReusable = !bUpgraded &&
		(iBuffered == 0) &&
		xrtHttp1ExchangeReusable(pCall->Exchange) &&
		(pCall->Transport == XRT_HTTP1_CALL_TCP ?
			__xrtHttp1TcpReusable(
				(const xnetstream*)pStream
			) :
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
				__xrtHttp1TlsReusable(
					(xtlsstream*)pStream
				)
			#else
				false
			#endif
		);
	if ( (pStream == NULL) ||
		!__xrtHttp1CallDetach(pCall, pStream) ) {
		xrtHttpResponseDestroy(pResponse);
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_STATE,
			XERR_STATE,
			"detach-http1-call",
			"HTTP/1 call could not detach its completed transport",
			xrtGetError()
		);
		return;
	}
	pStream = __xrtHttp1CallStreamTake(pCall);
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		xrtHttpResponseDestroy(pResponse);
		__xrtHttp1CallStreamAbort(pCall, pStream);
		__xrtHttp1CallStreamDestroy(pCall, pStream);
		__xrtHttp1CallFinishFailure(
			pCall,
			XNET_RESULT_CANCELLED,
			NULL
		);
		return;
	}
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		xrtHttpResponseDestroy(pResponse);
		__xrtHttp1CallStreamAbort(pCall, pStream);
		__xrtHttp1CallStreamDestroy(pCall, pStream);
		return;
	}
	__xrtHttp1CallSeal(pCall);
	(void)xrtSpinUnlock(&pCall->Lock);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
		__xrtHttp1CallAsyncStop(pCall);
	#endif
	xrtAtomic32Store(
		&pCall->State,
		XHTTP1_CALL_SUCCEEDED,
		XMEMORY_RELEASE
	);
	bTransfer = bReusable || bUpgraded;
	if ( !bTransfer ) {
		__xrtHttp1CallStreamClose(pCall, pStream);
	}
	memset(&CallResult, 0, sizeof(CallResult));
	CallResult.Result = XNET_RESULT_OK;
	CallResult.Response = pResponse;
	CallResult.Buffered = iBuffered;
	CallResult.Reusable = bReusable;
	CallResult.Upgraded = bUpgraded;
	if ( bTransfer && (pCall->Transport == XRT_HTTP1_CALL_TCP) ) {
		CallResult.Tcp = (xnetstream*)pStream;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		if ( bTransfer &&
			(pCall->Transport == XRT_HTTP1_CALL_TLS) ) {
			CallResult.Tls = (xtlsstream*)pStream;
		}
	#endif
	__xrtHttp1CallExchangeUnit(pCall);
	pCall->Events.Done(
		pCall,
		&CallResult,
		pCall->Events.Data
	);
	if ( !bTransfer ) {
		__xrtHttp1CallStreamDestroy(pCall, pStream);
	}
	if ( pCall->RuntimeHeld ) {
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
	}
}



/* 精确消费已经被 Exchange 接受的传输输入。 */
static bool __xrtHttp1CallConsume(
	xhttp1call* pCall,
	ptr pStream,
	size_t iSize
)
{
	if ( iSize == 0 ) {
		return true;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamConsume(
			(xnetstream*)pStream,
			iSize
		) == iSize;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamConsume(
			(xtlsstream*)pStream,
			iSize
		);
	#else
		return false;
	#endif
}



/* 向只读观察器报告已经确认的 I/O 进度。 */
static void __xrtHttp1CallProgress(
	xhttp1call* pCall,
	xhttp1progress Progress,
	size_t iBytes
)
{
	if ( pCall->Events.Progress != NULL ) {
		pCall->Events.Progress(
			pCall,
			Progress,
			iBytes,
			pCall->Events.Data
		);
	}
}



/* 在 Exchange 首次确认请求完整发送时发布唯一完成进度。 */
static void __xrtHttp1CallRequestDone(xhttp1call* pCall)
{
	if ( pCall->RequestDoneObserved ||
		!xrtHttp1ExchangeRequestComplete(
			pCall->Exchange
		) ) {
		return;
	}
	pCall->RequestDoneObserved = true;
	__xrtHttp1CallProgress(
		pCall,
		XHTTP1_PROGRESS_REQUEST_DONE,
		0
	);
}



/* 借用传输当前的第一个输入片段。 */
static bool __xrtHttp1CallInputFront(
	xhttp1call* pCall,
	ptr pStream,
	xnetspan* pSpan
)
{
	const xnetbuf* pBuffer;

	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		pBuffer = xrtNetStreamBuffer(
			(xnetstream*)pStream
		);
	} else {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
			pBuffer = xrtTlsStreamBuffer(
				(xtlsstream*)pStream
			);
		#else
			pBuffer = NULL;
		#endif
	}
	return (pBuffer != NULL) &&
		!xrtNetBufEmpty(pBuffer) &&
		xrtNetBufFront(pBuffer, pSpan);
}



/* 消费现有输入，终态后保留底层尚未接受的协议余量。 */
static bool __xrtHttp1CallDriveInput(
	xhttp1call* pCall,
	ptr pStream
)
{
	for ( ;; ) {
		xnetspan Span;
		xhttp1feedstatus Status;
		size_t iAccepted = 0;

		if ( !__xrtHttp1CallInputFront(
			pCall, pStream, &Span
		) ) {
			return true;
		}
		Status = xrtHttp1ExchangeFeed(
			pCall->Exchange,
			(xbytesview){ Span.Data, Span.Size },
			false,
			&iAccepted
		);
		if ( !__xrtHttp1CallConsume(
			pCall, pStream, iAccepted
		) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_TRANSPORT,
				XERR_INTERNAL,
				"consume-http1-response",
				"HTTP/1 call could not consume accepted transport input",
				xrtGetError()
			);
			return false;
		}
		if ( iAccepted != 0 ) {
			__xrtHttp1CallProgress(
				pCall,
				XHTTP1_PROGRESS_READ,
				iAccepted
			);
		}
		if ( Status == XHTTP1_FEED_PAUSED ) {
			xrtAtomic32Store(
				&pCall->PauseGate,
				1,
				XMEMORY_RELEASE
			);
			__xrtHttp1CallPauseTransport(pCall, pStream);
			return true;
		}
		if ( Status == XHTTP1_FEED_ERROR ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_PROTOCOL,
				"parse-http1-response",
				"HTTP/1 response parsing failed",
				xrtHttp1ExchangeError(pCall->Exchange)
			);
			return false;
		}
		if ( (Status == XHTTP1_FEED_DONE) ||
			(Status == XHTTP1_FEED_UPGRADED) ) {
			__xrtHttp1CallFinishSuccess(pCall);
			return false;
		}
		if ( iAccepted == 0 ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_INTERNAL,
				"parse-http1-response",
				"HTTP/1 Exchange made no input progress",
				NULL
			);
			return false;
		}
	}
}



/* TCP 租约离队后推进 Exchange，异常终态只负责延迟回收。 */
static void __xrtHttp1CallTcpOutputRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xhttp1call* pCall = (xhttp1call*)pContext;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);

	if ( pCall->OutputQueued &&
		(pCall->OutputBytes == iSize) ) {
		pCall->OutputQueued = false;
		pCall->OutputBytes = 0;
		if ( !xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) && (pCall->Exchange != NULL) &&
			(pStream != NULL) &&
			(xrtNetStreamState(pStream) ==
			 XNET_STREAM_OPEN) ) {
			if ( !xrtHttp1ExchangeOutputConsume(
				pCall->Exchange,
				iSize
			) ) {
				__xrtHttp1CallFail(
					pCall,
					XHTTP1_CALL_ERROR_EXCHANGE,
					XERR_INTERNAL,
					"consume-http1-request",
					"HTTP/1 Exchange rejected a completed TCP output lease",
					xrtGetError()
				);
			} else {
				__xrtHttp1CallProgress(
					pCall,
					XHTTP1_PROGRESS_WRITE,
					iSize
				);
				__xrtHttp1CallRequestDone(pCall);
				__xrtHttp1CallDrive(pCall);
			}
		} else if ( xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttp1CallExchangeUnit(pCall);
		}
	}
	(void)pData;
	xrtHttp1CallDestroy(pCall);
}



/* 把一段 Exchange 输出以稳定引用交给 TCP 发送队列。 */
static xnetresult __xrtHttp1CallSendTcp(
	xhttp1call* pCall,
	xnetstream* pStream,
	xbytesview Data
)
{
	xnetresult Result;

	if ( xrtRefRetain(&pCall->References) < 0 ) {
		__xrtHttp1CallSetError(
			XERR_STATE,
			XHTTP1_CALL_ERROR_STATE,
			"send-http1-request",
			"HTTP/1 call reference is no longer valid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pCall->OutputQueued = true;
	pCall->OutputBytes = Data.Size;
	Result = xrtNetStreamSendRef(
		pStream,
		Data.Data,
		Data.Size,
		__xrtHttp1CallTcpOutputRelease,
		pCall
	);
	if ( Result != XNET_RESULT_OK ) {
		pCall->OutputQueued = false;
		pCall->OutputBytes = 0;
		xrtHttp1CallDestroy(pCall);
	}
	return Result;
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 把一段 Exchange 输出同步编码到 TLS，并保留成功短写语义。 */
static xnetresult __xrtHttp1CallSendTls(
	xhttp1call* pCall,
	xtlsstream* pStream,
	xbytesview Data,
	size_t* pWritten
)
{
	xtlsresult Result = xrtTlsStreamSend(
		pStream,
		Data.Data,
		Data.Size,
		pWritten
	);

	(void)pCall;
	if ( Result == XTLS_OK ) {
		return XNET_RESULT_OK;
	}
	if ( Result == XTLS_AGAIN ) {
		return XNET_RESULT_AGAIN;
	}
	return XNET_RESULT_ERROR;
}

#endif



/* 在当前硬预算内持续发送请求，直到正文或传输产生背压。 */
static bool __xrtHttp1CallDriveOutput(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pCall->OutputWaiting || pCall->OutputQueued ) {
		return true;
	}
	for ( ;; ) {
		xbytesview Data;
		xhttp1outputstatus Status;
		xnetresult Result;
		size_t iMaximum = pCall->Config.WriteSize;
		size_t iWritten = 0;

		if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
			size_t iWritable = xrtNetStreamWritable(
				(const xnetstream*)pStream
			);

			if ( iWritable == 0 ) {
				return true;
			}
			if ( iMaximum > iWritable ) {
				iMaximum = iWritable;
			}
		}
		Status = xrtHttp1ExchangeOutput(
			pCall->Exchange,
			iMaximum,
			&Data
		);
		if ( Status == XHTTP1_OUTPUT_ERROR ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_IO,
				"produce-http1-request",
				"HTTP/1 request output failed",
				xrtHttp1ExchangeError(pCall->Exchange)
			);
			return false;
		}
		if ( Status == XHTTP1_OUTPUT_AGAIN ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
				if ( !__xrtHttp1CallAsyncWait(pCall) ) {
					__xrtHttp1CallFail(
						pCall,
						XHTTP1_CALL_ERROR_EXCHANGE,
						XERR_IO,
						"wait-http-request-body",
						"HTTP request body readiness wait failed",
						xrtGetError()
					);
					return false;
				}
				return true;
			#else
				__xrtHttp1CallFail(
					pCall,
					XHTTP1_CALL_ERROR_EXCHANGE,
					XERR_UNSUPPORTED,
					"wait-http-request-body",
					"asynchronous HTTP request body support is disabled",
					xrtHttp1ExchangeError(pCall->Exchange)
				);
				return false;
			#endif
		}
		if ( (Status == XHTTP1_OUTPUT_CONTINUE) ||
			(Status == XHTTP1_OUTPUT_DONE) ) {
			__xrtHttp1CallRequestDone(pCall);
			return true;
		}
		if ( (Status != XHTTP1_OUTPUT_DATA) ||
			(Data.Data == NULL) || (Data.Size == 0) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_INTERNAL,
				"produce-http1-request",
				"HTTP/1 Exchange returned invalid output",
				NULL
			);
			return false;
		}
		if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
			Result = __xrtHttp1CallSendTcp(
				pCall,
				(xnetstream*)pStream,
				Data
			);
			if ( Result == XNET_RESULT_OK ) {
				return true;
			}
		} else {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
				Result = __xrtHttp1CallSendTls(
					pCall,
					(xtlsstream*)pStream,
					Data,
					&iWritten
				);
			#else
				Result = XNET_RESULT_ERROR;
			#endif
		}
		if ( iWritten != 0 ) {
			if ( !xrtHttp1ExchangeOutputConsume(
				pCall->Exchange,
				iWritten
			) ) {
				__xrtHttp1CallFail(
					pCall,
					XHTTP1_CALL_ERROR_EXCHANGE,
					XERR_INTERNAL,
					"consume-http1-request",
					"HTTP/1 Exchange rejected acknowledged output",
					xrtGetError()
				);
				return false;
			}
			__xrtHttp1CallProgress(
				pCall,
				XHTTP1_PROGRESS_WRITE,
				iWritten
			);
			__xrtHttp1CallRequestDone(pCall);
			continue;
		}
		if ( Result == XNET_RESULT_AGAIN ) {
			return true;
		}
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_TRANSPORT,
			XERR_IO,
			"send-http1-request",
			"HTTP/1 request transport send failed",
			xrtGetError()
		);
		return false;
	}
}



/* 串行折叠发送、接收和同步水位回调。 */
void __xrtHttp1CallDrive(xhttp1call* pCall)
{
	ptr pStream;

	if ( pCall->Driving ) {
		pCall->DriveAgain = true;
		return;
	}
	/* 传输事件已经由运行时引用保证对象有效，只增加当前驱动引用。 */
	if ( xrtRefRetain(&pCall->References) < 0 ) {
		__xrtHttp1CallSetError(
			XERR_STATE,
			XHTTP1_CALL_ERROR_STATE,
			"drive-http1-call",
			"HTTP/1 call reference is no longer valid",
			NULL
		);
		return;
	}
	pCall->Driving = true;
	do {
		pCall->DriveAgain = false;
		if ( xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		if ( xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_CANCELLED,
				XERR_CANCELLED,
				"cancel-http1-call",
				"HTTP/1 call was cancelled",
				NULL
			);
			break;
		}
		pStream = xrtAtomicPtrLoad(
			&pCall->Stream,
			XMEMORY_ACQUIRE
		);
		if ( pStream == NULL ) {
			break;
		}
		if ( !__xrtHttp1CallDriveInput(
			pCall, pStream
		) || xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		if ( !__xrtHttp1CallDriveOutput(
			pCall, pStream
		) || xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		(void)__xrtHttp1CallDriveInput(
			pCall, pStream
		);
	} while ( pCall->DriveAgain && !xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) );
	pCall->Driving = false;
	if ( pCall->InputEnded &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttp1CallEnd(pCall);
	}
	xrtHttp1CallDestroy(pCall);
}



/* TCP 输入到达后推进完整双向状态机。 */
static void __xrtHttp1CallTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TCP 写队列重新具备容量后继续发送请求。 */
static void __xrtHttp1CallTcpWritable(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TCP 写队列完全排空时继续同步正文源。 */
static void __xrtHttp1CallTcpDrain(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* 把 TCP FIN 作为 HTTP/1 close-delimited 输入终点。 */
static void __xrtHttp1CallEnd(xhttp1call* pCall)
{
	size_t iAccepted = 0;
	xhttp1feedstatus Status;

	pCall->InputEnded = true;
	if ( pCall->Driving ||
		xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		return;
	}
	Status = xrtHttp1ExchangeFeed(
		pCall->Exchange,
		(xbytesview){ NULL, 0 },
		true,
		&iAccepted
	);

	if ( Status == XHTTP1_FEED_PAUSED ) {
		return;
	}
	if ( (Status == XHTTP1_FEED_DONE) ||
		(Status == XHTTP1_FEED_UPGRADED) ) {
		__xrtHttp1CallFinishSuccess(pCall);
		return;
	}
	__xrtHttp1CallFail(
		pCall,
		XHTTP1_CALL_ERROR_EXCHANGE,
		XERR_PROTOCOL,
		"finish-http1-response",
		"HTTP/1 response ended before Exchange completion",
		xrtHttp1ExchangeError(pCall->Exchange)
	);
}



/* TCP 对端结束读方向时完成 close-delimited 响应。 */
static void __xrtHttp1CallTcpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallEnd((xhttp1call*)pData);
}



/* TCP 在 HTTP 终态前关闭时发布传输失败。 */
static void __xrtHttp1CallTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pStream;
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ? XERR_CANCELLED : XERR_IO,
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ?
				XHTTP1_CALL_ERROR_CANCELLED :
				XHTTP1_CALL_ERROR_TRANSPORT,
			"close-http1-transport",
			"HTTP/1 TCP transport closed before completion",
			pError
		);
	}
	__xrtHttp1CallFinishFailure(pCall, Result, pError);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* TLS 明文到达后推进完整双向状态机。 */
static void __xrtHttp1CallTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TLS 记录和底层 TCP 重新具备容量后继续发送。 */
static void __xrtHttp1CallTlsWritable(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TLS 密文与底层 TCP 都排空时继续同步正文源。 */
static void __xrtHttp1CallTlsDrain(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* 对端认证关闭读方向时完成 close-delimited 响应。 */
static void __xrtHttp1CallTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallEnd((xhttp1call*)pData);
}



/* TLS 在 HTTP 终态前关闭时发布传输失败。 */
static void __xrtHttp1CallTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pStream;
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ? XERR_CANCELLED : XERR_IO,
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ?
				XHTTP1_CALL_ERROR_CANCELLED :
				XHTTP1_CALL_ERROR_TRANSPORT,
			"close-http1-transport",
			"HTTP/1 TLS transport closed before completion",
			pError
		);
	}
	__xrtHttp1CallFinishFailure(pCall, Result, pError);
}

#endif



/* 初始化不常驻缓冲的单次输出上限。 */
XRT_API void xrtHttp1CallConfigInit(xhttp1callconfig* pConfig)
{
	const xhttp1callconfig Config = {
		XRT_HTTP1_CALL_WRITE_DEFAULT
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"init-http1-call-config",
			"HTTP/1 call config range is invalid",
			NULL
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化调用事件表。 */
XRT_API void xrtHttp1CallEventsInit(xhttp1callevents* pEvents)
{
	const xhttp1callevents Events = { 0 };

	if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"init-http1-call-events",
			"HTTP/1 call events range is invalid",
			NULL
		);
		return;
	}
	memcpy(pEvents, &Events, sizeof(Events));
}



/* 建立调用公共状态；成功后调用对象接管 Exchange。 */
static xhttp1call* __xrtHttp1CallCreate(
	xnetworker* pWorker,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents,
	xrt_http1_call_transport Transport
)
{
	xhttp1callconfig Config = {
		XRT_HTTP1_CALL_WRITE_DEFAULT
	};
	xhttp1callevents Events;
	xhttp1call* pCall;

	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttp1CallSetError(
				XERR_ARGUMENT,
				XHTTP1_CALL_ERROR_ARGUMENT,
				"create-http1-call",
				"HTTP/1 call config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-call",
			"HTTP/1 call events range is invalid",
			NULL
		);
		return NULL;
	}
	memcpy(&Events, pEvents, sizeof(Events));
	if ( (pWorker == NULL) ||
		!xrtNetWorkerIsCurrent(pWorker) ||
		(pExchange == NULL) || (Events.Done == NULL) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-call",
			"HTTP/1 call requires an Exchange and its current Worker",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttp1CallConfigValid(&Config) ) {
		return NULL;
	}
	pCall = (xhttp1call*)xrtCalloc(
		1, sizeof(*pCall)
	);
	if ( pCall == NULL ) {
		return NULL;
	}
	if ( !xrtNetPostInit(&pCall->StartPost) ||
		!xrtNetPostInit(&pCall->ResumePost)
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
			|| !xrtNetPostInit(&pCall->OutputPost)
		#endif
	) {
		xrtFree(pCall);
		return NULL;
	}
	if ( !xrtSpinInit(&pCall->Lock) ) {
		xrtFree(pCall);
		return NULL;
	}
	pCall->References = 2;
	xrtAtomic32Init(
		&pCall->State,
		XHTTP1_CALL_RUNNING
	);
	xrtAtomic32Init(&pCall->CancelGate, 0);
	xrtAtomic32Init(&pCall->FinishGate, 0);
	xrtAtomic32Init(&pCall->PauseGate, 0);
	xrtAtomic32Init(&pCall->ResumeGate, 0);
	xrtAtomicPtrInit(&pCall->Stream, NULL);
	pCall->Worker = pWorker;
	pCall->Exchange = pExchange;
	pCall->Config = Config;
	pCall->Events = Events;
	pCall->Transport = Transport;
	pCall->RuntimeHeld = true;
	return pCall;
}



/* 当前构造回调返回后，在所属 Worker 上启动第一次双向驱动。 */
static void __xrtHttp1CallStart(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pWorker;
	__xrtHttp1CallDrive(pCall);
	xrtHttp1CallDestroy(pCall);
}



/* 接管 TCP 事件，并把第一次驱动延迟到当前 Worker 回调返回之后。 */
XRT_API xhttp1call* xrtHttp1CallTcp(
	xnetstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
)
{
	xnetstreamevents Events;
	xhttp1call* pCall;

	if ( pStream == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-tcp-call",
			"HTTP/1 TCP call stream is null",
			NULL
		);
		return NULL;
	}
	pCall = __xrtHttp1CallCreate(
		xrtNetStreamWorker(pStream),
		pExchange,
		pConfig,
		pEvents,
		XRT_HTTP1_CALL_TCP
	);
	if ( pCall == NULL ) {
		return NULL;
	}
	memset(&Events, 0, sizeof(Events));
	Events.Read = __xrtHttp1CallTcpRead;
	Events.End = __xrtHttp1CallTcpEnd;
	Events.LowWater = __xrtHttp1CallTcpWritable;
	Events.Drain = __xrtHttp1CallTcpDrain;
	Events.Close = __xrtHttp1CallTcpClose;
	if ( !xrtNetStreamSetEvents(
		pStream,
		&Events,
		pCall
	) ) {
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pCall->Stream,
		pStream,
		XMEMORY_RELEASE
	);
	(void)xrtHttp1CallRef(pCall);
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->StartPost,
		__xrtHttp1CallStart,
		pCall
	) ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		memset(&Events, 0, sizeof(Events));
		(void)xrtNetStreamSetEvents(pStream, &Events, NULL);
		xrtAtomicPtrStore(&pCall->Stream, NULL, XMEMORY_RELEASE);
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		__xrtHttp1CallSetError(
			xrtErrorKind(pCause),
			XHTTP1_CALL_ERROR_STATE,
			"start-http1-tcp-call",
			"HTTP/1 TCP call could not be scheduled",
			pCause
		);
		xrtErrorFree(pCause);
		return NULL;
	}
	return pCall;
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 接管 TLS 明文事件，并把第一次驱动延迟到当前 Worker 回调返回之后。 */
XRT_API xhttp1call* xrtHttp1CallTls(
	xtlsstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
)
{
	xtlsstreamevents Events;
	xhttp1call* pCall;

	if ( pStream == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-tls-call",
			"HTTP/1 TLS call stream is null",
			NULL
		);
		return NULL;
	}
	pCall = __xrtHttp1CallCreate(
		xrtNetStreamWorker(
			xrtTlsStreamTransport(pStream)
		),
		pExchange,
		pConfig,
		pEvents,
		XRT_HTTP1_CALL_TLS
	);
	if ( pCall == NULL ) {
		return NULL;
	}
	memset(&Events, 0, sizeof(Events));
	Events.Read = __xrtHttp1CallTlsRead;
	Events.End = __xrtHttp1CallTlsEnd;
	Events.Writable = __xrtHttp1CallTlsWritable;
	Events.Drain = __xrtHttp1CallTlsDrain;
	Events.Close = __xrtHttp1CallTlsClose;
	if ( !xrtTlsStreamSetEvents(
		pStream,
		&Events,
		pCall
	) ) {
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pCall->Stream,
		pStream,
		XMEMORY_RELEASE
	);
	(void)xrtHttp1CallRef(pCall);
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->StartPost,
		__xrtHttp1CallStart,
		pCall
	) ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		memset(&Events, 0, sizeof(Events));
		(void)xrtTlsStreamSetEvents(pStream, &Events, NULL);
		xrtAtomicPtrStore(&pCall->Stream, NULL, XMEMORY_RELEASE);
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		__xrtHttp1CallSetError(
			xrtErrorKind(pCause),
			XHTTP1_CALL_ERROR_STATE,
			"start-http1-tls-call",
			"HTTP/1 TLS call could not be scheduled",
			pCause
		);
		xrtErrorFree(pCause);
		return NULL;
	}
	return pCall;
}

#endif



/* 从任意线程请求取消，最终完成由所属 Worker 串行发布。 */
XRT_API bool xrtHttp1CallCancel(xhttp1call* pCall)
{
	ptr pOwned;
	ptr pStream;

	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"cancel-http1-call",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) || xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_RELAXED
	);
	pOwned = __xrtHttp1CallStreamRef(pCall, pStream);
	if ( (pStream != NULL) && (pOwned == NULL) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	xrtAtomic32Store(
		&pCall->CancelGate,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	__xrtHttp1CallStreamAbort(pCall, pOwned);
	__xrtHttp1CallStreamDestroy(pCall, pOwned);
	return true;
}



/* 在所属 Worker 上立即暂停 Exchange 与实际 TCP 读门。 */
XRT_API bool xrtHttp1CallPause(xhttp1call* pCall)
{
	ptr pStream;

	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"pause-http1-call",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pCall->Worker) ) {
		__xrtHttp1CallSetError(
			XERR_STATE,
			XHTTP1_CALL_ERROR_STATE,
			"pause-http1-call",
			"HTTP/1 call pause must run on its Worker",
			NULL
		);
		return false;
	}
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) || xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	if ( xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	if ( !xrtHttp1ExchangePause(pCall->Exchange) ) {
		return false;
	}
	xrtAtomic32Store(
		&pCall->PauseGate,
		1,
		XMEMORY_RELEASE
	);
	pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);
	__xrtHttp1CallPauseTransport(pCall, pStream);
	return true;
}



/* 在 Worker 上恢复协议输入，先消费已缓冲数据，再重新开放 TCP 读取。 */
static void __xrtHttp1CallResumeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;
	ptr pStream;

	(void)pWorker;
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) || (pCall->Exchange == NULL) ) {
		xrtAtomic32Store(
			&pCall->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		xrtHttp1CallDestroy(pCall);
		return;
	}
	if ( !xrtHttp1ExchangeResume(pCall->Exchange) ) {
		xrtAtomic32Store(
			&pCall->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_EXCHANGE,
			XERR_STATE,
			"resume-http1-response",
			"HTTP/1 Exchange could not resume response input",
			xrtGetError()
		);
		xrtHttp1CallDestroy(pCall);
		return;
	}
	xrtAtomic32Store(
		&pCall->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
	__xrtHttp1CallDrive(pCall);
	if ( !xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) && !xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_ACQUIRE
	) ) {
		pStream = xrtAtomicPtrLoad(
			&pCall->Stream,
			XMEMORY_ACQUIRE
		);
		if ( (pStream != NULL) &&
			!__xrtHttp1CallResumeTransport(pCall, pStream) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_TRANSPORT,
				XERR_IO,
				"resume-http1-response",
				"HTTP/1 call transport could not resume input",
				xrtGetError()
			);
		}
	}
	xrtHttp1CallDestroy(pCall);
}



/* 从任意线程合并提交一次恢复命令。 */
XRT_API bool xrtHttp1CallResume(xhttp1call* pCall)
{
	uint32 iExpected = 0;

	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"resume-http1-call",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) || !xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_RELAXED
	) || !xrtAtomic32CompareExchange(
		&pCall->ResumeGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_RELAXED
	) || (xrtRefRetain(&pCall->References) < 0) ) {
		if ( iExpected == 0 ) {
			xrtAtomic32Store(
				&pCall->ResumeGate,
				0,
				XMEMORY_RELEASE
			);
		}
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->ResumePost,
		__xrtHttp1CallResumeTask,
		pCall
	) ) {
		xrtAtomic32Store(
			&pCall->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		xrtHttp1CallDestroy(pCall);
		return false;
	}
	return true;
}



/* 返回暂停门状态；恢复命令真正执行前仍报告暂停。 */
XRT_API bool xrtHttp1CallPaused(const xhttp1call* pCall)
{
	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"query-http1-call-pause",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	return xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_ACQUIRE
	) != 0;
}



/* 返回调用状态快照。 */
XRT_API xhttp1callstate xrtHttp1CallState(
	const xhttp1call* pCall
)
{
	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"query-http1-call-state",
			"HTTP/1 call is null",
			NULL
		);
		return XHTTP1_CALL_FAILED;
	}
	return (xhttp1callstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回终态错误借用。 */
XRT_API const xerror* xrtHttp1CallError(
	const xhttp1call* pCall
)
{
	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"query-http1-call-error",
			"HTTP/1 call is null",
			NULL
		);
		return NULL;
	}
	return xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	) == XHTTP1_CALL_RUNNING ? NULL : pCall->Error;
}

#endif
