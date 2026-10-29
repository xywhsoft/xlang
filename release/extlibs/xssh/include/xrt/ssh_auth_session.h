#ifndef XRT_SSH_AUTH_SESSION_H
#define XRT_SSH_AUTH_SESSION_H

#include <xrt/ssh_auth_guard.h>
#include <xrt/ssh_auth_message.h>
#include <xrt/ssh_transport_core.h>



#if defined(XSSH_FEATURE_AUTH_SESSION) && \
	(!defined(XSSH_FEATURE_AUTH_GUARD) || \
	 !defined(XSSH_FEATURE_AUTH_MESSAGE) || \
	 !defined(XSSH_FEATURE_TRANSPORT_CORE))
	#error "XSSH_FEATURE_AUTH_SESSION requires auth guard, auth message and transport core"
#endif



#if defined(XSSH_FEATURE_AUTH_SESSION)

/* 会话阶段只表达认证编排，不表达 socket、等待或认证后端状态。 */
typedef enum xsshauthsessionphase {
	XSSH_AUTH_SESSION_IDLE = 0,
	XSSH_AUTH_SESSION_SERVICE = 1,
	XSSH_AUTH_SESSION_AUTHENTICATION = 2,
	XSSH_AUTH_SESSION_COMPLETE = 3,
	XSSH_AUTH_SESSION_FAILED = 4
} xsshauthsessionphase;



/* Event 表达驱动下一步应发送或接收哪类认证消息。 */
typedef enum xsshauthsessionevent {
	XSSH_AUTH_SESSION_EVENT_NONE = 0,
	XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_REQUEST = 1,
	XSSH_AUTH_SESSION_EVENT_READ_SERVICE_REQUEST = 2,
	XSSH_AUTH_SESSION_EVENT_WRITE_SERVICE_ACCEPT = 3,
	XSSH_AUTH_SESSION_EVENT_READ_SERVICE_ACCEPT = 4,
	XSSH_AUTH_SESSION_EVENT_WRITE_REQUEST = 5,
	XSSH_AUTH_SESSION_EVENT_READ_REQUEST = 6,
	XSSH_AUTH_SESSION_EVENT_WRITE_RESULT = 7,
	XSSH_AUTH_SESSION_EVENT_READ_RESULT = 8,
	XSSH_AUTH_SESSION_EVENT_COMPLETE = 9,
	XSSH_AUTH_SESSION_EVENT_FAILED = 10
} xsshauthsessionevent;



/* Packet 对通用认证编排分类，METHOD 的字段由具体认证模块解释。 */
typedef enum xsshauthsessionpacket {
	XSSH_AUTH_SESSION_PACKET_NONE = 0,
	XSSH_AUTH_SESSION_PACKET_SERVICE_REQUEST = 1,
	XSSH_AUTH_SESSION_PACKET_SERVICE_ACCEPT = 2,
	XSSH_AUTH_SESSION_PACKET_REQUEST = 3,
	XSSH_AUTH_SESSION_PACKET_FAILURE = 4,
	XSSH_AUTH_SESSION_PACKET_SUCCESS = 5,
	XSSH_AUTH_SESSION_PACKET_BANNER = 6,
	XSSH_AUTH_SESSION_PACKET_METHOD = 7
} xsshauthsessionpacket;



/*
	对象不拥有 payload；Request、Failure、Banner 和 Method 只在读事务期间借用输入。
	同一对象由一个执行流推进，认证后端需要异步决策时由调用方复制所需字段。
*/
typedef struct xsshauthsession {
	xsshauthguard Budget;
	xsshauthguard PendingBudget;
	xsshauthrequest Request;
	xsshauthfailure Failure;
	xsshauthbanner Banner;
	xbytesview Method;
	uint64 WriteOrdinal;
	uint64 ReadOrdinal;
	xsshrole Role;
	xsshauthsessionphase Phase;
	xsshauthsessionevent Event;
	xsshauthsessionpacket WritePending;
	xsshauthsessionpacket ReadPending;
	bool Active;
	bool ServiceAccepted;
	bool ContinueAllowed;
	uint32 ObjectGuard;
} xsshauthsession;



