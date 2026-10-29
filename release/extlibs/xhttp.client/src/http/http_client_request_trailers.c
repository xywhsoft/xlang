#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)



/* 校验尚未创建 Trailer 容器时的查询字段名。 */
static bool __xrtHttpRequestTrailerNameValid(xstrview Name)
{
	if ( !xrtMemRangeValid(Name.Data, Name.Size) ) {
		xrtSetErrorInfo(
			XERR_ARGUMENT,
			"http.request",
			0,
			"invalid argument"
		);
		return false;
	}
	if ( !xrtHttpTokenValid(Name) ) {
		xrtSetErrorInfo(
			XERR_VALUE,
			"http.request",
			0,
			"HTTP trailer name must be a non-empty token"
		);
		return false;
	}
	return true;
}



/* 首次修改时创建 Trailer 容器，失败时保持请求为空状态。 */
static xhttpheaders* __xrtHttpRequestTrailersCreate(
	xhttprequest* pRequest
)
{
	xhttpheaders* pTrailers;

	if ( pRequest->Trailers != NULL ) {
		return pRequest->Trailers;
	}
	pTrailers = xrtHttpHeadersCreate(NULL);
	if ( pTrailers == NULL ) {
		return NULL;
	}
	pRequest->Trailers = pTrailers;
	return pTrailers;
}



/* 首次追加时把容器创建与字段插入作为一个提交单元。 */
static bool __xrtHttpRequestTrailerAdd(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
)
{
	xhttpheaders* pTrailers;

	if ( pRequest->Trailers != NULL ) {
		return xrtHttpHeadersAdd(
			pRequest->Trailers, Name, Value
		);
	}
	pTrailers = xrtHttpHeadersCreate(NULL);
	if ( pTrailers == NULL ) {
		return false;
	}
	if ( !xrtHttpHeadersAdd(pTrailers, Name, Value) ) {
		xrtHttpHeadersDestroy(pTrailers);
		return false;
	}
	pRequest->Trailers = pTrailers;
	return true;
}



/* 首次设置时把容器创建与字段插入作为一个提交单元。 */
static bool __xrtHttpRequestTrailerSet(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
)
{
	xhttpheaders* pTrailers;

	if ( pRequest->Trailers != NULL ) {
		return xrtHttpHeadersSet(
			pRequest->Trailers, Name, Value
		);
	}
	pTrailers = xrtHttpHeadersCreate(NULL);
	if ( pTrailers == NULL ) {
		return false;
	}
	if ( !xrtHttpHeadersSet(pTrailers, Name, Value) ) {
		xrtHttpHeadersDestroy(pTrailers);
		return false;
	}
	pRequest->Trailers = pTrailers;
	return true;
}



/* 清空实际 Trailer 字段但保留已经按需建立的容器容量。 */
void __xrtHttpRequestClearTrailers(xhttprequest* pRequest)
{
	if ( (pRequest != NULL) &&
		(pRequest->Trailers != NULL) ) {
		xrtHttpHeadersClear(pRequest->Trailers);
	}
}



/* 追加拥有型 Trailer。 */
XRT_API bool xrtHttpRequestAddTrailer(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
)
{
	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	return __xrtHttpRequestTrailerAdd(
		pRequest, Name, Value
	);
}



/* 设置并折叠同名 Trailer。 */
XRT_API bool xrtHttpRequestSetTrailer(
	xhttprequest* pRequest,
	xstrview Name,
	xstrview Value
)
{
	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	return __xrtHttpRequestTrailerSet(
		pRequest, Name, Value
	);
}



/* 删除全部同名 Trailer。 */
XRT_API size_t xrtHttpRequestRemoveTrailer(
	xhttprequest* pRequest,
	xstrview Name
)
{
	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return 0;
	}
	if ( pRequest->Trailers == NULL ) {
		(void)__xrtHttpRequestTrailerNameValid(Name);
		return 0;
	}
	return xrtHttpHeadersRemove(pRequest->Trailers, Name);
}



/* 返回首个同名 Trailer。 */
XRT_API const xhttpfield* xrtHttpRequestTrailer(
	const xhttprequest* pRequest,
	xstrview Name
)
{
	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( pRequest->Trailers == NULL ) {
		(void)__xrtHttpRequestTrailerNameValid(Name);
		return NULL;
	}
	return xrtHttpHeadersGet(pRequest->Trailers, Name);
}



/* 返回 Trailer 数量。 */
XRT_API size_t xrtHttpRequestTrailerCount(
	const xhttprequest* pRequest
)
{
	return (pRequest != NULL) &&
		(pRequest->Trailers != NULL) ?
		xrtHttpHeadersCount(pRequest->Trailers) : 0;
}



/* 返回请求拥有的连续只读 Trailer 数组。 */
XRT_API const xhttpfield* xrtHttpRequestTrailerData(
	const xhttprequest* pRequest
)
{
	return (pRequest != NULL) &&
		(pRequest->Trailers != NULL) ?
		xrtHttpHeadersData(pRequest->Trailers) : NULL;
}



/* 返回指定位置的 Trailer。 */
XRT_API const xhttpfield* xrtHttpRequestTrailerAt(
	const xhttprequest* pRequest,
	size_t iIndex
)
{
	return (pRequest != NULL) &&
		(pRequest->Trailers != NULL) ?
		xrtHttpHeadersAt(pRequest->Trailers, iIndex) : NULL;
}



/* 返回只读 Trailer 容器且不触发创建。 */
XRT_API const xhttpheaders* xrtHttpRequestTrailers(
	const xhttprequest* pRequest
)
{
	return pRequest != NULL ? pRequest->Trailers : NULL;
}



/* 按需创建并返回可修改 Trailer 容器。 */
XRT_API xhttpheaders* xrtHttpRequestEditTrailers(
	xhttprequest* pRequest
)
{
	if ( pRequest == NULL ) {
		__xrtHttpRequestInvalidArgument();
		return NULL;
	}
	return __xrtHttpRequestTrailersCreate(pRequest);
}

#endif
