#ifndef XRT_SSH_KEX_SESSION_H
#define XRT_SSH_KEX_SESSION_H

#include <xrt/ssh_hostkey_ed25519.h>
#include <xrt/ssh_kex_curve25519.h>
#include <xrt/ssh_kex_ecdh.h>
#include <xrt/ssh_kex_sha256.h>
#include <xrt/ssh_transport_core.h>



#if defined(XSSH_FEATURE_KEX_SESSION) && \
	(!defined(XSSH_FEATURE_HOSTKEY_ED25519) || \
	 !defined(XSSH_FEATURE_KEX_CURVE25519) || \
	 !defined(XSSH_FEATURE_KEX_ECDH) || \
	 !defined(XSSH_FEATURE_KEX_SHA256) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE))
	#error "XSSH_FEATURE_KEX_SESSION requires Ed25519, Curve25519, ECDH, SHA-256 and transport core"
#endif



#if defined(XSSH_FEATURE_KEX_SESSION)

#define XSSH_KEX_SESSION_KEY_MAX 32u



/* KEX transcript 借用调用方稳定存储，字段均不包含 packet framing。 */
typedef struct xsshkextranscript {
	xbytesview ClientVersion;
	xbytesview ServerVersion;
	xbytesview ClientKexInit;
	xbytesview ServerKexInit;
} xsshkextranscript;



/* 会话阶段只表达 KEX 编排，不表达 socket、等待或任务状态。 */
typedef enum xsshkexsessionphase {
	XSSH_KEX_SESSION_IDLE = 0,
	XSSH_KEX_SESSION_METHOD = 1,
	XSSH_KEX_SESSION_HOST_KEY = 2,
	XSSH_KEX_SESSION_NEW_KEYS = 3,
	XSSH_KEX_SESSION_COMPLETE = 4,
	XSSH_KEX_SESSION_FAILED = 5
} xsshkexsessionphase;



/* Event 给驱动指出下一项常见动作；接收 NEWKEYS 可按网络到达顺序提前处理。 */
typedef enum xsshkexsessionevent {
	XSSH_KEX_EVENT_NONE = 0,
	XSSH_KEX_EVENT_WRITE_ECDH_INIT = 1,
	XSSH_KEX_EVENT_READ_ECDH_INIT = 2,
	XSSH_KEX_EVENT_WRITE_ECDH_REPLY = 3,
	XSSH_KEX_EVENT_READ_ECDH_REPLY = 4,
	XSSH_KEX_EVENT_VERIFY_HOST_KEY = 5,
	XSSH_KEX_EVENT_WRITE_NEWKEYS = 6,
	XSSH_KEX_EVENT_READ_NEWKEYS = 7,
	XSSH_KEX_EVENT_ACTIVATE_WRITE = 8,
	XSSH_KEX_EVENT_ACTIVATE_READ = 9,
	XSSH_KEX_EVENT_COMPLETE = 10,
	XSSH_KEX_EVENT_FAILED = 11
} xsshkexsessionevent;



/* Prepare 事务类型公开用于诊断，调用方不得直接修改。 */
typedef enum xsshkexsessionpacket {
	XSSH_KEX_PACKET_NONE = 0,
	XSSH_KEX_PACKET_DISCARD = 1,
	XSSH_KEX_PACKET_ECDH_INIT = 2,
	XSSH_KEX_PACKET_ECDH_REPLY = 3,
	XSSH_KEX_PACKET_NEWKEYS = 4
} xsshkexsessionpacket;



/*
	对象只保存固定尺寸密码状态和借用视图，不保存 KEXINIT、packet 或网络缓冲。
	同一对象由一个执行流推进；SessionId 跨 rekey 保留，Clear 时安全清零。
*/
typedef struct xsshkexsession {
	xsshkextranscript Transcript;
	xsshkexnegotiation Negotiation;
	xbytesview ServerHostKey;
	uint8 PrivateKey[XSSH_CURVE25519_PRIVATE_SIZE];
	uint8 PublicKey[XSSH_CURVE25519_PUBLIC_SIZE];
	uint8 PeerPublicKey[XSSH_CURVE25519_PUBLIC_SIZE];
	uint8 SharedSecret[XSSH_CURVE25519_SHARED_SIZE];
	uint8 ExchangeHash[XSSH_SHA256_SIZE];
	uint8 SessionId[XSSH_SHA256_SIZE];
	uint8 ClientToServerIV[XSSH_AES_GCM_IV_SIZE];
	uint8 ServerToClientIV[XSSH_AES_GCM_IV_SIZE];
	uint8 ClientToServerKey[XSSH_KEX_SESSION_KEY_MAX];
	uint8 ServerToClientKey[XSSH_KEX_SESSION_KEY_MAX];
	xsshrole Role;
	xsshkexsessionphase Phase;
	xsshkexsessionpacket WritePending;
	xsshkexsessionpacket ReadPending;
	uint8 ClientToServerKeySize;
	uint8 ServerToClientKeySize;
	bool Active;
	bool HasSessionId;
	bool KeysDerived;
	bool MethodWriteCommitted;
	bool MethodReadCommitted;
	bool HostKeyVerified;
	bool HostKeyAccepted;
	bool LocalNewKeys;
	bool PeerNewKeys;
	bool WriteActivated;
	bool ReadActivated;
	uint32 Guard;
} xsshkexsession;



XRT_EXTERN_C_BEGIN



