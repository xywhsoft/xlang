#ifndef XRT_INTERNAL_CODEC_H
#define XRT_INTERNAL_CODEC_H

#include "xrt_core_internal.h"

#include <xrt.h>



#if defined(XRT_FEATURE_CODEC_HEX) || defined(XRT_FEATURE_CODEC_BASE64) || \
	defined(XRT_FEATURE_CODEC_PERCENT)

/* 设置 Codec 模块共享的结构化错误。 */
static inline void __xrtCodecError(
	xerrkind Kind,
	xcodecerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.codec";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}

#endif



#if defined(XRT_FEATURE_CODEC_PERCENT)

/* 内部 percent 引擎使用完整 ASCII 安全字符位图。 */
typedef xpercentmap xrt_percent_map;



/* 构建 RFC 3986 unreserved 字节位图。 */
void __xrtPercentUnreservedMap(xrt_percent_map* pMap);



/* 判断字节是否属于 RFC 3986 unreserved 集合。 */
bool __xrtPercentUnreserved(uint8 iValue);



/* 从零结尾 ASCII 字符集构建内部安全位图。 */
void __xrtPercentMapInit(xrt_percent_map* pMap, cstr sSafe);



/* 计算指定安全位图和空格规则下的精确编码长度。 */
bool __xrtPercentEncodedSize(
	const uint8* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	size_t* pEncodedSize
);



/* 在已经预检的输入上执行向后 percent 编码。 */
void __xrtPercentEncodeBody(
	const uint8* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	char* sOutput,
	size_t iOutputSize,
	bool bTerminate
);



/* 在已经预检且输出不重叠的路径上顺序编码，并返回写出字节数。 */
size_t __xrtPercentEncodeForwardBody(
	const uint8* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	char* sOutput
);



/* 执行共享的参数、容量、重叠检查和 percent 编码。 */
bool __xrtPercentEncodeCore(
	const void* pData,
	size_t iSize,
	const xrt_percent_map* pSafe,
	bool bSpaceAsPlus,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	bool bTerminate
);



/* 严格验证全部 percent 转义并计算解码长度。 */
bool __xrtPercentDecodedSize(
	xstrview Text,
	bool bPlusAsSpace,
	size_t* pDecodedSize,
	cstr sOperation
);



/* 在已经预检的输入上执行向前 percent 解码。 */
size_t __xrtPercentDecodeBody(
	xstrview Text,
	bool bPlusAsSpace,
	uint8* pOutput
);



/* 在已经验证的文本上比较解码后字节，不创建临时缓冲。 */
bool __xrtPercentDecodedEqualBody(
	xstrview Text,
	bool bPlusAsSpace,
	xbytesview Data
);



/* 执行共享的参数、容量、重叠检查和严格 percent 解码。 */
bool __xrtPercentDecodeCore(
	xstrview Text,
	bool bPlusAsSpace,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	cstr sOperation
);

#endif

#endif
