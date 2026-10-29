#include "../internal/xrt_http_client_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 高层 HTTP/1 客户端只发布自己能够实际处理的 ALPN。 */
static const xstrview __xrtHttpClientProtocol = {
	"http/1.1",
	8
};



/* 使用默认 HTTP 策略和调用方提供的 TLS 对象创建 Client。 */
XRT_API xhttpclient* xrtHttpClientCreateTls(
	xnetengine* pEngine,
	const xtlscontext* pContext,
	const xtlsverifier* pVerifier
)
{
	xhttpclientconfig Config;

	xrtHttpClientConfigInit(&Config);
	Config.TlsContext = pContext;
	Config.TlsVerifier = pVerifier;
	Config.SystemTrust = (pVerifier == NULL);
	return xrtHttpClientCreate(pEngine, &Config);
}



/* 创建默认系统信任验证器。 */
static xtlsverifier* __xrtHttpClientSystemVerifier(void)
{
	xtlsverifierconfig Config;
	xx509store* pStore;
	xtlsverifier* pVerifier;

	pStore = xrtX509StoreSystem();
	if ( pStore == NULL ) {
		return NULL;
	}
	xrtTlsVerifierConfigInit(&Config);
	Config.Store = pStore;
	pVerifier = xrtTlsVerifierCreate(&Config);
	xrtX509StoreFree(pStore);
	return pVerifier;
}



