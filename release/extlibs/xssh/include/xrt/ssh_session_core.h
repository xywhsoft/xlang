#ifndef XRT_SSH_SESSION_CORE_H
#define XRT_SSH_SESSION_CORE_H

#include <xrt/ssh_auth_session.h>
#include <xrt/ssh_connection_session.h>
#include <xrt/ssh_kex_exchange.h>



#if defined(XSSH_FEATURE_SESSION_CORE) && \
	(!defined(XSSH_FEATURE_AUTH_SESSION) || \
	 !defined(XSSH_FEATURE_CONNECTION_SESSION) || \
	 !defined(XSSH_FEATURE_KEX_EXCHANGE))
	#error "XSSH_FEATURE_SESSION_CORE requires KEX, auth and connection sessions"
#endif



#if defined(XSSH_FEATURE_SESSION_CORE)

/* 连接级阶段只描述协议编排，不描述 socket、等待或任务状态。 */
typedef enum xsshsessionphase {
	XSSH_SESSION_IDENTIFICATION = 0,
	XSSH_SESSION_KEY_EXCHANGE = 1,
	XSSH_SESSION_AUTHENTICATION = 2,
	XSSH_SESSION_CONNECTION = 3,
	XSSH_SESSION_REKEY = 4,
	XSSH_SESSION_CLOSING = 5,
	XSSH_SESSION_FAILED = 6
} xsshsessionphase;



/*
	Action 把 identification、KEX 与认证子状态统一为驱动的下一项常见动作。
	WRITE/READ_PENDING 表示事务必须先提交或中止，CONNECTION 表示由应用层选择消息。
*/
typedef enum xsshsessionaction {
	XSSH_SESSION_ACTION_NONE = 0,
	XSSH_SESSION_ACTION_WRITE_IDENTIFICATION = 1,
	XSSH_SESSION_ACTION_READ_IDENTIFICATION = 2,
	XSSH_SESSION_ACTION_WRITE_KEXINIT = 3,
	XSSH_SESSION_ACTION_READ_KEXINIT = 4,
	XSSH_SESSION_ACTION_BEGIN_KEX = 5,
	XSSH_SESSION_ACTION_WRITE_ECDH_INIT = 6,
	XSSH_SESSION_ACTION_READ_ECDH_INIT = 7,
	XSSH_SESSION_ACTION_WRITE_ECDH_REPLY = 8,
	XSSH_SESSION_ACTION_READ_ECDH_REPLY = 9,
	XSSH_SESSION_ACTION_VERIFY_HOST_KEY = 10,
	XSSH_SESSION_ACTION_WRITE_NEWKEYS = 11,
	XSSH_SESSION_ACTION_READ_NEWKEYS = 12,
	XSSH_SESSION_ACTION_ACTIVATE_WRITE_KEYS = 13,
	XSSH_SESSION_ACTION_ACTIVATE_READ_KEYS = 14,
	XSSH_SESSION_ACTION_COMPLETE_KEX = 15,
	XSSH_SESSION_ACTION_BEGIN_AUTH = 16,
	XSSH_SESSION_ACTION_WRITE_SERVICE_REQUEST = 17,
	XSSH_SESSION_ACTION_READ_SERVICE_REQUEST = 18,
	XSSH_SESSION_ACTION_WRITE_SERVICE_ACCEPT = 19,
	XSSH_SESSION_ACTION_READ_SERVICE_ACCEPT = 20,
	XSSH_SESSION_ACTION_WRITE_AUTH_REQUEST = 21,
	XSSH_SESSION_ACTION_READ_AUTH_REQUEST = 22,
	XSSH_SESSION_ACTION_WRITE_AUTH_RESULT = 23,
	XSSH_SESSION_ACTION_READ_AUTH_RESULT = 24,
	XSSH_SESSION_ACTION_COMPLETE_AUTH = 25,
	XSSH_SESSION_ACTION_CONNECTION = 26,
	XSSH_SESSION_ACTION_WRITE_PENDING = 27,
	XSSH_SESSION_ACTION_READ_PENDING = 28,
	XSSH_SESSION_ACTION_CLOSING = 29,
	XSSH_SESSION_ACTION_FAILED = 30
} xsshsessionaction;



/* Packet 分类保留 transport 控制消息和未知扩展的直接访问路径。 */
typedef enum xsshsessionpacketkind {
	XSSH_SESSION_PACKET_NONE = 0,
	XSSH_SESSION_PACKET_DISCONNECT = 1,
	XSSH_SESSION_PACKET_IGNORE = 2,
	XSSH_SESSION_PACKET_UNIMPLEMENTED = 3,
	XSSH_SESSION_PACKET_DEBUG = 4,
	XSSH_SESSION_PACKET_EXT_INFO = 5,
	XSSH_SESSION_PACKET_NEWCOMPRESS = 6,
	XSSH_SESSION_PACKET_KEXINIT = 7,
	XSSH_SESSION_PACKET_KEX = 8,
	XSSH_SESSION_PACKET_AUTH = 9,
	XSSH_SESSION_PACKET_CONNECTION = 10,
	XSSH_SESSION_PACKET_EXTENSION = 11
} xsshsessionpacketkind;



/* 消息视图只在对应 transport 读事务提交或中止前有效。 */
typedef union xsshsessionmessage {
	xsshdisconnect Disconnect;
	xsshignore Ignore;
	uint32 UnimplementedSequence;
	xsshdebug Debug;
	xsshextinfo ExtInfo;
	xsshkexsessionpacket Kex;
	xsshauthsessionpacket Auth;
	xsshconnectionpacket Connection;
} xsshsessionmessage;



