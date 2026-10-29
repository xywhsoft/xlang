#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST)



/* 发布客户端请求构建错误。 */
void __xrtHttpRequestError(
	xhttprequesterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_VALUE;
	Desc.Code = (int32)Code;
	Desc.Domain = "http.request";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 用请求错误域包裹当前线程的底层构建错误。 */
void __xrtHttpRequestWrapError(
	xerrkind DefaultKind,
	xhttprequesterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;
	Desc.Code = (int32)Code;
	Desc.Domain = "http.request";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	} else if ( (xrtGetError() == NULL) &&
		(pCause != NULL) ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 设置请求实现使用的通用参数错误。 */
void __xrtHttpRequestInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT,
		"http.request",
		0,
		"invalid argument"
	);
}



/* 设置请求实现使用的大小溢出错误。 */
void __xrtHttpRequestSizeOverflow(void)
{
	xrtSetErrorInfo(
		XERR_RANGE,
		"http.request",
		0,
		"size overflow"
	);
}



/* 设置请求实现使用的内部契约错误。 */
void __xrtHttpRequestInternal(void)
{
	xrtSetErrorInfo(
		XERR_INTERNAL,
		"http.request",
		0,
		"internal contract violation"
	);
}



/* 验证借用文本视图是一段不会发生地址回绕的连续内存。 */
static bool __xrtHttpRequestViewValid(xstrview Text)
{
	if ( !xrtMemRangeValid(Text.Data, Text.Size) ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	return true;
}



/* 复制一段文本并附加只供 C 调试器和内部系统调用使用的零字符。 */
static str __xrtHttpRequestTextCopy(xstrview Text)
{
	str sCopy;

	if ( (Text.Data == NULL) || (Text.Size == 0) ) {
		__xrtHttpRequestInvalidArgument();
		return NULL;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtHttpRequestSizeOverflow();
		return NULL;
	}
	sCopy = (str)xrtMalloc(Text.Size + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, Text.Data, Text.Size);
	sCopy[Text.Size] = '\0';
	return sCopy;
}



/* 校验解析后的 URL 是否属于客户端核心可安全发送的范围。 */
static bool __xrtHttpRequestUrlValid(const xurl* pUrl)
{
	uint16 iPort;

	if ( ((pUrl->Flags & XURL_HAS_SCHEME) == 0) ||
		((pUrl->Flags & XURL_HAS_AUTHORITY) == 0) ) {
		__xrtHttpRequestError(
			XHTTP_REQUEST_ERROR_URL,
			"url",
			"HTTP client URL must be absolute"
		);
		return false;
	}
	if ( !xrtUrlSchemeIs(pUrl, XRT_STR_LITERAL("http")) &&
		!xrtUrlSchemeIs(pUrl, XRT_STR_LITERAL("https")) ) {
		__xrtHttpRequestError(
			XHTTP_REQUEST_ERROR_SCHEME,
			"url",
			"HTTP client URL scheme must be http or https"
		);
		return false;
	}
	if ( ((pUrl->Flags & XURL_HAS_HOST) == 0) ||
		(pUrl->Host.Size == 0) ) {
		__xrtHttpRequestError(
			XHTTP_REQUEST_ERROR_HOST,
			"url",
			"HTTP client URL must contain a non-empty host"
		);
		return false;
	}
	if ( (pUrl->Flags & XURL_HAS_USERINFO) != 0 ) {
		__xrtHttpRequestError(
			XHTTP_REQUEST_ERROR_USERINFO,
			"url",
			"HTTP client URL userinfo is not accepted"
		);
		return false;
	}
	if ( !xrtUrlPort(pUrl, &iPort) ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_URL,
			"url",
			"HTTP client URL port is outside the network range"
		);
		return false;
	}
	if ( iPort == 0 ) {
		__xrtHttpRequestError(
			XHTTP_REQUEST_ERROR_URL,
			"url",
			"HTTP client URL port must not be zero"
		);
		return false;
	}
	return true;
}



