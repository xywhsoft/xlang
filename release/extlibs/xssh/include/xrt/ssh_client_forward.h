#ifndef XRT_SSH_CLIENT_FORWARD_H
#define XRT_SSH_CLIENT_FORWARD_H

#include <xrt/ssh_client.h>
#include <xrt/ssh_forward_message.h>



#if defined(XSSH_FEATURE_CLIENT_FORWARD) && \
	(!defined(XSSH_FEATURE_CLIENT) || \
	 !defined(XSSH_FEATURE_FORWARD_MESSAGE))
	#error "XSSH_FEATURE_CLIENT_FORWARD requires client and forward message"
#endif



#if defined(XSSH_FEATURE_CLIENT_FORWARD)

XRT_EXTERN_C_BEGIN



/* 打开由 SSH 服务端连接目标地址的 direct-tcpip channel。 */
XRT_API xsshcode xrtSshClientDirectTcpipOpen(
	xsshclient* pClient,
	xbytesview Host,
	uint32 iPort,
	xbytesview Originator,
	uint32 iOriginatorPort,
	xsshchannel** ppChannel
);



/* 在 Packet 回调/HOLD 中解析并暂存接受 forwarded-tcpip channel。 */
XRT_API xsshcode xrtSshClientForwardedTcpipAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshtcpipopen* pTcpip,
	xsshchannel** ppChannel
);



/* 请求或取消服务端监听 remote forwarding 地址。 */
XRT_API xsshcode xrtSshClientTcpipForward(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
);
XRT_API xsshcode xrtSshClientTcpipForwardCancel(
	xsshclient* pClient,
	xbytesview Address,
	uint32 iPort,
	uint64 iReplyToken
);



XRT_EXTERN_C_END

#endif

#endif
