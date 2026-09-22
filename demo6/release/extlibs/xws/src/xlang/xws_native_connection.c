#include "xws_native_internal.h"



typedef struct xws_native_config {
	xwsconnconfig Config;
	str Protocol;
} xws_native_config;



typedef enum xws_native_transport {
	XWS_NATIVE_TCP = 1,
	XWS_NATIVE_TLS
} xws_native_transport;



typedef struct xws_native_attach {
	xws_native_transport Transport;
	ptr Stream;
	size_t Prefix;
	xwsconnconfig Config;
	xws_native_connection* Owner;
	xwsconn* Result;
} xws_native_attach;



typedef enum xws_native_send_kind {
	XWS_NATIVE_SEND = 1,
	XWS_NATIVE_SEND_TAKE,
	XWS_NATIVE_PING,
	XWS_NATIVE_PONG,
	XWS_NATIVE_CLOSE
} xws_native_send_kind;



typedef struct xws_native_send {
	xwsconn* Connection;
	xws_native_send_kind Kind;
	xwsopcode Opcode;
	xbytesview Data;
	xbuffer* Buffer;
	xstrview Reason;
	uint16 Code;
	bool Compressed;
	xnetresult Result;
} xws_native_send;



enum {
	XWS_NATIVE_EVENT_MESSAGE_BEGIN = 0,
	XWS_NATIVE_EVENT_MESSAGE_DATA,
	XWS_NATIVE_EVENT_MESSAGE_END,
	XWS_NATIVE_EVENT_PING,
	XWS_NATIVE_EVENT_PONG,
	XWS_NATIVE_EVENT_BACKPRESSURE,
	XWS_NATIVE_EVENT_WRITABLE,
	XWS_NATIVE_EVENT_DRAIN,
	XWS_NATIVE_EVENT_ERROR,
	XWS_NATIVE_EVENT_CLOSE
};



/* 释放配置持有的协议副本。 */
XLANG_XWS_EXPORT long long xws_config_destroy(void* pHandle)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;

	if ( pConfig != NULL ) {
		xrtFree(pConfig->Protocol);
		memset(pConfig, 0, sizeof(*pConfig));
		xrtFree(pConfig);
	}
	return 0;
}



/* 创建完整默认配置，并显式固定本地角色。 */
XLANG_XWS_EXPORT void* xws_config_create(long long iRole)
{
	xws_native_config* pConfig;

	if ( (iRole != XWS_ROLE_CLIENT) && (iRole != XWS_ROLE_SERVER) ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xws", 0,
			"WebSocket role must be client or server");
		return NULL;
	}
	pConfig = (xws_native_config*)xrtCalloc(1u, sizeof(*pConfig));
	if ( pConfig == NULL ) {
		return NULL;
	}
	xrtWsConnConfigInit(&pConfig->Config);
	pConfig->Config.Role = (xwsrole)iRole;
	return pConfig;
}



/* 返回供同包低层接口借用的配置。 */
XLANG_XWS_EXPORT void* xws_config_native(void* pHandle)
{
	return pHandle;
}



/* 替换协商后的子协议副本。 */
XLANG_XWS_EXPORT bool xws_config_protocol(
	void* pHandle,
	const char* sProtocol,
	size_t iSize
)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;
	str sCopy = NULL;

	if ( (pConfig == NULL) ||
		 ((sProtocol == NULL) && (iSize != 0u)) ||
		 (iSize == SIZE_MAX) ) {
		return false;
	}
	if ( iSize != 0u ) {
		sCopy = (str)xrtMalloc(iSize + 1u);
		if ( sCopy == NULL ) {
			return false;
		}
		memcpy(sCopy, sProtocol, iSize);
		sCopy[iSize] = 0;
	}
	xrtFree(pConfig->Protocol);
	pConfig->Protocol = sCopy;
	pConfig->Config.Protocol.Data = sCopy;
	pConfig->Config.Protocol.Size = iSize;
	return true;
}



/* 设置消息、线路帧和总发送预算。 */
XLANG_XWS_EXPORT bool xws_config_limits(
	void* pHandle,
	uint64 iMessage,
	uint64 iFrame,
	uint64 iSend
)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;

	if ( (pConfig == NULL) || (iMessage > SIZE_MAX) || (iSend > SIZE_MAX) ) {
		return false;
	}
	pConfig->Config.MessageLimit = (size_t)iMessage;
	pConfig->Config.FrameLimit = iFrame;
	pConfig->Config.SendLimit = (size_t)iSend;
	return true;
}



/* 设置控制帧保留预算和关闭握手超时，时间单位为微秒。 */
XLANG_XWS_EXPORT bool xws_config_control(
	void* pHandle,
	uint64 iReserve,
	uint64 iCloseTimeout
)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;

	if ( (pConfig == NULL) || (iReserve > SIZE_MAX) ) {
		return false;
	}
	pConfig->Config.ControlReserve = (size_t)iReserve;
	pConfig->Config.CloseTimeout = iCloseTimeout;
	return true;
}



