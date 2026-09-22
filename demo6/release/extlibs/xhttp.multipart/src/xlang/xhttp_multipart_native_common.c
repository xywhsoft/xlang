#include "xhttp_multipart_native_internal.h"



/* 名称和文件名写出共享的轻量上下文。 */
typedef struct xhttp_multipart_native_part_text {
	const xmultipartpart* Part;
	bool FileName;
} xhttp_multipart_native_part_text;



/* 发布语言桥接自己的结构化错误。 */
void xhttpMultipartNativeError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = iCode;
	Desc.Domain = "xlang.extlib.xhttp.multipart";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 把借用字节复制为可由 xlang Buffer 接管的对象。 */
xbuffer* xhttpMultipartNativeBuffer(xbytesview Data)
{
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		xhttpMultipartNativeError(
			XERR_ARGUMENT,
			1,
			"copy-view",
			"multipart byte view is invalid"
		);
		return NULL;
	}
	return xrtBufferFrom(Data);
}



/* 接管 XRT 分配的字节，并在构建失败时负责释放。 */
xbuffer* xhttpMultipartNativeBufferTake(
	bytes pData,
	size_t iSize,
	size_t iCapacity
)
{
	xbuffer* pBuffer;

	if ( (pData == NULL) || (iCapacity < iSize) ) {
		xrtFree(pData);
		xhttpMultipartNativeError(
			XERR_ARGUMENT,
			2,
			"take-text",
			"multipart owned text range is invalid"
		);
		return NULL;
	}
	pBuffer = xrtBufferCreateTake(&pData, iSize, iCapacity);
	if ( pBuffer == NULL ) {
		xrtFree(pData);
	}
	return pBuffer;
}



/* 对无部分结果写出器执行测量和精确 Buffer 构建。 */
xbuffer* xhttpMultipartNativeBuild(
	xhttp_multipart_native_write_proc pWrite,
	const void* pContext
)
{
	xbuffer* pBuffer;
	bytes pOutput;
	size_t iSize = 0u;
	size_t iWritten = 0u;

	if ( (pWrite == NULL) ||
		!pWrite(pContext, NULL, 0u, &iSize) ) {
		return NULL;
	}
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL ) {
		return NULL;
	}
	if ( iSize == 0u ) {
		return pBuffer;
	}
	pOutput = xrtBufferAdd(pBuffer, iSize);
	if ( pOutput == NULL ) {
		xrtBufferDestroy(pBuffer);
		return NULL;
	}
	if ( !pWrite(pContext, pOutput, iSize, &iWritten) ||
		(iWritten != iSize) ) {
		xrtBufferDestroy(pBuffer);
		if ( iWritten != iSize ) {
			xhttpMultipartNativeError(
				XERR_INTERNAL,
				2,
				"build-output",
				"multipart writer changed its measured size"
			);
		}
		return NULL;
	}
	return pBuffer;
}



/* 把语言层 -1 或非负预算转换成平台 size_t。 */
bool xhttpMultipartNativeLimit(
	long long iValue,
	size_t* pResult
)
{
	if ( pResult == NULL ) {
		return false;
	}
	if ( iValue == -1 ) {
		*pResult = SIZE_MAX;
		return true;
	}
	if ( (iValue < 0) ||
		((unsigned long long)iValue >
		 (unsigned long long)SIZE_MAX) ) {
		xhttpMultipartNativeError(
			XERR_RANGE,
			3,
			"convert-limit",
			"multipart limits must be -1 or platform sizes"
		);
		return false;
	}
	*pResult = (size_t)iValue;
	return true;
}



/* 从六个语言预算构建 XRT 限制结构。 */
bool xhttpMultipartNativeLimits(
	long long iMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	xmultipartlimits* pLimits
)
{
	xmultipartlimits Limits;

	if ( pLimits == NULL ) {
		return false;
	}
	if ( !xhttpMultipartNativeLimit(iMaxParts, &Limits.MaxParts) ||
		!xhttpMultipartNativeLimit(iMaxHeaders, &Limits.MaxHeaders) ||
		!xhttpMultipartNativeLimit(
			iMaxDelimiterBytes, &Limits.MaxDelimiterBytes
		) ||
		!xhttpMultipartNativeLimit(
			iMaxHeaderBytes, &Limits.MaxHeaderBytes
		) ||
		!xhttpMultipartNativeLimit(iMaxPartBytes, &Limits.MaxPartBytes) ||
		!xhttpMultipartNativeLimit(iMaxBodyBytes, &Limits.MaxBodyBytes) ) {
		return false;
	}
	memcpy(pLimits, &Limits, sizeof(Limits));
	return true;
}