/* 未知扩展保留完整 Payload，已知消息同时提供轻量解析结果。 */
typedef struct xsshsessionpacket {
	xsshsessionmessage Message;
	xbytesview Payload;
	xsshsessionpacketkind Kind;
	uint8 Number;
} xsshsessionpacket;



/*
	会话核心拥有 KEX transcript、认证和 connection 状态，不拥有 transport、channel 表或凭据。
	同一对象由一个执行流推进；公开字段只供诊断读取，子对象通过访问器继续暴露底层能力。
*/
typedef struct xsshsessioncore {
	xsshkexexchange Kex;
	xsshauthsession Auth;
	xsshconnectionsession Connection;
	xbytesview WritePayload;
	uint64 WriteOrdinal;
	uint64 ReadOrdinal;
	xsshrole Role;
	xsshsessionpacketkind WritePending;
	xsshsessionpacketkind ReadPending;
	uint8 WriteMessage;
	uint8 ReadMessage;
	bool WriteBound;
	bool Initialized;
	bool Failed;
	uint32 Guard;
} xsshsessioncore;



XRT_EXTERN_C_BEGIN



/* 初始化连接级协议核心；动态内存只用于 KEX transcript。 */
XRT_API bool xrtSshSessionCoreInit(
	xsshsessioncore* pSession,
	xnetbufpool* pPool,
	xsshrole Role,
	xsshchannelresolveproc pResolve,
	ptr pUserData,
	xsshreplyqueue* pGlobalReplies
);



/* 释放 transcript、清除密码材料和全部借用事务，不处理 transport 或外部存储。 */
XRT_API void xrtSshSessionCoreClear(xsshsessioncore* pSession);



/* 根据连接级对象和 transport 返回当前协议阶段。 */
XRT_API xsshsessionphase xrtSshSessionCorePhase(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* 返回稳定状态下建议驱动的下一动作；本端 identification 与 KEXINIT 优先于等待对端。 */
XRT_API xsshsessionaction xrtSshSessionCoreAction(
	const xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* 返回可直接使用的 KEX 交换对象。 */
XRT_API xsshkexexchange* xrtSshSessionCoreKex(
	xsshsessioncore* pSession
);



/* 返回只读 KEX 交换对象。 */
XRT_API const xsshkexexchange* xrtSshSessionCoreKexConst(
	const xsshsessioncore* pSession
);



/* 返回认证会话；认证方法和凭据策略仍由调用方驱动。 */
XRT_API xsshauthsession* xrtSshSessionCoreAuth(
	xsshsessioncore* pSession
);



/* 返回只读认证会话。 */
XRT_API const xsshauthsession* xrtSshSessionCoreAuthConst(
	const xsshsessioncore* pSession
);



/* 返回 connection 会话；channel 表和 reply FIFO 仍由调用方持有。 */
XRT_API xsshconnectionsession* xrtSshSessionCoreConnection(
	xsshsessioncore* pSession
);



/* 返回只读 connection 会话。 */
XRT_API const xsshconnectionsession* xrtSshSessionCoreConnectionConst(
	const xsshsessioncore* pSession
);



/* 在 transport identification 提交前保存本端或对端版本串。 */
XRT_API xsshcode xrtSshSessionCoreVersionPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
);



/* transport 已提交对应 identification 后发布版本串。 */
XRT_API xsshcode xrtSshSessionCoreVersionCommit(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* transport 尚未推进时放弃版本串暂存副本。 */
XRT_API xsshcode xrtSshSessionCoreVersionAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/* 使用显式 Curve25519 私钥开始当前已经就绪的一代 KEX。 */
XRT_API xsshcode xrtSshSessionCoreKexBeginWithPrivate(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 首轮 KEX 完成后开始双端 ssh-userauth 编排。 */
XRT_API xsshcode xrtSshSessionCoreAuthBegin(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
);



/*
	在 transport 写 Prepare 前分类并准备一个最终 payload。
	KEX 方法 payload 必须先由 KEX 会话构建；channel 和 FIFO 只用于 connection 消息。
*/
XRT_API xsshcode xrtSshSessionCoreWritePrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	uint64 iNowMs,
	xsshsessionpacketkind* pKind
);



/* transport 已准备同一 payload 后绑定 packet 类型和序号，尚不推进状态。 */
XRT_API xsshcode xrtSshSessionCoreWriteBind(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload
);



/* transport 已可靠提交输出后提交对应 KEX、认证或 connection 事务。 */
XRT_API xsshcode xrtSshSessionCoreWriteCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
);



/* transport 未推进时放弃上层写事务；transport 自身仍由调用方中止。 */
XRT_API xsshcode xrtSshSessionCoreWriteAbort(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore
);



/*
	分类并准备 transport 已认证的 peer payload。
	客户端 ECDH_REPLY 的主机公钥复制到调用方存储，其余消息不使用该存储。
*/
XRT_API xsshcode xrtSshSessionCoreReadPrepare(
	xsshsessioncore* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize,
	uint64 iNowMs,
	xsshsessionpacket* pPacket
);



/* transport 已提交输入后提交上层事务并按需激活读密钥。 */
XRT_API xsshcode xrtSshSessionCoreReadCommit(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
);



/* 放弃不可回滚的认证输入并终止会话；transport 自身仍由调用方中止。 */
XRT_API xsshcode xrtSshSessionCoreReadAbort(xsshsessioncore* pSession);



/* 终止全部协议子状态；transport、网络和外部 channel 由调用方关闭。 */
XRT_API void xrtSshSessionCoreFail(xsshsessioncore* pSession);



XRT_EXTERN_C_END

#endif

#endif
