#ifndef XRT_SSH_TRANSPORT_CORE_H
#define XRT_SSH_TRANSPORT_CORE_H

#include <xrt/ssh_packet_codec.h>
#include <xrt/ssh_transport_rekey.h>
#include <xrt/ssh_transport_state.h>



#if defined(XSSH_FEATURE_TRANSPORT_CORE) && \
	(!defined(XSSH_FEATURE_PACKET_CODEC) || \
	 !defined(XSSH_FEATURE_TRANSPORT_REKEY) || \
	 !defined(XSSH_FEATURE_TRANSPORT_STATE))
	#error "XSSH_FEATURE_TRANSPORT_CORE requires packet codec, rekey and transport state"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_CORE)

/* 待提交包的类别由 core 从完整 payload 自动识别。 */
typedef enum xsshtransportpacketkind {
	XSSH_TRANSPORT_PACKET_MESSAGE = 0,
	XSSH_TRANSPORT_PACKET_KEXINIT = 1,
	XSSH_TRANSPORT_PACKET_NEWKEYS = 2,
	XSSH_TRANSPORT_PACKET_AUTH_SUCCESS = 3
} xsshtransportpacketkind;



/* 待提交状态不拥有 payload 或线路缓冲，字段只供诊断读取。 */
typedef struct xsshtransportpending {
	uint64 WireBytes;
	uint64 CipherBlocks;
	xsshtransportpacketkind Kind;
	uint8 Message;
	bool FirstKexPacketFollows;
	bool Active;
} xsshtransportpending;



/*
	Core 只拥有 packet、顺序和预算状态，不拥有网络、时钟、密钥原文或缓冲。
	单个对象由一个执行流推进，读写网络等待可以由同步、future 或协程驱动共享。
*/
typedef struct xsshtransportcore {
	xsshpacketcodec Codec;
	xsshtransportstate State;
	xsshrekeystate Rekey;
	xsshtransportpending Write;
	xsshtransportpending Read;
	uint32 WriteKeyActions;
	uint32 ReadKeyActions;
	bool KexCompletePending;
	uint32 Guard;
} xsshtransportcore;



XRT_EXTERN_C_BEGIN



/* 初始化无缓冲 transport core；零包上限和空策略分别使用默认值。 */
XRT_API bool xrtSshTransportCoreInit(
	xsshtransportcore* pCore,
	xsshrole Role,
	uint32 iMaxPacketSize,
	const xsshrekeypolicy* pRekeyPolicy,
	uint64 iNowMs
);



/* 清除 cipher、序列、协议和预算状态，不处理任何调用方缓冲。 */
XRT_API void xrtSshTransportCoreClear(xsshtransportcore* pCore);



/* 提交本端 identification 已发送或对端 identification 已验证。 */
XRT_API xsshcode xrtSshTransportCoreIdentificationCommit(
	xsshtransportcore* pCore,
	xsshtransportdirection Direction
);



/* 判断对应方向当前是否允许应用消息且 NEWKEYS 密钥已经生效。 */
XRT_API bool xrtSshTransportCoreCanApplication(
	const xsshtransportcore* pCore,
	xsshtransportdirection Direction
);



/* 判断收到对端 KEXINIT 后本端是否必须回复。 */
XRT_API bool xrtSshTransportCoreKexReplyNeeded(
	const xsshtransportcore* pCore
);



/* 提交双方 KEXINIT 的协商结果和当前算法消息额度。 */
XRT_API xsshcode xrtSshTransportCoreKexConfigure(
	xsshtransportcore* pCore,
	const xsshkexinit* pLocal,
	const xsshkexinit* pPeer,
	const xsshkexnegotiation* pNegotiation,
	const xsshtransportkexrules* pRules
);



/* 请求一次策略阈值之外的主动 rekey。 */
XRT_API bool xrtSshTransportCoreRekeyRequest(xsshtransportcore* pCore);



/* 查询当前双向预算和时间产生的 rekey 决策。 */
XRT_API xsshcode xrtSshTransportCoreRekeyCheck(
	const xsshtransportcore* pCore,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 探测下一包所需完整线路长度和明文工作区。 */
XRT_API xsshcode xrtSshTransportCoreInspect(
	const xsshtransportcore* pCore,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
);



/*
	生成最终线路包但不推进协议、sequence、nonce 或 rekey 预算。
	网络队列返回 AGAIN 时保留 writer 新增字节并重试同一包。
*/
XRT_API xsshcode xrtSshTransportCoreWritePrepareWithPadding(
	xsshtransportcore* pCore,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData,
	uint64 iNowMs
);



/* 线路包可靠入队后提交写事务并返回更新后的 rekey 决策。 */
XRT_API xsshcode xrtSshTransportCoreWriteCommit(
	xsshtransportcore* pCore,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 放弃未发送的写事务；调用方负责丢弃 writer 新增的线路字节。 */
XRT_API xsshcode xrtSshTransportCoreWriteAbort(xsshtransportcore* pCore);



/*
	认证并准备一个完整接收包；成功后 packet 借用输入或 pPlain。
	调用方只能解析当前 packet，随后必须 Commit 或 Abort。
*/
XRT_API xsshcode xrtSshTransportCoreReadPrepare(
	xsshtransportcore* pCore,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity,
	uint64 iNowMs
);



/* 接收包完成协议处理后提交状态和 rekey 预算。 */
XRT_API xsshcode xrtSshTransportCoreReadCommit(
	xsshtransportcore* pCore,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 放弃已认证但不能接受的接收包，并关闭不可继续推进的 transport。 */
XRT_API xsshcode xrtSshTransportCoreReadAbort(xsshtransportcore* pCore);



/* 判断本端 NEWKEYS 后是否正在等待写方向新密钥。 */
XRT_API bool xrtSshTransportCoreWriteKeysPending(
	const xsshtransportcore* pCore
);



/* 判断对端 NEWKEYS 后是否正在等待读方向新密钥。 */
XRT_API bool xrtSshTransportCoreReadKeysPending(
	const xsshtransportcore* pCore
);



/* 在本端 NEWKEYS 可靠入队后激活写方向 AES-GCM 与方向性 rekey 新代。 */
XRT_API xsshcode xrtSshTransportCoreSetWriteAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	uint64 iNowMs
);



/* 在对端 NEWKEYS 已认证后激活读方向 AES-GCM 与方向性 rekey 新代。 */
XRT_API xsshcode xrtSshTransportCoreSetReadAesGcm(
	xsshtransportcore* pCore,
	xbytesview Key,
	xbytesview InitialIV,
	uint64 iNowMs
);



/* 判断至少一代 KEX 已完成且双向 NEWKEYS 密钥均已实际生效。 */
XRT_API bool xrtSshTransportCoreKexComplete(
	const xsshtransportcore* pCore
);



/* 关闭 transport；未发送写事务会安全放弃，已准备读事务不会回滚。 */
XRT_API void xrtSshTransportCoreClose(xsshtransportcore* pCore);



XRT_EXTERN_C_END

#endif

#endif
