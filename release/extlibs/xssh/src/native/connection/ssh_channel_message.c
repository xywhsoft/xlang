#include "ssh_channel_message_internal.h"



#if defined(XSSH_FEATURE_CHANNEL_MESSAGE)

/* 初始化局部 reader，并拒绝输出对象覆盖输入 payload。 */
static xsshcode xsshChannelReadBegin(
	xbytesview Payload,
	void* pOutput,
	size_t iOutputSize,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pOutput == NULL) || (pReader == NULL) ||
		!xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pOutput,
			iOutputSize
		) || !xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 借出 reader 的全部剩余字段。 */
static xsshcode xsshChannelReadFields(
	xsshreader* pReader,
	xbytesview* pFields
)
{
	return xrtSshReadBytes(
		pReader,
		xrtSshReaderRemaining(pReader),
		pFields
	);
}



/* 写入只包含消息号和 recipient channel 的固定消息。 */
static xsshcode xsshChannelSimpleWrite(
	xsshwriter* pWriter,
	uint8 iMessage,
	uint32 iRecipient
)
{
	xsshwriter Writer;
	xsshcode Code = xsshChannelPrepare(
		pWriter,
		5u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, iMessage) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRecipient) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取只包含消息号和 recipient channel 的固定消息。 */
static xsshcode xsshChannelSimpleRead(
	xbytesview Payload,
	uint8 iMessage,
	uint32* pRecipient
)
{
	xsshreader Reader;
	uint32 iRecipient;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pRecipient,
		sizeof(*pRecipient),
		iMessage,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iRecipient);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pRecipient = iRecipient;
	return XSSH_OK;
}



/* 写入可扩展 channel open。 */
xsshcode xrtSshChannelOpenWrite(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelOpenWriteBegin(
		pWriter,
		Type,
		iSender,
		iWindow,
		iMaxPacket,
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



/* 读取 channel open，并保留未知类型专用字段。 */
xsshcode xrtSshChannelOpenRead(
	xbytesview Payload,
	xsshchannelopen* pOpen
)
{
	xsshreader Reader;
	xsshchannelopen Open;
	xbytesview Value;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pOpen,
		sizeof(*pOpen),
		XSSH_MSG_CHANNEL_OPEN,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Open.Type = xsshChannelText(Value);
	if ( !xrtSshNameValid(Open.Type) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Open.Sender)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Open.Window)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Open.MaxPacket)) != XSSH_OK) ||
		((Code = xsshChannelReadFields(&Reader, &Open.Fields)) != XSSH_OK) ) {
		return Code;
	}
	if ( Open.MaxPacket == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pOpen = Open;
	return XSSH_OK;
}



/* 写入 channel open confirmation。 */
xsshcode xrtSshChannelOpenConfirmationWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Fields
)
{
	xsshwriter Writer;
	size_t iTotal;
	xsshcode Code;

	if ( iMaxPacket == 0u ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Fields.Size > (SIZE_MAX - 17u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 17u + Fields.Size;
	Code = xsshChannelPrepare(pWriter, iTotal, &Fields, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_OPEN_CONFIRMATION
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iSender
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iWindow
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iMaxPacket
	) != XSSH_OK) || (xrtSshWriteBytes(
		&Writer,
		Fields
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取 channel open confirmation 与专用字段。 */
xsshcode xrtSshChannelOpenConfirmationRead(
	xbytesview Payload,
	xsshchannelconfirmation* pConfirmation
)
{
	xsshreader Reader;
	xsshchannelconfirmation Confirmation;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pConfirmation,
		sizeof(*pConfirmation),
		XSSH_MSG_CHANNEL_OPEN_CONFIRMATION,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Confirmation.Recipient)) !=
		XSSH_OK) || ((Code = xrtSshReadU32(
		&Reader,
		&Confirmation.Sender
	)) != XSSH_OK) || ((Code = xrtSshReadU32(
		&Reader,
		&Confirmation.Window
	)) != XSSH_OK) || ((Code = xrtSshReadU32(
		&Reader,
		&Confirmation.MaxPacket
	)) != XSSH_OK) || ((Code = xsshChannelReadFields(
		&Reader,
		&Confirmation.Fields
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( Confirmation.MaxPacket == 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pConfirmation = Confirmation;
	return XSSH_OK;
}



/* 写入 UTF-8 channel open failure。 */
xsshcode xrtSshChannelOpenFailureWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iReason,
	xstrview Description,
	xstrview Language
)
{
	xbytesview arrInputs[2] = {
		xsshChannelBytes(Description),
		xsshChannelBytes(Language)
	};
	xsshwriter Writer;
	size_t iTotal = 9u;
	xsshcode Code;

	if ( !xrtUtf8Valid(Description, NULL) ||
		!xrtSshLanguageValid(Language) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xsshChannelAddString(arrInputs[0], &iTotal)) != XSSH_OK) ||
		((Code = xsshChannelAddString(arrInputs[1], &iTotal)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshChannelPrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_OPEN_FAILURE
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iReason
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[0]
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		arrInputs[1]
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取并校验 channel open failure 文本字段。 */
xsshcode xrtSshChannelOpenFailureRead(
	xbytesview Payload,
	xsshchannelopenfailure* pFailure
)
{
	xsshreader Reader;
	xsshchannelopenfailure Failure;
	xbytesview Value;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pFailure,
		sizeof(*pFailure),
		XSSH_MSG_CHANNEL_OPEN_FAILURE,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Failure.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Failure.Reason)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Value)) != XSSH_OK) ) {
		return Code;
	}
	Failure.Description = xsshChannelText(Value);
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Failure.Language = xsshChannelText(Value);
	if ( !xrtUtf8Valid(Failure.Description, NULL) ||
		!xrtSshLanguageValid(Failure.Language) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pFailure = Failure;
	return XSSH_OK;
}



/* 写入 channel window adjust。 */
xsshcode xrtSshChannelWindowAdjustWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iBytes
)
{
	xsshwriter Writer;
	xsshcode Code = xsshChannelPrepare(
		pWriter,
		9u,
		NULL,
		0u,
		&Writer
	);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_WINDOW_ADJUST
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iBytes
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 channel window adjust。 */
xsshcode xrtSshChannelWindowAdjustRead(
	xbytesview Payload,
	xsshchanneladjust* pAdjust
)
{
	xsshreader Reader;
	xsshchanneladjust Adjust;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pAdjust,
		sizeof(*pAdjust),
		XSSH_MSG_CHANNEL_WINDOW_ADJUST,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Adjust.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Adjust.Bytes)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pAdjust = Adjust;
	return XSSH_OK;
}



/* 写入普通 channel data。 */
xsshcode xrtSshChannelDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xbytesview Data
)
{
	xsshwriter Writer;
	size_t iTotal = 5u;
	xsshcode Code = xsshChannelAddString(Data, &iTotal);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshChannelPrepare(pWriter, iTotal, &Data, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_CHANNEL_DATA) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRecipient) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Data) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取普通 channel data。 */
