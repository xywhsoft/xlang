#include "../internal/xrt_websocket.h"



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)

/* 设置 Writer 的可恢复 Connection 域错误。 */
static void __xrtWsWriterReject(
	xwswriter* pWriter,
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		(pWriter->Connection == NULL) ) {
		__xwsErrorSetInvalidArgument();
		return;
	}
	(void)__xrtWsConnReject(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* Writer 释放独占权后唤醒排队的异步完整消息。 */
static void __xrtWsWriterNotify(xwsconn* pConnection)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		__xrtWsConnFutureNotify(pConnection);
	#else
		(void)pConnection;
	#endif
}



/* 清除当前 Writer；重复清理不会影响后来创建的 Writer。 */
static void __xrtWsWriterUnlock(xwswriter* pWriter)
{
	xwsconn* pConnection = pWriter->Connection;

	if ( (pConnection != NULL) &&
		(pConnection->Writer == pWriter) ) {
		pConnection->Writer = NULL;
		__xrtWsWriterNotify(pConnection);
	}
}



/* 释放嵌入式 Writer；已经开始的半条线路消息必须终止会话。 */
static void __xrtWsWriterDispose(xwswriter* pWriter)
{
	xwsconn* pConnection = pWriter->Connection;

	pWriter->DestroyRequested = true;
	if ( !pWriter->Finished && pWriter->Started &&
		(pConnection != NULL) &&
		(xrtWsConnState(pConnection) == XWS_CONN_OPEN) ) {
		xnetresult Result = xrtWsConnClose(
			pConnection,
			XWS_CLOSE_INTERNAL,
			XRT_STR_LITERAL("message writer abandoned")
		);

		if ( Result != XNET_RESULT_OK ) {
			(void)xrtWsConnAbort(pConnection);
		}
	}
	__xrtWsWriterUnlock(pWriter);
	pWriter->Connection = NULL;
	xrtWsConnDestroy(pConnection);
}



/*
	在不修改原状态的副本上检查累计上限和 UTF-8。
	调用方只有在线路帧受理后才提交副本。
*/
static bool __xrtWsWriterPrepare(
	xwswriter* pWriter,
	xbytesview Data,
	bool bFinal,
	xutf8state* pUtf8,
	size_t* pSize
)
{
	xwsconn* pConnection = pWriter->Connection;

	if ( !xrtMemRangeValid(Data.Data, Data.Size) ||
		xrtMemRangesOverlap(
			Data.Data, Data.Size, pWriter, sizeof(*pWriter)
		) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"write-websocket-message",
			"WebSocket Writer data view is invalid",
			NULL
		);
		return false;
	}
	if ( (pWriter->Size > pConnection->Config.MessageLimit) ||
		(Data.Size >
		 (pConnection->Config.MessageLimit - pWriter->Size)) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"write-websocket-message",
			"WebSocket Writer exceeded the message limit",
			NULL
		);
		return false;
	}
	*pSize = pWriter->Size + Data.Size;
	*pUtf8 = pWriter->Utf8;
	if ( pWriter->Opcode == XWS_OPCODE_TEXT ) {
		xutfstatus Status = xrtUtf8StateFeed(
			pUtf8,
			(xstrview) {
				(const char*)Data.Data,
				Data.Size
			},
			bFinal
		);

		if ( (Status == XUTF_INVALID) ||
			(Status == XUTF_OVERFLOW) ||
			(bFinal && (Status != XUTF_OK)) ) {
			__xrtWsWriterReject(
				pWriter,
				Status == XUTF_OVERFLOW ?
					XERR_RANGE : XERR_VALUE,
				Status == XUTF_OVERFLOW ?
					XWS_CONN_ERROR_LIMIT :
					XWS_CONN_ERROR_MESSAGE,
				"write-websocket-message",
				Status == XUTF_OVERFLOW ?
					"WebSocket Writer UTF-8 size overflowed" :
					"WebSocket Writer text is not valid UTF-8",
				Status == XUTF_OVERFLOW ?
					xrtGetError() : NULL
			);
			return false;
		}
	}
	return true;
}



/* 受理成功后一次提交 Writer 的逻辑消息状态。 */
static void __xrtWsWriterCommit(
	xwswriter* pWriter,
	const xutf8state* pUtf8,
	size_t iSize,
	bool bFinal
)
{
	pWriter->Utf8 = *pUtf8;
	pWriter->Size = iSize;
	pWriter->Started = true;
	if ( bFinal ) {
		pWriter->Finished = true;
		__xrtWsWriterUnlock(pWriter);
	}
}



