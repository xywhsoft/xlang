#ifndef XRT_SSH_PACKET_H
#define XRT_SSH_PACKET_H

#include <xrt/ssh_wire.h>



#if defined(XSSH_FEATURE_PACKET) && !defined(XSSH_FEATURE_WIRE)
	#error "XSSH_FEATURE_PACKET requires XSSH_FEATURE_WIRE"
#endif



#if defined(XSSH_FEATURE_PACKET)

/* RFC 4253 要求实现至少接收 32768 字节 payload；默认同时限制线路包长。 */
#define XSSH_PACKET_MAX_DEFAULT 35000u
#define XSSH_PACKET_BLOCK_MIN 8u
#define XSSH_PACKET_PADDING_MIN 4u
#define XSSH_PACKET_PADDING_MAX 255u



/* Packet view 借用 reader 输入，PacketSize 不包含四字节长度字段。 */
typedef struct xsshpacketview {
	uint32 Sequence;
	uint32 PacketSize;
	uint8 PaddingSize;
	xbytesview Payload;
	xbytesview Padding;
} xsshpacketview;



/* Padding 回调只填写当前临时片段，返回后不得持有 pOutput。 */
typedef bool (*xsshpaddingproc)(
	void* pOutput,
	size_t iSize,
	ptr pUserData
);



XRT_EXTERN_C_BEGIN



/* 计算 plain packet 的 padding 和 packet_length；零块长使用八字节。 */
XRT_API xsshcode xrtSshPacketMeasure(
	size_t iPayloadSize,
	size_t iBlockSize,
	uint8* pPaddingSize,
	uint32* pPacketSize
);



/* 使用调用方 padding 源写入一个完整 plain packet，并在成功后递增序列号。 */
XRT_API xsshcode xrtSshPacketWrite(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence,
	xsshpaddingproc pPadding,
	ptr pUserData
);



/* 从增量输入读取一个完整 plain packet，并在成功后递增序列号。 */
XRT_API xsshcode xrtSshPacketRead(
	xsshreader* pReader,
	size_t iBlockSize,
	uint32 iMaxPacketSize,
	uint32* pSequence,
	xsshpacketview* pPacket
);



XRT_EXTERN_C_END

#endif

#endif
