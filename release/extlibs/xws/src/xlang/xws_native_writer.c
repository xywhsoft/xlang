#include "xws_native_internal.h"



typedef enum xws_native_writer_command_kind {
	XWS_NATIVE_WRITER_BEGIN = 1,
	XWS_NATIVE_WRITER_WRITE,
	XWS_NATIVE_WRITER_FINISH,
	XWS_NATIVE_WRITER_WRITE_TAKE,
	XWS_NATIVE_WRITER_FINISH_TAKE,
	XWS_NATIVE_WRITER_DESTROY
} xws_native_writer_command_kind;



typedef struct xws_native_writer_command {
	xws_native_writer_command_kind Kind;
	xwsconn* Connection;
	xwswriter* Writer;
	xwsopcode Opcode;
	xbytesview Data;
	xbuffer* Buffer;
	bool Compressed;
	xnetresult Result;
} xws_native_writer_command;



/* 最后一份包装引用释放锁和对象存储。 */
static void xwsNativeWriterRelease(xws_native_writer* pOwner)
{
	if ( xrtAtomic32FetchSub(
		&pOwner->References,
		1u,
		XMEMORY_ACQ_REL
	) != 1u ) {
		return;
	}
	xrtSpinDestroy(&pOwner->Lock);
	memset(pOwner, 0, sizeof(*pOwner));
	xrtFree(pOwner);
}



/* 在连接 Worker 中执行一次 Writer 命令。 */
static void xwsNativeWriterRun(ptr pData)
{
	xws_native_writer_command* pCommand = (xws_native_writer_command*)pData;

	switch ( pCommand->Kind ) {
		case XWS_NATIVE_WRITER_BEGIN:
			pCommand->Writer = pCommand->Compressed ?
				xrtWsConnBeginCompressed(pCommand->Connection, pCommand->Opcode) :
				xrtWsConnBegin(pCommand->Connection, pCommand->Opcode);
			break;

		case XWS_NATIVE_WRITER_WRITE:
			pCommand->Result = xrtWsWriterWrite(pCommand->Writer, pCommand->Data);
			break;

		case XWS_NATIVE_WRITER_FINISH:
			pCommand->Result = xrtWsWriterFinish(pCommand->Writer, pCommand->Data);
			break;

		case XWS_NATIVE_WRITER_WRITE_TAKE:
			pCommand->Result = xrtWsWriterWriteTake(
				pCommand->Writer,
				pCommand->Buffer->Data,
				pCommand->Buffer->Size
			);
			break;

		case XWS_NATIVE_WRITER_FINISH_TAKE:
			pCommand->Result = xrtWsWriterFinishTake(
				pCommand->Writer,
				pCommand->Buffer->Data,
				pCommand->Buffer->Size
			);
			break;

		case XWS_NATIVE_WRITER_DESTROY:
			xrtWsWriterDestroy(pCommand->Writer);
			break;
	}

	if ( ((pCommand->Kind == XWS_NATIVE_WRITER_WRITE_TAKE) ||
		  (pCommand->Kind == XWS_NATIVE_WRITER_FINISH_TAKE)) &&
		 (pCommand->Result == XNET_RESULT_OK) ) {
		pCommand->Buffer->Data = NULL;
		pCommand->Buffer->Size = 0u;
		pCommand->Buffer->Capacity = 0u;
	}
}



