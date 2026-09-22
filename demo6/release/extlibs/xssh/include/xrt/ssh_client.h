#ifndef XRT_SSH_CLIENT_H
#define XRT_SSH_CLIENT_H

#include <xrt/ssh_channels.h>
#include <xrt/ssh_client_core.h>
#include <xrt/ssh_session_stream.h>



#if defined(XSSH_FEATURE_CLIENT) && \
	(!defined(XSSH_FEATURE_CHANNELS) || \
	 !defined(XSSH_FEATURE_CLIENT_CORE) || \
	 !defined(XSSH_FEATURE_SESSION_STREAM))
	#error "XSSH_FEATURE_CLIENT requires channels, client core and session stream"
#endif



#if defined(XSSH_FEATURE_CLIENT)

#define XSSH_CLIENT_CONTROL_INITIAL_DEFAULT 4096u
#define XSSH_CLIENT_CONTROL_LIMIT_DEFAULT 1048576u
#define XSSH_CLIENT_GLOBAL_REPLY_LIMIT_DEFAULT 64u
/* TCP 建连后到 Ready 的默认截止时间，单位为微秒；配置为零时禁用。 */
#define XSSH_CLIENT_READY_TIMEOUT_DEFAULT UINT64_C(30000000)



typedef struct xsshclient xsshclient;



/* 客户端状态只表达对象与连接生命周期，不复制底层 SSH 阶段。 */
typedef enum xsshclientstate {
	XSSH_CLIENT_CREATED = 0,
	XSSH_CLIENT_HANDSHAKE = 1,
	XSSH_CLIENT_READY = 2,
	XSSH_CLIENT_CLOSING = 3,
	XSSH_CLIENT_CLOSED = 4,
	XSSH_CLIENT_INVALID = 5
} xsshclientstate;



/* Peer channel open 的决定先暂存，读事务提交后才发送线路响应。 */
typedef enum xsshclientchanneldecision {
	XSSH_CLIENT_CHANNEL_NONE = 0,
	XSSH_CLIENT_CHANNEL_ACCEPT = 1,
	XSSH_CLIENT_CHANNEL_REJECT = 2
} xsshclientchanneldecision;



/* Channel 事件只在对应 SSH 读写事务可靠提交后发布。 */
typedef enum xsshclientchannelevent {
	XSSH_CLIENT_CHANNEL_EVENT_OPENED = 0,
	XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED = 1,
	XSSH_CLIENT_CHANNEL_EVENT_WRITABLE = 2,
	XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS = 3,
	XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE = 4,
	XSSH_CLIENT_CHANNEL_EVENT_EOF = 5,
	XSSH_CLIENT_CHANNEL_EVENT_CLOSED = 6
} xsshclientchannelevent;



/* 通知不借用 packet；Channel 保持有效，直到应用显式移除或客户端清理。 */
typedef struct xsshclientchannelnotice {
	xsshchannel* Channel;
	uint64 ReplyToken;
	uint32 Reason;
	xsshclientchannelevent Event;
	bool HasReplyToken;
	bool Incoming;
} xsshclientchannelnotice;



/* 全局请求回复只在 FIFO 出队与 SSH 读事务共同提交后发布。 */
typedef enum xsshclientglobalevent {
	XSSH_CLIENT_GLOBAL_EVENT_REQUEST_SUCCESS = 0,
	XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE = 1
} xsshclientglobalevent;



/* 全局通知不借用 packet，ReplyToken 是调用请求时提供的稳定关联值。 */
typedef struct xsshclientglobalnotice {
	uint64 ReplyToken;
	xsshclientglobalevent Event;
} xsshclientglobalnotice;



/* 动态控制报文构建器可返回 SPACE 扩容重试，或 NEED_MORE 等待外部数据。 */
typedef xsshcode (*xsshclientbuildproc)(
	xsshwriter* pWriter,
	ptr pUserData
);



/* Channel open 构建器借用只读 core，用于 session 与 forwarding 类型扩展。 */
typedef xsshcode (*xsshclientchannelopenproc)(
	xsshwriter* pWriter,
	const xsshchannelcore* pChannel,
	ptr pUserData
);