/* 校验四段 transcript 借用视图；调用方保证其存活到本轮 KEX 完成。 */
XRT_API xsshcode xrtSshKexTranscriptInit(
	xsshkextranscript* pTranscript,
	xbytesview ClientVersion,
	xbytesview ServerVersion,
	xbytesview ClientKexInit,
	xbytesview ServerKexInit
);



/* 计算复制完整 transcript 所需的精确字节数。 */
XRT_API xsshcode xrtSshKexTranscriptMeasure(
	const xsshkextranscript* pTranscript,
	size_t* pSize
);



/* 将 transcript 追加到 writer，并返回借用 writer 输出的稳定视图。 */
XRT_API xsshcode xrtSshKexTranscriptWrite(
	xsshwriter* pWriter,
	const xsshkextranscript* pInput,
	xsshkextranscript* pOutput
);



/* 初始化可重复执行初始 KEX 和 rekey 的确定性会话。 */
XRT_API bool xrtSshKexSessionInit(
	xsshkexsession* pSession,
	xsshrole Role
);



/* 安全清除临时私钥、共享秘密、派生密钥和 SessionId。 */
XRT_API void xrtSshKexSessionClear(xsshkexsession* pSession);



/*
	使用显式 Curve25519 私钥开始一代 KEX，并配置已经提交双方 KEXINIT 的 core。
	服务端必须提供稳定的 ssh-ed25519 HostKey；客户端传空视图。
*/
XRT_API xsshcode xrtSshKexSessionBeginWithPrivate(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	const xsshkextranscript* pTranscript,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 返回当前最常见的下一动作，不推进任何状态。 */
XRT_API xsshkexsessionevent xrtSshKexSessionEvent(
	const xsshkexsession* pSession
);



/* 返回本代协商结果；视图借用 transcript。 */
XRT_API xsshcode xrtSshKexSessionNegotiation(
	const xsshkexsession* pSession,
	xsshkexnegotiation* pNegotiation
);



/* 返回本代 exchange hash，服务端可交给本地密钥或 HSM 签名。 */
XRT_API xsshcode xrtSshKexSessionExchangeHash(
	const xsshkexsession* pSession,
	xbytesview* pHash
);



/* 返回首次 exchange hash 固化的 SessionId。 */
XRT_API xsshcode xrtSshKexSessionId(
	const xsshkexsession* pSession,
	xbytesview* pSessionId
);



/* 返回已完成密码学验签、等待信任策略确认的服务端主机公钥。 */
XRT_API xsshcode xrtSshKexSessionHostKey(
	const xsshkexsession* pSession,
	xbytesview* pHostKey
);



/* 客户端确认主机密钥信任；之后才允许发送 NEWKEYS。 */
XRT_API xsshcode xrtSshKexSessionHostKeyAccept(
	xsshkexsession* pSession
);



/* 将会话置为不可继续状态并安全清除本代秘密。 */
XRT_API void xrtSshKexSessionFail(xsshkexsession* pSession);



/* 客户端准备 SSH_MSG_KEX_ECDH_INIT；可靠提交后调用 WriteCommit。 */
XRT_API xsshcode xrtSshKexSessionEcdhInitPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
);



/* 服务端用外部签名 blob 准备 SSH_MSG_KEX_ECDH_REPLY。 */
XRT_API xsshcode xrtSshKexSessionEcdhReplyPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter,
	xbytesview Signature
);



/* 准备 SSH_MSG_NEWKEYS；方法交换和客户端主机信任必须已完成。 */
XRT_API xsshcode xrtSshKexSessionNewKeysPrepare(
	xsshkexsession* pSession,
	xsshwriter* pWriter
);



/* transport core 已可靠提交当前输出后提交 KEX 写事务。 */
XRT_API xsshcode xrtSshKexSessionWriteCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃尚未交给 transport 的 KEX 输出，不消费任何会话状态。 */
XRT_API xsshcode xrtSshKexSessionWriteAbort(xsshkexsession* pSession);



/*
	解析 transport core 已认证准备的 peer payload。
	客户端 ECDH_REPLY 会把 HostKey 复制到调用方存储，空间不足可按其长度重试。
*/
XRT_API xsshcode xrtSshKexSessionReadPrepare(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	void* pHostKeyStorage,
	size_t iHostKeyCapacity,
	size_t* pHostKeySize
);



/* transport core 已提交当前输入后提交 KEX 读事务。 */
XRT_API xsshcode xrtSshKexSessionReadCommit(
	xsshkexsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃已认证 KEX 输入并终止会话；对应 transport 也必须关闭。 */
XRT_API xsshcode xrtSshKexSessionReadAbort(xsshkexsession* pSession);



/* 本端 NEWKEYS 提交后，把角色对应的派生密钥装入写 codec。 */
XRT_API xsshcode xrtSshKexSessionActivateWrite(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
);



/* 对端 NEWKEYS 提交后，把角色对应的派生密钥装入读 codec。 */
XRT_API xsshcode xrtSshKexSessionActivateRead(
	xsshkexsession* pSession,
	xsshtransportcore* pCore,
	uint64 iNowMs
);



/* 判断本代双向密钥和 transport core 均已完成切换。 */
XRT_API bool xrtSshKexSessionComplete(
	const xsshkexsession* pSession,
	const xsshtransportcore* pCore
);



XRT_EXTERN_C_END

#endif

#endif