/* 设置跨线程异步提交的字节、数量和单轮批量上限。 */
XLANG_XWS_EXPORT bool xws_config_async(
	void* pHandle,
	uint64 iBytes,
	uint64 iCount,
	uint64 iBatch
)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;

	if ( (pConfig == NULL) || (iBytes > SIZE_MAX) ||
		 (iCount > UINT32_MAX) || (iBatch > UINT32_MAX) ) {
		return false;
	}
	pConfig->Config.AsyncBytesLimit = (size_t)iBytes;
	pConfig->Config.AsyncCountLimit = (uint32)iCount;
	pConfig->Config.AsyncBatch = (uint32)iBatch;
	return true;
}



/* 设置是否自动响应 Ping。 */
XLANG_XWS_EXPORT void xws_config_auto_pong(void* pHandle, bool bEnabled)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;

	if ( pConfig != NULL ) {
		pConfig->Config.AutoPong = bEnabled;
	}
}



/* 从已经协商的扩展字段中提取唯一 permessage-deflate 响应。 */
XLANG_XWS_EXPORT bool xws_config_deflate(
	void* pHandle,
	const char* sExtensions,
	size_t iSize
)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;
	xstrview Extensions = { sExtensions, iSize };
	xwsextension Extension;
	xwsdeflate Deflate;
	size_t iOffset = 0u;
	xhttpnext Next;
	bool bFound = false;

	if ( (pConfig == NULL) ||
		 ((sExtensions == NULL) && (iSize != 0u)) ) {
		return false;
	}
	if ( iSize == 0u ) {
		pConfig->Config.DeflateEnabled = false;
		xrtWsDeflateInit(&pConfig->Config.Deflate);
		return true;
	}
	for ( ;; ) {
		Next = xrtWsExtensionNext(Extensions, &iOffset, &Extension);
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return false;
		}
		if ( xrtWsDeflateIs(&Extension) ) {
			if ( bFound || !xrtWsDeflateResponseParse(&Extension, &Deflate) ) {
				return false;
			}
			bFound = true;
		}
	}
	if ( !bFound ) {
		return false;
	}
	pConfig->Config.Deflate = Deflate;
	pConfig->Config.DeflateEnabled = true;
	return true;
}



/* 执行最终无分配配置校验。 */
XLANG_XWS_EXPORT bool xws_config_valid(void* pHandle)
{
	xws_native_config* pConfig = (xws_native_config*)pHandle;

	return (pConfig != NULL) && xrtWsConnConfigValid(&pConfig->Config);
}



/* 增加包装连接中的原生引用，防止语言析构与当前操作竞态。 */
xwsconn* xwsNativeConnectionRef(void* pHandle)
{
	xws_native_connection* pOwner = (xws_native_connection*)pHandle;
	xwsconn* pConnection;

	if ( pOwner == NULL ) {
		return NULL;
	}
	xrtSpinLock(&pOwner->Lock);
	pConnection = pOwner->Connection != NULL ?
		xrtWsConnRef(pOwner->Connection) : NULL;
	xrtSpinUnlock(&pOwner->Lock);
	return pConnection;
}



/* 最后一份包装引用释放闭包和锁。 */
void xwsNativeConnectionRelease(xws_native_connection* pOwner)
{
	if ( (pOwner == NULL) ||
		 (xrtAtomic32FetchSub(
			&pOwner->References,
			1u,
			XMEMORY_ACQ_REL
		 ) != 1u) ) {
		return;
	}
	xrtValueRelease(pOwner->Handler);
	xrtErrorFree(pOwner->CallbackError);
	xrtSpinDestroy(&pOwner->Lock);
	memset(pOwner, 0, sizeof(*pOwner));
	xrtFree(pOwner);
}



/* 保存首个回调错误；连接关闭后仍可由语言对象读取。 */
static void xwsNativeCallbackError(xws_native_connection* pOwner)
{
	xerror* pError = xrtTakeError();
	const xerror* pCause;
	xerror* pLeaf;

	xl6ExtlibErrorClear();

	if ( pError == NULL ) {
		xrtSetErrorInfo(XERR_STATE, "xlang.extlib.xws", 0,
			"xws event handler failed");
		pError = xrtTakeError();
	}
	pCause = pError;
	while ( (pCause != NULL) && (xrtErrorCause(pCause) != NULL) ) {
		pCause = xrtErrorCause(pCause);
	}
	pLeaf = xrtErrorRef(pCause);
	xrtErrorFree(pError);
	pError = pLeaf;
	xrtSpinLock(&pOwner->Lock);
	if ( pOwner->CallbackError == NULL ) {
		pOwner->CallbackError = pError;
		pError = NULL;
	}
	xrtSpinUnlock(&pOwner->Lock);
	xrtErrorFree(pError);
}



