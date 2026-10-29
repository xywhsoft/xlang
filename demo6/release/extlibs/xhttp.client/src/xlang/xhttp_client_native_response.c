#include "xhttp_client_native_internal.h"



/* point 构造直接接管 Exchange 交付的响应句柄。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_response_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放响应拥有的全部协议元数据和正文。 */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_response_destroy(
	void* pHandle
)
{
	xrtHttpResponseDestroy((xhttpresponse*)pHandle);
}



/* 返回可传给 XRT C API 的原生响应句柄。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_response_native(
	void* pHandle
)
{
	return pHandle;
}



/* 返回响应的整数元数据。 */
XLANG_XHTTP_CLIENT_EXPORT unsigned long long
xhttp_client_response_value(void* pHandle, long long iField)
{
	xhttpresponse* pResponse = xhttpClientResponseRequire(pHandle);

	if ( pResponse == NULL ) {
		return 0u;
	}
	switch ( iField ) {
		case 0:
			return (unsigned long long)xrtHttpResponseVersion(pResponse);
		case 1:
			return (unsigned long long)xrtHttpResponseStatus(pResponse);
		case 2:
			return (unsigned long long)xrtHttpResponseFlags(pResponse);
		case 3:
			return (unsigned long long)xrtHttpResponseHeaderCount(pResponse);
		case 4:
			return (unsigned long long)xrtHttpResponseTrailerCount(pResponse);
		case 5:
			return (unsigned long long)xrtHttpResponseBodyBytes(pResponse);
		case 6:
			return (unsigned long long)xrtHttpResponseWireBodyBytes(pResponse);
		case 7:
			return (unsigned long long)xrtHttpResponseRedirects(pResponse);
		default:
			xhttpClientNativeError(
				XERR_ARGUMENT,
				"read-response-value",
				"Response value selector is invalid"
			);
			return 0u;
	}
}



/* 判断响应状态是否属于成功范围。 */
XLANG_XHTTP_CLIENT_EXPORT bool xhttp_client_response_success(
	void* pHandle
)
{
	xhttpresponse* pResponse = xhttpClientResponseRequire(pHandle);

	return (pResponse != NULL) && xrtHttpResponseSuccess(pResponse);
}



/* 复制 reason phrase 或最终 URL。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_response_text(
	void* pHandle,
	long long iField
)
{
	xhttpresponse* pResponse = xhttpClientResponseRequire(pHandle);
	xstrview Text;

	if ( pResponse == NULL ) {
		return NULL;
	}
	if ( iField == 0 ) {
		Text = xrtHttpResponseReason(pResponse);
	} else if ( iField == 1 ) {
		Text = xrtHttpResponseUrl(pResponse);
	} else {
		xhttpClientNativeError(
			XERR_ARGUMENT,
			"read-response-text",
			"Response text selector is invalid"
		);
		return NULL;
	}
	return xhttpClientNativeBuffer(Text);
}



/* 复制首个同名 Header 或 Trailer 的值。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_response_field(
	void* pHandle,
	const char* sName,
	size_t iName,
	bool bTrailer
)
{
	xhttpresponse* pResponse = xhttpClientResponseRequire(pHandle);
	const xhttpfield* pField;

	if ( pResponse == NULL ) {
		return NULL;
	}
	pField = bTrailer ?
		xrtHttpResponseTrailer(
			pResponse, (xstrview){ sName, iName }
		) :
		xrtHttpResponseHeader(
			pResponse, (xstrview){ sName, iName }
		);
	return pField != NULL ?
		xhttpClientNativeBuffer(pField->Value) : NULL;
}



/* 深复制 Header 或 Trailer，避免借用响应寿命。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_response_fields(
	void* pHandle,
	bool bTrailer
)
{
	xhttpresponse* pResponse = xhttpClientResponseRequire(pHandle);
	const xhttpheaders* pHeaders;

	if ( pResponse == NULL ) {
		return NULL;
	}
	pHeaders = bTrailer ?
		xrtHttpResponseTrailers(pResponse) :
		xrtHttpResponseHeaders(pResponse);
	return pHeaders != NULL ?
		xrtHttpHeadersClone(pHeaders) :
		xrtHttpHeadersCreate(NULL);
}



/* 复制缓冲模式下的完整二进制正文。 */
XLANG_XHTTP_CLIENT_EXPORT void* xhttp_client_response_body(
	void* pHandle
)
{
	xhttpresponse* pResponse = xhttpClientResponseRequire(pHandle);

	return pResponse != NULL ?
		xhttpClientNativeBytes(xrtHttpResponseBody(pResponse)) : NULL;
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_CLIENT_EXPORT void xhttp_client_response_destroy_abi(void* handle)
{
    (void)xhttp_client_response_destroy(handle);
}
