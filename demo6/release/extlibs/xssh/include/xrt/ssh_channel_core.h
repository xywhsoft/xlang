#ifndef XRT_SSH_CHANNEL_CORE_H
#define XRT_SSH_CHANNEL_CORE_H

#include <xrt/ssh_channel_message.h>
#include <xrt/ssh_channel_state.h>
#include <xrt/ssh_channel_window.h>



#if defined(XSSH_FEATURE_CHANNEL_CORE) && \
	(!defined(XSSH_FEATURE_CHANNEL_MESSAGE) || \
	 !defined(XSSH_FEATURE_CHANNEL_STATE) || \
	 !defined(XSSH_FEATURE_CHANNEL_WINDOW))
	#error "XSSH_FEATURE_CHANNEL_CORE requires channel message, state and window"
#endif



#if defined(XSSH_FEATURE_CHANNEL_CORE)

/* Channel core 阶段不表达应用缓冲、请求处理或网络等待。 */
typedef enum xsshchannelcorephase {
	XSSH_CHANNEL_CORE_OPENING = 0,
	XSSH_CHANNEL_CORE_ACCEPTING = 1,
	XSSH_CHANNEL_CORE_OPEN = 2,
	XSSH_CHANNEL_CORE_FAILED = 3,
	XSSH_CHANNEL_CORE_CLOSED = 4
} xsshchannelcorephase;



/* 单个 channel 只保存编号、窗口和关闭状态，不拥有 payload 或数据队列。 */
typedef struct xsshchannelcore {
	xsshchannelwindow Window;
	xsshchannelstate State;
	uint32 Local;
	uint32 Remote;
	uint32 FailureReason;
	xsshchannelcorephase Phase;
	bool Initialized;
} xsshchannelcore;



XRT_EXTERN_C_BEGIN



/* 初始化等待 peer confirmation 的本端 channel open。 */
XRT_API bool xrtSshChannelCoreOpenInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
);



/* 从已解析 peer open 初始化等待本端 confirmation 的 channel。 */
XRT_API bool xrtSshChannelCoreAcceptInit(
	xsshchannelcore* pChannel,
	uint32 iLocal,
	const xsshchannelopen* pOpen,
	uint32 iReceiveWindow,
	uint32 iReceiveMaxPacket,
	uint32 iAdjustThreshold
);



/* 清除 channel core；不会清理调用方数据或 request reply 队列。 */
XRT_API void xrtSshChannelCoreClear(xsshchannelcore* pChannel);



/* 返回公开 channel 阶段；无效对象返回 FAILED。 */
XRT_API xsshchannelcorephase xrtSshChannelCorePhase(
	const xsshchannelcore* pChannel
);



/* 返回本端 channel id；远端 id 仅在 accepting/open/closed 阶段可用。 */
XRT_API bool xrtSshChannelCoreIds(
	const xsshchannelcore* pChannel,
	uint32* pLocal,
	uint32* pRemote
);



/* 提交 peer 对本端 open 的 confirmation，并原子开放数据面。 */
XRT_API xsshcode xrtSshChannelCoreConfirmationCommit(
	xsshchannelcore* pChannel,
	const xsshchannelconfirmation* pConfirmation
);



/* 提交 peer 对本端 open 的 failure，并保留失败 reason。 */
XRT_API xsshcode xrtSshChannelCoreFailureCommit(
	xsshchannelcore* pChannel,
	const xsshchannelopenfailure* pFailure
);



/* 本端 confirmation 已可靠排队后提交 peer open。 */
XRT_API xsshcode xrtSshChannelCoreAcceptCommit(xsshchannelcore* pChannel);



/* 本端 failure 已可靠排队后提交拒绝结果。 */
XRT_API xsshcode xrtSshChannelCoreRejectCommit(
	xsshchannelcore* pChannel,
	uint32 iReason
);



/* 判断数据面是否已打开或双向 close 是否已经完成。 */
XRT_API bool xrtSshChannelCoreOpen(const xsshchannelcore* pChannel);
XRT_API bool xrtSshChannelCoreClosed(const xsshchannelcore* pChannel);



/* 返回下一条 data/extended-data payload 可发送的最大字节数。 */
XRT_API uint32 xrtSshChannelCoreSendLimit(
	const xsshchannelcore* pChannel
);



/* data 可靠排队后扣减远端窗口；EOF/CLOSE 后拒绝新数据。 */
XRT_API xsshcode xrtSshChannelCoreDataSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
);



/* 提交已验证的 peer data，并校验本端 recipient、窗口和生命周期。 */
XRT_API xsshcode xrtSshChannelCoreDataReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient,
	uint32 iBytes
);



/* 应用消费已接收数据；close 后仍可释放此前缓冲的数据。 */
XRT_API xsshcode xrtSshChannelCoreDataConsume(
	xsshchannelcore* pChannel,
	uint32 iBytes
);



/* 判断是否应发送 WINDOW_ADJUST，并返回当前可安全返还额度。 */
XRT_API bool xrtSshChannelCoreAdjustReady(
	const xsshchannelcore* pChannel
);
XRT_API uint32 xrtSshChannelCoreAdjustLimit(
	const xsshchannelcore* pChannel
);



/* WINDOW_ADJUST 可靠排队后提交本端返还额度。 */
XRT_API xsshcode xrtSshChannelCoreAdjustSendCommit(
	xsshchannelcore* pChannel,
	uint32 iBytes
);



/* 提交已验证的 peer WINDOW_ADJUST。 */
XRT_API xsshcode xrtSshChannelCoreAdjustReceiveCommit(
	xsshchannelcore* pChannel,
	const xsshchanneladjust* pAdjust
);



/* 判断当前方向是否还能发送或接收 channel request。 */
XRT_API bool xrtSshChannelCoreCanSendRequest(
	const xsshchannelcore* pChannel
);
XRT_API bool xrtSshChannelCoreCanReceiveRequest(
	const xsshchannelcore* pChannel
);



/* 校验已解析 peer channel 消息的 recipient，不推进状态。 */
XRT_API xsshcode xrtSshChannelCoreRecipientCheck(
	const xsshchannelcore* pChannel,
	uint32 iRecipient
);



/* EOF 可靠排队或验证接收后提交对应方向的半关闭。 */
XRT_API xsshcode xrtSshChannelCoreEofSendCommit(
	xsshchannelcore* pChannel
);
XRT_API xsshcode xrtSshChannelCoreEofReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
);



/* CLOSE 可靠排队或验证接收后提交；双向完成时进入 CLOSED。 */
XRT_API xsshcode xrtSshChannelCoreCloseSendCommit(
	xsshchannelcore* pChannel
);
XRT_API xsshcode xrtSshChannelCoreCloseReceiveCommit(
	xsshchannelcore* pChannel,
	uint32 iRecipient
);



XRT_EXTERN_C_END

#endif

#endif