/* 在所属 Worker 销毁已经从包装对象移出的 Writer。 */
static void xwsNativeWriterDestroyValue(
	xnetworker* pWorker,
	xwswriter* pWriter
)
{
	xws_native_writer_command Command;

	if ( pWriter == NULL ) {
		return;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = XWS_NATIVE_WRITER_DESTROY;
	Command.Writer = pWriter;
	(void)xwsNativeWorkerCall(pWorker, xwsNativeWriterRun, &Command);
}



/* 占用唯一 Writer 事务；并发和回调重入不等待，统一返回 AGAIN。 */
static xnetresult xwsNativeWriterAcquire(
	xws_native_writer* pOwner,
	xwswriter** pWriter,
	xnetworker** pWorker
)
{
	xnetresult Result = XNET_RESULT_CLOSED;

	xrtSpinLock(&pOwner->Lock);
	if ( !pOwner->Closing && (pOwner->Writer != NULL) ) {
		if ( pOwner->Busy ) {
			Result = XNET_RESULT_AGAIN;
		} else {
			pOwner->Busy = true;
			(void)xrtAtomic32FetchAdd(
				&pOwner->References,
				1u,
				XMEMORY_RELAXED
			);
			*pWriter = pOwner->Writer;
			*pWorker = pOwner->Worker;
			Result = XNET_RESULT_OK;
		}
	}
	xrtSpinUnlock(&pOwner->Lock);
	return Result;
}



/* 结束事务；并发析构请求由最后一个活动事务接续完成。 */
static void xwsNativeWriterComplete(
	xws_native_writer* pOwner,
	bool bFinished
)
{
	xwswriter* pDestroy = NULL;
	xnetworker* pWorker;

	xrtSpinLock(&pOwner->Lock);
	pOwner->Busy = false;
	if ( bFinished ) {
		pOwner->Finished = true;
	}
	pWorker = pOwner->Worker;
	if ( pOwner->Closing ) {
		pDestroy = pOwner->Writer;
		pOwner->Writer = NULL;
	}
	xrtSpinUnlock(&pOwner->Lock);
	xwsNativeWriterDestroyValue(pWorker, pDestroy);
	xwsNativeWriterRelease(pOwner);
}



/* 为 Connection 建立一条独占的分片消息 Writer。 */
XLANG_XWS_EXPORT void* xws_writer_begin(
	void* pConnectionHandle,
	long long iOpcode,
	bool bCompressed
)
{
	xwsconn* pConnection;
	xnetworker* pWorker;
	xws_native_writer* pOwner;
	xws_native_writer_command Command;

	if ( (iOpcode != XWS_OPCODE_TEXT) && (iOpcode != XWS_OPCODE_BINARY) ) {
		return NULL;
	}
	pConnection = xwsNativeConnectionRef(pConnectionHandle);
	pWorker = pConnection != NULL ? xrtWsConnWorker(pConnection) : NULL;
	if ( (pConnection == NULL) || (pWorker == NULL) ) {
		xrtWsConnDestroy(pConnection);
		return NULL;
	}
	pOwner = (xws_native_writer*)xrtCalloc(1u, sizeof(*pOwner));
	if ( (pOwner == NULL) || !xrtSpinInit(&pOwner->Lock) ) {
		xrtFree(pOwner);
		xrtWsConnDestroy(pConnection);
		return NULL;
	}
	xrtAtomic32Init(&pOwner->References, 1u);
	memset(&Command, 0, sizeof(Command));
	Command.Kind = XWS_NATIVE_WRITER_BEGIN;
	Command.Connection = pConnection;
	Command.Opcode = (xwsopcode)iOpcode;
	Command.Compressed = bCompressed;
	if ( !xwsNativeWorkerCall(pWorker, xwsNativeWriterRun, &Command) ||
		 (Command.Writer == NULL) ) {
		(void)xrtSpinDestroy(&pOwner->Lock);
		xrtFree(pOwner);
		xrtWsConnDestroy(pConnection);
		return NULL;
	}
	pOwner->Worker = pWorker;
	pOwner->Writer = Command.Writer;
	xrtWsConnDestroy(pConnection);
	return pOwner;
}



/* point 构造只接管已经建立的 Writer 包装句柄。 */
XLANG_XWS_EXPORT void* xws_writer_wrap(void* pHandle)
{
	return pHandle;
}



/* 在所属 Worker 中销毁 Writer，并释放语言包装。 */
XLANG_XWS_EXPORT void xws_writer_destroy(void* pHandle)
{
	xws_native_writer* pOwner = (xws_native_writer*)pHandle;
	xwswriter* pDestroy = NULL;
	xnetworker* pWorker;

	if ( pOwner == NULL ) {
		return;
	}
	xrtSpinLock(&pOwner->Lock);
	pOwner->Closing = true;
	pWorker = pOwner->Worker;
	if ( !pOwner->Busy ) {
		pDestroy = pOwner->Writer;
		pOwner->Writer = NULL;
	}
	xrtSpinUnlock(&pOwner->Lock);
	xwsNativeWriterDestroyValue(pWorker, pDestroy);
	xwsNativeWriterRelease(pOwner);
}



/* 复制提交普通或最终分片。 */
XLANG_XWS_EXPORT long long xws_writer_write(
	void* pHandle,
	void* pData,
	size_t iSize,
	bool bFinal
)
{
	xws_native_writer* pOwner = (xws_native_writer*)pHandle;
	xws_native_writer_command Command;
	xwswriter* pWriter = NULL;
	xnetworker* pWorker = NULL;
	xnetresult Acquire;

	if ( (pOwner == NULL) || ((pData == NULL) && (iSize != 0u)) ) {
		return XNET_RESULT_ERROR;
	}
	Acquire = xwsNativeWriterAcquire(pOwner, &pWriter, &pWorker);
	if ( Acquire != XNET_RESULT_OK ) {
		return Acquire;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = bFinal ? XWS_NATIVE_WRITER_FINISH : XWS_NATIVE_WRITER_WRITE;
	Command.Data = (xbytesview){ (cbytes)pData, iSize };
	Command.Result = XNET_RESULT_ERROR;
	Command.Writer = pWriter;
	if ( !xwsNativeWorkerCall(pWorker, xwsNativeWriterRun, &Command) ) {
		Command.Result = XNET_RESULT_ERROR;
	}
	xwsNativeWriterComplete(pOwner,
		bFinal && (Command.Result == XNET_RESULT_OK));
	return Command.Result;
}



/* 成功受理后移交 Buffer 的 XRT 存储；空分片自动走复制路径。 */
/* 文本分片便利入口使用稳定字符串 ABI。 */
XLANG_XWS_EXPORT long long xws_writer_write_text(
	void* pHandle,
	const char* sText,
	size_t iSize,
	bool bFinal
)
{
	return xws_writer_write(pHandle, (void*)sText, iSize, bFinal);
}



XLANG_XWS_EXPORT long long xws_writer_write_move(
	void* pHandle,
	void* pBuffer,
	bool bFinal
)
{
	xws_native_writer* pOwner = (xws_native_writer*)pHandle;
	xbuffer* pOwned = (xbuffer*)pBuffer;
	xws_native_writer_command Command;
	xwswriter* pWriter = NULL;
	xnetworker* pWorker = NULL;
	xnetresult Acquire;

	if ( (pOwner == NULL) || (pOwned == NULL) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pOwned->Size == 0u ) {
		return xws_writer_write(pHandle, NULL, 0u, bFinal);
	}
	Acquire = xwsNativeWriterAcquire(pOwner, &pWriter, &pWorker);
	if ( Acquire != XNET_RESULT_OK ) {
		return Acquire;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = bFinal ?
		XWS_NATIVE_WRITER_FINISH_TAKE : XWS_NATIVE_WRITER_WRITE_TAKE;
	Command.Buffer = pOwned;
	Command.Result = XNET_RESULT_ERROR;
	Command.Writer = pWriter;
	if ( !xwsNativeWorkerCall(pWorker, xwsNativeWriterRun, &Command) ) {
		Command.Result = XNET_RESULT_ERROR;
	}
	xwsNativeWriterComplete(pOwner,
		bFinal && (Command.Result == XNET_RESULT_OK));
	return Command.Result;
}



/* 并发读取 Writer 是否已受理最终分片。 */
XLANG_XWS_EXPORT bool xws_writer_finished(void* pHandle)
{
	xws_native_writer* pOwner = (xws_native_writer*)pHandle;
	bool bFinished = false;

	if ( pOwner == NULL ) {
		return false;
	}
	xrtSpinLock(&pOwner->Lock);
	bFinished = pOwner->Finished;
	xrtSpinUnlock(&pOwner->Lock);
	return bFinished;
}
