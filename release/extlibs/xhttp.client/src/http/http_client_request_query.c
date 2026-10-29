#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_QUERY)

/* 编码 QueryParams 并替换请求 URL 的查询组件。 */
XRT_API bool xrtHttpRequestSetQueryParams(
	xhttprequest* pRequest,
	const xqueryparams* pParams
)
{
	xurl Url;
	str sQuery;
	str sUrl;
	size_t iQuery;
	size_t iUrl;

	if ( (pRequest == NULL) || (pParams == NULL) ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	sQuery = xrtQueryParamsBuild(pParams, &iQuery);
	if ( sQuery == NULL ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_QUERY,
			"set-query",
			"HTTP request query parameters could not be encoded"
		);
		return false;
	}
	Url = *xrtHttpRequestUrl(pRequest);
	Url.Flags |= XURL_HAS_QUERY;
	Url.Query = (xstrview){ sQuery, iQuery };
	sUrl = xrtUrlBuild(&Url, &iUrl);
	xrtFree(sQuery);
	if ( sUrl == NULL ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_QUERY,
			"set-query",
			"HTTP request URL could not be rebuilt with query parameters"
		);
		return false;
	}
	if ( !__xrtHttpRequestTakeUrl(
		pRequest,
		sUrl,
		iUrl
	) ) {
		xrtFree(sUrl);
		__xrtHttpRequestWrapError(
			XERR_INTERNAL,
			XHTTP_REQUEST_ERROR_QUERY,
			"set-query",
			"rebuilt HTTP request URL failed validation"
		);
		return false;
	}
	return true;
}

#endif
