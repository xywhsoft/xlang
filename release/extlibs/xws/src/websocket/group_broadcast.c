#include "../internal/xrt_websocket_group.h"
#include "../internal/xrt_websocket_group_future.h"

#include <xrt.h>
#include "../../include/xrt/websocket_group.h"



#if defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE)

/* 共享负载在全部逐连接异步发送归还引用后释放一次来源。 */
typedef struct __xrt_ws_group_payload {
	volatile int32 References;
	xnetref Source;
	bool External;
	uint8 Data[];
} __xrt_ws_group_payload;



/* 完整消息提交上下文可借用空负载，也可共享一个所有权负载。 */
typedef struct __xrt_ws_group_message {
	xwsopcode Opcode;
	xbytesview Payload;
	__xrt_ws_group_payload* Shared;
} __xrt_ws_group_message;



/* 控制帧提交上下文保存操作码和小负载视图。 */
typedef struct __xrt_ws_group_control {
	xwsopcode Opcode;
	xbytesview Payload;
} __xrt_ws_group_control;



/* Close 提交上下文在同步遍历期间借用原因文本。 */
typedef struct __xrt_ws_group_close {
	uint16 Code;
	xstrview Reason;
} __xrt_ws_group_close;



/* Wait 提交上下文保存统一的 Connection 条件。 */
typedef struct __xrt_ws_group_wait {
	xwsconnwait Wait;
} __xrt_ws_group_wait;



/* 最后一个共享负载引用归还调用方来源或释放单次复制。 */
static void __xrtWsGroupPayloadRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	__xrt_ws_group_payload* pPayload =
		(__xrt_ws_group_payload*)pContext;

	(void)pData;
	(void)iSize;
	if ( xrtRefRelease(&pPayload->References) != 0 ) {
		return;
	}
	if ( pPayload->External ) {
		pPayload->Source.Release(
			pPayload->Source.Context,
			pPayload->Source.Data,
			pPayload->Source.Size
		);
	}
	xrtFree(pPayload);
}



/* 为复制型广播创建一次共享负载；空负载不需要所有权节点。 */
static __xrt_ws_group_payload* __xrtWsGroupPayloadCopy(
	xbytesview Payload
)
{
	__xrt_ws_group_payload* pShared;
	size_t iBytes;

	if ( Payload.Size == 0 ) {
		return NULL;
	}
	if ( Payload.Size > (SIZE_MAX - sizeof(*pShared)) ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.payload",
			"WebSocket group payload size overflows"
		);
		return NULL;
	}
	iBytes = sizeof(*pShared) + Payload.Size;
	pShared = (__xrt_ws_group_payload*)xrtMalloc(iBytes);
	if ( pShared == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.payload",
			"WebSocket group shared payload allocation failed"
		);
		return NULL;
	}
	pShared->References = 1;
	pShared->Source = (xnetref) {
		pShared->Data,
		Payload.Size,
		__xrtWsGroupPayloadRelease,
		pShared
	};
	pShared->External = false;
	memcpy(pShared->Data, Payload.Data, Payload.Size);
	return pShared;
}



/* 为调用方所有权负载创建共享控制节点，但尚不调用来源 Release。 */
static __xrt_ws_group_payload* __xrtWsGroupPayloadTake(
	const xnetref* pRef
)
{
	__xrt_ws_group_payload* pShared =
		(__xrt_ws_group_payload*)xrtMalloc(sizeof(*pShared));

	if ( pShared == NULL ) {
		__xrtWsGroupWrap(
			XERR_MEMORY,
			XWS_GROUP_ERROR_MEMORY,
			"websocket-group.payload",
			"WebSocket group reference control allocation failed"
		);
		return NULL;
	}
	pShared->References = 1;
	pShared->Source = *pRef;
	pShared->External = true;
	return pShared;
}



/* 为一个成员取得共享负载引用。 */
static bool __xrtWsGroupPayloadRef(
	__xrt_ws_group_payload* pShared,
	xnetref* pRef
)
{
	if ( xrtRefRetain(&pShared->References) < 0 ) {
		__xrtWsGroupError(
			XERR_RANGE,
			XWS_GROUP_ERROR_RANGE,
			"websocket-group.payload-ref",
			"WebSocket group payload reference limit is reached"
		);
		return false;
	}
	pRef->Data = pShared->Source.Data;
	pRef->Size = pShared->Source.Size;
	pRef->Release = __xrtWsGroupPayloadRelease;
	pRef->Context = pShared;
	return true;
}



