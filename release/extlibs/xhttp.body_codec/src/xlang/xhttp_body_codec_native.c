#include "../../include/xhttp_body_codec_package.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_BODY_CODEC_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_BODY_CODEC_EXPORT
#endif



/* 发布语言桥接自身的参数错误。 */
static void xhttpBodyCodecError(cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_ARGUMENT;
	Desc.Domain = "xlang.extlib.xhttp.body_codec";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 检查语言整数能否安全映射到正文变换配置。 */
static bool xhttpBodyCodecCommonValid(
	void* pSource,
	long long iOutputLimit,
	long long iWindowBits,
	long long iReadSize,
	long long iQueueLimit,
	cstr sOperation
)
{
	if ( (pSource == NULL) || (iOutputLimit < -1) ||
		(iWindowBits < 8) || (iWindowBits > 15) ||
		(iReadSize <= 0) ||
		((unsigned long long)iReadSize >
		 (unsigned long long)SIZE_MAX) ||
		(iQueueLimit < 0) ||
		((unsigned long long)iQueueLimit >
		 (unsigned long long)SIZE_MAX) ) {
		xhttpBodyCodecError(
			sOperation,
			"body codec configuration cannot be represented"
		);
		return false;
	}
	return true;
}



/* 根据完整 XRT 配置创建流式 Deflate 正文。 */
XLANG_XHTTP_BODY_CODEC_EXPORT void* xhttp_body_codec_deflate(
	void* pSource,
	long long iFormat,
	long long iLevel,
	long long iStrategy,
	long long iOutputLimit,
	long long iWindowBits,
	long long iReadSize,
	long long iQueueLimit
)
{
	xhttpbodydeflateconfig Config;

	if ( !xhttpBodyCodecCommonValid(
		pSource,
		iOutputLimit,
		iWindowBits,
		iReadSize,
		iQueueLimit,
		"deflate"
	) ) {
		return NULL;
	}
	if ( (iFormat < XDEFLATE_RAW) ||
		(iFormat > XDEFLATE_GZIP) ||
		(iLevel < 0) || (iLevel > 10) ||
		(iStrategy < XDEFLATE_STRATEGY_DEFAULT) ||
		(iStrategy > XDEFLATE_STRATEGY_FIXED) ) {
		xhttpBodyCodecError(
			"deflate", "Deflate mode is outside the supported range"
		);
		return NULL;
	}
	xrtHttpBodyDeflateConfigInit(&Config);
	Config.Deflate.Format = (xdeflateformat)iFormat;
	Config.Deflate.Level = (int32)iLevel;
	Config.Deflate.Strategy = (xdeflatestrategy)iStrategy;
	Config.Deflate.OutputLimit = iOutputLimit < 0 ?
		XDEFLATE_OUTPUT_UNLIMITED : (uint64)iOutputLimit;
	Config.Deflate.WindowBits = (uint8)iWindowBits;
	Config.ReadSize = (size_t)iReadSize;
	Config.QueueLimit = (size_t)iQueueLimit;
	return xrtHttpBodyDeflate((xhttpbody*)pSource, &Config);
}



/* 根据完整 XRT 配置创建流式 Inflate 正文。 */
XLANG_XHTTP_BODY_CODEC_EXPORT void* xhttp_body_codec_inflate(
	void* pSource,
	long long iFormat,
	long long iOutputLimit,
	long long iGzipHeaderLimit,
	long long iWindowBits,
	long long iReadSize,
	long long iQueueLimit
)
{
	xhttpbodyinflateconfig Config;

	if ( !xhttpBodyCodecCommonValid(
		pSource,
		iOutputLimit,
		iWindowBits,
		iReadSize,
		iQueueLimit,
		"inflate"
	) ) {
		return NULL;
	}
	if ( (iFormat < XINFLATE_RAW) ||
		(iFormat > XINFLATE_GZIP) ||
		(iGzipHeaderLimit < 10) ||
		((unsigned long long)iGzipHeaderLimit > UINT32_MAX) ) {
		xhttpBodyCodecError(
			"inflate",
			"Inflate mode or gzip header limit is invalid"
		);
		return NULL;
	}
	xrtHttpBodyInflateConfigInit(&Config);
	Config.Inflate.Format = (xinflateformat)iFormat;
	Config.Inflate.OutputLimit = iOutputLimit < 0 ?
		XINFLATE_OUTPUT_UNLIMITED : (uint64)iOutputLimit;
	Config.Inflate.GzipHeaderLimit = (uint32)iGzipHeaderLimit;
	Config.Inflate.WindowBits = (uint8)iWindowBits;
	Config.ReadSize = (size_t)iReadSize;
	Config.QueueLimit = (size_t)iQueueLimit;
	return xrtHttpBodyInflate((xhttpbody*)pSource, &Config);
}
