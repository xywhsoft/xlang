#include <xrt/ssh_packet_random.h>



#if defined(XSSH_FEATURE_PACKET_RANDOM)

/* 把 XRT 的系统安全随机源适配为 SSH packet padding 回调。 */
bool xrtSshSecurePadding(
	void* pOutput,
	size_t iSize,
	ptr pUserData
)
{
	(void)pUserData;
	return xrtSecureRandom(pOutput, iSize);
}



/* 为常用安全路径省去手工传递 padding 回调。 */
xsshcode xrtSshPacketWriteSecure(
	xsshwriter* pWriter,
	xbytesview Payload,
	size_t iBlockSize,
	uint32* pSequence
)
{
	return xrtSshPacketWrite(
		pWriter,
		Payload,
		iBlockSize,
		pSequence,
		xrtSshSecurePadding,
		NULL
	);
}

#endif
