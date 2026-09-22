#include "../internal/xrt_websocket.h"



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)

/* 开始一条压缩分片 Text 或 Binary 消息。 */
XRT_API xwswriter* xrtWsConnBeginCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode
)
{
	return __xrtWsWriterCreate(
		pConnection,
		Opcode,
		true
	);
}



/* 开始一条压缩分片 Text 消息。 */
XRT_API xwswriter* xrtWsConnBeginTextCompressed(
	xwsconn* pConnection
)
{
	return xrtWsConnBeginCompressed(
		pConnection,
		XWS_OPCODE_TEXT
	);
}



/* 开始一条压缩分片 Binary 消息。 */
XRT_API xwswriter* xrtWsConnBeginBinaryCompressed(
	xwsconn* pConnection
)
{
	return xrtWsConnBeginCompressed(
		pConnection,
		XWS_OPCODE_BINARY
	);
}

#endif
