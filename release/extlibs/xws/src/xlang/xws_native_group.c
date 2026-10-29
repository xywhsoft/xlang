#include "xws_native_internal.h"



typedef enum xws_native_group_command_kind {
	XWS_NATIVE_GROUP_SEND = 1,
	XWS_NATIVE_GROUP_PING,
	XWS_NATIVE_GROUP_PONG,
	XWS_NATIVE_GROUP_CLOSE,
	XWS_NATIVE_GROUP_WAIT
} xws_native_group_command_kind;



/* 创建一个线程安全连接组；零上限表示不限制成员数。 */
XLANG_XWS_EXPORT void* xws_group_create(long long iLimit)
{
	if ( iLimit < 0 ) {
		return NULL;
	}
	return xrtWsGroupCreate((size_t)iLimit);
}



/* point 构造直接接管一份 Group 引用。 */
XLANG_XWS_EXPORT void* xws_group_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放连接组引用；组本身不会关闭成员连接。 */
XLANG_XWS_EXPORT void xws_group_destroy(void* pHandle)
{
	xrtWsGroupDestroy((xwsgroup*)pHandle);
}



/* 加入或移除一个连接；临时原生引用覆盖整个调用。 */
static bool xwsNativeGroupMember(
	void* pGroupHandle,
	void* pConnectionHandle,
	long long iOperation
)
{
	xwsgroup* pGroup = (xwsgroup*)pGroupHandle;
	xwsconn* pConnection = xwsNativeConnectionRef(pConnectionHandle);
	bool bResult = false;

	if ( (pGroup != NULL) && (pConnection != NULL) ) {
		if ( iOperation == 0 ) {
			bResult = xrtWsGroupAdd(pGroup, pConnection);
		} else if ( iOperation == 1 ) {
			bResult = xrtWsGroupRemove(pGroup, pConnection);
		} else {
			bResult = xrtWsGroupHas(pGroup, pConnection);
		}
	}
	xrtWsConnDestroy(pConnection);
	return bResult;
}



/* 加入连接；重复加入保持成功且不增加第二份引用。 */
XLANG_XWS_EXPORT bool xws_group_add(void* pGroup, void* pConnection)
{
	return xwsNativeGroupMember(pGroup, pConnection, 0);
}



/* 移除连接；不存在时返回 false。 */
XLANG_XWS_EXPORT bool xws_group_remove(void* pGroup, void* pConnection)
{
	return xwsNativeGroupMember(pGroup, pConnection, 1);
}



/* 查询连接是否属于当前组。 */
XLANG_XWS_EXPORT bool xws_group_has(void* pGroup, void* pConnection)
{
	return xwsNativeGroupMember(pGroup, pConnection, 2);
}



/* 返回成员数、容量上限或封闭状态。 */
XLANG_XWS_EXPORT uint64 xws_group_value(void* pHandle, long long iField)
{
	xwsgroup* pGroup = (xwsgroup*)pHandle;

	if ( pGroup == NULL ) {
		return 0u;
	}
	if ( iField == 0 ) {
		return xrtWsGroupCount(pGroup);
	}
	if ( iField == 1 ) {
		return xrtWsGroupLimit(pGroup);
	}
	return xrtWsGroupSealed(pGroup) ? 1u : 0u;
}



/* 永久封闭连接组，阻止新成员加入。 */
XLANG_XWS_EXPORT bool xws_group_seal(void* pHandle)
{
	return (pHandle != NULL) && xrtWsGroupSeal((xwsgroup*)pHandle);
}



/* 移除并释放全部成员引用，但不关闭连接。 */
XLANG_XWS_EXPORT uint64 xws_group_clear(void* pHandle)
{
	return pHandle != NULL ? xrtWsGroupClear((xwsgroup*)pHandle) : 0u;
}



/* 创建独立持有连接引用的稳定快照。 */
XLANG_XWS_EXPORT void* xws_group_snapshot(void* pHandle)
{
	return pHandle != NULL ?
		xrtWsGroupSnapshotCreate((xwsgroup*)pHandle) : NULL;
}