/* 直接调用类型化事件处理器；异常转换为 XRT 错误并立即中止连接。 */
static void xwsNativeEmit(
	xws_native_connection* pOwner,
	xwsconn* pConnection,
	long long iEvent,
	long long iOpcode,
	long long iFlags,
	uint64 iValue,
	ptr pData,
	size_t iSize
)
{
	xvalue* Arguments[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
	xrtcallframe Frame;
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	size_t i;
	bool bInvoked;

	if ( (pOwner == NULL) || (pOwner->Handler == NULL) ) {
		return;
	}
	Arguments[0] = xrtValueInt(iEvent);
	Arguments[1] = xrtValueInt(iOpcode);
	Arguments[2] = xrtValueInt(iFlags);
	Arguments[3] = xrtValueInt((int64)iValue);
	Arguments[4] = xrtValuePointer(pData);
	Arguments[5] = xrtValueInt((int64)iSize);
	for ( i = 0u; i < 6u; i += 1u ) {
		if ( Arguments[i] == NULL ) {
			for ( i = 0u; i < 6u; i += 1u ) {
				xrtValueRelease(Arguments[i]);
			}
			(void)xrtWsConnAbort(pConnection);
			return;
		}
	}
	memset(&Frame, 0, sizeof(Frame));
	Frame.ArgumentCount = 6u;
	Frame.Arguments = Arguments;
	bInvoked = xrtValueInvoke(pOwner->Handler, &Frame, &Result);
	xrtCallResultUnit(&Result);
	for ( i = 0u; i < 6u; i += 1u ) {
		xrtValueRelease(Arguments[i]);
	}
	if ( !bInvoked ) {
		xwsNativeCallbackError(pOwner);
		(void)xrtWsConnAbort(pConnection);
	}
}



/* 保存当前消息语义并发布 Begin。 */
static void xwsNativeMessageBegin(
	xwsconn* pConnection,
	const xwsmessageinfo* pInfo,
	ptr pData
)
{
	xws_native_connection* pOwner = (xws_native_connection*)pData;

	if ( pInfo == NULL ) {
		return;
	}
	xrtAtomic32Store(&pOwner->MessageOpcode, pInfo->Opcode, XMEMORY_RELEASE);
	xrtAtomic32Store(&pOwner->MessageFlags, pInfo->Flags, XMEMORY_RELEASE);
	xwsNativeEmit(pOwner, pConnection, XWS_NATIVE_EVENT_MESSAGE_BEGIN,
		pInfo->Opcode, pInfo->Flags, pInfo->PayloadSize, (ptr)pInfo, sizeof(*pInfo));
}



/* 消息数据视图只在本次直接回调中有效。 */
static void xwsNativeMessageData(
	xwsconn* pConnection,
	xbytesview Data,
	ptr pData
)
{
	xws_native_connection* pOwner = (xws_native_connection*)pData;

	xwsNativeEmit(pOwner, pConnection, XWS_NATIVE_EVENT_MESSAGE_DATA,
		xrtAtomic32Load(&pOwner->MessageOpcode, XMEMORY_ACQUIRE),
		xrtAtomic32Load(&pOwner->MessageFlags, XMEMORY_ACQUIRE),
		0u, (ptr)Data.Data, Data.Size);
}



/* 发布当前消息的最终边界。 */
static void xwsNativeMessageEnd(xwsconn* pConnection, ptr pData)
{
	xws_native_connection* pOwner = (xws_native_connection*)pData;

	xwsNativeEmit(pOwner, pConnection, XWS_NATIVE_EVENT_MESSAGE_END,
		xrtAtomic32Load(&pOwner->MessageOpcode, XMEMORY_ACQUIRE),
		xrtAtomic32Load(&pOwner->MessageFlags, XMEMORY_ACQUIRE),
		0u, NULL, 0u);
}



/* 发布 Ping 控制帧。 */
static void xwsNativePing(xwsconn* pConnection, xbytesview Data, ptr pData)
{
	xwsNativeEmit((xws_native_connection*)pData, pConnection,
		XWS_NATIVE_EVENT_PING, XWS_OPCODE_PING, 0, 0u, (ptr)Data.Data, Data.Size);
}



/* 发布 Pong 控制帧。 */
static void xwsNativePong(xwsconn* pConnection, xbytesview Data, ptr pData)
{
	xwsNativeEmit((xws_native_connection*)pData, pConnection,
		XWS_NATIVE_EVENT_PONG, XWS_OPCODE_PONG, 0, 0u, (ptr)Data.Data, Data.Size);
}



/* 发布首次背压边沿。 */
static void xwsNativeBackpressure(xwsconn* pConnection, size_t iPending, ptr pData)
{
	xwsNativeEmit((xws_native_connection*)pData, pConnection,
		XWS_NATIVE_EVENT_BACKPRESSURE, 0, 0, iPending, NULL, 0u);
}



/* 发布恢复可写边沿。 */
static void xwsNativeWritable(xwsconn* pConnection, size_t iPending, ptr pData)
{
	xwsNativeEmit((xws_native_connection*)pData, pConnection,
		XWS_NATIVE_EVENT_WRITABLE, 0, 0, iPending, NULL, 0u);
}



/* 发布发送排空边沿。 */
static void xwsNativeDrain(xwsconn* pConnection, ptr pData)
{
	xwsNativeEmit((xws_native_connection*)pData, pConnection,
		XWS_NATIVE_EVENT_DRAIN, 0, 0, 0u, NULL, 0u);
}



/* 错误对象仅在同步回调期间借用。 */
static void xwsNativeError(
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xwsNativeEmit((xws_native_connection*)pData, pConnection,
		XWS_NATIVE_EVENT_ERROR, 0,
		pError != NULL ? xrtErrorKind(pError) : 0,
		pError != NULL ? (uint64)(uint32)xrtErrorCode(pError) : 0u,
		(ptr)pError, 0u);
}



/* Close 是包装回调生命周期的唯一终点。 */
static void xwsNativeClose(
	xwsconn* pConnection,
	const xwsconnclose* pClose,
	ptr pData
)
{
	xws_native_connection* pOwner = (xws_native_connection*)pData;
	uint64 iCodes = 0u;

	if ( pClose != NULL ) {
		iCodes = ((uint64)pClose->LocalCode << 16u) |
			(uint64)pClose->RemoteCode;
		xwsNativeEmit(pOwner, pConnection, XWS_NATIVE_EVENT_CLOSE,
			0, pClose->Flags, iCodes, (ptr)pClose->Reason.Data, pClose->Reason.Size);
	}
	if ( xrtAtomic32Exchange(
		&pOwner->CallbackHeld,
		0u,
		XMEMORY_ACQ_REL
	) != 0u ) {
		xwsNativeConnectionRelease(pOwner);
	}
}



static const xwsconnevents xwsNativeEvents = {
	xwsNativeMessageBegin,
	xwsNativeMessageData,
	xwsNativeMessageEnd,
	xwsNativePing,
	xwsNativePong,
	xwsNativeBackpressure,
	xwsNativeWritable,
	xwsNativeDrain,
	xwsNativeError,
	xwsNativeClose
};



/* Worker 内先消费 HTTP 头前缀，再把传输引用交给 Connection。 */
static void xwsNativeAttachRun(ptr pData)
{
	xws_native_attach* pAttach = (xws_native_attach*)pData;
	bool bConsumed;

	if ( pAttach->Transport == XWS_NATIVE_TCP ) {
		xnetstream* pStream = (xnetstream*)pAttach->Stream;

		bConsumed = (pAttach->Prefix <= xrtNetStreamAvailable(pStream)) &&
			(xrtNetStreamConsume(pStream, pAttach->Prefix) == pAttach->Prefix);
		if ( bConsumed ) {
			pAttach->Result = xrtWsConnAttach(pStream, &pAttach->Config,
				&xwsNativeEvents, pAttach->Owner);
		}
	} else {
		xtlsstream* pStream = (xtlsstream*)pAttach->Stream;

		bConsumed = (pAttach->Prefix <= xrtTlsStreamAvailable(pStream)) &&
			xrtTlsStreamConsume(pStream, pAttach->Prefix);
		if ( bConsumed ) {
			pAttach->Result = xrtWsConnAttachTls(pStream, &pAttach->Config,
				&xwsNativeEvents, pAttach->Owner);
		}
	}
}



/* 从 extern 借用参数建立独立 callable 引用和终态回调引用。 */
static xws_native_connection* xwsNativeConnectionCreate(xvalue* pHandler)
{
	xws_native_connection* pOwner;
	xvalue* pRetained;

	if ( (pHandler == NULL) || !xrtValueIsCallable(pHandler) ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xws", 0,
			"xws requires a typed event handler");
		return NULL;
	}
	pRetained = xrtValueRetain(pHandler);
	if ( pRetained == NULL ) {
		return NULL;
	}
	pOwner = (xws_native_connection*)xrtCalloc(1u, sizeof(*pOwner));
	if ( pOwner == NULL ) {
		xrtValueRelease(pRetained);
		return NULL;
	}
	xrtAtomic32Init(&pOwner->References, 2u);
	xrtAtomic32Init(&pOwner->CallbackHeld, 1u);
	xrtAtomic32Init(&pOwner->MessageOpcode, 0u);
	xrtAtomic32Init(&pOwner->MessageFlags, 0u);
	if ( !xrtSpinInit(&pOwner->Lock) ) {
		xrtFree(pOwner);
		xrtValueRelease(pRetained);
		return NULL;
	}
	pOwner->Handler = pRetained;
	return pOwner;
}



