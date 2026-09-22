#include <xrt/ssh_transport_tcp_random.h>



#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM)

/* 默认生产路径直接使用 XRT 系统安全随机源。 */
xsshcode xrtSshTransportTcpWritePrepare(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	uint64 iNowMs
)
{
	return xrtSshTransportTcpWritePrepareWithPadding(
		pTransport,
		Payload,
		xrtSshSecurePadding,
		NULL,
		iNowMs
	);
}

#endif
