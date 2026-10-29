#include "../internal/xrt_http_body_stream.h"



#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM)

/* 队列节点保存一次写入及其唯一外部释放责任。 */
typedef struct __xrt_http_body_stream_node {
	struct __xrt_http_body_stream_node* Next;
	xhttpbodystream* Stream;
	cbytes Data;
	size_t Size;
	size_t Offset;
	xhttpbodyreleaseproc Release;
	ptr ReleaseContext;
	bool Leased;
	bool Detached;
} __xrt_http_body_stream_node;



/* Future 信号离开 Stream 锁后完成，避免同步延续重入队列。 */
typedef enum __xrt_http_body_stream_signal_mode {
	XRT_HTTP_BODY_STREAM_SIGNAL_NONE = 0,
	XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE,
	XRT_HTTP_BODY_STREAM_SIGNAL_REJECT,
	XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
} __xrt_http_body_stream_signal_mode;



typedef struct __xrt_http_body_stream_signal {
	xpromise* Promise;
	xfuture* Future;
	const xerror* Error;
	__xrt_http_body_stream_signal_mode Mode;
} __xrt_http_body_stream_signal;



/* Stream 同时保存生产端、Body 工厂、队列、预算和两类等待代际。 */
struct xhttpbodystream {
	volatile int32 References;
	volatile int32 Producers;
	xmutex Lock;
	__xrt_http_body_stream_node* Head;
	__xrt_http_body_stream_node* Tail;
	xpromise* ReadPromise;
	xfuture* ReadFuture;
	xpromise* WritePromise;
	xfuture* WriteFuture;
	xerror* Error;
	xhttpbodystreamconfig Config;
	size_t PendingBytes;
	size_t PendingChunks;
	uint64 WrittenBytes;
	uint64 ReadBytes;
	bool Opened;
	bool InputClosed;
	bool ConsumerClosed;
	bool Failed;
	bool WriteBlocked;
};



