#include "xhttp_multipart_native_internal.h"



/* 把解析器切换到稳定错误终态。 */
static void xhttpMultipartNativeParserFail(
	xhttp_multipart_native_parser* pParser,
	xmultiparterror Code,
	size_t iOffset,
	cstr sMessage
)
{
	pParser->Error.Code = Code;
	pParser->Error.Offset = iOffset;
	pParser->Failed = true;
	xhttpMultipartNativeError(
		XERR_RANGE,
		(int32)Code,
		"parse-body",
		sMessage
	);
}



/* 对未预验证迭代路径执行可由 Part 描述符判定的预算。 */
static bool xhttpMultipartNativeParserPartLimit(
	xhttp_multipart_native_parser* pParser,
	const xmultipartpart* pPart
)
{
	if ( pParser->Parts >= pParser->Limits.MaxParts ) {
		xhttpMultipartNativeParserFail(
			pParser,
			XMULTIPART_ERROR_PARTS_LIMIT,
			pParser->Offset,
			"multipart part count limit exceeded"
		);
		return false;
	}
	if ( pPart->HeaderCount > pParser->Limits.MaxHeaders ) {
		xhttpMultipartNativeParserFail(
			pParser,
			XMULTIPART_ERROR_HEADERS_LIMIT,
			pParser->Offset,
			"multipart header count limit exceeded"
		);
		return false;
	}
	if ( pPart->Headers.Size > pParser->Limits.MaxHeaderBytes ) {
		xhttpMultipartNativeParserFail(
			pParser,
			XMULTIPART_ERROR_HEADER_BYTES_LIMIT,
			pParser->Offset,
			"multipart header byte limit exceeded"
		);
		return false;
	}
	if ( pPart->Body.Size > pParser->Limits.MaxPartBytes ) {
		xhttpMultipartNativeParserFail(
			pParser,
			XMULTIPART_ERROR_PART_BYTES_LIMIT,
			pParser->Offset,
			"multipart part byte limit exceeded"
		);
		return false;
	}
	return true;
}



/* 创建借用正文的整包解析器，可选先执行完整安全验证。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_parser_create(
	const void* pData,
	size_t iSize,
	const void* pBoundaryHandle,
	long long iMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMaxPartBytes,
	long long iMaxBodyBytes,
	bool bValidate
)
{
	const xmultipartboundary* pBoundary =
		(const xmultipartboundary*)pBoundaryHandle;
	xhttp_multipart_native_parser* pParser;
	xmultipartlimits Limits;
	xmultiparterrorinfo Error;
	xbytesview Body = { (cbytes)pData, iSize };

	if ( (pBoundary == NULL) ||
		!xrtMemRangeValid(pData, iSize) ||
		!xhttpMultipartNativeLimits(
			iMaxParts,
			iMaxHeaders,
			iMaxDelimiterBytes,
			iMaxHeaderBytes,
			iMaxPartBytes,
			iMaxBodyBytes,
			&Limits
		) ) {
		if ( pBoundary == NULL ) {
			xhttpMultipartNativeError(
				XERR_ARGUMENT,
				10,
				"create-parser",
				"multipart boundary is required"
			);
		}
		return NULL;
	}
	if ( iSize > Limits.MaxBodyBytes ) {
		xhttpMultipartNativeError(
			XERR_RANGE,
			XMULTIPART_ERROR_BODY_BYTES_LIMIT,
			"create-parser",
			"multipart body byte limit exceeded"
		);
		return NULL;
	}
	if ( bValidate &&
		!xrtMultipartValidate(Body, pBoundary, &Limits, &Error) ) {
		return NULL;
	}
	pParser = (xhttp_multipart_native_parser*)xrtCalloc(
		1u, sizeof(*pParser)
	);
	if ( pParser == NULL ) {
		return NULL;
	}
	pParser->Body = Body;
	memcpy(&pParser->Boundary, pBoundary, sizeof(pParser->Boundary));
	memcpy(&pParser->Limits, &Limits, sizeof(pParser->Limits));
	pParser->Error.Code = XMULTIPART_ERROR_NONE;
	return pParser;
}



/* point 构造直接接管完整解析器。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_parser_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放解析状态，不触碰语言层保活的正文。 */
XLANG_XHTTP_MULTIPART_EXPORT long long xhttp_multipart_parser_destroy(
	void* pHandle
)
{
	if ( pHandle != NULL ) {
		memset(pHandle, 0, sizeof(xhttp_multipart_native_parser));
		xrtFree(pHandle);
	}
	return 0;
}



/* 返回桥接解析器句柄，C 扩展可由此读取当前状态。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_parser_native(
	void* pHandle
)
{
	return pHandle;
}



/* 读取下一项；结束返回空句柄，失败由 failed 属性区分。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_parser_next(
	void* pHandle
)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;
	xmultipartpart Part;
	xhttpnext Status;
	xhttp_multipart_native_part* pResult;

	if ( (pParser == NULL) || pParser->Done || pParser->Failed ) {
		return NULL;
	}
	Status = xrtMultipartNext(
		pParser->Body,
		&pParser->Boundary,
		&pParser->Offset,
		&Part,
		&pParser->Error
	);
	if ( Status == XHTTP_NEXT_END ) {
		pParser->Done = true;
		return NULL;
	}
	if ( Status != XHTTP_NEXT_ITEM ) {
		pParser->Failed = true;
		return NULL;
	}
	if ( !xhttpMultipartNativeParserPartLimit(pParser, &Part) ) {
		return NULL;
	}
	pResult = xhttpMultipartNativePartCreate(&Part);
	if ( pResult == NULL ) {
		pParser->Failed = true;
		return NULL;
	}
	pParser->Parts++;
	return pResult;
}



/* 保留正文、边界和限制并重置迭代游标。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_parser_reset(
	void* pHandle
)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;

	if ( pParser == NULL ) {
		return false;
	}
	pParser->Offset = 0u;
	pParser->Parts = 0u;
	pParser->Error.Code = XMULTIPART_ERROR_NONE;
	pParser->Error.Offset = 0u;
	pParser->Done = false;
	pParser->Failed = false;
	return true;
}



/* 返回解析器是否已经验证关闭边界。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_parser_done(
	void* pHandle
)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;

	return (pParser != NULL) && pParser->Done;
}



/* 返回解析器是否处于稳定错误终态。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_parser_failed(
	void* pHandle
)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;

	return (pParser == NULL) || pParser->Failed;
}



/* 返回已经发布的 Part 数量。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long xhttp_multipart_parser_parts(
	void* pHandle
)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;

	return pParser != NULL ? pParser->Parts : 0u;
}



/* 返回下一次解析起点。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long xhttp_multipart_parser_offset(
	void* pHandle
)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;

	return pParser != NULL ? pParser->Offset : 0u;
}



/* 返回稳定 Multipart 错误码。 */
XLANG_XHTTP_MULTIPART_EXPORT long long xhttp_multipart_parser_error_code(
	void* pHandle
)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;

	return pParser != NULL ? pParser->Error.Code :
		XMULTIPART_ERROR_ARGUMENT;
}



/* 返回相对正文起点的错误字节位置。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long
xhttp_multipart_parser_error_offset(void* pHandle)
{
	xhttp_multipart_native_parser* pParser =
		(xhttp_multipart_native_parser*)pHandle;

	return pParser != NULL ? pParser->Error.Offset : 0u;
}
