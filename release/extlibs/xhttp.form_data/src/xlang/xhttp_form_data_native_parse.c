#include "xhttp_form_data_native_internal.h"



/* 解析完整正文和显式 boundary 为拥有型 FormData。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_parse(
	void* pData,
	size_t iSize,
	void* pBoundary,
	long long iInitialParts,
	long long iMaxParts,
	long long iMaxName,
	long long iMaxFilename,
	long long iMaxContentType,
	long long iMaxMetadata,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	long long iMultipartMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMultipartMaxPartBytes,
	long long iMultipartMaxBodyBytes
)
{
	xformdataconfig Config;
	xmultipartlimits Limits;
	xmultiparterrorinfo Error;

	if ( (pBoundary == NULL) ||
		!xrtMemRangeValid(pData, iSize) ||
		!xhttpFormDataNativeConfig(
			iInitialParts,
			iMaxParts,
			iMaxName,
			iMaxFilename,
			iMaxContentType,
			iMaxMetadata,
			iMaxPartBytes,
			iMaxBodyBytes,
			&Config
		) || !xhttpFormDataNativeLimits(
			iMultipartMaxParts,
			iMaxHeaders,
			iMaxDelimiterBytes,
			iMaxHeaderBytes,
			iMultipartMaxPartBytes,
			iMultipartMaxBodyBytes,
			&Limits
		) ) {
		if ( pBoundary == NULL ) {
			xhttpFormDataNativeError(
				XERR_ARGUMENT,
				20,
				"parse",
				"FormData boundary is required"
			);
		}
		return NULL;
	}
	return xrtFormDataParse(
		(xbytesview){ (cbytes)pData, iSize },
		(const xmultipartboundary*)pBoundary,
		&Config,
		&Limits,
		&Error
	);
}



/* 从 Content-Type 提取 boundary 并解析完整正文。 */
XLANG_XHTTP_FORM_DATA_EXPORT void* xhttp_form_data_parse_content_type(
	const char* sContentType,
	size_t iContentType,
	void* pData,
	size_t iSize,
	long long iInitialParts,
	long long iMaxParts,
	long long iMaxName,
	long long iMaxFilename,
	long long iMaxContentTypeLimit,
	long long iMaxMetadata,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	long long iMultipartMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMultipartMaxPartBytes,
	long long iMultipartMaxBodyBytes
)
{
	xformdataconfig Config;
	xmultipartlimits Limits;
	xmultiparterrorinfo Error;

	if ( !xrtMemRangeValid(sContentType, iContentType) ||
		!xrtMemRangeValid(pData, iSize) ||
		!xhttpFormDataNativeConfig(
			iInitialParts,
			iMaxParts,
			iMaxName,
			iMaxFilename,
			iMaxContentTypeLimit,
			iMaxMetadata,
			iMaxPartBytes,
			iMaxBodyBytes,
			&Config
		) || !xhttpFormDataNativeLimits(
			iMultipartMaxParts,
			iMaxHeaders,
			iMaxDelimiterBytes,
			iMaxHeaderBytes,
			iMultipartMaxPartBytes,
			iMultipartMaxBodyBytes,
			&Limits
		) ) {
		return NULL;
	}
	return xrtFormDataParseContentType(
		(xstrview){ sContentType, iContentType },
		(xbytesview){ (cbytes)pData, iSize },
		&Config,
		&Limits,
		&Error
	);
}
