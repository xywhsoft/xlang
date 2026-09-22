#include "../internal/xrt_http_client_runtime.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

/* 保存一次低层失败提升后的稳定客户端错误。 */
typedef struct xrt_http_client_failure {
	xhttpclienterror Error;
	xerrkind Kind;
	cstr Operation;
	cstr Message;
} xrt_http_client_failure;



/* 沿原因链查找指定错误域中的第一层错误。 */
static const xerror* __xrtHttpClientErrorDomain(
	const xerror* pError,
	cstr sDomain
)
{
	while ( pError != NULL ) {
		cstr sCurrent = xrtErrorDomain(pError);

		if ( (sCurrent != NULL) &&
			(strcmp(sCurrent, sDomain) == 0) ) {
			return pError;
		}
		pError = xrtErrorCause(pError);
	}
	return NULL;
}



/* 判断 Exchange 回调失败是否来自调用方公开事件。 */
static bool __xrtHttpClientUserCallback(
	const xhttpcall* pCall,
	const xerror* pExchangeError
)
{
	int32 iCode = xrtErrorCode(pExchangeError);
	cstr sOperation = xrtErrorOperation(pExchangeError);

	if ( iCode == XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK ) {
		return pCall->Events.Body != NULL;
	}
	if ( iCode != XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK ) {
		return false;
	}
	if ( (sOperation != NULL) &&
		(strcmp(
			sOperation,
			"deliver-http1-informational"
		) == 0) ) {
		return pCall->Events.Informational != NULL;
	}
	if ( (sOperation != NULL) &&
		(strcmp(
			sOperation,
			"deliver-http1-response-head"
		) == 0) ) {
		return pCall->Events.Headers != NULL;
	}
	return (pCall->Events.Informational != NULL) ||
		(pCall->Events.Headers != NULL);
}



/* 把 Exchange 错误提升为请求、响应、协议或用户回调错误。 */
static void __xrtHttpClientClassifyExchange(
	const xhttpcall* pCall,
	const xerror* pExchangeError,
	xrt_http_client_failure* pFailure
)
{
	int32 iCode = xrtErrorCode(pExchangeError);
	xerrkind Kind = __xrtHttpClientCauseKind(
		pExchangeError,
		XERR_PROTOCOL
	);

	pFailure->Kind = Kind;
	switch ( iCode ) {
		case XHTTP1_EXCHANGE_ERROR_REQUEST_BODY:
		case XHTTP1_EXCHANGE_ERROR_REQUEST_LENGTH:
			pFailure->Error = XHTTP_CLIENT_ERROR_REQUEST;
			pFailure->Operation = "produce-http-request";
			pFailure->Message =
				"HTTP request body production failed";
			return;

		case XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK:
		case XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK:
			if ( __xrtHttpClientUserCallback(
				pCall,
				pExchangeError
			) ) {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_CALLBACK;
				pFailure->Operation =
					"handle-http-response-callback";
				pFailure->Message =
					"HTTP response callback stopped the call";
			} else {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_RESPONSE;
				pFailure->Operation =
					"store-http-response";
				pFailure->Message =
					"HTTP response processing failed";
			}
			return;

		case XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT:
		case XHTTP1_EXCHANGE_ERROR_INFORMATIONAL_LIMIT:
			pFailure->Error = XHTTP_CLIENT_ERROR_RESPONSE;
			pFailure->Operation = "receive-http-response";
			pFailure->Message =
				"HTTP response exceeds its configured limits";
			return;

		case XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD:
		case XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING:
			if ( (Kind == XERR_MEMORY) ||
				(Kind == XERR_RANGE) ) {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_RESPONSE;
				pFailure->Operation =
					"store-http-response";
				pFailure->Message =
					"HTTP response could not be represented";
			} else {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_PROTOCOL;
				pFailure->Operation =
					"parse-http-response";
				pFailure->Message =
					"HTTP response protocol is invalid";
			}
			return;

		case XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF:
			pFailure->Error = XHTTP_CLIENT_ERROR_PROTOCOL;
			pFailure->Operation = "parse-http-response";
			pFailure->Message =
				"HTTP response ended before completion";
			return;

		case XHTTP1_EXCHANGE_ERROR_ARGUMENT:
		case XHTTP1_EXCHANGE_ERROR_STATE:
		default:
			pFailure->Error = XHTTP_CLIENT_ERROR_INTERNAL;
			pFailure->Operation = "run-http-exchange";
			pFailure->Message =
				"HTTP Exchange entered an invalid state";
			return;
	}
}



