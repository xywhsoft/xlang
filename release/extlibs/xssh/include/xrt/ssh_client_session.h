#ifndef XRT_SSH_CLIENT_SESSION_H
#define XRT_SSH_CLIENT_SESSION_H

#include <xrt/ssh_channel_request.h>
#include <xrt/ssh_client.h>



#if defined(XSSH_FEATURE_CLIENT_SESSION) && \
	(!defined(XSSH_FEATURE_CHANNEL_REQUEST) || \
	 !defined(XSSH_FEATURE_CLIENT))
	#error "XSSH_FEATURE_CLIENT_SESSION requires channel request and client"
#endif



#if defined(XSSH_FEATURE_CLIENT_SESSION)

#define XSSH_CHANNEL_TYPE_SESSION "session"



XRT_EXTERN_C_BEGIN



/* 创建等待服务端 confirmation 的 session channel。 */
XRT_API xsshcode xrtSshClientSessionOpen(
	xsshclient* pClient,
	xsshchannel** ppChannel
);



/* 发送保留未知扩展字段的 session request。 */
XRT_API xsshcode xrtSshClientSessionRequest(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Type,
	xbytesview Fields,
	bool bWantReply,
	uint64 iReplyToken
);



/* 设置一个环境变量；回复 token 按同一 channel 的 FIFO 顺序返回。 */
XRT_API xsshcode xrtSshClientSessionEnv(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Name,
	xbytesview Value,
	bool bWantReply,
	uint64 iReplyToken
);



/* 请求交互 shell、单条命令或命名 subsystem。 */
XRT_API xsshcode xrtSshClientSessionShell(
	xsshclient* pClient,
	xsshchannel* pChannel,
	bool bWantReply,
	uint64 iReplyToken
);
XRT_API xsshcode xrtSshClientSessionExec(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Command,
	bool bWantReply,
	uint64 iReplyToken
);
XRT_API xsshcode xrtSshClientSessionSubsystem(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Subsystem,
	bool bWantReply,
	uint64 iReplyToken
);



/* 向远端进程发送规范 signal 或 RFC 4335 break。 */
XRT_API xsshcode xrtSshClientSessionSignal(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xstrview Signal
);
XRT_API xsshcode xrtSshClientSessionBreak(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iLengthMs,
	bool bWantReply,
	uint64 iReplyToken
);



XRT_EXTERN_C_END

#endif

#endif
