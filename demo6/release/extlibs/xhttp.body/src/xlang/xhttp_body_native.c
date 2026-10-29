#include "xhttp_body_native_internal.h"

/* 语言 point 的 ABI 是 void*；本文件的字节输入只读，核心继续接收 const 视图。 */



/* 创建空正文。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_create_empty(void)
{
	return xrtHttpBodyEmpty();
}



/* 复制任意二进制数据并创建正文。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_create_copy(
	void* pData,
	size_t iSize
)
{
	return xrtHttpBodyCopy((xbytesview){ (cbytes)pData, iSize });
}



/* 复制语言字符串并创建正文。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_create_text(
	const char* sText,
	size_t iSize
)
{
	return xhttp_body_create_copy((void*)sText, iSize);
}



/* point 构造直接接管正文引用。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_wrap(void* pHandle)
{
	return pHandle;
}



/* 增加不可变正文引用。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_ref(void* pHandle)
{
	return xrtHttpBodyRef((xhttpbody*)pHandle);
}



/* 释放正文引用。 */
XLANG_XHTTP_BODY_EXPORT void xhttp_body_destroy(void* pHandle)
{
	xrtHttpBodyDestroy((xhttpbody*)pHandle);
}



/* 返回可传给 XRT C API 的原生正文句柄。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_native(void* pHandle)
{
	return pHandle;
}



/* 返回正文长度；未知或超过 xlang int 范围时返回 -1。 */
XLANG_XHTTP_BODY_EXPORT long long xhttp_body_length(void* pHandle)
{
	uint64 iLength;

	if ( pHandle == NULL ) {
		return -1;
	}
	iLength = xrtHttpBodyLength((xhttpbody*)pHandle);
	if ( (iLength == XHTTP_BODY_UNKNOWN) ||
		(iLength > (uint64)LLONG_MAX) ) {
		return -1;
	}
	return (long long)iLength;
}



/* 返回正文公开能力标志。 */
XLANG_XHTTP_BODY_EXPORT unsigned long long xhttp_body_flags(
	void* pHandle
)
{
	return pHandle != NULL ?
		xrtHttpBodyFlags((xhttpbody*)pHandle) : 0u;
}



/* 判断正文是否可重复打开。 */
XLANG_XHTTP_BODY_EXPORT bool xhttp_body_replayable(void* pHandle)
{
	return xrtHttpBodyReplayable((xhttpbody*)pHandle);
}



/* 复制固定正文的连续视图；流式正文返回空句柄且不报错。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_view(void* pHandle)
{
	xbytesview Data;

	if ( pHandle == NULL ) {
		return NULL;
	}
	if ( !xrtHttpBodyView((xhttpbody*)pHandle, &Data) ) {
		return NULL;
	}
	return xhttpBodyNativeBuffer(Data);
}



/* 打开独立 Reader 并保存状态。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_open(void* pHandle)
{
	xhttp_body_native_reader* pOwner;
	xhttpbodyreader* pReader;

	if ( pHandle == NULL ) {
		return NULL;
	}
	pReader = xrtHttpBodyOpen((xhttpbody*)pHandle);
	if ( pReader == NULL ) {
		return NULL;
	}
	pOwner = (xhttp_body_native_reader*)xrtCalloc(
		1u, sizeof(*pOwner)
	);
	if ( pOwner == NULL ) {
		xrtHttpBodyReaderDestroy(pReader);
		return NULL;
	}
	pOwner->Reader = pReader;
	pOwner->Status = XHTTP_BODY_DATA;
	return pOwner;
}



/* 释放 Reader。 */
XLANG_XHTTP_BODY_EXPORT void xhttp_body_reader_destroy(
	void* pHandle
)
{
	xhttp_body_native_reader* pOwner =
		(xhttp_body_native_reader*)pHandle;

	if ( pOwner != NULL ) {
		xrtHttpBodyReaderDestroy(pOwner->Reader);
		memset(pOwner, 0, sizeof(*pOwner));
		xrtFree(pOwner);
	}
}



