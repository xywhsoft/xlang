#ifndef XRT_SSH_SESSION_TCP_H
#define XRT_SSH_SESSION_TCP_H

#include <xrt/ssh_session_core.h>
#include <xrt/ssh_transport_tcp.h>



#if defined(XSSH_FEATURE_SESSION_TCP) && \
	(!defined(XSSH_FEATURE_SESSION_CORE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP))
	#error "XSSH_FEATURE_SESSION_TCP requires session core and TCP transport"
#endif



#if defined(XSSH_FEATURE_SESSION_TCP)

/* TCP 会话配置只组合 transport 预算与外部 connection 查找器，不接管外部对象。 */
typedef struct xsshsessiontcpconfig {
	xsshtransporttcpconfig Transport;
	xsshchannelresolveproc ChannelResolve;
	ptr ChannelUserData;
	xsshreplyqueue* GlobalReplies;
} xsshsessiontcpconfig;



/* 一次读取同时保留 packet 线路信息和连接级轻量解析结果。 */
typedef struct xsshsessiontcppacket {
	xsshpacketview Transport;
	xsshsessionpacket Session;
} xsshsessiontcppacket;



/*
	TCP 会话只拥有动态 transport 与协议核心，不拥有 Stream、等待、时钟、凭据或 channel 表。
	ReadPacket 和 ReadVersion 都是未决读事务期间的借用视图，不增加固定报文缓冲。
*/
typedef struct xsshsessiontcp {
	xsshtransporttcp Transport;
	xsshsessioncore Session;
	xsshpacketview ReadPacket;
	xstrview ReadVersion;
	void* ReadPlain;
	size_t ReadPlainCapacity;
	uint32 Guard;
} xsshsessiontcp;



XRT_EXTERN_C_BEGIN



/* 写入指定角色的默认 transport、rekey 与空外部 connection 配置。 */
XRT_API bool xrtSshSessionTcpConfigInit(
	xsshsessiontcpconfig* pConfig,
	xsshrole Role
);



/* 使用同一动态缓冲池初始化 TCP transport 与连接级协议核心。 */
XRT_API bool xrtSshSessionTcpInit(
	xsshsessiontcp* pSession,
	xnetbufpool* pPool,
	const xsshsessiontcpconfig* pConfig,
	uint64 iNowMs
);



/* 放弃未决事务、释放动态链并清除全部密码状态；不处理 Stream 和外部对象。 */
XRT_API void xrtSshSessionTcpClear(xsshsessiontcp* pSession);



/* 返回可直接访问 packet、cipher、rekey 和 TCP 动态缓冲的 transport。 */
XRT_API xsshtransporttcp* xrtSshSessionTcpTransport(
	xsshsessiontcp* pSession
);



/* 返回只读 TCP transport；无效对象返回空。 */
XRT_API const xsshtransporttcp* xrtSshSessionTcpTransportConst(
	const xsshsessiontcp* pSession
);



/* 返回 KEX、认证和 connection 编排核心，保留全部底层访问能力。 */
XRT_API xsshsessioncore* xrtSshSessionTcpCore(xsshsessiontcp* pSession);



/* 返回只读连接级协议核心；无效对象返回空。 */
XRT_API const xsshsessioncore* xrtSshSessionTcpCoreConst(
	const xsshsessiontcp* pSession
);



/* 返回当前 identification、KEX、认证、connection、rekey 或失败阶段。 */
XRT_API xsshsessionphase xrtSshSessionTcpPhase(
	const xsshsessiontcp* pSession
);



/* 返回统一的 identification、KEX、认证或 connection 下一动作。 */
XRT_API xsshsessionaction xrtSshSessionTcpAction(
	const xsshsessiontcp* pSession
);



/* 使用显式 Curve25519 私钥开始当前已就绪的 KEX。 */
XRT_API xsshcode xrtSshSessionTcpKexBeginWithPrivate(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 首轮 KEX 完成后开始 USERAUTH；策略和单调时钟仍由调用方提供。 */
XRT_API xsshcode xrtSshSessionTcpAuthBegin(
	xsshsessiontcp* pSession,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
);



/* 同时准备本端版本 transcript 与唯一 identification 线路输出。 */
XRT_API xsshcode xrtSshSessionTcpIdentificationWritePrepare(
	xsshsessiontcp* pSession,
	xstrview Version
);



/* 同时准备上层协议事务和使用调用方 padding 的唯一 packet 线路输出。 */
XRT_API xsshcode xrtSshSessionTcpWritePrepareWithPadding(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	xsshpaddingproc pPadding,
	ptr pPaddingData,
	uint64 iNowMs,
	xsshsessionpacketkind* pKind
);



/*
	把未决 identification 或 packet 零复制交给 Stream。
	AGAIN 和网络 ERROR 保留两层事务，成功接管才按 transport、session 顺序共同提交。
*/
XRT_API xnetresult xrtSshSessionTcpWriteSubmit(
	xsshsessiontcp* pSession,
	xnetstream* pStream,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 先回滚上层候选，再放弃尚未进入 TCP 队列的动态输出。 */
XRT_API xsshcode xrtSshSessionTcpWriteAbort(xsshsessiontcp* pSession);



/* 返回当前可直接重试提交的 identification 或 packet 线路字节数。 */
XRT_API size_t xrtSshSessionTcpWriteSize(
	const xsshsessiontcp* pSession
);



/* 从分块 TCP 输入准备 peer identification，并同步保存版本 transcript。 */
XRT_API xsshcode xrtSshSessionTcpIdentificationReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	xstrview* pVersion
);



/* 只探测下一 packet 的线路尺寸和明文工作区需求，不改变会话状态。 */
XRT_API xsshcode xrtSshSessionTcpReadInspect(
	const xsshsessiontcp* pSession,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
);



/*
	认证并轻量解析下一 packet；返回值借用到 ReadCommit/Abort。
	主机密钥空间不足时保留 transport 事务，使用相同 Input 和 Plain 扩容后可重试。
*/
XRT_API xsshcode xrtSshSessionTcpReadPrepare(
	xsshsessiontcp* pSession,
	xnetbuf* pInput,
	void* pPlain,
	size_t iPlainCapacity,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	uint64 iNowMs,
	xsshsessiontcppacket* pPacket
);



/* 先消费并提交 transport，再提交版本或协议事务并按需切换读密钥。 */
XRT_API xsshcode xrtSshSessionTcpReadCommit(
	xsshsessiontcp* pSession,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 拒绝当前借用输入，消费对应线路前缀并终止不可继续的 transport 与会话。 */
XRT_API xsshcode xrtSshSessionTcpReadAbort(xsshsessiontcp* pSession);



XRT_EXTERN_C_END

#endif

#endif
