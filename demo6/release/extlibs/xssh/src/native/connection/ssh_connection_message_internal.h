#ifndef XSSH_CONNECTION_MESSAGE_INTERNAL_H
#define XSSH_CONNECTION_MESSAGE_INTERNAL_H

#include <xrt/ssh_connection_message.h>



/* 预留并写入 global request 公共前缀。 */
static inline xsshcode xsshGlobalRequestWriteBegin(
	xsshwriter* pWriter,
	xstrview Name,
	bool bWantReply,
	size_t iFieldsSize,
	const xbytesview* pInputs,
	size_t iInputCount,
	xsshwriter* pWork
)
{
	xbytesview NameBytes = {
		(const unsigned char*)Name.Data,
		Name.Size
	};
	xbytesview arrInputs[5];
	size_t iTotal;
	size_t i;
	xsshcode Code;

	if ( (pWork == NULL) || !xrtSshNameValid(Name) ||
		(iInputCount > 4u) ||
		((pInputs == NULL) && (iInputCount != 0u)) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( Name.Size > (SIZE_MAX - 6u) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal = 6u + Name.Size;
	if ( iFieldsSize > (SIZE_MAX - iTotal) ) {
		return XSSH_ERROR_OVERFLOW;
	}
	iTotal += iFieldsSize;
	arrInputs[0] = NameBytes;
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
	if ( (xrtSshWriteByte(pWork, XSSH_MSG_GLOBAL_REQUEST) != XSSH_OK) ||
		(xrtSshWriteString(pWork, NameBytes) != XSSH_OK) ||
		(xrtSshWriteBool(pWork, bWantReply) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}

#endif