xsshcode xrtSshChannelDataRead(
	xbytesview Payload,
	xsshchanneldata* pData
)
{
	xsshreader Reader;
	xsshchanneldata Data;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pData,
		sizeof(*pData),
		XSSH_MSG_CHANNEL_DATA,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Data.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Data.Data)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pData = Data;
	return XSSH_OK;
}



/* 写入 channel extended data。 */
xsshcode xrtSshChannelExtendedDataWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iType,
	xbytesview Data
)
{
	xsshwriter Writer;
	size_t iTotal = 9u;
	xsshcode Code = xsshChannelAddString(Data, &iTotal);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshChannelPrepare(pWriter, iTotal, &Data, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(
		&Writer,
		XSSH_MSG_CHANNEL_EXTENDED_DATA
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteU32(
		&Writer,
		iType
	) != XSSH_OK) || (xrtSshWriteString(
		&Writer,
		Data
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 channel extended data。 */
xsshcode xrtSshChannelExtendedDataRead(
	xbytesview Payload,
	xsshchannelextendeddata* pData
)
{
	xsshreader Reader;
	xsshchannelextendeddata Data;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pData,
		sizeof(*pData),
		XSSH_MSG_CHANNEL_EXTENDED_DATA,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Data.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Data.Type)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Data.Data)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pData = Data;
	return XSSH_OK;
}



/* 写入 channel EOF。 */
xsshcode xrtSshChannelEofWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_EOF,
		iRecipient
	);
}



/* 严格读取 channel EOF。 */
xsshcode xrtSshChannelEofRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_EOF,
		pRecipient
	);
}



/* 写入 channel close。 */
xsshcode xrtSshChannelCloseWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_CLOSE,
		iRecipient
	);
}



/* 严格读取 channel close。 */
xsshcode xrtSshChannelCloseRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_CLOSE,
		pRecipient
	);
}



/* 写入保留未知专用字段的 channel request。 */
xsshcode xrtSshChannelRequestWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	xbytesview Fields
)
{
	xbytesview arrInputs[2] = { xsshChannelBytes(Type), Fields };
	xsshwriter Writer;
	size_t iTotal = 6u;
	xsshcode Code;

	if ( !xrtSshNameValid(Type) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelAddString(arrInputs[0], &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( !xrtMemRangeValid(Fields.Data, Fields.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Fields.Size > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += Fields.Size;
	Code = xsshChannelPrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_CHANNEL_REQUEST) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iRecipient) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bWantReply) != XSSH_OK) ||
		(xrtSshWriteBytes(&Writer, Fields) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取 channel request，并借出全部未知专用字段。 */
xsshcode xrtSshChannelRequestRead(
	xbytesview Payload,
	xsshchannelrequest* pRequest
)
{
	xsshreader Reader;
	xsshchannelrequest Request;
	xbytesview Value;
	xsshcode Code;

	Code = xsshChannelReadBegin(
		Payload,
		pRequest,
		sizeof(*pRequest),
		XSSH_MSG_CHANNEL_REQUEST,
		&Reader
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(&Reader, &Request.Recipient)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Value)) != XSSH_OK) ) {
		return Code;
	}
	Request.Type = xsshChannelText(Value);
	if ( !xrtSshNameValid(Request.Type) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	if ( ((Code = xrtSshReadBool(&Reader, &Request.WantReply)) != XSSH_OK) ||
		((Code = xsshChannelReadFields(&Reader, &Request.Fields)) != XSSH_OK) ) {
		return Code;
	}
	*pRequest = Request;
	return XSSH_OK;
}



/* 写入 channel request success。 */
xsshcode xrtSshChannelSuccessWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_SUCCESS,
		iRecipient
	);
}



/* 严格读取 channel request success。 */
xsshcode xrtSshChannelSuccessRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_SUCCESS,
		pRecipient
	);
}



/* 写入 channel request failure。 */
xsshcode xrtSshChannelFailureWrite(xsshwriter* pWriter, uint32 iRecipient)
{
	return xsshChannelSimpleWrite(
		pWriter,
		XSSH_MSG_CHANNEL_FAILURE,
		iRecipient
	);
}



/* 严格读取 channel request failure。 */
xsshcode xrtSshChannelFailureRead(xbytesview Payload, uint32* pRecipient)
{
	return xsshChannelSimpleRead(
		Payload,
		XSSH_MSG_CHANNEL_FAILURE,
		pRecipient
	);
}

#endif