XRT_EXTERN_C_BEGIN



/* 初始化不拥有外部资源的 client 或 server 认证会话。 */
XRT_API bool xrtSshAuthSessionInit(
	xsshauthsession* pSession,
	xsshrole Role
);



/* 清除会话状态和当前借用视图，不处理 transport 或调用方缓冲。 */
XRT_API void xrtSshAuthSessionClear(xsshauthsession* pSession);



/* 在首轮 KEX 完成后开始 service 与 USERAUTH 编排。 */
XRT_API xsshcode xrtSshAuthSessionBegin(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
);



/* 返回当前最常见的下一动作，不推进任何状态。 */
XRT_API xsshauthsessionevent xrtSshAuthSessionEvent(
	const xsshauthsession* pSession
);



/* 复制当前认证资源预算，输出不借用会话内部地址。 */
XRT_API xsshcode xrtSshAuthSessionBudget(
	const xsshauthsession* pSession,
	xsshauthguard* pBudget
);



/* 检查认证超时和资源预算；耗尽时会话进入失败状态。 */
XRT_API xsshcode xrtSshAuthSessionCheck(
	xsshauthsession* pSession,
	uint64 iNowMs,
	xsshauthguarddecision* pDecision
);



/*
	验证一个已经构建的 service、USERAUTH 或方法 payload，并准备写事务。
	函数不复制、不修改 payload，也不生成 packet framing。
*/
XRT_API xsshcode xrtSshAuthSessionWritePrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint64 iNowMs
);



/* transport core 已可靠提交当前输出后提交认证写事务。 */
XRT_API xsshcode xrtSshAuthSessionWriteCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃尚未交给 transport 的认证输出，不消费状态或资源预算。 */
XRT_API xsshcode xrtSshAuthSessionWriteAbort(xsshauthsession* pSession);



/*
	解析 transport core 已认证准备的 peer payload，并返回通用消息类别。
	成功后必须先读取所需借用视图，再依次提交 core 和 session。
*/
XRT_API xsshcode xrtSshAuthSessionReadPrepare(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore,
	xbytesview Payload,
	uint64 iNowMs,
	xsshauthsessionpacket* pPacket
);



/* 返回当前待读事务中的通用 USERAUTH_REQUEST 借用视图。 */
XRT_API xsshcode xrtSshAuthSessionRequest(
	const xsshauthsession* pSession,
	xsshauthrequest* pRequest
);



/* 返回当前待读事务中的 USERAUTH_FAILURE 借用视图。 */
XRT_API xsshcode xrtSshAuthSessionFailure(
	const xsshauthsession* pSession,
	xsshauthfailure* pFailure
);



/* 返回当前待读事务中的 USERAUTH_BANNER 借用视图。 */
XRT_API xsshcode xrtSshAuthSessionBanner(
	const xsshauthsession* pSession,
	xsshauthbanner* pBanner
);



/* 返回当前待读事务中的方法专用完整 payload。 */
XRT_API xsshcode xrtSshAuthSessionMethod(
	const xsshauthsession* pSession,
	xbytesview* pPayload
);



/* transport core 已提交当前输入后提交认证读事务。 */
XRT_API xsshcode xrtSshAuthSessionReadCommit(
	xsshauthsession* pSession,
	const xsshtransportcore* pCore
);



/* 放弃已认证但不能接受的认证输入，并把会话置为失败状态。 */
XRT_API xsshcode xrtSshAuthSessionReadAbort(xsshauthsession* pSession);



/* 显式终止认证编排；重复调用保持失败状态。 */
XRT_API void xrtSshAuthSessionFail(xsshauthsession* pSession);



/* 判断本端会话、预算和 transport 的 server 成功方向都已提交。 */
XRT_API bool xrtSshAuthSessionComplete(
	const xsshauthsession* pSession,
	const xsshtransportcore* pCore
);



XRT_EXTERN_C_END

#endif

#endif
