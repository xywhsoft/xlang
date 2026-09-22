#include <string.h>

#include "ssh_channel_message_internal.h"
#include "ssh_connection_message_internal.h"

#include <xrt/ssh_forward_message.h>



#if defined(XSSH_FEATURE_FORWARD_MESSAGE)

/* 比较借用名称与编译期 request/channel 类型。 */
static bool xsshForwardNameEqual(
	xstrview Name,
	const char* pExpected,
	size_t iExpected
)
{
	return (Name.Size == iExpected) &&
		(memcmp(Name.Data, pExpected, iExpected) == 0);
}



/* 写入包含地址和端口的 remote forwarding 全局请求。 */
static xsshcode xsshForwardGlobalWrite(
	xsshwriter* pWriter,
	xstrview Name,
	xbytesview Address,
	uint32 iPort
)
{
	xsshwriter Writer;
	size_t iFieldsSize = 4u;
	xsshcode Code = xsshChannelAddString(Address, &iFieldsSize);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = xsshGlobalRequestWriteBegin(
		pWriter,
		Name,
		true,
		iFieldsSize,
		&Address,
		1u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Address) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPort) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 remote forwarding 全局请求字段。 */
static xsshcode xsshForwardGlobalRead(
	const xsshglobalrequest* pRequest,
	const char* pName,
	size_t iNameSize,
	xsshtcpipforward* pForward
)
{
	xsshreader Reader;
	xsshtcpipforward Forward;
	xsshcode Code;

	if ( (pRequest == NULL) || (pForward == NULL) ||
		!pRequest->WantReply || !xrtSshNameValid(pRequest->Name) ||
		!xsshForwardNameEqual(pRequest->Name, pName, iNameSize) ||
		!xrtMemRangeValid(pRequest->Fields.Data, pRequest->Fields.Size) ||
		xrtMemRangesOverlap(
			pRequest->Fields.Data,
			pRequest->Fields.Size,
			pForward,
			sizeof(*pForward)
		) || !xrtSshReaderInit(&Reader, pRequest->Fields) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Forward.Address)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Forward.Port)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pForward = Forward;
	return XSSH_OK;
}



/* 写入 direct/forwarded 共用的 TCP/IP channel open。 */
static xsshcode xsshForwardOpenWrite(
	xsshwriter* pWriter,
	xstrview Type,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
)
{
	xbytesview arrInputs[2] = { Host, Originator };
	xsshwriter Writer;
	size_t iFieldsSize = 8u;
	xsshcode Code;

	if ( ((Code = xsshChannelAddString(Host, &iFieldsSize)) != XSSH_OK) ||
		((Code = xsshChannelAddString(Originator, &iFieldsSize)) != XSSH_OK) ) {
		return Code;
	}
	Code = xsshChannelOpenWriteBegin(
		pWriter,
		Type,
		iSender,
		iWindow,
		iMaxPacket,
		iFieldsSize,
		arrInputs,
		2u,
		&Writer
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( (xrtSshWriteString(&Writer, Host) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPort) != XSSH_OK) ||
		(xrtSshWriteString(&Writer, Originator) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iOriginatorPort) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取 direct/forwarded 共用的 TCP/IP 专用字段。 */
static xsshcode xsshForwardOpenRead(
	const xsshchannelopen* pOpen,
	const char* pType,
	size_t iTypeSize,
	xsshtcpipopen* pTcpip
)
{
	xsshreader Reader;
	xsshtcpipopen Tcpip;
	xsshcode Code;

	if ( (pOpen == NULL) || (pTcpip == NULL) ||
		!xrtSshNameValid(pOpen->Type) ||
		!xsshForwardNameEqual(pOpen->Type, pType, iTypeSize) ||
		!xrtMemRangeValid(pOpen->Fields.Data, pOpen->Fields.Size) ||
		xrtMemRangesOverlap(
			pOpen->Fields.Data,
			pOpen->Fields.Size,
			pTcpip,
			sizeof(*pTcpip)
		) || !xrtSshReaderInit(&Reader, pOpen->Fields) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xrtSshReadString(&Reader, &Tcpip.Host)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Tcpip.Port)) != XSSH_OK) ||
		((Code = xrtSshReadString(&Reader, &Tcpip.Originator)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &Tcpip.OriginatorPort)) != XSSH_OK) ) {
		return Code;
	}
	if ( xrtSshReaderRemaining(&Reader) != 0u ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pTcpip = Tcpip;
	return XSSH_OK;
}