/* 返回可传给 XRT C API 的原生 Reader 句柄。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_reader_native(
	void* pHandle
)
{
	xhttp_body_native_reader* pOwner =
		(xhttp_body_native_reader*)pHandle;

	return pOwner != NULL ? pOwner->Reader : NULL;
}



/* 读取一个零复制 Chunk 租约。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_reader_next(
	void* pHandle,
	size_t iMaxBytes
)
{
	xhttp_body_native_reader* pOwner =
		(xhttp_body_native_reader*)pHandle;
	xhttp_body_native_chunk* pChunk;

	if ( (pOwner == NULL) || (iMaxBytes == 0u) ) {
		xhttpBodyNativeError(
			XERR_ARGUMENT,
			"read-body-chunk",
			"BodyReader handle and maximum size must be valid"
		);
		return NULL;
	}
	pChunk = (xhttp_body_native_chunk*)xrtCalloc(
		1u, sizeof(*pChunk)
	);
	if ( pChunk == NULL ) {
		pOwner->Status = XHTTP_BODY_ERROR;
		return NULL;
	}
	pOwner->Status = xrtHttpBodyNext(
		pOwner->Reader, iMaxBytes, &pChunk->Chunk
	);
	if ( pOwner->Status != XHTTP_BODY_DATA ) {
		xrtFree(pChunk);
		return NULL;
	}
	return pChunk;
}



/* 返回 Reader 最后一次读取状态。 */
XLANG_XHTTP_BODY_EXPORT long long xhttp_body_reader_status(
	void* pHandle
)
{
	xhttp_body_native_reader* pOwner =
		(xhttp_body_native_reader*)pHandle;

	return pOwner != NULL ? (long long)pOwner->Status :
		(long long)XHTTP_BODY_ERROR;
}



/* 返回 Reader 已发布的字节数。 */
XLANG_XHTTP_BODY_EXPORT unsigned long long xhttp_body_reader_bytes(
	void* pHandle
)
{
	xhttp_body_native_reader* pOwner =
		(xhttp_body_native_reader*)pHandle;

	return pOwner != NULL ?
		xrtHttpBodyReaderBytes(pOwner->Reader) : 0u;
}



/* point 构造直接接管 Chunk 租约包装。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_chunk_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放 Chunk 数据租约。 */
XLANG_XHTTP_BODY_EXPORT void xhttp_body_chunk_destroy(
	void* pHandle
)
{
	xhttp_body_native_chunk* pChunk =
		(xhttp_body_native_chunk*)pHandle;

	if ( pChunk != NULL ) {
		xrtHttpBodyChunkRelease(&pChunk->Chunk);
		memset(pChunk, 0, sizeof(*pChunk));
		xrtFree(pChunk);
	}
}



/* 返回原生 xhttpbodychunk 句柄。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_chunk_native(
	void* pHandle
)
{
	xhttp_body_native_chunk* pChunk =
		(xhttp_body_native_chunk*)pHandle;

	return pChunk != NULL ? &pChunk->Chunk : NULL;
}



/* 返回 Chunk 借用数据地址。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_chunk_data(void* pHandle)
{
	xhttp_body_native_chunk* pChunk =
		(xhttp_body_native_chunk*)pHandle;

	return pChunk != NULL ? (ptr)pChunk->Chunk.Data : NULL;
}



/* 返回 Chunk 字节数。 */
XLANG_XHTTP_BODY_EXPORT unsigned long long xhttp_body_chunk_size(
	void* pHandle
)
{
	xhttp_body_native_chunk* pChunk =
		(xhttp_body_native_chunk*)pHandle;

	return pChunk != NULL ? pChunk->Chunk.Size : 0u;
}



