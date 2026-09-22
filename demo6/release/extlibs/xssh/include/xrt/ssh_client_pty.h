#ifndef XRT_SSH_CLIENT_PTY_H
#define XRT_SSH_CLIENT_PTY_H

#include <xrt/ssh_channel_pty.h>
#include <xrt/ssh_client_session.h>



#if defined(XSSH_FEATURE_CLIENT_PTY) && \
	(!defined(XSSH_FEATURE_CHANNEL_PTY) || \
	 !defined(XSSH_FEATURE_CLIENT_SESSION))
	#error "XSSH_FEATURE_CLIENT_PTY requires channel PTY and client session"
#endif



#if defined(XSSH_FEATURE_CLIENT_PTY)

XRT_EXTERN_C_BEGIN



/* 请求 PTY；Modes 必须是以 TTY_OP_END 结束的已编码 mode 流。 */
XRT_API xsshcode xrtSshClientSessionPty(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xbytesview Terminal,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight,
	xbytesview Modes,
	bool bWantReply,
	uint64 iReplyToken
);



/* 发送不要求回复的终端窗口尺寸变化。 */
XRT_API xsshcode xrtSshClientSessionResize(
	xsshclient* pClient,
	xsshchannel* pChannel,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
);



XRT_EXTERN_C_END

#endif

#endif
