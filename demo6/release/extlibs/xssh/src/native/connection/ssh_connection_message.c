#include "ssh_connection_message_internal.h"



#if defined(XSSH_FEATURE_CONNECTION_MESSAGE)

/* 写入可扩展全局请求并一次发布 writer。 */
xsshcode xrtSshGlobalRequestWrite(
	xsshwriter* pWriter,
	xstrview Name,
	bool bWantReply,
	xbytesview Fields
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshGlobalRequestWriteBegin(
		pWriter,
		Name,
		bWantReply,
		Fields.Size,
		&Fields,
		1u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteBytes(&Writer, Fields) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取请求前缀并把剩余专用字段完整借出。 */
xsshcode xrtSshGlobalRequestRead(
	xbytesview Payload,
	xsshglobalrequest* pRequest
)
{
	xsshreader Reader;
	xsshglobalrequest Request;
	xbytesview Value;
	uint8 iMessage;
	xsshcode Code;

	if ( (pRequest == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_GLOBAL_REQUEST ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Request.Name = (xstrview){ (const char*)Value.Data, Value.Size };
	if ( !xrtSshNameValid(Request.Name) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBool(&Reader, &Request.WantReply);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&Request.Fields
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pRequest = Request;
	return XSSH_OK;
}



/* 写入带任意专用数据的全局请求成功响应。 */
xsshcode xrtSshGlobalSuccessWrite(
	xsshwriter* pWriter,
	xbytesview Fields
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( Fields.Size == SIZE_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	Code = xrtSshWriterReserveInputs(pWriter, 1u + Fields.Size, &Fields, 1u);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_REQUEST_SUCCESS
	) != XSSH_OK) || (xrtSshWriteBytes(
		&Writer,
		Fields
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取成功响应并借用全部请求专用数据。 */
xsshcode xrtSshGlobalSuccessRead(
	xbytesview Payload,
	xbytesview* pFields
)
{
	xsshreader Reader;
	xbytesview Fields;
	uint8 iMessage;
	xsshcode Code;

	if ( (pFields == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iMessage != XSSH_MSG_REQUEST_SUCCESS ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Code = xrtSshReadBytes(
		&Reader,
		xrtSshReaderRemaining(&Reader),
		&Fields
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pFields = Fields;
	return XSSH_OK;
}



/* 写入无字段全局请求失败响应。 */
xsshcode xrtSshGlobalFailureWrite(xsshwriter* pWriter)
{
	xsshwriter Writer;
	xsshcode Code = xrtSshWriterReserve(pWriter, 1u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteByte(&Writer, XSSH_MSG_REQUEST_FAILURE) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取无字段全局请求失败响应。 */
xsshcode xrtSshGlobalFailureRead(xbytesview Payload)
{
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Payload.Size == 0u ) {
		return XSSH_NEED_MORE;
	}
	return (Payload.Size == 1u) &&
		(Payload.Data[0] == XSSH_MSG_REQUEST_FAILURE) ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}

#endif