/* 创建并设置 Body Stream 域错误。 */
static xerror* __xrtHttpBodyStreamErrorCreate(
	xerrkind Kind,
	xhttpbodystreamerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.body.stream";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 设置当前线程的 Body Stream 域错误。 */
static void __xrtHttpBodyStreamSetError(
	xerrkind Kind,
	xhttpbodystreamerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpBodyStreamErrorCreate(
		Kind, Code, sOperation, sMessage, pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 在离开 Stream 锁后发布已经持有的错误引用。 */
static void __xrtHttpBodyStreamPublishError(xerror* pError)
{
	if ( pError == NULL ) {
		return;
	}
	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 增加 Stream 内部生命周期引用。 */
static xhttpbodystream* __xrtHttpBodyStreamRef(
	xhttpbodystream* pStream
)
{
	if ( ( pStream == NULL ) ||
		( xrtRefRetain(&pStream->References) < 0 ) ) {
		return NULL;
	}
	return pStream;
}



/* 释放已接管节点的数据和描述符。 */
static void __xrtHttpBodyStreamNodeDestroy(
	__xrt_http_body_stream_node* pNode
)
{
	if ( pNode == NULL ) {
		return;
	}
	if ( pNode->Release != NULL ) {
		pNode->Release(
			pNode->ReleaseContext,
			pNode->Data,
			pNode->Size
		);
	}
	xrtFree(pNode);
}



/* 释放一条已经与 Stream 脱离的节点链。 */
static void __xrtHttpBodyStreamNodesDestroy(
	__xrt_http_body_stream_node* pNode
)
{
	while ( pNode != NULL ) {
		__xrt_http_body_stream_node* pNext = pNode->Next;

		__xrtHttpBodyStreamNodeDestroy(pNode);
		pNode = pNext;
	}
}



/* 完成并释放一个已经离开 Stream 锁的 Future 代际。 */
static void __xrtHttpBodyStreamSignal(
	__xrt_http_body_stream_signal* pSignal
)
{
	if ( ( pSignal == NULL ) ||
		( pSignal->Promise == NULL ) ) {
		return;
	}
	if ( pSignal->Mode == XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE ) {
		(void)xrtPromiseResolve(pSignal->Promise, NULL);
	} else if ( pSignal->Mode ==
		XRT_HTTP_BODY_STREAM_SIGNAL_REJECT ) {
		(void)xrtPromiseReject(
			pSignal->Promise,
			pSignal->Error
		);
	} else {
		(void)xrtPromiseClose(pSignal->Promise);
	}
	xrtPromiseDestroy(pSignal->Promise);
	xrtFutureDestroy(pSignal->Future);
	memset(pSignal, 0, sizeof(*pSignal));
}



/* 从锁内摘除可读等待代际。 */
static void __xrtHttpBodyStreamReadSignalTake(
	xhttpbodystream* pStream,
	__xrt_http_body_stream_signal* pSignal,
	__xrt_http_body_stream_signal_mode Mode
)
{
	if ( pStream->ReadPromise == NULL ) {
		return;
	}
	pSignal->Promise = pStream->ReadPromise;
	pSignal->Future = pStream->ReadFuture;
	pSignal->Error = pStream->Error;
	pSignal->Mode = Mode;
	pStream->ReadPromise = NULL;
	pStream->ReadFuture = NULL;
}



/* 从锁内摘除可写等待代际。 */
static void __xrtHttpBodyStreamWriteSignalTake(
	xhttpbodystream* pStream,
	__xrt_http_body_stream_signal* pSignal,
	__xrt_http_body_stream_signal_mode Mode
)
{
	if ( pStream->WritePromise == NULL ) {
		return;
	}
	pSignal->Promise = pStream->WritePromise;
	pSignal->Future = pStream->WriteFuture;
	pSignal->Error = pStream->Error;
	pSignal->Mode = Mode;
	pStream->WritePromise = NULL;
	pStream->WriteFuture = NULL;
}



/* 判断至少一个新字节和一个新 Chunk 能否进入硬预算。 */
static bool __xrtHttpBodyStreamWritable(
	const xhttpbodystream* pStream
)
{
	return ( pStream->PendingBytes < pStream->Config.MaxBytes ) &&
		( pStream->PendingChunks < pStream->Config.MaxChunks );
}



/* 验证非空载荷范围不会回绕或覆盖 Stream 内部状态。 */
static bool __xrtHttpBodyStreamDataValid(
	const xhttpbodystream* pStream,
	xbytesview Data
)
{
	if ( ( pStream == NULL ) || ( Data.Size == 0 ) ||
		!xrtMemRangeValid(Data.Data, Data.Size) ) {
		return false;
	}
	return !xrtMemRangesOverlap(
		Data.Data, Data.Size, pStream, sizeof(*pStream)
	);
}



/* 判断 Reader 能否取得数据或观察正常 EOF。 */
static bool __xrtHttpBodyStreamReadable(
	const xhttpbodystream* pStream
)
{
	if ( pStream->Head != NULL ) {
		return !pStream->Head->Leased;
	}
	return pStream->InputClosed;
}



/* 累计流量采用饱和计数，避免无限期流在极端生命周期中回绕。 */
static uint64 __xrtHttpBodyStreamCounterAdd(
	uint64 iCurrent,
	size_t iAdded
)
{
	if ( ( (uint64)iAdded > ( UINT64_MAX - iCurrent ) ) ||
		( ( sizeof(size_t) > sizeof(uint64) ) &&
		  ( iAdded > (size_t)UINT64_MAX ) ) ) {
		return UINT64_MAX;
	}
	return iCurrent + (uint64)iAdded;
}



/* 丢弃全部未租用节点；活动租约脱链后由释放回调回收。 */
static __xrt_http_body_stream_node*
__xrtHttpBodyStreamDiscardLocked(xhttpbodystream* pStream)
{
	__xrt_http_body_stream_node* pNode = pStream->Head;
	__xrt_http_body_stream_node* pFree = NULL;
	__xrt_http_body_stream_node* pTail = NULL;

	pStream->Head = NULL;
	pStream->Tail = NULL;
	while ( pNode != NULL ) {
		__xrt_http_body_stream_node* pNext = pNode->Next;

		pNode->Next = NULL;
		if ( pNode->Leased ) {
			pNode->Detached = true;
		} else {
			pStream->PendingBytes -= pNode->Size;
			pStream->PendingChunks--;
			if ( pTail == NULL ) {
				pFree = pNode;
			} else {
				pTail->Next = pNode;
			}
			pTail = pNode;
		}
		pNode = pNext;
	}
	return pFree;
}



/* 释放 Stream 最后一个内部引用。 */
static void __xrtHttpBodyStreamRelease(
	xhttpbodystream* pStream
)
{
	__xrt_http_body_stream_signal Read = { 0 };
	__xrt_http_body_stream_signal Write = { 0 };
	__xrt_http_body_stream_node* pNodes;

	if ( ( pStream == NULL ) ||
		( xrtRefRelease(&pStream->References) != 0 ) ) {
		return;
	}
	pNodes = pStream->Head;
	pStream->Head = NULL;
	pStream->Tail = NULL;
	__xrtHttpBodyStreamReadSignalTake(
		pStream, &Read, XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
	);
	__xrtHttpBodyStreamWriteSignalTake(
		pStream, &Write, XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
	);
	__xrtHttpBodyStreamSignal(&Read);
	__xrtHttpBodyStreamSignal(&Write);
	__xrtHttpBodyStreamNodesDestroy(pNodes);
	xrtErrorFree(pStream->Error);
	(void)xrtMutexUnit(&pStream->Lock);
	memset(pStream, 0, sizeof(*pStream));
	xrtFree(pStream);
}



/* 释放 xrtHttpBodyStreamWriteTake 接管的数据。 */
static void __xrtHttpBodyStreamFreeData(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 预留一个写入的完整硬预算，阻止并发生产者瞬时越界。 */
static xhttpbodystreamresult __xrtHttpBodyStreamReserve(
	xhttpbodystream* pStream,
	size_t iSize
)
{
	xhttpbodystreamresult Result;
	xerror* pFailure = NULL;
	bool bLimit = false;

	(void)xrtMutexLock(&pStream->Lock);
	if ( pStream->Failed ) {
		pFailure = xrtErrorRef(pStream->Error);
		Result = XHTTP_BODY_STREAM_ERROR;
	} else if ( pStream->InputClosed || pStream->ConsumerClosed ) {
		Result = XHTTP_BODY_STREAM_CLOSED;
	} else if ( iSize > pStream->Config.MaxBytes ) {
		Result = XHTTP_BODY_STREAM_ERROR;
		bLimit = true;
	} else if ( ( iSize >
		( pStream->Config.MaxBytes - pStream->PendingBytes ) ) ||
		( pStream->PendingChunks == pStream->Config.MaxChunks ) ) {
		pStream->WriteBlocked = true;
		Result = XHTTP_BODY_STREAM_AGAIN;
	} else {
		pStream->PendingBytes += iSize;
		pStream->PendingChunks++;
		Result = XHTTP_BODY_STREAM_OK;
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	__xrtHttpBodyStreamPublishError(pFailure);
	if ( bLimit ) {
		__xrtHttpBodyStreamSetError(
			XERR_RANGE,
			XHTTP_BODY_STREAM_ERROR_LIMIT,
			"write",
			"HTTP body stream Chunk exceeds its byte budget",
			NULL
		);
	}
	return Result;
}



/* 回滚尚未进入队列的预算，并唤醒可能被该预留阻塞的生产者。 */
static void __xrtHttpBodyStreamReserveRollback(
	xhttpbodystream* pStream,
	size_t iSize
)
{
	__xrt_http_body_stream_signal Write = { 0 };

	(void)xrtMutexLock(&pStream->Lock);
	pStream->PendingBytes -= iSize;
	pStream->PendingChunks--;
	if ( !pStream->Failed && !pStream->InputClosed &&
		!pStream->ConsumerClosed &&
		__xrtHttpBodyStreamWritable(pStream) ) {
		pStream->WriteBlocked = false;
		__xrtHttpBodyStreamWriteSignalTake(
			pStream,
			&Write,
			XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE
		);
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	__xrtHttpBodyStreamSignal(&Write);
}



/* 把已分配节点提交到队尾，失败时只归还预算而不接管载荷。 */
static xhttpbodystreamresult __xrtHttpBodyStreamCommit(
	xhttpbodystream* pStream,
	__xrt_http_body_stream_node* pNode
)
{
	__xrt_http_body_stream_signal Read = { 0 };
	xhttpbodystreamresult Result;
	xerror* pFailure = NULL;

	(void)xrtMutexLock(&pStream->Lock);
	if ( pStream->Failed ) {
		pStream->PendingBytes -= pNode->Size;
		pStream->PendingChunks--;
		pFailure = xrtErrorRef(pStream->Error);
		Result = XHTTP_BODY_STREAM_ERROR;
	} else if ( pStream->InputClosed || pStream->ConsumerClosed ) {
		pStream->PendingBytes -= pNode->Size;
		pStream->PendingChunks--;
		Result = XHTTP_BODY_STREAM_CLOSED;
	} else {
		pNode->Stream = pStream;
		if ( pStream->Tail == NULL ) {
			pStream->Head = pNode;
		} else {
			pStream->Tail->Next = pNode;
		}
		pStream->Tail = pNode;
		pStream->WrittenBytes = __xrtHttpBodyStreamCounterAdd(
			pStream->WrittenBytes,
			pNode->Size
		);
		__xrtHttpBodyStreamReadSignalTake(
			pStream,
			&Read,
			XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE
		);
		Result = XHTTP_BODY_STREAM_OK;
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	__xrtHttpBodyStreamPublishError(pFailure);
	__xrtHttpBodyStreamSignal(&Read);
	return Result;
}



/* 在失败分配回滚预算后恢复原始线程错误。 */
static xhttpbodystreamresult __xrtHttpBodyStreamAllocationFail(
	xhttpbodystream* pStream,
	size_t iSize
)
{
	xerror* pError = xrtTakeError();

	if ( pError == NULL ) {
		__xhttpErrorSetOutOfMemory();
		pError = xrtTakeError();
	}
	__xrtHttpBodyStreamReserveRollback(pStream, iSize);
	if ( pError != NULL ) {
		__xrtHttpBodyStreamPublishError(pError);
	}
	return XHTTP_BODY_STREAM_ERROR;
}



/* 分配外部所有权节点并在成功时提交释放责任。 */
static xhttpbodystreamresult __xrtHttpBodyStreamWriteOwned(
	xhttpbodystream* pStream,
	xbytesview Data,
	xhttpbodyreleaseproc pRelease,
	ptr pContext
)
{
	__xrt_http_body_stream_node* pNode;
	xhttpbodystreamresult Result = __xrtHttpBodyStreamReserve(
		pStream, Data.Size
	);

	if ( Result != XHTTP_BODY_STREAM_OK ) {
		return Result;
	}
	pNode = (__xrt_http_body_stream_node*)xrtCalloc(
		1, sizeof(*pNode)
	);
	if ( pNode == NULL ) {
		return __xrtHttpBodyStreamAllocationFail(
			pStream, Data.Size
		);
	}
	pNode->Data = Data.Data;
	pNode->Size = Data.Size;
	pNode->Release = pRelease;
	pNode->ReleaseContext = pContext;
	Result = __xrtHttpBodyStreamCommit(pStream, pNode);
	if ( Result != XHTTP_BODY_STREAM_OK ) {
		xrtFree(pNode);
	}
	return Result;
}



/* 填充一次精确长度的单分配节点。 */
xhttpbodystreamresult __xrtHttpBodyStreamBuild(
	xhttpbodystream* pStream,
	size_t iSize,
	__xrt_http_body_stream_fill_proc pFill,
	ptr pData
)
{
	__xrt_http_body_stream_node* pNode;
	xhttpbodystreamresult Result;
	size_t iTotal;

	if ( ( pStream == NULL ) || ( pFill == NULL ) ||
		( iSize == 0 ) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"write",
			"HTTP body stream, fill callback and non-empty data are required",
			NULL
		);
		return XHTTP_BODY_STREAM_ERROR;
	}
	Result = __xrtHttpBodyStreamReserve(pStream, iSize);
	if ( Result != XHTTP_BODY_STREAM_OK ) {
		return Result;
	}
	if ( iSize > (SIZE_MAX - sizeof(*pNode)) ) {
		__xhttpErrorSetSizeOverflow();
		return __xrtHttpBodyStreamAllocationFail(
			pStream, iSize
		);
	}
	iTotal = sizeof(*pNode) + iSize;
	pNode = (__xrt_http_body_stream_node*)xrtCalloc(
		1, iTotal
	);
	if ( pNode == NULL ) {
		return __xrtHttpBodyStreamAllocationFail(
			pStream, iSize
		);
	}
	pNode->Data = (cbytes)(pNode + 1);
	pNode->Size = iSize;
	if ( !pFill((void*)pNode->Data, iSize, pData) ) {
		xerror* pError = xrtTakeError();

		xrtFree(pNode);
		if ( pError == NULL ) {
			__xrtHttpBodyStreamSetError(
				XERR_INTERNAL,
				XHTTP_BODY_STREAM_ERROR_INTERNAL,
				"write",
				"HTTP body stream fill callback failed",
				NULL
			);
			pError = xrtTakeError();
		}
		__xrtHttpBodyStreamReserveRollback(pStream, iSize);
		if ( pError != NULL ) {
			__xrtHttpBodyStreamPublishError(pError);
		}
		return XHTTP_BODY_STREAM_ERROR;
	}
	Result = __xrtHttpBodyStreamCommit(pStream, pNode);
	if ( Result != XHTTP_BODY_STREAM_OK ) {
		xrtFree(pNode);
	}
	return Result;
}



/* 复制写入的单分配填充过程。 */
static bool __xrtHttpBodyStreamCopyFill(
	void* pOutput,
	size_t iSize,
	ptr pData
)
{
	memcpy(pOutput, pData, iSize);
	return true;
}



/* 释放 Reader 借出的一个片段并推进节点、背压和可读状态。 */
static void __xrtHttpBodyStreamChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	__xrt_http_body_stream_node* pNode =
		(__xrt_http_body_stream_node*)pContext;
	xhttpbodystream* pStream = pNode->Stream;
	__xrt_http_body_stream_signal Read = { 0 };
	__xrt_http_body_stream_signal Write = { 0 };
	bool bDestroy = false;

	(void)pData;
	(void)iSize;
	(void)xrtMutexLock(&pStream->Lock);
	pNode->Leased = false;
	if ( pNode->Detached || (pNode->Offset == pNode->Size) ) {
		if ( !pNode->Detached ) {
			pStream->Head = pNode->Next;
			if ( pStream->Head == NULL ) {
				pStream->Tail = NULL;
			}
		}
		pNode->Next = NULL;
		pStream->PendingBytes -= pNode->Size;
		pStream->PendingChunks--;
		bDestroy = true;
	}
	if ( !pStream->Failed && !pStream->ConsumerClosed &&
		__xrtHttpBodyStreamReadable(pStream) ) {
		__xrtHttpBodyStreamReadSignalTake(
			pStream,
			&Read,
			XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE
		);
	}
	if ( !pStream->Failed && !pStream->InputClosed &&
		!pStream->ConsumerClosed &&
		__xrtHttpBodyStreamWritable(pStream) ) {
		pStream->WriteBlocked = false;
		__xrtHttpBodyStreamWriteSignalTake(
			pStream,
			&Write,
			XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE
		);
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	if ( bDestroy ) {
		__xrtHttpBodyStreamNodeDestroy(pNode);
	}
	__xrtHttpBodyStreamSignal(&Read);
	__xrtHttpBodyStreamSignal(&Write);
	__xrtHttpBodyStreamRelease(pStream);
}



/* 返回排队节点的下一段，活动租约释放前维持硬预算。 */
static xhttpbodystatus __xrtHttpBodyStreamNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xhttpbodystream* pStream = (xhttpbodystream*)pContext;
	__xrt_http_body_stream_node* pNode;
	size_t iSize;
	xhttpbodystatus Status;
	xerror* pFailure = NULL;

	(void)xrtMutexLock(&pStream->Lock);
	if ( pStream->Failed ) {
		pFailure = xrtErrorRef(pStream->Error);
		Status = XHTTP_BODY_ERROR;
	} else if ( pStream->Head == NULL ) {
		Status = pStream->InputClosed ?
			XHTTP_BODY_EOF : XHTTP_BODY_AGAIN;
	} else if ( pStream->Head->Leased ) {
		Status = XHTTP_BODY_AGAIN;
	} else {
		pNode = pStream->Head;
		iSize = pNode->Size - pNode->Offset;
		if ( iSize > iMaxBytes ) {
			iSize = iMaxBytes;
		}
		if ( __xrtHttpBodyStreamRef(pStream) == NULL ) {
			Status = XHTTP_BODY_ERROR;
		} else {
			pNode->Leased = true;
			pChunk->Data = pNode->Data + pNode->Offset;
			pChunk->Size = iSize;
			pChunk->Release = __xrtHttpBodyStreamChunkRelease;
			pChunk->Context = pNode;
			pNode->Offset += iSize;
			pStream->ReadBytes = __xrtHttpBodyStreamCounterAdd(
				pStream->ReadBytes,
				iSize
			);
			Status = XHTTP_BODY_DATA;
		}
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	__xrtHttpBodyStreamPublishError(pFailure);
	return Status;
}



/* 按当前终态或就绪状态摘除一个已安装的等待代际。 */
static void __xrtHttpBodyStreamWaitSignalTake(
	xhttpbodystream* pStream,
	__xrt_http_body_stream_signal* pSignal,
	bool bRead
)
{
	if ( pStream->Failed ) {
		if ( bRead ) {
			__xrtHttpBodyStreamReadSignalTake(
				pStream,
				pSignal,
				XRT_HTTP_BODY_STREAM_SIGNAL_REJECT
			);
		} else {
			__xrtHttpBodyStreamWriteSignalTake(
				pStream,
				pSignal,
				XRT_HTTP_BODY_STREAM_SIGNAL_REJECT
			);
		}
		return;
	}
	if ( bRead ) {
		if ( pStream->ConsumerClosed ) {
			__xrtHttpBodyStreamReadSignalTake(
				pStream,
				pSignal,
				XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
			);
		} else if ( __xrtHttpBodyStreamReadable(pStream) ) {
			__xrtHttpBodyStreamReadSignalTake(
				pStream,
				pSignal,
				XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE
			);
		}
	} else if ( pStream->InputClosed || pStream->ConsumerClosed ) {
		__xrtHttpBodyStreamWriteSignalTake(
			pStream,
			pSignal,
			XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
		);
	} else if ( !pStream->WriteBlocked &&
		__xrtHttpBodyStreamWritable(pStream) ) {
		__xrtHttpBodyStreamWriteSignalTake(
			pStream,
			pSignal,
			XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE
		);
	}
}



/* 锁外创建、锁内安装并复查一个共享等待代际。 */
static xfuture* __xrtHttpBodyStreamWaitGeneration(
	xhttpbodystream* pStream,
	bool bRead
)
{
	__xrt_http_body_stream_signal Signal = { 0 };
	xpromise* pCandidatePromise = NULL;
	xfuture* pCandidateFuture = NULL;
	xfuture* pCurrent;
	xfuture* pResult = NULL;

	(void)xrtMutexLock(&pStream->Lock);
	pCurrent = bRead ? pStream->ReadFuture : pStream->WriteFuture;
	if ( pCurrent == NULL ) {
		(void)xrtMutexUnlock(&pStream->Lock);
		pCandidatePromise = xrtPromiseCreate(
			&pCandidateFuture, NULL
		);
		if ( pCandidatePromise == NULL ) {
			return NULL;
		}
		(void)xrtMutexLock(&pStream->Lock);
		pCurrent = bRead ?
			pStream->ReadFuture : pStream->WriteFuture;
		if ( pCurrent == NULL ) {
			if ( bRead ) {
				pStream->ReadPromise = pCandidatePromise;
				pStream->ReadFuture = pCandidateFuture;
				pCurrent = pStream->ReadFuture;
			} else {
				pStream->WritePromise = pCandidatePromise;
				pStream->WriteFuture = pCandidateFuture;
				pCurrent = pStream->WriteFuture;
			}
			pCandidatePromise = NULL;
			pCandidateFuture = NULL;
		}
	}
	if ( pCurrent != NULL ) {
		pResult = xrtFutureRef(pCurrent);
		if ( pResult != NULL ) {
			__xrtHttpBodyStreamWaitSignalTake(
				pStream, &Signal, bRead
			);
		}
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	if ( pCandidatePromise != NULL ) {
		xrtPromiseDestroy(pCandidatePromise);
		xrtFutureDestroy(pCandidateFuture);
	}
	__xrtHttpBodyStreamSignal(&Signal);
	return pResult;
}



/* 创建或复用当前可读等待代际，并处理安装竞态。 */
static xfuture* __xrtHttpBodyStreamWaitRead(ptr pContext)
{
	return __xrtHttpBodyStreamWaitGeneration(
		(xhttpbodystream*)pContext, true
	);
}



/* 关闭唯一 Reader，丢弃未租用节点并终结全部等待。 */
static void __xrtHttpBodyStreamReaderClose(ptr pContext)
{
	xhttpbodystream* pStream = (xhttpbodystream*)pContext;
	__xrt_http_body_stream_signal Read = { 0 };
	__xrt_http_body_stream_signal Write = { 0 };
	__xrt_http_body_stream_node* pNodes = NULL;

	(void)xrtMutexLock(&pStream->Lock);
	if ( !pStream->ConsumerClosed ) {
		pStream->ConsumerClosed = true;
		pNodes = __xrtHttpBodyStreamDiscardLocked(pStream);
		__xrtHttpBodyStreamReadSignalTake(
			pStream,
			&Read,
			XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
		);
		__xrtHttpBodyStreamWriteSignalTake(
			pStream,
			&Write,
			XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
		);
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	__xrtHttpBodyStreamNodesDestroy(pNodes);
	__xrtHttpBodyStreamSignal(&Read);
	__xrtHttpBodyStreamSignal(&Write);
}



/* 打开一次性 Stream Reader。 */
static bool __xrtHttpBodyStreamOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	xhttpbodystream* pStream = (xhttpbodystream*)pFactory;
	bool bResult;

	(void)xrtMutexLock(&pStream->Lock);
	if ( pStream->Opened || pStream->ConsumerClosed ) {
		bResult = false;
	} else {
		pStream->Opened = true;
		memset(pOps, 0, sizeof(*pOps));
		pOps->Next = __xrtHttpBodyStreamNext;
		pOps->Close = __xrtHttpBodyStreamReaderClose;
		pOps->Wait = __xrtHttpBodyStreamWaitRead;
		*ppReader = pStream;
		bResult = true;
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	if ( !bResult ) {
		__xrtHttpBodyStreamSetError(
			XERR_STATE,
			XHTTP_BODY_STREAM_ERROR_STATE,
			"open",
			"HTTP body stream consumer is unavailable",
			NULL
		);
	}
	return bResult;
}



/* Body 工厂释放时确保没有生产者继续写入失去消费者的队列。 */
static void __xrtHttpBodyStreamFactoryDestroy(ptr pFactory)
{
	xhttpbodystream* pStream = (xhttpbodystream*)pFactory;

	__xrtHttpBodyStreamReaderClose(pStream);
	__xrtHttpBodyStreamRelease(pStream);
}



/* 发布正常输入关闭并唤醒 EOF 与 writable 等待者。 */
static bool __xrtHttpBodyStreamCloseInput(
	xhttpbodystream* pStream,
	bool bPublic
)
{
	__xrt_http_body_stream_signal Read = { 0 };
	__xrt_http_body_stream_signal Write = { 0 };
	xerror* pFailure = NULL;
	bool bResult = true;

	(void)xrtMutexLock(&pStream->Lock);
	if ( pStream->Failed ) {
		if ( bPublic ) {
			pFailure = xrtErrorRef(pStream->Error);
		}
		bResult = false;
	} else if ( !pStream->InputClosed ) {
		pStream->InputClosed = true;
		if ( __xrtHttpBodyStreamReadable(pStream) ) {
			__xrtHttpBodyStreamReadSignalTake(
				pStream,
				&Read,
				XRT_HTTP_BODY_STREAM_SIGNAL_RESOLVE
			);
		}
		__xrtHttpBodyStreamWriteSignalTake(
			pStream,
			&Write,
			XRT_HTTP_BODY_STREAM_SIGNAL_CLOSE
		);
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	__xrtHttpBodyStreamPublishError(pFailure);
	__xrtHttpBodyStreamSignal(&Read);
	__xrtHttpBodyStreamSignal(&Write);
	if ( bPublic && !bResult && (xrtGetError() == NULL) ) {
		__xrtHttpBodyStreamSetError(
			XERR_STATE,
			XHTTP_BODY_STREAM_ERROR_STATE,
			"close",
			"HTTP body stream has already failed",
			NULL
		);
	}
	return bResult;
}



/* 初始化有界生产流默认配置。 */
XRT_API void xrtHttpBodyStreamConfigInit(
	xhttpbodystreamconfig* pConfig
)
{
	const xhttpbodystreamconfig Config = {
		XHTTP_BODY_STREAM_BYTES_DEFAULT,
		XHTTP_BODY_STREAM_CHUNKS_DEFAULT
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"init-config",
			"HTTP body stream config range is invalid",
			NULL
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证两个硬预算都能容纳至少一个非空 Chunk。 */
XRT_API bool xrtHttpBodyStreamConfigValid(
	const xhttpbodystreamconfig* pConfig
)
{
	xhttpbodystreamconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	return (Config.MaxBytes != 0) && (Config.MaxChunks != 0);
}



/* 创建共享状态、生产端和未知长度一次性 Body。 */
XRT_API xhttpbody* xrtHttpBodyStreamCreate(
	const xhttpbodystreamconfig* pConfig,
	xhttpbodystream** ppStream
)
{
	static const xhttpbodyops Ops = {
		__xrtHttpBodyStreamOpen,
		__xrtHttpBodyStreamFactoryDestroy
	};
	xhttpbodystreamconfig Config;
	xhttpbodystream* pStream;
	xhttpbodystream* pOutput = NULL;
	xhttpbody* pBody;

	if ( !xrtMemRangeValid(ppStream, sizeof(pOutput)) ||
		((pConfig != NULL) && !xrtMemRangeValid(
			pConfig, sizeof(Config)
		)) || ( ( pConfig != NULL ) &&
		xrtMemRangesOverlap(
			ppStream,
			sizeof(pOutput),
			pConfig,
			sizeof(Config)
		)) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"create",
			"HTTP body stream output is null or overlaps its config",
			NULL
		);
		return NULL;
	}
	if ( pConfig == NULL ) {
		xrtHttpBodyStreamConfigInit(&Config);
	} else {
		memcpy(&Config, pConfig, sizeof(Config));
	}
	memcpy(ppStream, &pOutput, sizeof(pOutput));
	if ( !xrtHttpBodyStreamConfigValid(&Config) ) {
		__xrtHttpBodyStreamSetError(
			XERR_VALUE,
			XHTTP_BODY_STREAM_ERROR_CONFIG,
			"create",
			"HTTP body stream budgets must be non-zero",
			NULL
		);
		return NULL;
	}
	pStream = (xhttpbodystream*)xrtCalloc(
		1, sizeof(*pStream)
	);
	if ( pStream == NULL ) {
		return NULL;
	}
	if ( !xrtMutexInit(&pStream->Lock) ) {
		xrtFree(pStream);
		return NULL;
	}
	pStream->References = 2;
	pStream->Producers = 1;
	pStream->Config = Config;
	pBody = xrtHttpBodyCreate(
		&Ops,
		pStream,
		XHTTP_BODY_UNKNOWN,
		XHTTP_BODY_NONE
	);
	if ( pBody == NULL ) {
		pStream->References = 1;
		__xrtHttpBodyStreamRelease(pStream);
		return NULL;
	}
	memcpy(ppStream, &pStream, sizeof(pStream));
	return pBody;
}



/* 增加一个可独立关闭的生产端引用。 */
XRT_API xhttpbodystream* xrtHttpBodyStreamRef(
	xhttpbodystream* pStream
)
{
	if ( __xrtHttpBodyStreamRef(pStream) == NULL ) {
		__xrtHttpBodyStreamSetError(
			pStream == NULL ? XERR_ARGUMENT : XERR_STATE,
			pStream == NULL ?
				XHTTP_BODY_STREAM_ERROR_ARGUMENT :
				XHTTP_BODY_STREAM_ERROR_STATE,
			"retain",
			"HTTP body stream is null or already released",
			NULL
		);
		return NULL;
	}
	if ( xrtRefRetain(&pStream->Producers) < 0 ) {
		__xrtHttpBodyStreamRelease(pStream);
		__xrtHttpBodyStreamSetError(
			XERR_STATE,
			XHTTP_BODY_STREAM_ERROR_STATE,
			"retain",
			"HTTP body stream has no remaining producer",
			NULL
		);
		return NULL;
	}
	return pStream;
}



/* 最后一个生产端释放时自然结束输入。 */
XRT_API void xrtHttpBodyStreamDestroy(
	xhttpbodystream* pStream
)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pStream->Producers) == 0 ) {
		(void)__xrtHttpBodyStreamCloseInput(
			pStream, false
		);
	}
	__xrtHttpBodyStreamRelease(pStream);
}



/* 复制并提交一个非空 Chunk。 */
XRT_API xhttpbodystreamresult xrtHttpBodyStreamWrite(
	xhttpbodystream* pStream,
	xbytesview Data
)
{
	if ( !__xrtHttpBodyStreamDataValid(pStream, Data) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"write",
			"HTTP body stream data range is invalid",
			NULL
		);
		return XHTTP_BODY_STREAM_ERROR;
	}
	return __xrtHttpBodyStreamBuild(
		pStream,
		Data.Size,
		__xrtHttpBodyStreamCopyFill,
		(ptr)Data.Data
	);
}



/* 提交由调用方释放过程管理的 Chunk 租约。 */
XRT_API xhttpbodystreamresult xrtHttpBodyStreamWriteRef(
	xhttpbodystream* pStream,
	xbytesview Data,
	xhttpbodyreleaseproc pRelease,
	ptr pContext
)
{
	if ( !__xrtHttpBodyStreamDataValid(pStream, Data) ||
		( pRelease == NULL ) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"write-ref",
			"HTTP body stream reference range or release callback is invalid",
			NULL
		);
		return XHTTP_BODY_STREAM_ERROR;
	}
	return __xrtHttpBodyStreamWriteOwned(
		pStream, Data, pRelease, pContext
	);
}



/* 提交由 xrtMalloc 分配并在消费后自动释放的 Chunk。 */
XRT_API xhttpbodystreamresult xrtHttpBodyStreamWriteTake(
	xhttpbodystream* pStream,
	ptr pData,
	size_t iSize
)
{
	if ( !__xrtHttpBodyStreamDataValid(
		pStream, (xbytesview){ (cbytes)pData, iSize }
	) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"write-take",
			"HTTP body stream take range is invalid",
			NULL
		);
		return XHTTP_BODY_STREAM_ERROR;
	}
	return __xrtHttpBodyStreamWriteOwned(
		pStream,
		(xbytesview){ (cbytes)pData, iSize },
		__xrtHttpBodyStreamFreeData,
		NULL
	);
}



/* 创建或复用当前可写代际，并在已有容量时立即完成。 */
XRT_API xfuture* xrtHttpBodyStreamWaitWritable(
	xhttpbodystream* pStream
)
{
	if ( pStream == NULL ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"wait-writable",
			"HTTP body stream is null",
			NULL
		);
		return NULL;
	}
	return __xrtHttpBodyStreamWaitGeneration(pStream, false);
}



/* 幂等发布正常 EOF。 */
XRT_API bool xrtHttpBodyStreamClose(
	xhttpbodystream* pStream
)
{
	if ( pStream == NULL ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"close",
			"HTTP body stream is null",
			NULL
		);
		return false;
	}
	return __xrtHttpBodyStreamCloseInput(pStream, true);
}



/* 发布永久生产失败并丢弃未交付节点。 */
XRT_API bool xrtHttpBodyStreamFail(
	xhttpbodystream* pStream,
	const xerror* pError
)
{
	__xrt_http_body_stream_signal Read = { 0 };
	__xrt_http_body_stream_signal Write = { 0 };
	__xrt_http_body_stream_node* pNodes;
	xerror* pFailure;
	xerror* pExisting = NULL;
	bool bEligible;
	bool bResult = false;

	if ( ( pStream == NULL ) || ( pError == NULL ) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"fail",
			"HTTP body stream and failure error are required",
			NULL
		);
		return false;
	}
	(void)xrtMutexLock(&pStream->Lock);
	if ( pStream->Failed ) {
		pExisting = xrtErrorRef(pStream->Error);
		bEligible = false;
	} else {
		bEligible = !pStream->InputClosed &&
			!pStream->ConsumerClosed;
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	if ( !bEligible ) {
		if ( pExisting != NULL ) {
			__xrtHttpBodyStreamPublishError(pExisting);
		} else {
			__xrtHttpBodyStreamSetError(
				XERR_STATE,
				XHTTP_BODY_STREAM_ERROR_STATE,
				"fail",
				"HTTP body stream is already closed",
				NULL
			);
		}
		return false;
	}
	pFailure = __xrtHttpBodyStreamErrorCreate(
		xrtErrorKind(pError),
		XHTTP_BODY_STREAM_ERROR_FAILED,
		"produce",
		"HTTP body stream producer failed",
		pError
	);
	if ( pFailure == NULL ) {
		return false;
	}
	(void)xrtMutexLock(&pStream->Lock);
	if ( !pStream->Failed && !pStream->InputClosed &&
		!pStream->ConsumerClosed ) {
		pStream->Failed = true;
		pStream->Error = pFailure;
		pFailure = NULL;
		pNodes = __xrtHttpBodyStreamDiscardLocked(pStream);
		__xrtHttpBodyStreamReadSignalTake(
			pStream,
			&Read,
			XRT_HTTP_BODY_STREAM_SIGNAL_REJECT
		);
		__xrtHttpBodyStreamWriteSignalTake(
			pStream,
			&Write,
			XRT_HTTP_BODY_STREAM_SIGNAL_REJECT
		);
		bResult = true;
	} else {
		pNodes = NULL;
		if ( pStream->Failed ) {
			pExisting = xrtErrorRef(pStream->Error);
		}
	}
	(void)xrtMutexUnlock(&pStream->Lock);
	__xrtHttpBodyStreamNodesDestroy(pNodes);
	__xrtHttpBodyStreamSignal(&Read);
	__xrtHttpBodyStreamSignal(&Write);
	if ( !bResult ) {
		if ( pExisting != NULL ) {
			__xrtHttpBodyStreamPublishError(pExisting);
			pExisting = NULL;
		} else {
			__xrtHttpBodyStreamSetError(
				XERR_STATE,
				XHTTP_BODY_STREAM_ERROR_STATE,
				"fail",
				"HTTP body stream is already closed",
				NULL
			);
		}
	}
	xrtErrorFree(pExisting);
	xrtErrorFree(pFailure);
	return bResult;
}



/* 复制不暴露节点和等待对象的并发快照。 */
XRT_API bool xrtHttpBodyStreamInfo(
	const xhttpbodystream* pStream,
	xhttpbodystreaminfo* pInfo
)
{
	xhttpbodystream* pMutable = (xhttpbodystream*)pStream;
	xhttpbodystreaminfo Info;

	if ( ( pStream == NULL ) ||
		!xrtMemRangeValid(pInfo, sizeof(Info)) ||
		xrtMemRangesOverlap(
			pInfo,
			sizeof(Info),
			pStream,
			sizeof(*pStream)
		) ) {
		__xrtHttpBodyStreamSetError(
			XERR_ARGUMENT,
			XHTTP_BODY_STREAM_ERROR_ARGUMENT,
			"query-info",
			"HTTP body stream or disjoint info output is invalid",
			NULL
		);
		return false;
	}
	memset(&Info, 0, sizeof(Info));
	(void)xrtMutexLock(&pMutable->Lock);
	Info.PendingBytes = pStream->PendingBytes;
	Info.PendingChunks = pStream->PendingChunks;
	Info.WrittenBytes = pStream->WrittenBytes;
	Info.ReadBytes = pStream->ReadBytes;
	Info.Opened = pStream->Opened;
	Info.InputClosed = pStream->InputClosed;
	Info.ConsumerClosed = pStream->ConsumerClosed;
	Info.Failed = pStream->Failed;
	(void)xrtMutexUnlock(&pMutable->Lock);
	memcpy(pInfo, &Info, sizeof(Info));
	return true;
}

#endif