/* TCP/TLS 共用配置快照、Worker 编组、所有权回滚和包装发布。 */
static void* xwsNativeAttach(
	ptr pStream,
	xws_native_transport Transport,
	void* pConfigHandle,
	xvalue* pHandler,
	size_t iPrefix
)
{
	xws_native_config* pConfig = (xws_native_config*)pConfigHandle;
	xws_native_connection* pOwner;
	xws_native_attach Attach;
	xnetworker* pWorker;
	ptr pOwned;

	if ( (pStream == NULL) || (pConfig == NULL) ||
		 !xrtWsConnConfigValid(&pConfig->Config) ) {
		return NULL;
	}
	if ( Transport == XWS_NATIVE_TCP ) {
		pOwned = xrtNetStreamRef((xnetstream*)pStream);
		pWorker = pOwned != NULL ? xrtNetStreamWorker((xnetstream*)pOwned) : NULL;
	} else {
		xnetstream* pTcp;

		pOwned = xrtTlsStreamRef((xtlsstream*)pStream);
		pTcp = pOwned != NULL ? xrtTlsStreamTransport((xtlsstream*)pOwned) : NULL;
		pWorker = pTcp != NULL ? xrtNetStreamWorker(pTcp) : NULL;
	}
	if ( (pOwned == NULL) || (pWorker == NULL) ) {
		if ( Transport == XWS_NATIVE_TCP ) {
			xrtNetStreamDestroy((xnetstream*)pOwned);
		} else {
			xrtTlsStreamDestroy((xtlsstream*)pOwned);
		}
		return NULL;
	}
	pOwner = xwsNativeConnectionCreate(pHandler);
	if ( pOwner == NULL ) {
		if ( Transport == XWS_NATIVE_TCP ) {
			xrtNetStreamDestroy((xnetstream*)pOwned);
		} else {
			xrtTlsStreamDestroy((xtlsstream*)pOwned);
		}
		return NULL;
	}
	memset(&Attach, 0, sizeof(Attach));
	Attach.Transport = Transport;
	Attach.Stream = pOwned;
	Attach.Prefix = iPrefix;
	Attach.Config = pConfig->Config;
	Attach.Owner = pOwner;
	if ( !xwsNativeWorkerCall(pWorker, xwsNativeAttachRun, &Attach) ||
		 (Attach.Result == NULL) ) {
		if ( Transport == XWS_NATIVE_TCP ) {
			xrtNetStreamDestroy((xnetstream*)pOwned);
		} else {
			xrtTlsStreamDestroy((xtlsstream*)pOwned);
		}
		(void)xrtAtomic32Exchange(&pOwner->CallbackHeld, 0u, XMEMORY_ACQ_REL);
		xwsNativeConnectionRelease(pOwner);
		xwsNativeConnectionRelease(pOwner);
		return NULL;
	}
	xrtSpinLock(&pOwner->Lock);
	pOwner->Connection = Attach.Result;
	xrtSpinUnlock(&pOwner->Lock);
	return pOwner;
}