/* 把新正文提交到请求；调用方转移 NewBody 的一个引用。 */
bool __xrtHttpRequestCommitBody(
	xhttprequest* pRequest,
	xhttpbody* pNewBody,
	xstrview ContentType
)
{
	xhttpbody* pOldBody;

	if ( ContentType.Size != 0 ) {
		if ( (ContentType.Data == NULL) ||
			!xrtHttpHeadersSet(
				pRequest->Headers,
				XRT_STR_LITERAL("Content-Type"),
				ContentType
			) ) {
			xrtHttpBodyDestroy(pNewBody);
			if ( ContentType.Data == NULL ) {
				__xrtHttpRequestInvalidArgument();
			}
			return false;
		}
	}
	pOldBody = pRequest->Body;
	pRequest->Body = pNewBody;
	xrtHttpBodyDestroy(pOldBody);
	return true;
}



/* 校验并接管已经拥有的完整 URL 文本。 */
bool __xrtHttpRequestTakeUrl(
	xhttprequest* pRequest,
	str sUrl,
	size_t iSize
)
{
	xurl Parsed;

	if ( (pRequest == NULL) || (sUrl == NULL) ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	if ( !xrtUrlParse(
		(xstrview){ sUrl, iSize },
		&Parsed
	) ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_URL,
			"url",
			"HTTP client URL is invalid"
		);
		return false;
	}
	if ( !__xrtHttpRequestUrlValid(&Parsed) ) {
		return false;
	}
	xrtFree(pRequest->UrlText);
	pRequest->UrlText = sUrl;
	pRequest->UrlSize = iSize;
	pRequest->Url = Parsed;
	return true;
}