/* 写入要求回复的 tcpip-forward。 */
xsshcode xrtSshTcpipForwardWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
)
{
	return xsshForwardGlobalWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_GLOBAL_REQUEST_TCPIP_FORWARD),
		Address,
		iPort
	);
}



/* 严格读取 tcpip-forward。 */
xsshcode xrtSshTcpipForwardRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
)
{
	return xsshForwardGlobalRead(
		pRequest,
		XSSH_GLOBAL_REQUEST_TCPIP_FORWARD,
		sizeof(XSSH_GLOBAL_REQUEST_TCPIP_FORWARD) - 1u,
		pForward
	);
}



/* 写入要求回复的 cancel-tcpip-forward。 */
xsshcode xrtSshTcpipForwardCancelWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
)
{
	return xsshForwardGlobalWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD),
		Address,
		iPort
	);
}



/* 严格读取 cancel-tcpip-forward。 */
xsshcode xrtSshTcpipForwardCancelRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
)
{
	return xsshForwardGlobalRead(
		pRequest,
		XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD,
		sizeof(XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD) - 1u,
		pForward
	);
}



/* 写入动态端口分配成功响应。 */
xsshcode xrtSshTcpipForwardSuccessWrite(
	xsshwriter* pWriter,
	uint32 iPort
)
{
	xsshwriter Writer;
	xsshcode Code = xrtSshWriterReserveInputs(pWriter, 5u, NULL, 0u);

	if ( Code != XSSH_OK ) {
		return Code;
	}
	Writer = *pWriter;
	if ( (xrtSshWriteByte(&Writer, XSSH_MSG_REQUEST_SUCCESS) != XSSH_OK) ||
		(xrtSshWriteU32(&Writer, iPort) != XSSH_OK) ) {
		return XSSH_ERROR_STATE;
	}
	*pWriter = Writer;
	return XSSH_OK;
}



/* 严格读取动态端口分配成功响应。 */
xsshcode xrtSshTcpipForwardSuccessRead(
	xbytesview Payload,
	uint32* pPort
)
{
	xsshreader Reader;
	uint32 iPort;
	uint8 iMessage;
	xsshcode Code;

	if ( (pPort == NULL) || !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		xrtMemRangesOverlap(
			Payload.Data,
			Payload.Size,
			pPort,
			sizeof(*pPort)
		) || !xrtSshReaderInit(&Reader, Payload) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( ((Code = xrtSshReadByte(&Reader, &iMessage)) != XSSH_OK) ||
		((Code = xrtSshReadU32(&Reader, &iPort)) != XSSH_OK) ) {
		return Code;
	}
	if ( (iMessage != XSSH_MSG_REQUEST_SUCCESS) ||
		(xrtSshReaderRemaining(&Reader) != 0u) ) {
		return XSSH_ERROR_PROTOCOL;
	}
	*pPort = iPort;
	return XSSH_OK;
}



/* 写入 direct-tcpip channel open。 */
xsshcode xrtSshDirectTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
)
{
	return xsshForwardOpenWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_CHANNEL_TYPE_DIRECT_TCPIP),
		iSender,
		iWindow,
		iMaxPacket,
		Host,
		iPort,
		Originator,
		iOriginatorPort
	);
}



/* 严格读取 direct-tcpip 专用字段。 */
xsshcode xrtSshDirectTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
)
{
	return xsshForwardOpenRead(
		pOpen,
		XSSH_CHANNEL_TYPE_DIRECT_TCPIP,
		sizeof(XSSH_CHANNEL_TYPE_DIRECT_TCPIP) - 1u,
		pTcpip
	);
}



/* 写入 forwarded-tcpip channel open。 */
xsshcode xrtSshForwardedTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
)
{
	return xsshForwardOpenWrite(
		pWriter,
		XRT_STR_LITERAL(XSSH_CHANNEL_TYPE_FORWARDED_TCPIP),
		iSender,
		iWindow,
		iMaxPacket,
		Host,
		iPort,
		Originator,
		iOriginatorPort
	);
}



/* 严格读取 forwarded-tcpip 专用字段。 */
xsshcode xrtSshForwardedTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
)
{
	return xsshForwardOpenRead(
		pOpen,
		XSSH_CHANNEL_TYPE_FORWARDED_TCPIP,
		sizeof(XSSH_CHANNEL_TYPE_FORWARDED_TCPIP) - 1u,
		pTcpip
	);
}

#endif