/* 接管一份 TCP 强引用并在其 Worker 上建立连接。 */
XLANG_XWS_EXPORT void* xws_conn_attach_tcp(
	void* pStream,
	void* pConfig,
	xvalue* pHandler,
	size_t iPrefix
)
{
	return xwsNativeAttach(pStream, XWS_NATIVE_TCP, pConfig, pHandler, iPrefix);
}



/* 接管一份 TLS 强引用并在其 TCP Worker 上建立连接。 */
XLANG_XWS_EXPORT void* xws_conn_attach_tls(
	void* pStream,
	void* pConfig,
	xvalue* pHandler,
	size_t iPrefix
)
{
	return xwsNativeAttach(pStream, XWS_NATIVE_TLS, pConfig, pHandler, iPrefix);
}



/* point 构造只接管已经建立的包装句柄。 */
XLANG_XWS_EXPORT void* xws_conn_wrap(void* pHandle)
{
	return pHandle;
}



/* 语言对象析构请求异常关闭，但终态回调仍由独立引用保护。 */
XLANG_XWS_EXPORT long long xws_conn_destroy(void* pHandle)
{
	xws_native_connection* pOwner = (xws_native_connection*)pHandle;
	xwsconn* pConnection;

	if ( pOwner == NULL ) {
		return 0;
	}
	xrtSpinLock(&pOwner->Lock);
	pConnection = pOwner->Connection;
	pOwner->Connection = NULL;
	xrtSpinUnlock(&pOwner->Lock);
	if ( pConnection != NULL ) {
		(void)xrtWsConnAbort(pConnection);
		xrtWsConnDestroy(pConnection);
	}
	xwsNativeConnectionRelease(pOwner);
	return 0;
}



/* 返回仅在包装对象存活期间有效的底层借用指针。 */
XLANG_XWS_EXPORT void* xws_conn_native(void* pHandle)
{
	xws_native_connection* pOwner = (xws_native_connection*)pHandle;
	xwsconn* pConnection;

	if ( pOwner == NULL ) {
		return NULL;
	}
	xrtSpinLock(&pOwner->Lock);
	pConnection = pOwner->Connection;
	xrtSpinUnlock(&pOwner->Lock);
	return pConnection;
}



/* 并发读取连接状态。 */
XLANG_XWS_EXPORT long long xws_conn_state(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	long long iState = pConnection != NULL ? xrtWsConnState(pConnection) : XWS_CONN_CLOSED;

	xrtWsConnDestroy(pConnection);
	return iState;
}



/* 并发读取固定角色。 */
XLANG_XWS_EXPORT long long xws_conn_role(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	long long iRole = pConnection != NULL ? xrtWsConnRole(pConnection) : XWS_ROLE_SERVER;

	xrtWsConnDestroy(pConnection);
	return iRole;
}



/* 复制协商后的子协议。 */
XLANG_XWS_EXPORT void* xws_conn_protocol(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xbuffer* pResult = pConnection != NULL ?
		xwsNativeText(xrtWsConnProtocol(pConnection)) : xwsNativeText((xstrview){ NULL, 0u });

	xrtWsConnDestroy(pConnection);
	return pResult;
}



/* 查询是否启用了 permessage-deflate。 */
XLANG_XWS_EXPORT bool xws_conn_deflate(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xwsdeflate Deflate;
	bool bEnabled = (pConnection != NULL) && xrtWsConnDeflate(pConnection, &Deflate);

	xrtWsConnDestroy(pConnection);
	return bEnabled;
}



/* 并发读取线路待发预算。 */
XLANG_XWS_EXPORT uint64 xws_conn_pending(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	uint64 iPending = pConnection != NULL ? xrtWsConnPending(pConnection) : 0u;

	xrtWsConnDestroy(pConnection);
	return iPending;
}



/* 并发读取普通数据剩余可写预算。 */
XLANG_XWS_EXPORT uint64 xws_conn_writable(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	uint64 iWritable = pConnection != NULL ? xrtWsConnWritable(pConnection) : 0u;

	xrtWsConnDestroy(pConnection);
	return iWritable;
}



