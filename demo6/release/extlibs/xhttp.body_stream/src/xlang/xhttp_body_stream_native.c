#include "../../include/xhttp_body_stream_package.h"

#include <limits.h>
#include <string.h>



#if defined(_WIN32) || defined(_WIN64)
	#define XLANG_XHTTP_BODY_STREAM_EXPORT __declspec(dllexport)
#else
	#define XLANG_XHTTP_BODY_STREAM_EXPORT
#endif



/* 语言对象同时拥有正文工厂和一个生产端引用。 */
typedef struct xhttp_body_stream_native_owner {
	xhttpbody* Body;
	xhttpbodystream* Stream;
	xhttpbodystreamresult LastWrite;
} xhttp_body_stream_native_owner;



/* Info 对象保存一次一致的并发快照。 */
typedef struct xhttp_body_stream_native_info {
	xhttpbodystreaminfo Value;
} xhttp_body_stream_native_info;



/* 发布语言桥接自身的参数或状态错误。 */
static void xhttpBodyStreamNativeError(
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
	Desc.Domain = "xlang.xhttp.body_stream";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 创建有界正文流；预算必须能由当前平台 size_t 表达。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void* xhttp_body_stream_create(
	unsigned long long iMaxBytes,
	unsigned long long iMaxChunks
)
{
	xhttpbodystreamconfig Config;
	xhttp_body_stream_native_owner* pOwner;
	xhttpbodystream* pStream = NULL;
	xhttpbody* pBody;

	if ( (iMaxBytes == 0u) || (iMaxChunks == 0u) ||
		(iMaxBytes > (unsigned long long)SIZE_MAX) ||
		(iMaxChunks > (unsigned long long)SIZE_MAX) ) {
		xhttpBodyStreamNativeError(
			XERR_ARGUMENT,
			1,
			"create",
			"body stream budgets must be positive platform sizes"
		);
		return NULL;
	}
	Config.MaxBytes = (size_t)iMaxBytes;
	Config.MaxChunks = (size_t)iMaxChunks;
	pBody = xrtHttpBodyStreamCreate(&Config, &pStream);
	if ( pBody == NULL ) {
		return NULL;
	}
	pOwner = (xhttp_body_stream_native_owner*)xrtCalloc(
		1u, sizeof(*pOwner)
	);
	if ( pOwner == NULL ) {
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	pOwner->Body = pBody;
	pOwner->Stream = pStream;
	pOwner->LastWrite = XHTTP_BODY_STREAM_OK;
	return pOwner;
}



/* point 构造直接接管已经完整建立的语言所有者。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void* xhttp_body_stream_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放生产端和正文工厂；最后一个生产端会自动发布 EOF。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void xhttp_body_stream_destroy(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;

	if ( pOwner != NULL ) {
		xrtHttpBodyStreamDestroy(pOwner->Stream);
		xrtHttpBodyDestroy(pOwner->Body);
		memset(pOwner, 0, sizeof(*pOwner));
		xrtFree(pOwner);
	}
}



/* 创建共享生产端；两个语言对象分别拥有正文和生产端引用。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void* xhttp_body_stream_share(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pSource =
		(xhttp_body_stream_native_owner*)pHandle;
	xhttp_body_stream_native_owner* pOwner;

	if ( pSource == NULL ) {
		return NULL;
	}
	pOwner = (xhttp_body_stream_native_owner*)xrtCalloc(
		1u, sizeof(*pOwner)
	);
	if ( pOwner == NULL ) {
		return NULL;
	}
	pOwner->Body = xrtHttpBodyRef(pSource->Body);
	pOwner->Stream = xrtHttpBodyStreamRef(pSource->Stream);
	if ( (pOwner->Body == NULL) || (pOwner->Stream == NULL) ) {
		xrtHttpBodyStreamDestroy(pOwner->Stream);
		xrtHttpBodyDestroy(pOwner->Body);
		xrtFree(pOwner);
		return NULL;
	}
	pOwner->LastWrite = pSource->LastWrite;
	return pOwner;
}



/* 返回调用方拥有的正文引用。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void* xhttp_body_stream_body(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;

	return pOwner != NULL ? xrtHttpBodyRef(pOwner->Body) : NULL;
}



/* 返回可直接传给 XRT C API 的生产端句柄。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void* xhttp_body_stream_native(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;

	return pOwner != NULL ? pOwner->Stream : NULL;
}



/* 安全复制一段字节到有界队列。 */
XLANG_XHTTP_BODY_STREAM_EXPORT long long xhttp_body_stream_write(
	void* pHandle,
	void* pData,
	size_t iSize
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;

	if ( pOwner == NULL ) {
		xhttpBodyStreamNativeError(
			XERR_ARGUMENT, 2, "write", "body stream handle is null"
		);
		return XHTTP_BODY_STREAM_ERROR;
	}
	pOwner->LastWrite = xrtHttpBodyStreamWrite(
		pOwner->Stream,
		(xbytesview){ (cbytes)pData, iSize }
	);
	return pOwner->LastWrite;
}



/* 安全复制语言字符串的原始 UTF-8 字节。 */
XLANG_XHTTP_BODY_STREAM_EXPORT long long xhttp_body_stream_write_text(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	/* point 的 C ABI 为 void*；写入时复制文本，不修改借用来源。 */
	return xhttp_body_stream_write(pHandle, (void*)sText, iSize);
}



/* 成功时把 Buffer 的连续内存零拷贝转移给正文流。 */
XLANG_XHTTP_BODY_STREAM_EXPORT long long xhttp_body_stream_write_take(
	void* pHandle,
	void* pBufferHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;
	xbuffer* pBuffer = (xbuffer*)pBufferHandle;
	bytes pData;
	size_t iSize;
	size_t iCapacity;
	xerror* pError;

	if ( (pOwner == NULL) || (pBuffer == NULL) ) {
		xhttpBodyStreamNativeError(
			XERR_ARGUMENT,
			3,
			"write-take",
			"body stream and Buffer handles are required"
		);
		return XHTTP_BODY_STREAM_ERROR;
	}
	pData = xrtBufferTake(pBuffer, &iSize, &iCapacity);
	if ( iSize == 0u ) {
		xrtFree(pData);
		xhttpBodyStreamNativeError(
			XERR_ARGUMENT,
			4,
			"write-take",
			"body stream chunks must not be empty"
		);
		pOwner->LastWrite = XHTTP_BODY_STREAM_ERROR;
		return pOwner->LastWrite;
	}
	pOwner->LastWrite = xrtHttpBodyStreamWriteTake(
		pOwner->Stream, pData, iSize
	);
	if ( pOwner->LastWrite == XHTTP_BODY_STREAM_OK ) {
		return pOwner->LastWrite;
	}

	pError = xrtTakeError();
	if ( !xrtBufferSetTake(
		pBuffer, &pData, iSize, iCapacity
	) ) {
		xrtFree(pData);
		xrtErrorFree(pError);
		pOwner->LastWrite = XHTTP_BODY_STREAM_ERROR;
		return pOwner->LastWrite;
	}
	xrtSetErrorTake(pError);
	return pOwner->LastWrite;
}



/* 返回最近一次写入结果，用于保持 AGAIN 与 CLOSED 的显式语义。 */
XLANG_XHTTP_BODY_STREAM_EXPORT long long xhttp_body_stream_last_write(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;

	return pOwner != NULL ? pOwner->LastWrite :
		XHTTP_BODY_STREAM_ERROR;
}



/* 在一次 AGAIN 后取得共享可写代际 Future。 */
XLANG_XHTTP_BODY_STREAM_EXPORT xfuture* xhttp_body_stream_wait_writable(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;

	if ( (pOwner == NULL) ||
		(pOwner->LastWrite != XHTTP_BODY_STREAM_AGAIN) ) {
		xhttpBodyStreamNativeError(
			XERR_STATE,
			5,
			"wait-writable",
			"writable wait requires a preceding AGAIN result"
		);
		return NULL;
	}
	return xrtHttpBodyStreamWaitWritable(pOwner->Stream);
}



/* 幂等关闭输入，队列会在 EOF 前完整排空。 */
XLANG_XHTTP_BODY_STREAM_EXPORT bool xhttp_body_stream_close(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;

	return pOwner != NULL &&
		xrtHttpBodyStreamClose(pOwner->Stream);
}



/* 用稳定 Cause 永久失败正文流。 */
XLANG_XHTTP_BODY_STREAM_EXPORT bool xhttp_body_stream_fail(
	void* pHandle,
	const char* sMessage,
	int32 iCode
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;
	xerror* pCause;
	bool bResult;

	if ( pOwner == NULL ) {
		return false;
	}
	pCause = xrtErrorCreate(
		XERR_IO,
		"xlang.xhttp.body_stream.producer",
		iCode,
		sMessage != NULL ? sMessage : "body stream producer failed"
	);
	if ( pCause == NULL ) {
		return false;
	}
	bResult = xrtHttpBodyStreamFail(pOwner->Stream, pCause);
	xrtErrorFree(pCause);
	return bResult;
}



/* 创建一次一致的流状态快照。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void* xhttp_body_stream_info(
	void* pHandle
)
{
	xhttp_body_stream_native_owner* pOwner =
		(xhttp_body_stream_native_owner*)pHandle;
	xhttp_body_stream_native_info* pInfo;

	if ( pOwner == NULL ) {
		return NULL;
	}
	pInfo = (xhttp_body_stream_native_info*)xrtCalloc(
		1u, sizeof(*pInfo)
	);
	if ( pInfo == NULL ) {
		return NULL;
	}
	if ( !xrtHttpBodyStreamInfo(pOwner->Stream, &pInfo->Value) ) {
		xrtFree(pInfo);
		return NULL;
	}
	return pInfo;
}



/* point 构造直接接管 Info 快照。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void* xhttp_body_stream_info_wrap(
	void* pHandle
)
{
	return pHandle;
}



/* 释放 Info 快照。 */
XLANG_XHTTP_BODY_STREAM_EXPORT void xhttp_body_stream_info_destroy(
	void* pHandle
)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	if ( pInfo != NULL ) {
		memset(pInfo, 0, sizeof(*pInfo));
		xrtFree(pInfo);
	}
}



/* 返回快照中的待处理字节。 */
XLANG_XHTTP_BODY_STREAM_EXPORT unsigned long long
xhttp_body_stream_info_pending_bytes(void* pHandle)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL ? pInfo->Value.PendingBytes : 0u;
}