/* 把低层 HTTP/1 Call 原因链提升为唯一的高层错误契约。 */
static xrt_http_client_failure __xrtHttpClientClassifyFailure(
	const xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	xrt_http_client_failure Failure = {
		XHTTP_CLIENT_ERROR_INTERNAL,
		XERR_INTERNAL,
		"run-http1-call",
		"HTTP/1 call failed without a classified cause"
	};
	const xerror* pCallError;
	const xerror* pExchangeError;
	int32 iCode;

	if ( pResult == NULL ) {
		return Failure;
	}
	if ( pResult->Result == XNET_RESULT_CANCELLED ) {
		Failure.Error = XHTTP_CLIENT_ERROR_CANCELLED;
		Failure.Kind = XERR_CANCELLED;
		Failure.Operation = "cancel-http-call";
		Failure.Message = "HTTP call was cancelled";
		return Failure;
	}
	pCallError = __xrtHttpClientErrorDomain(
		pResult->Error,
		"xrt.http.call"
	);
	if ( pCallError == NULL ) {
		Failure.Kind = __xrtHttpClientCauseKind(
			pResult->Error,
			XERR_INTERNAL
		);
		return Failure;
	}
	iCode = xrtErrorCode(pCallError);
	if ( iCode == XHTTP1_CALL_ERROR_TRANSPORT ) {
		Failure.Error = XHTTP_CLIENT_ERROR_TRANSPORT;
		Failure.Kind = __xrtHttpClientCauseKind(
			pCallError,
			XERR_IO
		);
		Failure.Operation = "transport-http1";
		Failure.Message = "HTTP/1 transport failed";
		return Failure;
	}
	if ( iCode == XHTTP1_CALL_ERROR_CANCELLED ) {
		Failure.Error = XHTTP_CLIENT_ERROR_CANCELLED;
		Failure.Kind = XERR_CANCELLED;
		Failure.Operation = "cancel-http-call";
		Failure.Message = "HTTP call was cancelled";
		return Failure;
	}
	if ( iCode != XHTTP1_CALL_ERROR_EXCHANGE ) {
		Failure.Kind = __xrtHttpClientCauseKind(
			pCallError,
			XERR_INTERNAL
		);
		return Failure;
	}
	pExchangeError = __xrtHttpClientErrorDomain(
		pCallError,
		"xrt.http.exchange"
	);
	if ( pExchangeError != NULL ) {
		__xrtHttpClientClassifyExchange(
			pCall,
			pExchangeError,
			&Failure
		);
		return Failure;
	}
	Failure.Kind = __xrtHttpClientCauseKind(
		pCallError,
		XERR_INTERNAL
	);
	if ( xrtAtomic32Load(
		&pCall->Info.Phase,
		XMEMORY_ACQUIRE
	) == XHTTP_CALL_PHASE_REQUEST ) {
		Failure.Error = XHTTP_CLIENT_ERROR_REQUEST;
		Failure.Operation = "produce-http-request";
		Failure.Message = "HTTP request production failed";
	}
	return Failure;
}



/* 从 Call 摘除低级 HTTP/1 调用的调用方引用。 */
static void __xrtHttpClientStreamDetach(
	xhttpcall* pCall,
	xhttp1call* pStreamCall
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->StreamCall == pStreamCall ) {
		pCall->StreamCall = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* 正常关闭一个不再暴露给调用方的可复用传输。 */
static void __xrtHttpClientStreamClose(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			if ( pResult->Tls != NULL ) {
				(void)xrtTlsStreamClose(pResult->Tls);
				xrtTlsStreamDestroy(pResult->Tls);
			}
		#else
			(void)pResult;
		#endif
		return;
	}
	if ( pResult->Tcp != NULL ) {
		(void)xrtNetStreamClose(pResult->Tcp);
		xrtNetStreamDestroy(pResult->Tcp);
	}
}