/* Packet 在读事务提交前执行；Data 在对应 channel I/O 提交后执行。 */
typedef struct xsshclientevents {
	void (*Open)(xsshclient* pClient, ptr pData);
	void (*Ready)(xsshclient* pClient, ptr pData);
	void (*HostKey)(xsshclient* pClient, ptr pData);
	void (*Authenticate)(xsshclient* pClient, ptr pData);
	xsshsessionstreamdecision (*Packet)(
		xsshclient* pClient,
		const xsshsessiontcppacket* pPacket,
		ptr pData
	);
	void (*Data)(
		xsshclient* pClient,
		xsshchannel* pChannel,
		xsshchanneliostream Stream,
		ptr pData
	);
	void (*Rekey)(
		xsshclient* pClient,
		xsshrekeydecision Decision,
		ptr pData
	);
	void (*Error)(
		xsshclient* pClient,
		xsshcode Code,
		const xerror* pError,
		ptr pData
	);
	void (*End)(xsshclient* pClient, ptr pData);
	void (*HighWater)(
		xsshclient* pClient,
		size_t iQueued,
		ptr pData
	);
	void (*LowWater)(
		xsshclient* pClient,
		size_t iQueued,
		ptr pData
	);
	void (*Drain)(xsshclient* pClient, ptr pData);
	void (*Close)(
		xsshclient* pClient,
		xnetresult Result,
		const xerror* pError,
		ptr pData
	);
	void (*Channel)(
		xsshclient* pClient,
		const xsshclientchannelnotice* pNotice,
		ptr pData
	);
	void (*Global)(
		xsshclient* pClient,
		const xsshclientglobalnotice* pNotice,
		ptr pData
	);
} xsshclientevents;



/* 配置组合握手策略、Ready 截止时间、动态 channel 预算和控制报文上限。 */
typedef struct xsshclientconfig {
	xsshclientcoreconfig Core;
	xsshchannelsconfig Channels;
	uint64 ReadyTimeout;
	size_t ControlInitial;
	size_t ControlLimit;
	size_t GlobalReplyLimit;
} xsshclientconfig;



/* 客户端拥有协议状态和动态数据，但只借用 Stream、Worker 与用户配置视图。 */
struct xsshclient {
	xsshsessionstream Stream;
	xsshclientcore Core;
	xsshchannels Channels;
	xsshreplyqueue GlobalReplies;
	xnetbuf Control;
	xsshclientconfig Config;
	xsshclientevents Events;
	xerror* TerminalError;
	xsshchannel* ReceiveChannel;
	xsshchannel* SendChannel;
	xsshchannel* OpenPendingChannel;
	xsshchannel* OpenSendChannel;
	const xsshchannelopen* OpenCurrent;
	xsshclientchannelnotice ChannelNotice;
	xsshclientglobalnotice GlobalNotice;
	uint64* GlobalReplyTokens;
	uint64 ReadyTimer;
	ptr UserData;
	#if defined(XSSH_FEATURE_CLIENT_FUTURE)
		ptr FutureState;
		xsshchannel* FutureWritableChannel;
	#endif
	size_t ControlTarget;
	size_t GlobalReplyCapacity;
	xsshchanneliostream ReceiveStream;
	xsshclientstate State;
	xsshclientchanneldecision OpenDecision;
	uint32 OpenReason;
	bool ResourcesReady;
	bool ReceivePending;
	bool ReceiveRetry;
	bool ReceiveRetryOpen;
	bool SendPending;
	bool ChannelNoticePending;
	bool GlobalNoticePending;
	bool ReadyNotified;
	bool HostNotified;
	bool AuthNotified;
	uint32 Guard;
};



XRT_EXTERN_C_BEGIN



/* 写入没有隐藏 Engine、默认拒绝未知主机且预算有界的客户端配置。 */
XRT_API bool xrtSshClientConfigInit(xsshclientconfig* pConfig);



/* 初始化未附着客户端；配置中的文本、策略数据和回调上下文保持借用。 */
XRT_API bool xrtSshClientInit(
	xsshclient* pClient,
	const xsshclientconfig* pConfig,
	const xsshclientevents* pEvents,
	ptr pData
);



/* 只清理尚未附着或已经关闭的客户端及其全部动态 channel 数据。 */
XRT_API bool xrtSshClientClear(xsshclient* pClient);



/* 返回交给 xrtNetStreamConnect 的事件表和 data 指针。 */
XRT_API const xnetstreamevents* xrtSshClientNetEvents(void);
XRT_API ptr xrtSshClientNetData(xsshclient* pClient);