/* 统一复制与所有权分片的事务发送路径。 */
static xnetresult __xrtWsWriterSend(
	xwswriter* pWriter,
	xbytesview Data,
	const xnetref* pRef,
	bool bFinal
)
{
	xwsconn* pConnection;
	xutf8state Utf8;
	size_t iSize;
	xnetresult Result;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xwsErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	pConnection = pWriter->Connection;
	if ( !__xrtWsConnWorker(
		pConnection,
		"write-websocket-message"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pWriter->Finished || pWriter->DestroyRequested ||
		pWriter->Calling ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"write-websocket-message",
			"WebSocket Writer is not writable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	if ( !__xrtWsWriterPrepare(
		pWriter,
		Data,
		bFinal,
		&Utf8,
		&iSize
	) ) {
		return XNET_RESULT_ERROR;
	}

	pWriter->Calling = true;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		if ( pWriter->Compressed ) {
			Result = __xrtWsConnSendDeflatePart(
				pConnection,
				pWriter->Started ?
					XWS_OPCODE_CONTINUATION :
					pWriter->Opcode,
				Data,
				!pWriter->Started,
				bFinal
			);
		} else
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
		if ( pRef != NULL ) {
			Result = __xrtWsConnSendRefFrame(
				pConnection,
				pWriter->Started ?
					XWS_OPCODE_CONTINUATION :
					pWriter->Opcode,
				*pRef,
				bFinal
			);
		} else
	#endif
	{
		(void)pRef;
		Result = __xrtWsConnSendFrame(
			pConnection,
			pWriter->Started ?
				XWS_OPCODE_CONTINUATION :
				pWriter->Opcode,
			Data,
			bFinal,
			__XRT_WS_SEND_DATA,
			false
		);
	}
	if ( Result == XNET_RESULT_OK ) {
		#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE) && \
			defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
			if ( pWriter->Compressed && (pRef != NULL) ) {
				pRef->Release(
					pRef->Context,
					pRef->Data,
					pRef->Size
				);
			}
		#endif
		__xrtWsWriterCommit(
			pWriter,
			&Utf8,
			iSize,
			bFinal
		);
	}
	pWriter->Calling = false;
	if ( pWriter->DestroyRequested ) {
		__xrtWsWriterDispose(pWriter);
	}
	return Result;
}



/* 复用 Connection 内嵌状态并独占一条普通或压缩出站数据消息。 */
xwswriter* __xrtWsWriterCreate(
	xwsconn* pConnection,
	xwsopcode Opcode,
	bool bCompressed
)
{
	xwswriter* pWriter;

	if ( !__xrtWsConnWorker(
		pConnection,
		"begin-websocket-message"
	) ) {
		return NULL;
	}
	if ( (Opcode != XWS_OPCODE_TEXT) &&
		(Opcode != XWS_OPCODE_BINARY) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"begin-websocket-message",
			"WebSocket Writer opcode must be Text or Binary",
			NULL
		);
		return NULL;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		(void)__xrtWsConnReject(
			XERR_CLOSED,
			XWS_CONN_ERROR_STATE,
			"begin-websocket-message",
			"WebSocket connection is not open",
			NULL
		);
		return NULL;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		if ( bCompressed &&
			!pConnection->Config.DeflateEnabled ) {
			(void)__xrtWsConnReject(
				XERR_STATE,
				XWS_CONN_ERROR_CONFIG,
				"begin-compressed-websocket-message",
				"WebSocket connection did not negotiate compression",
				NULL
			);
			return NULL;
		}
	#else
		(void)bCompressed;
	#endif
	if ( pConnection->Writer != NULL ) {
		(void)__xrtWsConnReject(
			XERR_AGAIN,
			XWS_CONN_ERROR_STATE,
			"begin-websocket-message",
			"WebSocket connection already has an active Writer",
			NULL
		);
		return NULL;
	}
	pWriter = &pConnection->WriterStorage;
	memset(pWriter, 0, sizeof(*pWriter));
	pWriter->Connection = xrtWsConnRef(pConnection);
	if ( pWriter->Connection == NULL ) {
		(void)__xrtWsConnReject(
			XERR_CLOSED,
			XWS_CONN_ERROR_STATE,
			"begin-websocket-message",
			"WebSocket connection reference is closed",
			NULL
		);
		return NULL;
	}
	pWriter->Opcode = Opcode;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		pWriter->Compressed = bCompressed;
	#endif
	xrtUtf8StateInit(&pWriter->Utf8);
	pConnection->Writer = pWriter;
	return pWriter;
}



