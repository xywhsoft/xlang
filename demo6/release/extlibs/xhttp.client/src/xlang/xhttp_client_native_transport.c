#include "xhttp_client_native_internal.h"



typedef struct xhttp_client_future_mapper {
	xvalue* Boxer;
} xhttp_client_future_mapper;



/* 把语言层毫秒转换为 XRT 微秒，并保留零继承与负一无限语义。 */
static bool xhttpClientDuration(
	long long iMilliseconds,
	uint64* pMicroseconds
)
{
	if ( pMicroseconds == NULL ) {
		return false;
	}
	if ( iMilliseconds == -1 ) {
		*pMicroseconds = UINT64_MAX;
		return true;
	}
	if ( (iMilliseconds < 0) ||
		((uint64)iMilliseconds > (UINT64_MAX / UINT64_C(1000))) ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"convert-http-duration",
			"HTTP timeout must be zero, positive or -1"
		);
		return false;
	}
	*pMicroseconds = (uint64)iMilliseconds * UINT64_C(1000);
	return true;
}



/* 根据语言层参数建立一次调用的完整选项快照。 */
static bool xhttpClientOptions(
	xhttpcalloptions* pOptions,
	long long iTimeoutMilliseconds,
	long long iIdleTimeoutMilliseconds,
	long long iResponseBodyLimit
)
{
	uint64 iTimeout;
	uint64 iIdleTimeout;

	if ( (pOptions == NULL) ||
		!xhttpClientDuration(iTimeoutMilliseconds, &iTimeout) ||
		!xhttpClientDuration(iIdleTimeoutMilliseconds, &iIdleTimeout) ) {
		return false;
	}
	if ( iResponseBodyLimit < -1 ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"configure-http-call",
			"HTTP response body limit must be non-negative or -1"
		);
		return false;
	}
	xrtHttpCallOptionsInit(pOptions);
	pOptions->Timeout = iTimeout;
	pOptions->IdleTimeout = iIdleTimeout;
	pOptions->ResponseBodyLimit = iResponseBodyLimit == -1 ?
		UINT64_MAX : (uint64)iResponseBodyLimit;
	return true;
}



/* 检查 Client 原生句柄。 */
static xhttpclient* xhttpClientRequire(void* pHandle)
{
	if ( pHandle == NULL ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"access-http-client",
			"HTTP client handle is invalid"
		);
		return NULL;
	}
	return (xhttpclient*)pHandle;
}



/* 检查拥有型调用结果句柄。 */
static xhttpresult* xhttpClientResultRequire(void* pHandle)
{
	if ( pHandle == NULL ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"access-http-result",
			"HTTP result handle is invalid"
		);
		return NULL;
	}
	return (xhttpresult*)pHandle;
}



/* 根据公共传输策略和可选 TLS 对象创建 Client。 */
static xhttpclient* xhttpClientCreateConfigured(
	void* pEngine,
	long long iTimeoutMilliseconds,
	long long iIdleTimeoutMilliseconds,
	bool bSystemTrust,
	const xtlscontext* pTlsContext,
	const xtlsverifier* pTlsVerifier
)
{
	xhttpclientconfig Config;
	uint64 iTimeout;
	uint64 iIdleTimeout;

	if ( pEngine == NULL ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"create-http-client",
			"HTTP client requires a network engine"
		);
		return NULL;
	}
	if ( !xhttpClientDuration(iTimeoutMilliseconds, &iTimeout) ||
		!xhttpClientDuration(iIdleTimeoutMilliseconds, &iIdleTimeout) ) {
		return NULL;
	}
	xrtHttpClientConfigInit(&Config);
	if ( iTimeoutMilliseconds != 0 ) {
		Config.Timeout = iTimeout;
	}
	if ( iIdleTimeoutMilliseconds != 0 ) {
		Config.IdleTimeout = iIdleTimeout;
	}
	Config.SystemTrust = bSystemTrust;
	Config.TlsContext = pTlsContext;
	Config.TlsVerifier = pTlsVerifier;
	return xrtHttpClientCreate((xnetengine*)pEngine, &Config);
}



