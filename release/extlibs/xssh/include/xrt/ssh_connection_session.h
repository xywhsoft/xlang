#ifndef XRT_SSH_CONNECTION_SESSION_H
#define XRT_SSH_CONNECTION_SESSION_H

#include <xrt/ssh_channel_core.h>
#include <xrt/ssh_connection_message.h>
#include <xrt/ssh_reply_queue.h>
#include <xrt/ssh_transport_core.h>



#if defined(XSSH_FEATURE_CONNECTION_SESSION) && \
	(!defined(XSSH_FEATURE_CHANNEL_CORE) || \
	 !defined(XSSH_FEATURE_CONNECTION_MESSAGE) || \
	 !defined(XSSH_FEATURE_REPLY_QUEUE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE))
	#error "XSSH_FEATURE_CONNECTION_SESSION requires channel core, connection message, reply queue and transport core"
#endif



#if defined(XSSH_FEATURE_CONNECTION_SESSION)

/* Connection packet 覆盖 RFC 4254 公共全局与 channel 消息。 */
typedef enum xsshconnectionpacketkind {
	XSSH_CONNECTION_PACKET_NONE = 0,
	XSSH_CONNECTION_PACKET_GLOBAL_REQUEST = 1,
	XSSH_CONNECTION_PACKET_GLOBAL_SUCCESS = 2,
	XSSH_CONNECTION_PACKET_GLOBAL_FAILURE = 3,
	XSSH_CONNECTION_PACKET_CHANNEL_OPEN = 4,
	XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION = 5,
	XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE = 6,
	XSSH_CONNECTION_PACKET_CHANNEL_ADJUST = 7,
	XSSH_CONNECTION_PACKET_CHANNEL_DATA = 8,
	XSSH_CONNECTION_PACKET_CHANNEL_EXTENDED_DATA = 9,
	XSSH_CONNECTION_PACKET_CHANNEL_EOF = 10,
	XSSH_CONNECTION_PACKET_CHANNEL_CLOSE = 11,
	XSSH_CONNECTION_PACKET_CHANNEL_REQUEST = 12,
	XSSH_CONNECTION_PACKET_CHANNEL_SUCCESS = 13,
	XSSH_CONNECTION_PACKET_CHANNEL_FAILURE = 14
} xsshconnectionpacketkind;



/* 借用视图只在对应 transport read 事务提交前有效。 */
typedef union xsshconnectionmessage {
	xsshglobalrequest GlobalRequest;
	xbytesview GlobalSuccess;
	xsshchannelopen ChannelOpen;
	xsshchannelconfirmation ChannelConfirmation;
	xsshchannelopenfailure ChannelOpenFailure;
	xsshchanneladjust ChannelAdjust;
	xsshchanneldata ChannelData;
	xsshchannelextendeddata ChannelExtendedData;
	xsshchannelrequest ChannelRequest;
	uint32 Recipient;
} xsshconnectionmessage;



/* ReplyToken 只在 success/failure 已关联等待队首时有效。 */
typedef struct xsshconnectionpacket {
	xsshconnectionmessage Message;
	uint64 ReplyToken;
	xsshconnectionpacketkind Kind;
	bool HasReplyToken;
} xsshconnectionpacket;



/* Resolver 把本地 recipient 映射到调用方 channel 与可选 request reply FIFO。 */
typedef bool (*xsshchannelresolveproc)(
	ptr pUserData,
	uint32 iLocal,
	xsshchannelcore** ppChannel,
	xsshreplyqueue** ppReplies
);



/* QueueAction 只描述当前待提交事务，不改变调用方 FIFO。 */
typedef enum xsshconnectionqueueaction {
	XSSH_CONNECTION_QUEUE_NONE = 0,
	XSSH_CONNECTION_QUEUE_PUSH = 1,
	XSSH_CONNECTION_QUEUE_POP = 2
} xsshconnectionqueueaction;



/* 会话持有短事务快照，不拥有 channel、FIFO、payload 或网络对象。 */
typedef struct xsshconnectionsession {
	xsshchannelcore ChannelBefore;
	xsshchannelcore ChannelPending;
	xsshreplyqueue QueueBefore;
	xsshchannelcore* Channel;
	xsshreplyqueue* Queue;
	xsshreplyqueue* GlobalReplies;
	xsshchannelresolveproc Resolve;
	ptr UserData;
	uint64 QueueToken;
	uint64 WriteOrdinal;
	uint64 ReadOrdinal;
	xsshconnectionpacketkind WritePending;
	xsshconnectionpacketkind ReadPending;
	xsshconnectionqueueaction QueueAction;
	xsshrole Role;
	bool Active;
	bool Failed;
	uint32 ObjectGuard;
} xsshconnectionsession;



XRT_EXTERN_C_BEGIN



/* 初始化无网络会话；全局 FIFO 必须为空，空 resolver 只允许 global 与新 channel open。 */
XRT_API bool xrtSshConnectionSessionInit(
	xsshconnectionsession* pSession,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
);



/* 清除事务与借用指针，不清理调用方 channel 或 reply FIFO。 */
XRT_API void xrtSshConnectionSessionClear(xsshconnectionsession* pSession);



/* 在 server USERAUTH_SUCCESS 已按正确方向提交后开放 connection 层。 */
XRT_API xsshcode xrtSshConnectionSessionBegin(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
);



/* 返回会话是否仍可处理 connection 消息。 */
XRT_API bool xrtSshConnectionSessionActive(
	const xsshconnectionsession* pSession
);



/*
	解析最终输出 payload，并在 channel 副本中准备提交结果。
	pChannel 只用于 channel 消息；want-reply request 额外传入对应 FIFO 与 token。
*/
XRT_API xsshcode xrtSshConnectionSessionWritePrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken
);



/* transport 已可靠提交输出后，原子提交 channel 与 request FIFO 状态。 */
XRT_API xsshcode xrtSshConnectionSessionWriteCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃未交给 transport 的输出，不修改 channel 或 reply FIFO。 */
XRT_API xsshcode xrtSshConnectionSessionWriteAbort(
	xsshconnectionsession* pSession
);



/*
	解析 transport 已认证的 peer payload，并借出通用 packet。
	已有关联 channel 的消息通过 resolver 在副本中验证；新 CHANNEL_OPEN 由调用方决定存储。
*/
XRT_API xsshcode xrtSshConnectionSessionReadPrepare(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshconnectionpacket* pPacket
);



/* transport 已提交输入后，原子提交 channel 与 reply FIFO 状态。 */
XRT_API xsshcode xrtSshConnectionSessionReadCommit(
	xsshconnectionsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃已认证输入并终止 connection 会话；对应 transport 也必须关闭。 */
XRT_API xsshcode xrtSshConnectionSessionReadAbort(
	xsshconnectionsession* pSession
);



/* 显式终止 connection 编排；重复调用保持失败状态。 */
XRT_API void xrtSshConnectionSessionFail(xsshconnectionsession* pSession);



XRT_EXTERN_C_END

#endif

#endif
