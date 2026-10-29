#include "../internal/xrt_http_client_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

/* HTTP Client 默认把一次完整请求限制在三十秒内。 */
#define XRT_HTTP_CLIENT_TIMEOUT_DEFAULT_VALUE UINT64_C(30000000)
#define XRT_HTTP_CLIENT_IDLE_TIMEOUT_DEFAULT_VALUE UINT64_C(30000000)

#define XRT_HTTP_TIMEOUT_NONE	UINT32_C(0)
#define XRT_HTTP_TIMEOUT_TOTAL	UINT32_C(1)
#define XRT_HTTP_TIMEOUT_IDLE	UINT32_C(2)



/* 建立客户端域错误并保留明确的下层原因。 */
xerror* __xrtHttpClientErrorCreate(
	xerrkind Kind,
	xhttpclienterror Code,
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
	Desc.Domain = "xrt.http.client";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 判断错误是否已经是所需的高层 HTTP Client 终态错误。 */
static bool __xrtHttpCallErrorIs(
	const xerror* pError,
	xerrkind Kind,
	xhttpclienterror Code
)
{
	return (pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(xrtErrorCode(pError) == (int32)Code) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0);
}



/*
	建立取消或超时终态错误并接管旧错误。
	分配失败时退回核心静态错误，保证终态类别仍然可靠。
*/
xerror* __xrtHttpClientTerminalError(
	xerror* pCause,
	xhttpclienterror Code
)
{
	xerrkind Kind;
	cstr sOperation;
	cstr sMessage;
	xerror* pError;

	if ( Code == XHTTP_CLIENT_ERROR_TIMEOUT_IDLE ) {
		Kind = XERR_TIMEOUT;
		sOperation = "run-http-call";
		sMessage =
			"HTTP call made no progress before its idle deadline";
	} else if ( Code == XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL ) {
		Kind = XERR_TIMEOUT;
		sOperation = "run-http-call";
		sMessage = "HTTP call exceeded its total deadline";
	} else {
		Code = XHTTP_CLIENT_ERROR_CANCELLED;
		Kind = XERR_CANCELLED;
		sOperation = "cancel-http-call";
		sMessage = "HTTP call was cancelled";
	}
	if ( __xrtHttpCallErrorIs(pCause, Kind, Code) ) {
		return pCause;
	}
	pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	if ( __xrtHttpCallErrorIs(pError, Kind, Code) ) {
		xrtErrorFree(pCause);
		return pError;
	}
	xrtErrorFree(pError);
	xrtErrorFree(pCause);
	xrtSetErrorKind(Kind);
	return xrtErrorRef(xrtGetError());
}



/* 设置创建阶段当前线程错误。 */
void __xrtHttpClientSetError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 沿原因链返回最内层有效错误类别。 */
xerrkind __xrtHttpClientCauseKind(
	const xerror* pError,
	xerrkind Fallback
)
{
	xerrkind Kind = Fallback;

	while ( pError != NULL ) {
		if ( xrtErrorKind(pError) != XERR_NONE ) {
			Kind = xrtErrorKind(pError);
		}
		pError = xrtErrorCause(pError);
	}
	return Kind;
}



/*
	接管当前错误；已经属于 HTTP Client 域时原样保留，
	否则建立稳定高层分类并把原错误挂入 Cause。
*/
static xerror* __xrtHttpClientPromoteOwned(
	xerror* pCause,
	xhttpclienterror Code,
	xerrkind Fallback,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError;
	cstr sDomain = pCause != NULL ?
		xrtErrorDomain(pCause) : NULL;

	if ( (sDomain != NULL) &&
		(strcmp(sDomain, "xrt.http.client") == 0) ) {
		return pCause;
	}
	pError = __xrtHttpClientErrorCreate(
		__xrtHttpClientCauseKind(pCause, Fallback),
		Code,
		sOperation,
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	return pError;
}



/* 把下层配置错误提升为 Client 创建阶段的统一错误。 */
static bool __xrtHttpClientConfigFailure(cstr sMessage)
{
	xerror* pError = __xrtHttpClientPromoteOwned(
		xrtTakeError(),
		XHTTP_CLIENT_ERROR_CONFIG,
		XERR_ARGUMENT,
		"configure-http-client",
		sMessage
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
	return false;
}



/* 只写入尚未到达的第一个时间点。 */
static bool __xrtHttpCallFirstTime(
	xatomic64* pTime,
	uint64 iNow
)
{
	uint64 iExpected = 0;

	return xrtAtomic64CompareExchange(
		pTime,
		&iExpected,
		iNow,
		XMEMORY_RELEASE,
		XMEMORY_RELAXED
	);
}



/* 把有限相对时长转换为 Engine 可调度的最大有限 deadline。 */
static xdeadline __xrtHttpCallDeadline(
	uint64 iNow,
	uint64 iTimeout
)
{
	if ( iNow >=
		((XRT_DEADLINE_NEVER - 1u) - iTimeout) ) {
		return XRT_DEADLINE_NEVER - 1u;
	}
	return iNow + iTimeout;
}



/* 按最后一次真实进度刷新 idle deadline，不创建新 Timer。 */
static void __xrtHttpCallProgressAt(
	xhttpcall* pCall,
	uint64 iNow
)
{
	uint64 iDeadline;

	xrtAtomic64Store(
		&pCall->Info.LastProgress,
		iNow,
		XMEMORY_RELEASE
	);
	if ( pCall->IdleTimeout == XHTTP_CLIENT_TIMEOUT_NONE ) {
		return;
	}
	iDeadline = __xrtHttpCallDeadline(
		iNow,
		pCall->IdleTimeout
	);
	xrtAtomic64Store(
		&pCall->IdleDeadline,
		iDeadline,
		XMEMORY_RELEASE
	);
}



/* 发布高层阶段。 */
void __xrtHttpCallSetPhase(
	xhttpcall* pCall,
	xhttpcallphase Phase
)
{
	if ( pCall != NULL ) {
		xrtAtomic32Store(
			&pCall->Info.Phase,
			(uint32)Phase,
			XMEMORY_RELEASE
		);
	}
}



/* 标记当前 Hop 的传输已经可以承载 HTTP。 */
void __xrtHttpCallTransportReady(xhttpcall* pCall)
{
	uint64 iNow;

	if ( pCall == NULL ) {
		return;
	}
	iNow = xrtClock();
	(void)__xrtHttpCallFirstTime(
		&pCall->Info.TransportReady,
		iNow
	);
	__xrtHttpCallProgressAt(pCall, iNow);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_REQUEST
	);
}



/* 标记当前 Hop 复用了既有连接。 */
void __xrtHttpCallReused(xhttpcall* pCall)
{
	if ( pCall != NULL ) {
		xrtAtomic32Store(
			&pCall->Info.Reused,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 把低级 HTTP/1 的真实 I/O 进度累计到高层 Call 快照。 */
static void __xrtHttpCallStreamProgress(
	xhttp1call* pStreamCall,
	xhttp1progress Progress,
	size_t iBytes,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	uint64 iNow = xrtClock();

	(void)pStreamCall;
	__xrtHttpCallProgressAt(pCall, iNow);
	if ( Progress == XHTTP1_PROGRESS_WRITE ) {
		(void)xrtAtomic64FetchAdd(
			&pCall->Info.RequestWireBytes,
			(uint64)iBytes,
			XMEMORY_RELAXED
		);
	} else if ( Progress == XHTTP1_PROGRESS_READ ) {
		(void)xrtAtomic64FetchAdd(
			&pCall->Info.ResponseWireBytes,
			(uint64)iBytes,
			XMEMORY_RELAXED
		);
		if ( __xrtHttpCallFirstTime(
			&pCall->Info.FirstByte,
			iNow
		) ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_RESPONSE_HEADERS
			);
		}
	} else if ( Progress ==
		XHTTP1_PROGRESS_REQUEST_DONE ) {
		(void)__xrtHttpCallFirstTime(
			&pCall->Info.RequestSent,
			iNow
		);
		if ( xrtAtomic64Load(
			&pCall->Info.FirstByte,
			XMEMORY_ACQUIRE
		) == 0 ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_RESPONSE_HEADERS
			);
		}
	}
}



/* 建立高层 Client 复用的低级 HTTP/1 Call 事件表。 */
void __xrtHttpCallStreamEvents(
	xhttpcall* pCall,
	xhttp1callevents* pEvents
)
{
	xrtHttp1CallEventsInit(pEvents);
	pEvents->Done = __xrtHttpClientStreamDone;
	pEvents->Progress = __xrtHttpCallStreamProgress;
	pEvents->Data = pCall;
}



/* 初始化 Client 的完整默认策略。 */
XRT_API void xrtHttpClientConfigInit(xhttpclientconfig* pConfig)
{
	xhttpclientconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-client-config",
			"HTTP client config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtNetResolverConfigInit(&Config.Resolver);
	xrtNetDialConfigInit(&Config.Dial);
	xrtHttp1CallConfigInit(&Config.Call);
	xrtHttp1ExchangeConfigInit(&Config.Exchange);
	Config.Timeout = XRT_HTTP_CLIENT_TIMEOUT_DEFAULT_VALUE;
	Config.IdleTimeout =
		XRT_HTTP_CLIENT_IDLE_TIMEOUT_DEFAULT_VALUE;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		Config.Proxy = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xrtHttpRedirectConfigInit(&Config.Redirect);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtHttpRetryConfigInit(&Config.Retry);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		xrtHttpClientPoolConfigInit(&Config.Pool);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		xrtHttpDecompressConfigInit(
			&Config.Decompress
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xrtHttpClientCacheConfigInit(
			&Config.Cache
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtTlsStreamConfigInit(&Config.TlsStream);
		Config.SystemTrust = true;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xrtHttpResumeConfigInit(&Config.Resume);
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化单次调用选项。 */
XRT_API void xrtHttpCallOptionsInit(xhttpcalloptions* pOptions)
{
	xhttpcalloptions Options;

	if ( !xrtMemRangeValid(pOptions, sizeof(*pOptions)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-call-options",
			"HTTP call options range is invalid",
			NULL
		);
		return;
	}
	memset(&Options, 0, sizeof(Options));
	xrtHttp1RequestOptionsInit(&Options.Request);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtHttpProxyOptionsInit(&Options.Proxy);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xrtHttpCookieOptionsInit(&Options.Cookies);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtHttpRetryOptionsInit(&Options.Retry);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xrtHttpClientCacheOptionsInit(
			&Options.Cache
		);
	#endif
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 检查 Client 层必须立即确定的配置边界。 */
static bool __xrtHttpClientConfigValid(
	const xhttpclientconfig* pConfig
)
{
	if ( (pConfig == NULL) ||
		(pConfig->Timeout == 0) ||
		(pConfig->IdleTimeout == 0) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-http-client",
			"HTTP client write size and timeout policies must be non-zero",
			NULL
		);
		return false;
	}
	if ( !xrtNetDialConfigValid(&pConfig->Dial) ) {
		return __xrtHttpClientConfigFailure(
			"HTTP client Dial policy is invalid"
		);
	}
	if ( !__xrtHttp1CallConfigValid(&pConfig->Call) ) {
		return __xrtHttpClientConfigFailure(
			"HTTP client transport policy is invalid"
		);
	}
	if ( !__xrtHttp1ExchangeConfigValid(
		&pConfig->Exchange
	) ) {
		return __xrtHttpClientConfigFailure(
			"HTTP client response policy is invalid"
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( (pConfig->Redirect.Flags &
			~(XHTTP_REDIRECT_POST_TO_GET |
			  XHTTP_REDIRECT_FORWARD_CREDENTIALS |
			  XHTTP_REDIRECT_ALLOW_DOWNGRADE)) != 0 ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"configure-http-client",
				"HTTP redirect flags are invalid",
				NULL
			);
			return false;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( ((pConfig->Retry.Flags &
			~(XHTTP_RETRY_STATUS |
			  XHTTP_RETRY_TRANSPORT |
			  XHTTP_RETRY_RESPECT_AFTER |
			  XHTTP_RETRY_JITTER)) != 0) ||
			(pConfig->Retry.MaxDelay == 0) ||
			(pConfig->Retry.BaseDelay >
			 pConfig->Retry.MaxDelay) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"configure-http-client",
				"HTTP retry policy is invalid",
				NULL
			);
			return false;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( (pConfig->Decompress.MaxBody == 0) ||
			(pConfig->Decompress.MaxCodings == 0) ||
			(pConfig->Decompress.MaxCodings >
			 XHTTP_DECOMPRESS_CODINGS_MAX) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"configure-http-client",
				"HTTP decompression limits are invalid",
				NULL
			);
			return false;
		}
	#endif
	return true;
}



/* 增加不改变公开 Owner 数量的内部引用。 */
xhttpclient* __xrtHttpClientHold(xhttpclient* pClient)
{
	if ( (pClient == NULL) ||
		(xrtRefRetain(&pClient->References) < 0) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-client",
			"HTTP client is null or already released",
			NULL
		);
		return NULL;
	}
	return pClient;
}



/* 释放最后一个内部引用和 Client 拥有的共享资源。 */
void __xrtHttpClientRelease(xhttpclient* pClient)
{
	if ( (pClient == NULL) ||
		(xrtRefRelease(&pClient->References) != 0) ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolUnit(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		__xrtHttpResumeUnit(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpClientTlsUnit(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xrtCookieJarRelease(pClient->Cookies);
		pClient->Cookies = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		__xrtHttpClientCacheClose(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyRelease(pClient->Proxy);
		pClient->Proxy = NULL;
		pClient->Config.Proxy = NULL;
	#endif
	if ( pClient->OwnResolver && (pClient->Resolver != NULL) ) {
		(void)xrtNetResolverDestroy(pClient->Resolver);
		pClient->Resolver = NULL;
	}
	if ( pClient->EngineHeld ) {
		pClient->EngineHeld = false;
		(void)xrtNetEngineUnpin(pClient->Engine);
	}
	(void)xrtSpinUnit(&pClient->LifecycleLock);
	memset(pClient, 0, sizeof(*pClient));
	xrtFree(pClient);
}



/* 判断连接池拥有的异步传输和 Timer 是否已经完全退出。 */
static bool __xrtHttpClientPoolStopped(xhttpclient* pClient)
{
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( !pClient->PoolReady ) {
			return true;
		}
		return xrtAtomic64Load(
			&pClient->PoolLive,
			XMEMORY_ACQUIRE
		) == 0;
	#else
		(void)pClient;
		return true;
	#endif
}



/* 在全部活动对象退出后发布唯一 Client 关闭终态。 */
void __xrtHttpClientTryFinish(xhttpclient* pClient)
{
	xhttpclientstate State;
	bool bPoolStopped;
	bool bFinished = false;

	if ( pClient == NULL ) {
		return;
	}
	/* 池互斥锁可能阻塞，不能在生命周期自旋锁内等待。 */
	bPoolStopped = __xrtHttpClientPoolStopped(pClient);
	(void)xrtSpinLock(&pClient->LifecycleLock);
	State = (xhttpclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	if ( ((State == XHTTP_CLIENT_DRAINING) ||
		  (State == XHTTP_CLIENT_ABORTING)) &&
		(pClient->ActiveCalls == 0) &&
		bPoolStopped ) {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_CLIENT_CLOSED,
			XMEMORY_RELEASE
		);
		bFinished = true;
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)
		if ( bFinished ) {
			__xrtHttpClientFutureFinish(pClient);
		}
	#else
		(void)bFinished;
	#endif
}



/* 在仍运行的 Client 中登记一个已经准备完成的 Call。 */
bool __xrtHttpClientCallAttach(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	bool bAttached = false;

	(void)xrtSpinLock(&pClient->LifecycleLock);
	if ( xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	) == XHTTP_CLIENT_RUNNING ) {
		pCall->ClientPrevious = pClient->CallTail;
		pCall->ClientNext = NULL;
		if ( pClient->CallTail != NULL ) {
			pClient->CallTail->ClientNext = pCall;
		} else {
			pClient->CallHead = pCall;
		}
		pClient->CallTail = pCall;
		pClient->ActiveCalls++;
		pCall->ClientLinked = true;
		bAttached = true;
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	if ( !bAttached ) {
		__xrtHttpClientSetError(
			XERR_CLOSED,
			XHTTP_CLIENT_ERROR_STATE,
			"run-http-client",
			"HTTP client is not accepting new calls",
			NULL
		);
	}
	return bAttached;
}



/* 用户完成回调返回后从 Client 活动表摘除 Call。 */
void __xrtHttpClientCallDetach(xhttpcall* pCall)
{
	xhttpclient* pClient;
	bool bDetached = false;

	if ( (pCall == NULL) || !pCall->ClientLinked ) {
		return;
	}
	pClient = pCall->Client;
	(void)xrtSpinLock(&pClient->LifecycleLock);
	if ( pCall->ClientLinked ) {
		if ( pCall->ClientPrevious != NULL ) {
			pCall->ClientPrevious->ClientNext =
				pCall->ClientNext;
		} else {
			pClient->CallHead = pCall->ClientNext;
		}
		if ( pCall->ClientNext != NULL ) {
			pCall->ClientNext->ClientPrevious =
				pCall->ClientPrevious;
		} else {
			pClient->CallTail = pCall->ClientPrevious;
		}
		pCall->ClientPrevious = NULL;
		pCall->ClientNext = NULL;
		pCall->ClientLinked = false;
		pClient->ActiveCalls--;
		bDetached = true;
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	if ( bDetached ) {
		__xrtHttpClientTryFinish(pClient);
	}
}



/* 增加一个可以独立调用 Destroy 的公开 Owner 引用。 */
XRT_API xhttpclient* xrtHttpClientRef(xhttpclient* pClient)
{
	if ( (pClient == NULL) ||
		(__xrtHttpClientHold(pClient) == NULL) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-client",
			"HTTP client is null or already released",
			NULL
		);
		return NULL;
	}
	if ( xrtRefRetain(&pClient->Owners) < 0 ) {
		__xrtHttpClientRelease(pClient);
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-client",
			"HTTP client has no remaining public owner",
			NULL
		);
		return NULL;
	}
	return pClient;
}



/* 释放公开 Owner；最后一个 Owner 会隐式开始平滑排空。 */
XRT_API void xrtHttpClientDestroy(xhttpclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pClient->Owners) == 0 ) {
		(void)xrtHttpClientDrain(pClient);
	}
	__xrtHttpClientRelease(pClient);
}



/* 返回 Client 当前生命周期状态。 */
XRT_API xhttpclientstate xrtHttpClientState(
	const xhttpclient* pClient
)
{
	return pClient != NULL ?
		(xhttpclientstate)xrtAtomic32Load(
			&pClient->State,
			XMEMORY_ACQUIRE
		) : XHTTP_CLIENT_CLOSED;
}



/* 返回 Client 创建时绑定且由 Client 生命周期保留的网络 Engine。 */
XRT_API xnetengine* xrtHttpClientEngine(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Engine : NULL;
}



/* 原子开始平滑排空并关闭全部空闲连接。 */
XRT_API bool xrtHttpClientDrain(xhttpclient* pClient)
{
	xhttpclientstate State;
	bool bAccepted;

	if ( pClient == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"drain-http-client",
			"HTTP client is null",
			NULL
		);
		return false;
	}
	(void)xrtSpinLock(&pClient->LifecycleLock);
	State = (xhttpclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	bAccepted = (State == XHTTP_CLIENT_RUNNING) ||
		(State == XHTTP_CLIENT_DRAINING) ||
		(State == XHTTP_CLIENT_CLOSED);
	if ( State == XHTTP_CLIENT_RUNNING ) {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_CLIENT_DRAINING,
			XMEMORY_RELEASE
		);
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	if ( !bAccepted ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtHttpClientCloseIdle(pClient);
	#endif
	__xrtHttpClientTryFinish(pClient);
	return true;
}



/* 构造失败时保留原始错误并回收已经建立的 Client 资源。 */
static xhttpclient* __xrtHttpClientCreateFail(xhttpclient* pClient)
{
	xerror* pError = __xrtHttpClientPromoteOwned(
		xrtTakeError(),
		XHTTP_CLIENT_ERROR_CONFIG,
		XERR_STATE,
		"create-http-client",
		"HTTP client shared resources could not be initialized"
	);

	xrtHttpClientDestroy(pClient);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return NULL;
}



/* 统一创建拥有或借用 Resolver 的 Client。 */
static xhttpclient* __xrtHttpClientCreate(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xhttpclientconfig* pConfig,
	bool bOwnResolver
)
{
	xhttpclientconfig Config;
	xhttpclient* pClient;

	xrtHttpClientConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				"create-http-client",
				"HTTP client config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( (pEngine == NULL) ||
		(!bOwnResolver && (pResolver == NULL)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"create-http-client",
			"HTTP client requires an engine and a valid resolver policy",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttpClientConfigValid(&Config) ) {
		return NULL;
	}
	pClient = (xhttpclient*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->References = 1;
	pClient->Owners = 1;
	xrtAtomic64Init(
		&pClient->NextAffinity,
		Config.Dial.Affinity
	);
	xrtAtomic32Init(&pClient->State, XHTTP_CLIENT_RUNNING);
	if ( !xrtSpinInit(&pClient->LifecycleLock) ) {
		xrtFree(pClient);
		return NULL;
	}
	pClient->Engine = pEngine;
	pClient->Config = Config;
	pClient->OwnResolver = bOwnResolver;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCacheOpen(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( Config.Proxy != NULL ) {
			pClient->Proxy = xrtNetProxyRetain(
				Config.Proxy
			);
			if ( pClient->Proxy == NULL ) {
				return __xrtHttpClientCreateFail(pClient);
			}
			pClient->Config.Proxy = pClient->Proxy;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		if ( Config.Cookies != NULL ) {
			pClient->Cookies = xrtCookieJarRetain(
				Config.Cookies
			);
			if ( pClient->Cookies == NULL ) {
				return __xrtHttpClientCreateFail(pClient);
			}
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( !__xrtHttpPoolInit(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		if ( !__xrtHttpResumeInit(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	if ( !xrtNetEnginePin(pEngine) ) {
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_CONFIG,
			"create-http-client",
			"HTTP client requires a running network engine",
			NULL
		);
		return __xrtHttpClientCreateFail(pClient);
	}
	pClient->EngineHeld = true;
	if ( bOwnResolver ) {
		pClient->Resolver = xrtNetResolverCreate(
			&Config.Resolver
		);
		if ( pClient->Resolver == NULL ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	} else {
		pClient->Resolver = pResolver;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( !__xrtHttpClientTlsInit(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	return pClient;
}



/* 创建拥有私有 Resolver 的 Client。 */
XRT_API xhttpclient* xrtHttpClientCreate(
	xnetengine* pEngine,
	const xhttpclientconfig* pConfig
)
{
	return __xrtHttpClientCreate(
		pEngine,
		NULL,
		pConfig,
		true
	);
}



/* 创建借用共享 Resolver 的 Client。 */
XRT_API xhttpclient* xrtHttpClientCreateWithResolver(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xhttpclientconfig* pConfig
)
{
	return __xrtHttpClientCreate(
		pEngine,
		pResolver,
		pConfig,
		false
	);
}



/* 增加 Call 引用。 */
XRT_API xhttpcall* xrtHttpCallRef(xhttpcall* pCall)
{
	if ( (pCall == NULL) ||
		(xrtRefRetain(&pCall->References) < 0) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-call",
			"HTTP call is null or already released",
			NULL
		);
		return NULL;
	}
	return pCall;
}



/* 释放 Call 最后一个引用及尚未转移的资源。 */
XRT_API void xrtHttpCallDestroy(xhttpcall* pCall)
{
	if ( (pCall == NULL) ||
		(xrtRefRelease(&pCall->References) != 0) ) {
		return;
	}
	xrtNetDialDestroy(pCall->TcpDial);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyDialDestroy(pCall->ProxyDial);
		xrtNetProxyRelease(pCall->Proxy);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtTlsDialDestroy(pCall->TlsDial);
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			xrtTlsStreamDestroy(pCall->ProxyTls);
		#endif
	#endif
	xrtHttp1CallDestroy(pCall->StreamCall);
	xrtHttp1ExchangeDestroy(pCall->Exchange);
	xrtCancelUnwatch(pCall->CancelWatch);
	xrtCancelDestroy(pCall->Cancel);
	xrtErrorFree(pCall->Error);
	xrtFree(pCall->Host);
	xrtHttpRequestDestroy(pCall->Request);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		__xrtHttpRedirectUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		__xrtHttpCookieUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		__xrtHttpDecompressReset(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		__xrtHttpClientCacheUnit(pCall);
	#endif
	(void)xrtSpinUnit(&pCall->Lock);
	__xrtHttpClientRelease(pCall->Client);
	memset(pCall, 0, sizeof(*pCall));
	xrtFree(pCall);
}



/* 回收尚未提交的 Call，并把下层同步错误提升为稳定 Client 错误。 */
static xhttpcall* __xrtHttpCallSubmitFail(
	xhttpcall* pCall,
	xhttpclienterror Code,
	xerrkind Fallback,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError = __xrtHttpClientPromoteOwned(
		xrtTakeError(),
		Code,
		Fallback,
		sOperation,
		sMessage
	);

	xrtHttpCallDestroy(pCall);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return NULL;
}



/* 取消仍在队列中的一个 Call Timer；回调负责释放其 Call 引用。 */
static void __xrtHttpCallCancelTimer(
	xhttpcall* pCall,
	xatomic64* pTimer,
	const xatomic32* pDone
)
{
	uint64 Id = xrtAtomic64Exchange(
		pTimer,
		0,
		XMEMORY_ACQ_REL
	);

	if ( (Id != 0) && !xrtAtomic32Load(
		pDone,
		XMEMORY_ACQUIRE
	) ) {
		if ( !xrtNetEngineTimerCancelCurrent(
			pCall->Client->Engine,
			Id
		) && !xrtNetEngineTimerCancel(
			pCall->Client->Engine,
			Id
		) ) {
			xrtClearError();
		}
	}
}



/* 同时撤销总截止时间和 idle deadline。 */
static void __xrtHttpCallCancelTimers(xhttpcall* pCall)
{
	__xrtHttpCallCancelTimer(
		pCall,
		&pCall->TotalTimer,
		&pCall->TotalTimerDone
	);
	__xrtHttpCallCancelTimer(
		pCall,
		&pCall->IdleTimer,
		&pCall->IdleTimerDone
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		__xrtHttpCallCancelTimer(
			pCall,
			&pCall->RetryTimer,
			&pCall->RetryTimerDone
		);
	#endif
}



/* 释放一个未被用户接管的成功结果。 */
static void __xrtHttpCallResultDestroy(
	xhttpresponse* pResponse,
	xnetstream* pTcp
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		, xtlsstream* pTls
	#endif
)
{
	xrtHttpResponseDestroy(pResponse);
	if ( pTcp != NULL ) {
		(void)xrtNetStreamAbort(pTcp);
		xrtNetStreamDestroy(pTcp);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTls != NULL ) {
			(void)xrtTlsStreamAbort(pTls);
			xrtTlsStreamDestroy(pTls);
		}
	#endif
}



/* 初始化 Call 的无锁诊断快照和绝对截止时间。 */
static void __xrtHttpCallInfoInit(
	xhttpcall* pCall,
	uint64 iNow
)
{
	uint64 iTotalDeadline = 0;
	uint64 iIdleDeadline = 0;
	uint64 iCandidate;

	if ( pCall->Timeout != XHTTP_CLIENT_TIMEOUT_NONE ) {
		iTotalDeadline = __xrtHttpCallDeadline(
			iNow,
			pCall->Timeout
		);
	}
	if ( pCall->IdleTimeout !=
		XHTTP_CLIENT_TIMEOUT_NONE ) {
		iCandidate = __xrtHttpCallDeadline(
			iNow,
			pCall->IdleTimeout
		);
		if ( (iTotalDeadline == 0) ||
			(iCandidate < iTotalDeadline) ) {
			iIdleDeadline = iCandidate;
		}
	}
	pCall->TotalDeadline = iTotalDeadline;
	xrtAtomic32Init(
		&pCall->Info.Phase,
		XHTTP_CALL_PHASE_QUEUED
	);
	xrtAtomic32Init(
		&pCall->Info.Result,
		(uint32)(int32)XNET_RESULT_AGAIN
	);
	xrtAtomic32Init(&pCall->Info.Error, 0);
	xrtAtomic32Init(&pCall->Info.Reused, 0);
	xrtAtomic32Init(&pCall->Info.Secure, 0);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xrtAtomic32Init(
			&pCall->Info.Cache,
			XHTTP_CLIENT_CACHE_NONE
		);
	#endif
	xrtAtomic64Init(&pCall->Info.Submitted, iNow);
	xrtAtomic64Init(&pCall->Info.Started, 0);
	xrtAtomic64Init(&pCall->Info.TransportReady, 0);
	xrtAtomic64Init(&pCall->Info.RequestSent, 0);
	xrtAtomic64Init(&pCall->Info.FirstByte, 0);
	xrtAtomic64Init(&pCall->Info.Headers, 0);
	xrtAtomic64Init(&pCall->Info.LastProgress, iNow);
	xrtAtomic64Init(&pCall->Info.Completed, 0);
	xrtAtomic64Init(&pCall->Info.RequestWireBytes, 0);
	xrtAtomic64Init(&pCall->Info.ResponseWireBytes, 0);
	xrtAtomic64Init(&pCall->Info.ResponseBodyBytes, 0);
	xrtAtomic64Init(&pCall->Info.Redirects, 0);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtAtomic64Init(&pCall->Info.Retries, 0);
	#endif
	xrtAtomic64Init(&pCall->IdleDeadline, iIdleDeadline);
}



/* 把内部原子计数复制为公开值快照。 */
static void __xrtHttpCallInfoCopy(
	const xhttpcall* pCall,
	xhttpcallinfo* pInfo
)
{
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->State = (xhttpcallstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
	pInfo->Phase = (xhttpcallphase)xrtAtomic32Load(
		&pCall->Info.Phase,
		XMEMORY_ACQUIRE
	);
	pInfo->Result = (xnetresult)(int32)xrtAtomic32Load(
		&pCall->Info.Result,
		XMEMORY_ACQUIRE
	);
	pInfo->Error = (xhttpclienterror)xrtAtomic32Load(
		&pCall->Info.Error,
		XMEMORY_ACQUIRE
	);
	pInfo->Submitted = xrtAtomic64Load(
		&pCall->Info.Submitted,
		XMEMORY_ACQUIRE
	);
	pInfo->Started = xrtAtomic64Load(
		&pCall->Info.Started,
		XMEMORY_ACQUIRE
	);
	pInfo->TransportReady = xrtAtomic64Load(
		&pCall->Info.TransportReady,
		XMEMORY_ACQUIRE
	);
	pInfo->RequestSent = xrtAtomic64Load(
		&pCall->Info.RequestSent,
		XMEMORY_ACQUIRE
	);
	pInfo->FirstByte = xrtAtomic64Load(
		&pCall->Info.FirstByte,
		XMEMORY_ACQUIRE
	);
	pInfo->Headers = xrtAtomic64Load(
		&pCall->Info.Headers,
		XMEMORY_ACQUIRE
	);
	pInfo->LastProgress = xrtAtomic64Load(
		&pCall->Info.LastProgress,
		XMEMORY_ACQUIRE
	);
	pInfo->Completed = xrtAtomic64Load(
		&pCall->Info.Completed,
		XMEMORY_ACQUIRE
	);
	pInfo->RequestWireBytes = xrtAtomic64Load(
		&pCall->Info.RequestWireBytes,
		XMEMORY_ACQUIRE
	);
	pInfo->ResponseWireBytes = xrtAtomic64Load(
		&pCall->Info.ResponseWireBytes,
		XMEMORY_ACQUIRE
	);
	pInfo->ResponseBodyBytes = xrtAtomic64Load(
		&pCall->Info.ResponseBodyBytes,
		XMEMORY_ACQUIRE
	);
	pInfo->Redirects = (size_t)xrtAtomic64Load(
		&pCall->Info.Redirects,
		XMEMORY_ACQUIRE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		pInfo->Retries = (size_t)xrtAtomic64Load(
			&pCall->Info.Retries,
			XMEMORY_ACQUIRE
		);
	#endif
	pInfo->ReusedConnection = xrtAtomic32Load(
		&pCall->Info.Reused,
		XMEMORY_ACQUIRE
	) != 0;
	pInfo->Secure = xrtAtomic32Load(
		&pCall->Info.Secure,
		XMEMORY_ACQUIRE
	) != 0;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		pInfo->Cache =
			(xhttpclientcacheoutcome)xrtAtomic32Load(
				&pCall->Info.Cache,
				XMEMORY_ACQUIRE
			);
	#endif
}



/* 发布一次不可变终态并转移响应或升级传输。 */
static void __xrtHttpCallFinish(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xhttpresponse* pResponse,
	xnetstream* pTcp,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls,
	#endif
	size_t iBuffered,
	bool bUpgraded,
	xerror* pError
)
{
	xhttpcallresult CallResult;
	xhttp1exchange* pExchange;
	xcancelwatch* pWatch;
	uint32 iTimeout;
	xhttpcallstate State;
	xhttpclienterror TerminalCode;
	uint64 iDeliveredBody;
	uint64 iStoredBody;
	bool bCancelled;

	/*
		完成、显式取消和超时共用这条线性化边界。
		锁内只提交门状态和摘取所有权，不运行回调或释放资源。
	*/
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		__xrtHttpCallResultDestroy(
			pResponse,
			pTcp
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				, pTls
			#endif
		);
		xrtErrorFree(pError);
		return;
	}
	bCancelled = xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) != 0;
	iTimeout = xrtAtomic32Load(
		&pCall->TimeoutCause,
		XMEMORY_RELAXED
	);
	xrtAtomic32Store(
		&pCall->FinishGate,
		1,
		XMEMORY_RELEASE
	);
	pExchange = pCall->Exchange;
	pCall->Exchange = NULL;
	pWatch = pCall->CancelWatch;
	pCall->CancelWatch = NULL;
	(void)xrtSpinUnlock(&pCall->Lock);

	if ( bCancelled ) {
		__xrtHttpCallResultDestroy(
			pResponse,
			pTcp
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				, pTls
			#endif
		);
		pResponse = NULL;
		pTcp = NULL;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls = NULL;
		#endif
		iBuffered = 0;
		bUpgraded = false;
		if ( iTimeout == XRT_HTTP_TIMEOUT_IDLE ) {
			TerminalCode =
				XHTTP_CLIENT_ERROR_TIMEOUT_IDLE;
		} else if ( iTimeout ==
			XRT_HTTP_TIMEOUT_TOTAL ) {
			TerminalCode =
				XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL;
		} else {
			TerminalCode =
				XHTTP_CLIENT_ERROR_CANCELLED;
		}
		pError = __xrtHttpClientTerminalError(
			pError,
			TerminalCode
		);
		if ( iTimeout != XRT_HTTP_TIMEOUT_NONE ) {
			Result = XNET_RESULT_TIMEOUT;
		} else {
			Result = XNET_RESULT_CANCELLED;
		}
		Code = TerminalCode;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolFinish(pCall);
		(void)xrtAtomic64FetchAdd(
			&pCall->Client->RequestsCompleted,
			1,
			XMEMORY_RELAXED
		);
	#endif
	__xrtHttpCallCancelTimers(pCall);
	pCall->Error = pError;
	xrtCancelUnwatch(pWatch);
	xrtHttp1ExchangeDestroy(pExchange);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		/*
			重定向策略在终态前已经不再需要下一跳草案，及时释放它，
			避免用户长期保留已完成 Call 时继续占用请求快照。
		*/
		__xrtHttpRedirectUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		/*
			Cookie 已完成最后一次逐跳选择和响应存储，终态不再需要
			调用级分区键，避免保留 Call 时继续持有它的副本。
		*/
		__xrtHttpCookieUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		/*
			缓存写入和重放在终态前已经完成，及时释放正文与字段快照，
			避免用户长期保留已完成 Call 时继续占用临时内存。
		*/
		__xrtHttpClientCacheUnit(pCall);
	#endif

	if ( Result == XNET_RESULT_OK ) {
		State = XHTTP_CALL_SUCCEEDED;
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		State = XHTTP_CALL_CANCELLED;
	} else {
		State = XHTTP_CALL_FAILED;
	}
	xrtAtomic32Store(
		&pCall->Info.Result,
		(uint32)(int32)Result,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->Info.Error,
		(uint32)Code,
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(
		&pCall->Info.Completed,
		xrtClock(),
		XMEMORY_RELEASE
	);
	iDeliveredBody = xrtAtomic64Load(
		&pCall->Info.ResponseBodyBytes,
		XMEMORY_ACQUIRE
	);
	iStoredBody = pResponse != NULL ?
		xrtHttpResponseBodyBytes(pResponse) : 0;
	if ( iStoredBody > iDeliveredBody ) {
		xrtAtomic64Store(
			&pCall->Info.ResponseBodyBytes,
			iStoredBody,
			XMEMORY_RELEASE
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xrtAtomic64Store(
			&pCall->Info.Redirects,
			(uint64)pCall->Redirects,
			XMEMORY_RELEASE
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtAtomic64Store(
			&pCall->Info.Retries,
			(uint64)pCall->Retries,
			XMEMORY_RELEASE
		);
	#endif
	xrtAtomic32Store(
		&pCall->State,
		(uint32)State,
		XMEMORY_RELEASE
	);
	memset(&CallResult, 0, sizeof(CallResult));
	CallResult.Result = Result;
	CallResult.Response = pResponse;
	CallResult.Tcp = pTcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		CallResult.Tls = pTls;
	#endif
	CallResult.Error = pCall->Error;
	__xrtHttpCallInfoCopy(
		pCall,
		&CallResult.Info
	);
	CallResult.Buffered = iBuffered;
	CallResult.Upgraded = bUpgraded;
	pCall->Done(pCall, &CallResult, pCall->Data);
	__xrtHttpClientCallDetach(pCall);
	if ( pCall->RuntimeHeld ) {
		pCall->RuntimeHeld = false;
		xrtHttpCallDestroy(pCall);
	}
}



/* 发布带响应和可选升级传输的成功终态。 */
void __xrtHttpCallSucceed(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	xnetstream* pTcp,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls,
	#endif
	size_t iBuffered,
	bool bUpgraded
)
{
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		__xrtHttpResponseSetRedirects(
			pResponse,
			pCall->Redirects
		);
	#endif
	if ( pCall->Events.Body == NULL ) {
		__xrtHttpResponseSetBuffered(pResponse);
	}
	__xrtHttpCallFinish(
		pCall,
		XNET_RESULT_OK,
		XHTTP_CLIENT_ERROR_NONE,
		pResponse,
		pTcp,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls,
		#endif
		iBuffered,
		bUpgraded,
		NULL
	);
}



/* 发布一个没有响应或升级传输的失败终态。 */
void __xrtHttpCallFail(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError;
	uint32 iTimeout = xrtAtomic32Load(
		&pCall->TimeoutCause,
		XMEMORY_ACQUIRE
	);

	if ( iTimeout != XRT_HTTP_TIMEOUT_NONE ) {
		Result = XNET_RESULT_TIMEOUT;
		Code = iTimeout == XRT_HTTP_TIMEOUT_IDLE ?
			XHTTP_CLIENT_ERROR_TIMEOUT_IDLE :
			XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL;
		Kind = XERR_TIMEOUT;
		sOperation = "run-http-call";
		sMessage = iTimeout == XRT_HTTP_TIMEOUT_IDLE ?
			"HTTP call made no progress before its idle deadline" :
			"HTTP call exceeded its total deadline";
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( (iTimeout == XRT_HTTP_TIMEOUT_NONE) &&
			__xrtHttpRetryFailure(
				pCall,
				Result,
				Code,
				Kind,
				pCause
			) ) {
			return;
		}
	#endif
	pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	__xrtHttpCallFinish(
		pCall,
		Result,
		Code,
		NULL,
		NULL,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			NULL,
		#endif
		0,
		false,
		pError
	);
}



/* 任一截止时间到达后复用统一取消路径。 */
static bool __xrtHttpCallTimeout(
	xhttpcall* pCall,
	uint32 iCause
);



/* 总截止时间到达后复用统一取消路径。 */
static void __xrtHttpCallTotalTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(
		&pCall->TotalTimerDone,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic64Exchange(
		&pCall->TotalTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Result == XNET_RESULT_OK) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		(void)__xrtHttpCallTimeout(
			pCall,
			XRT_HTTP_TIMEOUT_TOTAL
		);
	} else if ( (Result == XNET_RESULT_ERROR) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"run-http-call-timeout",
			"HTTP total timeout timer failed",
			NULL
		);
	}
	xrtHttpCallDestroy(pCall);
}



/* 为现有 idle Timer 引用重新安排下一次 deadline。 */
static bool __xrtHttpCallIdleTimerAgain(
	xhttpcall* pCall,
	xdeadline iDeadline
);



/*
	Idle Timer 只在 deadline 到达时检查一次。
	期间任意真实 I/O 仅更新原子 deadline，不产生 Timer 或命令。
*/
static void __xrtHttpCallIdleTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xdeadline iDeadline;
	uint64 iNow;
	uint32 iCause;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(
		&pCall->IdleTimerDone,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic64Exchange(
		&pCall->IdleTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Result == XNET_RESULT_OK) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		iDeadline = xrtAtomic64Load(
			&pCall->IdleDeadline,
			XMEMORY_ACQUIRE
		);
		iNow = xrtClock();
		if ( iDeadline > iNow ) {
			if ( __xrtHttpCallIdleTimerAgain(
				pCall,
				iDeadline
			) ) {
				return;
			}
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"schedule-http-idle-timeout",
				"HTTP idle timeout timer could not be rescheduled",
				xrtGetError()
			);
		} else {
			iCause =
				(pCall->Timeout !=
				 XHTTP_CLIENT_TIMEOUT_NONE) &&
				(pCall->TotalDeadline <= iNow) ?
				XRT_HTTP_TIMEOUT_TOTAL :
				XRT_HTTP_TIMEOUT_IDLE;
			(void)__xrtHttpCallTimeout(pCall, iCause);
		}
	} else if ( (Result == XNET_RESULT_ERROR) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"run-http-idle-timeout",
			"HTTP idle timeout timer failed",
			NULL
		);
	}
	xrtHttpCallDestroy(pCall);
}



/* 把当前 Timer 持有的 Call 引用转移给下一次 idle deadline。 */
static bool __xrtHttpCallIdleTimerAgain(
	xhttpcall* pCall,
	xdeadline iDeadline
)
{
	uint64 Id;

	xrtAtomic32Store(
		&pCall->IdleTimerDone,
		0,
		XMEMORY_RELEASE
	);
	Id = xrtNetEngineSchedule(
		pCall->Client->Engine,
		pCall->Affinity,
		iDeadline,
		__xrtHttpCallIdleTimer,
		pCall
	);
	if ( Id == 0 ) {
		xrtAtomic32Store(
			&pCall->IdleTimerDone,
			1,
			XMEMORY_RELEASE
		);
		return false;
	}
	xrtAtomic64Store(
		&pCall->IdleTimer,
		Id,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pCall->IdleTimerDone,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtAtomic64Exchange(
			&pCall->IdleTimer,
			0,
			XMEMORY_ACQ_REL
		);
	}
	return true;
}



/* 外部取消令牌只请求 Call 取消，不直接接触阶段对象。 */
static void __xrtHttpCallCancelled(ptr pData)
{
	(void)xrtHttpCallCancel((xhttpcall*)pData);
}



/* 在连接池选定的 Worker 上复用传输或启动新拨号。 */
static void __xrtHttpCallStartTransport(xhttpcall* pCall)
{
	xerror* pCause;
	bool bStarted;

	/*
		Dial 返回值和完成回调的交接依赖同 Worker 不重入契约。
		禁止未来从提交线程或错误的 Worker 直接进入本函数。
	*/
	if ( (pCall->Worker == NULL) ||
		!xrtNetWorkerIsCurrent(pCall->Worker) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_STATE,
			"start-http-transport",
			"HTTP transport must start on the call worker",
			NULL
		);
		return;
	}
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"start-http-call",
			"HTTP call was cancelled before dialing",
			NULL
		);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( pCall->PooledTcp != NULL ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_REQUEST
			);
			bStarted =
				__xrtHttpCallStartPooledTcp(pCall);
		} else if (
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				pCall->PooledTls != NULL
			#else
				false
			#endif
		) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				__xrtHttpCallSetPhase(
					pCall,
					XHTTP_CALL_PHASE_REQUEST
				);
				bStarted =
					__xrtHttpCallStartPooledTls(
						pCall
					);
			#else
				bStarted = false;
			#endif
		} else
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pCall->Proxy != NULL ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_PROXY
			);
			bStarted = __xrtHttpCallStartProxy(pCall);
		} else
	#endif
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_TLS
			);
			bStarted = __xrtHttpCallStartTls(pCall);
		#else
			bStarted = false;
			__xrtHttpClientSetError(
				XERR_UNSUPPORTED,
				XHTTP_CLIENT_ERROR_TLS,
				"start-http-call",
				"HTTPS support is not present in this build",
				NULL
			);
		#endif
	} else {
		__xrtHttpCallSetPhase(
			pCall,
			XHTTP_CALL_PHASE_CONNECT
		);
		bStarted = __xrtHttpCallStartTcp(pCall);
	}
	if ( !bStarted ) {
		pCause = xrtTakeError();
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				pCall->Proxy != NULL ?
					XHTTP_CLIENT_ERROR_PROXY :
			#endif
			(pCall->Secure ?
				XHTTP_CLIENT_ERROR_TLS :
				XHTTP_CLIENT_ERROR_DIAL),
			__xrtHttpClientCauseKind(
				pCause,
				XERR_IO
			),
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				pCall->Proxy != NULL ?
					"dial-http-proxy" :
			#endif
			(pCall->Secure ?
				"dial-https" :
				"dial-http"),
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				pCall->Proxy != NULL ?
					"HTTP proxy transport could not start" :
			#endif
			(pCall->Secure ?
				"HTTPS transport could not start" :
				"HTTP transport could not start"),
			pCause
		);
		xrtErrorFree(pCause);
	}
}



/* 获取当前 origin 的连接配额，然后启动或等待这一跳传输。 */
void __xrtHttpCallStartHop(xhttpcall* pCall)
{
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xerror* pCacheCause;
		bool bCacheHandled;

		if ( !__xrtHttpClientCacheStart(
			pCall,
			&bCacheHandled
		) ) {
			pCacheCause = xrtTakeError();
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_CACHE,
				__xrtHttpClientCauseKind(
					pCacheCause,
					XERR_MEMORY
				),
				"deliver-http-cache-response",
				"HTTP cached response could not be rebuilt",
				pCacheCause
			);
			xrtErrorFree(pCacheCause);
			return;
		}
		if ( bCacheHandled ) {
			return;
		}
	#endif
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_POOL
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		xerror* pCause;
		bool bReady;

		if ( !__xrtHttpPoolAcquire(
			pCall,
			&bReady
		) ) {
			pCause = xrtTakeError();
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_POOL,
				(pCause != NULL) &&
				(xrtErrorIs(
					pCause,
					XERR_MEMORY
				) != NULL) ?
					XERR_MEMORY : XERR_AGAIN,
				"acquire-http-connection",
				"HTTP connection pool could not schedule the call",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
		if ( !bReady ) {
			return;
		}
	#endif
	__xrtHttpCallStartTransport(pCall);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 连接池唤醒后在新的 Stream Worker 上继续执行 Call。 */
void __xrtHttpCallPoolReady(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	(void)pWorker;
	__xrtHttpCallStartTransport(pCall);
}

#endif



/* 在目标 Worker 上装配取消、总 Timer 并进入池或实际拨号。 */
static void __xrtHttpCallStart(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xerror* pCause;
	uint64 iNow;
	uint64 Id;

	(void)pWorker;
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"start-http-call",
			"HTTP call was cancelled before dialing",
			NULL
		);
		return;
	}
	if ( pCall->Cancel != NULL ) {
		pCall->CancelWatch = xrtCancelWatch(
			pCall->Cancel,
			__xrtHttpCallCancelled,
			pCall
		);
		if ( pCall->CancelWatch == NULL ) {
			pCause = xrtTakeError();
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"watch-http-call-cancel",
				"HTTP call could not observe its cancellation token",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
	}
	iNow = xrtClock();
	(void)__xrtHttpCallFirstTime(
		&pCall->Info.Started,
		iNow
	);
	__xrtHttpCallProgressAt(pCall, iNow);
	if ( pCall->Timeout != XHTTP_CLIENT_TIMEOUT_NONE ) {
		if ( xrtHttpCallRef(pCall) == NULL ) {
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_STATE,
				"schedule-http-call-timeout",
				"HTTP call timer could not retain the call",
				xrtGetError()
			);
			return;
		}
		xrtAtomic32Store(
			&pCall->TotalTimerDone,
			0,
			XMEMORY_RELEASE
		);
		Id = xrtNetEngineSchedule(
			pCall->Client->Engine,
			pCall->Affinity,
			pCall->TotalDeadline,
			__xrtHttpCallTotalTimer,
			pCall
		);
		if ( Id == 0 ) {
			pCause = xrtTakeError();
			xrtHttpCallDestroy(pCall);
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"schedule-http-call-timeout",
				"HTTP call total deadline could not be scheduled",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
		xrtAtomic64Store(
			&pCall->TotalTimer,
			Id,
			XMEMORY_RELEASE
		);
		if ( xrtAtomic32Load(
			&pCall->TotalTimerDone,
			XMEMORY_ACQUIRE
		) ) {
			(void)xrtAtomic64Exchange(
				&pCall->TotalTimer,
				0,
				XMEMORY_ACQ_REL
			);
		}
	}
	if ( (pCall->IdleTimeout !=
		XHTTP_CLIENT_TIMEOUT_NONE) &&
		((pCall->Timeout ==
		  XHTTP_CLIENT_TIMEOUT_NONE) ||
		 (xrtAtomic64Load(
			&pCall->IdleDeadline,
			XMEMORY_ACQUIRE
		  ) < pCall->TotalDeadline)) ) {
		if ( xrtHttpCallRef(pCall) == NULL ) {
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_STATE,
				"schedule-http-idle-timeout",
				"HTTP idle timer could not retain the call",
				xrtGetError()
			);
			return;
		}
		xrtAtomic32Store(
			&pCall->IdleTimerDone,
			0,
			XMEMORY_RELEASE
		);
		Id = xrtNetEngineSchedule(
			pCall->Client->Engine,
			pCall->Affinity,
			xrtAtomic64Load(
				&pCall->IdleDeadline,
				XMEMORY_ACQUIRE
			),
			__xrtHttpCallIdleTimer,
			pCall
		);
		if ( Id == 0 ) {
			pCause = xrtTakeError();
			xrtHttpCallDestroy(pCall);
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"schedule-http-idle-timeout",
				"HTTP call idle deadline could not be scheduled",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
		xrtAtomic64Store(
			&pCall->IdleTimer,
			Id,
			XMEMORY_RELEASE
		);
		if ( xrtAtomic32Load(
			&pCall->IdleTimerDone,
			XMEMORY_ACQUIRE
		) ) {
			(void)xrtAtomic64Exchange(
				&pCall->IdleTimer,
				0,
				XMEMORY_ACQ_REL
			);
		}
	}
	__xrtHttpCallStartHop(pCall);
}



/* 复制一个计划拥有的 Host 视图并追加 C 字符串终止符。 */
static str __xrtHttpCallHost(xstrview Host)
{
	str sHost;

	if ( (Host.Data == NULL) || (Host.Size == 0) ||
		(Host.Size == SIZE_MAX) ) {
		__xrtHttpClientSetError(
			XERR_VALUE,
			XHTTP_CLIENT_ERROR_REQUEST,
			"prepare-http-call",
			"HTTP request plan has no usable host",
			NULL
		);
		return NULL;
	}
	sHost = (str)xrtMalloc(Host.Size + 1u);
	if ( sHost == NULL ) {
		return NULL;
	}
	memcpy(sHost, Host.Data, Host.Size);
	sHost[Host.Size] = 0;
	return sHost;
}



/* 记录信息响应到达，并保持原事件链的返回语义。 */
static bool __xrtHttpCallInfoInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	(void)__xrtHttpCallFirstTime(
		&pCall->Info.FirstByte,
		xrtClock()
	);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_RESPONSE_HEADERS
	);
	if ( pCall->Events.Informational == NULL ) {
		return true;
	}
	return pCall->Events.Informational(
		pCall,
		pResponse,
		pCall->Events.Data
	);
}



/* 记录首个最终响应 Header，并保持原事件链的返回语义。 */
static bool __xrtHttpCallInfoHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	uint64 iNow = xrtClock();

	(void)__xrtHttpCallFirstTime(
		&pCall->Info.FirstByte,
		iNow
	);
	(void)__xrtHttpCallFirstTime(
		&pCall->Info.Headers,
		iNow
	);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_RESPONSE_HEADERS
	);
	if ( pCall->Events.Headers == NULL ) {
		return true;
	}
	return pCall->Events.Headers(
		pCall,
		pResponse,
		pCall->Events.Data
	);
}



/* 记录已经接受的响应正文，并保留缓冲或流式交付策略。 */
static bool __xrtHttpCallInfoBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	bool bAccepted;

	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_RESPONSE_BODY
	);
	if ( pCall->Events.Body == NULL ) {
		bAccepted = __xrtHttpResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	} else {
		bAccepted = pCall->Events.Body(
			pCall,
			pResponse,
			Data,
			pCall->Events.Data
		);
	}
	if ( bAccepted ) {
		(void)xrtAtomic64FetchAdd(
			&pCall->Info.ResponseBodyBytes,
			(uint64)Data.Size,
			XMEMORY_RELAXED
		);
	}
	return bAccepted;
}



/* 把诊断观察器放在用户事件外层，统计用户最终看到的响应字节。 */
static const xhttp1exchangeevents* __xrtHttpCallInfoEvents(
	xhttpcall* pCall
)
{
	memset(&pCall->InfoEvents, 0, sizeof(pCall->InfoEvents));
	pCall->InfoEvents.Informational =
		__xrtHttpCallInfoInformational;
	pCall->InfoEvents.Headers = __xrtHttpCallInfoHeaders;
	pCall->InfoEvents.Body = __xrtHttpCallInfoBody;
	pCall->InfoEvents.Data = pCall;
	return &pCall->InfoEvents;
}



/* 失败原子地建立一跳请求所需的 Plan、Exchange 和传输端点。 */
bool __xrtHttpCallPrepareHop(xhttpcall* pCall)
{
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	xhttp1exchangeconfig ExchangeConfig;
	const xhttp1exchangeevents* pEvents;
	xstrview Host;
	str sHost;
	uint16 iPort;
	bool bSecure;

	if ( (pCall == NULL) || (pCall->Request == NULL) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"prepare-http-call",
			"HTTP call and request are required",
			NULL
		);
		return false;
	}
	xrtAtomic64Store(
		&pCall->Info.ResponseBodyBytes,
		0,
		XMEMORY_RELEASE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		__xrtHttpDecompressReset(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		if ( !__xrtHttpCookiePrepare(pCall) ) {
			return false;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCachePrepare(pCall) ) {
			return false;
		}
	#endif
	pEvents = __xrtHttpCallInfoEvents(pCall);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		pEvents = __xrtHttpDecompressEvents(
			pCall,
			pEvents
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		pEvents = __xrtHttpRedirectEvents(
			pCall,
			pEvents
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		pEvents = __xrtHttpClientCacheEvents(
			pCall,
			pEvents
		);
		if ( pCall->CacheReady ) {
			xrtHttp1ExchangeDestroy(pCall->Exchange);
			xrtFree(pCall->Host);
			pCall->Exchange = NULL;
			pCall->Host = NULL;
			pCall->Port = 0;
			return true;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		pEvents = __xrtHttpRetryEvents(pCall, pEvents);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		pEvents = __xrtHttpCookieEvents(pCall, pEvents);
	#endif
	pPlan = xrtHttp1RequestPrepare(
		pCall->Request,
		&pCall->RequestOptions
	);
	if ( pPlan == NULL ) {
		xerror* pError = __xrtHttpClientPromoteOwned(
			xrtTakeError(),
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_VALUE,
			"prepare-http-request",
			"HTTP request could not be prepared for HTTP/1"
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
		return false;
	}
	Host = xrtHttp1RequestPlanHost(pPlan);
	sHost = __xrtHttpCallHost(Host);
	if ( sHost == NULL ) {
		xerror* pError = __xrtHttpClientPromoteOwned(
			xrtTakeError(),
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_MEMORY,
			"prepare-http-request",
			"HTTP request host could not be retained"
		);

		xrtHttp1RequestPlanDestroy(pPlan);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return false;
	}
	iPort = xrtHttp1RequestPlanPort(pPlan);
	bSecure = xrtHttp1RequestPlanSecure(pPlan);
	#if !defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( bSecure ) {
			xrtFree(sHost);
			xrtHttp1RequestPlanDestroy(pPlan);
			__xrtHttpClientSetError(
				XERR_UNSUPPORTED,
				XHTTP_CLIENT_ERROR_TLS,
				"prepare-http-call",
				"HTTPS support is not present in this build",
				NULL
			);
			return false;
		}
	#endif
	ExchangeConfig = pCall->Client->Config.Exchange;
	ExchangeConfig.Body.MaxBody = pCall->ResponseBodyLimit;
	pExchange = xrtHttp1ExchangeCreate(
		pPlan,
		&ExchangeConfig,
		pEvents
	);
	if ( pExchange == NULL ) {
		xerror* pError = __xrtHttpClientPromoteOwned(
			xrtTakeError(),
			XHTTP_CLIENT_ERROR_RESPONSE,
			XERR_MEMORY,
			"prepare-http-response",
			"HTTP response exchange could not be initialized"
		);

		xrtFree(sHost);
		xrtHttp1RequestPlanDestroy(pPlan);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return false;
	}
	xrtHttp1ExchangeDestroy(pCall->Exchange);
	xrtFree(pCall->Host);
	pCall->Exchange = pExchange;
	pCall->Host = sHost;
	pCall->Port = iPort;
	pCall->Secure = bSecure;
	xrtAtomic32Store(
		&pCall->Info.Secure,
		bSecure ? 1u : 0u,
		XMEMORY_RELEASE
	);
	return true;
}



XRT_API xhttpcall* xrtHttpClientDo(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	xhttpcalloptions Options;
	xhttpclient* pClientRef;
	xhttpcall* pCall;
	uint64 iAffinity;
	uint64 iNow;
	uint32 iWorkers;

	if ( (pClient == NULL) || (pRequest == NULL) ||
		(pDone == NULL) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"run-http-client",
			"HTTP client, request and completion callback are required",
			NULL
		);
		return NULL;
	}
	if ( xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	) != XHTTP_CLIENT_RUNNING ) {
		__xrtHttpClientSetError(
			XERR_CLOSED,
			XHTTP_CLIENT_ERROR_STATE,
			"run-http-client",
			"HTTP client is not accepting new calls",
			NULL
		);
		return NULL;
	}
	xrtHttpCallOptionsInit(&Options);
	if ( pOptions != NULL ) {
		if ( !xrtMemRangeValid(pOptions, sizeof(*pOptions)) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				"run-http-client",
				"HTTP call options range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Options, pOptions, sizeof(Options));
	}
	pClientRef = __xrtHttpClientHold(pClient);
	if ( pClientRef == NULL ) {
		return NULL;
	}
	pCall = (xhttpcall*)xrtCalloc(1, sizeof(*pCall));
	if ( pCall == NULL ) {
		__xrtHttpClientRelease(pClientRef);
		return NULL;
	}
	pCall->References = 1;
	pCall->Client = pClientRef;
	pCall->Timeout = Options.Timeout == 0 ?
		pClient->Config.Timeout : Options.Timeout;
	pCall->IdleTimeout = Options.IdleTimeout == 0 ?
		pClient->Config.IdleTimeout : Options.IdleTimeout;
	pCall->ResponseBodyLimit = Options.ResponseBodyLimit == 0 ?
		pClient->Config.Exchange.Body.MaxBody :
		Options.ResponseBodyLimit;
	xrtAtomic32Init(&pCall->State, XHTTP_CALL_QUEUED);
	xrtAtomic32Init(&pCall->CancelGate, 0);
	xrtAtomic32Init(&pCall->FinishGate, 0);
	xrtAtomic32Init(
		&pCall->TimeoutCause,
		XRT_HTTP_TIMEOUT_NONE
	);
	xrtAtomic32Init(&pCall->TotalTimerDone, 0);
	xrtAtomic32Init(&pCall->IdleTimerDone, 0);
	xrtAtomic64Init(&pCall->TotalTimer, 0);
	xrtAtomic64Init(&pCall->IdleTimer, 0);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtAtomic32Init(&pCall->RetryTimerDone, 0);
		xrtAtomic64Init(&pCall->RetryTimer, 0);
	#endif
	iNow = xrtClock();
	__xrtHttpCallInfoInit(pCall, iNow);
	if ( !xrtSpinInit(&pCall->Lock) ) {
		__xrtHttpClientRelease(pCall->Client);
		xrtFree(pCall);
		return NULL;
	}
	if ( !xrtNetPostInit(&pCall->StartPost) ) {
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"initialize-http-call-post",
			"HTTP call could not initialize its Worker post"
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( !xrtNetPostInit(&pCall->PoolPost) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"initialize-http-pool-post",
				"HTTP call could not initialize its pool post"
			);
		}
	#endif
	pCall->Done = pDone;
	pCall->Data = pData;
	pCall->Request = xrtHttpRequestClone(pRequest);
	if ( pCall->Request == NULL ) {
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_MEMORY,
			"freeze-http-request",
			"HTTP request could not be frozen for asynchronous execution"
		);
	}
	pCall->RequestOptions = Options.Request;
	pCall->Events = Options.Events;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( !__xrtHttpRetryInit(pCall, &Options) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_RETRY,
				XERR_ARGUMENT,
				"configure-http-retry",
				"HTTP retry policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( !__xrtHttpDecompressInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_DECOMPRESSION,
				XERR_ARGUMENT,
				"configure-http-decompression",
				"HTTP response decompression policy is invalid"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCacheInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_CACHE,
				XERR_ARGUMENT,
				"configure-http-cache",
				"HTTP cache policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( !__xrtHttpProxyInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_PROXY,
				XERR_ARGUMENT,
				"configure-http-proxy",
				"HTTP proxy policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( !__xrtHttpRedirectInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_REDIRECT,
				XERR_ARGUMENT,
				"configure-http-redirect",
				"HTTP redirect policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		if ( !__xrtHttpCookieInit(pCall, &Options) ) {
			__xrtHttpCookieSetSubmitError(pCall);
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_COOKIE,
				XERR_ARGUMENT,
				"configure-http-cookie",
				"HTTP Cookie policy could not be frozen"
			);
		}
	#endif
	if ( !__xrtHttpCallPrepareHop(pCall) ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
			__xrtHttpCookieSetSubmitError(pCall);
		#endif
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_VALUE,
			"prepare-http-call",
			"HTTP call could not prepare its initial request"
		);
	}
	if ( Options.Cancel != NULL ) {
		pCall->Cancel = xrtCancelRef(Options.Cancel);
		if ( pCall->Cancel == NULL ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				XERR_ARGUMENT,
				"retain-http-cancel",
				"HTTP call cancellation token could not be retained"
			);
		}
		if ( xrtCancelRequested(pCall->Cancel) ) {
			xrtAtomic32Store(
				&pCall->CancelGate,
				1,
				XMEMORY_RELEASE
			);
		}
	}
	iWorkers = xrtNetEngineWorkerCount(pClient->Engine);
	if ( iWorkers == 0 ) {
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_CONFIG,
			"run-http-client",
			"HTTP client engine has no active worker",
			NULL
		);
		xrtHttpCallDestroy(pCall);
		return NULL;
	}
	iAffinity = xrtAtomic64FetchAdd(
		&pClient->NextAffinity,
		1,
		XMEMORY_ACQ_REL
	);
	pCall->Affinity = iAffinity;
	pCall->Worker = xrtNetEngineWorker(
		pClient->Engine,
		(uint32)(iAffinity % iWorkers)
	);
	pCall->References = 2;
	pCall->RuntimeHeld = true;
	if ( !__xrtHttpClientCallAttach(pCall) ) {
		pCall->RuntimeHeld = false;
		pCall->References = 1;
		xrtHttpCallDestroy(pCall);
		return NULL;
	}
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->StartPost,
		__xrtHttpCallStart,
		pCall
	) ) {
		__xrtHttpClientCallDetach(pCall);
		pCall->RuntimeHeld = false;
		xrtHttpCallDestroy(pCall);
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_CLOSED,
			"post-http-call",
			"HTTP call could not enter its network Worker"
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtAtomic64FetchAdd(
			&pClient->RequestsStarted,
			1,
			XMEMORY_RELAXED
		);
	#endif
	return pCall;
}



/* 把已经取得 CancelGate 的取消请求转发给当前有效阶段。 */
static void __xrtHttpCallCancelActive(xhttpcall* pCall)
{
	xnetdial* pTcpDial = NULL;
	xhttp1call* pStreamCall = NULL;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxydial* pProxyDial = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsdial* pTlsDial = NULL;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			xtlsstream* pProxyTls = NULL;
		#endif
	#endif

	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->TcpDial != NULL ) {
		pTcpDial = xrtNetDialRef(pCall->TcpDial);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pCall->ProxyDial != NULL ) {
			pProxyDial = xrtNetProxyDialRef(
				pCall->ProxyDial
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pCall->TlsDial != NULL ) {
			pTlsDial = xrtTlsDialRef(pCall->TlsDial);
		}
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			if ( pCall->ProxyTls != NULL ) {
				pProxyTls = xrtTlsStreamRef(
					pCall->ProxyTls
				);
			}
		#endif
	#endif
	if ( pCall->StreamCall != NULL ) {
		pStreamCall = xrtHttp1CallRef(pCall->StreamCall);
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( pTcpDial != NULL ) {
		(void)xrtNetDialCancel(pTcpDial);
		xrtNetDialDestroy(pTcpDial);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pProxyDial != NULL ) {
			(void)xrtNetProxyDialCancel(pProxyDial);
			xrtNetProxyDialDestroy(pProxyDial);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTlsDial != NULL ) {
			(void)xrtTlsDialCancel(pTlsDial);
			xrtTlsDialDestroy(pTlsDial);
		}
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			if ( pProxyTls != NULL ) {
				(void)xrtTlsStreamAbort(pProxyTls);
				xrtTlsStreamDestroy(pProxyTls);
			}
		#endif
	#endif
	if ( pStreamCall != NULL ) {
		(void)xrtHttp1CallCancel(pStreamCall);
		xrtHttp1CallDestroy(pStreamCall);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		(void)__xrtHttpRetryCancel(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)__xrtHttpPoolCancel(pCall);
	#endif
}



/*
	原子进入异常终止状态并保留全部需要取消的活动 Call。
	Call 引用组成锁外临时链，避免取消回调在 Client 锁内重入。
*/
static xhttpcall* __xrtHttpClientAbortCalls(
	xhttpclient* pClient,
	bool* pAccepted
)
{
	xhttpclientstate State;
	xhttpcall* pAbort = NULL;

	(void)xrtSpinLock(&pClient->LifecycleLock);
	State = (xhttpclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	*pAccepted = (State == XHTTP_CLIENT_RUNNING) ||
		(State == XHTTP_CLIENT_DRAINING) ||
		(State == XHTTP_CLIENT_ABORTING) ||
		(State == XHTTP_CLIENT_CLOSED);
	if ( (State == XHTTP_CLIENT_RUNNING) ||
		(State == XHTTP_CLIENT_DRAINING) ) {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_CLIENT_ABORTING,
			XMEMORY_RELEASE
		);
		for ( xhttpcall* pCall = pClient->CallHead;
			pCall != NULL;
			pCall = pCall->ClientNext ) {
			bool bCancel = false;

			(void)xrtSpinLock(&pCall->Lock);
			if ( !xrtAtomic32Load(
				&pCall->FinishGate,
				XMEMORY_RELAXED
			) && !xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_RELAXED
			) ) {
				xrtAtomic32Store(
					&pCall->CancelGate,
					1,
					XMEMORY_RELEASE
				);
				bCancel = true;
			}
			(void)xrtSpinUnlock(&pCall->Lock);
			if ( bCancel ) {
				(void)xrtRefRetain(&pCall->References);
				pCall->AbortNext = pAbort;
				pAbort = pCall;
			}
		}
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	return pAbort;
}



/* 停止 Client 并协作取消全部已提交 Call。 */
XRT_API bool xrtHttpClientAbort(xhttpclient* pClient)
{
	xhttpcall* pAbort;
	bool bAccepted;

	if ( pClient == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"abort-http-client",
			"HTTP client is null",
			NULL
		);
		return false;
	}
	pAbort = __xrtHttpClientAbortCalls(
		pClient,
		&bAccepted
	);
	if ( !bAccepted ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtHttpClientCloseIdle(pClient);
	#endif
	while ( pAbort != NULL ) {
		xhttpcall* pNext = pAbort->AbortNext;

		pAbort->AbortNext = NULL;
		__xrtHttpCallCancelActive(pAbort);
		xrtHttpCallDestroy(pAbort);
		pAbort = pNext;
	}
	__xrtHttpClientTryFinish(pClient);
	return true;
}



/* 原子决定超时与显式取消的胜者，再执行统一取消分发。 */
static bool __xrtHttpCallTimeout(
	xhttpcall* pCall,
	uint32 iCause
)
{
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
	xrtAtomic32Store(
		&pCall->TimeoutCause,
		iCause,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->CancelGate,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	__xrtHttpCallCancelActive(pCall);
	return true;
}



/* 从任意线程把显式取消请求转发给当前有效阶段。 */
XRT_API bool xrtHttpCallCancel(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"cancel-http-call",
			"HTTP call is null",
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
	xrtAtomic32Store(
		&pCall->CancelGate,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	__xrtHttpCallCancelActive(pCall);
	return true;
}



/* 在线性化锁内取得当前低级 Call 的独立引用。 */
static xhttp1call* __xrtHttpCallStreamRef(xhttpcall* pCall)
{
	xhttp1call* pStreamCall = NULL;

	(void)xrtSpinLock(&pCall->Lock);
	if ( !xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) && (pCall->StreamCall != NULL) ) {
		pStreamCall = xrtHttp1CallRef(pCall->StreamCall);
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	return pStreamCall;
}



/* 把 Worker 内暂停请求转发给当前 HTTP/1 调用。 */
XRT_API bool xrtHttpCallPause(xhttpcall* pCall)
{
	xhttp1call* pStreamCall;
	bool bResult;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"pause-http-call",
			"HTTP call is null",
			NULL
		);
		return false;
	}
	pStreamCall = __xrtHttpCallStreamRef(pCall);
	if ( pStreamCall == NULL ) {
		return false;
	}
	bResult = xrtHttp1CallPause(pStreamCall);
	xrtHttp1CallDestroy(pStreamCall);
	return bResult;
}



/* 把跨线程恢复请求转发给当前 HTTP/1 调用。 */
XRT_API bool xrtHttpCallResume(xhttpcall* pCall)
{
	xhttp1call* pStreamCall;
	bool bResult;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"resume-http-call",
			"HTTP call is null",
			NULL
		);
		return false;
	}
	pStreamCall = __xrtHttpCallStreamRef(pCall);
	if ( pStreamCall == NULL ) {
		return false;
	}
	bResult = xrtHttp1CallResume(pStreamCall);
	xrtHttp1CallDestroy(pStreamCall);
	return bResult;
}



/* 查询当前低级 Call 的暂停快照。 */
XRT_API bool xrtHttpCallPaused(const xhttpcall* pCall)
{
	xhttp1call* pStreamCall;
	bool bPaused;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-pause",
			"HTTP call is null",
			NULL
		);
		return false;
	}
	pStreamCall = __xrtHttpCallStreamRef((xhttpcall*)pCall);
	if ( pStreamCall == NULL ) {
		return false;
	}
	bPaused = xrtHttp1CallPaused(pStreamCall);
	xrtHttp1CallDestroy(pStreamCall);
	return bPaused;
}



/* 返回创建 Call 时选择且不再变化的网络 Worker。 */
XRT_API xnetworker* xrtHttpCallWorker(const xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-worker",
			"HTTP call is null",
			NULL
		);
		return NULL;
	}
	return pCall->Worker;
}



