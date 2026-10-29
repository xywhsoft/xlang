#ifndef XRT_SSH_CHANNEL_WINDOW_H
#define XRT_SSH_CHANNEL_WINDOW_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_CHANNEL_WINDOW) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_CHANNEL_WINDOW requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_CHANNEL_WINDOW)

/* 单个 channel 的双向窗口状态；字段只读，更新必须经过公开操作。 */
typedef struct xsshchannelwindow {
	uint32 SendWindow;
	uint32 SendMaxPacket;
	uint32 ReceiveWindow;
	uint32 ReceiveMaxPacket;
	uint32 AdjustThreshold;
	uint64 ReceiveBuffered;
	uint64 ReceivePending;
} xsshchannelwindow;



XRT_EXTERN_C_BEGIN



/* 初始化远端发送额度和本地接收额度，不分配缓冲。 */
XRT_API bool xrtSshChannelWindowInit(
	xsshchannelwindow* pWindow,
	uint32 iSendWindow,
	uint32 iSendMaxPacket,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
);



/* 返回下一条普通或扩展数据消息可发送的最大字节数。 */
XRT_API uint32 xrtSshChannelSendLimit(const xsshchannelwindow* pWindow);



/* 提交已排队的数据字节，并扣减远端窗口。 */
XRT_API xsshcode xrtSshChannelSendCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 应用远端 WINDOW_ADJUST，溢出视为协议错误。 */
XRT_API xsshcode xrtSshChannelSendAdjust(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 接收一条数据消息并校验本地窗口与最大包限制。 */
XRT_API xsshcode xrtSshChannelReceiveCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 标记应用已经消费的接收字节，使额度可稍后返还。 */
XRT_API xsshcode xrtSshChannelReceiveConsume(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 判断已消费额度是否达到返还阈值或本地窗口已经耗尽。 */
XRT_API bool xrtSshChannelReceiveAdjustReady(
	const xsshchannelwindow* pWindow
);



/* 返回当前单条 WINDOW_ADJUST 可安全返还的最大额度。 */
XRT_API uint32 xrtSshChannelReceiveAdjustLimit(
	const xsshchannelwindow* pWindow
);



/* 在 WINDOW_ADJUST 已可靠排队后提交返还额度。 */
XRT_API xsshcode xrtSshChannelReceiveAdjustCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



/* 提交独立于已消费字节的新接收额度，用于动态扩容或零窗口恢复。 */
XRT_API xsshcode xrtSshChannelReceiveGrantCommit(
	xsshchannelwindow* pWindow,
	uint32 iBytes
);



XRT_EXTERN_C_END

#endif

#endif