/* 返回快照中的待处理 Chunk 数。 */
XLANG_XHTTP_BODY_STREAM_EXPORT unsigned long long
xhttp_body_stream_info_pending_chunks(void* pHandle)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL ? pInfo->Value.PendingChunks : 0u;
}



/* 返回累计成功写入字节。 */
XLANG_XHTTP_BODY_STREAM_EXPORT unsigned long long
xhttp_body_stream_info_written_bytes(void* pHandle)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL ? pInfo->Value.WrittenBytes : 0u;
}



/* 返回累计交付字节。 */
XLANG_XHTTP_BODY_STREAM_EXPORT unsigned long long
xhttp_body_stream_info_read_bytes(void* pHandle)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL ? pInfo->Value.ReadBytes : 0u;
}



/* 返回正文是否已经打开。 */
XLANG_XHTTP_BODY_STREAM_EXPORT bool xhttp_body_stream_info_opened(
	void* pHandle
)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL && pInfo->Value.Opened;
}



/* 返回生产输入是否已经关闭。 */
XLANG_XHTTP_BODY_STREAM_EXPORT bool xhttp_body_stream_info_input_closed(
	void* pHandle
)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL && pInfo->Value.InputClosed;
}



/* 返回消费端是否已经关闭。 */
XLANG_XHTTP_BODY_STREAM_EXPORT bool xhttp_body_stream_info_consumer_closed(
	void* pHandle
)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL && pInfo->Value.ConsumerClosed;
}



/* 返回正文流是否已经永久失败。 */
XLANG_XHTTP_BODY_STREAM_EXPORT bool xhttp_body_stream_info_failed(
	void* pHandle
)
{
	xhttp_body_stream_native_info* pInfo =
		(xhttp_body_stream_native_info*)pHandle;

	return pInfo != NULL && pInfo->Value.Failed;
}
