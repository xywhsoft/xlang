#ifndef XRT_SSH_PACKET_RANDOM_H
#define XRT_SSH_PACKET_RANDOM_H

#include <xrt/ssh_packet.h>
#include <xrt/random.h>



#if defined(XSSH_FEATURE_PACKET_RANDOM) && \
	(!defined(XSSH_FEATURE_PACKET) || !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XSSH_FEATURE_PACKET_RANDOM requires packet and random_secure"
#endif



#if defined(XSSH_FEATURE_PACKET_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统密码学安全随机源填充 packet padding；可直接作为 padding 回调。 */
XRT_API bool xrtSshSecurePadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
);



/* 使用操作系统密码学安全随机源写入一个 plain packet。 */
XRT_API xsshcode xrtSshPacketWriteSecure(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence
);



XRT_EXTERN_C_END

#endif

#endif
