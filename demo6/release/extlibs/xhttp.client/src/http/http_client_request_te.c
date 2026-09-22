#include "../internal/xrt_http_client.h"




#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TE)

/* 销毁工作副本并把当前底层错误包装为稳定请求错误。 */
static bool __xrtHttp1RequestAcceptTrailersFail(
	xhttpheaders* pWork,
	xerrkind Kind,
	xhttprequesterror Code,
	cstr sMessage
)
{
	xrtHttpHeadersDestroy(pWork);
	__xrtHttpRequestWrapError(
		Kind,
		Code,
		"accept-http1-response-trailers",
		sMessage
	);
	return false;
}



/* 失败原子地补齐 RFC 9110 要求的 TE 与 Connection 声明。 */
XRT_API bool xrtHttp1RequestAcceptTrailers(
	xhttprequest* pRequest
)
{
	const xhttpfield* pFields;
	xhttpheaders* pWork;
	xhttpteinfo Te;
	xhttpnext Connection;
	size_t iCount;
	bool bTrailers;

	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	pFields = xrtHttpHeadersData(pRequest->Headers);
	iCount = xrtHttpHeadersCount(pRequest->Headers);
	if ( !xrtHttpTeParse(pFields, iCount, &Te) ) {
		return __xrtHttp1RequestAcceptTrailersFail(
			NULL,
			XERR_PROTOCOL,
			XHTTP_REQUEST_ERROR_TE,
			"HTTP request contains an invalid TE field"
		);
	}
	Connection = xrtHttpConnectionFind(
		pFields, iCount, XRT_STR_LITERAL("TE")
	);
	if ( Connection == XHTTP_NEXT_ERROR ) {
		return __xrtHttp1RequestAcceptTrailersFail(
			NULL,
			XERR_PROTOCOL,
			XHTTP_REQUEST_ERROR_CONNECTION,
			"HTTP request contains an invalid Connection field"
		);
	}
	bTrailers = (Te.Flags & XHTTP_TE_ACCEPTS_TRAILERS) != 0;
	if ( bTrailers && (Connection == XHTTP_NEXT_ITEM) ) {
		return true;
	}
	pWork = xrtHttpHeadersClone(pRequest->Headers);
	if ( pWork == NULL ) {
		return __xrtHttp1RequestAcceptTrailersFail(
			NULL,
			XERR_MEMORY,
			XHTTP_REQUEST_ERROR_TE,
			"HTTP request Trailer capability could not be copied"
		);
	}
	if ( !bTrailers && !xrtHttpHeadersAdd(
		pWork,
		XRT_STR_LITERAL("TE"),
		XRT_STR_LITERAL("trailers")
	) ) {
		return __xrtHttp1RequestAcceptTrailersFail(
			pWork,
			XERR_MEMORY,
			XHTTP_REQUEST_ERROR_TE,
			"HTTP request TE field could not be extended"
		);
	}
	if ( (Connection != XHTTP_NEXT_ITEM) && !xrtHttpHeadersAdd(
		pWork,
		XRT_STR_LITERAL("Connection"),
		XRT_STR_LITERAL("TE")
	) ) {
		return __xrtHttp1RequestAcceptTrailersFail(
			pWork,
			XERR_MEMORY,
			XHTTP_REQUEST_ERROR_CONNECTION,
			"HTTP request Connection field could not be extended"
		);
	}
	if ( !xrtHttpHeadersSwap(pRequest->Headers, pWork) ) {
		return __xrtHttp1RequestAcceptTrailersFail(
			pWork,
			XERR_INTERNAL,
			XHTTP_REQUEST_ERROR_TE,
			"HTTP request Trailer capability could not be committed"
		);
	}
	xrtHttpHeadersDestroy(pWork);
	return true;
}

#endif