/* 为一个成员提交共享消息；空消息沿用普通异步路径。 */
static xfuture* __xrtWsGroupSubmitMessage(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_message* pMessage =
		(__xrt_ws_group_message*)pData;
	xnetref Ref;
	xfuture* pFuture;

	if ( pMessage->Shared == NULL ) {
		return xrtWsConnSendAsync(
			pConnection,
			pMessage->Opcode,
			pMessage->Payload
		);
	}
	if ( !__xrtWsGroupPayloadRef(pMessage->Shared, &Ref) ) {
		return NULL;
	}
	pFuture = xrtWsConnSendRefAsync(
		pConnection,
		pMessage->Opcode,
		&Ref
	);
	if ( pFuture == NULL ) {
		__xrtWsGroupPayloadRelease(
			Ref.Context,
			Ref.Data,
			Ref.Size
		);
	}
	return pFuture;
}



/* 为一个成员提交 Ping 或 Pong。 */
static xfuture* __xrtWsGroupSubmitControl(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_control* pControl =
		(__xrt_ws_group_control*)pData;

	return pControl->Opcode == XWS_OPCODE_PING ?
		xrtWsConnPingAsync(pConnection, pControl->Payload) :
		xrtWsConnPongAsync(pConnection, pControl->Payload);
}



/* 为一个成员提交 Close。 */
static xfuture* __xrtWsGroupSubmitClose(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_close* pClose = (__xrt_ws_group_close*)pData;

	return xrtWsConnCloseAsync(
		pConnection,
		pClose->Code,
		pClose->Reason
	);
}



/* 为一个成员提交 Connection 条件等待。 */
static xfuture* __xrtWsGroupSubmitWait(
	xwsconn* pConnection,
	ptr pData
)
{
	__xrt_ws_group_wait* pWait = (__xrt_ws_group_wait*)pData;

	return xrtWsConnWaitAsync(pConnection, pWait->Wait);
}



/* 校验完整消息共有的操作码、视图和 UTF-8 语义。 */
static bool __xrtWsGroupMessageValid(
	xwsopcode Opcode,
	xbytesview Payload
)
{
	if ( ((Opcode != XWS_OPCODE_TEXT) &&
		 (Opcode != XWS_OPCODE_BINARY)) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send",
			"WebSocket group message opcode or view is invalid"
		);
		return false;
	}
	if ( (Opcode == XWS_OPCODE_TEXT) &&
		!xrtUtf8Valid(
			(xstrview) {
				(const char*)Payload.Data,
				Payload.Size
			},
			NULL
		) ) {
		__xrtWsGroupWrap(
			XERR_VALUE,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send",
			"WebSocket group text is not valid UTF-8"
		);
		return false;
	}
	return true;
}



/* 创建操作并以一次复制的共享负载提交完整消息。 */
XRT_API xwsgroupop* xrtWsGroupSendAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_message Message;

	if ( !__xrtWsGroupMessageValid(Opcode, Payload) ) {
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Message.Opcode = Opcode;
	Message.Payload = Payload;
	Message.Shared = NULL;
	if ( (Payload.Size != 0) &&
		(xrtWsGroupOpCount(pOperation) != 0) ) {
		Message.Shared = __xrtWsGroupPayloadCopy(Payload);
		if ( Message.Shared == NULL ) {
			xrtWsGroupOpDestroy(pOperation);
			return NULL;
		}
		Message.Payload = (xbytesview) {
			Message.Shared->Source.Data,
			Message.Shared->Source.Size
		};
	}
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitMessage,
		&Message
	) ) {
		if ( Message.Shared != NULL ) {
			__xrtWsGroupPayloadRelease(
				Message.Shared,
				Message.Shared->Source.Data,
				Message.Shared->Source.Size
			);
		}
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	if ( Message.Shared != NULL ) {
		__xrtWsGroupPayloadRelease(
			Message.Shared,
			Message.Shared->Source.Data,
			Message.Shared->Source.Size
		);
	}
	return pOperation;
}



