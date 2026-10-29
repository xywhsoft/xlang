#include "xhttp_client_native_internal.h"



/* 创建拥有全部可变构建状态的客户端请求。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_create(
	const char* sMethod,
	size_t iMethod,
	const char* sUrl,
	size_t iUrl
)
{
	return xrtHttpRequestCreate(
		(xstrview){ sMethod, iMethod },
		(xstrview){ sUrl, iUrl }
	);
}



/* point 构造直接接管 Request 句柄。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 深复制请求文本和字段，并共享不可变正文引用。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_clone(
	void* pHandle
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);

	return pRequest != NULL ? xrtHttpRequestClone(pRequest) : NULL;
}



/* 释放 Request 及其全部拥有资源。 */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_request_destroy(
	void* pHandle
)
{
	xrtHttpRequestDestroy((xhttprequest*)pHandle);
}



/* 返回可传给 XRT C API 的原生 Request 句柄。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_native(
	void* pHandle
)
{
	return pHandle;
}



/* 复制请求方法或原始 URL 文本。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_text(
	void* pHandle,
	long long iField
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	xstrview Text;

	if ( pRequest == NULL ) {
		return NULL;
	}
	Text = iField == 0 ?
		xrtHttpRequestMethod(pRequest) :
		xrtHttpRequestUrlText(pRequest);
	return xhttpClientNativeBuffer(Text);
}



/* 失败原子地替换请求方法或 URL。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_set_text(
	void* pHandle,
	const char* sText,
	size_t iSize,
	long long iField
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	xstrview Text = { sText, iSize };

	if ( pRequest == NULL ) {
		return false;
	}
	return iField == 0 ?
		xrtHttpRequestSetMethod(pRequest, Text) :
		xrtHttpRequestSetUrl(pRequest, Text);
}



/* 追加或替换请求 Header/Trailer。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_write_field(
	void* pHandle,
	const char* sName,
	size_t iName,
	const char* sValue,
	size_t iValue,
	bool bTrailer,
	bool bSet
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	xstrview Name = { sName, iName };
	xstrview Value = { sValue, iValue };

	if ( pRequest == NULL ) {
		return false;
	}
	if ( bTrailer ) {
		return bSet ?
			xrtHttpRequestSetTrailer(pRequest, Name, Value) :
			xrtHttpRequestAddTrailer(pRequest, Name, Value);
	}
	return bSet ?
		xrtHttpRequestSetHeader(pRequest, Name, Value) :
		xrtHttpRequestAddHeader(pRequest, Name, Value);
}



/* 删除全部同名请求 Header/Trailer。 */
XLANG_XHTTP_CLIENT_EXPORT unsigned long long
xhttp_client_request_remove_field(
	void* pHandle,
	const char* sName,
	size_t iName,
	bool bTrailer
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	xstrview Name = { sName, iName };

	if ( pRequest == NULL ) {
		return 0u;
	}
	return bTrailer ?
		xrtHttpRequestRemoveTrailer(pRequest, Name) :
		xrtHttpRequestRemoveHeader(pRequest, Name);
}



/* 复制首个同名请求 Header/Trailer 值。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_field(
	void* pHandle,
	const char* sName,
	size_t iName,
	bool bTrailer
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	const xhttpfield* pField;

	if ( pRequest == NULL ) {
		return NULL;
	}
	pField = bTrailer ?
		xrtHttpRequestTrailer(
			pRequest, (xstrview){ sName, iName }
		) :
		xrtHttpRequestHeader(
			pRequest, (xstrview){ sName, iName }
		);
	return pField != NULL ?
		xhttpClientNativeBuffer(pField->Value) : NULL;
}



/* 返回请求 Header/Trailer 数量。 */
XLANG_XHTTP_CLIENT_EXPORT unsigned long long
xhttp_client_request_field_count(
	void* pHandle,
	bool bTrailer
)
{
	xhttprequest* pRequest = (xhttprequest*)pHandle;

	if ( pRequest == NULL ) {
		return 0u;
	}
	return bTrailer ?
		xrtHttpRequestTrailerCount(pRequest) :
		xrtHttpRequestHeaderCount(pRequest);
}



