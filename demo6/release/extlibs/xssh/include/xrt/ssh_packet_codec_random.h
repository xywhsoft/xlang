#ifndef XRT_SSH_PACKET_CODEC_RANDOM_H
#define XRT_SSH_PACKET_CODEC_RANDOM_H

#include <xrt/ssh_packet_codec.h>
#include <xrt/ssh_packet_random.h>



#if defined(XSSH_FEATURE_PACKET_CODEC_RANDOM) && \
	(!defined(XSSH_FEATURE_PACKET_CODEC) || \
	 !defined(XSSH_FEATURE_PACKET_RANDOM))
	#error "XSSH_FEATURE_PACKET_CODEC_RANDOM requires packet codec and secure random padding"
#endif



#if defined(XSSH_FEATURE_PACKET_CODEC_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机源准备写事务，可靠入队后必须另行提交。 */
XRT_API xsshcode xrtSshPacketCodecWritePrepare(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
);



/* 使用 XRT 系统安全随机源写入 codec 当前方向。 */
XRT_API xsshcode xrtSshPacketCodecWrite(
	xsshpacketcodec* pCodec,
	xsshwriter* pWriter,
	xbytesview Payload
);



XRT_EXTERN_C_END

#endif

#endif