/* 校验输出不会破坏请求对象及其拥有或借用的任一可见存储。 */
bool __xrtHttpRequestOutputValid(
	const xhttprequest* pRequest,
	const void* pOutput,
	size_t iSize
)
{
	xbytesview Body = { NULL, 0 };

	if ( !xrtMemRangeValid(pRequest, sizeof(*pRequest)) ||
		!xrtMemRangeValid(pOutput, iSize) ||
		(pRequest->Method == NULL) ||
		(pRequest->MethodSize == SIZE_MAX) ||
		(pRequest->UrlText == NULL) ||
		(pRequest->UrlSize == SIZE_MAX) ||
		(pRequest->Headers == NULL) ||
		xrtMemRangesOverlap(
			pRequest, sizeof(*pRequest), pOutput, iSize
		) || xrtMemRangesOverlap(
			pRequest->Method,
			pRequest->MethodSize + 1u,
			pOutput,
			iSize
		) || xrtMemRangesOverlap(
			pRequest->UrlText,
			pRequest->UrlSize + 1u,
			pOutput,
			iSize
		) || __xrtHttpHeadersOwnedOverlap(
			pRequest->Headers, pOutput, iSize
		) || ((pRequest->Body != NULL) &&
			xrtMemRangesOverlap(
				pRequest->Body,
				sizeof(*pRequest->Body),
				pOutput,
				iSize
			)
		) ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		if ( (pRequest->Trailers != NULL) &&
			__xrtHttpHeadersOwnedOverlap(
				pRequest->Trailers, pOutput, iSize
			) ) {
			return false;
		}
	#endif
	if ( (pRequest->Body != NULL) &&
		xrtHttpBodyView(pRequest->Body, &Body) &&
		xrtMemRangesOverlap(
			Body.Data, Body.Size, pOutput, iSize
		) ) {
		return false;
	}
	return true;
}



/* 创建拥有全部请求构建状态的对象。 */
XRT_API xhttprequest* xrtHttpRequestCreate(
	xstrview Method,
	xstrview Url
)
{
	return xrtHttpRequestCreateWithHeaders(
		Method, Url, NULL
	);
}



/* 使用指定 Header 配置创建拥有全部请求构建状态的对象。 */
XRT_API xhttprequest* xrtHttpRequestCreateWithHeaders(
	xstrview Method,
	xstrview Url,
	const xhttpheadersconfig* pHeaders
)
{
	xhttprequest* pRequest;

	pRequest = (xhttprequest*)xrtCalloc(1, sizeof(*pRequest));
	if ( pRequest == NULL ) {
		return NULL;
	}
	pRequest->Headers = xrtHttpHeadersCreate(pHeaders);
	if ( (pRequest->Headers == NULL) ||
		!xrtHttpRequestSetMethod(pRequest, Method) ||
		!xrtHttpRequestSetUrl(pRequest, Url) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* 创建 Header 和文本独立、正文共享引用的请求副本。 */
XRT_API xhttprequest* xrtHttpRequestClone(
	const xhttprequest* pRequest
)
{
	xhttprequest* pClone;
	str sUrl;

	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return NULL;
	}
	pClone = (xhttprequest*)xrtCalloc(1, sizeof(*pClone));
	if ( pClone == NULL ) {
		return NULL;
	}
	pClone->Method = __xrtHttpRequestTextCopy(
		(xstrview){ pRequest->Method, pRequest->MethodSize }
	);
	if ( pClone->Method == NULL ) {
		goto fail;
	}
	pClone->MethodSize = pRequest->MethodSize;
	sUrl = __xrtHttpRequestTextCopy(
		(xstrview){ pRequest->UrlText, pRequest->UrlSize }
	);
	if ( sUrl == NULL ) {
		goto fail;
	}
	if ( !__xrtHttpRequestTakeUrl(
		pClone, sUrl, pRequest->UrlSize
	) ) {
		xrtFree(sUrl);
		goto fail;
	}
	pClone->Headers = xrtHttpHeadersClone(pRequest->Headers);
	if ( pClone->Headers == NULL ) {
		goto fail;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		if ( pRequest->Trailers != NULL ) {
			pClone->Trailers = xrtHttpHeadersClone(
				pRequest->Trailers
			);
			if ( pClone->Trailers == NULL ) {
				goto fail;
			}
		}
	#endif
	if ( pRequest->Body != NULL ) {
		pClone->Body = xrtHttpBodyRef(pRequest->Body);
		if ( pClone->Body == NULL ) {
			goto fail;
		}
	}
	return pClone;

fail:
	xrtHttpRequestDestroy(pClone);
	return NULL;
}



/* 释放请求拥有的全部动态资源。 */
XRT_API void xrtHttpRequestDestroy(xhttprequest* pRequest)
{
	if ( pRequest == NULL ) {
		return;
	}
	xrtHttpBodyDestroy(pRequest->Body);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		xrtHttpHeadersDestroy(pRequest->Trailers);
	#endif
	xrtHttpHeadersDestroy(pRequest->Headers);
	xrtFree(pRequest->UrlText);
	xrtFree(pRequest->Method);
	memset(pRequest, 0, sizeof(*pRequest));
	xrtFree(pRequest);
}



/* 校验并失败原子地替换方法。 */
XRT_API bool xrtHttpRequestSetMethod(
	xhttprequest* pRequest,
	xstrview Method
)
{
	str sMethod;

	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	if ( !__xrtHttpRequestViewValid(Method) ) {
		return false;
	}
	if ( !xrtHttpTokenValid(Method) ) {
		__xrtHttpRequestError(
			XHTTP_REQUEST_ERROR_METHOD,
			"method",
			"HTTP request method must be a non-empty token"
		);
		return false;
	}
	sMethod = __xrtHttpRequestTextCopy(Method);
	if ( sMethod == NULL ) {
		return false;
	}
	xrtFree(pRequest->Method);
	pRequest->Method = sMethod;
	pRequest->MethodSize = Method.Size;
	return true;
}



/* 返回请求方法的借用视图。 */
XRT_API xstrview xrtHttpRequestMethod(
	const xhttprequest* pRequest
)
{
	if ( pRequest == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pRequest->Method,
		pRequest->MethodSize
	};
}



/* 解析并失败原子地替换 URL。 */
XRT_API bool xrtHttpRequestSetUrl(
	xhttprequest* pRequest,
	xstrview Url
)
{
	str sText;

	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	if ( !__xrtHttpRequestViewValid(Url) ) {
		return false;
	}
	if ( Url.Size == 0 ) {
		__xrtHttpRequestError(
			XHTTP_REQUEST_ERROR_URL,
			"url",
			"HTTP client URL must not be empty"
		);
		return false;
	}
	sText = __xrtHttpRequestTextCopy(Url);
	if ( sText == NULL ) {
		return false;
	}
	if ( !__xrtHttpRequestTakeUrl(
		pRequest,
		sText,
		Url.Size
	) ) {
		xrtFree(sText);
		return false;
	}
	return true;
}



/* 返回原始 URL 文本的借用视图。 */
XRT_API xstrview xrtHttpRequestUrlText(
	const xhttprequest* pRequest
)
{
	if ( pRequest == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pRequest->UrlText,
		pRequest->UrlSize
	};
}



/* 返回借用请求文本的解析 URL。 */
XRT_API const xurl* xrtHttpRequestUrl(
	const xhttprequest* pRequest
)
{
	return pRequest != NULL ? &pRequest->Url : NULL;
}



/* 追加拥有型 Header。 */
XRT_API bool xrtHttpRequestAddHeader(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
)
{
	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	return xrtHttpHeadersAdd(pRequest->Headers, Name, Value);
}



/* 设置首个同名 Header。 */
XRT_API bool xrtHttpRequestSetHeader(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
)
{
	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	return xrtHttpHeadersSet(pRequest->Headers, Name, Value);
}



/* 删除全部同名 Header。 */
XRT_API size_t xrtHttpRequestRemoveHeader(
	xhttprequest* pRequest,
	xstrview Name
)
{
	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return 0;
	}
	return xrtHttpHeadersRemove(pRequest->Headers, Name);
}



/* 返回首个同名 Header。 */
XRT_API const xhttpfield* xrtHttpRequestHeader(
	const xhttprequest* pRequest,
	xstrview Name
)
{
	if ( pRequest == NULL ) {
		return NULL;
	}
	return xrtHttpHeadersGet(pRequest->Headers, Name);
}



/* 返回 Header 数量。 */
XRT_API size_t xrtHttpRequestHeaderCount(
	const xhttprequest* pRequest
)
{
	return pRequest != NULL ?
		xrtHttpHeadersCount(pRequest->Headers) : 0;
}



/* 返回请求拥有的连续只读 Header 数组。 */
XRT_API const xhttpfield* xrtHttpRequestHeaderData(
	const xhttprequest* pRequest
)
{
	return pRequest != NULL ?
		xrtHttpHeadersData(pRequest->Headers) : NULL;
}



/* 返回指定位置的 Header。 */
XRT_API const xhttpfield* xrtHttpRequestHeaderAt(
	const xhttprequest* pRequest,
	size_t iIndex
)
{
	if ( pRequest == NULL ) {
		return NULL;
	}
	return xrtHttpHeadersAt(pRequest->Headers, iIndex);
}



/* 返回请求拥有的可变 Header 容器。 */
XRT_API xhttpheaders* xrtHttpRequestHeaders(
	xhttprequest* pRequest
)
{
	return pRequest != NULL ? pRequest->Headers : NULL;
}



/* 保留并替换正文引用。 */
XRT_API bool xrtHttpRequestSetBody(
	xhttprequest* pRequest,
	xhttpbody* pBody
)
{
	xhttpbody* pNewBody = NULL;

	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	if ( pBody != NULL ) {
		pNewBody = xrtHttpBodyRef(pBody);
		if ( pNewBody == NULL ) {
			return false;
		}
	}
	return __xrtHttpRequestCommitBody(
		pRequest,
		pNewBody,
		(xstrview){ NULL, 0 }
	);
}



/* 复制正文并按需设置 Content-Type。 */
XRT_API bool xrtHttpRequestSetBytes(
	xhttprequest* pRequest,
	xbytesview Data,
	xstrview ContentType
)
{
	xhttpbody* pBody;

	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	pBody = xrtHttpBodyCopy(Data);
	if ( pBody == NULL ) {
		return false;
	}
	return __xrtHttpRequestCommitBody(
		pRequest, pBody, ContentType
	);
}



/* 返回请求借用的正文引用。 */
XRT_API xhttpbody* xrtHttpRequestBody(
	const xhttprequest* pRequest
)
{
	return pRequest != NULL ? pRequest->Body : NULL;
}

#endif
