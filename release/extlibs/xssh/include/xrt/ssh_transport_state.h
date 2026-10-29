#ifndef XRT_SSH_TRANSPORT_STATE_H
#define XRT_SSH_TRANSPORT_STATE_H

#include <xrt/ssh_kexinit.h>
#include <xrt/ssh_transport_message.h>



#if defined(XSSH_FEATURE_TRANSPORT_STATE) && \
	(!defined(XSSH_FEATURE_KEXINIT) || \
	 !defined(XSSH_FEATURE_TRANSPORT_MESSAGE))
	#error "XSSH_FEATURE_TRANSPORT_STATE requires KEXINIT and transport messages"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_STATE)

#define XSSH_KEX_METHOD_MIN 30u
#define XSSH_KEX_METHOD_MAX 49u
#define XSSH_KEX_METHOD_COUNT 20u

#if !defined(XSSH_MSG_USERAUTH_SUCCESS)
	#define XSSH_MSG_USERAUTH_SUCCESS 52u
#endif



/* Transport 阶段不表示网络状态，只表示已经可靠提交的 SSH 协议状态。 */
typedef enum xsshtransportphase {
	XSSH_TRANSPORT_IDENTIFICATION = 0,
	XSSH_TRANSPORT_KEY_EXCHANGE = 1,
	XSSH_TRANSPORT_OPEN = 2,
	XSSH_TRANSPORT_CLOSING = 3,
	XSSH_TRANSPORT_CLOSED = 4
} xsshtransportphase;



/* LOCAL 表示本端发送方向，PEER 表示认证完成的对端接收方向。 */
typedef enum xsshtransportdirection {
	XSSH_TRANSPORT_LOCAL = 0,
	XSSH_TRANSPORT_PEER = 1
} xsshtransportdirection;



/* NEWKEYS 提交动作由调用方在对应 packet codec 方向执行。 */
typedef enum xsshtransportaction {
	XSSH_TRANSPORT_ACTION_NONE = 0,
	XSSH_TRANSPORT_ACTION_ACTIVATE_KEYS = 1,
	XSSH_TRANSPORT_ACTION_RESET_SEQUENCE = 2,
	XSSH_TRANSPORT_ACTION_KEX_COMPLETE = 4
} xsshtransportaction;



/* 每个 KEX 方法消息的额度为精确次数，零表示该方向禁止。 */
typedef struct xsshtransportkexrules {
	uint8 Local[XSSH_KEX_METHOD_COUNT];
	uint8 Peer[XSSH_KEX_METHOD_COUNT];
	uint32 Guard;
} xsshtransportkexrules;



/*
 * 状态对象不拥有 KEXINIT payload、密钥、时钟、socket 或缓冲。
 * 单个对象只能由一个执行流推进，跨线程串行化由调用方负责。
 */
typedef struct xsshtransportstate {
	uint8 LocalKexRemaining[XSSH_KEX_METHOD_COUNT];
	uint8 PeerKexRemaining[XSSH_KEX_METHOD_COUNT];
	uint64 LocalPackets;
	uint64 PeerPackets;
	uint64 LocalKexInitOrdinal;
	uint64 PeerKexInitOrdinal;
	uint64 KexCount;
	xsshrole Role;
	xsshtransportphase Phase;
	uint8 LocalGuessMessage;
	uint8 PeerGuessMessage;
	bool LocalIdentification;
	bool PeerIdentification;
	bool LocalKexInit;
	bool PeerKexInit;
	bool LocalNewKeys;
	bool PeerNewKeys;
	bool KexConfigured;
	bool Strict;
	bool AcceptExtInfo;
	bool SendExtInfo;
	bool LocalGuessExpected;
	bool PeerGuessExpected;
	bool LocalGuessSeen;
	bool PeerGuessSeen;
	bool LocalGuessSkip;
	bool PeerGuessSkip;
	bool LocalStrictViolation;
	bool PeerStrictViolation;
	bool LocalFirstExtOpen;
	bool PeerFirstExtOpen;
	bool LocalFirstExtUsed;
	bool PeerFirstExtUsed;
	bool LocalSecondExtUsed;
	bool PeerSecondExtUsed;
	bool LocalAuthSuccessPending;
	bool PeerAuthSuccessPending;
	bool LocalAuthSuccess;
	bool PeerAuthSuccess;
	uint32 Guard;
} xsshtransportstate;



XRT_EXTERN_C_BEGIN



/* 初始化空 KEX 方法规则。 */
XRT_API bool xrtSshTransportKexRulesInit(xsshtransportkexrules* pRules);



/* 设置一个 30..49 方法消息在本端或对端方向的精确额度。 */
XRT_API bool xrtSshTransportKexRuleSet(
	xsshtransportkexrules* pRules,
	xsshtransportdirection Direction,
	uint8 iMessage,
	uint8 iCount
);



/* 初始化一个不拥有外部资源的 client 或 server transport。 */
XRT_API bool xrtSshTransportStateInit(
	xsshtransportstate* pState,
	xsshrole Role
);



/* 清除状态；不会清理任何调用方密钥或缓冲。 */
XRT_API void xrtSshTransportStateClear(xsshtransportstate* pState);



/* 提交本端 identification 已发送或对端 identification 已验证。 */
XRT_API xsshcode xrtSshTransportIdentificationCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 判断对应方向当前是否可以提交应用层消息。 */
XRT_API bool xrtSshTransportCanApplication(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 判断是否已收到对端 KEXINIT，且本端必须回复 KEXINIT。 */
XRT_API bool xrtSshTransportKexReplyNeeded(
	const xsshtransportstate* pState
);



/* 检查 KEXINIT 是否可在指定方向可靠提交。 */
XRT_API xsshcode xrtSshTransportKexInitCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 提交 KEXINIT，并记录下一包是否为猜测的 KEX 方法消息。 */
XRT_API xsshcode xrtSshTransportKexInitCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	bool bFirstKexPacketFollows
);



/*
 * 双方 KEXINIT 到达后提交本代协商和方法规则。
 * KEXINIT 与协商视图仅在调用期间借用，不保存在状态对象中。
 */
XRT_API xsshcode xrtSshTransportKexConfigure(
	xsshtransportstate* pState,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
);



/* 检查普通 transport、KEX 方法或应用消息；KEXINIT/NEWKEYS 使用专用 API。 */
XRT_API xsshcode xrtSshTransportMessageCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
);



/* 在消息已可靠入队或已认证接收后提交普通消息。 */
XRT_API xsshcode xrtSshTransportMessageCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint8 iMessage
);



/* 检查对应方向已经完成全部 KEX 方法消息，可以发送或接受 NEWKEYS。 */
XRT_API xsshcode xrtSshTransportNewKeysCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 提交 NEWKEYS，并返回密钥切换、序列重置和整代完成动作。 */
XRT_API xsshcode xrtSshTransportNewKeysCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction,
	uint32* pActions
);



/* 检查 server USERAUTH_SUCCESS；用于约束第二次 EXT_INFO 必须紧邻在前。 */
XRT_API xsshcode xrtSshTransportAuthSuccessCheck(
	const xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 提交 server USERAUTH_SUCCESS；消息格式仍由 ssh_auth_message 处理。 */
XRT_API xsshcode xrtSshTransportAuthSuccessCommit(
	xsshtransportstate* pState,
	xsshtransportdirection Direction
);



/* 网络关闭后终止状态机；重复调用保持关闭状态。 */
XRT_API void xrtSshTransportClose(xsshtransportstate* pState);



XRT_EXTERN_C_END

#endif

#endif
