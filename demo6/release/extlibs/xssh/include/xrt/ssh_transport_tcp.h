#ifndef XRT_SSH_TRANSPORT_TCP_H
#define XRT_SSH_TRANSPORT_TCP_H

#include <xrt/ssh_transport_core.h>
#include <xrt/tcp.h>



#if defined(XSSH_FEATURE_TRANSPORT_TCP) && \
	(!defined(XSSH_FEATURE_TRANSPORT_CORE) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "XSSH_FEATURE_TRANSPORT_TCP requires transport core and XRT TCP"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_TCP)

#define XSSH_TRANSPORT_TCP_BANNER_LIMIT_DEFAULT 65536u



/* TCP 适配层一次只借出一个 identification 或 packet 事务。 */
typedef enum xsshtransporttcppending {
	XSSH_TRANSPORT_TCP_PENDING_NONE = 0,
	XSSH_TRANSPORT_TCP_PENDING_IDENTIFICATION = 1,
	XSSH_TRANSPORT_TCP_PENDING_PACKET = 2
} xsshtransporttcppending;



/* 配置只描述协议预算；Engine、Stream、Worker 和时钟均由调用方持有。 */
typedef struct xsshtransporttcpconfig {
	xsshrekeypolicy Rekey;
	size_t MaxBannerBytes;
	uint32 MaxPacketSize;
	xsshrole Role;
} xsshtransporttcpconfig;



/*
	TCP transport 只拥有 core 与按需输出链，不含固定收发数组。
	除 API 明确返回的 Core 指针外，公开字段只供诊断读取。
*/
typedef struct xsshtransporttcp {
	xsshtransportcore Core;
	xnetbuf Output;
	xnetbuf* Input;
	size_t MaxBannerBytes;
	size_t ReadSize;
	xsshtransporttcppending WritePending;
	xsshtransporttcppending ReadPending;
	uint32 Guard;
} xsshtransporttcp;



XRT_EXTERN_C_BEGIN



/* 写入指定角色、默认 packet、banner 和 rekey 预算。 */
XRT_API bool xrtSshTransportTcpConfigInit(
	xsshtransporttcpconfig* pConfig,
	xsshrole Role
);



/* 使用所属 Worker 的缓冲池初始化 TCP transport；不接管缓冲池。 */
XRT_API bool xrtSshTransportTcpInit(
	xsshtransporttcp* pTransport,
	xnetbufpool* pPool,
	const xsshtransporttcpconfig* pConfig,
	uint64 iNowMs
);



/* 放弃未决事务、释放动态块并安全清除 cipher 状态。 */
XRT_API void xrtSshTransportTcpClear(xsshtransporttcp* pTransport);



/* 返回可推进 KEX、密钥、认证和连接协议的 transport core。 */
XRT_API xsshtransportcore* xrtSshTransportTcpCore(
	xsshtransporttcp* pTransport
);



/* 返回只读 transport core；无效对象返回空。 */
XRT_API const xsshtransportcore* xrtSshTransportTcpCoreConst(
	const xsshtransporttcp* pTransport
);



/* 在动态输出链中准备本端 identification，但不推进协议状态。 */
XRT_API xsshcode xrtSshTransportTcpIdentificationPrepare(
	xsshtransporttcp* pTransport,
	xstrview Banner
);



/* 使用调用方 padding 源在动态输出链中准备唯一 packet。 */
XRT_API xsshcode xrtSshTransportTcpWritePrepareWithPadding(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	uint64 iNowMs
);



/*
	把未决输出零复制交给 Stream；AGAIN 或 ERROR 时事务和缓冲保持不变。
	成功接管后立即提交 identification 或 packet 状态。
*/
XRT_API xnetresult xrtSshTransportTcpWriteSubmit(
	xsshtransporttcp* pTransport,
	xnetstream* pStream,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 放弃尚未被 TCP 接管的输出；packet sequence 与 nonce 保持不变。 */
XRT_API xsshcode xrtSshTransportTcpWriteAbort(
	xsshtransporttcp* pTransport
);



/* 返回当前待重试输出字节数；没有未决输出时返回零。 */
XRT_API size_t xrtSshTransportTcpWriteSize(
	const xsshtransporttcp* pTransport
);



/* 从分块 TCP 输入准备 peer identification，返回值借用到 ReadCommit/Abort。 */
XRT_API xsshcode xrtSshTransportTcpIdentificationReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xstrview* pBanner
);



/* 仅复制四字节长度头，探测下一 packet 的线路和解密工作区需求。 */
XRT_API xsshcode xrtSshTransportTcpReadInspect(
	const xsshtransporttcp* pTransport,
	const xnetbuf* pInput,
	xsshpacketneed* pNeed
);



/* 按需连续化一个完整 packet，认证后返回借用 view。 */
XRT_API xsshcode xrtSshTransportTcpReadPrepare(
	xsshtransporttcp* pTransport,
	xnetbuf* pInput,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs
);



/* 提交上层已经接受的输入并从原 TCP 缓冲精确消费。 */
XRT_API xsshcode xrtSshTransportTcpReadCommit(
	xsshtransporttcp* pTransport,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 拒绝当前借用输入、消费其线路字节并关闭不可继续的 transport。 */
XRT_API xsshcode xrtSshTransportTcpReadAbort(
	xsshtransporttcp* pTransport
);



XRT_EXTERN_C_END

#endif

#endif