/* 在 Call 锁内克隆当前有效请求，避免与重定向替换并发。 */
XRT_API xhttprequest* xrtHttpCallRequestClone(
	const xhttpcall* pCall
)
{
	xhttpcall* pMutable = (xhttpcall*)pCall;
	xhttprequest* pRequest;
	xerror* pCause;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"clone-http-call-request",
			"HTTP call is null",
			NULL
		);
		return NULL;
	}
	(void)xrtSpinLock(&pMutable->Lock);
	pRequest = xrtHttpRequestClone(pCall->Request);
	(void)xrtSpinUnlock(&pMutable->Lock);
	if ( pRequest == NULL ) {
		pCause = xrtTakeError();
		__xrtHttpClientSetError(
			XERR_MEMORY,
			XHTTP_CLIENT_ERROR_REQUEST,
			"clone-http-call-request",
			"HTTP call request could not be cloned",
			pCause
		);
		xrtErrorFree(pCause);
	}
	return pRequest;
}



/* 返回 Call 状态快照。 */
XRT_API xhttpcallstate xrtHttpCallState(const xhttpcall* pCall)
{
	xhttpcallstate State;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-state",
			"HTTP call is null",
			NULL
		);
		return XHTTP_CALL_FAILED;
	}
	State = (xhttpcallstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( (State == XHTTP_CALL_DIALING) &&
			(xrtAtomic32Load(
				&pCall->Info.Secure,
				XMEMORY_ACQUIRE
			) != 0) ) {
			xhttpcall* pMutable = (xhttpcall*)pCall;
			xtlsdial* pDial = NULL;

			(void)xrtSpinLock(&pMutable->Lock);
			if ( pMutable->TlsDial != NULL ) {
				pDial = xrtTlsDialRef(
					pMutable->TlsDial
				);
			}
			(void)xrtSpinUnlock(&pMutable->Lock);
			if ( pDial != NULL ) {
				if ( xrtTlsDialState(pDial) ==
					XTLS_DIAL_HANDSHAKE ) {
					State =
						XHTTP_CALL_HANDSHAKING;
				}
				xrtTlsDialDestroy(pDial);
			}
		}
	#endif
	return State;
}