/* 深复制请求 Header/Trailer，使语言对象不借用 Request。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_fields(
	void* pHandle,
	bool bTrailer
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	const xhttpheaders* pHeaders;

	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( bTrailer ) {
		pHeaders = xrtHttpRequestTrailers(pRequest);
		return pHeaders != NULL ?
			xrtHttpHeadersClone(pHeaders) :
			xrtHttpHeadersCreate(NULL);
	}
	return xrtHttpHeadersClone(xrtHttpRequestHeaders(pRequest));
}



/* 清空请求 Header/Trailer 并保留已经分配的容器容量。 */
XLANG_XHTTP_CLIENT_EXPORT long long xhttp_client_request_clear_fields(
	void* pHandle,
	bool bTrailer
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	xhttpheaders* pHeaders;

	if ( pRequest == NULL ) {
		return 0;
	}
	if ( bTrailer && (xrtHttpRequestTrailerCount(pRequest) == 0u) ) {
		return 0;
	}
	pHeaders = bTrailer ?
		xrtHttpRequestEditTrailers(pRequest) :
		xrtHttpRequestHeaders(pRequest);
	if ( pHeaders != NULL ) {
		xrtHttpHeadersClear(pHeaders);
	}
	return 0;
}



/* 保留并替换正文引用，空句柄表示清除正文。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_set_body(
	void* pHandle,
	void* pBody
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);

	return (pRequest != NULL) && xrtHttpRequestSetBody(
		pRequest, (xhttpbody*)pBody
	);
}



/* 清除 Request 当前正文引用。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_clear_body(
	void* pHandle
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);

	return (pRequest != NULL) &&
		xrtHttpRequestSetBody(pRequest, NULL);
}



/* 复制二进制正文并按需设置 Content-Type。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_set_bytes(
	void* pHandle,
	void* pData,
	size_t iSize,
	const char* sContentType,
	size_t iContentType
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);

	return (pRequest != NULL) && xrtHttpRequestSetBytes(
		pRequest,
		(xbytesview){ (cbytes)pData, iSize },
		(xstrview){ sContentType, iContentType }
	);
}



/* 增加并返回 Request 当前不可变正文引用。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_request_body(
	void* pHandle
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	xhttpbody* pBody;

	if ( pRequest == NULL ) {
		return NULL;
	}
	pBody = xrtHttpRequestBody(pRequest);
	return pBody != NULL ? xrtHttpBodyRef(pBody) : NULL;
}



/* 补齐接收 HTTP/1 Trailer 所需的 TE 与 Connection 字段。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_accept_trailers(
	void* pHandle
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);

	return (pRequest != NULL) &&
		xrtHttp1RequestAcceptTrailers(pRequest);
}



/* 用 QueryParams 替换 URL 查询或设置 urlencoded 正文。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_set_params(
	void* pHandle,
	void* pParams,
	bool bForm
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);
	const xqueryparams* pQuery = (const xqueryparams*)pParams;

	if ( (pRequest == NULL) || (pQuery == NULL) ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"set-request-params",
			"Request and QueryParams handles must be valid"
		);
		return false;
	}
	return bForm ?
		xrtHttpRequestSetForm(pRequest, pQuery) :
		xrtHttpRequestSetQueryParams(pRequest, pQuery);
}



/* 用明确 boundary 设置零拼接的组合式 FormData 正文。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_set_form_data(
	void* pHandle,
	void* pForm,
	void* pBoundary
)
{
	xhttprequest* pRequest = xhttpClientRequestRequire(pHandle);

	if ( (pRequest == NULL) || (pForm == NULL) ||
		(pBoundary == NULL) ) {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"set-request-form-data",
			"Request, FormData and boundary handles must be valid"
		);
		return false;
	}
	return xrtHttpRequestSetFormData(
		pRequest,
		(const xformdata*)pForm,
		(const xmultipartboundary*)pBoundary
	);
}

XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_request_set_text_body(
    void* handle, xstrview text, xstrview contentType)
{
    return xhttp_client_request_set_bytes(handle, (void*)text.Data, text.Size,
        contentType.Data, contentType.Size);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_request_destroy_abi(void* handle)
{
    (void)xhttp_client_request_destroy(handle);
}
