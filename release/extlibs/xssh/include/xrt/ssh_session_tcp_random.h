#ifndef XRT_SSH_SESSION_TCP_RANDOM_H
#define XRT_SSH_SESSION_TCP_RANDOM_H

#include <xrt/ssh_session_core_random.h>
#include <xrt/ssh_session_tcp.h>
#include <xrt/ssh_transport_tcp_random.h>



#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM) && \
	(!defined(XSSH_FEATURE_SESSION_TCP) || \
	 !defined(XSSH_FEATURE_SESSION_CORE_RANDOM) || \
	 !defined(XSSH_FEATURE_TRANSPORT_TCP_RANDOM))
	#error "XSSH_FEATURE_SESSION_TCP_RANDOM requires TCP session and secure-random helpers"
#endif



#if defined(XSSH_FEATURE_SESSION_TCP_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用 XRT 系统安全随机临时私钥开始当前已就绪的 Curve25519 KEX。 */
XRT_API xsshcode xrtSshSessionTcpKexBegin(
	xsshsessiontcp* pSession,
	xbytesview ServerHostKey
);



/* 使用 XRT 系统安全随机 padding 同时准备协议与 TCP packet 事务。 */
XRT_API xsshcode xrtSshSessionTcpWritePrepare(
	xsshsessiontcp* pSession,
	xbytesview Payload,
	xsshchannelcore* pChannel,
	xsshreplyqueue* pReplies,
	uint64 iReplyToken,
	uint64 iNowMs,
	xsshsessionpacketkind* pKind
);



XRT_EXTERN_C_END

#endif

#endif
