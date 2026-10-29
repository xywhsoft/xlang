#ifndef XRT_SSH_KEX_EXCHANGE_H
#define XRT_SSH_KEX_EXCHANGE_H

#include <xrt/net.h>
#include <xrt/ssh_kex_session.h>



#if defined(XSSH_FEATURE_KEX_EXCHANGE) && \
	(!defined(XSSH_FEATURE_KEX_SESSION) || \
	 !defined(XRT_FEATURE_NET_BUFFER))
	#error "XSSH_FEATURE_KEX_EXCHANGE requires KEX session and XRT network buffer"
#endif



#if defined(XSSH_FEATURE_KEX_EXCHANGE)

/* 交换阶段独立于网络驱动；READY 表示四段 transcript 已经稳定。 */
typedef enum xsshkexexchangephase {
	XSSH_KEX_EXCHANGE_IDENTIFICATION = 0,
	XSSH_KEX_EXCHANGE_KEXINIT = 1,
	XSSH_KEX_EXCHANGE_READY = 2,
	XSSH_KEX_EXCHANGE_METHOD = 3,
	XSSH_KEX_EXCHANGE_COMPLETE = 4,
	XSSH_KEX_EXCHANGE_FAILED = 5
} xsshkexexchangephase;



/* 一个交换对象最多存在一个 identification 或 KEXINIT 保存事务。 */
typedef enum xsshkexexchangepending {
	XSSH_KEX_EXCHANGE_PENDING_NONE = 0,
	XSSH_KEX_EXCHANGE_PENDING_VERSION = 1,
	XSSH_KEX_EXCHANGE_PENDING_KEXINIT = 2
} xsshkexexchangepending;



/*
	对象拥有连接级版本串、本代与下一代 KEXINIT 动态链，以及可重复 rekey 的 KEX 会话。
	对象不拥有 transport、网络、随机源、主机密钥或任务；公开字段只供诊断读取。
*/
typedef struct xsshkexexchange {
	xsshkexsession Session;
	xnetbuf ClientVersion;
	xnetbuf ServerVersion;
	xnetbuf ClientKexInit;
	xnetbuf ServerKexInit;
	xnetbuf NextClientKexInit;
	xnetbuf NextServerKexInit;
	xnetbuf Staging;
	uint64 PendingOrdinal;
	uint64 PendingKexCount;
	xsshrole Role;
	xsshtransportdirection PendingDirection;
	xsshtransportphase PendingCorePhase;
	xsshkexexchangephase Phase;
	xsshkexexchangepending Pending;
	bool PendingFirstKexPacketFollows;
	bool Initialized;
	uint32 Guard;
} xsshkexexchange;



XRT_EXTERN_C_BEGIN



/* 使用同一动态缓冲池初始化连接级 KEX 交换对象；空池使用全局分配器。 */
XRT_API bool xrtSshKexExchangeInit(
	xsshkexexchange* pExchange,
	xnetbufpool* pPool,
	xsshrole Role
);



/* 释放全部 transcript 动态块，并安全清除 KEX 密钥和 SessionId。 */
XRT_API void xrtSshKexExchangeClear(xsshkexexchange* pExchange);



/*
	在 transport identification 提交前复制本端或对端的无换行版本串。
	本端只接受 SSH-2.0；对端同时接受 wire 层支持的 SSH-1.99 兼容形式。
*/
XRT_API xsshcode xrtSshKexExchangeVersionPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xstrview Version
);



/* transport 已提交对应 identification 后，发布连接级稳定版本串。 */
XRT_API xsshcode xrtSshKexExchangeVersionCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* transport 尚未提交对应 identification 时放弃暂存副本。 */
XRT_API xsshcode xrtSshKexExchangeVersionAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/*
	复制完整 KEXINIT payload；本端在 transport Prepare 前调用，对端在认证读取后调用。
	本端 guessed packet 暂不受 KEX 会话支持，会在这里明确返回 UNSUPPORTED。
*/
XRT_API xsshcode xrtSshKexExchangeKexInitPrepare(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction,
	xbytesview Payload
);



/* transport 已提交同一方向和 packet 序号后，发布本代 KEXINIT。 */
XRT_API xsshcode xrtSshKexExchangeKexInitCommit(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* transport 状态和 packet 计数尚未推进时放弃暂存 KEXINIT。 */
XRT_API xsshcode xrtSshKexExchangeKexInitAbort(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* 判断版本串、双方 KEXINIT 与 transport 状态是否可以开始本代方法交换。 */
XRT_API bool xrtSshKexExchangeReady(
	const xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* 返回 READY、METHOD 或 COMPLETE 阶段的稳定 transcript 借用视图。 */
XRT_API xsshcode xrtSshKexExchangeTranscript(
	xsshkexexchange* pExchange,
	xsshkextranscript* pTranscript
);



/* 使用显式 Curve25519 私钥开始本代 KEX，并在成功后晋升下一代动态缓冲。 */
XRT_API xsshcode xrtSshKexExchangeBeginWithPrivate(
	xsshkexexchange* pExchange,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey,
	xbytesview PrivateKey
);



/* 返回 METHOD 或 COMPLETE 阶段可推进的 KEX 会话。 */
XRT_API xsshkexsession* xrtSshKexExchangeSession(
	xsshkexexchange* pExchange
);



/* 返回 METHOD 或 COMPLETE 阶段的只读 KEX 会话。 */
XRT_API const xsshkexsession* xrtSshKexExchangeSessionConst(
	const xsshkexexchange* pExchange
);



/* 确认 KEX 会话和 transport 均已完成本代双向密钥切换。 */
XRT_API xsshcode xrtSshKexExchangeComplete(
	xsshkexexchange* pExchange,
	const xsshtransportcore* pCore
);



/* 终止交换，放弃未发布的下一代材料并清除本代临时秘密。 */
XRT_API void xrtSshKexExchangeFail(xsshkexexchange* pExchange);



XRT_EXTERN_C_END

#endif

#endif
