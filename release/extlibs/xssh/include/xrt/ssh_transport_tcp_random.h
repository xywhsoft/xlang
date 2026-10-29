#ifndef XRT_SSH_TRANSPORT_TCP_RANDOM_H
#define XRT_SSH_TRANSPORT_TCP_RANDOM_H

#include <xrt/ssh_packet_random.h>
#include <xrt/ssh_transport_tcp.h>



#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM) && \
	(!defined(XSSH_FEATURE_TRANSPORT_TCP) || \
	 !defined(XSSH_FEATURE_PACKET_RANDOM))
	#error "XSSH_FEATURE_TRANSPORT_TCP_RANDOM requires TCP transport and secure padding"
#endif



#if defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机 padding 在动态输出链中准备唯一 packet。 */
XRT_API xsshcode xrtSshTransportTcpWritePrepare(
	xsshtransporttcp* pTransport,
	xbytesview Payload,
	uint64 iNowMs
);



XRT_EXTERN_C_END

#endif

#endif
