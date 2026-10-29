#ifndef XRT_SSH_CHANNEL_REQUEST_H
#define XRT_SSH_CHANNEL_REQUEST_H

#include <xrt/ssh_channel_message.h>



#if defined(XSSH_FEATURE_CHANNEL_REQUEST) && \
	!defined(XSSH_FEATURE_CHANNEL_MESSAGE)
	#error "XSSH_FEATURE_CHANNEL_REQUEST requires XSSH_FEATURE_CHANNEL_MESSAGE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_REQUEST)

#define XSSH_CHANNEL_REQUEST_ENV "env"
#define XSSH_CHANNEL_REQUEST_SHELL "shell"
#define XSSH_CHANNEL_REQUEST_EXEC "exec"
#define XSSH_CHANNEL_REQUEST_SUBSYSTEM "subsystem"
#define XSSH_CHANNEL_REQUEST_XON_XOFF "xon-xoff"
#define XSSH_CHANNEL_REQUEST_WINDOW_CHANGE "window-change"
#define XSSH_CHANNEL_REQUEST_SIGNAL "signal"
#define XSSH_CHANNEL_REQUEST_EXIT_STATUS "exit-status"
#define XSSH_CHANNEL_REQUEST_EXIT_SIGNAL "exit-signal"
#define XSSH_CHANNEL_REQUEST_BREAK "break"



/* Env request 借用不限定编码的名称和值。 */
typedef struct xsshchannelenv {
	xbytesview Name;
	xbytesview Value;
} xsshchannelenv;



/* Window-change request 保留字符和像素两个尺寸系统。 */
typedef struct xsshchannelwindowchange {
	uint32 Columns;
	uint32 Rows;
	uint32 PixelWidth;
	uint32 PixelHeight;
} xsshchannelwindowchange;



/* Exit-signal request 借用规范信号名、UTF-8 描述和 language tag。 */
typedef struct xsshchannelexitsignal {
	xstrview Signal;
	bool CoreDumped;
	xstrview Message;
	xstrview Language;
} xsshchannelexitsignal;



XRT_EXTERN_C_BEGIN



/* 校验 RFC channel signal 名称，并拒绝多余的 SIG 前缀。 */
XRT_API bool xrtSshChannelSignalValid(xstrview Signal);



/* 写入或严格读取无专用字段的 shell request。 */
XRT_API xsshcode xrtSshChannelShellWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply
);
XRT_API xsshcode xrtSshChannelShellRead(
	const xsshchannelrequest* pRequest
);



/* 写入或严格读取不限定编码的 exec command。 */
XRT_API xsshcode xrtSshChannelExecWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Command
);
XRT_API xsshcode xrtSshChannelExecRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pCommand
);



/* 写入或严格读取不限定编码的 subsystem 名称。 */
XRT_API xsshcode xrtSshChannelSubsystemWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Subsystem
);
XRT_API xsshcode xrtSshChannelSubsystemRead(
	const xsshchannelrequest* pRequest,
	xbytesview* pSubsystem
);



/* 写入或严格读取 env 名称和值。 */
XRT_API xsshcode xrtSshChannelEnvWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	xbytesview Name,
	xbytesview Value
);
XRT_API xsshcode xrtSshChannelEnvRead(
	const xsshchannelrequest* pRequest,
	xsshchannelenv* pEnv
);



/* 写入或严格读取不要求回复的 xon-xoff 通知。 */
XRT_API xsshcode xrtSshChannelXonXoffWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bClientCanDo
);
XRT_API xsshcode xrtSshChannelXonXoffRead(
	const xsshchannelrequest* pRequest,
	bool* pClientCanDo
);



/* 写入或严格读取不要求回复的终端尺寸变更。 */
XRT_API xsshcode xrtSshChannelWindowChangeWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iColumns,
	uint32 iRows,
	uint32 iPixelWidth,
	uint32 iPixelHeight
);
XRT_API xsshcode xrtSshChannelWindowChangeRead(
	const xsshchannelrequest* pRequest,
	xsshchannelwindowchange* pChange
);



/* 写入或严格读取不要求回复的信号通知。 */
XRT_API xsshcode xrtSshChannelSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal
);
XRT_API xsshcode xrtSshChannelSignalRead(
	const xsshchannelrequest* pRequest,
	xstrview* pSignal
);



/* 写入或严格读取 RFC 4335 break request。 */
XRT_API xsshcode xrtSshChannelBreakWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	bool bWantReply,
	uint32 iLengthMs
);
XRT_API xsshcode xrtSshChannelBreakRead(
	const xsshchannelrequest* pRequest,
	uint32* pLengthMs
);



/* 写入或严格读取不要求回复的进程退出状态。 */
XRT_API xsshcode xrtSshChannelExitStatusWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	uint32 iStatus
);
XRT_API xsshcode xrtSshChannelExitStatusRead(
	const xsshchannelrequest* pRequest,
	uint32* pStatus
);



/* 写入或严格读取不要求回复的进程退出信号。 */
XRT_API xsshcode xrtSshChannelExitSignalWrite(
	xsshwriter* pWriter,
	uint32 iRecipient,
	xstrview Signal,
	bool bCoreDumped,
	xstrview Message,
	xstrview Language
);
XRT_API xsshcode xrtSshChannelExitSignalRead(
	const xsshchannelrequest* pRequest,
	xsshchannelexitsignal* pSignal
);



XRT_EXTERN_C_END

#endif

#endif
