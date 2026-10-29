#ifndef XRT_SSH_FORWARD_MESSAGE_H
#define XRT_SSH_FORWARD_MESSAGE_H

#include <xrt/ssh_channel_message.h>
#include <xrt/ssh_connection_message.h>



#if defined(XSSH_FEATURE_FORWARD_MESSAGE) && \
	(!defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
	 !defined(XSSH_FEATURE_CONNECTION_MESSAGE))
	#error "XSSH_FEATURE_FORWARD_MESSAGE requires SSH channel and connection messages"
#endif



#if defined(XSSH_FEATURE_FORWARD_MESSAGE)

#define XSSH_GLOBAL_REQUEST_TCPIP_FORWARD "tcpip-forward"
#define XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD "cancel-tcpip-forward"
#define XSSH_CHANNEL_TYPE_DIRECT_TCPIP "direct-tcpip"
#define XSSH_CHANNEL_TYPE_FORWARDED_TCPIP "forwarded-tcpip"



/* Remote forwarding 请求借用线路地址并保留 uint32 端口。 */
typedef struct xsshtcpipforward {
	xbytesview Address;
	uint32 Port;
} xsshtcpipforward;



/* TCP/IP channel open 借用目标与来源地址。 */
typedef struct xsshtcpipopen {
	xbytesview Host;
	uint32 Port;
	xbytesview Originator;
	uint32 OriginatorPort;
} xsshtcpipopen;



XRT_EXTERN_C_BEGIN



/* 写入或严格读取要求回复的 tcpip-forward 全局请求。 */
XRT_API xsshcode xrtSshTcpipForwardWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
);
XRT_API xsshcode xrtSshTcpipForwardRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
);



/* 写入或严格读取要求回复的 cancel-tcpip-forward 请求。 */
XRT_API xsshcode xrtSshTcpipForwardCancelWrite(
	xsshwriter* pWriter,
	xbytesview Address,
	uint32 iPort
);
XRT_API xsshcode xrtSshTcpipForwardCancelRead(
	const xsshglobalrequest* pRequest,
	xsshtcpipforward* pForward
);



/* 写入或严格读取动态分配端口的 REQUEST_SUCCESS。 */
XRT_API xsshcode xrtSshTcpipForwardSuccessWrite(
	xsshwriter* pWriter,
	uint32 iPort
);
XRT_API xsshcode xrtSshTcpipForwardSuccessRead(
	xbytesview Payload,
	uint32* pPort
);



/* 写入或严格读取 direct-tcpip channel open。 */
XRT_API xsshcode xrtSshDirectTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
);
XRT_API xsshcode xrtSshDirectTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
);



/* 写入或严格读取 forwarded-tcpip channel open。 */
XRT_API xsshcode xrtSshForwardedTcpipOpenWrite(
	xsshwriter* pWriter,
	uint32 iSender,
	uint32 iWindow,
	uint32 iMaxPacket,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort
);
XRT_API xsshcode xrtSshForwardedTcpipOpenRead(
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip
);



XRT_EXTERN_C_END

#endif

#endif