/* 为 Client 建立或保留 TLS Context 与验证器。 */
bool __xrtHttpClientTlsInit(xhttpclient* pClient)
{
	if ( pClient->Config.TlsContext != NULL ) {
		pClient->TlsContext = xrtTlsContextRetain(
			pClient->Config.TlsContext
		);
	} else {
		pClient->TlsContext = xrtTlsContextCreate(NULL);
	}
	if ( pClient->TlsContext == NULL ) {
		return false;
	}
	if ( pClient->Config.TlsVerifier != NULL ) {
		pClient->TlsVerifier = xrtTlsVerifierRetain(
			pClient->Config.TlsVerifier
		);
	} else if ( pClient->Config.SystemTrust ) {
		pClient->TlsVerifier = __xrtHttpClientSystemVerifier();
	} else {
		xerror* pError = __xrtHttpClientErrorCreate(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-https-client",
			"HTTPS client requires system trust or an explicit TLS verifier",
			NULL
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	if ( pClient->TlsVerifier == NULL ) {
		xrtTlsContextRelease(pClient->TlsContext);
		pClient->TlsContext = NULL;
		return false;
	}
	pClient->Config.TlsContext = pClient->TlsContext;
	pClient->Config.TlsVerifier = pClient->TlsVerifier;
	return true;
}



/* 释放 Client 持有的 TLS 共享对象。 */
void __xrtHttpClientTlsUnit(xhttpclient* pClient)
{
	xrtTlsVerifierRelease(pClient->TlsVerifier);
	xrtTlsContextRelease(pClient->TlsContext);
	pClient->TlsVerifier = NULL;
	pClient->TlsContext = NULL;
	pClient->Config.TlsVerifier = NULL;
	pClient->Config.TlsContext = NULL;
}



/* 为直连和代理隧道生成完全一致的目标 TLS 身份配置。 */
void __xrtHttpCallTlsConfig(
	xhttpcall* pCall,
	xtlsclientconfig* pConfig
)
{
	xnetaddr Address;
	xstrview Host;

	xrtTlsClientConfigInit(pConfig);
	pConfig->Context = pCall->Client->TlsContext;
	Host.Data = pCall->Host;
	Host.Size = strlen(pCall->Host);
	pConfig->VerifyName = Host;
	if ( !xrtNetAddrParse(&Address, pCall->Host, 0) ) {
		xrtClearError();
		pConfig->ServerName = Host;
	}
	pConfig->Protocols = &__xrtHttpClientProtocol;
	pConfig->ProtocolCount = 1;
	pConfig->Verifier = pCall->Client->TlsVerifier;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		pConfig->Resume = __xrtHttpResumeTake(pCall);
	#endif
}



/* 从 Call 摘除 TLS Dial 的调用方引用。 */
static void __xrtHttpCallTlsDialDetach(
	xhttpcall* pCall,
	xtlsdial* pDial
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->TlsDial == pDial ) {
		pCall->TlsDial = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* 验证服务端没有把 HTTP/1 客户端切换到不支持的 ALPN。 */
static bool __xrtHttpCallTlsProtocol(xtlsstream* pStream)
{
	xbytesview Protocol;

	if ( !xrtTlsSessionProtocol(
		xrtTlsStreamSession(pStream),
		&Protocol
	) ) {
		return true;
	}
	if ( (Protocol.Size == __xrtHttpClientProtocol.Size) &&
		(memcmp(
			Protocol.Data,
			__xrtHttpClientProtocol.Data,
			Protocol.Size
		) == 0) ) {
		return true;
	}
	{
		xerror* pError = __xrtHttpClientErrorCreate(
			XERR_PROTOCOL,
			XHTTP_CLIENT_ERROR_TLS,
			"negotiate-https",
			"HTTPS peer selected an unsupported ALPN protocol",
			NULL
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 在已验证 TLS Stream 上接管 Exchange 并发布活动低级 Call。 */
static bool __xrtHttpCallAttachTls(
	xhttpcall* pCall,
	xtlsstream* pStream
)
{
	xhttp1callevents Events;
	xhttp1call* pStreamCall;

	__xrtHttpCallTransportReady(pCall);
	__xrtHttpCallStreamEvents(pCall, &Events);
	pStreamCall = xrtHttp1CallTls(
		pStream,
		pCall->Exchange,
		&pCall->Client->Config.Call,
		&Events
	);

	if ( pStreamCall == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->Exchange = NULL;
	pCall->StreamCall = pStreamCall;
	(void)xrtSpinUnlock(&pCall->Lock);
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_EXCHANGING,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtHttp1CallCancel(pStreamCall);
	}
	return true;
}



/* TLS Dial 成功后在安全 Stream 所属 Worker 上建立 HTTP/1 Call。 */
static void __xrtHttpCallTlsDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	__xrtHttpCallTlsDialDetach(pCall, pDial);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			Result,
			Result == XNET_RESULT_CANCELLED ?
				XHTTP_CLIENT_ERROR_CANCELLED :
				XHTTP_CLIENT_ERROR_TLS,
			Result == XNET_RESULT_CANCELLED ?
				XERR_CANCELLED :
				__xrtHttpClientCauseKind(
					pError,
					XERR_IO
				),
			"dial-https",
			Result == XNET_RESULT_CANCELLED ?
				"HTTPS dial was cancelled" :
				"HTTPS dial or TLS handshake failed",
			pError
		);
		xrtTlsDialDestroy(pDial);
		return;
	}
	__xrtHttpCallTlsConnected(
		pCall,
		pStream,
		"dial-https"
	);
	xrtTlsDialDestroy(pDial);
}



/* 接管一条已验证 TLS 传输并统一处理取消、ALPN、配额与 HTTP/1 附加。 */
void __xrtHttpCallTlsConnected(
	xhttpcall* pCall,
	xtlsstream* pStream,
	cstr sOperation
)
{
	xerror* pCause;

	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolOpened(pCall);
	#endif
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtTlsStreamAbort(pStream);
		xrtTlsStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			sOperation,
			"HTTP call was cancelled after TLS connected",
			NULL
		);
		return;
	}
	if ( !__xrtHttpCallTlsProtocol(pStream) ) {
		pCause = xrtTakeError();
		(void)xrtTlsStreamAbort(pStream);
		xrtTlsStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_TLS,
			XERR_PROTOCOL,
			"negotiate-https",
			"HTTPS ALPN negotiation is incompatible with HTTP/1",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
	if ( !__xrtHttpCallAttachTls(pCall, pStream) ) {
		pCause = xrtTakeError();
		(void)xrtTlsStreamAbort(pStream);
		xrtTlsStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			__xrtHttpClientCauseKind(
				pCause,
				XERR_INTERNAL
			),
			"start-https-http1",
			"HTTP/1 call could not attach to the connected TLS stream",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 在空闲 TLS Stream Worker 上重新附加 Exchange，失效连接原位重拨。 */
bool __xrtHttpCallStartPooledTls(xhttpcall* pCall)
{
	xtlsstream* pStream = pCall->PooledTls;

	if ( !__xrtHttp1TlsReusable(pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return __xrtHttpCallStartTls(pCall);
	}
	if ( !__xrtHttpCallAttachTls(pCall, pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return false;
	}
	__xrtHttpPoolPooledUsed(pCall);
	return true;
}

#endif



/* 在目标 Worker 上创建 DNS、TCP 和 TLS 组合拨号。 */
bool __xrtHttpCallStartTls(xhttpcall* pCall)
{
	xtlsclientconfig Tls;
	xtlsdialconfig Config;
	xtlsstreamevents Events;
	xtlsdial* pDial;

	__xrtHttpCallTlsConfig(pCall, &Tls);
	xrtTlsDialConfigInit(&Config);
	Config.Transport = pCall->Client->Config.Dial;
	Config.Transport.Affinity = pCall->Affinity;
	Config.Stream = pCall->Client->Config.TlsStream;
	Config.Timeout = pCall->Timeout ==
		XHTTP_CLIENT_TIMEOUT_NONE ? 0 : pCall->Timeout;
	Config.ServerNameFromHost = false;
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_DIALING,
		XMEMORY_RELEASE
	);
	pDial = xrtTlsDial(
		pCall->Client->Engine,
		pCall->Client->Resolver,
		pCall->Host,
		pCall->Port,
		&Tls,
		&Config,
		&Events,
		pCall,
		__xrtHttpCallTlsDone,
		pCall
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xrtTlsResumeRelease((xtlsresume*)Tls.Resume);
	#endif
	if ( pDial == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->TlsDial = pDial;
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtTlsDialCancel(pDial);
	}
	return true;
}

#endif
