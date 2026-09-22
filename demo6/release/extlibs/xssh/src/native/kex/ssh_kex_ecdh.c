#include <xrt/ssh_kex_ecdh.h>



#if defined(XSSH_FEATURE_KEX_ECDH)

/* 验证一个可编码为 SSH string 的借用视图。 */
static bool xsshEcdhViewValid(xbytesview Value)
{
	return !((Value.Data == NULL) && (Value.Size != 0u)) &&
		(Value.Size <= UINT32_MAX);
}



/* 计算一组 SSH string 与消息号的总长度。 */
static xsshcode xsshEcdhMeasure(
	const xbytesview* pValues,
	size_t iCount,
	size_t* pTotalSize
)
{
	size_t iTotalSize = 1u;
	size_t i;

	if ( (pValues == NULL) || (pTotalSize == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( !xsshEcdhViewValid(pValues[i]) ) {
			return XSSH_ERROR_ARGUMENT;
		}
		if ( (iTotalSize > (SIZE_MAX - 4u)) ||
			(pValues[i].Size > (SIZE_MAX - iTotalSize - 4u)) ) {
			return XSSH_ERROR_OVERFLOW;
		}
		iTotalSize += 4u + pValues[i].Size;
	}
	*pTotalSize = iTotalSize;
	return XSSH_OK;
}



/* 在容量已验证的 writer 副本中写入一组 SSH string。 */
static xsshcode xsshEcdhWrite(
	xsshwriter* pWriter,
	uint8 iMessage,
	const xbytesview* pValues,
	size_t iCount
)
{
	xsshwriter Writer;
	size_t iTotalSize;
	size_t i;
	xsshcode Code;

	if ( pWriter == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshEcdhMeasure(pValues, iCount, &iTotalSize);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotalSize,
		pValues,
		iCount
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( xrtSshWriteByte(&Writer, iMessage) != XSSH_OK ) {
		return XSSH_ERROR_PROTOCOL;
	}
	for ( i = 0u; i < iCount; ++i ) {
		if ( xrtSshWriteString(&Writer, pValues[i]) != XSSH_OK ) {
			return XSSH_ERROR_PROTOCOL;
		}
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 初始化 reader 并验证消息号。 */
static xsshcode xsshEcdhReader(
	xbytesview Payload,
	uint8 iExpected,
	xsshreader* pReader
)
{
	uint8 iMessage;
	xsshcode Code;

	if ( (pReader == NULL) ||
		((Payload.Data == NULL) && (Payload.Size != 0u)) ||
		!xrtSshReaderInit(pReader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xrtSshReadByte(pReader, &iMessage);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	return iMessage == iExpected ? XSSH_OK : XSSH_ERROR_PROTOCOL;
}



/* 构建客户端临时公钥消息。 */
xsshcode xrtSshEcdhInitWrite(
	xsshwriter* pWriter,
	xbytesview ClientPublic
)
{
	return xsshEcdhWrite(
		pWriter,
		XSSH_MSG_KEX_ECDH_INIT,
		&ClientPublic,
		1u
	);
}



/* 解析客户端临时公钥，并拒绝尾随数据。 */
xsshcode xrtSshEcdhInitRead(
	xbytesview Payload,
	xsshecdhinit* pMessage
)
{
	xsshreader Reader;
	xsshecdhinit Message;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshEcdhReader(Payload, XSSH_MSG_KEX_ECDH_INIT, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xrtSshReadString(&Reader, &Message.ClientPublic);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}



/* 构建服务端 host key、临时公钥和签名消息。 */
xsshcode xrtSshEcdhReplyWrite(
	xsshwriter* pWriter,
	xbytesview ServerHostKey,
	xbytesview ServerPublic,
	xbytesview Signature
)
{
	xbytesview arrValues[3];

	arrValues[0] = ServerHostKey;
	arrValues[1] = ServerPublic;
	arrValues[2] = Signature;
	return xsshEcdhWrite(
		pWriter,
		XSSH_MSG_KEX_ECDH_REPLY,
		arrValues,
		3u
	);
}



/* 解析服务端 ECDH reply，并拒绝尾随数据。 */
xsshcode xrtSshEcdhReplyRead(
	xbytesview Payload,
	xsshecdhreply* pMessage
)
{
	xsshreader Reader;
	xsshecdhreply Message;
	xsshcode Code;

	if ( pMessage == NULL ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshEcdhReader(Payload, XSSH_MSG_KEX_ECDH_REPLY, &Reader);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( ((Code = xrtSshReadString(
		&Reader,
		&Message.ServerHostKey
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Message.ServerPublic
	)) != XSSH_OK) || ((Code = xrtSshReadString(
		&Reader,
		&Message.Signature
	)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pMessage = Message;
	return XSSH_OK;
}

#endif