/* 创建同时支持 HTTP 与系统信任 HTTPS 的默认 Client。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_create(
	void* pEngine,
	long long iTimeoutMilliseconds,
	long long iIdleTimeoutMilliseconds,
	bool bSystemTrust
)
{
	return xhttpClientCreateConfigured(
		pEngine,
		iTimeoutMilliseconds,
		iIdleTimeoutMilliseconds,
		bSystemTrust,
		NULL,
		NULL
	);
}



/* 使用原生 TLS 上下文创建 Client，并消费调用方传入的 Verifier 引用。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_create_tls(
	void* pEngine,
	const xtlscontext* pContext,
	xtlsverifier* pVerifier,
	long long iTimeoutMilliseconds,
	long long iIdleTimeoutMilliseconds
)
{
	xhttpclient* pClient;

	if ( (pContext == NULL) || (pVerifier == NULL) ) {
		xrtTlsVerifierRelease(pVerifier);
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"create-http-client-tls",
			"HTTPS client requires a TLS context and verifier"
		);
		return NULL;
	}
	pClient = xhttpClientCreateConfigured(
		pEngine,
		iTimeoutMilliseconds,
		iIdleTimeoutMilliseconds,
		false,
		pContext,
		pVerifier
	);
	xrtTlsVerifierRelease(pVerifier);
	return pClient;
}



/* point 构造只接管一份 Client 所有者引用。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放语言对象持有的 Client 所有者引用。 */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_destroy(void* pHandle)
{
	xrtHttpClientDestroy((xhttpclient*)pHandle);
}



/* 返回仅在语言对象存活期间有效的借用指针。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_native(void* pHandle)
{
	return pHandle;
}



/* 开始平滑排空 Client。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_drain(void* pHandle)
{
	xhttpclient* pClient = xhttpClientRequire(pHandle);

	return (pClient != NULL) && xrtHttpClientDrain(pClient);
}



/* 中止 Client 及其全部活动调用。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_abort(void* pHandle)
{
	xhttpclient* pClient = xhttpClientRequire(pHandle);

	return (pClient != NULL) && xrtHttpClientAbort(pClient);
}



/* 返回 Client 生命周期状态快照。 */
XLANG_XHTTP_CLIENT_EXPORT long long xhttp_client_state(void* pHandle)
{
	xhttpclient* pClient = xhttpClientRequire(pHandle);

	return pClient != NULL ?
		(long long)xrtHttpClientState(pClient) :
		(long long)XHTTP_CLIENT_CLOSED;
}



/* 创建可独立取消的 Client 关闭等待 Future。 */
XLANG_XHTTP_CLIENT_EXPORT xfuture* xhttp_client_wait_async(void* pHandle)
{
	xhttpclient* pClient = xhttpClientRequire(pHandle);

	return pClient != NULL ? xrtHttpClientWaitAsync(pClient) : NULL;
}



/* 同步执行请求并返回保留 Upgrade 能力的完整结果。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_execute(
	void* pClientHandle,
	void* pRequestHandle,
	long long iTimeoutMilliseconds,
	long long iIdleTimeoutMilliseconds,
	long long iResponseBodyLimit
)
{
	xhttpclient* pClient = xhttpClientRequire(pClientHandle);
	xhttprequest* pRequest = xhttpClientRequestRequire(pRequestHandle);
	xhttpcalloptions Options;

	if ( (pClient == NULL) || (pRequest == NULL) ||
		!xhttpClientOptions(
			&Options,
			iTimeoutMilliseconds,
			iIdleTimeoutMilliseconds,
			iResponseBodyLimit
		) ) {
		return NULL;
	}
	return xrtHttpClientDoSync(pClient, pRequest, &Options);
}



/* 释放 Future 最终拥有的 xlang 对象引用。 */
static void xhttpClientFutureValueFree(void* pValue)
{
	xvalue** ppValue = (xvalue**)pValue;

	if ( ppValue != NULL ) {
		xrtValueRelease(*ppValue);
		xrtFree(ppValue);
	}
}



/* 释放异步映射器持有的语言闭包。 */
static void xhttpClientFutureMapperFree(void* pArgument)
{
	xhttp_client_future_mapper* pMapper =
		(xhttp_client_future_mapper*)pArgument;

	if ( pMapper != NULL ) {
		xrtValueRelease(pMapper->Boxer);
		xrtFree(pMapper);
	}
}



/* 把当前线程错误复制进目标 Future。 */
static int xhttpClientFutureMapError(
	xl6_future_result* pOutput,
	cstr sMessage
)
{
	if ( xrtGetError() == NULL ) {
		xhttpClientNativeError(
			XERR_INTERNAL,
			"map-http-result",
			sMessage
		);
	}
	pOutput->iStatus = XL6_FUTURE_STATUS_FAILED;
	pOutput->Error = xrtErrorRef(xrtGetError());
	return -1;
}



