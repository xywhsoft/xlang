#ifndef XLANG_EXTLIB_XHTTP_FORM_DATA_NATIVE_INTERNAL_H
#define XLANG_EXTLIB_XHTTP_FORM_DATA_NATIVE_INTERNAL_H

#include "../../include/xhttp_form_data_package.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_FORM_DATA_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_FORM_DATA_EXPORT
#endif



/* Part 快照独立拥有元数据和正文引用，不受 FormData 后续修改影响。 */
typedef struct xhttp_form_data_native_part {
	str Metadata;
	size_t NameSize;
	size_t FilenameSize;
	size_t ContentTypeSize;
	xhttpbody* Body;
	uint64 Length;
	uint32 Flags;
} xhttp_form_data_native_part;



/* 发布语言桥接自身的结构化错误。 */
void xhttpFormDataNativeError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
);



/* 把借用字节复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpFormDataNativeBuffer(xbytesview Data);



/* 把语言层限制转换为平台 size_t；-1 可选地表示无限。 */
bool xhttpFormDataNativeSize(
	long long iValue,
	bool bAllowUnlimited,
	size_t* pResult
);



/* 从语言层字段构建完整 FormData 配置。 */
bool xhttpFormDataNativeConfig(
	long long iInitialParts,
	long long iMaxParts,
	long long iMaxName,
	long long iMaxFilename,
	long long iMaxContentType,
	long long iMaxMetadata,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	xformdataconfig* pConfig
);



/* 从语言层字段构建完整 multipart 限制。 */
bool xhttpFormDataNativeLimits(
	long long iMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	xmultipartlimits* pLimits
);



/* 深复制一个借用 Part 为稳定语言句柄。 */
xhttp_form_data_native_part* xhttpFormDataNativePartCreate(
	const xformdatapart* pPart
);



#endif
