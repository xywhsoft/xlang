/*
	WebSocket 事件直接传递标量和借用数据视图，不为每个数据块构造语言对象。
	0/1/2 为消息 Begin/Data/End，3/4 为 Ping/Pong，5/6/7 为背压/Writable/Drain，
	8/9 为 Error/Close。
*/
static const char xl6WebSocketStreamSource[] = XL6_WEBSOCKET_SOURCE(
enum {
	XL6_WS_EVENT_MESSAGE_BEGIN = 0,
	XL6_WS_EVENT_MESSAGE_DATA,
	XL6_WS_EVENT_MESSAGE_END,
	XL6_WS_EVENT_PING,
	XL6_WS_EVENT_PONG,
	XL6_WS_EVENT_BACKPRESSURE,
	XL6_WS_EVENT_WRITABLE,
	XL6_WS_EVENT_DRAIN,
	XL6_WS_EVENT_ERROR,
	XL6_WS_EVENT_CLOSE
};



typedef struct xl6_ws_connection {
	xatomic32 References;
	xatomic32 CallbackHeld;
	xatomic32 MessageOpcode;
	xatomic32 MessageFlags;
	xatomicptr Stream;
	xatomicptr Owner;
	XL6_Function Handler;
} xl6_ws_connection;



typedef xwsstream* (*xl6_ws_attach_proc)(
	void* pTransport,
	size_t iPrefix,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
);



typedef void (*xl6_ws_transport_release_proc)(void* pTransport);



typedef struct xl6_ws_attach_command {
	xevent Done;
	xnetpost Post;
	xl6_ws_attach_proc Attach;
	void* Transport;
	size_t Prefix;
	xwsstreamconfig Config;
	const xwsstreamevents* Events;
	xl6_ws_connection* Connection;
	xwsstream* Result;
	bool Signal;
} xl6_ws_attach_command;



typedef enum xl6_ws_command_kind {
	XL6_WS_COMMAND_SEND = 1,
	XL6_WS_COMMAND_SEND_MOVE,
	XL6_WS_COMMAND_PING,
	XL6_WS_COMMAND_PONG,
	XL6_WS_COMMAND_CLOSE,
	XL6_WS_COMMAND_PAUSE,
	XL6_WS_COMMAND_RESUME
} xl6_ws_command_kind;



typedef struct xl6_ws_command {
	xevent Done;
	xnetpost Post;
	xwsstream* Stream;
	xl6_ws_command_kind Kind;
	xwsopcode Opcode;
	xbytesview Data;
	xbuffer* Buffer;
	xstrview Reason;
	uint16 Code;
	xnetresult Result;
	bool Compressed;
	bool Boolean;
	bool Signal;
} xl6_ws_command;



/* 最后一份引用负责释放回调、传输语言对象和桥接存储。 */
static void xl6_ws_connection_release(xl6_ws_connection* pConnection)
{
	xvalue* pOwner;

	if ( (pConnection == 0) ||
		 (xrtAtomic32FetchAdd(&pConnection->References, UINT32_MAX, XMEMORY_ACQ_REL) != 1u) ) {
		return;
	}
	pOwner = (xvalue*)xrtAtomicPtrExchange(&pConnection->Owner, 0, XMEMORY_ACQ_REL);
	xrtValueRelease(pOwner);
	xl6_function_release_value(pConnection->Handler);
	memset(pConnection, 0, sizeof(*pConnection));
	xrtFree(pConnection);
}



/* 直接调用常规 xlang 函数；动态 callable 才回退到 xvalue 参数路径。 */
static void xl6_ws_emit(
	xl6_ws_connection* pConnection,
	long long iEvent,
	long long iOpcode,
	long long iFlags,
	uint64_t iValue,
	void* pData,
	size_t iSize
)
{
	XL6_Function Handler;
	xvalue* pResult;
	xwsstream* pStream;

	if ( pConnection == 0 ) {
		return;
	}
	Handler = pConnection->Handler;
	if ( Handler.invoke != 0 ) {
		((void (*)(void*, long long, long long, long long, uint64_t, void*, size_t))Handler.invoke)(
			Handler.invoke_env,
			iEvent,
			iOpcode,
			iFlags,
			iValue,
			pData,
			iSize
		);
	} else if ( Handler.invoke_var != 0 ) {
		pResult = xl6_function_call_var(
			Handler,
			6,
			xrtValueInt(iEvent),
			xrtValueInt(iOpcode),
			xrtValueInt(iFlags),
			xrtValueUInt(iValue),
			xrtValuePointer(pData),
			xrtValueUInt((uint64)iSize)
		);
		xrtValueRelease(pResult);
	}
	if ( xl6_error_has() ) {
		xl6_error_publish_xrt();
		pStream = (xwsstream*)xrtAtomicPtrLoad(&pConnection->Stream, XMEMORY_ACQUIRE);
		if ( pStream != 0 ) {
			(void)xrtWsStreamAbort(pStream);
		}
	}
}



/* 发布消息开始，并保存后续 Data/End 所需的语义。 */
static void xl6_ws_message_begin(
	xwsstream* pStream,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	xl6_ws_connection* pConnection = (xl6_ws_connection*)pData;

	(void)pStream;
	if ( pInfo == 0 ) {
		return;
	}
	xrtAtomic32Store(&pConnection->MessageOpcode, pInfo->Opcode, XMEMORY_RELEASE);
	xrtAtomic32Store(&pConnection->MessageFlags, pInfo->Flags, XMEMORY_RELEASE);
	xl6_ws_emit(
		pConnection,
		XL6_WS_EVENT_MESSAGE_BEGIN,
		pInfo->Opcode,
		pInfo->Flags,
		pInfo->PayloadSize,
		(void*)pInfo,
		sizeof(*pInfo)
	);
}



/* 数据视图只在本次回调期间有效。 */
static void xl6_ws_message_data(
	xwsstream* pStream,
	xbytesview Data,
	ptr pData
)
{
	xl6_ws_connection* pConnection = (xl6_ws_connection*)pData;

	(void)pStream;
	xl6_ws_emit(
		pConnection,
		XL6_WS_EVENT_MESSAGE_DATA,
		xrtAtomic32Load(&pConnection->MessageOpcode, XMEMORY_ACQUIRE),
		xrtAtomic32Load(&pConnection->MessageFlags, XMEMORY_ACQUIRE),
		0u,
		(void*)Data.Data,
		Data.Size
	);
}



/* 发布当前消息结束边界。 */
static void xl6_ws_message_end(xwsstream* pStream, ptr pData)
{
	xl6_ws_connection* pConnection = (xl6_ws_connection*)pData;

	(void)pStream;
	xl6_ws_emit(
		pConnection,
		XL6_WS_EVENT_MESSAGE_END,
		xrtAtomic32Load(&pConnection->MessageOpcode, XMEMORY_ACQUIRE),
		xrtAtomic32Load(&pConnection->MessageFlags, XMEMORY_ACQUIRE),
		0u,
		0,
		0u
	);
}



/* 发布 Ping 控制帧。 */
static void xl6_ws_ping(xwsstream* pStream, xbytesview Payload, ptr pData)
{
	(void)pStream;
	xl6_ws_emit((xl6_ws_connection*)pData, XL6_WS_EVENT_PING, XWS_OPCODE_PING, 0, 0u, (void*)Payload.Data, Payload.Size);
}



/* 发布 Pong 控制帧。 */
static void xl6_ws_pong(xwsstream* pStream, xbytesview Payload, ptr pData)
{
	(void)pStream;
	xl6_ws_emit((xl6_ws_connection*)pData, XL6_WS_EVENT_PONG, XWS_OPCODE_PONG, 0, 0u, (void*)Payload.Data, Payload.Size);
}



/* 发布首次背压边沿。 */
static void xl6_ws_backpressure(xwsstream* pStream, size_t iPending, ptr pData)
{
	(void)pStream;
	xl6_ws_emit((xl6_ws_connection*)pData, XL6_WS_EVENT_BACKPRESSURE, 0, 0, iPending, 0, 0u);
}



/* 发布恢复可写边沿。 */
static void xl6_ws_writable(xwsstream* pStream, size_t iPending, ptr pData)
{
	(void)pStream;
	xl6_ws_emit((xl6_ws_connection*)pData, XL6_WS_EVENT_WRITABLE, 0, 0, iPending, 0, 0u);
}



/* 发布发送队列排空事件。 */
static void xl6_ws_drain(xwsstream* pStream, ptr pData)
{
	(void)pStream;
	xl6_ws_emit((xl6_ws_connection*)pData, XL6_WS_EVENT_DRAIN, 0, 0, 0u, 0, 0u);
}



/* 错误对象是 Stream 持有的借用视图。 */
static void xl6_ws_error(
	xwsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	(void)pStream;
	xl6_ws_emit(
		(xl6_ws_connection*)pData,
		XL6_WS_EVENT_ERROR,
		0,
		pError != 0 ? xrtErrorKind(pError) : 0,
		pError != 0 ? (uint64_t)(uint32_t)xrtErrorCode(pError) : 0u,
		(void*)pError,
		0u
	);
}



/* Close 是终态事件，发布后释放传输对象与回调保活引用。 */
static void xl6_ws_stream_on_close(
	xwsstream* pStream,
	const xwsstreamclose* pClose,
	ptr pData
)
{
	xl6_ws_connection* pConnection = (xl6_ws_connection*)pData;
	xvalue* pOwner;
	uint64_t iCodes = 0u;

	(void)pStream;
	if ( pClose != 0 ) {
		iCodes = ((uint64_t)pClose->LocalCode << 16u) | (uint64_t)pClose->RemoteCode;
		xl6_ws_emit(
			pConnection,
			XL6_WS_EVENT_CLOSE,
			0,
			pClose->Flags,
			iCodes,
			(void*)pClose->Reason.Data,
			pClose->Reason.Size
		);
	}
	pOwner = (xvalue*)xrtAtomicPtrExchange(&pConnection->Owner, 0, XMEMORY_ACQ_REL);
	xrtValueRelease(pOwner);
	if ( xrtAtomic32Exchange(&pConnection->CallbackHeld, 0u, XMEMORY_ACQ_REL) != 0u ) {
		xl6_ws_connection_release(pConnection);
	}
}



static const xwsstreamevents xl6_ws_events = {
	xl6_ws_message_begin,
	xl6_ws_message_data,
	xl6_ws_message_end,
	xl6_ws_ping,
	xl6_ws_pong,
	xl6_ws_backpressure,
	xl6_ws_writable,
	xl6_ws_drain,
	xl6_ws_error,
	xl6_ws_stream_on_close
};



/* TCP Attach 适配到与 TLS 共用的无类型命令入口。 */
static xwsstream* xl6_ws_attach_tcp_proc(
	void* pTransport,
	size_t iPrefix,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
)
{
	return xrtWsStreamAttach((xnetstream*)pTransport, iPrefix, pConfig, pEvents, pData);
}



/* 释放未被 Attach 接管的 TCP 引用。 */
static void xl6_ws_tcp_release(void* pTransport)
{
	xrtNetStreamDestroy((xnetstream*)pTransport);
}



/* 在目标 Worker 执行 Attach。 */
static void xl6_ws_attach_run(xnetworker* pWorker, ptr pData)
{
	xl6_ws_attach_command* pCommand = (xl6_ws_attach_command*)pData;

	(void)pWorker;
	pCommand->Result = pCommand->Attach(
		pCommand->Transport,
		pCommand->Prefix,
		&pCommand->Config,
		pCommand->Events,
		pCommand->Connection
	);
	if ( pCommand->Signal ) {
		(void)xrtEventSet(&pCommand->Done);
	}
}



/* 无分配地把 Attach 编组到传输 Worker，并同步取得接管结果。 */
static bool xl6_ws_attach_execute(
	xnetworker* pWorker,
	xl6_ws_attach_command* pCommand
)
{
	bool bPosted;
	bool bWaited;

	if ( (pWorker == 0) || (pCommand == 0) ) {
		return false;
	}
	if ( xrtNetWorkerIsCurrent(pWorker) ) {
		pCommand->Signal = false;
		xl6_ws_attach_run(pWorker, pCommand);
		return true;
	}
	if ( !xrtEventInit(&pCommand->Done, false, false) ) {
		return false;
	}
	if ( !xrtNetPostInit(&pCommand->Post) ) {
		(void)xrtEventUnit(&pCommand->Done);
		return false;
	}
	pCommand->Signal = true;
	bPosted = xrtNetPost(pWorker, &pCommand->Post, xl6_ws_attach_run, pCommand);
	bWaited = bPosted && (xrtEventWait(&pCommand->Done) == XWAIT_OK);
	/* Done belongs to the caller's command; Unit releases only event resources. */
	(void)xrtEventUnit(&pCommand->Done);
	return bWaited;
}



/* 从 Upgrade 快照完成 Stream 配置，调用方配置只覆盖资源策略。 */
static bool xl6_ws_attach_config(
	xl6_ws_upgrade* pUpgrade,
	xl6_ws_stream_config* pInput,
	xwsstreamconfig* pOutput
)
{
	if ( (pUpgrade == 0) || !pUpgrade->Ready || (pOutput == 0) ) {
		return false;
	}
	if ( pInput != 0 ) {
		xrtWsStreamConfigInit(pOutput);
		pOutput->MessageLimit = pInput->MessageLimit;
		pOutput->FrameLimit = pInput->FrameLimit;
		pOutput->SendLimit = pInput->SendLimit;
		pOutput->ControlReserve = pInput->ControlReserve;
		pOutput->CloseTimeout = pInput->CloseTimeout;
		pOutput->AutoPong = pInput->AutoPong;
	} else {
		xrtWsStreamConfigInit(pOutput);
	}
	pOutput->Role = (xwsrole)pUpgrade->Role;
	pOutput->Protocol = pUpgrade->Value.Protocol;
	pOutput->DeflateEnabled = pUpgrade->Value.DeflateEnabled;
	if ( pUpgrade->Value.DeflateEnabled ) {
		pOutput->Deflate = pUpgrade->Value.Deflate;
	}
	return xrtWsStreamConfigValid(pOutput);
}



/* 建立带两份生命周期引用的语言桥接对象。 */
static xl6_ws_connection* xl6_ws_connection_create(
	xvalue* pOwner,
	XL6_Function Handler
)
{
	xl6_ws_connection* pConnection;
	xvalue* pRetainedOwner;

	if ( (pOwner == 0) ||
		 ((Handler.invoke == 0) && (Handler.invoke_var == 0) && (Handler.invoke_xcall == 0)) ) {
		return 0;
	}
	pConnection = (xl6_ws_connection*)xrtCalloc(1u, sizeof(*pConnection));
	if ( pConnection == 0 ) {
		return 0;
	}
	pRetainedOwner = xrtValueRetain(pOwner);
	if ( pRetainedOwner == 0 ) {
		xrtFree(pConnection);
		return 0;
	}
	xrtAtomic32Store(&pConnection->References, 2u, XMEMORY_RELAXED);
	xrtAtomic32Store(&pConnection->CallbackHeld, 1u, XMEMORY_RELAXED);
	xrtAtomicPtrStore(&pConnection->Owner, pRetainedOwner, XMEMORY_RELEASE);
	pConnection->Handler = xl6_function_copy_value(Handler);
	return pConnection;
}



/* TCP 与 TLS 共用引用、配置、Worker 编组和失败回滚。 */
static void* xl6_ws_attach_common(
	xvalue* pOwner,
	void* pUpgradeHandle,
	void* pConfigHandle,
	XL6_Function Handler,
	size_t iPrefix,
	void* pTransport,
	xnetworker* pWorker,
	xl6_ws_attach_proc pAttach,
	xl6_ws_transport_release_proc pRelease
)
{
	xl6_ws_connection* pConnection;
	xl6_ws_attach_command Command;

	if ( (pTransport == 0) || (pWorker == 0) || (pAttach == 0) || (pRelease == 0) ) {
		if ( pRelease != 0 ) {
			pRelease(pTransport);
		}
		return 0;
	}
	pConnection = xl6_ws_connection_create(pOwner, Handler);
	if ( pConnection == 0 ) {
		pRelease(pTransport);
		return 0;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Attach = pAttach;
	Command.Transport = pTransport;
	Command.Prefix = iPrefix;
	Command.Events = &xl6_ws_events;
	Command.Connection = pConnection;
	if ( !xl6_ws_attach_config(
		(xl6_ws_upgrade*)pUpgradeHandle,
		(xl6_ws_stream_config*)pConfigHandle,
		&Command.Config
	) || !xl6_ws_attach_execute(pWorker, &Command) || (Command.Result == 0) ) {
		pRelease(pTransport);
		(void)xrtAtomic32Exchange(&pConnection->CallbackHeld, 0u, XMEMORY_ACQ_REL);
		xl6_ws_connection_release(pConnection);
		xl6_ws_connection_release(pConnection);
		return 0;
	}
	xrtAtomicPtrStore(&pConnection->Stream, Command.Result, XMEMORY_RELEASE);
	return pConnection;
}



/* 在 TCP Worker 上接管经过校验的 HTTP Upgrade。 */
void* xl6_xrt_ws_stream_attach_tcp(
	xvalue* pOwner,
	void* pUpgrade,
	void* pConfig,
	XL6_Function Handler,
	size_t iPrefix
)
{
	xnetstream* pSource = (xnetstream*)xl6ValueHandleData(pOwner);
	xnetstream* pTransport = pSource != 0 ? xrtNetStreamRef(pSource) : 0;
	xnetworker* pWorker = pTransport != 0 ? xrtNetStreamWorker(pTransport) : 0;

	return xl6_ws_attach_common(
		pOwner,
		pUpgrade,
		pConfig,
		Handler,
		iPrefix,
		pTransport,
		pWorker,
		xl6_ws_attach_tcp_proc,
		xl6_ws_tcp_release
	);
}



/* 在目标 Worker 执行一项不等待网络排空的有界操作。 */
static void xl6_ws_command_run(xnetworker* pWorker, ptr pData)
{
	xl6_ws_command* pCommand = (xl6_ws_command*)pData;

	(void)pWorker;
	switch ( pCommand->Kind ) {
		case XL6_WS_COMMAND_SEND:
			pCommand->Result = pCommand->Compressed ?
				xrtWsStreamSendCompressed(pCommand->Stream, pCommand->Opcode, pCommand->Data) :
				xrtWsStreamSend(pCommand->Stream, pCommand->Opcode, pCommand->Data);
			break;

		case XL6_WS_COMMAND_SEND_MOVE:
			pCommand->Result = xrtWsStreamSendTake(
				pCommand->Stream,
				pCommand->Opcode,
				pCommand->Buffer->Data,
				pCommand->Buffer->Size
			);
			if ( pCommand->Result == XNET_RESULT_OK ) {
				pCommand->Buffer->Data = 0;
				pCommand->Buffer->Size = 0u;
				pCommand->Buffer->Capacity = 0u;
			}
			break;

		case XL6_WS_COMMAND_PING:
			pCommand->Result = xrtWsStreamPing(pCommand->Stream, pCommand->Data);
			break;

		case XL6_WS_COMMAND_PONG:
			pCommand->Result = xrtWsStreamPong(pCommand->Stream, pCommand->Data);
			break;

		case XL6_WS_COMMAND_CLOSE:
			pCommand->Result = xrtWsStreamClose(pCommand->Stream, pCommand->Code, pCommand->Reason);
			break;

		case XL6_WS_COMMAND_PAUSE:
			xrtWsStreamPause(pCommand->Stream);
			pCommand->Boolean = true;
			break;

		case XL6_WS_COMMAND_RESUME:
			pCommand->Boolean = xrtWsStreamResume(pCommand->Stream);
			break;
	}
	if ( pCommand->Signal ) {
		(void)xrtEventSet(&pCommand->Done);
	}
}



/* 任意线程调用都被同步编组到 Stream Worker。 */
static bool xl6_ws_command_execute(
	xl6_ws_connection* pConnection,
	xl6_ws_command* pCommand
)
{
	xwsstream* pStream;
	xnetworker* pWorker;
	bool bPosted;
	bool bWaited;

	if ( (pConnection == 0) || (pCommand == 0) ) {
		return false;
	}
	pStream = xrtWsStreamRef((xwsstream*)xrtAtomicPtrLoad(&pConnection->Stream, XMEMORY_ACQUIRE));
	if ( pStream == 0 ) {
		return false;
	}
	pWorker = xrtWsStreamWorker(pStream);
	pCommand->Stream = pStream;
	if ( xrtNetWorkerIsCurrent(pWorker) ) {
		pCommand->Signal = false;
		xl6_ws_command_run(pWorker, pCommand);
		xrtWsStreamDestroy(pStream);
		return true;
	}
	if ( !xrtEventInit(&pCommand->Done, false, false) ) {
		xrtWsStreamDestroy(pStream);
		return false;
	}
	if ( !xrtNetPostInit(&pCommand->Post) ) {
		(void)xrtEventUnit(&pCommand->Done);
		xrtWsStreamDestroy(pStream);
		return false;
	}
	pCommand->Signal = true;
	bPosted = xrtNetPost(pWorker, &pCommand->Post, xl6_ws_command_run, pCommand);
	bWaited = bPosted && (xrtEventWait(&pCommand->Done) == XWAIT_OK);
	(void)xrtEventUnit(&pCommand->Done);
	xrtWsStreamDestroy(pStream);
	return bWaited;
}



void* xl6_xrt_ws_stream_wrap(void* pHandle) { return pHandle; }
void* xl6_xrt_ws_stream_native(void* pHandle) { xl6_ws_connection* pConnection = (xl6_ws_connection*)pHandle; return pConnection != 0 ? xrtAtomicPtrLoad(&pConnection->Stream, XMEMORY_ACQUIRE) : 0; }



/* 语言对象放弃引用时请求终止；终态回调仍由独立引用保护。 */
XL6_NATIVE_VOID_RESULT xl6_xrt_ws_stream_destroy(void* pHandle)
{
	xl6_ws_connection* pConnection = (xl6_ws_connection*)pHandle;
	xwsstream* pStream;

	if ( pConnection == 0 ) {
		XL6_NATIVE_VOID_RETURN();
	}
	pStream = (xwsstream*)xrtAtomicPtrExchange(&pConnection->Stream, 0, XMEMORY_ACQ_REL);
	if ( pStream != 0 ) {
		(void)xrtWsStreamAbort(pStream);
		xrtWsStreamDestroy(pStream);
	}
	xl6_ws_connection_release(pConnection);
	XL6_NATIVE_VOID_RETURN();
}



/* 并发只读状态查询。 */
long long xl6_xrt_ws_stream_state(void* pHandle) { xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle); return pStream != 0 ? xrtWsStreamState(pStream) : XWS_STREAM_CLOSED; }
long long xl6_xrt_ws_stream_role(void* pHandle) { xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle); return pStream != 0 ? xrtWsStreamRole(pStream) : -1; }
long long xl6_xrt_ws_stream_pending(void* pHandle) { xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle); return pStream != 0 ? (long long)xrtWsStreamPending(pStream) : 0; }
long long xl6_xrt_ws_stream_writable(void* pHandle) { xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle); return pStream != 0 ? (long long)xrtWsStreamWritable(pStream) : 0; }
int xl6_xrt_ws_stream_paused(void* pHandle) { xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle); return pStream != 0 && xrtWsStreamPaused(pStream); }
int xl6_xrt_ws_stream_deflate(void* pHandle) { xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle); xwsdeflate Deflate; return pStream != 0 && xrtWsStreamDeflate(pStream, &Deflate); }



