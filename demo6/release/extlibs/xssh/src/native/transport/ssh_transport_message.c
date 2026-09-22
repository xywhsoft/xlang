#include <xrt/ssh_transport_message.h>



#if defined(XSSH_FEATURE_TRANSPORT_MESSAGE)

/* 转换字节视图为不要求零结尾的文本视图。 */
static xstrview xsshMessageText(xbytesview Value)
{
	xstrview Text;

	Text.Data = (const char*)Value.Data;
	Text.Size = Value.Size;
	return Text;
}



/* 转换文本视图为原始字节视图。 */
static xbytesview xsshMessageBytes(xstrview Value)
{
	xbytesview Bytes;

	Bytes.Data = (const unsigned char*)Value.Data;
	Bytes.Size = Value.Size;
	return Bytes;
}



/* 验证 string 视图并向总长度加入四字节前缀。 */
static xsshcode xsshMessageAddString(xbytesview Value, size_t* pTotal)
{
	if ( (pTotal == NULL) ||
		((Value.Data == NULL) && (Value.Size != 0u)) ||
		(Value.Size > UINT32_MAX) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 验证 writer、容量和输入重叠，返回可安全提交的副本。 */
static xsshcode xsshMessagePrepare(
	xsshwriter* pWriter,
	size_t iTotal,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pCopy
)
{
	xsshwriter Writer;
	xsshcode Code;

	if ( (pWriter == NULL) || (pCopy == NULL) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		pInputs,
		iInputCount
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	*pCopy = Writer;
	return XSSH_OK;
}



/* 初始化 reader 并消费期望的消息号。 */
static xsshcode xsshMessageReader(
	xbytesview Payload,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pReader == NULL) || !xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 写入只有消息号的 payload。 */
static xsshcode xsshMessageWriteEmpty(
	xsshwriter* pWriter,
	uint8 iMessage
)
{
	xsshwriter Writer;
	xsshcode Code;

	Code = xsshMessagePrepare(pWriter, 1u, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshWriteByte(&Writer, iMessage) != XSSH_OK ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取只有消息号的 payload。 */
static xsshcode xsshMessageReadEmpty(
	xbytesview Payload,
	uint8 iMessage
)
{
	xsshreader Reader;
	xsshcode Code;

	Code = xsshMessageReader(Payload, iMessage, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return xrtSshReaderRemaining(&Reader) == 0u ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 返回 payload 的第一个消息号。 */
xsshcode xrtSshMessageType(xbytesview Payload, uint8* pMessage)
{
	xsshreader Reader;
	uint8 iMessage;
	xsshcode Code;

	if ( (pMessage == NULL) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(&Reader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pMessage = iMessage;
	return XSSH_OK;
}



/* 写入 NEWKEYS。 */
xsshcode xrtSshNewKeysWrite(xsshwriter* pWriter)
{
	return xsshMessageWriteEmpty(pWriter, XSSH_MSG_NEWKEYS);
}



/* 严格读取 NEWKEYS。 */
xsshcode xrtSshNewKeysRead(xbytesview Payload)
{
	return xsshMessageReadEmpty(Payload, XSSH_MSG_NEWKEYS);
}



/* 写入 disconnect reason 与诊断文本。 */
xsshcode xrtSshDisconnectWrite(
	xsshwriter* pWriter,
	uint32 iReason,
	xstrview Description,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 1u + 4u;
	xsshcode Code;

	arrInputs[0] = xsshMessageBytes(Description);
	arrInputs[1] = xsshMessageBytes(Language);
	if ( ((Code = xsshMessageAddString(
		arrInputs[0],
		&iTotal
	)) != XSSH_OK) || ((Code = xsshMessageAddString(
		arrInputs[1],
		&iTotal
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_DISCONNECT) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iReason) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 disconnect payload。 */
xsshcode xrtSshDisconnectRead(
	xbytesview Payload,
	xsshdisconnect* pMessage
)
{
	xsshreader Reader;
	xsshdisconnect Message;
	xbytesview Description;
	xbytesview Language;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_DISCONNECT, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadU32(
		&Reader,
		&Message.Reason
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Description
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Language
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	Message.Description = xsshMessageText(Description);
	Message.Language = xsshMessageText(Language);
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入可忽略的任意二进制数据。 */
xsshcode xrtSshIgnoreWrite(xsshwriter* pWriter, xbytesview Data)
{
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	Code = xsshMessageAddString(Data, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, &Data, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_IGNORE) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Data) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 ignore payload。 */
xsshcode xrtSshIgnoreRead(xbytesview Payload, xsshignore* pMessage)
{
	xsshreader Reader;
	xsshignore Message;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_IGNORE, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Message.Data);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入无法实现消息对应的入站序列号。 */
xsshcode xrtSshUnimplementedWrite(
	xsshwriter* pWriter,
	uint32 iSequence
)
{
	xsshwriter Writer;
	xsshcode Code;

	Code = xsshMessagePrepare(pWriter, 5u, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_UNIMPLEMENTED) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iSequence) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 unimplemented 序列号。 */
xsshcode xrtSshUnimplementedRead(
	xbytesview Payload,
	uint32* pSequence
)
{
	xsshreader Reader;
	uint32 iSequence;
	xsshcode Code;

	if ( pSequence == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_UNIMPLEMENTED, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &iSequence);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pSequence = iSequence;
	return XSSH_OK;
}



/* 写入 debug 显示策略与诊断文本。 */
xsshcode xrtSshDebugWrite(
	xsshwriter* pWriter,
	bool bAlwaysDisplay,
	xstrview Message,
	xstrview Language
)
{
	xbytesview arrInputs[2];
	xsshwriter Writer;
	size_t iTotal = 2u;
	xsshcode Code;

	arrInputs[0] = xsshMessageBytes(Message);
	arrInputs[1] = xsshMessageBytes(Language);
	if ( ((Code = xsshMessageAddString(
		arrInputs[0],
		&iTotal
	)) != XSSH_OK) || ((Code = xsshMessageAddString(
		arrInputs[1],
		&iTotal
	)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, arrInputs, 2u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_DEBUG) != XSSH_OK) ||
		(xrtSshWriteBool(&Writer, bAlwaysDisplay) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[0]) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, arrInputs[1]) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 debug payload。 */
xsshcode xrtSshDebugRead(xbytesview Payload, xsshdebug* pMessage)
{
	xsshreader Reader;
	xsshdebug Message;
	xbytesview Text;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_DEBUG, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadBool(&Reader, &Message.AlwaysDisplay);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Text);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Message.Message = xsshMessageText(Text);
	Code = xrtSshReadString(&Reader, &Text);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Message.Language = xsshMessageText(Text);
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入 request 或 accept 共用的 service payload。 */
static xsshcode xsshServiceWrite(
	xsshwriter* pWriter,
	uint8 iMessage,
	xstrview Service
)
{
	xbytesview Value = xsshMessageBytes(Service);
	xsshwriter Writer;
	size_t iTotal = 1u;
	xsshcode Code;

	if ( !xrtSshNameValid(Service) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageAddString(Value, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshMessagePrepare(pWriter, iTotal, &Value, 1u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(&Writer, iMessage) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Value) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 读取并校验 request 或 accept 共用的 service payload。 */
static xsshcode xsshServiceRead(
	xbytesview Payload,
	uint8 iMessage,
	xsshservice* pMessage
)
{
	xsshreader Reader;
	xsshservice Message;
	xbytesview Value;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, iMessage, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Value);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Message.Name = xsshMessageText(Value);
	if ( !xrtSshNameValid(Message.Name) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 写入 service request。 */
xsshcode xrtSshServiceRequestWrite(
	xsshwriter* pWriter,
	xstrview Service
)
{
	return xsshServiceWrite(pWriter, XSSH_MSG_SERVICE_REQUEST, Service);
}



/* 读取 service request。 */
xsshcode xrtSshServiceRequestRead(
	xbytesview Payload,
	xsshservice* pMessage
)
{
	return xsshServiceRead(Payload, XSSH_MSG_SERVICE_REQUEST, pMessage);
}



/* 写入 service accept。 */
xsshcode xrtSshServiceAcceptWrite(
	xsshwriter* pWriter,
	xstrview Service
)
{
	return xsshServiceWrite(pWriter, XSSH_MSG_SERVICE_ACCEPT, Service);
}



/* 读取 service accept。 */
xsshcode xrtSshServiceAcceptRead(
	xbytesview Payload,
	xsshservice* pMessage
)
{
	return xsshServiceRead(Payload, XSSH_MSG_SERVICE_ACCEPT, pMessage);
}



/* 写入数组形式的 extension-info，值保持任意二进制。 */
xsshcode xrtSshExtInfoWrite(
	xsshwriter* pWriter,
	const xsshextension* pExtensions,
	size_t iCount
)
{
	xsshwriter Writer;
	size_t iTotal = 1u + 4u;
	size_t i;
	xsshcode Code;

	if ( (iCount > UINT32_MAX) ||
		(iCount > (SIZE_MAX / sizeof(*pExtensions))) ||
		((pExtensions == NULL) && (iCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xrtSshNameValid(pExtensions[i].Name) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		Code = xsshMessageAddString(
			xsshMessageBytes(pExtensions[i].Name),
			&iTotal
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		Code = xsshMessageAddString(pExtensions[i].Value, &iTotal);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	Code = xsshMessagePrepare(pWriter, iTotal, NULL, 0u, &Writer);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtMemRangesOverlap(
		Writer.Data + Writer.Size,
		iTotal,
		pExtensions,
		iCount * sizeof(*pExtensions)
	) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( xrtMemRangesOverlap(
				Writer.Data + Writer.Size,
				iTotal,
				pExtensions[i].Name.Data,
				pExtensions[i].Name.Size
			) || xrtMemRangesOverlap(
				Writer.Data + Writer.Size,
				iTotal,
				pExtensions[i].Value.Data,
				pExtensions[i].Value.Size
			) ) {
			return XSSH_ERROR_ARGUMENT;
		}
	}
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_EXT_INFO) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, (uint32)iCount) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( (xrtSshWriteString(
			&Writer,
			xsshMessageBytes(pExtensions[i].Name)
		) != XSSH_OK) || (xrtSshWriteString(
			&Writer,
			pExtensions[i].Value
		) != XSSH_OK) ) {
			return XSSH_ERROR_STATE;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 预验证完整 EXT_INFO 后初始化无分配迭代器。 */
xsshcode xrtSshExtInfoRead(
	xbytesview Payload,
	xsshextinfo* pExtInfo
)
{
	xsshreader Reader;
	xsshreader Items;
	xsshextinfo ExtInfo;
	xbytesview Name;
	xbytesview Value;
	uint32 i;
	xsshcode Code;

	if ( pExtInfo == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshMessageReader(Payload, XSSH_MSG_EXT_INFO, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadU32(&Reader, &ExtInfo.Count);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (size_t)ExtInfo.Count >
		(xrtSshReaderRemaining(&Reader) / 8u) ) {
		return XSSH_NEED_MORE;
	}
	Items = Reader;
	for ( i = 0u; i < ExtInfo.Count; ++i ) {
		Code = xrtSshReadString(&Reader, &Name);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		if ( !xrtSshNameValid(xsshMessageText(Name)) ) {
			return XSSH_ERROR_PROTOCOL;
		}
		Code = xrtSshReadString(&Reader, &Value);
		if ( Code != XSSH_OK ) {
			return Code;
		}
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	ExtInfo.Index = 0u;
	ExtInfo.Reader = Items;
	*pExtInfo = ExtInfo;
	return XSSH_OK;
}



/* 从已验证 EXT_INFO 中返回下一项。 */
bool xrtSshExtInfoNext(
	xsshextinfo* pExtInfo,
	xsshextension* pExtension
)
{
	xsshextinfo ExtInfo;
	xsshextension Extension;
	xbytesview Name;

	if ( (pExtInfo == NULL) || (pExtension == NULL) ||
		(pExtInfo->Index >= pExtInfo->Count) ) {
		return false;
	}
	ExtInfo = *pExtInfo;
	if ( (xrtSshReadString(&ExtInfo.Reader, &Name) != XSSH_OK) ||
		(xrtSshReadString(
			&ExtInfo.Reader,
			&Extension.Value
		) != XSSH_OK) ) {
		return false;
	}
	Extension.Name = xsshMessageText(Name);
	++ExtInfo.Index;
	*pExtInfo = ExtInfo;
	*pExtension = Extension;
	return true;
}



/* 写入 delayed compression 激活消息。 */
xsshcode xrtSshNewCompressWrite(xsshwriter* pWriter)
{
	return xsshMessageWriteEmpty(pWriter, XSSH_MSG_NEWCOMPRESS);
}



/* 严格读取 delayed compression 激活消息。 */
xsshcode xrtSshNewCompressRead(xbytesview Payload)
{
	return xsshMessageReadEmpty(Payload, XSSH_MSG_NEWCOMPRESS);
}

#endif