/* 异常回收一个不满足高层结果不变量的传输。 */
static void __xrtHttpClientStreamAbort(
	const xhttp1callresult* pResult
)
{
	if ( pResult->Tcp != NULL ) {
		(void)xrtNetStreamAbort(pResult->Tcp);
		xrtNetStreamDestroy(pResult->Tcp);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pResult->Tls != NULL ) {
			(void)xrtTlsStreamAbort(pResult->Tls);
			xrtTlsStreamDestroy(pResult->Tls);
		}
	#endif
}



/* 验证成功结果中的传输所有权、复用和协议升级关系。 */
static bool __xrtHttpClientStreamSuccessValid(
	const xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	bool bTransferred;

	if ( (pResult->Response == NULL) ||
		(pResult->Reusable && pResult->Upgraded) ||
		(pResult->Reusable && (pResult->Buffered != 0)) ) {
		return false;
	}
	bTransferred = pResult->Reusable || pResult->Upgraded;
	if ( !bTransferred ) {
		return (pResult->Tcp == NULL)
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				&& (pResult->Tls == NULL)
			#endif
			;
	}
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			return (pResult->Tcp == NULL) &&
				(pResult->Tls != NULL);
		#else
			return false;
		#endif
	}
	return (pResult->Tcp != NULL)
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			&& (pResult->Tls == NULL)
		#endif
		;
}