/* 对稳定成员快照提交 UTF-8 Text。 */
XRT_API xwsgroupop* xrtWsGroupTextAsync(
	xwsgroup* pGroup,
	xstrview Text
)
{
	return xrtWsGroupSendAsync(
		pGroup,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 对稳定成员快照提交 Binary。 */
XRT_API xwsgroupop* xrtWsGroupBinaryAsync(
	xwsgroup* pGroup,
	xbytesview Data
)
{
	return xrtWsGroupSendAsync(
		pGroup,
		XWS_OPCODE_BINARY,
		Data
	);
}



/* 创建操作后接管并共享调用方所有权负载。 */
XRT_API xwsgroupop* xrtWsGroupSendRefAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	const xnetref* pRef
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_payload* pShared;
	__xrt_ws_group_message Message;
	xnetref Ref;

	if ( !__xrtWsGroupCheck(
		pGroup,
		"websocket-group.send-ref"
	) ) {
		return NULL;
	}
	if ( !xrtMemRangeValid(pRef, sizeof(Ref)) ||
		__xrtWsGroupOverlaps(
			pGroup,
			(cbytes)pRef,
			sizeof(Ref)
		) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send-ref",
			"WebSocket group reference range is invalid"
		);
		return NULL;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( (Ref.Data == NULL) ||
		(Ref.Size == 0) ||
		(Ref.Release == NULL) ||
		!xrtMemRangeValid(Ref.Data, Ref.Size) ||
		__xrtWsGroupOverlaps(
			pGroup,
			Ref.Data,
			Ref.Size
		) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.send-ref",
			"WebSocket group reference is incomplete or invalid"
		);
		return NULL;
	}
	if ( !__xrtWsGroupMessageValid(
		Opcode,
		(xbytesview) { Ref.Data, Ref.Size }
	) ) {
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	pShared = __xrtWsGroupPayloadTake(&Ref);
	if ( pShared == NULL ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	Message.Opcode = Opcode;
	Message.Payload = (xbytesview) {
		Ref.Data,
		Ref.Size
	};
	Message.Shared = pShared;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitMessage,
		&Message
	) ) {
		xrtFree(pShared);
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	__xrtWsGroupPayloadRelease(
		pShared,
		pShared->Source.Data,
		pShared->Source.Size
	);
	return pOperation;
}



/* 对稳定成员快照提交共享所有权 Text。 */
XRT_API xwsgroupop* xrtWsGroupTextRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
)
{
	return xrtWsGroupSendRefAsync(
		pGroup,
		XWS_OPCODE_TEXT,
		pRef
	);
}



/* 对稳定成员快照提交共享所有权 Binary。 */
XRT_API xwsgroupop* xrtWsGroupBinaryRefAsync(
	xwsgroup* pGroup,
	const xnetref* pRef
)
{
	return xrtWsGroupSendRefAsync(
		pGroup,
		XWS_OPCODE_BINARY,
		pRef
	);
}



/* 创建并提交一个 Ping 或 Pong 批量操作。 */
static xwsgroupop* __xrtWsGroupControlAsync(
	xwsgroup* pGroup,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_control Control;

	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Size > XWS_CLOSE_PAYLOAD_MAX) ) {
		__xrtWsGroupError(
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XERR_ARGUMENT : XERR_RANGE,
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XWS_GROUP_ERROR_ARGUMENT :
				XWS_GROUP_ERROR_RANGE,
			"websocket-group.control",
			"WebSocket group control payload is invalid"
		);
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Control.Opcode = Opcode;
	Control.Payload = Payload;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitControl,
		&Control
	) ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	return pOperation;
}



/* 对稳定成员快照提交 Ping。 */
XRT_API xwsgroupop* xrtWsGroupPingAsync(
	xwsgroup* pGroup,
	xbytesview Payload
)
{
	return __xrtWsGroupControlAsync(
		pGroup,
		XWS_OPCODE_PING,
		Payload
	);
}



/* 对稳定成员快照提交 Pong。 */
XRT_API xwsgroupop* xrtWsGroupPongAsync(
	xwsgroup* pGroup,
	xbytesview Payload
)
{
	return __xrtWsGroupControlAsync(
		pGroup,
		XWS_OPCODE_PONG,
		Payload
	);
}



/* 对稳定成员快照提交唯一 Close。 */
XRT_API xwsgroupop* xrtWsGroupCloseAsync(
	xwsgroup* pGroup,
	uint16 iCode,
	xstrview Reason
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_close Close;
	size_t iPayload = 0;

	if ( !xrtWsCloseWrite(
		iCode,
		Reason,
		NULL,
		0,
		&iPayload
	) ) {
		__xrtWsGroupWrap(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.close",
			"WebSocket group Close code or reason is invalid"
		);
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Close.Code = iCode;
	Close.Reason = Reason;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitClose,
		&Close
	) ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	return pOperation;
}



/* 对稳定成员快照提交统一 Connection 条件等待。 */
XRT_API xwsgroupop* xrtWsGroupWaitAsync(
	xwsgroup* pGroup,
	xwsconnwait Wait
)
{
	xwsgroupop* pOperation;
	__xrt_ws_group_wait Context;

	if ( (Wait < XWS_CONN_WAIT_WRITE) ||
		(Wait > XWS_CONN_WAIT_CLOSE) ) {
		__xrtWsGroupError(
			XERR_ARGUMENT,
			XWS_GROUP_ERROR_ARGUMENT,
			"websocket-group.wait",
			"WebSocket group wait condition is invalid"
		);
		return NULL;
	}
	pOperation = __xrtWsGroupOpCreate(pGroup);
	if ( pOperation == NULL ) {
		return NULL;
	}
	Context.Wait = Wait;
	if ( !__xrtWsGroupOpSubmit(
		pOperation,
		__xrtWsGroupSubmitWait,
		&Context
	) ) {
		xrtWsGroupOpDestroy(pOperation);
		return NULL;
	}
	return pOperation;
}

#endif
