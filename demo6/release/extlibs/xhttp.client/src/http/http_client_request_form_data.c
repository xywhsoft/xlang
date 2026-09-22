#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_FORM_DATA)

/* 生成 multipart Content-Type 并原子提交正文与字段。 */
bool __xrtHttpRequestCommitFormData(
	xhttprequest* pRequest,
	xhttpbody* pBody,
	const xmultipartboundary* pBoundary
)
{
	char ContentType[
		(2u * XMULTIPART_BOUNDARY_MAX) + 64u
	];
	size_t iContentType;

	if ( !xrtMultipartContentTypeWrite(
		pBoundary,
		ContentType,
		sizeof(ContentType),
		&iContentType
	) ) {
		xrtHttpBodyDestroy(pBody);
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_FORM_DATA,
			"set-form-data",
			"multipart/form-data Content-Type could not be built"
		);
		return false;
	}
	if ( !__xrtHttpRequestCommitBody(
		pRequest,
		pBody,
		(xstrview){ ContentType, iContentType }
	) ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_FORM_DATA,
			"set-form-data",
			"multipart/form-data request metadata could not be committed"
		);
		return false;
	}
	return true;
}



/* 创建给定 boundary 的组合 FormData 正文并提交到请求。 */
XRT_API bool xrtHttpRequestSetFormData(
	xhttprequest* pRequest,
	const xformdata* pForm,
	const xmultipartboundary* pBoundary
)
{
	xhttpbody* pBody;

	if ( (pRequest == NULL) || (pForm == NULL) ||
		(pBoundary == NULL) ) {
		__xrtHttpRequestInvalidArgument();
		return false;
	}
	pBody = xrtFormDataBody(pForm, pBoundary);
	if ( pBody == NULL ) {
		__xrtHttpRequestWrapError(
			XERR_VALUE,
			XHTTP_REQUEST_ERROR_FORM_DATA,
			"set-form-data",
			"multipart/form-data body could not be created"
		);
		return false;
	}
	return __xrtHttpRequestCommitFormData(
		pRequest,
		pBody,
		pBoundary
	);
}

#endif