/* point 构造直接接管一份 Snapshot 引用。 */
XLANG_XWS_EXPORT void* xws_group_snapshot_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放快照及其持有的连接引用。 */
XLANG_XWS_EXPORT void xws_group_snapshot_destroy(void* pHandle)
{
	xrtWsGroupSnapshotDestroy((xwsgroupsnapshot*)pHandle);
}



/* 返回稳定快照成员数。 */
XLANG_XWS_EXPORT uint64 xws_group_snapshot_count(void* pHandle)
{
	return pHandle != NULL ?
		xrtWsGroupSnapshotCount((xwsgroupsnapshot*)pHandle) : 0u;
}



/* 返回只在快照生命周期内有效的底层借用指针。 */
XLANG_XWS_EXPORT void* xws_group_snapshot_native(void* pHandle, long long iIndex)
{
	if ( (pHandle == NULL) || (iIndex < 0) ) {
		return NULL;
	}
	return xrtWsGroupSnapshotGet(
		(xwsgroupsnapshot*)pHandle,
		(size_t)iIndex
	);
}



/* 对稳定成员快照提交一次批量异步操作。 */
XLANG_XWS_EXPORT void* xws_group_operation(
	void* pHandle,
	long long iKind,
	long long iOpcode,
	void* pData,
	size_t iSize,
	long long iValue
)
{
	xwsgroup* pGroup = (xwsgroup*)pHandle;
	xbytesview Data = { (cbytes)pData, iSize };

	if ( (pGroup == NULL) || ((pData == NULL) && (iSize != 0u)) ) {
		return NULL;
	}
	switch ( (xws_native_group_command_kind)iKind ) {
		case XWS_NATIVE_GROUP_SEND:
			if ( (iOpcode != XWS_OPCODE_TEXT) &&
				 (iOpcode != XWS_OPCODE_BINARY) ) {
				return NULL;
			}
			return xrtWsGroupSendAsync(pGroup, (xwsopcode)iOpcode, Data);

		case XWS_NATIVE_GROUP_PING:
			return xrtWsGroupPingAsync(pGroup, Data);

		case XWS_NATIVE_GROUP_PONG:
			return xrtWsGroupPongAsync(pGroup, Data);

		case XWS_NATIVE_GROUP_CLOSE:
			if ( (iValue < 0) || (iValue > UINT16_MAX) ) {
				return NULL;
			}
			return xrtWsGroupCloseAsync(
				pGroup,
				(uint16)iValue,
				(xstrview){ (cstr)pData, iSize }
			);

		case XWS_NATIVE_GROUP_WAIT:
			if ( (iValue < XWS_CONN_WAIT_WRITE) ||
				 (iValue > XWS_CONN_WAIT_CLOSE) ) {
				return NULL;
			}
			return xrtWsGroupWaitAsync(pGroup, (xwsconnwait)iValue);
	}
	return NULL;
}



/* point 构造直接接管一份批量操作引用。 */
/* 文本批量发送便利入口保留字符串 ABI。 */
XLANG_XWS_EXPORT void* xws_group_text(
	void* pHandle,
	const char* sText,
	size_t iSize
)
{
	return xws_group_operation(
		pHandle,
		XWS_NATIVE_GROUP_SEND,
		XWS_OPCODE_TEXT,
		(void*)sText,
		iSize,
		0
	);
}



/* 关闭便利入口按字符串 ABI 提交原因文本。 */
XLANG_XWS_EXPORT void* xws_group_close(
	void* pHandle,
	long long iCode,
	const char* sReason,
	size_t iSize
)
{
	return xws_group_operation(
		pHandle,
		XWS_NATIVE_GROUP_CLOSE,
		0,
		(void*)sReason,
		iSize,
		iCode
	);
}



XLANG_XWS_EXPORT void* xws_group_op_wrap(void* pHandle)
{
	return pHandle;
}



