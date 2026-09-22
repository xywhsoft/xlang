#ifndef XSSH_CHANNEL_MESSAGE_INTERNAL_H
#define XSSH_CHANNEL_MESSAGE_INTERNAL_H

#include <xrt/ssh_channel_message.h>



/* 将文本视图转换为不改变所有权的字节视图。 */
static inline xbytesview xsshChannelBytes(xstrview Text)
{
	return (xbytesview){ (const unsigned char*)Text.Data, Text.Size };
}



/* 将字节视图转换为不改变所有权的文本视图。 */
static inline xstrview xsshChannelText(xbytesview Value)
{
	return (xstrview){ (const char*)Value.Data, Value.Size };
}



/* 将 SSH string 的编码长度安全加入总长度。 */
static inline xsshcode xsshChannelAddString(
	xbytesview Value,
	size_t* pTotal
)
{
	if ( (pTotal == NULL) || !xrtMemRangeValid(Value.Data, Value.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Value.Size > UINT32_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( (*pTotal > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pTotal - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pTotal += 4u + Value.Size;
	return XSSH_OK;
}



/* 预留整条消息并复制 writer，供事务式构建使用。 */
static inline xsshcode xsshChannelPrepare(
	xsshwriter* pWriter,
	size_t iTotal,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xsshcode Code;

	if ( pWork == NULL ) {
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
	*pWork = *pWriter;
	return XSSH_OK;
}



/* 预留并写入 channel open 公共前缀。 */
static inline xsshcode xsshChannelOpenWriteBegin(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	size_t iFieldsSize,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xbytesview TypeBytes = xsshChannelBytes(Type);
	xbytesview arrInputs[6];
	size_t iTotal = 13u;
	size_t i;
	xsshcode Code;

	if ( (pWork == NULL) || !xrtSshNameValid(Type) ||
		(iMaxPacket == 0u) || (iInputCount > 5u) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	Code = xsshChannelAddString(TypeBytes, &iTotal);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += iFieldsSize;
	arrInputs[0] = TypeBytes;
	for ( i = 0u; i < iInputCount; ++i ) {
		arrInputs[i + 1u] = pInputs[i];
	}
	Code = xsshChannelPrepare(
		pWriter,
		iTotal,
		arrInputs,
		iInputCount + 1u,
		pWork
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteByte(pWork, XSSH_MSG_CHANNEL_OPEN) != XSSH_OK) ||
		(xrtSshWriteString(pWork, TypeBytes) != XSSH_OK) ||
		(xrtSshWriteU32(pWork, iSender) != XSSH_OK) ||
		(xrtSshWriteU32(pWork, iWindow) != XSSH_OK) ||
		(xrtSshWriteU32(pWork, iMaxPacket) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}

#endif