/* 在 Stream 所属 Worker 中接管已打开且尚无积压输入的 TCP Stream。 */
XRT_API bool xrtSshClientAttach(
	xsshclient* pClient,
	xnetstream* pStream
);



/* 返回客户端生命周期以及完整底层对象，保留高级用户的直接控制能力。 */
XRT_API xsshclientstate xrtSshClientState(const xsshclient* pClient);
XRT_API bool xrtSshClientIsCurrent(const xsshclient* pClient);
XRT_API xsshsessionstream* xrtSshClientStream(xsshclient* pClient);
XRT_API xsshsessiontcp* xrtSshClientSession(xsshclient* pClient);
XRT_API xsshsessionreader* xrtSshClientReader(xsshclient* pClient);
XRT_API xsshchannels* xrtSshClientChannels(xsshclient* pClient);



/* 判断借用 channel 是否仍由当前客户端拥有。 */
XRT_API bool xrtSshClientOwnsChannel(
	const xsshclient* pClient,
	const xsshchannel* pChannel
);



/* 在 CHANNEL_OPEN Packet/HOLD 期间暂存接受或拒绝决定。 */
XRT_API xsshcode xrtSshClientChannelAccept(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	xsshchannel** ppChannel
);
XRT_API xsshcode xrtSshClientChannelReject(
	xsshclient* pClient,
	const xsshchannelopen* pOpen,
	uint32 iReason
);



/* 返回全局 request reply FIFO，并按需扩展其有界 token 存储。 */
XRT_API xsshreplyqueue* xrtSshClientGlobalReplies(xsshclient* pClient);
XRT_API xsshcode xrtSshClientGlobalReplyReserve(
	xsshclient* pClient,
	size_t iCapacity
);



/* 凭据或其他外部认证数据就绪后，重新推进当前客户端动作。 */
XRT_API xsshcode xrtSshClientContinue(xsshclient* pClient);



/* 完成被 HostKey 回调延迟的信任决定。 */
XRT_API xsshcode xrtSshClientHostKeyAccept(xsshclient* pClient);
XRT_API xsshcode xrtSshClientHostKeyReject(xsshclient* pClient);



/* 提交或拒绝用户 Packet 回调保留的输入；Retry 专门重试内部 OOM 暂停。 */
XRT_API xsshcode xrtSshClientPacketAccept(xsshclient* pClient);
XRT_API xsshcode xrtSshClientPacketReject(xsshclient* pClient);
XRT_API xsshcode xrtSshClientPacketRetry(xsshclient* pClient);



/* 直接把完整 payload 编码并提交给有界 TCP 队列；视图只借用到函数返回。 */
XRT_API xsshcode xrtSshClientSend(
	xsshclient* pClient,
	xbytesview Payload,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
);



/* 在动态连续 scratch 中构建并发送一个控制报文，SPACE 会按上限扩容重试。 */
XRT_API xsshcode xrtSshClientBuild(
	xsshclient* pClient,
	xsshclientbuildproc pBuild,
	ptr pBuildData,
	xsshchannel* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
);



/* 创建动态 channel，并用类型专用构建器发送唯一 CHANNEL_OPEN。 */
XRT_API xsshcode xrtSshClientChannelOpen(
	xsshclient* pClient,
	xsshclientchannelopenproc pOpen,
	ptr pOpenData,
	xsshchannel** ppChannel
);



/* 把指定 channel 发送队首封装为一条受窗口和最大包约束的数据消息。 */
XRT_API xsshcode xrtSshClientChannelFlush(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream
);



/* 发送当前已消费接收数据对应的 WINDOW_ADJUST；无额度时返回 NEED_MORE。 */
XRT_API xsshcode xrtSshClientChannelAdjust(
	xsshclient* pClient,
	xsshchannel* pChannel
);



/* 发送 channel 写方向 EOF 或双向关闭消息。 */
XRT_API xsshcode xrtSshClientChannelEof(
	xsshclient* pClient,
	xsshchannel* pChannel
);
XRT_API xsshcode xrtSshClientChannelClose(
	xsshclient* pClient,
	xsshchannel* pChannel
);



/* 中止未决 SSH 事务并异常关闭底层 TCP Stream。 */
XRT_API bool xrtSshClientAbort(xsshclient* pClient);



XRT_EXTERN_C_END

#endif

#endif
