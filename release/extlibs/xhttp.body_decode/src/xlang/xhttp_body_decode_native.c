#include "../../include/xhttp_body_decode_package.h"

#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_BODY_DECODE_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_BODY_DECODE_EXPORT
#endif



/* 原生结果同时拥有状态和一个独立正文引用。 */
typedef struct xhttp_body_decode_native_result {
	xhttpbody* Body;
	xhttpbodydecoderesult Status;
} xhttp_body_decode_native_result;



/* 发布语言桥接自身的参数错误。 */
static void xhttpBodyDecodeError(cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_ARGUMENT;
	Desc.Domain = "xlang.extlib.xhttp.body_decode";
	Desc.Operation = "decode";
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 将语言整数安全映射为 XRT 自动解码配置。 */
static bool xhttpBodyDecodeConfig(
	xhttpbodydecodeconfig* pConfig,
	long long iFormat,
	long long iOutputLimit,
	long long iGzipHeaderLimit,
	long long iWindowBits,
	long long iReadSize,
	long long iQueueLimit,
	long long iMaxCodings
)
{
	if ( (iFormat < XINFLATE_RAW) ||
		(iFormat > XINFLATE_GZIP) ||
		(iOutputLimit < -1) ||
		(iGzipHeaderLimit < 10) ||
		((unsigned long long)iGzipHeaderLimit > UINT32_MAX) ||
		(iWindowBits < 8) || (iWindowBits > 15) ||
		(iReadSize <= 0) ||
		((unsigned long long)iReadSize >
		 (unsigned long long)SIZE_MAX) ||
		(iQueueLimit < 0) ||
		((unsigned long long)iQueueLimit >
		 (unsigned long long)SIZE_MAX) ||
		(iMaxCodings <= 0) ||
		(iMaxCodings > XHTTP_CONTENT_CODINGS_MAX) ) {
		xhttpBodyDecodeError(
			"body decode configuration cannot be represented"
		);
		return false;
	}
	xrtHttpBodyDecodeConfigInit(pConfig);
	pConfig->Inflate.Inflate.Format = (xinflateformat)iFormat;
	pConfig->Inflate.Inflate.OutputLimit = iOutputLimit < 0 ?
		XINFLATE_OUTPUT_UNLIMITED : (uint64)iOutputLimit;
	pConfig->Inflate.Inflate.GzipHeaderLimit =
		(uint32)iGzipHeaderLimit;
	pConfig->Inflate.Inflate.WindowBits = (uint8)iWindowBits;
	pConfig->Inflate.ReadSize = (size_t)iReadSize;
	pConfig->Inflate.QueueLimit = (size_t)iQueueLimit;
	pConfig->MaxCodings = (size_t)iMaxCodings;
	return true;
}



/* 为成功的 XRT 解码决策创建语言拥有者。 */
static void* xhttpBodyDecodeResult(
	xhttpbody* pBody,
	xhttpbodydecoderesult Status
)
{
	xhttp_body_decode_native_result* pResult;

	if ( Status == XHTTP_BODY_DECODE_ERROR ) {
		return NULL;
	}
	pResult = (xhttp_body_decode_native_result*)xrtCalloc(
		1u, sizeof(*pResult)
	);
	if ( pResult == NULL ) {
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	pResult->Body = pBody;
	pResult->Status = Status;
	return pResult;
}



/* 按单个 Content-Encoding 字段值构建解码正文。 */
XLANG_XHTTP_BODY_DECODE_EXPORT void* xhttp_body_decode_value(
	void* pSource,
	const char* sEncoding,
	size_t iEncoding,
	long long iFormat,
	long long iOutputLimit,
	long long iGzipHeaderLimit,
	long long iWindowBits,
	long long iReadSize,
	long long iQueueLimit,
	long long iMaxCodings
)
{
	xhttpbodydecodeconfig Config;
	xhttpbody* pBody = NULL;
	xhttpbodydecoderesult Status;

	if ( pSource == NULL ) {
		xhttpBodyDecodeError("source body is required");
		return NULL;
	}
	if ( !xrtMemRangeValid(sEncoding, iEncoding) ) {
		xhttpBodyDecodeError("Content-Encoding is not a valid memory range");
		return NULL;
	}
	if ( !xhttpBodyDecodeConfig(
			&Config,
			iFormat,
			iOutputLimit,
			iGzipHeaderLimit,
			iWindowBits,
			iReadSize,
			iQueueLimit,
			iMaxCodings
		) ) {
		return NULL;
	}
	Status = xrtHttpBodyDecode(
		(xhttpbody*)pSource,
		(xstrview){ sEncoding, iEncoding },
		&Config,
		&pBody
	);
	return xhttpBodyDecodeResult(pBody, Status);
}



/* 直接扫描拥有型 Headers 的连续字段数组，不做字符串拼接。 */
XLANG_XHTTP_BODY_DECODE_EXPORT void* xhttp_body_decode_headers(
	void* pSource,
	void* pHeaders,
	long long iFormat,
	long long iOutputLimit,
	long long iGzipHeaderLimit,
	long long iWindowBits,
	long long iReadSize,
	long long iQueueLimit,
	long long iMaxCodings
)
{
	xhttpbodydecodeconfig Config;
	xhttpheaders* pHeaderList = (xhttpheaders*)pHeaders;
	xhttpbody* pBody = NULL;
	xhttpbodydecoderesult Status;

	if ( (pSource == NULL) || (pHeaderList == NULL) ) {
		xhttpBodyDecodeError("source body and Headers are required");
		return NULL;
	}
	if ( !xhttpBodyDecodeConfig(
			&Config,
			iFormat,
			iOutputLimit,
			iGzipHeaderLimit,
			iWindowBits,
			iReadSize,
			iQueueLimit,
			iMaxCodings
		) ) {
		return NULL;
	}
	Status = xrtHttpBodyDecodeFields(
		(xhttpbody*)pSource,
		xrtHttpHeadersData(pHeaderList),
		xrtHttpHeadersCount(pHeaderList),
		&Config,
		&pBody
	);
	return xhttpBodyDecodeResult(pBody, Status);
}



/* point 构造直接接管解码结果。 */
XLANG_XHTTP_BODY_DECODE_EXPORT void* xhttp_body_decode_result_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放结果拥有的正文引用。 */
XLANG_XHTTP_BODY_DECODE_EXPORT void xhttp_body_decode_result_destroy(
	void* pHandle
)
{
	xhttp_body_decode_native_result* pResult =
		(xhttp_body_decode_native_result*)pHandle;

	if ( pResult != NULL ) {
		xrtHttpBodyDestroy(pResult->Body);
		memset(pResult, 0, sizeof(*pResult));
		xrtFree(pResult);
	}
}



/* 返回 XRT 的完整解码决策。 */
XLANG_XHTTP_BODY_DECODE_EXPORT long long xhttp_body_decode_result_status(
	void* pHandle
)
{
	xhttp_body_decode_native_result* pResult =
		(xhttp_body_decode_native_result*)pHandle;

	return pResult != NULL ? pResult->Status : XHTTP_BODY_DECODE_ERROR;
}



/* 返回调用方拥有的结果正文引用。 */
XLANG_XHTTP_BODY_DECODE_EXPORT void* xhttp_body_decode_result_body(
	void* pHandle
)
{
	xhttp_body_decode_native_result* pResult =
		(xhttp_body_decode_native_result*)pHandle;

	return pResult != NULL ? xrtHttpBodyRef(pResult->Body) : NULL;
}