/* 返回 Call 保存的终态错误。 */
XRT_API const xerror* xrtHttpCallError(const xhttpcall* pCall)
{
	xhttpcallstate State;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-error",
			"HTTP call is null",
			NULL
		);
		return NULL;
	}
	State = (xhttpcallstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
	if ( State < XHTTP_CALL_SUCCEEDED ) {
		return NULL;
	}
	return pCall->Error;
}



/* 复制 Call 当前的一致终态或近似并发运行快照。 */
XRT_API bool xrtHttpCallInfo(
	const xhttpcall* pCall,
	xhttpcallinfo* pInfo
)
{
	xhttpcallinfo Info;

	if ( (pCall == NULL) ||
		!xrtMemRangeValid(pInfo, sizeof(Info)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-info",
			"HTTP call and complete info storage are required",
			NULL
		);
		return false;
	}
	__xrtHttpCallInfoCopy(pCall, &Info);
	memcpy(pInfo, &Info, sizeof(Info));
	return true;
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 返回 Client 保留的默认代理借用引用。 */
XRT_API const xnetproxy* xrtHttpClientProxy(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Proxy : NULL;
}

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/* 返回 Client 保留的共享 CookieJar。 */
XRT_API xcookiejar* xrtHttpClientCookieJar(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Cookies : NULL;
}

#endif



#endif