/* 把 XRT 的拥有型 HTTP 结果装箱为 xlang HttpResult。 */
static int xhttpClientFutureMap(
	const xl6_future_result* pInput,
	void* pArgument,
	xl6_future_result* pOutput
)
{
	xhttp_client_future_mapper* pMapper =
		(xhttp_client_future_mapper*)pArgument;
	xvalue* pArgumentValue;
	xvalue* pResult;
	xvalue** ppResult;
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	bool bInvoked;

	if ( (pInput == NULL) || (pInput->pValue == NULL) ||
		(pMapper == NULL) || !xrtValueIsCallable(pMapper->Boxer) ) {
		xhttpClientNativeError(
			XERR_INTERNAL,
			"map-http-result",
			"HTTP Future mapper received an invalid result"
		);
		return xhttpClientFutureMapError(
			pOutput,
			"HTTP Future mapper received an invalid result"
		);
	}
	xrtClearError();
	pArgumentValue = xrtValuePointer(pInput->pValue);
	if ( pArgumentValue == NULL ) {
		return xhttpClientFutureMapError(
			pOutput,
			"failed to create the HTTP result callback argument"
		);
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 1u;
	Frame.Arguments = &pArgumentValue;
	bInvoked = xrtValueInvoke(pMapper->Boxer, &Frame, &Result);
	xrtValueRelease(pArgumentValue);
	if ( !bInvoked ) {
		/* xcall 失败已带有 XRT 结构化错误，不存在第二份语言异常槽。 */
		xrtCallResultUnit(&Result);
		return xhttpClientFutureMapError(
			pOutput,
			"HTTP result callback failed"
		);
	}
	pResult = xrtValueRetain(xrtCallResultGet(&Result, 0u));
	xrtCallResultUnit(&Result);
	if ( pResult == NULL ) {
		return xhttpClientFutureMapError(
			pOutput,
			"HTTP result callback returned no object"
		);
	}
	ppResult = (xvalue**)xrtMalloc(sizeof(*ppResult));
	if ( ppResult == NULL ) {
		xrtValueRelease(pResult);
		return xhttpClientFutureMapError(
			pOutput,
			"failed to allocate the HTTP Future result"
		);
	}
	*ppResult = pResult;
	pOutput->iStatus = XL6_FUTURE_STATUS_OK;
	pOutput->pValue = ppResult;
	pOutput->iFlags = XL6_FUTURE_OWN_VALUE;
	pOutput->pfnFreeValue = xhttpClientFutureValueFree;
	return 0;
}



/* 异步执行请求，并映射为可直接 await 的 future<HttpResult>。 */
XLANG_XHTTP_CLIENT_EXPORT xfuture* xhttp_client_execute_async(
	void* pClientHandle,
	void* pRequestHandle,
	long long iTimeoutMilliseconds,
	long long iIdleTimeoutMilliseconds,
	long long iResponseBodyLimit,
	xvalue* pBoxer
)
{
	xhttpclient* pClient = xhttpClientRequire(pClientHandle);
	xhttprequest* pRequest = xhttpClientRequestRequire(pRequestHandle);
	xhttp_client_future_mapper* pMapper;
	xhttpcalloptions Options;
	xfuture* pSource;
	xfuture* pResult;

	if ( (pClient == NULL) || (pRequest == NULL) ||
		(pBoxer == NULL) || !xrtValueIsCallable(pBoxer) ||
		!xhttpClientOptions(
			&Options,
			iTimeoutMilliseconds,
			iIdleTimeoutMilliseconds,
			iResponseBodyLimit
		) ) {
		return NULL;
	}
	pMapper = (xhttp_client_future_mapper*)xrtCalloc(
		1u,
		sizeof(*pMapper)
	);
	if ( pMapper == NULL ) {
		return NULL;
	}
	pMapper->Boxer = xrtValueRetain(pBoxer);
	if ( pMapper->Boxer == NULL ) {
		xrtFree(pMapper);
		return NULL;
	}
	pSource = xrtHttpClientDoAsync(pClient, pRequest, &Options);
	if ( pSource == NULL ) {
		xhttpClientFutureMapperFree(pMapper);
		return NULL;
	}
	pResult = xl6FutureThenOwned(
		pSource,
		xhttpClientFutureMap,
		pMapper,
		xhttpClientFutureMapperFree
	);
	if ( pResult == NULL ) {
		xhttpClientFutureMapperFree(pMapper);
		xl6FutureDestroy(pSource);
		return NULL;
	}
	if ( !xl6FutureLinkCancel(pResult, pSource) ) {
		xl6FutureDestroy(pResult);
		xl6FutureDestroy(pSource);
		return NULL;
	}
	xl6FutureDestroy(pSource);
	return pResult;
}



/* point 构造接管一份结果引用。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_result_wrap(void* pHandle)
{
	return pHandle;
}



/* 为 Future 装箱增加一份结果引用。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_result_ref(void* pHandle)
{
	return xrtHttpResultRef((xhttpresult*)pHandle);
}



/* 释放结果及尚未取走的响应和 Upgrade 传输。 */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_result_destroy(
	void* pHandle
)
{
	xrtHttpResultDestroy((xhttpresult*)pHandle);
}



/* 返回结果原生借用句柄。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_result_native(void* pHandle)
{
	return pHandle;
}



/* 判断响应所有权是否仍保留在结果中。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_result_has_response(
	void* pHandle
)
{
	xhttpresult* pResult = xhttpClientResultRequire(pHandle);

	return (pResult != NULL) && (xrtHttpResultResponse(pResult) != NULL);
}



/* 原子取走响应所有权。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_result_take_response(
	void* pHandle
)
{
	xhttpresult* pResult = xhttpClientResultRequire(pHandle);
	xhttpresponse* pResponse;

	if ( pResult == NULL ) {
		return NULL;
	}
	pResponse = xrtHttpResultTakeResponse(pResult);
	if ( pResponse == NULL ) {
		xhttpClientNativeError(
			XERR_STATE,
			"take-http-response",
			"HTTP response has already been taken"
		);
	}
	return pResponse;
}



/* 原子取走明文 Upgrade Stream 所有权。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_result_take_tcp(
	void* pHandle
)
{
	xhttpresult* pResult = xhttpClientResultRequire(pHandle);

	return pResult != NULL ? xrtHttpResultTakeTcp(pResult) : NULL;
}



/* 原子取走 TLS Upgrade Stream 所有权。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_result_take_tls(
	void* pHandle
)
{
	xhttpresult* pResult = xhttpClientResultRequire(pHandle);

	return pResult != NULL ? xrtHttpResultTakeTls(pResult) : NULL;
}



/* 释放尚未被语言 TlsStream 接管的原生 TLS 流。 */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_tls_stream_discard(
	void* pHandle
)
{
	xtlsstream* pStream = (xtlsstream*)pHandle;

	if ( pStream == NULL ) {
		return;
	}
	(void)xrtTlsStreamAbort(pStream);
	xrtTlsStreamDestroy(pStream);
}



/* 读取完整调用诊断快照中的一个数值字段。 */
XLANG_XHTTP_CLIENT_EXPORT long long xhttp_client_result_value(
	void* pHandle,
	long long iField
)
{
	xhttpresult* pResult = xhttpClientResultRequire(pHandle);
	xhttpcallinfo Info;

	if ( (pResult == NULL) || !xrtHttpResultInfo(pResult, &Info) ) {
		return 0;
	}
	switch ( iField ) {
		case 0: return Info.State;
		case 1: return Info.Phase;
		case 2: return Info.Result;
		case 3: return Info.Error;
		case 4: return (long long)Info.Submitted;
		case 5: return (long long)Info.Started;
		case 6: return (long long)Info.TransportReady;
		case 7: return (long long)Info.RequestSent;
		case 8: return (long long)Info.FirstByte;
		case 9: return (long long)Info.Headers;
		case 10: return (long long)Info.LastProgress;
		case 11: return (long long)Info.Completed;
		case 12: return (long long)Info.RequestWireBytes;
		case 13: return (long long)Info.ResponseWireBytes;
		case 14: return (long long)Info.ResponseBodyBytes;
		case 15: return (long long)Info.Redirects;
		case 16: return Info.ReusedConnection ? 1 : 0;
		case 17: return Info.Secure ? 1 : 0;
		case 18: return (long long)xrtHttpResultBuffered(pResult);
		case 19: return xrtHttpResultUpgraded(pResult) ? 1 : 0;
		default:
			xhttpClientNativeError(
				XERR_ARGUMENT,
				"read-http-result",
				"HTTP result field is out of range"
			);
			return 0;
	}
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_destroy_abi(void* handle)
{
    (void)xhttp_client_destroy(handle);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_result_destroy_abi(void* handle)
{
    (void)xhttp_client_result_destroy(handle);
}