/* 复制 Chunk 数据到独立 Buffer。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_chunk_copy(void* pHandle)
{
	xhttp_body_native_chunk* pChunk =
		(xhttp_body_native_chunk*)pHandle;

	return pChunk != NULL ?
		xhttpBodyNativeBuffer((xbytesview){
			pChunk->Chunk.Data,
			pChunk->Chunk.Size
		}) : NULL;
}



/* 同步读取完整正文，并执行显式总字节上限。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_read_all(
	void* pHandle,
	unsigned long long iMaxBytes
)
{
	xhttpbodyreader* pReader;
	xhttpbodychunk Chunk;
	xhttpbodystatus Status;
	xbuffer* pBuffer;
	uint64 iLength;

	if ( pHandle == NULL ) {
		return NULL;
	}
	iLength = xrtHttpBodyLength((xhttpbody*)pHandle);
	if ( (iMaxBytes != 0u) && (iLength != XHTTP_BODY_UNKNOWN) &&
		(iLength > iMaxBytes) ) {
		xhttpBodyNativeError(
			XERR_RANGE,
			"read-all-body",
			"HTTP body exceeds the requested byte limit"
		);
		return NULL;
	}
	pReader = xrtHttpBodyOpen((xhttpbody*)pHandle);
	if ( pReader == NULL ) {
		return NULL;
	}
	pBuffer = xrtBufferCreate();
	if ( pBuffer == NULL ) {
		xrtHttpBodyReaderDestroy(pReader);
		return NULL;
	}
	if ( (iLength != XHTTP_BODY_UNKNOWN) &&
		(iLength <= (uint64)SIZE_MAX) &&
		!xrtBufferReserve(pBuffer, (size_t)iLength) ) {
		xrtBufferDestroy(pBuffer);
		xrtHttpBodyReaderDestroy(pReader);
		return NULL;
	}
	for ( ;; ) {
		Status = xrtHttpBodyNext(
			pReader, 64u * 1024u, &Chunk
		);
		if ( Status == XHTTP_BODY_DATA ) {
			xbytesview Current = xrtBufferView(pBuffer);

			if ( (iMaxBytes != 0u) &&
				((uint64)Current.Size > iMaxBytes ||
				 (uint64)Chunk.Size >
				 (iMaxBytes - (uint64)Current.Size)) ) {
				xrtHttpBodyChunkRelease(&Chunk);
				xrtBufferDestroy(pBuffer);
				xrtHttpBodyReaderDestroy(pReader);
				xhttpBodyNativeError(
					XERR_RANGE,
					"read-all-body",
					"HTTP body exceeds the requested byte limit"
				);
				return NULL;
			}
			if ( !xrtBufferAppend(pBuffer, (xbytesview){
				Chunk.Data, Chunk.Size
			}) ) {
				xrtHttpBodyChunkRelease(&Chunk);
				xrtBufferDestroy(pBuffer);
				xrtHttpBodyReaderDestroy(pReader);
				return NULL;
			}
			xrtHttpBodyChunkRelease(&Chunk);
			continue;
		}
		if ( Status == XHTTP_BODY_EOF ) {
			xrtHttpBodyReaderDestroy(pReader);
			return pBuffer;
		}
		xrtBufferDestroy(pBuffer);
		xrtHttpBodyReaderDestroy(pReader);
		if ( Status == XHTTP_BODY_AGAIN ) {
			xhttpBodyNativeError(
				XERR_AGAIN,
				"read-all-body",
				"HTTP body requires asynchronous readiness"
			);
		}
		return NULL;
	}
}



/* 按顺序组合两个正文，并保留各自引用。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_concat(
	void* pLeft,
	void* pRight
)
{
	xhttpbodypiece Pieces[2];

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		return NULL;
	}
	Pieces[0] = xrtHttpBodyPieceBody((xhttpbody*)pLeft);
	Pieces[1] = xrtHttpBodyPieceBody((xhttpbody*)pRight);
	return xrtHttpBodyCompose(Pieces, 2u);
}



/* 在正文前后追加一次立即复制的字节片段。 */
XLANG_XHTTP_BODY_EXPORT void* xhttp_body_with_bytes(
	void* pHandle,
	void* pData,
	size_t iSize,
	bool bPrepend
)
{
	xhttpbodypiece Pieces[2];
	xhttpbodypiece Body;
	xhttpbodypiece Bytes;

	if ( pHandle == NULL ) {
		return NULL;
	}
	Body = xrtHttpBodyPieceBody((xhttpbody*)pHandle);
	Bytes = xrtHttpBodyPieceBytes((xbytesview){
		(cbytes)pData, iSize
	});
	Pieces[0] = bPrepend ? Bytes : Body;
	Pieces[1] = bPrepend ? Body : Bytes;
	return xrtHttpBodyCompose(Pieces, 2u);
}

XLANG_XHTTP_BODY_EXPORT void* xhttp_body_with_text(void* handle,
    xstrview text, bool prepend)
{
    return xhttp_body_with_bytes(handle, (void*)text.Data, text.Size, prepend);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_BODY_EXPORT void xhttp_body_destroy_abi(void* handle)
{
    (void)xhttp_body_destroy(handle);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_BODY_EXPORT void xhttp_body_reader_destroy_abi(void* handle)
{
    (void)xhttp_body_reader_destroy(handle);
}

/* Void destructor ABI used by current XLang native classes. */
XLANG_XHTTP_BODY_EXPORT void xhttp_body_chunk_destroy_abi(void* handle)
{
    (void)xhttp_body_chunk_destroy(handle);
}