/* 查询接收侧暂停状态。 */
XLANG_XWS_EXPORT bool xws_conn_paused(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	bool bPaused = (pConnection != NULL) && xrtWsConnPaused(pConnection);

	xrtWsConnDestroy(pConnection);
	return bPaused;
}



/* 从任意线程暂停后续应用消息事件。 */
XLANG_XWS_EXPORT void xws_conn_pause(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);

	if ( pConnection != NULL ) {
		xrtWsConnPause(pConnection);
	}
	xrtWsConnDestroy(pConnection);
}



/* 从任意线程恢复消息事件并驱动 Worker。 */
XLANG_XWS_EXPORT bool xws_conn_resume(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	bool bResult = (pConnection != NULL) && xrtWsConnResume(pConnection);

	xrtWsConnDestroy(pConnection);
	return bResult;
}



/* 从任意线程请求异常关闭。 */
XLANG_XWS_EXPORT bool xws_conn_abort(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	bool bResult = (pConnection != NULL) && xrtWsConnAbort(pConnection);

	xrtWsConnDestroy(pConnection);
	return bResult;
}



/* Worker 内执行一项有界发送命令。 */
static void xwsNativeSendRun(ptr pData)
{
	xws_native_send* pSend = (xws_native_send*)pData;

	switch ( pSend->Kind ) {
		case XWS_NATIVE_SEND:
			pSend->Result = pSend->Compressed ?
				xrtWsConnSendCompressed(pSend->Connection, pSend->Opcode, pSend->Data) :
				xrtWsConnSend(pSend->Connection, pSend->Opcode, pSend->Data);
			break;

		case XWS_NATIVE_SEND_TAKE:
			pSend->Result = xrtWsConnSendTake(pSend->Connection, pSend->Opcode,
				pSend->Buffer->Data, pSend->Buffer->Size);
			if ( pSend->Result == XNET_RESULT_OK ) {
				pSend->Buffer->Data = NULL;
				pSend->Buffer->Size = 0u;
				pSend->Buffer->Capacity = 0u;
			}
			break;

		case XWS_NATIVE_PING:
			pSend->Result = xrtWsConnPing(pSend->Connection, pSend->Data);
			break;

		case XWS_NATIVE_PONG:
			pSend->Result = xrtWsConnPong(pSend->Connection, pSend->Data);
			break;

		case XWS_NATIVE_CLOSE:
			pSend->Result = xrtWsConnClose(pSend->Connection,
				pSend->Code, pSend->Reason);
			break;
	}
}



/* 把同步发送编组到连接 Worker。 */
static xnetresult xwsNativeSendCall(void* pHandle, xws_native_send* pSend)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xnetworker* pWorker = pConnection != NULL ? xrtWsConnWorker(pConnection) : NULL;

	if ( (pConnection == NULL) || (pWorker == NULL) ) {
		xrtWsConnDestroy(pConnection);
		return XNET_RESULT_CLOSED;
	}
	pSend->Connection = pConnection;
	pSend->Result = XNET_RESULT_ERROR;
	if ( !xwsNativeWorkerCall(pWorker, xwsNativeSendRun, pSend) ) {
		pSend->Result = XNET_RESULT_ERROR;
	}
	xrtWsConnDestroy(pConnection);
	return pSend->Result;
}



/* 复制发送一条 Text 或 Binary 消息。 */
XLANG_XWS_EXPORT long long xws_conn_send(
	void* pHandle,
	long long iOpcode,
	void* pData,
	size_t iSize,
	bool bCompressed
)
{
	xws_native_send Send;

	if ( ((iOpcode != XWS_OPCODE_TEXT) && (iOpcode != XWS_OPCODE_BINARY)) ||
		 ((pData == NULL) && (iSize != 0u)) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Send, 0, sizeof(Send));
	Send.Kind = XWS_NATIVE_SEND;
	Send.Opcode = (xwsopcode)iOpcode;
	Send.Data = (xbytesview){ (cbytes)pData, iSize };
	Send.Compressed = bCompressed;
	return xwsNativeSendCall(pHandle, &Send);
}



/* 成功受理后直接移交 Buffer 的 XRT 存储。 */
/* 文本便利入口保留字符串 ABI，同时复用统一发送实现。 */
XLANG_XWS_EXPORT long long xws_conn_send_text(
	void* pHandle,
	const char* sText,
	size_t iSize,
	bool bCompressed
)
{
	return xws_conn_send(
		pHandle,
		XWS_OPCODE_TEXT,
		(void*)sText,
		iSize,
		bCompressed
	);
}



XLANG_XWS_EXPORT long long xws_conn_send_move(
	void* pHandle,
	long long iOpcode,
	void* pBuffer
)
{
	xws_native_send Send;

	if ( ((iOpcode != XWS_OPCODE_TEXT) && (iOpcode != XWS_OPCODE_BINARY)) ||
		 (pBuffer == NULL) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Send, 0, sizeof(Send));
	Send.Kind = XWS_NATIVE_SEND_TAKE;
	Send.Opcode = (xwsopcode)iOpcode;
	Send.Buffer = (xbuffer*)pBuffer;
	return xwsNativeSendCall(pHandle, &Send);
}