/* 把低级 HTTP/1 Call 的唯一终态提升为完整客户端结果。 */
void __xrtHttpClientStreamDone(
	xhttp1call* pStreamCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xrt_http_client_failure Failure;
	xhttpresponse* pResponse;
	xnetstream* pTcp = NULL;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		bool bPooled = false;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xerror* pPolicyCause;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		xerror* pDecompressCause;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xerror* pCacheCause;
		bool bCacheReplayed;
	#endif

	__xrtHttpClientStreamDetach(pCall, pStreamCall);
	if ( (pResult == NULL) ||
		(pResult->Result != XNET_RESULT_OK) ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
			if ( __xrtHttpCookieFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
			if ( __xrtHttpClientCacheFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
			if ( __xrtHttpRedirectFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
			if ( __xrtHttpDecompressFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		Failure = __xrtHttpClientClassifyFailure(
			pCall,
			pResult
		);
		__xrtHttpCallFail(
			pCall,
			pResult != NULL ?
				pResult->Result :
				XNET_RESULT_ERROR,
			Failure.Error,
			Failure.Kind,
			Failure.Operation,
			Failure.Message,
			pResult != NULL ? pResult->Error : NULL
		);
		xrtHttp1CallDestroy(pStreamCall);
		return;
	}
	pResponse = pResult->Response;
	if ( !__xrtHttpClientStreamSuccessValid(
		pCall,
		pResult
	) ) {
		xrtHttpResponseDestroy(pResponse);
		__xrtHttpClientStreamAbort(pResult);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"complete-http-call",
			"HTTP/1 call returned an inconsistent success result",
			NULL
		);
		xrtHttp1CallDestroy(pStreamCall);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		__xrtHttpResumeCollect(pCall, pResult->Tls);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( __xrtHttpRetryPending(pCall) ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				if ( pResult->Reusable ) {
					bPooled = __xrtHttpPoolPut(
						pCall,
						pResult
					);
					if ( !bPooled ) {
						xrtClearError();
					}
				}
				if ( !bPooled )
			#endif
			{
				__xrtHttpClientStreamClose(
					pCall,
					pResult
				);
			}
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				__xrtHttpPoolFinish(pCall);
			#endif
			xrtHttpResponseDestroy(pResponse);
			xrtHttp1CallDestroy(pStreamCall);
			(void)__xrtHttpRetrySchedule(pCall);
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCacheDone(
			pCall,
			pResponse,
			&bCacheReplayed
		) ) {
			pCacheCause = xrtTakeError();
			__xrtHttpClientStreamClose(
				pCall,
				pResult
			);
			xrtHttpResponseDestroy(pResponse);
			if ( !__xrtHttpClientCacheFail(
				pCall,
				pCacheCause
			) ) {
				__xrtHttpCallFail(
					pCall,
					XNET_RESULT_ERROR,
					XHTTP_CLIENT_ERROR_CACHE,
					__xrtHttpClientCauseKind(
						pCacheCause,
						XERR_IO
					),
					"process-http-cache-response",
					"HTTP response cache processing failed",
					pCacheCause
				);
			}
			xrtErrorFree(pCacheCause);
			xrtHttp1CallDestroy(pStreamCall);
			return;
		}
		if ( bCacheReplayed ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				if ( pResult->Reusable ) {
					bPooled = __xrtHttpPoolPut(
						pCall,
						pResult
					);
					if ( !bPooled ) {
						xrtClearError();
					}
				}
				if ( !bPooled )
			#endif
			{
				__xrtHttpClientStreamClose(
					pCall,
					pResult
				);
			}
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				__xrtHttpPoolFinish(pCall);
			#endif
			xrtHttpResponseDestroy(pResponse);
			xrtHttp1CallDestroy(pStreamCall);
			if ( !__xrtHttpClientCacheStart(
				pCall,
				&bCacheReplayed
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
					"deliver-http-cache-validation",
					"validated HTTP cache response could not be rebuilt",
					pCacheCause
				);
				xrtErrorFree(pCacheCause);
			}
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( pCall->RedirectPending ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				if ( pResult->Reusable ) {
					bPooled = __xrtHttpPoolPut(
						pCall,
						pResult
					);
					if ( !bPooled ) {
						xrtClearError();
					}
				}
				if ( !bPooled )
			#endif
			{
				__xrtHttpClientStreamClose(
					pCall,
					pResult
				);
			}
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				__xrtHttpPoolFinish(pCall);
			#endif
			xrtHttpResponseDestroy(pResponse);
			xrtHttp1CallDestroy(pStreamCall);
			if ( !__xrtHttpRedirectAdvance(pCall) ) {
				pPolicyCause = xrtTakeError();
				#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
					if ( __xrtHttpCookieFail(
						pCall,
						pPolicyCause
					) ) {
						xrtErrorFree(pPolicyCause);
						return;
					}
				#endif
				(void)__xrtHttpRedirectFail(
					pCall,
					pPolicyCause
				);
				xrtErrorFree(pPolicyCause);
				return;
			}
			__xrtHttpCallStartHop(pCall);
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( !__xrtHttpDecompressFinish(
			pCall,
			pResponse
		) ) {
			pDecompressCause = xrtTakeError();
			__xrtHttpClientStreamClose(
				pCall,
				pResult
			);
			xrtHttpResponseDestroy(pResponse);
			if ( !__xrtHttpDecompressFail(
				pCall,
				pDecompressCause
			) ) {
				__xrtHttpCallFail(
					pCall,
					XNET_RESULT_ERROR,
					XHTTP_CLIENT_ERROR_CALLBACK,
					__xrtHttpClientCauseKind(
						pDecompressCause,
						XERR_CANCELLED
					),
					"handle-http-response-callback",
					"HTTP response body callback stopped final decoding",
					pDecompressCause
				);
			}
			xrtErrorFree(pDecompressCause);
			xrtHttp1CallDestroy(pStreamCall);
			return;
		}
	#endif
	if ( pResult->Upgraded ) {
		pTcp = pResult->Tcp;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls = pResult->Tls;
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
			__xrtHttpPoolTransferred(pCall);
		#endif
	} else {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
			if ( pResult->Reusable ) {
				bPooled = __xrtHttpPoolPut(
					pCall,
					pResult
				);
				if ( !bPooled ) {
					xrtClearError();
				}
			}
			if ( !bPooled )
		#endif
		{
			__xrtHttpClientStreamClose(pCall, pResult);
		}
	}
	__xrtHttpCallSucceed(
		pCall,
		pResponse,
		pTcp,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls,
		#endif
		pResult->Upgraded ? pResult->Buffered : 0,
		pResult->Upgraded
	);
	xrtHttp1CallDestroy(pStreamCall);
}



/* 从 Call 摘除 TCP Dial 的调用方引用。 */
static void __xrtHttpCallTcpDialDetach(
	xhttpcall* pCall,
	xnetdial* pDial
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->TcpDial == pDial ) {
		pCall->TcpDial = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* 在已打开 TCP Stream 上接管 Exchange 并发布活动低级 Call。 */
static bool __xrtHttpCallAttachTcp(
	xhttpcall* pCall,
	xnetstream* pStream
)
{
	xhttp1callevents Events;
	xhttp1call* pStreamCall;

	__xrtHttpCallTransportReady(pCall);
	__xrtHttpCallStreamEvents(pCall, &Events);
	pStreamCall = xrtHttp1CallTcp(
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



/* TCP Dial 成功后在获胜 Stream 所属 Worker 上建立 HTTP/1 Call。 */
static void __xrtHttpCallTcpDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	__xrtHttpCallTcpDialDetach(pCall, pDial);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			Result,
			Result == XNET_RESULT_CANCELLED ?
				XHTTP_CLIENT_ERROR_CANCELLED :
				XHTTP_CLIENT_ERROR_DIAL,
			Result == XNET_RESULT_CANCELLED ?
				XERR_CANCELLED :
				__xrtHttpClientCauseKind(
					pError,
					XERR_IO
				),
			"dial-http",
			Result == XNET_RESULT_CANCELLED ?
				"HTTP TCP dial was cancelled" :
				"HTTP TCP dial failed",
			pError
		);
		xrtNetDialDestroy(pDial);
		return;
	}
	__xrtHttpCallTcpConnected(
		pCall,
		pStream,
		"dial-http"
	);
	xrtNetDialDestroy(pDial);
}



/* 接管一条已经建立的 TCP 传输并统一处理取消、配额与 HTTP/1 附加。 */
void __xrtHttpCallTcpConnected(
	xhttpcall* pCall,
	xnetstream* pStream,
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
		(void)xrtNetStreamAbort(pStream);
		xrtNetStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			sOperation,
			"HTTP call was cancelled after TCP connected",
			NULL
		);
		return;
	}
	if ( !__xrtHttpCallAttachTcp(pCall, pStream) ) {
		pCause = xrtTakeError();
		(void)xrtNetStreamAbort(pStream);
		xrtNetStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			__xrtHttpClientCauseKind(
				pCause,
				XERR_INTERNAL
			),
			"start-http1",
			"HTTP/1 call could not attach to the connected TCP stream",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 在空闲 TCP Stream Worker 上重新附加 Exchange，失效连接原位重拨。 */
bool __xrtHttpCallStartPooledTcp(xhttpcall* pCall)
{
	xnetstream* pStream = pCall->PooledTcp;

	if ( !__xrtHttp1TcpReusable(pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return __xrtHttpCallStartTcp(pCall);
	}
	if ( !__xrtHttpCallAttachTcp(pCall, pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return false;
	}
	__xrtHttpPoolPooledUsed(pCall);
	return true;
}

#endif



/* 在目标 Worker 上创建解析和地址竞速操作。 */
bool __xrtHttpCallStartTcp(xhttpcall* pCall)
{
	xnetdialconfig Config = pCall->Client->Config.Dial;
	xnetstreamevents Events;
	xnetdial* pDial;

	memset(&Events, 0, sizeof(Events));
	Config.Affinity = pCall->Affinity;
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_DIALING,
		XMEMORY_RELEASE
	);
	pDial = xrtNetDial(
		pCall->Client->Engine,
		pCall->Client->Resolver,
		pCall->Host,
		pCall->Port,
		&Config,
		&Events,
		pCall,
		__xrtHttpCallTcpDone,
		pCall
	);
	if ( pDial == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->TcpDial = pDial;
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetDialCancel(pDial);
	}
	return true;
}

#endif
