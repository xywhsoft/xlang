#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM)

/* 编码 QueryParams 并接管其存储作为 urlencoded 固定正文。 */
XRT_API bool xrtHttpRequestSetForm(
	xhttprequest* pRequest,
	const xqueryparams* pParams
)
{
	str sForm;
	xhttpbody* pBody;
	size_t iForm;

	if ( (pRequest == NULL) || (pParams == NULL) ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	sForm = xrtQueryParamsBuild(pParams, &iForm);
	if ( sForm == NULL ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_FORM,
			"set-form",
			"HTTP request form parameters could not be encoded"
		);
		return false;
	}
	pBody = xrtHttpBodyTake(sForm, iForm);
	if ( pBody == NULL ) {
		xrtFree(sForm);
		__xrtHttpRequestWrapError(
			XERR_MEMORY,
			XHTTP_REQUEST_ERROR_FORM,
			"set-form",
			"HTTP request form body could not be created"
		);
		return false;
	}
	if ( !__xrtHttpRequestCommitBody(
		pRequest,
		pBody,
		XRT_STR_LITERAL("application/x-www-form-urlencoded")
	) ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_FORM,
			"set-form",
			"HTTP request form metadata could not be committed"
		);
		return false;
	}
	return true;
}

#endif