/* 复制稳定子协议文本。 */
void* xl6_xrt_ws_stream_protocol(void* pHandle)
{
	xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle);
	xstrview Protocol = pStream != 0 ? xrtWsStreamProtocol(pStream) : (xstrview){ 0 };

	return xl6_xrt_string_dup(Protocol.Data != 0 ? Protocol.Data : "", Protocol.Size);
}



/* 复制或压缩发送一条完整消息，返回原始 xnetresult。 */
long long xl6_xrt_ws_stream_send(
	void* pHandle,
	long long iOpcode,
	void* pData,
	size_t iSize,
	int bCompressed
)
{
	xl6_ws_command Command;

	if ( ((iOpcode != XWS_OPCODE_TEXT) && (iOpcode != XWS_OPCODE_BINARY)) ||
		 ((pData == 0) && (iSize != 0u)) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = XL6_WS_COMMAND_SEND;
	Command.Opcode = (xwsopcode)iOpcode;
	Command.Data = (xbytesview){ (cbytes)pData, iSize };
	Command.Compressed = bCompressed != 0;
	Command.Result = XNET_RESULT_ERROR;
	return xl6_ws_command_execute((xl6_ws_connection*)pHandle, &Command) ?
		Command.Result : XNET_RESULT_ERROR;
}



/* 文本负载传递精确字节长度，内嵌零字节不在传输层截断。 */
long long xl6_xrt_ws_stream_send_text(
	void* pHandle,
	xlir_string text,
	int bCompressed
)
{
	return xl6_xrt_ws_stream_send(
		pHandle,
		XWS_OPCODE_TEXT,
		(void*)text.Data,
		text.Size,
		bCompressed
	);
}



/* 成功受理后把 Buffer 的 xrtMalloc 存储直接转交 XRT。 */
long long xl6_xrt_ws_stream_send_move(
	void* pHandle,
	long long iOpcode,
	void* pBuffer
)
{
	xl6_ws_command Command;

	if ( ((iOpcode != XWS_OPCODE_TEXT) && (iOpcode != XWS_OPCODE_BINARY)) ||
		 (pBuffer == 0) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = XL6_WS_COMMAND_SEND_MOVE;
	Command.Opcode = (xwsopcode)iOpcode;
	Command.Buffer = (xbuffer*)pBuffer;
	Command.Result = XNET_RESULT_ERROR;
	return xl6_ws_command_execute((xl6_ws_connection*)pHandle, &Command) ?
		Command.Result : XNET_RESULT_ERROR;
}



/* 发送 Ping。 */
long long xl6_xrt_ws_stream_ping(void* pHandle, void* pData, size_t iSize)
{
	xl6_ws_command Command;

	if ( (pData == 0) && (iSize != 0u) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = XL6_WS_COMMAND_PING;
	Command.Data = (xbytesview){ (cbytes)pData, iSize };
	Command.Result = XNET_RESULT_ERROR;
	return xl6_ws_command_execute((xl6_ws_connection*)pHandle, &Command) ? Command.Result : XNET_RESULT_ERROR;
}



/* 发送 Pong。 */
long long xl6_xrt_ws_stream_pong(void* pHandle, void* pData, size_t iSize)
{
	xl6_ws_command Command;

	if ( (pData == 0) && (iSize != 0u) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = XL6_WS_COMMAND_PONG;
	Command.Data = (xbytesview){ (cbytes)pData, iSize };
	Command.Result = XNET_RESULT_ERROR;
	return xl6_ws_command_execute((xl6_ws_connection*)pHandle, &Command) ? Command.Result : XNET_RESULT_ERROR;
}



/* 发起标准关闭握手。 */
long long xl6_xrt_ws_stream_close(void* pHandle, long long iCode, xlir_string reason)
{
	xl6_ws_command Command;

	if ( (iCode < 0) || (iCode > UINT16_MAX) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Command, 0, sizeof(Command));
	Command.Kind = XL6_WS_COMMAND_CLOSE;
	Command.Code = (uint16)iCode;
	Command.Reason = reason;
	Command.Result = XNET_RESULT_ERROR;
	return xl6_ws_command_execute((xl6_ws_connection*)pHandle, &Command) ? Command.Result : XNET_RESULT_ERROR;
}



/* 在消息块边界暂停应用数据事件。 */
void xl6_xrt_ws_stream_pause(void* pHandle)
{
	xl6_ws_command Command;

	memset(&Command, 0, sizeof(Command));
	Command.Kind = XL6_WS_COMMAND_PAUSE;
	(void)xl6_ws_command_execute((xl6_ws_connection*)pHandle, &Command);
}



/* 恢复应用数据事件。 */
int xl6_xrt_ws_stream_resume(void* pHandle)
{
	xl6_ws_command Command;

	memset(&Command, 0, sizeof(Command));
	Command.Kind = XL6_WS_COMMAND_RESUME;
	return xl6_ws_command_execute((xl6_ws_connection*)pHandle, &Command) && Command.Boolean;
}



/* Abort 原生支持任意线程。 */
int xl6_xrt_ws_stream_abort(void* pHandle)
{
	xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle);

	return pStream != 0 && xrtWsStreamAbort(pStream);
}



/* 获取一次性 Close 快照。 */
static bool xl6_ws_close_info(void* pHandle, xwsstreamclose* pClose)
{
	xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle);

	return pStream != 0 && xrtWsStreamCloseInfo(pStream, pClose);
}



long long xl6_xrt_ws_stream_close_flags(void* pHandle) { xwsstreamclose Close; return xl6_ws_close_info(pHandle, &Close) ? Close.Flags : 0; }
long long xl6_xrt_ws_stream_close_local(void* pHandle) { xwsstreamclose Close; return xl6_ws_close_info(pHandle, &Close) ? Close.LocalCode : 0; }
long long xl6_xrt_ws_stream_close_remote(void* pHandle) { xwsstreamclose Close; return xl6_ws_close_info(pHandle, &Close) ? Close.RemoteCode : 0; }



void* xl6_xrt_ws_stream_close_reason(void* pHandle)
{
	xwsstreamclose Close;

	return xl6_ws_close_info(pHandle, &Close) ?
		xl6_xrt_string_dup(Close.Reason.Data != 0 ? Close.Reason.Data : "", Close.Reason.Size) :
		xl6_xrt_string_dup("", 0u);
}



/* 获取 Stream 保存的首个结构化错误。 */
static const xerror* xl6_ws_error_info(void* pHandle)
{
	xwsstream* pStream = (xwsstream*)xl6_xrt_ws_stream_native(pHandle);

	return pStream != 0 ? xrtWsStreamError(pStream) : 0;
}



long long xl6_xrt_ws_stream_error_kind(void* pHandle) { const xerror* pError = xl6_ws_error_info(pHandle); return pError != 0 ? xrtErrorKind(pError) : 0; }
long long xl6_xrt_ws_stream_error_code(void* pHandle) { const xerror* pError = xl6_ws_error_info(pHandle); return pError != 0 ? xrtErrorCode(pError) : 0; }
void* xl6_xrt_ws_stream_error_domain(void* pHandle) { const xerror* pError = xl6_ws_error_info(pHandle); return xl6_xrt_string_dup(pError != 0 && xrtErrorDomain(pError) != 0 ? xrtErrorDomain(pError) : "", 0u); }
void* xl6_xrt_ws_stream_error_operation(void* pHandle) { const xerror* pError = xl6_ws_error_info(pHandle); return xl6_xrt_string_dup(pError != 0 && xrtErrorOperation(pError) != 0 ? xrtErrorOperation(pError) : "", 0u); }
void* xl6_xrt_ws_stream_error_message(void* pHandle) { const xerror* pError = xl6_ws_error_info(pHandle); return xl6_xrt_string_dup(pError != 0 && xrtErrorMessage(pError) != 0 ? xrtErrorMessage(pError) : "", 0u); }
);



/* TLS 接管作为独立片段，普通 WebSocket 不引用任何 TLS 符号。 */
static const char xl6WebSocketStreamTlsSource[] = XL6_WEBSOCKET_SOURCE(
/* TLS Attach 适配到共用命令入口。 */
static xwsstream* xl6_ws_attach_tls_proc(
	void* pTransport,
	size_t iPrefix,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
)
{
	return xrtWsStreamAttachTls((xtlsstream*)pTransport, iPrefix, pConfig, pEvents, pData);
}



/* 释放未被 Attach 接管的 TLS 引用。 */
static void xl6_ws_tls_release(void* pTransport)
{
	xrtTlsStreamDestroy((xtlsstream*)pTransport);
}



/* 在 TLS 所属 TCP Worker 上接管经过校验的 HTTPS Upgrade。 */
void* xl6_xrt_ws_stream_attach_tls(
	xvalue* pOwner,
	void* pUpgrade,
	void* pConfig,
	XL6_Function Handler,
	size_t iPrefix
)
{
	xtlsstream* pSource = (xtlsstream*)xl6ValueHandleData(pOwner);
	xtlsstream* pTransport = pSource != 0 ? xrtTlsStreamRef(pSource) : 0;
	xnetstream* pTcp = pTransport != 0 ? xrtTlsStreamTransport(pTransport) : 0;
	xnetworker* pWorker = pTcp != 0 ? xrtNetStreamWorker(pTcp) : 0;

	return xl6_ws_attach_common(
		pOwner,
		pUpgrade,
		pConfig,
		Handler,
		iPrefix,
		pTransport,
		pWorker,
		xl6_ws_attach_tls_proc,
		xl6_ws_tls_release
	);
}
);