/* 发送 Ping 或 Pong 控制帧。 */
XLANG_XWS_EXPORT long long xws_conn_control(
	void* pHandle,
	long long iOpcode,
	void* pData,
	size_t iSize
)
{
	xws_native_send Send;

	if ( ((iOpcode != XWS_OPCODE_PING) && (iOpcode != XWS_OPCODE_PONG)) ||
		 ((pData == NULL) && (iSize != 0u)) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Send, 0, sizeof(Send));
	Send.Kind = iOpcode == XWS_OPCODE_PING ? XWS_NATIVE_PING : XWS_NATIVE_PONG;
	Send.Data = (xbytesview){ (cbytes)pData, iSize };
	return xwsNativeSendCall(pHandle, &Send);
}



/* 提交唯一关闭握手。 */
XLANG_XWS_EXPORT long long xws_conn_close(
	void* pHandle,
	long long iCode,
	const char* sReason,
	size_t iSize
)
{
	xws_native_send Send;

	if ( (iCode < 0) || (iCode > UINT16_MAX) ||
		 ((sReason == NULL) && (iSize != 0u)) ) {
		return XNET_RESULT_ERROR;
	}
	memset(&Send, 0, sizeof(Send));
	Send.Kind = XWS_NATIVE_CLOSE;
	Send.Code = (uint16)iCode;
	Send.Reason = (xstrview){ sReason, iSize };
	return xwsNativeSendCall(pHandle, &Send);
}



/* 返回异步队列字节快照。 */
XLANG_XWS_EXPORT uint64 xws_conn_async_bytes(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	uint64 iValue = pConnection != NULL ? xrtWsConnAsyncBytes(pConnection) : 0u;

	xrtWsConnDestroy(pConnection);
	return iValue;
}



/* 返回异步队列操作数快照。 */
XLANG_XWS_EXPORT uint64 xws_conn_async_count(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	uint64 iValue = pConnection != NULL ? xrtWsConnAsyncCount(pConnection) : 0u;

	xrtWsConnDestroy(pConnection);
	return iValue;
}



/* 从任意线程异步复制发送消息。 */
XLANG_XWS_EXPORT void* xws_conn_send_async(
	void* pHandle,
	long long iOpcode,
	void* pData,
	size_t iSize,
	bool bCompressed
)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xbytesview Data = { (cbytes)pData, iSize };
	xfuture* pFuture = NULL;

	if ( (pConnection != NULL) &&
		 ((iOpcode == XWS_OPCODE_TEXT) || (iOpcode == XWS_OPCODE_BINARY)) &&
		 ((pData != NULL) || (iSize == 0u)) ) {
		pFuture = bCompressed ?
			xrtWsConnSendCompressedAsync(pConnection, (xwsopcode)iOpcode, Data) :
			xrtWsConnSendAsync(pConnection, (xwsopcode)iOpcode, Data);
	}
	xrtWsConnDestroy(pConnection);
	return pFuture;
}



/* 从任意线程异步发送控制帧。 */
/* 文本异步入口只在提交期间借用字符串，XRT 会在返回前完成复制。 */
XLANG_XWS_EXPORT void* xws_conn_send_text_async(
	void* pHandle,
	const char* sText,
	size_t iSize,
	bool bCompressed
)
{
	return xws_conn_send_async(
		pHandle,
		XWS_OPCODE_TEXT,
		(void*)sText,
		iSize,
		bCompressed
	);
}



XLANG_XWS_EXPORT void* xws_conn_control_async(
	void* pHandle,
	long long iOpcode,
	void* pData,
	size_t iSize
)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xbytesview Data = { (cbytes)pData, iSize };
	xfuture* pFuture = NULL;

	if ( (pConnection != NULL) && ((pData != NULL) || (iSize == 0u)) ) {
		if ( iOpcode == XWS_OPCODE_PING ) {
			pFuture = xrtWsConnPingAsync(pConnection, Data);
		} else if ( iOpcode == XWS_OPCODE_PONG ) {
			pFuture = xrtWsConnPongAsync(pConnection, Data);
		}
	}
	xrtWsConnDestroy(pConnection);
	return pFuture;
}



/* 从任意线程异步提交关闭握手。 */
XLANG_XWS_EXPORT void* xws_conn_close_async(
	void* pHandle,
	long long iCode,
	const char* sReason,
	size_t iSize
)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xfuture* pFuture = NULL;

	if ( (pConnection != NULL) && (iCode >= 0) && (iCode <= UINT16_MAX) &&
		 ((sReason != NULL) || (iSize == 0u)) ) {
		pFuture = xrtWsConnCloseAsync(pConnection, (uint16)iCode,
			(xstrview){ sReason, iSize });
	}
	xrtWsConnDestroy(pConnection);
	return pFuture;
}



/* 创建可写、排空或关闭条件 Future。 */
XLANG_XWS_EXPORT void* xws_conn_wait_async(void* pHandle, long long iWait)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xfuture* pFuture = NULL;

	if ( (pConnection != NULL) &&
		 (iWait >= XWS_CONN_WAIT_WRITE) && (iWait <= XWS_CONN_WAIT_CLOSE) ) {
		pFuture = xrtWsConnWaitAsync(pConnection, (xwsconnwait)iWait);
	}
	xrtWsConnDestroy(pConnection);
	return pFuture;
}