/* 创建并独占 Connection 的一条未压缩出站数据消息。 */
XRT_API xwswriter* xrtWsConnBegin(
	xwsconn* pConnection,
	xwsopcode Opcode
)
{
	return __xrtWsWriterCreate(
		pConnection,
		Opcode,
		false
	);
}



/* 开始一条分片 Text 消息。 */
XRT_API xwswriter* xrtWsConnBeginText(
	xwsconn* pConnection
)
{
	return xrtWsConnBegin(
		pConnection,
		XWS_OPCODE_TEXT
	);
}



/* 开始一条分片 Binary 消息。 */
XRT_API xwswriter* xrtWsConnBeginBinary(
	xwsconn* pConnection
)
{
	return xrtWsConnBegin(
		pConnection,
		XWS_OPCODE_BINARY
	);
}



/* 复制并提交一个非最终分片。 */
XRT_API xnetresult xrtWsWriterWrite(
	xwswriter* pWriter,
	xbytesview Data
)
{
	return __xrtWsWriterSend(
		pWriter,
		Data,
		NULL,
		false
	);
}



/* 复制并提交最终分片。 */
XRT_API xnetresult xrtWsWriterFinish(
	xwswriter* pWriter,
	xbytesview Data
)
{
	return __xrtWsWriterSend(
		pWriter,
		Data,
		NULL,
		true
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
/* 验证 Ref 后提交一个所有权分片。 */
static xnetresult __xrtWsWriterSendRef(
	xwswriter* pWriter,
	const xnetref* pRef,
	bool bFinal
)
{
	xnetref Ref;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pRef, sizeof(Ref)) ||
		xrtMemRangesOverlap(
			pRef, sizeof(Ref), pWriter, sizeof(*pWriter)
		) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"write-websocket-message-ref",
			"WebSocket Writer reference range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( !xrtMemRangeValid(Ref.Data, Ref.Size) ||
		(Ref.Size == 0) || (Ref.Release == NULL) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"write-websocket-message-ref",
			"WebSocket Writer reference is incomplete or empty",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtWsWriterSend(
		pWriter,
		(xbytesview) {
			Ref.Data,
			Ref.Size
		},
		&Ref,
		bFinal
	);
}



/* 提交一个非最终所有权分片。 */
XRT_API xnetresult xrtWsWriterWriteRef(
	xwswriter* pWriter,
	const xnetref* pRef
)
{
	return __xrtWsWriterSendRef(
		pWriter,
		pRef,
		false
	);
}



/* 提交最终所有权分片。 */
XRT_API xnetresult xrtWsWriterFinishRef(
	xwswriter* pWriter,
	const xnetref* pRef
)
{
	return __xrtWsWriterSendRef(
		pWriter,
		pRef,
		true
	);
}



/* 提交并接管一个非空 XRT 内存分片。 */
XRT_API xnetresult xrtWsWriterWriteTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
)
{
	xnetref Ref = {
		(cbytes)pData,
		iSize,
		__xrtWsConnTakeRelease,
		NULL
	};

	return xrtWsWriterWriteRef(pWriter, &Ref);
}



/* 提交并接管一个非空 XRT 内存最终分片。 */
XRT_API xnetresult xrtWsWriterFinishTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
)
{
	xnetref Ref = {
		(cbytes)pData,
		iSize,
		__xrtWsConnTakeRelease,
		NULL
	};

	return xrtWsWriterFinishRef(pWriter, &Ref);
}
#endif



/* 返回最终分片是否已经受理。 */
XRT_API bool xrtWsWriterIsFinished(
	const xwswriter* pWriter
)
{
	if ( pWriter == NULL ) {
		return false;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xwsErrorSetInvalidArgument();
		return false;
	}
	return pWriter->Finished;
}



/* 销毁 Writer；发送回调重入销毁时延迟到当前事务结束。 */
XRT_API void xrtWsWriterDestroy(
	xwswriter* pWriter
)
{
	if ( pWriter == NULL ) {
		return;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xwsErrorSetInvalidArgument();
		return;
	}
	if ( !__xrtWsConnWorker(
		pWriter->Connection,
		"destroy-websocket-writer"
	) ) {
		return;
	}
	if ( pWriter->DestroyRequested ) {
		return;
	}
	pWriter->DestroyRequested = true;
	if ( !pWriter->Calling ) {
		__xrtWsWriterDispose(pWriter);
	}
}

#endif
