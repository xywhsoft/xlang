#ifndef XRT_SSH_SESSION_CORE_RANDOM_H
#define XRT_SSH_SESSION_CORE_RANDOM_H

#include <xrt/ssh_kex_exchange_random.h>
#include <xrt/ssh_session_core.h>



#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM) && \
	(!defined(XSSH_FEATURE_KEX_EXCHANGE_RANDOM) || \
	 !defined(XSSH_FEATURE_SESSION_CORE))
	#error "XSSH_FEATURE_SESSION_CORE_RANDOM requires session core and secure KEX exchange"
#endif



#if defined(XSSH_FEATURE_SESSION_CORE_RANDOM)

XRT_EXTERN_C_BEGIN



/* 使用操作系统安全随机临时私钥开始当前已经就绪的一代 KEX。 */
XRT_API xsshcode xrtSshSessionCoreKexBegin(
	xsshsessioncore* pSession,
	xsshtransportcore* pCore,
	xbytesview ServerHostKey
);



XRT_EXTERN_C_END

#endif

#endif