/* 释放批量操作，不隐式取消已经提交的成员 Future。 */
XLANG_XWS_EXPORT void xws_group_op_destroy(void* pHandle)
{
	xrtWsGroupOpDestroy((xwsgroupop*)pHandle);
}



/* 返回批量操作的总数、受理数、拒绝数或完成数。 */
XLANG_XWS_EXPORT uint64 xws_group_op_value(void* pHandle, long long iField)
{
	xwsgroupop* pOperation = (xwsgroupop*)pHandle;

	if ( pOperation == NULL ) {
		return 0u;
	}
	if ( iField == 0 ) { return xrtWsGroupOpCount(pOperation); }
	if ( iField == 1 ) { return xrtWsGroupOpAccepted(pOperation); }
	if ( iField == 2 ) { return xrtWsGroupOpRejected(pOperation); }
	return xrtWsGroupOpDoneCount(pOperation);
}



/* 读取逐成员状态、连接存在性和结构化错误整数。 */
XLANG_XWS_EXPORT long long xws_group_op_result_value(
	void* pHandle,
	long long iIndex,
	long long iField
)
{
	xwsgroupopresult Result;

	if ( (pHandle == NULL) || (iIndex < 0) ||
		 !xrtWsGroupOpResult(
			(xwsgroupop*)pHandle,
			(size_t)iIndex,
			&Result
		 ) ) {
		return -1;
	}
	if ( iField == 0 ) { return Result.State; }
	if ( iField == 1 ) { return Result.Connection != NULL ? 1 : 0; }
	if ( iField == 2 ) {
		return Result.Error != NULL ? xrtErrorKind(Result.Error) : 0;
	}
	return Result.Error != NULL ? xrtErrorCode(Result.Error) : 0;
}



/* 复制逐成员失败原因，避免借用 Operation 内部错误对象。 */
XLANG_XWS_EXPORT void* xws_group_op_result_error(
	void* pHandle,
	long long iIndex
)
{
	xwsgroupopresult Result;
	cstr sMessage = "";

	if ( (pHandle != NULL) && (iIndex >= 0) &&
		 xrtWsGroupOpResult(
			(xwsgroupop*)pHandle,
			(size_t)iIndex,
			&Result
		 ) && (Result.Error != NULL) ) {
		sMessage = xrtErrorMessage(Result.Error);
		if ( sMessage == NULL ) {
			sMessage = "";
		}
	}
	return xwsNativeText((xstrview){ sMessage, strlen(sMessage) });
}



/* 返回增加引用后的逐成员 Future。 */
XLANG_XWS_EXPORT void* xws_group_op_item_future(
	void* pHandle,
	long long iIndex
)
{
	if ( (pHandle == NULL) || (iIndex < 0) ) {
		return NULL;
	}
	return xrtWsGroupOpItemFutureRef(
		(xwsgroupop*)pHandle,
		(size_t)iIndex
	);
}



/* 返回增加引用后的聚合 Future。 */
XLANG_XWS_EXPORT void* xws_group_op_future(void* pHandle)
{
	return pHandle != NULL ?
		xrtWsGroupOpFutureRef((xwsgroupop*)pHandle) : NULL;
}



/* 向全部尚未结束的逐成员 Future 请求协作取消。 */
XLANG_XWS_EXPORT uint64 xws_group_op_cancel(void* pHandle)
{
	return pHandle != NULL ?
		xrtWsGroupOpCancel((xwsgroupop*)pHandle) : 0u;
}



/* 以毫秒为语言层单位等待批量操作；负值表示无限等待。 */
XLANG_XWS_EXPORT long long xws_group_op_wait(
	void* pHandle,
	long long iTimeoutMs
)
{
	uint64 iTimeout;

	if ( pHandle == NULL ) {
		return XWAIT_ERROR;
	}
	if ( iTimeoutMs < 0 ) {
		return xrtWsGroupOpWait((xwsgroupop*)pHandle);
	}
	iTimeout = (uint64)iTimeoutMs;
	return xrtWsGroupOpWaitFor((xwsgroupop*)pHandle, iTimeout);
}
