#ifndef XRT_SSH_TRANSPORT_REKEY_H
#define XRT_SSH_TRANSPORT_REKEY_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_TRANSPORT_REKEY) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_TRANSPORT_REKEY requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_REKEY)

#define XSSH_REKEY_DEFAULT_BYTE_LIMIT UINT64_C(1073741824)
#define XSSH_REKEY_DEFAULT_SEND_PACKET_LIMIT UINT64_C(2147483648)
#define XSSH_REKEY_DEFAULT_RECEIVE_PACKET_LIMIT UINT64_C(2147483648)
#define XSSH_REKEY_DEFAULT_BLOCK_LIMIT UINT64_C(4294967296)
#define XSSH_REKEY_DEFAULT_TIME_LIMIT_MS UINT64_C(3600000)
#define XSSH_REKEY_HARD_PACKET_LIMIT UINT64_C(4294967296)



/* Rekey 决策按严重程度单调递增。 */
typedef enum xsshrekeydecision {
	XSSH_REKEY_NONE = 0,
	XSSH_REKEY_RECOMMENDED = 1,
	XSSH_REKEY_REQUIRED = 2
} xsshrekeydecision;



/* 零值软阈值表示禁用；HardPacketLimit 必须非零且不超过协议硬上限。 */
typedef struct xsshrekeypolicy {
	uint64 ByteLimit;
	uint64 SendPacketLimit;
	uint64 ReceivePacketLimit;
	uint64 BlockLimit;
	uint64 TimeLimitMs;
	uint64 HardPacketLimit;
} xsshrekeypolicy;



/* 单方向计数器使用饱和加法，永不回绕。 */
typedef struct xsshrekeycounter {
	uint64 Bytes;
	uint64 Packets;
	uint64 Blocks;
} xsshrekeycounter;



/* Rekey 状态不拥有时钟；时间由 transport 的单调时钟显式传入。 */
typedef struct xsshrekeystate {
	xsshrekeypolicy Policy;
	xsshrekeycounter Sent;
	xsshrekeycounter Received;
	uint64 SendStartedMs;
	uint64 ReceiveStartedMs;
	bool Requested;
} xsshrekeystate;



XRT_EXTERN_C_BEGIN



/* 初始化 RFC 4253/4344 对应的保守默认策略。 */
XRT_API void xrtSshRekeyPolicyInit(xsshrekeypolicy* pPolicy);



/* 校验策略并初始化一代密钥的计数状态。 */
XRT_API bool xrtSshRekeyInit(
	xsshrekeystate* pState,
	const xsshrekeypolicy* pPolicy,
	uint64 iNowMs
);



/* 同时清空双向计数并开始新一代，适用于两方向具有同一提交边界的驱动。 */
XRT_API bool xrtSshRekeyReset(
	xsshrekeystate* pState,
	uint64 iNowMs
);



/* 写密钥生效后只清空发送方向计数和时间。 */
XRT_API bool xrtSshRekeyResetSend(
	xsshrekeystate* pState,
	uint64 iNowMs
);



/* 读密钥生效后只清空接收方向计数和时间。 */
XRT_API bool xrtSshRekeyResetReceive(
	xsshrekeystate* pState,
	uint64 iNowMs
);



/* 双向新密钥均已生效后结束主动 rekey 请求，不修改新代计数。 */
XRT_API bool xrtSshRekeyComplete(xsshrekeystate* pState);



/* 请求策略阈值之外的主动 rekey。 */
XRT_API bool xrtSshRekeyRequest(xsshrekeystate* pState);



/* 查询当前计数、主动请求和时间阈值产生的决策。 */
XRT_API xsshcode xrtSshRekeyCheck(
	const xsshrekeystate* pState,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 在发送前预留下一包；REQUIRED 时状态不变且该包不得发送。 */
XRT_API xsshcode xrtSshRekeyReserveSend(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



/* 在认证接收包前预留下一包；REQUIRED 时状态不变且该包不得接受。 */
XRT_API xsshcode xrtSshRekeyReserveReceive(
	xsshrekeystate* pState,
	uint64 iWireBytes,
	uint64 iCipherBlocks,
	uint64 iNowMs,
	xsshrekeydecision* pDecision
);



XRT_EXTERN_C_END

#endif

#endif
