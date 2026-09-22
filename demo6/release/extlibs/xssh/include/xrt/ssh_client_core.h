#ifndef XRT_SSH_CLIENT_CORE_H
#define XRT_SSH_CLIENT_CORE_H

#include <xrt/ssh_auth_password.h>
#include <xrt/ssh_kexinit_random.h>
#include <xrt/ssh_session_reader.h>
#include <xrt/ssh_session_tcp_random.h>



#if defined(XSSH_FEATURE_CLIENT_CORE) && \
	(!defined(XSSH_FEATURE_AUTH_PASSWORD) || \
	 !defined(XSSH_FEATURE_KEXINIT_RANDOM) || \
	 !defined(XSSH_FEATURE_SESSION_READER) || \
	 !defined(XSSH_FEATURE_SESSION_TCP_RANDOM))
	#error "XSSH_FEATURE_CLIENT_CORE requires password auth, secure KEXINIT, session reader and secure TCP session helpers"
#endif



#if defined(XSSH_FEATURE_CLIENT_CORE)

#define XSSH_CLIENT_OUTPUT_INITIAL_DEFAULT 4096u
#define XSSH_CLIENT_OUTPUT_LIMIT_DEFAULT 1048576u
#define XSSH_CLIENT_VERSION_DEFAULT "SSH-2.0-xssh"



typedef struct xsshclientcore xsshclientcore;



/* 主机密钥策略必须显式接受；延迟决定时底层连接保持在 VERIFY_HOST_KEY。 */
typedef enum xsshclienthostdecision {
	XSSH_CLIENT_HOST_REJECT = 0,
	XSSH_CLIENT_HOST_ACCEPT = 1,
	XSSH_CLIENT_HOST_DEFER = 2
} xsshclienthostdecision;



/* 一次主机验证同时提供完整 key blob 和本轮稳定协商结果。 */
typedef struct xsshclienthost {
	xbytesview Key;
	xsshkexnegotiation Negotiation;
} xsshclienthost;



/* 认证构建器可根据服务端方法列表和已经提交的尝试数选择下一种方法。 */
typedef struct xsshclientauth {
	xstrview User;
	xstrview Methods;
	xbytesview SessionId;
	uint32 Attempts;
	bool PartialSuccess;
} xsshclientauth;



typedef xsshclienthostdecision (*xsshclienthostproc)(
	xsshclientcore* pClient,
	const xsshclienthost* pHost,
	ptr pUserData
);



/* 返回 SPACE 时核心扩展动态输出并重试；NEED_MORE 表示凭据尚未就绪。 */
typedef xsshcode (*xsshclientauthproc)(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
);



/* 客户端核心配置只借用文本、凭据上下文和回调，不拥有网络对象。 */
typedef struct xsshclientcoreconfig {
	xsshkexinitconfig Kex;
	xsshauthguardpolicy AuthGuard;
	xstrview Version;
	xstrview User;
	xsshclienthostproc HostKey;
	ptr HostKeyData;
	xsshclientauthproc Authenticate;
	ptr AuthenticateData;
	size_t OutputInitial;
	size_t OutputLimit;
	bool ProbeNone;
} xsshclientcoreconfig;



/* Next 返回的结果区分线路输出、输入等待、外部决策和 connection 就绪。 */
typedef enum xsshclientnextkind {
	XSSH_CLIENT_NEXT_INPUT = 0,
	XSSH_CLIENT_NEXT_TRANSACTION = 1,
	XSSH_CLIENT_NEXT_IDENTIFICATION = 2,
	XSSH_CLIENT_NEXT_PAYLOAD = 3,
	XSSH_CLIENT_NEXT_HOST_KEY = 4,
	XSSH_CLIENT_NEXT_AUTH = 5,
	XSSH_CLIENT_NEXT_READY = 6,
	XSSH_CLIENT_NEXT_CLOSING = 7
} xsshclientnextkind;



/* IDENTIFICATION 使用 Text，PAYLOAD 使用 Data，其余分类返回空视图。 */
typedef struct xsshclientnext {
	xstrview Text;
	xbytesview Data;
	xsshclientnextkind Kind;
} xsshclientnext;



/* 核心拥有可增长敏感输出和服务端方法副本，不拥有 SSH 会话与 Reader。 */
struct xsshclientcore {
	xsshclientcoreconfig Config;
	bytes Output;
	char* AuthMethods;
	size_t OutputCapacity;
	size_t AuthMethodsSize;
	bool AuthPartialSuccess;
	bool Initialized;
	uint32 Guard;
};



XRT_EXTERN_C_BEGIN



/* 写入客户端安全默认值；默认拒绝未配置验证器的主机密钥。 */
XRT_API bool xrtSshClientCoreConfigInit(xsshclientcoreconfig* pConfig);



/* 复制配置并创建有界动态输出；所有借用配置必须存活到 Clear。 */
XRT_API bool xrtSshClientCoreInit(
	xsshclientcore* pClient,
	const xsshclientcoreconfig* pConfig
);



/* 安全清除可能包含口令的输出、认证方法和配置借用视图。 */
XRT_API void xrtSshClientCoreClear(xsshclientcore* pClient);



/*
	推进全部无线路等待的客户端动作，直到需要输入、输出、外部决定或已经就绪。
	PAYLOAD 借用核心动态输出，调用方必须在再次调用 Next 前完成 SessionTcpWritePrepare。
*/
XRT_API xsshcode xrtSshClientCoreNext(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession,
	const xsshsessionreader* pReader,
	uint64 iNowMs,
	xsshclientnext* pNext
);



/* 在认证 packet 提交前复制 FAILURE 方法列表；空间不足可重试同一输入。 */
XRT_API xsshcode xrtSshClientCoreObserve(
	xsshclientcore* pClient,
	const xsshsessiontcp* pSession,
	const xsshsessiontcppacket* pPacket
);



/* 显式完成此前延迟的主机密钥决定；拒绝会终止当前 KEX。 */
XRT_API xsshcode xrtSshClientCoreHostKeyAccept(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
);
XRT_API xsshcode xrtSshClientCoreHostKeyReject(
	xsshclientcore* pClient,
	xsshsessiontcp* pSession
);



/* 返回最近一次 USERAUTH_FAILURE 的稳定方法列表与 partial-success 标记。 */
XRT_API xstrview xrtSshClientCoreAuthMethods(
	const xsshclientcore* pClient,
	bool* pPartialSuccess
);



/* 通用 password 构建器；UserData 指向在调用期间有效的 xstrview 口令。 */
XRT_API xsshcode xrtSshClientPasswordAuth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif

#endif