/* 复制一个借用 Part 描述符到语言拥有的轻量句柄。 */
xhttp_multipart_native_part* xhttpMultipartNativePartCreate(
	const xmultipartpart* pPart
)
{
	xhttp_multipart_native_part* pOwner;

	if ( pPart == NULL ) {
		return NULL;
	}
	pOwner = (xhttp_multipart_native_part*)xrtMalloc(sizeof(*pOwner));
	if ( pOwner != NULL ) {
		memcpy(&pOwner->Value, pPart, sizeof(pOwner->Value));
	}
	return pOwner;
}



/* 返回当前线程最近一次 multipart/XRT 错误文本。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_last_error(void)
{
	const xerror* pError = xrtGetError();
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : NULL;

	if ( (sMessage == NULL) || (sMessage[0] == 0) ) {
		sMessage = "xhttp.multipart operation failed";
	}
	return xhttpMultipartNativeBuffer((xbytesview){
		(cbytes)sMessage,
		strlen(sMessage)
	});
}



/* 校验文本并创建拥有型 boundary。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_boundary_create(
	const char* sText,
	size_t iSize,
	long long iMode
)
{
	xmultipartboundary* pBoundary;
	bool bResult;

	pBoundary = (xmultipartboundary*)xrtMalloc(sizeof(*pBoundary));
	if ( pBoundary == NULL ) {
		return NULL;
	}
	if ( iMode == 0 ) {
		bResult = xrtMultipartBoundaryParse(
			(xstrview){ sText, iSize }, pBoundary
		);
	} else if ( iMode == 1 ) {
		bResult = xrtMultipartBoundaryFromContentType(
			(xstrview){ sText, iSize }, pBoundary
		);
	} else {
		xhttpMultipartNativeError(
			XERR_ARGUMENT,
			4,
			"create-boundary",
			"multipart boundary mode is invalid"
		);
		bResult = false;
	}
	if ( !bResult ) {
		xrtFree(pBoundary);
		return NULL;
	}
	return pBoundary;
}



/* 使用系统安全随机源创建拥有型 boundary。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_boundary_random(void)
{
	xmultipartboundary* pBoundary =
		(xmultipartboundary*)xrtMalloc(sizeof(*pBoundary));

	if ( pBoundary == NULL ) {
		return NULL;
	}
	if ( !xrtMultipartBoundaryRandom(pBoundary) ) {
		xrtFree(pBoundary);
		return NULL;
	}
	return pBoundary;
}



/* point 构造直接接管完整 boundary。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_boundary_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放拥有型 boundary。 */
XLANG_XHTTP_MULTIPART_EXPORT long long xhttp_multipart_boundary_destroy(
	void* pHandle
)
{
	if ( pHandle != NULL ) {
		memset(pHandle, 0, sizeof(xmultipartboundary));
		xrtFree(pHandle);
	}
	return 0;
}



/* 返回可直接传入 XRT Multipart API 的 boundary 地址。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_boundary_native(
	void* pHandle
)
{
	return pHandle;
}



/* 复制 boundary 文本到独立 Buffer。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_boundary_text(
	void* pHandle
)
{
	xstrview Text;

	if ( pHandle == NULL ) {
		return NULL;
	}
	Text = xrtMultipartBoundaryView((xmultipartboundary*)pHandle);
	if ( (Text.Data == NULL) && (Text.Size != 0u) ) {
		return NULL;
	}
	return xhttpMultipartNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}



/* point 构造直接接管 Part 描述符。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放 Part 描述符，不触碰其借用的正文。 */
XLANG_XHTTP_MULTIPART_EXPORT long long xhttp_multipart_part_destroy(
	void* pHandle
)
{
	if ( pHandle != NULL ) {
		memset(pHandle, 0, sizeof(xhttp_multipart_native_part));
		xrtFree(pHandle);
	}
	return 0;
}



