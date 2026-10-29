#ifndef XRT_SSH_PACKET_CODEC_H
#define XRT_SSH_PACKET_CODEC_H

#include <xrt/ssh_packet_aes_gcm.h>



#if defined(XSSH_FEATURE_PACKET_CODEC) && \
	!defined(XSSH_FEATURE_PACKET_AES_GCM)
	#error "XSSH_FEATURE_PACKET_CODEC requires plain and AES-GCM packet support"
#endif



#if defined(XSSH_FEATURE_PACKET_CODEC)

/* 收发方向独立切换；NEWKEYS 不会隐式重置 RFC 4253 序列号。 */
typedef enum xsshpacketmode {
	XSSH_PACKET_MODE_PLAIN = 0,
	XSSH_PACKET_MODE_AES_GCM = 1
} xsshpacketmode;



/* 尺寸探测只读取四字节 packet_length，不要求完整 packet 已经到达。 */
typedef struct xsshpacketneed {
	size_t WireSize;
	size_t PlainSize;
	uint32 PacketSize;
} xsshpacketneed;



/* Codec 不拥有收发缓冲；每个方向只能由一个执行流推进。 */
typedef struct xsshpacketcodec {
	xsshaesgcm ReadAesGcm;
	xsshaesgcm WriteAesGcm;
	uint32 ReadSequence;
	uint32 WriteSequence;
	uint32 MaxPacketSize;
	xsshpacketmode ReadMode;
	xsshpacketmode WriteMode;
	bool WritePending;
	uint32 Guard;
} xsshpacketcodec;



XRT_EXTERN_C_BEGIN



/* 初始化双向 plain codec；零上限使用 XSSH_PACKET_MAX_DEFAULT。 */
XRT_API xsshcode xrtSshPacketCodecInit(
	xsshpacketcodec* pCodec,
	uint32 iMaxPacketSize
);



/* 清除双向 cipher、nonce、序列号和状态标记。 */
XRT_API void xrtSshPacketCodecClear(xsshpacketcodec* pCodec);



/* 在收到 peer NEWKEYS 后原子切换读取方向，不修改读取序列号。 */
XRT_API xsshcode xrtSshPacketCodecSetReadAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
);



/* 在发送本端 NEWKEYS 后原子切换写入方向，不修改写入序列号。 */
XRT_API xsshcode xrtSshPacketCodecSetWriteAesGcm(
	xsshpacketcodec* pCodec,
	xbytesview Key,
	xbytesview InitialIV
);



/* 仅供协商 strict-kex 后在对应方向 NEWKEYS 边界重置序列号。 */
XRT_API xsshcode xrtSshPacketCodecResetReadSequence(
	xsshpacketcodec* pCodec
);
XRT_API xsshcode xrtSshPacketCodecResetWriteSequence(
	xsshpacketcodec* pCodec
);



/* 探测 reader 当前 packet 的完整线长和所需解密工作区。 */
XRT_API xsshcode xrtSshPacketCodecInspect(
	const xsshpacketcodec* pCodec,
	const xsshreader* pReader,
	xsshpacketneed* pNeed
);



/* 按当前写方向精确计算 packet_length 和最终线路长度。 */
XRT_API xsshcode xrtSshPacketCodecWriteMeasure(
	const xsshpacketcodec* pCodec,
	size_t iPayloadSize,
	xsshpacketneed* pNeed
);



/*
	使用调用方 padding 源准备当前方向的最终线路包。
	成功后 writer 已推进，但 sequence 和 nonce 必须等可靠入队后再提交。
*/
XRT_API xsshcode xrtSshPacketCodecWritePrepareWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 提交唯一一个已可靠入队的写事务，并推进 sequence 与 cipher nonce。 */
XRT_API xsshcode xrtSshPacketCodecWriteCommit(xsshpacketcodec* pCodec);



/* 放弃尚未发送的写事务；调用方负责丢弃已生成的线路字节。 */
XRT_API xsshcode xrtSshPacketCodecWriteAbort(xsshpacketcodec* pCodec);



/* 使用调用方 padding 源一次性准备并提交，适用于无需背压重试的路径。 */
XRT_API xsshcode xrtSshPacketCodecWriteWithPadding(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 从当前方向增量读取；AES-GCM 模式把明文写入调用方工作区。 */
XRT_API xsshcode xrtSshPacketCodecRead(
	xsshpacketcodec* pCodec,
	xsshreader* pReader,
	xsshpacketview* pPacket,
	void* pPlain,
	size_t iPlainCapacity
);



XRT_EXTERN_C_END

#endif

#endif
