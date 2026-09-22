#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)

/* 建立客户端响应读取错误并保留借用原因链。 */
void __xrtHttpResponseSetError(
	xerrkind Kind,
	xhttpresponseerror Code,
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
	Desc.Domain = "xrt.http.client.response";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 取得当前错误所有权，再用稳定响应错误域包裹。 */
void __xrtHttpResponseWrapError(
	xerrkind DefaultKind,
	xhttpresponseerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrkind Kind = pCause != NULL ?
		xrtErrorKind(pCause) : DefaultKind;

	__xrtHttpResponseSetError(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	if ( (xrtGetError() == NULL) && (pCause != NULL) ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 设置响应实现使用的通用参数错误。 */
static void __xrtHttpResponseInvalidArgument(void)
{
	__xrtHttpResponseSetError(
		XERR_ARGUMENT,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"response",
		"invalid argument",
		NULL
	);
}



/* 设置响应实现使用的大小溢出错误。 */
static void __xrtHttpResponseSizeOverflow(void)
{
	__xrtHttpResponseSetError(
		XERR_RANGE,
		XHTTP_RESPONSE_ERROR_ARGUMENT,
		"response",
		"size overflow",
		NULL
	);
}



/* 验证空 Trailer 容器查询使用的字段名。 */
static bool __xrtHttpResponseLookupNameValid(xstrview Name)
{
	if ( !xrtMemRangeValid(Name.Data, Name.Size) ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		__xrtHttpResponseSetError(
			XERR_VALUE,
			XHTTP_RESPONSE_ERROR_HEADER,
			"lookup-trailer",
			"HTTP trailer name must be a non-empty token",
			NULL
		);
		return false;
	}
	return true;
}



/* 校验输出不会破坏响应对象、文本、字段或正文。 */
bool __xrtHttpResponseOutputValid(
	const xhttpresponse* pResponse,
	const void* pOutput,
	size_t iSize
)
{
	size_t iReasonStorage;
	size_t iUrlStorage;

	if ( !xrtMemRangeValid(pResponse, sizeof(*pResponse)) ||
		!xrtMemRangeValid(pOutput, iSize) ||
		(pResponse->Reason == NULL) ||
		(pResponse->ReasonSize == SIZE_MAX) ||
		(pResponse->Headers == NULL) ||
		(pResponse->BodySize > pResponse->BodyCapacity) ||
		((pResponse->Body == NULL) &&
		 (pResponse->BodyCapacity != 0)) ||
		((pResponse->Url == NULL) && (pResponse->UrlSize != 0)) ||
		((pResponse->Url != NULL) &&
		 (pResponse->UrlSize == SIZE_MAX)) ) {
		return false;
	}
	iReasonStorage = pResponse->ReasonSize + 1u;
	iUrlStorage = pResponse->Url != NULL ?
		(pResponse->UrlSize + 1u) : 0;
	if ( xrtMemRangesOverlap(
		pResponse, sizeof(*pResponse), pOutput, iSize
	) || xrtMemRangesOverlap(
		pResponse->Reason,
		iReasonStorage,
		pOutput,
		iSize
	) || xrtMemRangesOverlap(
		pResponse->Body,
		pResponse->BodyCapacity,
		pOutput,
		iSize
	) || xrtMemRangesOverlap(
		pResponse->Url,
		iUrlStorage,
		pOutput,
		iSize
	) || __xrtHttpHeadersOwnedOverlap(
		pResponse->Headers, pOutput, iSize
	) || __xrtHttpHeadersOwnedOverlap(
		pResponse->Trailers, pOutput, iSize
	) ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( ((pResponse->OriginalEncoding == NULL) &&
			 (pResponse->OriginalEncodingSize != 0)) ||
			((pResponse->OriginalEncoding != NULL) &&
			 (pResponse->OriginalEncodingSize == SIZE_MAX)) ||
			xrtMemRangesOverlap(
			pResponse->OriginalEncoding,
			pResponse->OriginalEncoding != NULL ?
				(pResponse->OriginalEncodingSize + 1u) : 0,
			pOutput,
			iSize
		) ) {
			return false;
		}
	#endif
	return true;
}



/* 复制允许为空的响应文本并附加零字符。 */
static str __xrtHttpResponseTextCopy(xstrview Text)
{
	str sCopy;

	if ( !xrtMemRangeValid(Text.Data, Text.Size) ) {
		__xrtHttpResponseInvalidArgument();
		return NULL;
	}
	if ( Text.Size == SIZE_MAX ) {
		__xrtHttpResponseSizeOverflow();
		return NULL;
	}
	sCopy = (str)xrtMalloc(Text.Size + 1);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( Text.Size != 0 ) {
		memcpy(sCopy, Text.Data, Text.Size);
	}
	sCopy[Text.Size] = '\0';
	return sCopy;
}



/* 验证响应起始行元数据。 */
static bool __xrtHttpResponseHeadValid(
	xhttpversion Version,
	uint16 iStatus,
	xstrview Reason
)
{
	if ( ((Version != XHTTP_VERSION_1_0) &&
			(Version != XHTTP_VERSION_1_1)) ||
		(iStatus < 100) || (iStatus > 999) ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	if ( !xrtHttpFieldValueValid(Reason) ) {
		return false;
	}
	return true;
}



/* 扩大按需正文缓冲，不给空响应预留固定空间。 */
static bool __xrtHttpResponseReserveBody(
	xhttpresponse* pResponse,
	size_t iRequired
)
{
	size_t iCapacity;
	uint8* pBody;

	if ( iRequired <= pResponse->BodyCapacity ) {
		return true;
	}
	iCapacity = pResponse->BodyCapacity;
	if ( iCapacity == 0 ) {
		iCapacity = iRequired;
	}
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (SIZE_MAX / 2) ?
			iRequired : (iCapacity * 2);

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pBody = (uint8*)xrtRealloc(
		pResponse->Body, iCapacity
	);
	if ( pBody == NULL ) {
		return false;
	}
	pResponse->Body = pBody;
	pResponse->BodyCapacity = iCapacity;
	return true;
}



/* 创建客户端事务内部可填充的响应对象。 */
xhttpresponse* __xrtHttpResponseCreate(
	xhttpversion Version,
	uint16 iStatus,
	xstrview Reason,
	const xhttpheadersconfig* pHeaders
)
{
	xhttpresponse* pResponse;

	if ( !__xrtHttpResponseHeadValid(
		Version, iStatus, Reason
	) ) {
		return NULL;
	}
	pResponse = (xhttpresponse*)xrtCalloc(
		1, sizeof(*pResponse)
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	pResponse->Version = Version;
	pResponse->Status = iStatus;
	pResponse->Reason = __xrtHttpResponseTextCopy(Reason);
	pResponse->ReasonSize = Reason.Size;
	pResponse->Headers = xrtHttpHeadersCreate(pHeaders);
	if ( (pResponse->Reason == NULL) ||
		(pResponse->Headers == NULL) ) {
		xrtHttpResponseDestroy(pResponse);
		return NULL;
	}
	return pResponse;
}



/* 追加响应 Header。 */
bool __xrtHttpResponseAddHeader(
	xhttpresponse* pResponse,
	xstrview Name,
	xstrview Value
)
{
	if ( pResponse == NULL ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	return xrtHttpHeadersAdd(
		pResponse->Headers, Name, Value
	);
}



/* 追加响应 trailer。 */
bool __xrtHttpResponseAddTrailer(
	xhttpresponse* pResponse,
	const xhttpheadersconfig* pConfig,
	xstrview Name,
	xstrview Value
)
{
	xhttpheaders* pTrailers;

	if ( pResponse == NULL ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	if ( pResponse->Trailers != NULL ) {
		return xrtHttpHeadersAdd(
			pResponse->Trailers, Name, Value
		);
	}
	pTrailers = xrtHttpHeadersCreate(pConfig);
	if ( pTrailers == NULL ) {
		return false;
	}
	if ( !xrtHttpHeadersAdd(pTrailers, Name, Value) ) {
		xrtHttpHeadersDestroy(pTrailers);
		return false;
	}
	pResponse->Trailers = pTrailers;
	return true;
}



/* 复制响应正文，并由调用方决定是否同时累计交付字节数。 */
static bool __xrtHttpResponseAppendBodyData(
	xhttpresponse* pResponse,
	xbytesview Data,
	bool bCount
)
{
	size_t iRequired;

	if ( (pResponse == NULL) ||
		!xrtMemRangeValid(Data.Data, Data.Size) ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	if ( Data.Size == 0 ) {
		return true;
	}
	if ( (pResponse->BodySize > (SIZE_MAX - Data.Size)) ||
		(bCount &&
		 (pResponse->BodyBytes >
		  (UINT64_MAX - (uint64)Data.Size))) ) {
		__xrtHttpResponseSizeOverflow();
		return false;
	}
	iRequired = pResponse->BodySize + Data.Size;
	if ( !__xrtHttpResponseReserveBody(
		pResponse, iRequired
	) ) {
		return false;
	}
	memcpy(
		pResponse->Body + pResponse->BodySize,
		Data.Data,
		Data.Size
	);
	pResponse->BodySize = iRequired;
	if ( bCount ) {
		pResponse->BodyBytes += (uint64)Data.Size;
	}
	return true;
}



/* 缓冲正文并累计调用方可见正文长度。 */
bool __xrtHttpResponseAppendBody(
	xhttpresponse* pResponse,
	xbytesview Data
)
{
	if ( (pResponse == NULL) ||
		((pResponse->Flags &
		  XHTTP_RESPONSE_STREAMED) != 0) ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	return __xrtHttpResponseAppendBodyData(
		pResponse,
		Data,
		true
	);
}



/* 为内部流式包装器缓冲正文，交付长度仍由 Exchange 统一累计。 */
bool __xrtHttpResponseBufferDeliveredBody(
	xhttpresponse* pResponse,
	xbytesview Data
)
{
	if ( (pResponse == NULL) ||
		((pResponse->Flags &
		  XHTTP_RESPONSE_STREAMED) == 0) ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	return __xrtHttpResponseAppendBodyData(
		pResponse,
		Data,
		false
	);
}



/* 累计流式交付正文而不分配响应正文缓冲。 */
bool __xrtHttpResponseDeliverBody(
	xhttpresponse* pResponse,
	uint64 iBytes
)
{
	if ( (pResponse == NULL) ||
		((pResponse->Flags & XHTTP_RESPONSE_STREAMED) == 0) ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	if ( pResponse->BodyBytes > (UINT64_MAX - iBytes) ) {
		__xrtHttpResponseSizeOverflow();
		return false;
	}
	pResponse->BodyBytes += iBytes;
	return true;
}



/* 累计线上编码正文载荷长度。 */
bool __xrtHttpResponseAddWireBody(
	xhttpresponse* pResponse,
	uint64 iBytes
)
{
	if ( pResponse == NULL ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	if ( pResponse->WireBodyBytes > (UINT64_MAX - iBytes) ) {
		__xrtHttpResponseSizeOverflow();
		return false;
	}
	pResponse->WireBodyBytes += iBytes;
	return true;
}



/* 失败原子地替换最终有效 URL。 */
bool __xrtHttpResponseSetUrl(
	xhttpresponse* pResponse,
	xstrview Url
)
{
	str sUrl;

	if ( pResponse == NULL ) {
		__xrtHttpResponseInvalidArgument();
		return false;
	}
	sUrl = __xrtHttpResponseTextCopy(Url);
	if ( sUrl == NULL ) {
		return false;
	}
	xrtFree(pResponse->Url);
	pResponse->Url = sUrl;
	pResponse->UrlSize = Url.Size;
	return true;
}



/* 保存高层客户端已经完成的重定向跳数。 */
void __xrtHttpResponseSetRedirects(
	xhttpresponse* pResponse,
	size_t iRedirects
)
{
	if ( pResponse != NULL ) {
		pResponse->Redirects = iRedirects;
	}
}



/* 恢复内部包装器代理前调用方选择的缓冲响应语义。 */
void __xrtHttpResponseSetBuffered(xhttpresponse* pResponse)
{
	if ( pResponse != NULL ) {
		pResponse->Flags &=
			~(uint32)XHTTP_RESPONSE_STREAMED;
	}
}



/* 增加客户端已经确认的响应标志。 */
void __xrtHttpResponseSetFlags(
	xhttpresponse* pResponse,
	uint32 iFlags
)
{
	if ( pResponse != NULL ) {
		pResponse->Flags |= iFlags;
	}
}



/* 销毁客户端响应。 */
XRT_API void xrtHttpResponseDestroy(xhttpresponse* pResponse)
{
	if ( pResponse == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		xrtFree(pResponse->OriginalEncoding);
	#endif
	xrtFree(pResponse->Url);
	xrtFree(pResponse->Body);
	xrtHttpHeadersDestroy(pResponse->Trailers);
	xrtHttpHeadersDestroy(pResponse->Headers);
	xrtFree(pResponse->Reason);
	memset(pResponse, 0, sizeof(*pResponse));
	xrtFree(pResponse);
}



/* 返回响应版本。 */
XRT_API xhttpversion xrtHttpResponseVersion(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->Version : 0;
}



/* 返回响应状态码。 */
XRT_API uint16 xrtHttpResponseStatus(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->Status : 0;
}



/* 判断响应是否属于成功状态范围。 */
XRT_API bool xrtHttpResponseSuccess(
	const xhttpresponse* pResponse
)
{
	return (pResponse != NULL) &&
		(pResponse->Status >= 200) &&
		(pResponse->Status <= 299);
}



/* 返回借用 reason phrase。 */
XRT_API xstrview xrtHttpResponseReason(
	const xhttpresponse* pResponse
)
{
	if ( pResponse == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pResponse->Reason,
		pResponse->ReasonSize
	};
}



/* 返回响应标志。 */
XRT_API uint32 xrtHttpResponseFlags(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ?
		pResponse->Flags : XHTTP_RESPONSE_NONE;
}



/* 返回首个同名 Header。 */
XRT_API const xhttpfield* xrtHttpResponseHeader(
	const xhttpresponse* pResponse,
	xstrview Name
)
{
	return pResponse != NULL ?
		xrtHttpHeadersGet(pResponse->Headers, Name) : NULL;
}



/* 返回 Header 数量。 */
XRT_API size_t xrtHttpResponseHeaderCount(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ?
		xrtHttpHeadersCount(pResponse->Headers) : 0;
}



/* 返回响应拥有的连续只读 Header 数组。 */
XRT_API const xhttpfield* xrtHttpResponseHeaderData(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ?
		xrtHttpHeadersData(pResponse->Headers) : NULL;
}



/* 返回指定位置的 Header。 */
XRT_API const xhttpfield* xrtHttpResponseHeaderAt(
	const xhttpresponse* pResponse,
	size_t iIndex
)
{
	return pResponse != NULL ?
		xrtHttpHeadersAt(pResponse->Headers, iIndex) : NULL;
}



/* 返回响应拥有的只读 Header 容器。 */
XRT_API const xhttpheaders* xrtHttpResponseHeaders(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->Headers : NULL;
}



/* 返回首个同名 trailer。 */
XRT_API const xhttpfield* xrtHttpResponseTrailer(
	const xhttpresponse* pResponse,
	xstrview Name
)
{
	if ( pResponse == NULL ) {
		return NULL;
	}
	if ( pResponse->Trailers == NULL ) {
		(void)__xrtHttpResponseLookupNameValid(Name);
		return NULL;
	}
	return xrtHttpHeadersGet(pResponse->Trailers, Name);
}



/* 返回 trailer 数量。 */
XRT_API size_t xrtHttpResponseTrailerCount(
	const xhttpresponse* pResponse
)
{
	return ((pResponse != NULL) &&
		(pResponse->Trailers != NULL)) ?
		xrtHttpHeadersCount(pResponse->Trailers) : 0;
}



/* 返回响应拥有的连续只读 trailer 数组。 */
XRT_API const xhttpfield* xrtHttpResponseTrailerData(
	const xhttpresponse* pResponse
)
{
	return ((pResponse != NULL) &&
		(pResponse->Trailers != NULL)) ?
		xrtHttpHeadersData(pResponse->Trailers) : NULL;
}



/* 返回指定位置的 trailer。 */
XRT_API const xhttpfield* xrtHttpResponseTrailerAt(
	const xhttpresponse* pResponse,
	size_t iIndex
)
{
	return ((pResponse != NULL) &&
		(pResponse->Trailers != NULL)) ?
		xrtHttpHeadersAt(pResponse->Trailers, iIndex) : NULL;
}



/* 返回响应拥有的只读 Trailer 容器。 */
XRT_API const xhttpheaders* xrtHttpResponseTrailers(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->Trailers : NULL;
}



/* 返回缓冲正文视图。 */
XRT_API xbytesview xrtHttpResponseBody(
	const xhttpresponse* pResponse
)
{
	if ( pResponse == NULL ) {
		return (xbytesview){ NULL, 0 };
	}
	return (xbytesview){
		pResponse->Body,
		pResponse->BodySize
	};
}



/* 返回交付正文总字节数。 */
XRT_API uint64 xrtHttpResponseBodyBytes(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->BodyBytes : 0;
}



/* 返回线上编码正文载荷总字节数。 */
XRT_API uint64 xrtHttpResponseWireBodyBytes(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ?
		pResponse->WireBodyBytes : 0;
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

/* 返回自动解码前的完整 Content-Encoding。 */
XRT_API xstrview xrtHttpResponseOriginalEncoding(
	const xhttpresponse* pResponse
)
{
	if ( pResponse == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pResponse->OriginalEncoding,
		pResponse->OriginalEncodingSize
	};
}



/* 接管原始编码文本并删除已经失效的线路表示 Header。 */
void __xrtHttpResponseSetDecoded(
	xhttpresponse* pResponse,
	str sOriginalEncoding,
	size_t iOriginalEncodingSize
)
{
	if ( pResponse == NULL ) {
		xrtFree(sOriginalEncoding);
		return;
	}
	xrtFree(pResponse->OriginalEncoding);
	pResponse->OriginalEncoding = sOriginalEncoding;
	pResponse->OriginalEncodingSize =
		iOriginalEncodingSize;
	(void)xrtHttpHeadersRemove(
		pResponse->Headers,
		XRT_STR_LITERAL("Content-Encoding")
	);
	(void)xrtHttpHeadersRemove(
		pResponse->Headers,
		XRT_STR_LITERAL("Content-Length")
	);
	pResponse->Flags |=
		(uint32)XHTTP_RESPONSE_DECOMPRESSED;
}



/* 覆盖 Exchange 按线路片段累计的正文长度。 */
void __xrtHttpResponseSetBodyBytes(
	xhttpresponse* pResponse,
	uint64 iBodyBytes
)
{
	if ( pResponse != NULL ) {
		pResponse->BodyBytes = iBodyBytes;
	}
}

#endif



/* 复制正文并附加零字符。 */
XRT_API str xrtHttpResponseBodyText(
	const xhttpresponse* pResponse
)
{
	xbytesview Body;
	str sText;

	if ( pResponse == NULL ) {
		__xrtHttpResponseInvalidArgument();
		return NULL;
	}
	Body = xrtHttpResponseBody(pResponse);
	if ( Body.Size == SIZE_MAX ) {
		__xrtHttpResponseSizeOverflow();
		return NULL;
	}
	sText = (str)xrtMalloc(Body.Size + 1);
	if ( sText == NULL ) {
		return NULL;
	}
	if ( Body.Size != 0 ) {
		memcpy(sText, Body.Data, Body.Size);
	}
	sText[Body.Size] = '\0';
	return sText;
}



/* 返回最终有效 URL。 */
XRT_API xstrview xrtHttpResponseUrl(
	const xhttpresponse* pResponse
)
{
	if ( pResponse == NULL ) {
		return (xstrview){ NULL, 0 };
	}
	return (xstrview){
		pResponse->Url,
		pResponse->UrlSize
	};
}



/* 返回高层客户端实际跟随的重定向跳数。 */
XRT_API size_t xrtHttpResponseRedirects(
	const xhttpresponse* pResponse
)
{
	return pResponse != NULL ? pResponse->Redirects : 0;
}

#endif