/* 复制一次关闭快照中的原因文本。 */
XLANG_XWS_EXPORT void* xws_conn_close_reason(void* pHandle)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xwsconnclose Close;
	xbuffer* pResult;

	memset(&Close, 0, sizeof(Close));
	pResult = (pConnection != NULL) && xrtWsConnCloseInfo(pConnection, &Close) ?
		xwsNativeText(Close.Reason) : xwsNativeText((xstrview){ NULL, 0u });
	xrtWsConnDestroy(pConnection);
	return pResult;
}



/* 读取关闭快照中的标志和代码。 */
XLANG_XWS_EXPORT uint64 xws_conn_close_value(void* pHandle, long long iField)
{
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	xwsconnclose Close;
	uint64 iValue = 0u;

	memset(&Close, 0, sizeof(Close));
	if ( (pConnection != NULL) && xrtWsConnCloseInfo(pConnection, &Close) ) {
		if ( iField == 0 ) { iValue = Close.Flags; }
		else if ( iField == 1 ) { iValue = Close.LocalCode; }
		else if ( iField == 2 ) { iValue = Close.RemoteCode; }
		else if ( iField == 3 ) { iValue = Close.Transport; }
	}
	xrtWsConnDestroy(pConnection);
	return iValue;
}



/* 复制连接保存的结构化错误字段。 */
XLANG_XWS_EXPORT void* xws_conn_error_text(void* pHandle, long long iField)
{
	xws_native_connection* pOwner = (xws_native_connection*)pHandle;
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	const xerror* pError;
	cstr sText = "";

	if ( pOwner == NULL ) {
		return xwsNativeText((xstrview){ NULL, 0u });
	}
	xrtSpinLock(&pOwner->Lock);
	pError = pOwner->CallbackError != NULL ?
		pOwner->CallbackError :
		(pConnection != NULL ? xrtWsConnError(pConnection) : NULL);
	if ( pError != NULL ) {
		if ( iField == 0 ) { sText = xrtErrorDomain(pError); }
		else if ( iField == 1 ) { sText = xrtErrorOperation(pError); }
		else { sText = xrtErrorMessage(pError); }
	}
	if ( sText == NULL ) { sText = ""; }
	{
		xbuffer* pResult = xwsNativeText((xstrview){ sText, strlen(sText) });

		xrtSpinUnlock(&pOwner->Lock);
		xrtWsConnDestroy(pConnection);
		return pResult;
	}
}



/* 读取结构化错误的 kind 或 code。 */
XLANG_XWS_EXPORT long long xws_conn_error_value(void* pHandle, long long iField)
{
	xws_native_connection* pOwner = (xws_native_connection*)pHandle;
	xwsconn* pConnection = xwsNativeConnectionRef(pHandle);
	const xerror* pError;
	long long iValue = 0;

	if ( pOwner == NULL ) {
		return 0;
	}
	xrtSpinLock(&pOwner->Lock);
	pError = pOwner->CallbackError != NULL ?
		pOwner->CallbackError :
		(pConnection != NULL ? xrtWsConnError(pConnection) : NULL);
	if ( pError != NULL ) {
		iValue = iField == 0 ?
			(long long)xrtErrorKind(pError) :
			(long long)xrtErrorCode(pError);
	}
	xrtSpinUnlock(&pOwner->Lock);
	xrtWsConnDestroy(pConnection);
	return iValue;
}



/* Future point 构造直接接管一份返回引用。 */
XLANG_XWS_EXPORT void* xws_future_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放 Future 引用。 */
XLANG_XWS_EXPORT long long xws_future_destroy(void* pHandle)
{
	xrtFutureDestroy((xfuture*)pHandle);
	return 0;
}



/* 返回 Future 终态快照。 */
XLANG_XWS_EXPORT long long xws_future_state(void* pHandle)
{
	return pHandle != NULL ? xrtFutureState((xfuture*)pHandle) : XFUTURE_CLOSED;
}



/* 请求协作取消。 */
XLANG_XWS_EXPORT bool xws_future_cancel(void* pHandle)
{
	return (pHandle != NULL) && xrtFutureCancel((xfuture*)pHandle);
}



/* 以毫秒为语言单位等待；负值表示无限等待。 */
XLANG_XWS_EXPORT long long xws_future_wait(void* pHandle, long long iTimeoutMs)
{
	uint64 iTimeout;

	if ( pHandle == NULL ) {
		return XWAIT_ERROR;
	}
	if ( iTimeoutMs < 0 ) {
		return xrtFutureWait((xfuture*)pHandle);
	}
	if ( (uint64)iTimeoutMs > (UINT64_MAX / UINT64_C(1000)) ) {
		return XWAIT_ERROR;
	}
	iTimeout = (uint64)iTimeoutMs * UINT64_C(1000);
	return xrtFutureWaitFor((xfuture*)pHandle, iTimeout);
}



/* 复制 Future 失败错误文本。 */
XLANG_XWS_EXPORT void* xws_future_error(void* pHandle)
{
	const xerror* pError = pHandle != NULL ?
		xrtFutureError((xfuture*)pHandle) : NULL;
	cstr sMessage = pError != NULL ? xrtErrorMessage(pError) : "";

	if ( sMessage == NULL ) { sMessage = ""; }
	return xwsNativeText((xstrview){ sMessage, strlen(sMessage) });
}