/* 返回可直接传入 XRT Multipart Part API 的描述符地址。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_native(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return pPart != NULL ? &pPart->Value : NULL;
}



/* 返回 Part 的正文首地址。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_data(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return pPart != NULL ? (void*)pPart->Value.Body.Data : NULL;
}



/* 返回 Part 的正文长度。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long xhttp_multipart_part_size(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return pPart != NULL ? pPart->Value.Body.Size : 0u;
}



/* 返回 Part 的 Header 数量。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long
xhttp_multipart_part_header_count(void* pHandle)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return pPart != NULL ? pPart->Value.HeaderCount : 0u;
}



/* 返回 Part 的协议字段标志。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long xhttp_multipart_part_flags(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return pPart != NULL ? pPart->Value.Flags : 0u;
}



/* 判断 Part 是否满足 RFC 7578 form-data 字段约束。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_part_form_data(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return (pPart != NULL) &&
		xrtMultipartFormPartValid(&pPart->Value);
}



/* 判断 Part 是否包含可解码字段名。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_part_has_name(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return (pPart != NULL) &&
		((pPart->Value.Disposition.Flags &
		  XCONTENT_DISPOSITION_NAME) != 0u);
}



/* 判断 Part 是否包含普通或扩展文件名。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_part_has_file_name(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;
	uint32 iFlags;

	if ( pPart == NULL ) {
		return false;
	}
	iFlags = pPart->Value.Disposition.Flags;
	return (iFlags & (XCONTENT_DISPOSITION_FILENAME |
		XCONTENT_DISPOSITION_FILENAME_EXT)) != 0u;
}



/* 写出 Part 名称或文件名。 */
static bool xhttpMultipartNativePartTextWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	const xhttp_multipart_native_part_text* pText =
		(const xhttp_multipart_native_part_text*)pContext;

	return pText->FileName ?
		xrtMultipartPartFileNameWrite(
			pText->Part, pOutput, iCapacity, pSize
		) :
		xrtMultipartPartNameWrite(
			pText->Part, pOutput, iCapacity, pSize
		);
}



/* 复制 Part 字段名。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_name(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;
	xhttp_multipart_native_part_text Text;

	if ( (pPart == NULL) ||
		!xhttp_multipart_part_has_name(pHandle) ) {
		return NULL;
	}
	Text.Part = &pPart->Value;
	Text.FileName = false;
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativePartTextWrite, &Text
	);
}



/* 复制 Part 文件名。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_file_name(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;
	xhttp_multipart_native_part_text Text;

	if ( (pPart == NULL) ||
		!xhttp_multipart_part_has_file_name(pHandle) ) {
		return NULL;
	}
	Text.Part = &pPart->Value;
	Text.FileName = true;
	return xhttpMultipartNativeBuild(
		xhttpMultipartNativePartTextWrite, &Text
	);
}



/* 复制 Part 原始 Header 块。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_raw_headers(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return pPart != NULL ?
		xhttpMultipartNativeBuffer((xbytesview){
			(cbytes)pPart->Value.Headers.Data,
			pPart->Value.Headers.Size
		}) : NULL;
}



/* 解析 Part Header 块为 xhttp.protocol.Headers 所有者。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_headers(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;
	size_t iError = 0u;

	return pPart != NULL ?
		xrtHttpHeadersParse(pPart->Value.Headers, NULL, &iError) : NULL;
}



/* 复制 Part 正文到独立 Buffer。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_body(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;

	return pPart != NULL ?
		xhttpMultipartNativeBuffer(pPart->Value.Body) : NULL;
}



/* 复制 Part 媒体类型主值，不重复解析完整 Header。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_content_type(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;
	str sText;
	size_t iSize = 0u;

	if ( (pPart == NULL) ||
		((pPart->Value.Flags & XMULTIPART_PART_CONTENT_TYPE) == 0u) ) {
		return NULL;
	}
	sText = xrtHttpMediaTypeBuild(&pPart->Value.ContentType, &iSize);
	return sText != NULL ? xhttpMultipartNativeBufferTake(
		(bytes)sText, iSize, iSize + 1u
	) : NULL;
}



/* 复制 Part 的 Content-Transfer-Encoding 值。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_part_transfer_encoding(
	void* pHandle
)
{
	xhttp_multipart_native_part* pPart =
		(xhttp_multipart_native_part*)pHandle;
	xstrview Text;

	if ( (pPart == NULL) ||
		((pPart->Value.Flags &
		  XMULTIPART_PART_TRANSFER_ENCODING) == 0u) ) {
		return NULL;
	}
	Text = pPart->Value.TransferEncoding;
	return xhttpMultipartNativeBuffer((xbytesview){
		(cbytes)Text.Data,
		Text.Size
	});
}
