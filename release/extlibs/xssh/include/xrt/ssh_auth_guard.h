#ifndef XRT_SSH_AUTH_GUARD_H
#define XRT_SSH_AUTH_GUARD_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_AUTH_GUARD) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_AUTH_GUARD requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_AUTH_GUARD)

#define XSSH_AUTH_DEFAULT_TIMEOUT_MS UINT64_C(600000)
#define XSSH_AUTH_DEFAULT_BYTE_LIMIT UINT64_C(16777216)
#define XSSH_AUTH_DEFAULT_ATTEMPT_LIMIT 20u
#define XSSH_AUTH_DEFAULT_ROUND_LIMIT 32u
#define XSSH_AUTH_DEFAULT_MESSAGE_LIMIT 256u



/* 每条认证消息只选择一个预算事件，避免重复计数。 */
typedef enum xsshauthevent {
	XSSH_AUTH_EVENT_MESSAGE = 0,
	XSSH_AUTH_EVENT_ATTEMPT,
	XSSH_AUTH_EVENT_ROUND
} xsshauthevent;



/* Guard 决策区分正常处理、成功后忽略和必须断开。 */
typedef enum xsshauthguarddecision {
	XSSH_AUTH_GUARD_ALLOW = 0,
	XSSH_AUTH_GUARD_IGNORE,
	XSSH_AUTH_GUARD_DISCONNECT
} xsshauthguarddecision;



/* 首个耗尽原因保持稳定，便于结构化错误和统计。 */
typedef enum xsshauthexhaustion {
	XSSH_AUTH_EXHAUST_NONE = 0,
	XSSH_AUTH_EXHAUST_TIMEOUT,
	XSSH_AUTH_EXHAUST_ATTEMPTS,
	XSSH_AUTH_EXHAUST_ROUNDS,
	XSSH_AUTH_EXHAUST_MESSAGES,
	XSSH_AUTH_EXHAUST_BYTES
} xsshauthexhaustion;



/* 零值单项限制表示禁用；时间统一使用单调毫秒。 */
typedef struct xsshauthguardpolicy {
	uint64 TimeoutMs;
	uint64 ByteLimit;
	uint32 AttemptLimit;
	uint32 RoundLimit;
	uint32 MessageLimit;
} xsshauthguardpolicy;



/* Guard 只保存会话总预算，不保存用户名、凭据或报文借用视图。 */
typedef struct xsshauthguard {
	xsshauthguardpolicy Policy;
	uint64 StartedMs;
	uint64 Bytes;
	uint32 Attempts;
	uint32 Rounds;
	uint32 Messages;
	xsshauthexhaustion Exhaustion;
	bool Complete;
	bool Initialized;
} xsshauthguard;



XRT_EXTERN_C_BEGIN



/* 初始化 RFC 推荐值和有界资源默认策略。 */
XRT_API void xrtSshAuthGuardPolicyInit(xsshauthguardpolicy* pPolicy);



/* 复制显式或默认策略并开始一个认证会话。 */
XRT_API bool xrtSshAuthGuardInit(
	xsshauthguard* pGuard,
	const xsshauthguardpolicy* pPolicy,
	uint64 iNowMs
);



/* 查询当前时间、完成状态和已有预算产生的决策。 */
XRT_API xsshcode xrtSshAuthGuardCheck(
	xsshauthguard* pGuard,
	uint64 iNowMs,
	xsshauthguarddecision* pDecision
);



/* 原子预留一条认证消息；超过任一上限时进入不可恢复的断开状态。 */
XRT_API xsshcode xrtSshAuthGuardReserve(
	xsshauthguard* pGuard,
	xsshauthevent Event,
	uint64 iMessageBytes,
	uint64 iNowMs,
	xsshauthguarddecision* pDecision
);



/* 认证成功后冻结预算；后续认证消息统一返回 IGNORE。 */
XRT_API bool xrtSshAuthGuardComplete(xsshauthguard* pGuard);



XRT_EXTERN_C_END

#endif

#endif
