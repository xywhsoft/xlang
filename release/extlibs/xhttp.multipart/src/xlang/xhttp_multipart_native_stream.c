#include "xhttp_multipart_native_internal.h"



/* 创建无固定输入缓冲的增量 Reader。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_reader_create(
	void* pBoundaryHandle,
	long long iMaxParts,
	long long iMaxHeaders,
	long long iMaxDelimiterBytes,
	long long iMaxHeaderBytes,
	long long iMaxPartBytes,
	long long iMaxBodyBytes
)
{
	const xmultipartboundary* pBoundary =
		(const xmultipartboundary*)pBoundaryHandle;
	xhttp_multipart_native_reader* pReader;
	xmultipartlimits Limits;

	if ( (pBoundary == NULL) ||
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
				30,
				"create-reader",
				"multipart boundary is required"
			);
		}
		return NULL;
	}
	pReader = (xhttp_multipart_native_reader*)xrtCalloc(
		1u, sizeof(*pReader)
	);
	if ( pReader == NULL ) {
		return NULL;
	}
	if ( !xrtMultipartReaderInit(
		&pReader->Value, pBoundary, &Limits
	) ) {
		xrtFree(pReader);
		return NULL;
	}
	return pReader;
}



/* point 构造直接接管完整 Reader。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_reader_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放 Reader 状态，不触碰任何输入 Buffer。 */
XLANG_XHTTP_MULTIPART_EXPORT void xhttp_multipart_reader_destroy(
	void* pHandle
)
{
	if ( pHandle != NULL ) {
		memset(pHandle, 0, sizeof(xhttp_multipart_native_reader));
		xrtFree(pHandle);
	}
}



/* 返回可直接传入 XRT 流 Reader API 的状态地址。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_reader_native(
	void* pHandle
)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;

	return pReader != NULL ? &pReader->Value : NULL;
}



/* 保留 boundary 和限制并重置流解析进度。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_reader_reset(
	void* pHandle
)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;

	if ( pReader == NULL ) {
		return false;
	}
	xrtMultipartReaderReset(&pReader->Value);
	return true;
}



/* 按调用方显式切片推进 Reader 并发布一个借用事件。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_reader_read(
	void* pHandle,
	void* pData,
	size_t iTotal,
	size_t iOffset,
	size_t iSize,
	bool bEnd
)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;
	xhttp_multipart_native_event* pEvent;
	const uint8* pBytes = (const uint8*)pData;
	xbytesview Input;

	if ( (pReader == NULL) ||
		!xrtMemRangeValid(pData, iTotal) ||
		(iOffset > iTotal) || (iSize > (iTotal - iOffset)) ) {
		xhttpMultipartNativeError(
			XERR_RANGE,
			31,
			"read-stream",
			"multipart input slice is out of range"
		);
		return NULL;
	}
	Input.Data = iSize != 0u ? pBytes + iOffset : NULL;
	Input.Size = iSize;
	pEvent = (xhttp_multipart_native_event*)xrtCalloc(
		1u, sizeof(*pEvent)
	);
	if ( pEvent == NULL ) {
		return NULL;
	}
	pEvent->Status = xrtMultipartReaderRead(
		&pReader->Value,
		Input,
		bEnd,
		&pEvent->Consumed,
		&pEvent->Part,
		&pEvent->Data,
		&pEvent->Error
	);
	return pEvent;
}



/* 返回 Reader 是否已经消费关闭 boundary 和 epilogue。 */
XLANG_XHTTP_MULTIPART_EXPORT bool xhttp_multipart_reader_done(
	void* pHandle
)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;

	return (pReader != NULL) &&
		xrtMultipartReaderDone(&pReader->Value);
}



/* 返回 Reader 已发布的 Part 数量。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long xhttp_multipart_reader_parts(
	void* pHandle
)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;

	return pReader != NULL ? pReader->Value.Parts : 0u;
}



/* 返回当前 Part 已消费正文长度。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long
xhttp_multipart_reader_part_bytes(void* pHandle)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;

	return pReader != NULL ? pReader->Value.PartBytes : 0u;
}



/* 返回累计发布的数据正文长度。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long
xhttp_multipart_reader_body_bytes(void* pHandle)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;

	return pReader != NULL ? pReader->Value.BodyBytes : 0u;
}



/* 返回累计消费的线缆字节长度。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long xhttp_multipart_reader_wire_bytes(
	void* pHandle
)
{
	xhttp_multipart_native_reader* pReader =
		(xhttp_multipart_native_reader*)pHandle;

	return pReader != NULL ? pReader->Value.WireBytes : 0u;
}



/* point 构造直接接管流事件。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_event_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放流事件，不触碰其借用的输入。 */
XLANG_XHTTP_MULTIPART_EXPORT void xhttp_multipart_event_destroy(
	void* pHandle
)
{
	if ( pHandle != NULL ) {
		memset(pHandle, 0, sizeof(xhttp_multipart_native_event));
		xrtFree(pHandle);
	}
}



/* 返回事件状态。 */
XLANG_XHTTP_MULTIPART_EXPORT long long xhttp_multipart_event_status(
	void* pHandle
)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	return pEvent != NULL ? pEvent->Status : XMULTIPART_READ_ERROR;
}



/* 返回本次可以从输入头部移除的字节数。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long
xhttp_multipart_event_consumed(void* pHandle)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	return pEvent != NULL ? pEvent->Consumed : 0u;
}



/* 复制 PART 事件描述符到独立轻量句柄。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_event_part(
	void* pHandle
)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	if ( (pEvent == NULL) ||
		(pEvent->Status != XMULTIPART_READ_PART) ) {
		return NULL;
	}
	return xhttpMultipartNativePartCreate(&pEvent->Part);
}



/* 返回 DATA 事件借用字节首地址。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_event_data(
	void* pHandle
)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	return ((pEvent != NULL) &&
		(pEvent->Status == XMULTIPART_READ_DATA)) ?
		(void*)pEvent->Data.Data : NULL;
}



/* 返回 DATA 事件借用字节长度。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long xhttp_multipart_event_size(
	void* pHandle
)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	return ((pEvent != NULL) &&
		(pEvent->Status == XMULTIPART_READ_DATA)) ?
		pEvent->Data.Size : 0u;
}



/* 复制 DATA 事件字节到独立 Buffer。 */
XLANG_XHTTP_MULTIPART_EXPORT void* xhttp_multipart_event_buffer(
	void* pHandle
)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	if ( (pEvent == NULL) ||
		(pEvent->Status != XMULTIPART_READ_DATA) ) {
		return NULL;
	}
	return xhttpMultipartNativeBuffer(pEvent->Data);
}



/* 返回流事件稳定错误码。 */
XLANG_XHTTP_MULTIPART_EXPORT long long xhttp_multipart_event_error_code(
	void* pHandle
)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	return pEvent != NULL ? pEvent->Error.Code :
		XMULTIPART_ERROR_ARGUMENT;
}



/* 返回流事件相对完整正文起点的错误位置。 */
XLANG_XHTTP_MULTIPART_EXPORT unsigned long long
xhttp_multipart_event_error_offset(void* pHandle)
{
	xhttp_multipart_native_event* pEvent =
		(xhttp_multipart_native_event*)pHandle;

	return pEvent != NULL ? pEvent->Error.Offset : 0u;
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_MULTIPART_EXPORT void xhttp_multipart_reader_destroy_abi(void* handle)
{
    (void)xhttp_multipart_reader_destroy(handle);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_MULTIPART_EXPORT void xhttp_multipart_event_destroy_abi(void* handle)
{
    (void)xhttp_multipart_event_destroy(handle);
}
