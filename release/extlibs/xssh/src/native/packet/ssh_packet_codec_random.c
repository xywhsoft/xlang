#include <xrt/ssh_packet_codec_random.h>



#if defined(XSSH_FEATURE_PACKET_CODEC_RANDOM)

/* 系统 CSPRNG 只负责填充准备阶段的 padding。 */
xsshcode xrtSshPacketCodecWritePrepare(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
)
{
	return xrtSshPacketCodecWritePrepareWithPadding(
		pCodec,
		pWriter,
		Payload,
		xrtSshSecurePadding,
		NULL
	);
}



/* 默认生产路径只组合 codec 与 XRT 系统安全随机源。 */
xsshcode xrtSshPacketCodecWrite(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
)
{
	return xrtSshPacketCodecWriteWithPadding(
		pCodec,
		pWriter,
		Payload,
		xrtSshSecurePadding,
		NULL
	);
}

#endif
