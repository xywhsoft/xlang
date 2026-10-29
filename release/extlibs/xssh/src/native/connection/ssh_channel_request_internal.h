#ifndef XSSH_CHANNEL_REQUEST_INTERNAL_H
#define XSSH_CHANNEL_REQUEST_INTERNAL_H

#include <string.h>

#include <xrt/ssh_channel_request.h>



/* 将文本视图转换为不改变所有权的字节视图。 */
static inline xbytesview xsshRequestBytes(xstrview Text)
{
	return (xbytesview){ (const unsigned char*)Text.Data, Text.Size };
}



/* 将字节视图转换为不改变所有权的文本视图。 */
static inline xstrview xsshRequestText(xbytesview Value)
{
	return (xstrview){ (const char*)Value.Data, Value.Size };
}



/* 比较 request 类型与编译期常量。 */
static inline bool xsshRequestTypeEqual(
	xstrview Type,
	const char* pExpected,
	size_t iExpected
)
{
	return (Type.Size == iExpected) &&
		(memcmp(Type.Data, pExpected, iExpected) == 0);
}



/* 将一个 SSH string 安全加入专用字段长度。 */
static inline xsshcode xsshRequestAddString(
	xbytesview Value,
	size_t* pSize
)
{
	if ( (pSize == NULL) || !xrtMemRangeValid(Value.Data, Value.Size) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Value.Size > UINT32_MAX ) {
		return XSSH_ERROR_OVERFLOW;
	}
	if ( (*pSize > (SIZE_MAX - 4u)) ||
		(Value.Size > (SIZE_MAX - *pSize - 4u)) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	*pSize += 4u + Value.Size;
	return XSSH_OK;
}



/* 预留完整 request 并写入公共前缀。 */
static inline xsshcode xsshRequestWriteBegin(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Type,
	bool bWantReply,
	size_t iFieldsSize,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xbytesview TypeBytes = xsshRequestBytes(Type);
	xbytesview arrInputs[5];
	size_t iTotal;
	size_t i;
	xsshcode Code;

	if ( (pWork == NULL) || !xrtSshNameValid(Type) ||
		(iInputCount > 4u) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Type.Size > (SIZE_MAX - 10u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 10u + Type.Size;
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += iFieldsSize;
	arrInputs[0] = TypeBytes;
	for ( i = 0u; i < iInputCount; ++i ) {
		arrInputs[i + 1u] = pInputs[i];
	}
	Code = xrtSshWriterReserveInputs(
		pWriter,
		iTotal,
		arrInputs,
		iInputCount + 1u
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	*pWork = *pWriter;
	if ( (xrtSshWriteByte(
		pWork,
		XSSH_MSG_CHANNEL_REQUEST
	) != XSSH_OK) || (xrtSshWriteU32(
		pWork,
		iRecipient
	) != XSSH_OK) || (xrtSshWriteString(
		pWork,
		TypeBytes
	) != XSSH_OK) || (xrtSshWriteBool(
		pWork,
		bWantReply
	) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 校验 request 类型、回复标志和输出重叠后初始化字段 reader。 */
static inline xsshcode xsshRequestReadBegin(
	const xsshchannelrequest* pRequest,
	const char* pType,
	size_t iTypeSize,
	int iWantReply,
	void* pOutput,
	size_t iOutputSize,
	xsshreader* pReader
)
{
	if ( (pRequest == NULL) || (pType == NULL) || (pReader == NULL) ||
		((pOutput == NULL) && (iOutputSize != 0u)) ||
		!xrtSshNameValid(pRequest->Type) ||
		!xrtMemRangeValid(pRequest->Fields.Data, pRequest->Fields.Size) ||
		!xsshRequestTypeEqual(pRequest->Type, pType, iTypeSize) ||
		((iWantReply >= 0) &&
		 (pRequest->WantReply != (iWantReply != 0))) ||
		xrtMemRangesOverlap(
			pRequest->Fields.Data,
			pRequest->Fields.Size,
			pOutput,
			iOutputSize
		) || !xrtSshReaderInit(pReader, pRequest->Fields) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	return XSSH_OK;
}



/* 确认专用字段 reader 已经严格消费到末尾。 */
static inline xsshcode xsshRequestReadEnd(const xsshreader* pReader)
{
	return xrtSshReaderRemaining(pReader) == 0u ?
		XSSH_OK : XSSH_ERROR_PROTOCOL;
}

#endif
